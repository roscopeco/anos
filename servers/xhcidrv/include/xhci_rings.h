/*
 * xHCI Ring Management and TRB Definitions
 * anos - An Operating System
 *
 * xHCI-specific Transfer Request Block (TRB) structures and ring management
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_XHCI_RINGS_H
#define __ANOS_XHCI_RINGS_H

#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// TRB (Transfer Request Block) Types
// =============================================================================

// TRB Types
#define TRB_TYPE_NORMAL 1
#define TRB_TYPE_SETUP_STAGE 2
#define TRB_TYPE_DATA_STAGE 3
#define TRB_TYPE_STATUS_STAGE 4
#define TRB_TYPE_ISOCH 5
#define TRB_TYPE_LINK 6
#define TRB_TYPE_EVENT_DATA 7
#define TRB_TYPE_NOOP 8

// Command TRB Types
#define TRB_TYPE_ENABLE_SLOT 9
#define TRB_TYPE_DISABLE_SLOT 10
#define TRB_TYPE_ADDRESS_DEVICE 11
#define TRB_TYPE_CONFIGURE_ENDPOINT 12
#define TRB_TYPE_EVALUATE_CONTEXT 13
#define TRB_TYPE_RESET_ENDPOINT 14
#define TRB_TYPE_STOP_ENDPOINT 15
#define TRB_TYPE_SET_TR_DEQUEUE 16
#define TRB_TYPE_RESET_DEVICE 17
#define TRB_TYPE_FORCE_EVENT 18
#define TRB_TYPE_NEGOTIATE_BW 19
#define TRB_TYPE_SET_LATENCY_TOLERANCE 20
#define TRB_TYPE_GET_PORT_BW 21
#define TRB_TYPE_FORCE_HEADER 22
#define TRB_TYPE_NOOP_CMD 23

// Event TRB Types
#define TRB_TYPE_TRANSFER 32
#define TRB_TYPE_COMMAND_COMPLETION 33
#define TRB_TYPE_PORT_STATUS_CHANGE 34
#define TRB_TYPE_BANDWIDTH_REQUEST 35
#define TRB_TYPE_DOORBELL 36
#define TRB_TYPE_HOST_CONTROLLER 37
#define TRB_TYPE_DEVICE_NOTIFICATION 38
#define TRB_TYPE_MFINDEX_WRAP 39

// =============================================================================
// TRB Structure Definitions
// =============================================================================

// Generic TRB structure
typedef struct {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} __attribute__((packed)) XhciTrb;

// TRB Control field bit definitions
#define TRB_CONTROL_CYCLE_BIT (1U << 0)
#define TRB_CONTROL_EVALUATE_NEXT (1U << 1)
#define TRB_CONTROL_INTERRUPT_ON_SHORT (1U << 2)
#define TRB_CONTROL_NO_SNOOP (1U << 3)
#define TRB_CONTROL_CHAIN_BIT (1U << 4)
#define TRB_CONTROL_INTERRUPT_ON_COMPLETE (1U << 5)
#define TRB_CONTROL_IMMEDIATE_DATA (1U << 6)
#define TRB_CONTROL_BLOCK_EVENT_INTERRUPT (1U << 9)

#define TRB_CONTROL_TYPE_SHIFT 10
#define TRB_CONTROL_TYPE_MASK (0x3F << TRB_CONTROL_TYPE_SHIFT)

#define TRB_CONTROL_ENDPOINT_ID_SHIFT 16
#define TRB_CONTROL_ENDPOINT_ID_MASK (0x1F << TRB_CONTROL_ENDPOINT_ID_SHIFT)

#define TRB_CONTROL_SLOT_ID_SHIFT 24
#define TRB_CONTROL_SLOT_ID_MASK (0xFF << TRB_CONTROL_SLOT_ID_SHIFT)

// Helper macros for TRB fields
#define TRB_SET_TYPE(trb, type)                                                \
    ((trb)->control = ((trb)->control & ~TRB_CONTROL_TYPE_MASK) |              \
                      ((type) << TRB_CONTROL_TYPE_SHIFT))
#define TRB_GET_TYPE(trb)                                                      \
    (((trb)->control & TRB_CONTROL_TYPE_MASK) >> TRB_CONTROL_TYPE_SHIFT)

#define TRB_SET_SLOT_ID(trb, slot)                                             \
    ((trb)->control = ((trb)->control & ~TRB_CONTROL_SLOT_ID_MASK) |           \
                      ((slot) << TRB_CONTROL_SLOT_ID_SHIFT))
#define TRB_GET_SLOT_ID(trb)                                                   \
    (((trb)->control & TRB_CONTROL_SLOT_ID_MASK) >> TRB_CONTROL_SLOT_ID_SHIFT)

#define TRB_SET_ENDPOINT_ID(trb, ep)                                           \
    ((trb)->control = ((trb)->control & ~TRB_CONTROL_ENDPOINT_ID_MASK) |       \
                      ((ep) << TRB_CONTROL_ENDPOINT_ID_SHIFT))
#define TRB_GET_ENDPOINT_ID(trb)                                               \
    (((trb)->control & TRB_CONTROL_ENDPOINT_ID_MASK) >>                        \
     TRB_CONTROL_ENDPOINT_ID_SHIFT)

// =============================================================================
// Ring Structure Definitions
// =============================================================================

#define XHCI_RING_SIZE 256 // Number of TRBs per ring

typedef struct {
    volatile XhciTrb *trbs; // TRB array (page-aligned, hardware-writable)
    uint64_t trbs_physical; // Physical address of TRB array

    uint32_t enqueue_index; // Next TRB to enqueue
    uint32_t dequeue_index; // Next TRB to dequeue
    uint32_t cycle_state;   // Current cycle bit value

    uint32_t size;             // Number of TRBs in ring
    bool producer_cycle_state; // Producer cycle state
    bool consumer_cycle_state; // Consumer cycle state

    // Statistics
    uint32_t enqueued_count; // Total TRBs enqueued
    uint32_t dequeued_count; // Total TRBs dequeued
} XhciRing;

// =============================================================================
// Command TRB Structures
// =============================================================================

// Enable Slot Command TRB
typedef struct {
    uint64_t reserved1;
    uint32_t reserved2;
    uint32_t control; // Slot type in bits 16-20, cycle bit, TRB type
} __attribute__((packed)) XhciEnableSlotTrb;

// Address Device Command TRB
typedef struct {
    uint64_t input_context_ptr; // Input context physical address
    uint32_t reserved;
    uint32_t control; // BSR bit 9, slot ID bits 24-31, cycle bit, TRB type
} __attribute__((packed)) XhciAddressDeviceTrb;

// Configure Endpoint Command TRB
typedef struct {
    uint64_t input_context_ptr; // Input context physical address
    uint32_t reserved;
    uint32_t control; // DC bit 9, slot ID bits 24-31, cycle bit, TRB type
} __attribute__((packed)) XhciConfigureEndpointTrb;

// =============================================================================
// Transfer TRB Structures
// =============================================================================

// Setup Stage TRB (for control transfers)
typedef struct {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
    uint32_t trb_transfer_length; // Always 8 for setup stage
    uint32_t control;             // TRT bits 16-17, cycle bit, TRB type
} __attribute__((packed)) XhciSetupStageTrb;

// Data Stage TRB (for control transfers)
typedef struct {
    uint64_t data_buffer_ptr;     // Data buffer physical address
    uint32_t trb_transfer_length; // Transfer length and TD size
    uint32_t control;             // DIR bit 16, cycle bit, TRB type
} __attribute__((packed)) XhciDataStageTrb;

// Status Stage TRB (for control transfers)
typedef struct {
    uint64_t reserved;
    uint32_t reserved2;
    uint32_t control; // DIR bit 16, cycle bit, TRB type
} __attribute__((packed)) XhciStatusStageTrb;

// Normal TRB (for bulk/interrupt transfers)
typedef struct {
    uint64_t data_buffer_ptr;     // Data buffer physical address
    uint32_t trb_transfer_length; // Transfer length and TD size
    uint32_t control;             // ISP bit 2, cycle bit, TRB type
} __attribute__((packed)) XhciNormalTrb;

// =============================================================================
// Event TRB Structures
// =============================================================================

// Transfer Event TRB
typedef struct {
    uint64_t trb_pointer;     // TRB pointer or buffer pointer
    uint32_t transfer_length; // Completion code and transfer length
    uint32_t control;         // Endpoint ID, slot ID, cycle bit, TRB type
} __attribute__((packed)) XhciTransferEventTrb;

// Command Completion Event TRB
typedef struct {
    uint64_t command_trb_pointer;  // Command TRB pointer
    uint32_t completion_parameter; // Completion code and parameter
    uint32_t control;              // VF ID, slot ID, cycle bit, TRB type
} __attribute__((packed)) XhciCommandCompletionEventTrb;

// Port Status Change Event TRB
typedef struct {
    uint32_t reserved1;
    uint32_t port_id; // Port ID in bits 24-31
    uint32_t reserved2;
    uint32_t control; // Completion code, cycle bit, TRB type
} __attribute__((packed)) XhciPortStatusChangeEventTrb;

// =============================================================================
// Completion Codes
// =============================================================================

#define XHCI_COMP_SUCCESS 1
#define XHCI_COMP_DATA_BUFFER_ERROR 2
#define XHCI_COMP_BABBLE_DETECTED 3
#define XHCI_COMP_USB_TRANSACTION_ERROR 4
#define XHCI_COMP_TRB_ERROR 5
#define XHCI_COMP_STALL_ERROR 6
#define XHCI_COMP_RESOURCE_ERROR 7
#define XHCI_COMP_BANDWIDTH_ERROR 8
#define XHCI_COMP_NO_SLOTS_AVAILABLE 9
#define XHCI_COMP_INVALID_STREAM_TYPE 10
#define XHCI_COMP_SLOT_NOT_ENABLED 11
#define XHCI_COMP_ENDPOINT_NOT_ENABLED 12
#define XHCI_COMP_SHORT_PACKET 13
#define XHCI_COMP_RING_UNDERRUN 14
#define XHCI_COMP_RING_OVERRUN 15
#define XHCI_COMP_VF_EVENT_RING_FULL 16
#define XHCI_COMP_PARAMETER_ERROR 17
#define XHCI_COMP_BANDWIDTH_OVERRUN 18
#define XHCI_COMP_CONTEXT_STATE_ERROR 19
#define XHCI_COMP_NO_PING_RESPONSE 20
#define XHCI_COMP_EVENT_RING_FULL 21
#define XHCI_COMP_INCOMPATIBLE_DEVICE 22
#define XHCI_COMP_MISSED_SERVICE 23
#define XHCI_COMP_COMMAND_RING_STOPPED 24
#define XHCI_COMP_COMMAND_ABORTED 25
#define XHCI_COMP_STOPPED 26
#define XHCI_COMP_STOPPED_LENGTH_INVALID 27
#define XHCI_COMP_STOPPED_SHORT_PACKET 28
#define XHCI_COMP_MAX_EXIT_LATENCY 29
#define XHCI_COMP_ISOCH_BUFFER_OVERRUN 31
#define XHCI_COMP_EVENT_LOST 32
#define XHCI_COMP_UNDEFINED 33
#define XHCI_COMP_INVALID_STREAM_ID 34
#define XHCI_COMP_SECONDARY_BW_ERROR 35
#define XHCI_COMP_SPLIT_TRANSACTION 36

// =============================================================================
// Function Prototypes
// =============================================================================

// Ring Management
int xhci_ring_init(XhciRing *ring, uint32_t size);
void xhci_ring_free(XhciRing *ring);
XhciTrb *xhci_ring_enqueue_trb(XhciRing *ring);
volatile XhciTrb *xhci_ring_dequeue_trb(XhciRing *ring);
bool xhci_ring_has_space(XhciRing *ring, uint32_t num_trbs);
void xhci_ring_inc_enqueue(XhciRing *ring);
void xhci_ring_inc_dequeue(XhciRing *ring);

// TRB Utilities
const char *xhci_trb_type_string(uint32_t trb_type);
const char *xhci_completion_code_string(uint32_t completion_code);

#endif