#pragma once

/* ***********************************************************************************************************
    PROCESSOR AND ROM DEFINES
*********************************************************************************************************** */
// ROM HEADER *****
#define ROM_HEAD_ADDR				0x0100
#define ROM_HEAD_SIZE				0x0050

#define ROM_HEAD_TITLE				0x0134
#define ROM_HEAD_CGBFLAG			0x0143				// until here; 0x80 = supports CGB, 0xC0 = CGB only		(bit 6 gets ignored)
#define ROM_HEAD_NEWLIC				0x0144				// 2 char ascii; only if oldlic = 0x33
#define ROM_HEAD_SGBFLAG			0x0146				// 0x03 = en, else not
#define ROM_HEAD_HW_TYPE			0x0147				// hardware on cartridge
#define ROM_HEAD_ROMSIZE			0x0148
#define ROM_HEAD_RAMSIZE			0x0149
#define ROM_HEAD_DEST				0x014A				// 0x00 = japan, 0x01 = overseas
#define ROM_HEAD_OLDLIC				0x014B
#define ROM_HEAD_VERSION			0x014C
#define ROM_HEAD_CHKSUM				0x014D

#define ROM_HEAD_TITLE_SIZE_CGB		15
#define ROM_HEAD_TITLE_SIZE_GB		16
