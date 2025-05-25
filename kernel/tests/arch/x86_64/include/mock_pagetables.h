/*
 * stage3 - Mock Recursive mapping access functions for testing
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_MOCK_RECURSIVE_H
#define __ANOS_KERNEL_MOCK_RECURSIVE_H

#include <stdint.h>

#include "vmm/vmmapper.h"

static const uintptr_t L1_LSHIFT = 39;
static const uintptr_t L2_LSHIFT = 30;
static const uintptr_t L3_LSHIFT = 21;
static const uintptr_t L4_LSHIFT = 12;
static const uintptr_t L1_RSHIFT = 12;
static const uintptr_t L2_RSHIFT = 21;
static const uintptr_t L3_RSHIFT = 30;
static const uintptr_t L4_RSHIFT = 39;
static const uintptr_t LVL_MASK = 0x1ff;

// TODO this is brittle as all hell, and makes modifying / refactoring
// in tests a real pain in the arse.
//
// If you're here because some tests started randomly failing to build
// after some change, it's probably down to this.
//
// The usual trick is to:
//
//  * Be sure to include "munit.h" **first**
//  * _then_ include "mock_recursive" afterward, even if your test
//    doesn't seem to need it.
//
// But really this should be fixed properly. Which if recursive goes
// away it will do, but otherwise, well, welcome to the club :D
//
// (Oh, and if you add those two steps in your test, make sure you have
// a blank line between them or clang-format will "helpfully" reorder
// them and break everything again...)
//
#ifdef MUNIT_H
PageTable empty_pml4 __attribute__((__aligned__(4096)));

PageTable complete_pml4 __attribute__((__aligned__(4096)));
PageTable complete_pdpt __attribute__((__aligned__(4096)));
PageTable complete_pd __attribute__((__aligned__(4096)));
PageTable complete_pt __attribute__((__aligned__(4096)));

PageTable *current_recursive_pml4 = &complete_pml4;
#else
#include <stdio.h>

#define PML4ENTRY(addr) (((unsigned short)((addr & 0x0000ff8000000000) >> 39)))
#define PDPTENTRY(addr) (((unsigned short)((addr & 0x0000007fc0000000) >> 30)))
#define PDENTRY(addr) (((unsigned short)((addr & 0x000000003fe00000) >> 21)))
#define PTENTRY(addr) (((unsigned short)((addr & 0x00000000001ff000) >> 12)))

#define MEM(arg) ((arg & ~0xfff))

extern PageTable empty_pml4;

extern PageTable complete_pml4;
extern PageTable complete_pdpt;
extern PageTable complete_pd;
extern PageTable complete_pt;

extern PageTable *current_recursive_pml4;

#endif
#endif //__ANOS_KERNEL_MOCK_RECURSIVE_H