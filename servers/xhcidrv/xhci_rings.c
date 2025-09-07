/*
 * xHCI Ring Management Implementation
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <anos/syscalls.h>

#include "include/xhci_rings.h"

#ifdef DEBUG_XHCI_RINGS
#define ring_debugf(...) printf(__VA_ARGS__)
#else
#define ring_debugf(...)
#endif

// =============================================================================
// Ring Management
// =============================================================================

int xhci_ring_init(XhciRing *ring, uint32_t size) {
    if (!ring || size == 0 || size > 4096) {
        return -1;
    }

    memset(ring, 0, sizeof(XhciRing));
    ring->size = size;

    // Allocate physical memory for TRBs (must be page-aligned)
    const size_t trb_array_size = size * sizeof(XhciTrb);
    const size_t aligned_size = (trb_array_size + 0xFFF) & ~0xFFF; // Page align

    const SyscallResultA alloc_result = anos_alloc_physical_pages(aligned_size);
    if (alloc_result.result != SYSCALL_OK) {
        printf("xHCI: Failed to allocate physical memory for ring - syscall "
               "result: 0x%016lx\n",
               (uint64_t)alloc_result.result);
        printf("xHCI: Requested %lu pages (%lu bytes)\n", aligned_size / 4096,
               aligned_size);
        return -1;
    }

    ring->trbs_physical = alloc_result.value;

    // Map to virtual address space
    const SyscallResult map_result = anos_map_physical(
            ring->trbs_physical,
            (void *)(0x500000000 +
                     ring->trbs_physical), // Simple virtual mapping
            aligned_size,
            ANOS_MAP_PHYSICAL_FLAG_READ | ANOS_MAP_PHYSICAL_FLAG_WRITE |
                    ANOS_MAP_PHYSICAL_FLAG_NOCACHE);

    if (map_result.result != SYSCALL_OK) {
        printf("xHCI: Failed to map ring memory\n");
        return -1;
    }

    ring->trbs = (XhciTrb *)(0x500000000 + ring->trbs_physical);

    // Initialize ring state
    ring->enqueue_index = 0;
    ring->dequeue_index = 0;
    ring->cycle_state = 1;
    ring->producer_cycle_state = 1;
    ring->consumer_cycle_state = 1;

    // Clear all TRBs
    memset(ring->trbs, 0, size * sizeof(XhciTrb));

    ring_debugf(
            "xHCI: Initialized ring with %u TRBs at phys=0x%016lx virt=%p\n",
            size, ring->trbs_physical, ring->trbs);

    return 0;
}

void xhci_ring_free(XhciRing *ring) {
    if (!ring || !ring->trbs) {
        return;
    }

    // TODO: Unmap virtual memory and free physical pages
    ring->trbs = NULL;
    ring->trbs_physical = 0;
    memset(ring, 0, sizeof(XhciRing));
}

XhciTrb *xhci_ring_enqueue_trb(XhciRing *ring) {
    if (!ring || !ring->trbs) {
        return NULL;
    }

    // Check if we have space (leave one slot for link TRB if needed)
    uint32_t next_enqueue = (ring->enqueue_index + 1) % ring->size;
    if (next_enqueue == ring->dequeue_index) {
        ring_debugf("xHCI: Ring full, cannot enqueue TRB\n");
        return NULL;
    }

    XhciTrb *trb = &ring->trbs[ring->enqueue_index];

    // Set cycle bit for producer
    trb->control = (trb->control & ~TRB_CONTROL_CYCLE_BIT) |
                   (ring->producer_cycle_state ? TRB_CONTROL_CYCLE_BIT : 0);

    ring_debugf("xHCI: Enqueuing TRB at index %u, cycle=%u\n",
                ring->enqueue_index, ring->producer_cycle_state);

    return trb;
}

XhciTrb *xhci_ring_dequeue_trb(XhciRing *ring) {
    if (!ring || !ring->trbs) {
        return NULL;
    }

    // Check if ring is empty
    if (ring->enqueue_index == ring->dequeue_index) {
        return NULL;
    }

    XhciTrb *trb = &ring->trbs[ring->dequeue_index];

    // Check cycle bit
    bool trb_cycle = (trb->control & TRB_CONTROL_CYCLE_BIT) != 0;
    if (trb_cycle != ring->consumer_cycle_state) {
        return NULL; // TRB not ready
    }

    ring_debugf("xHCI: Dequeuing TRB at index %u, cycle=%u\n",
                ring->dequeue_index, ring->consumer_cycle_state);

    return trb;
}

bool xhci_ring_has_space(XhciRing *ring, uint32_t num_trbs) {
    if (!ring)
        return false;

    uint32_t available_slots;
    if (ring->enqueue_index >= ring->dequeue_index) {
        available_slots =
                ring->size - (ring->enqueue_index - ring->dequeue_index) - 1;
    } else {
        available_slots = ring->dequeue_index - ring->enqueue_index - 1;
    }

    return available_slots >= num_trbs;
}

void xhci_ring_inc_enqueue(XhciRing *ring) {
    if (!ring)
        return;

    ring->enqueue_index = (ring->enqueue_index + 1) % ring->size;
    ring->enqueued_count++;

    // Handle cycle bit wrap-around
    if (ring->enqueue_index == 0) {
        ring->producer_cycle_state = !ring->producer_cycle_state;
        ring_debugf("xHCI: Producer cycle state flipped to %u\n",
                    ring->producer_cycle_state);
    }
}

void xhci_ring_inc_dequeue(XhciRing *ring) {
    if (!ring)
        return;

    ring->dequeue_index = (ring->dequeue_index + 1) % ring->size;
    ring->dequeued_count++;

    // Handle cycle bit wrap-around
    if (ring->dequeue_index == 0) {
        ring->consumer_cycle_state = !ring->consumer_cycle_state;
        ring_debugf("xHCI: Consumer cycle state flipped to %u\n",
                    ring->consumer_cycle_state);
    }
}

// =============================================================================
// Utility Functions
// =============================================================================

const char *xhci_trb_type_string(uint32_t trb_type) {
    switch (trb_type) {
    case TRB_TYPE_NORMAL:
        return "Normal";
    case TRB_TYPE_SETUP_STAGE:
        return "Setup Stage";
    case TRB_TYPE_DATA_STAGE:
        return "Data Stage";
    case TRB_TYPE_STATUS_STAGE:
        return "Status Stage";
    case TRB_TYPE_ISOCH:
        return "Isochronous";
    case TRB_TYPE_LINK:
        return "Link";
    case TRB_TYPE_EVENT_DATA:
        return "Event Data";
    case TRB_TYPE_NOOP:
        return "No-Op";
    case TRB_TYPE_ENABLE_SLOT:
        return "Enable Slot";
    case TRB_TYPE_DISABLE_SLOT:
        return "Disable Slot";
    case TRB_TYPE_ADDRESS_DEVICE:
        return "Address Device";
    case TRB_TYPE_CONFIGURE_ENDPOINT:
        return "Configure Endpoint";
    case TRB_TYPE_EVALUATE_CONTEXT:
        return "Evaluate Context";
    case TRB_TYPE_RESET_ENDPOINT:
        return "Reset Endpoint";
    case TRB_TYPE_STOP_ENDPOINT:
        return "Stop Endpoint";
    case TRB_TYPE_SET_TR_DEQUEUE:
        return "Set TR Dequeue";
    case TRB_TYPE_RESET_DEVICE:
        return "Reset Device";
    case TRB_TYPE_TRANSFER:
        return "Transfer Event";
    case TRB_TYPE_COMMAND_COMPLETION:
        return "Command Completion";
    case TRB_TYPE_PORT_STATUS_CHANGE:
        return "Port Status Change";
    default:
        return "Unknown";
    }
}

const char *xhci_completion_code_string(uint32_t completion_code) {
    switch (completion_code) {
    case XHCI_COMP_SUCCESS:
        return "Success";
    case XHCI_COMP_DATA_BUFFER_ERROR:
        return "Data Buffer Error";
    case XHCI_COMP_BABBLE_DETECTED:
        return "Babble Detected";
    case XHCI_COMP_USB_TRANSACTION_ERROR:
        return "USB Transaction Error";
    case XHCI_COMP_TRB_ERROR:
        return "TRB Error";
    case XHCI_COMP_STALL_ERROR:
        return "Stall Error";
    case XHCI_COMP_RESOURCE_ERROR:
        return "Resource Error";
    case XHCI_COMP_BANDWIDTH_ERROR:
        return "Bandwidth Error";
    case XHCI_COMP_NO_SLOTS_AVAILABLE:
        return "No Slots Available";
    case XHCI_COMP_SLOT_NOT_ENABLED:
        return "Slot Not Enabled";
    case XHCI_COMP_ENDPOINT_NOT_ENABLED:
        return "Endpoint Not Enabled";
    case XHCI_COMP_SHORT_PACKET:
        return "Short Packet";
    case XHCI_COMP_RING_UNDERRUN:
        return "Ring Underrun";
    case XHCI_COMP_RING_OVERRUN:
        return "Ring Overrun";
    case XHCI_COMP_PARAMETER_ERROR:
        return "Parameter Error";
    case XHCI_COMP_CONTEXT_STATE_ERROR:
        return "Context State Error";
    case XHCI_COMP_COMMAND_RING_STOPPED:
        return "Command Ring Stopped";
    case XHCI_COMP_COMMAND_ABORTED:
        return "Command Aborted";
    case XHCI_COMP_STOPPED:
        return "Stopped";
    case XHCI_COMP_EVENT_RING_FULL:
        return "Event Ring Full";
    default:
        return "Unknown";
    }
}