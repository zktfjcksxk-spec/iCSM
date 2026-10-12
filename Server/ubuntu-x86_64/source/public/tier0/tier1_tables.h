//========== Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Global tables used by tier1
//
//===========================================================================//

#ifndef TIER1_TABLES_H
#define TIER1_TABLES_H
#pragma once

#if defined(TIER0_STATIC_LIB)
#define TIER1_TABLE_DECL extern
#else
#if defined(BUILD_SHARED_TABLES)
#define TIER1_TABLE_DECL DLL_EXPORT
#else
#define TIER1_TABLE_DECL DLL_IMPORT
#endif
#endif

#define NUM_CRC32_TABLE_ENTRIES 256

TIER1_TABLE_DECL const uint32 g_Tier1_CRC32Table[NUM_CRC32_TABLE_ENTRIES];

#define NUM_CRC64_TABLE_ENTRIES 256

TIER1_TABLE_DECL const uint64 g_Tier1_CRC64Table[NUM_CRC64_TABLE_ENTRIES];

struct Tier1FullHTMLEntity_t
{
	uchar32 uCharCode;
	const char *pchEntity;
	int nEntityLength;
};

TIER1_TABLE_DECL const Tier1FullHTMLEntity_t g_Tier1_FullHTMLEntities[];

TIER1_TABLE_DECL uint32 g_Tier1_BitWriteMasks[32][33];

#if !PLAT_LITTLE_ENDIAN
TIER1_TABLE_DECL uint32 g_Tier1_LittleBits[32];
#endif

TIER1_TABLE_DECL uint32 g_Tier1_ExtraMasks[33];

#endif // TIER1_TABLES_H
