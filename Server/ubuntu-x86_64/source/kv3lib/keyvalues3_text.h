//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#pragma once

#include "tier0/platform.h"

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#define KV3_BINARY_ENCODING_BEGIN_MARKER 0x03564B56 // VKV{0x03} - intentionally contains a non-printable character so that heuristics (like perforce) won't mis-identify as a text file
#define KV3_BINARY_ENCODING_EOF_MARKER 0xFFFFFFFF


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#define KV3_TOKEN_OPEN_CURLY "{"
#define KV3_TOKEN_CLOSE_CURLY "}"
#define KV3_TOKEN_OPEN_SQUARE "["
#define KV3_TOKEN_CLOSE_SQUARE "]"
#define KV3_TOKEN_EQUAL "="
#define KV3_TOKEN_ARRAY_SEPARATOR ","
#define KV3_TOKEN_WHITESPACE " \t\n"
#define KV3_TOKEN_QUOTECHARS "\'\""
#define KV3_TOKEN_NULL "NULL"
#define KV3_TOKEN_BEGIN_HEADER "<!--"
#define KV3_TOKEN_END_HEADER "-->"
#define KV3_TOKEN_HEADER_ID "kv3"
#define KV3_TOKEN_HEADER_ENCODING "encoding"
#define KV3_TOKEN_HEADER_FORMAT "format"
#define KV3_TOKEN_HEADER_VERSION "version"
#define KV3_TOKEN_SEMI ";"
#define KV3_TOKEN_COLON ":"
#define KV3_TOKEN_VALUE_TAG_COMBINER "+"
#define KV3_TOKEN_BEGIN_BINARY_BLOB "#"


//--------------------------------------------------------------------------------------------------
// Separate enum for binary encoding to decouple runtime from disk format
//--------------------------------------------------------------------------------------------------
enum KeyValues3BinaryType_t
{
	KEYVALUES3_BINARY_TYPE_INVALID,

	KEYVALUES3_BINARY_TYPE_NULL,

	KEYVALUES3_BINARY_TYPE_BOOL,

	KEYVALUES3_BINARY_TYPE_INT64,
	KEYVALUES3_BINARY_TYPE_UINT64,

	KEYVALUES3_BINARY_TYPE_DOUBLE,

	KEYVALUES3_BINARY_TYPE_STRING,
	KEYVALUES3_BINARY_TYPE_BINARY_BLOB,

	KEYVALUES3_BINARY_TYPE_ARRAY,
	KEYVALUES3_BINARY_TYPE_TABLE,

	KEYVALUES3_BINARY_TYPE_MAX,
};

#define KEYVALUES3_BINARY_TYPE_FLAG_HAS_VALUE_FLAGS (1<<7)

COMPILE_TIME_ASSERT( KEYVALUES3_BINARY_TYPE_MAX < KEYVALUES3_BINARY_TYPE_FLAG_HAS_VALUE_FLAGS );


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#define ROOT_KEYNAME_MEMBER_NAME "_root_keyname"

bool KV3Helper_IsUTF16Buffer( const char *pBuffer, int nLen );

