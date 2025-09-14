/*
 * stage3 - Types and constants for Limine bootloader interface
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef _ANOS_ARCH_COMMON_LIMINE_H
#define _ANOS_ARCH_COMMON_LIMINE_H

#include <stdint.h>

#include "machine.h"

typedef struct {
    uint64_t id[4];
    uint64_t revision;
    Limine_MemMap *memmap;
} __attribute__((packed)) Limine_MemMapRequest;

typedef struct {
    uint64_t revision;
    void *address;
} __attribute__((packed)) Limine_Rsdp;

typedef struct {
    uint64_t id[4];
    uint64_t revision;
    Limine_Rsdp *rsdp;
} __attribute__((packed)) Limine_RsdpRequest;

typedef struct {
    uint64_t pitch;
    uint64_t width;
    uint64_t height;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
} __attribute__((packed)) Limine_VideoMode;

typedef struct {
    void *address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp; // Bits per pixel
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t unused[7];
    uint64_t edid_size;
    void *edid;

    /* Response revision 1 */
    uint64_t mode_count;
    Limine_VideoMode **modes;
} __attribute__((packed)) Limine_FrameBuffer;

typedef struct {
    uint64_t revision;
    uint64_t framebuffer_count;
    Limine_FrameBuffer **framebuffers;
} __attribute__((packed)) Limine_FrameBuffers;

typedef struct {
    uint64_t id[4];
    uint64_t revision;
    Limine_FrameBuffers *response;
} __attribute__((packed)) Limine_FrameBufferRequest;

typedef struct {
    uint64_t revision;
    uint64_t offset;
} __attribute__((packed)) Limine_HHDM;

typedef struct {
    uint64_t id[4];
    uint64_t revision;
    Limine_HHDM *response;
} __attribute__((packed)) Limine_HHDMRequest;

typedef struct {
    const char *path;
    const char *string;
    uint64_t flags;
} __attribute__((packed)) Limine_InternalModule;

typedef struct {
    uint32_t a;
    uint16_t b;
    uint16_t c;
    uint8_t d[8];
} __attribute__((packed)) Limine_UUID;

typedef struct {
    uint64_t revision;
    void *address;
    uint64_t size;
    char *path;
    char *string;
    uint32_t media_type;
    uint32_t unused;
    uint32_t tftp_ip;
    uint32_t tftp_port;
    uint32_t partition_index;
    uint32_t mbr_disk_id;
    Limine_UUID gpt_disk_uuid;
    Limine_UUID gpt_part_uuid;
    Limine_UUID part_uuid;
} __attribute__((packed)) Limine_File;

typedef struct {
    uint64_t revision;
    uint64_t module_count;
    Limine_File **modules;
} __attribute__((packed)) Limine_ModuleResponse;

typedef struct {
    uint64_t id[4];
    uint64_t revision;
    Limine_ModuleResponse *response;
    uint64_t internal_module_count;
    struct Limine_InternalModule **internal_modules;
} __attribute__((packed)) Limine_ModuleRequest;

#define LIMINE_COMMON_MAGIC 0xc7b1dd30df4c8b88, 0x0a82e883a194f07b

#define LIMINE_MEMMAP_REQUEST                                                  \
    {LIMINE_COMMON_MAGIC, 0x67cf3d9d378a806f, 0xe304acdfc50c3c62}
#define LIMINE_RSDP_REQUEST                                                    \
    {LIMINE_COMMON_MAGIC, 0xc5e77b6b397e7b43, 0x27637845accdcf3c}
#define LIMINE_FRAMEBUFFER_REQUEST                                             \
    {LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75, 0xa3148604f6fab11b}
#define LIMINE_HHDM_REQUEST                                                    \
    {LIMINE_COMMON_MAGIC, 0x48dcf1cb8ad2b852, 0x63984e959a98244b}
#define LIMINE_MODULE_REQUEST                                                  \
    {LIMINE_COMMON_MAGIC, 0x3e7e279702be32af, 0xca1c4f3bd1280cee}

#define LIMINE_MEDIA_TYPE_GENERIC 0
#define LIMINE_MEDIA_TYPE_OPTICAL 1
#define LIMINE_MEDIA_TYPE_TFTP 2

#endif