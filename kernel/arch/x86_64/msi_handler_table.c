/*
 * stage3 - MSI interrupt handler table
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "x86_64/kdrivers/msi.h"

extern void msi_handler_0x40(void);
extern void msi_handler_0x41(void);
extern void msi_handler_0x42(void);
extern void msi_handler_0x43(void);
extern void msi_handler_0x44(void);
extern void msi_handler_0x45(void);
extern void msi_handler_0x46(void);
extern void msi_handler_0x47(void);
extern void msi_handler_0x48(void);
extern void msi_handler_0x49(void);
extern void msi_handler_0x4A(void);
extern void msi_handler_0x4B(void);
extern void msi_handler_0x4C(void);
extern void msi_handler_0x4D(void);
extern void msi_handler_0x4E(void);
extern void msi_handler_0x4F(void);
extern void msi_handler_0x50(void);
extern void msi_handler_0x51(void);
extern void msi_handler_0x52(void);
extern void msi_handler_0x53(void);
extern void msi_handler_0x54(void);
extern void msi_handler_0x55(void);
extern void msi_handler_0x56(void);
extern void msi_handler_0x57(void);
extern void msi_handler_0x58(void);
extern void msi_handler_0x59(void);
extern void msi_handler_0x5A(void);
extern void msi_handler_0x5B(void);
extern void msi_handler_0x5C(void);
extern void msi_handler_0x5D(void);
extern void msi_handler_0x5E(void);
extern void msi_handler_0x5F(void);
extern void msi_handler_0x60(void);
extern void msi_handler_0x61(void);
extern void msi_handler_0x62(void);
extern void msi_handler_0x63(void);
extern void msi_handler_0x64(void);
extern void msi_handler_0x65(void);
extern void msi_handler_0x66(void);
extern void msi_handler_0x67(void);
extern void msi_handler_0x68(void);
extern void msi_handler_0x69(void);
extern void msi_handler_0x6A(void);
extern void msi_handler_0x6B(void);
extern void msi_handler_0x6C(void);
extern void msi_handler_0x6D(void);
extern void msi_handler_0x6E(void);
extern void msi_handler_0x6F(void);
extern void msi_handler_0x70(void);
extern void msi_handler_0x71(void);
extern void msi_handler_0x72(void);
extern void msi_handler_0x73(void);
extern void msi_handler_0x74(void);
extern void msi_handler_0x75(void);
extern void msi_handler_0x76(void);
extern void msi_handler_0x77(void);
extern void msi_handler_0x78(void);
extern void msi_handler_0x79(void);
extern void msi_handler_0x7A(void);
extern void msi_handler_0x7B(void);
extern void msi_handler_0x7C(void);
extern void msi_handler_0x7D(void);
extern void msi_handler_0x7E(void);
extern void msi_handler_0x7F(void);
extern void msi_handler_0x80(void);
extern void msi_handler_0x81(void);
extern void msi_handler_0x82(void);
extern void msi_handler_0x83(void);
extern void msi_handler_0x84(void);
extern void msi_handler_0x85(void);
extern void msi_handler_0x86(void);
extern void msi_handler_0x87(void);
extern void msi_handler_0x88(void);
extern void msi_handler_0x89(void);
extern void msi_handler_0x8A(void);
extern void msi_handler_0x8B(void);
extern void msi_handler_0x8C(void);
extern void msi_handler_0x8D(void);
extern void msi_handler_0x8E(void);
extern void msi_handler_0x8F(void);
extern void msi_handler_0x90(void);
extern void msi_handler_0x91(void);
extern void msi_handler_0x92(void);
extern void msi_handler_0x93(void);
extern void msi_handler_0x94(void);
extern void msi_handler_0x95(void);
extern void msi_handler_0x96(void);
extern void msi_handler_0x97(void);
extern void msi_handler_0x98(void);
extern void msi_handler_0x99(void);
extern void msi_handler_0x9A(void);
extern void msi_handler_0x9B(void);
extern void msi_handler_0x9C(void);
extern void msi_handler_0x9D(void);
extern void msi_handler_0x9E(void);
extern void msi_handler_0x9F(void);
extern void msi_handler_0xA0(void);
extern void msi_handler_0xA1(void);
extern void msi_handler_0xA2(void);
extern void msi_handler_0xA3(void);
extern void msi_handler_0xA4(void);
extern void msi_handler_0xA5(void);
extern void msi_handler_0xA6(void);
extern void msi_handler_0xA7(void);
extern void msi_handler_0xA8(void);
extern void msi_handler_0xA9(void);
extern void msi_handler_0xAA(void);
extern void msi_handler_0xAB(void);
extern void msi_handler_0xAC(void);
extern void msi_handler_0xAD(void);
extern void msi_handler_0xAE(void);
extern void msi_handler_0xAF(void);
extern void msi_handler_0xB0(void);
extern void msi_handler_0xB1(void);
extern void msi_handler_0xB2(void);
extern void msi_handler_0xB3(void);
extern void msi_handler_0xB4(void);
extern void msi_handler_0xB5(void);
extern void msi_handler_0xB6(void);
extern void msi_handler_0xB7(void);
extern void msi_handler_0xB8(void);
extern void msi_handler_0xB9(void);
extern void msi_handler_0xBA(void);
extern void msi_handler_0xBB(void);
extern void msi_handler_0xBC(void);
extern void msi_handler_0xBD(void);
extern void msi_handler_0xBE(void);
extern void msi_handler_0xBF(void);
extern void msi_handler_0xC0(void);
extern void msi_handler_0xC1(void);
extern void msi_handler_0xC2(void);
extern void msi_handler_0xC3(void);
extern void msi_handler_0xC4(void);
extern void msi_handler_0xC5(void);
extern void msi_handler_0xC6(void);
extern void msi_handler_0xC7(void);
extern void msi_handler_0xC8(void);
extern void msi_handler_0xC9(void);
extern void msi_handler_0xCA(void);
extern void msi_handler_0xCB(void);
extern void msi_handler_0xCC(void);
extern void msi_handler_0xCD(void);
extern void msi_handler_0xCE(void);
extern void msi_handler_0xCF(void);
extern void msi_handler_0xD0(void);
extern void msi_handler_0xD1(void);
extern void msi_handler_0xD2(void);
extern void msi_handler_0xD3(void);
extern void msi_handler_0xD4(void);
extern void msi_handler_0xD5(void);
extern void msi_handler_0xD6(void);
extern void msi_handler_0xD7(void);
extern void msi_handler_0xD8(void);
extern void msi_handler_0xD9(void);
extern void msi_handler_0xDA(void);
extern void msi_handler_0xDB(void);
extern void msi_handler_0xDC(void);
extern void msi_handler_0xDD(void);
extern void msi_handler_0xDE(void);
extern void msi_handler_0xDF(void);

void (*msi_handler_table[MSI_VECTOR_COUNT])(void) = {
        msi_handler_0x40, msi_handler_0x41, msi_handler_0x42, msi_handler_0x43,
        msi_handler_0x44, msi_handler_0x45, msi_handler_0x46, msi_handler_0x47,
        msi_handler_0x48, msi_handler_0x49, msi_handler_0x4A, msi_handler_0x4B,
        msi_handler_0x4C, msi_handler_0x4D, msi_handler_0x4E, msi_handler_0x4F,
        msi_handler_0x50, msi_handler_0x51, msi_handler_0x52, msi_handler_0x53,
        msi_handler_0x54, msi_handler_0x55, msi_handler_0x56, msi_handler_0x57,
        msi_handler_0x58, msi_handler_0x59, msi_handler_0x5A, msi_handler_0x5B,
        msi_handler_0x5C, msi_handler_0x5D, msi_handler_0x5E, msi_handler_0x5F,
        msi_handler_0x60, msi_handler_0x61, msi_handler_0x62, msi_handler_0x63,
        msi_handler_0x64, msi_handler_0x65, msi_handler_0x66, msi_handler_0x67,
        msi_handler_0x68, msi_handler_0x69, msi_handler_0x6A, msi_handler_0x6B,
        msi_handler_0x6C, msi_handler_0x6D, msi_handler_0x6E, msi_handler_0x6F,
        msi_handler_0x70, msi_handler_0x71, msi_handler_0x72, msi_handler_0x73,
        msi_handler_0x74, msi_handler_0x75, msi_handler_0x76, msi_handler_0x77,
        msi_handler_0x78, msi_handler_0x79, msi_handler_0x7A, msi_handler_0x7B,
        msi_handler_0x7C, msi_handler_0x7D, msi_handler_0x7E, msi_handler_0x7F,
        msi_handler_0x80, msi_handler_0x81, msi_handler_0x82, msi_handler_0x83,
        msi_handler_0x84, msi_handler_0x85, msi_handler_0x86, msi_handler_0x87,
        msi_handler_0x88, msi_handler_0x89, msi_handler_0x8A, msi_handler_0x8B,
        msi_handler_0x8C, msi_handler_0x8D, msi_handler_0x8E, msi_handler_0x8F,
        msi_handler_0x90, msi_handler_0x91, msi_handler_0x92, msi_handler_0x93,
        msi_handler_0x94, msi_handler_0x95, msi_handler_0x96, msi_handler_0x97,
        msi_handler_0x98, msi_handler_0x99, msi_handler_0x9A, msi_handler_0x9B,
        msi_handler_0x9C, msi_handler_0x9D, msi_handler_0x9E, msi_handler_0x9F,
        msi_handler_0xA0, msi_handler_0xA1, msi_handler_0xA2, msi_handler_0xA3,
        msi_handler_0xA4, msi_handler_0xA5, msi_handler_0xA6, msi_handler_0xA7,
        msi_handler_0xA8, msi_handler_0xA9, msi_handler_0xAA, msi_handler_0xAB,
        msi_handler_0xAC, msi_handler_0xAD, msi_handler_0xAE, msi_handler_0xAF,
        msi_handler_0xB0, msi_handler_0xB1, msi_handler_0xB2, msi_handler_0xB3,
        msi_handler_0xB4, msi_handler_0xB5, msi_handler_0xB6, msi_handler_0xB7,
        msi_handler_0xB8, msi_handler_0xB9, msi_handler_0xBA, msi_handler_0xBB,
        msi_handler_0xBC, msi_handler_0xBD, msi_handler_0xBE, msi_handler_0xBF,
        msi_handler_0xC0, msi_handler_0xC1, msi_handler_0xC2, msi_handler_0xC3,
        msi_handler_0xC4, msi_handler_0xC5, msi_handler_0xC6, msi_handler_0xC7,
        msi_handler_0xC8, msi_handler_0xC9, msi_handler_0xCA, msi_handler_0xCB,
        msi_handler_0xCC, msi_handler_0xCD, msi_handler_0xCE, msi_handler_0xCF,
        msi_handler_0xD0, msi_handler_0xD1, msi_handler_0xD2, msi_handler_0xD3,
        msi_handler_0xD4, msi_handler_0xD5, msi_handler_0xD6, msi_handler_0xD7,
        msi_handler_0xD8, msi_handler_0xD9, msi_handler_0xDA, msi_handler_0xDB,
        msi_handler_0xDC, msi_handler_0xDD, msi_handler_0xDE, msi_handler_0xDF};