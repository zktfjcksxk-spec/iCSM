//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
// Instead of integers, KeyValues3 uses GUIDs to track encoding and format versions.
//
// The reason for this is that when a codebase branches and evolves in parallel over a long period of time,
// there is a decent chance that 'version 10' will be bumped to 'version 11' in both branches but mean
// totally different things. Then when those branches merge later you cannot distinguish between the two
// different types of 'version 11' files. This has happened in the past and caused a bunch of grief.
//
// To create a UUID use guidgen.exe (ships with Visual Studio) or a website like https://www.guidgen.com/
//
// Conversions between formats are implemented in kv3formats.cpp - the ConvertKV3Format function will
// chain together as many conversions are necessary in order to get from one format to another.
//
//==================================================================================================

#pragma once

#include "tier0/platform.h" // V_uuid_t
#include "tier0/dbg.h"		// AssertDbg
#include "tier1/strtools.h" // V_strcmp


//------------------------------------------------------------------------------
// A UUID (GUID) plus a human-friendly name - used to disambiguate encoding and formats
// Friendly names can't carry actionable information (they don't exist in the binary encoding), but are present in the text format to make it more human-friendly
//------------------------------------------------------------------------------
struct KV3ID_t
{
	KV3ID_t( )
		: m_pName( "" )
		, m_UUID( NULL_UUID )
	{
	}

	KV3ID_t( const char *pName, const V_uuid_t &id )
		: m_pName( pName )
		, m_UUID( id )
	{
	}

	KV3ID_t( const KV3ID_t &other )
		: m_pName( other.m_pName )
		, m_UUID( other.m_UUID )
	{
	}

	KV3ID_t( KV3ID_t &&other )
		: m_pName( other.m_pName )
		, m_UUID( other.m_UUID )
	{
	}

	bool operator==( const KV3ID_t &rhs ) const
	{
		return ( m_UUID == rhs.m_UUID );
	}

	bool operator!=( const KV3ID_t &rhs ) const
	{
		return !( *this == rhs );
	}

	const char *m_pName;	// NOTE: This may be null even if the UUID is valid (eg. when we load a binary file)
	const V_uuid_t &m_UUID;
};

#define DEFINE_KV3_ID( identifier, name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8 ) \
	DEFINE_UUID( identifier##_UUID, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8 ); \
	extern const KV3ID_t SELECTANY identifier( name, identifier##_UUID );


//--------------------------------------------------------------------------------------------------
// KV3 Encodings
//--------------------------------------------------------------------------------------------------
// The ways that KV3 data can be serialized
//--------------------------------------------------------------------------------------------------

// Text encoding
DEFINE_KV3_ID( KV3_ENCODING_TEXT, "text",								0xe21c7f3c, 0x8a33, 0x41c5, 0x99, 0x77, 0xa7, 0x6d, 0x3a, 0x32, 0xaa, 0xd );	// {E21C7F3C-8A33-41C5-9977-A76D3A32AA0D}

// Uncompressed binary encoding
DEFINE_KV3_ID( KV3_ENCODING_BINARY_UNCOMPRESSED, "binary",				0x1b860500, 0xf7d8, 0x40c1, 0xad, 0x82, 0x75, 0xa4, 0x82, 0x67, 0xe7, 0x14 );	// {1B860500-F7D8-40C1-AD82-75A48267E714}

// Compressed binary encoding - prefer this for binary KV3 data (it's a simple CBlockCompress wrapper around KV3_ENCODING_BINARY_UNCOMPRESSED)
DEFINE_KV3_ID( KV3_ENCODING_BINARY_BLOCK_COMPRESSED, "binary_bc",		0x95791a46, 0x95bc, 0x4f6c, 0xa7, 0xb, 0x5, 0xbc, 0xa1, 0xb7, 0xdf, 0xd2 );		// {95791A46-95BC-4F6C-A70B-05BCA1B7DFD2}


//--------------------------------------------------------------------------------------------------
// KV3 Formats
//--------------------------------------------------------------------------------------------------
// How to interpret loaded KV3 data - see kv3formats.cpp for conversion routines
//--------------------------------------------------------------------------------------------------

// The default format for arbitrary data - expresses no assumption about how the data will be used
DEFINE_KV3_ID( KV3_FORMAT_GENERIC, "generic",							0x7412167c, 0x6e9, 0x4698, 0xaf, 0xf2, 0xe6, 0x3e, 0xb5, 0x90, 0x37, 0xe7 );	// {7412167C-06E9-4698-AFF2-E63EB59037E7}


//--------------------------------------------------------------------------------------------------
// Modeldoc Formats in: "modeldoc_lib/modeldoc_formats.h"
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
// Formats used in unit tests
//--------------------------------------------------------------------------------------------------
DEFINE_KV3_ID( KV3_FORMAT_TEST_A,	"TestFormatA",						0x1d92fe13, 0x3c51, 0x4492, 0x9f, 0x22, 0x96, 0x2e, 0x8c, 0xa8, 0x35, 0x74	);	// {1D92FE13-3C51-4492-9F22-962E8CA83574}
DEFINE_KV3_ID( KV3_FORMAT_TEST_B,	"TestFormatB",						0x8c9fd954, 0x16d5, 0x4dec, 0x88, 0x70, 0x16, 0xd0, 0x41, 0xe1, 0x38, 0x78	);	// {8C9FD954-16D5-4DEC-8870-16D041E13878}
DEFINE_KV3_ID( KV3_FORMAT_TEST_C,	"TestFormatC",						0xbe372c39, 0x4063, 0x45fa, 0xa1, 0x58, 0xd6, 0x17, 0x7, 0x5d, 0x67, 0x98	);	// {BE372C39-4063-45FA-A158-D617075D6798}
DEFINE_KV3_ID( KV3_FORMAT_TEST_D,	"TestFormatD",						0x56ee5ea8, 0x7b54, 0x4a8b, 0xa9, 0xfd, 0xac, 0xa1, 0x67, 0x9b, 0x40, 0xdf	);	// {56EE5EA8-7B54-4A8B-A9FD-ACA1679B40DF}
DEFINE_KV3_ID( KV3_FORMAT_TEST_E,	"TestFormatE",						0x622a055f, 0x4781, 0x4e4b, 0x9f, 0x20, 0xf3, 0xa5, 0x64, 0x5b, 0xe4, 0xf4	);	// {622A055F-4781-4E4B-9F20-F3A5645BE4F4}
