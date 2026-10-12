//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "tier0/dbg.h"
#include "tier1/strtools.h"


//-----------------------------------------------------------------------------
// Purpose: determine if a uchar32 represents a valid Unicode code point
//-----------------------------------------------------------------------------
bool Q_IsValidUChar32( uchar32 uVal )
{
	// Values > 0x10FFFF are explicitly invalid; ditto for UTF-16 surrogate halves,
	// values ending in FFFE or FFFF, or values in the 0x00FDD0-0x00FDEF reserved range
	return ( uVal < 0x110000u ) && ( (uVal - 0x00D800u) > 0x7FFu ) && ( (uVal & 0xFFFFu) < 0xFFFEu ) && ( ( uVal - 0x00FDD0u ) > 0x1Fu );
}

//-----------------------------------------------------------------------------
// Purpose: return number of UTF-8 bytes required to encode a Unicode code point
//-----------------------------------------------------------------------------
int Q_UChar32ToUTF8Len( uchar32 uVal )
{
	DbgAssert( Q_IsValidUChar32( uVal ) );
	if ( uVal <= 0x7F )
		return 1;
	if ( uVal <= 0x7FF )
		return 2;
	if ( uVal <= 0xFFFF )
		return 3;
	return 4;
}


//-----------------------------------------------------------------------------
// Purpose: return number of UTF-16 elements required to encode a Unicode code point
//-----------------------------------------------------------------------------
int Q_UChar32ToUTF16Len( uchar32 uVal )
{
	DbgAssert( Q_IsValidUChar32( uVal ) );
	if ( uVal <= 0xFFFF )
		return 1;
	return 2;
}


//-----------------------------------------------------------------------------
// Purpose: encode Unicode code point as UTF-8, returns number of bytes written
//-----------------------------------------------------------------------------
int Q_UChar32ToUTF8( uchar32 uVal, char *pUTF8Out )
{
	DbgAssert( Q_IsValidUChar32( uVal ) );
	if ( uVal <= 0x7F )
	{
		pUTF8Out[0] = (unsigned char) uVal;
		return 1;
	}
	if ( uVal <= 0x7FF )
	{
		pUTF8Out[0] = (unsigned char)(uVal >> 6) | 0xC0;
		pUTF8Out[1] = (unsigned char)(uVal & 0x3F) | 0x80;
		return 2;
	}
	if ( uVal <= 0xFFFF )
	{
		pUTF8Out[0] = (unsigned char)(uVal >> 12) | 0xE0;
		pUTF8Out[1] = (unsigned char)((uVal >> 6) & 0x3F) | 0x80;
		pUTF8Out[2] = (unsigned char)(uVal & 0x3F) | 0x80;
		return 3;
	}
	pUTF8Out[0] = (unsigned char)((uVal >> 18) & 0x07) | 0xF0;
	pUTF8Out[1] = (unsigned char)((uVal >> 12) & 0x3F) | 0x80;
	pUTF8Out[2] = (unsigned char)((uVal >> 6) & 0x3F) | 0x80;
	pUTF8Out[3] = (unsigned char)(uVal & 0x3F) | 0x80;
	return 4;
}

//-----------------------------------------------------------------------------
// Purpose: encode Unicode code point as UTF-16, returns number of elements written
//-----------------------------------------------------------------------------
int Q_UChar32ToUTF16( uchar32 uVal, uchar16 *pUTF16Out )
{
	DbgAssert( Q_IsValidUChar32( uVal ) );
	if ( uVal <= 0xFFFF )
	{
		pUTF16Out[0] = (uchar16) uVal;
		return 1;
	}
	uVal -= 0x010000;
	pUTF16Out[0] = (uchar16)(uVal >> 10) | 0xD800;
	pUTF16Out[1] = (uchar16)(uVal & 0x3FF) | 0xDC00;
	return 2;
}
	

// Decode one character from a UTF-8 encoded string. Treats 6-byte CESU-8 sequences
// as a single character, as if they were a correctly-encoded 4-byte UTF-8 sequence.
int Q_UTF8ToUChar32( const char *pUTF8_, uchar32 &uValueOut, bool &bErrorOut )
{
	const uint8 *pUTF8 = (const uint8 *)pUTF8_;

	int nBytes = 1;
	uint32 uValue = pUTF8[0];
	uint32 uMinValue = 0;

	// 0....... single byte
	if ( uValue < 0x80 )
		goto decodeFinishedNoCheck;

	// Expecting at least a two-byte sequence with 0xC0 <= first <= 0xF7 (110...... and 11110...)
	if ( (uValue - 0xC0u) > 0x37u || ( pUTF8[1] & 0xC0 ) != 0x80 )
		goto decodeError;

	uValue = (uValue << 6) - (0xC0 << 6) + pUTF8[1] - 0x80;
	nBytes = 2;
	uMinValue = 0x80;

	// 110..... two-byte lead byte
	if ( !( uValue & (0x20 << 6) ) )
		goto decodeFinished;

	// Expecting at least a three-byte sequence
	if ( ( pUTF8[2] & 0xC0 ) != 0x80 )
		goto decodeError;

	uValue = (uValue << 6) - (0x20 << 12) + pUTF8[2] - 0x80;
	nBytes = 3;
	uMinValue = 0x800;

	// 1110.... three-byte lead byte
	if ( !( uValue & (0x10 << 12) ) )
		goto decodeFinishedMaybeCESU8;

	// Expecting a four-byte sequence, longest permissible in UTF-8
	if ( ( pUTF8[3] & 0xC0 ) != 0x80 )
		goto decodeError;

	uValue = (uValue << 6) - (0x10 << 18) + pUTF8[3] - 0x80;
	nBytes = 4;
	uMinValue = 0x10000;

	// 11110... four-byte lead byte. fall through to finished.

decodeFinished:
	if ( uValue >= uMinValue && Q_IsValidUChar32( uValue ) )
	{
decodeFinishedNoCheck:
		uValueOut = uValue;
		bErrorOut = false;
		return nBytes;
	}
decodeError:
	uValueOut = '?';
	bErrorOut = true;
	return nBytes;

decodeFinishedMaybeCESU8:
	// Do we have a full UTF-16 surrogate pair that's been UTF-8 encoded afterwards?
	// That is, do we have 0xD800-0xDBFF followed by 0xDC00-0xDFFF? If so, decode it all.
	if ( ( uValue - 0xD800u ) < 0x400u && pUTF8[3] == 0xED && (uint8)( pUTF8[4] - 0xB0 ) < 0x10 && ( pUTF8[5] & 0xC0 ) == 0x80 )
	{
		uValue = 0x10000 + ( ( uValue - 0xD800u ) << 10 ) + ( (uint8)( pUTF8[4] - 0xB0 ) << 6 ) + pUTF8[5] - 0x80;
		nBytes = 6;
		uMinValue = 0x10000;
	}
	goto decodeFinished;
}

// Decode one character from a UTF-16 encoded string.
int Q_UTF16ToUChar32( const uchar16 *pUTF16, uchar32 &uValueOut, bool &bErrorOut )
{
	if ( Q_IsValidUChar32( pUTF16[0] ) )
	{
		uValueOut = pUTF16[0];
		bErrorOut = false;
		return 1;
	}
	else if ( (pUTF16[0] - 0xD800u) < 0x400u && (pUTF16[1] - 0xDC00u) < 0x400u )
	{
		// Valid surrogate pair, but maybe not encoding a valid Unicode code point...
		uchar32 uVal = 0x010000 + ((pUTF16[0] - 0xD800u) << 10) + (pUTF16[1] - 0xDC00);
		if ( Q_IsValidUChar32( uVal ) )
		{
			uValueOut = uVal;
			bErrorOut = false;
			return 2;
		}
		else
		{
			uValueOut = '?';
			bErrorOut = true;
			return 2;
		}
	}
	else
	{
		uValueOut = '?';
		bErrorOut = true;
		return 1;
	}
}

#define V_UTF32ToUChar32 Q_UTF32ToUChar32
#define V_UChar32ToUTF32Len Q_UChar32ToUTF32Len
#define V_UChar32ToUTF32 Q_UChar32ToUTF32

namespace // internal use only
{
	// Identity transformations and validity tests for use with Q_UnicodeConvertT
	int Q_UTF32ToUChar32( const uchar32 *pUTF32, uchar32 &uVal, bool &bErr )
	{
		bErr = !Q_IsValidUChar32( *pUTF32 );
		uVal = bErr ? '?' : *pUTF32;
		return 1;
	}

	int Q_UChar32ToUTF32Len( uchar32 uVal )
	{
		return 1;
	}

	int Q_UChar32ToUTF32( uchar32 uVal, uchar32 *pUTF32 )
	{
		*pUTF32 = uVal;
		return 1;
	}

	// A generic Unicode processing loop: decode one character from input to uchar32, handle errors, encode uchar32 to output
	template < typename SrcType, typename DstType, bool bStopAtNull, int (&DecodeSrc)( const SrcType*, uchar32&, bool& ), int (&EncodeDstLen)( uchar32 ), int (&EncodeDst)( uchar32, DstType* ) >
	int Q_UnicodeConvertT( const SrcType *pIn, int nInChars, DstType *pOut, int nOutBytes, EStringConvertErrorPolicy ePolicy )
	{
		int nOut = 0;

		if ( !pOut )
		{
			while ( bStopAtNull ? ( *pIn ) : ( nInChars-- > 0 ) )
			{
				uchar32 uVal;
				// Initialize in order to avoid /analyze warnings.
				bool bErr = false;
				pIn += DecodeSrc( pIn, uVal, bErr );
				nOut += EncodeDstLen( uVal );
				if ( bErr )
				{
#ifdef _DEBUG
					AssertMsg( !(ePolicy & _STRINGCONVERTFLAG_ASSERT), "invalid Unicode byte sequence" );
#endif
					if ( ePolicy & _STRINGCONVERTFLAG_SKIP )
					{
						nOut -= EncodeDstLen( uVal );
					}
					else if ( ePolicy & _STRINGCONVERTFLAG_FAIL )
					{
						return 0;
					}
				}
			}
		}
		else
		{
			int nOutElems = nOutBytes / sizeof( DstType );
			if ( nOutElems <= 0 )
				return 0;

			int nMaxOut = nOutElems - 1;
			while ( bStopAtNull ? ( *pIn ) : ( nInChars-- > 0 ) )
			{
				uchar32 uVal;
				// Initialize in order to avoid /analyze warnings.
				bool bErr = false;
				pIn += DecodeSrc( pIn, uVal, bErr );
				if ( nOut + EncodeDstLen( uVal ) > nMaxOut )
				{
					// Out of space!
					if ( ePolicy & _STRINGCONVERTFLAG_TOTALSIZE )
					{
						// Null terminate output, return total bytes that would have been written on success
						pOut[nOut] = 0;
						return (nOut + EncodeDstLen( uVal )) * sizeof( DstType ) + Q_UnicodeConvertT<SrcType, DstType, bStopAtNull, DecodeSrc, EncodeDstLen, EncodeDst>( pIn, nInChars, (DstType*)NULL, 0, ePolicy);
					}
					break;
				}
				nOut += EncodeDst( uVal, pOut + nOut );
				if ( bErr )
				{
#ifdef _DEBUG
					AssertMsg( !(ePolicy & _STRINGCONVERTFLAG_ASSERT), "invalid Unicode byte sequence" );
#endif
					if ( ePolicy & _STRINGCONVERTFLAG_SKIP )
					{
						nOut -= EncodeDstLen( uVal );
					}
					else if ( ePolicy & _STRINGCONVERTFLAG_FAIL )
					{
						pOut[0] = 0;
						return 0;
					}
				}
			}
			pOut[nOut] = 0;
		}

		return (nOut + 1) * sizeof( DstType );
	}

	// Source2 version of UnicodeConvertT
	// Required for UnicodeCaseConvert functions
	// TODO: TEST they are identical and then replace Q_UnicodeConvertT with this new version from src2

	// A generic Unicode conversion loop: decode one character from input to uchar32, handle errors, encode uchar32 to output
	template < bool bStopAtNull = true, typename SrcType, typename DstType, typename TDecodeSrc, typename TEncodeDstLen, typename TEncodeDst >
	int UnicodeConvertT_Src2( const SrcType *pIn, int nInChars, DstType *pOut, int nOutBytes, EStringConvertErrorPolicy ePolicy, TDecodeSrc& DecodeSrc, TEncodeDstLen& EncodeDstLen, TEncodeDst& EncodeDst )
	{
		int nOut = 0;
		if ( !pIn )
		{
			AssertMsg( false, "invalid input pointer" );
			if ( pOut )
				pOut[0] = 0;
			return 0;
		}
		else if ( !pOut )
		{
			while ( bStopAtNull ? (*pIn) : (nInChars-- > 0) )
			{
				uchar32 uVal;
				// Initialize in order to avoid /analyze warnings.
				bool bErr = false;
				pIn += DecodeSrc( pIn, uVal, bErr );
				nOut += EncodeDstLen( uVal );
				if ( bErr )
				{
#ifdef _DEBUG
					AssertMsg( !(ePolicy & _STRINGCONVERTFLAG_ASSERT), "invalid Unicode byte sequence" );
#endif
					if ( ePolicy & _STRINGCONVERTFLAG_SKIP )
					{
						nOut -= EncodeDstLen( uVal );
					}
					else if ( ePolicy & _STRINGCONVERTFLAG_FAIL )
					{
						return 0;
					}
				}
			}
		}
		else
		{
			int nOutElems = nOutBytes / sizeof( DstType );
			if ( nOutElems <= 0 )
				return 0;

			int nMaxOut = nOutElems - 1;
			while ( bStopAtNull ? (*pIn) : (nInChars-- > 0) )
			{
				uchar32 uVal;
				// Initialize in order to avoid /analyze warnings.
				bool bErr = false;
				pIn += DecodeSrc( pIn, uVal, bErr );
				if ( nOut + EncodeDstLen( uVal ) > nMaxOut )
				{
					// Out of space!
					if ( ePolicy & _STRINGCONVERTFLAG_TOTALSIZE )
					{
						// Null terminate output, return total bytes that would have been written on success
						pOut[nOut] = 0;
						return (nOut + EncodeDstLen( uVal )) * sizeof( DstType ) + UnicodeConvertT_Src2<bStopAtNull>( pIn, nInChars, (DstType*)NULL, 0, ePolicy, DecodeSrc, EncodeDstLen, EncodeDst );
					}
					break;
				}
				nOut += EncodeDst( uVal, pOut + nOut );
				if ( bErr )
				{
#ifdef _DEBUG
					AssertMsg( !(ePolicy & _STRINGCONVERTFLAG_ASSERT), "invalid Unicode byte sequence" );
#endif
					if ( ePolicy & _STRINGCONVERTFLAG_SKIP )
					{
						nOut -= EncodeDstLen( uVal );
					}
					else if ( ePolicy & _STRINGCONVERTFLAG_FAIL )
					{
						pOut[0] = 0;
						return 0;
					}
				}
			}
			pOut[nOut] = 0;
		}

		return (nOut + 1) * sizeof( DstType );
	}

	template < typename T, int( &func )(const T *pUTF, uchar32 &uVal, bool &bErr) >
	struct DecodeFuncAdapter { int operator()( const T *pUTF, uchar32 &uVal, bool &bErr ) { return func( pUTF, uVal, bErr ); } };

	template < int( &func )(uchar32 uVal) >
	struct EncodeLenFuncAdapter { int operator()( uchar32 uVal ) { return func( uVal ); } };

	template < typename T, int( &func )(uchar32 uVal, T *pUTF) >
	struct EncodeFuncAdapter { int operator()( uchar32 uVal, T *pUTF ) { return func( uVal, pUTF ); } };

	template < typename SrcType, typename DstType, bool bStopAtNull, int( &DecodeSrc )(const SrcType*, uchar32&, bool&), int( &EncodeDstLen )(uchar32), int( &EncodeDst )(uchar32, DstType*) >
	int UnicodeConvertT_Src2( const SrcType *pIn, int nInChars, DstType *pOut, int nOutBytes, EStringConvertErrorPolicy ePolicy )
	{
		DecodeFuncAdapter< SrcType, DecodeSrc > a;
		EncodeLenFuncAdapter< EncodeDstLen > b;
		EncodeFuncAdapter< DstType, EncodeDst > c;
		return UnicodeConvertT_Src2< bStopAtNull, SrcType, DstType >( pIn, nInChars, pOut, nOutBytes, ePolicy, a, b, c );
	}
	//-----------------------------------------------------------------------------
	// Purpose: common helper struct for V_towlower32 and V_towupper32, etc
	//
	// This is used in character-range tables, where each entry is a starting point
	// and a range length, plus an 8-bit index into an external "value table". The
	// value might be a set of UTF-32 characters, or a numeric delta to apply, or
	// anything like that.
	//
	// NOTE: as a special case, index 0 has additional meaning and is reserved for
	// use with "alternating ranges" which are very common in the case tables. It
	// is assumed that values which are start, start+2, start+4, until start+len
	// are matches for the table but start+1, start+3, start+5, etc are not. The
	// uppercase and lowercase delta tables make heavy use of this.
	//-----------------------------------------------------------------------------
	struct InternalUnicodeRangeTableEntry_t
	{
		uint16 start;
		uint8 len;
		uint8 index;
	};

	static int InternalUnicodeTableLookupN( const InternalUnicodeRangeTableEntry_t *pTable, int N, uchar32 uc )
	{
		int lo = 0, hi = N - 1;

		// Are we outside of the table bounds? If so, no delta.
		if ( (uint32)(uc - pTable[lo].start) > (uint32)((pTable[hi].start + pTable[hi].len - 1u) - pTable[lo].start) )
			return -1;

		// Binary search the table section for a matching range and apply delta if found
		while ( lo <= hi )
		{
			int mid = (lo + hi) / 2;
			if ( (uint32)(uc - pTable[mid].start) < (uint32)pTable[mid].len )
			{
				// Special case: table is encoded where any range w/ index 0 is an
				// alternating range, where the first value is a match, the second
				// is not, third is a match, fourth is not, etc.
				if ( pTable[mid].index == 0 && ((uc - pTable[mid].start) & 1) )
					return -1;

				return pTable[mid].index;
			}

			if ( uc < (uint32)pTable[mid].start )
				hi = mid - 1;
			else
				lo = mid + 1;
		}
		return -1;
	}

	template <int N>
	static inline int InternalUnicodeTableLookup( const InternalUnicodeRangeTableEntry_t( &table )[N], uchar32 uc )
	{
		return InternalUnicodeTableLookupN( table, N, uc );
	}

	template <int N>
	static inline uchar32 InternalUnicodeApplyDeltaFromTable( const InternalUnicodeRangeTableEntry_t( &table )[N], const int32 *pDeltas, uchar32 uc )
	{
		int nIndex = InternalUnicodeTableLookupN( table, N, uc );
		return uc + (nIndex >= 0 ? pDeltas[nIndex] : 0);
	}

}

// From source2...

//-----------------------------------------------------------------------------
// Purpose: implement stable Unicode 8.0 case conversion using a small (< 1kb)
// data table. This function only performs reversible 1-to-1 mappings.
//-----------------------------------------------------------------------------
uchar32 V_towupper32( uchar32 uc )
{
	// Lower bound of table / fast path: special case all characters < 0xFFu
	if ( uc < 0xFFu )
	{
		if ( uc - 0x61u < 26u || uc - 0xE0u < 23u || uc - 0xF8u < 7u )
			return uc - 32;
		return uc;
	}

	// Manually built from UnicodeData.txt for Unicode 8.0  -henryg Jan 2016
	static const int32 s_SimpleToUpper_deltas[] = {
		-1, -32, 121, 195, 97, 163, 130, 56, -2, -79, 10815, 10783, 10780,
		10782, -210, -206, -205, -202, -203, 42319, 42315, -207, 42280,
		42308, -209, -211, 10743, 42305, 10749, -213, -214, 10727, -218,
		42282, -69, -217, -71, -219, 42261, 42258, -38, -37, -64, -63, -8,
		7, -116, -80, -15, -48, -40, 35332, 3814, 8, 74, 86, 100, 128, 112,
		126, 9, -28, -16, -26, -10795, -10792, -7264, -928, -38864
	};
	static const InternalUnicodeRangeTableEntry_t s_SimpleToUpper_1[] = {
		//	values < 0xFF handled by fast path above
		{ 0x00FF, 1, 2 }, { 0x0101, 47, 0 }, { 0x0133, 5, 0 },
		{ 0x013A, 15, 0 }, { 0x014B, 45, 0 }, { 0x017A, 5, 0 },
		{ 0x0180, 1, 3 }, { 0x0183, 3, 0 }, { 0x0188, 1, 0 },
		{ 0x018C, 1, 0 }, { 0x0192, 1, 0 }, { 0x0195, 1, 4 },
		{ 0x0199, 1, 0 }, { 0x019A, 1, 5 }, { 0x019E, 1, 6 },
		{ 0x01A1, 5, 0 }, { 0x01A8, 1, 0 }, { 0x01AD, 1, 0 },
		{ 0x01B0, 1, 0 }, { 0x01B4, 3, 0 }, { 0x01B9, 1, 0 },
		{ 0x01BD, 1, 0 }, { 0x01BF, 1, 7 }, { 0x01C6, 1, 8 },
		{ 0x01C9, 1, 8 }, { 0x01CC, 1, 8 }, { 0x01CE, 15, 0 },
		{ 0x01DD, 1, 9 }, { 0x01DF, 17, 0 }, { 0x01F3, 1, 8 },
		{ 0x01F5, 1, 0 }, { 0x01F9, 39, 0 }, { 0x0223, 17, 0 },
		{ 0x023C, 1, 0 }, { 0x023F, 2, 10 }, { 0x0242, 1, 0 },
		{ 0x0247, 9, 0 }, { 0x0250, 1, 11 }, { 0x0251, 1, 12 },
		{ 0x0252, 1, 13 }, { 0x0253, 1, 14 }, { 0x0254, 1, 15 },
		{ 0x0256, 2, 16 }, { 0x0259, 1, 17 }, { 0x025B, 1, 18 },
		{ 0x025C, 1, 19 }, { 0x0260, 1, 16 }, { 0x0261, 1, 20 },
		{ 0x0263, 1, 21 }, { 0x0265, 1, 22 }, { 0x0266, 1, 23 },
		{ 0x0268, 1, 24 }, { 0x0269, 1, 25 }, { 0x026B, 1, 26 },
		{ 0x026C, 1, 27 }, { 0x026F, 1, 25 }, { 0x0271, 1, 28 },
		{ 0x0272, 1, 29 }, { 0x0275, 1, 30 }, { 0x027D, 1, 31 },
		{ 0x0280, 1, 32 }, { 0x0283, 1, 32 }, { 0x0287, 1, 33 },
		{ 0x0288, 1, 32 }, { 0x0289, 1, 34 }, { 0x028A, 2, 35 },
		{ 0x028C, 1, 36 }, { 0x0292, 1, 37 }, { 0x029D, 1, 38 },
		{ 0x029E, 1, 39 }
	}, s_SimpleToUpper_2[] = {
		{ 0x0371, 3, 0 }, { 0x0377, 1, 0 }, { 0x037B, 3, 6 },
		{ 0x03AC, 1, 40 }, { 0x03AD, 3, 41 }, { 0x03B1, 17, 1 },
		{ 0x03C3, 9, 1 }, { 0x03CC, 1, 42 }, { 0x03CD, 2, 43 },
		{ 0x03D7, 1, 44 }, { 0x03D9, 23, 0 }, { 0x03F2, 1, 45 },
		{ 0x03F3, 1, 46 }, { 0x03F8, 1, 0 }, { 0x03FB, 1, 0 },
		{ 0x0430, 32, 1 }, { 0x0450, 16, 47 }, { 0x0461, 33, 0 },
		{ 0x048B, 53, 0 }, { 0x04C2, 13, 0 }, { 0x04CF, 1, 48 },
		{ 0x04D1, 95, 0 }, { 0x0561, 38, 49 }
		}, s_SimpleToUpper_3[] = {
			{ 0x13F8, 6, 44 }, { 0x1D79, 1, 51 }, { 0x1D7D, 1, 52 },
			{ 0x1E01, 149, 0 }, { 0x1EA1, 95, 0 }, { 0x1F00, 8, 53 },
			{ 0x1F10, 6, 53 }, { 0x1F20, 8, 53 }, { 0x1F30, 8, 53 },
			{ 0x1F40, 6, 53 }, { 0x1F51, 1, 53 }, { 0x1F53, 1, 53 },
			{ 0x1F55, 1, 53 }, { 0x1F57, 1, 53 }, { 0x1F60, 8, 53 },
			{ 0x1F70, 2, 54 }, { 0x1F72, 4, 55 }, { 0x1F76, 2, 56 },
			{ 0x1F78, 2, 57 }, { 0x1F7A, 2, 58 }, { 0x1F7C, 2, 59 },
			{ 0x1F80, 8, 53 }, { 0x1F90, 8, 53 }, { 0x1FA0, 8, 53 },
			{ 0x1FB0, 2, 53 }, { 0x1FB3, 1, 60 }, { 0x1FC3, 1, 60 },
			{ 0x1FD0, 2, 53 }, { 0x1FE0, 2, 53 }, { 0x1FE5, 1, 45 },
			{ 0x1FF3, 1, 60 }, { 0x214E, 1, 61 }, { 0x2170, 16, 62 },
			{ 0x2184, 1, 0 }, { 0x24D0, 26, 63 }, { 0x2C30, 47, 49 },
			{ 0x2C61, 1, 0 }, { 0x2C65, 1, 64 }, { 0x2C66, 1, 65 },
			{ 0x2C68, 5, 0 }, { 0x2C73, 1, 0 }, { 0x2C76, 1, 0 },
			{ 0x2C81, 99, 0 }, { 0x2CEC, 3, 0 }, { 0x2CF3, 1, 0 },
			{ 0x2D00, 38, 66 }, { 0x2D27, 1, 66 }, { 0x2D2D, 1, 66 }
		}, s_SimpleToUpper_4[] = {
			{ 0xA641, 45, 0 }, { 0xA681, 27, 0 }, { 0xA723, 13, 0 },
			{ 0xA733, 61, 0 }, { 0xA77A, 3, 0 }, { 0xA77F, 9, 0 },
			{ 0xA78C, 1, 0 }, { 0xA791, 3, 0 }, { 0xA797, 19, 0 },
			{ 0xA7B5, 3, 0 }, { 0xAB53, 1, 67 }, { 0xAB70, 80, 68 }
			// upper-bound cases handled below
			};

			// Pick one of the four table sections based on our hand-selected partition points
			if ( uc < s_SimpleToUpper_3[0].start )
			{
				if ( uc < s_SimpleToUpper_2[0].start )
					return InternalUnicodeApplyDeltaFromTable( s_SimpleToUpper_1, s_SimpleToUpper_deltas, uc );
				else
					return InternalUnicodeApplyDeltaFromTable( s_SimpleToUpper_2, s_SimpleToUpper_deltas, uc );
			}
			else if ( uc < 0xFF41u )
			{
				if ( uc < s_SimpleToUpper_4[0].start )
					return InternalUnicodeApplyDeltaFromTable( s_SimpleToUpper_3, s_SimpleToUpper_deltas, uc );
				else
					return InternalUnicodeApplyDeltaFromTable( s_SimpleToUpper_4, s_SimpleToUpper_deltas, uc );
			}
			else
			{
				// special section off the bottom end of the table
				if ( uc - 0xFF41u < 26u )	return uc + s_SimpleToUpper_deltas[1];
				if ( uc - 0x10428u < 40u )	return uc + s_SimpleToUpper_deltas[50];
				if ( uc - 0x10CC0u < 51u )	return uc + s_SimpleToUpper_deltas[42];
				if ( uc - 0x118C0u < 32u )	return uc + s_SimpleToUpper_deltas[1];
				return uc;
			}
};


// ---------------------------------------------------------------------------- -
// Purpose: implement stable Unicode 8.0 case conversion using a small (< 1kb)
// data table. This function only performs reversible 1-to-1 mappings.
//-----------------------------------------------------------------------------
uchar32 V_towlower32( uchar32 uc )
{
	// Lower bound of table / fast path: special case all characters <= 0x100u
	if ( uc < 0x100u )
	{
		if ( uc - 0x41u < 26u || uc - 0xC0u < 23u || uc - 0xD8u < 7u )
			return uc + 32;
		return uc;
	}

	// Manually built from UnicodeData.txt for Unicode 8.0  -henryg Jan 2016
	static int32 const s_SimpleToLower_deltas[] = {
		1, 32, -121, 210, 206, 205, 79, 202, 203, 207, 211, 209, 213, 214,
		218, 217, 219, 2, -97, -56, -130, 10795, -163, 10792, -195, 69, 71,
		116, 38, 37, 64, 63, 8, -7, 80, 15, 48, 40, 7264, 38864, -8, -74, -9,
		-86, -100, -112, -128, -126, 28, 16, 26, -10743, -3814, -10727,
		-10780, -10749, -10783, -10782, -10815, -35332, -42280, -42308,
		-42319, -42315, -42305, -42258, -42282, -42261, 928
	};
	static const InternalUnicodeRangeTableEntry_t s_SimpleToLower_1[] = {
		// values < 0x100 handled by fast path above
		{ 0x0100, 47, 0 }, { 0x0132, 5, 0 }, { 0x0139, 15, 0 },
		{ 0x014A, 45, 0 }, { 0x0178, 1, 2 }, { 0x0179, 5, 0 },
		{ 0x0181, 1, 3 }, { 0x0182, 3, 0 }, { 0x0186, 1, 4 },
		{ 0x0187, 1, 0 }, { 0x0189, 2, 5 }, { 0x018B, 1, 0 },
		{ 0x018E, 1, 6 }, { 0x018F, 1, 7 }, { 0x0190, 1, 8 },
		{ 0x0191, 1, 0 }, { 0x0193, 1, 5 }, { 0x0194, 1, 9 },
		{ 0x0196, 1, 10 }, { 0x0197, 1, 11 }, { 0x0198, 1, 0 },
		{ 0x019C, 1, 10 }, { 0x019D, 1, 12 }, { 0x019F, 1, 13 },
		{ 0x01A0, 5, 0 }, { 0x01A6, 1, 14 }, { 0x01A7, 1, 0 },
		{ 0x01A9, 1, 14 }, { 0x01AC, 1, 0 }, { 0x01AE, 1, 14 },
		{ 0x01AF, 1, 0 }, { 0x01B1, 2, 15 }, { 0x01B3, 3, 0 },
		{ 0x01B7, 1, 16 }, { 0x01B8, 1, 0 }, { 0x01BC, 1, 0 },
		{ 0x01C4, 1, 17 }, { 0x01C7, 1, 17 }, { 0x01CA, 1, 17 },
		{ 0x01CD, 15, 0 }, { 0x01DE, 17, 0 }, { 0x01F1, 1, 17 },
		{ 0x01F4, 1, 0 }, { 0x01F6, 1, 18 }, { 0x01F7, 1, 19 },
		{ 0x01F8, 39, 0 }, { 0x0220, 1, 20 }, { 0x0222, 17, 0 },
		{ 0x023A, 1, 21 }, { 0x023B, 1, 0 }, { 0x023D, 1, 22 },
		{ 0x023E, 1, 23 }, { 0x0241, 1, 0 }, { 0x0243, 1, 24 },
		{ 0x0244, 1, 25 }, { 0x0245, 1, 26 }, { 0x0246, 9, 0 }
	}, s_SimpleToLower_2[] = {
		{ 0x0370, 3, 0 }, { 0x0376, 1, 0 }, { 0x037F, 1, 27 },
		{ 0x0386, 1, 28 }, { 0x0388, 3, 29 }, { 0x038C, 1, 30 },
		{ 0x038E, 2, 31 }, { 0x0391, 17, 1 }, { 0x03A3, 9, 1 },
		{ 0x03CF, 1, 32 }, { 0x03D8, 23, 0 }, { 0x03F7, 1, 0 },
		{ 0x03F9, 1, 33 }, { 0x03FA, 1, 0 }, { 0x03FD, 3, 20 },
		{ 0x0400, 16, 34 }, { 0x0410, 32, 1 }, { 0x0460, 33, 0 },
		{ 0x048A, 53, 0 }, { 0x04C0, 1, 35 }, { 0x04C1, 13, 0 },
		{ 0x04D0, 95, 0 }, { 0x0531, 38, 36 }
		}, s_SimpleToLower_3[] = {
			{ 0x10A0, 38, 38 }, { 0x10C7, 1, 38 }, { 0x10CD, 1, 38 },
			{ 0x13A0, 80, 39 }, { 0x13F0, 6, 32 }, { 0x1E00, 149, 0 },
			{ 0x1EA0, 95, 0 }, { 0x1F08, 8, 40 }, { 0x1F18, 6, 40 },
			{ 0x1F28, 8, 40 }, { 0x1F38, 8, 40 }, { 0x1F48, 6, 40 },
			{ 0x1F59, 1, 40 }, { 0x1F5B, 1, 40 }, { 0x1F5D, 1, 40 },
			{ 0x1F5F, 1, 40 }, { 0x1F68, 8, 40 }, { 0x1F88, 8, 40 },
			{ 0x1F98, 8, 40 }, { 0x1FA8, 8, 40 }, { 0x1FB8, 2, 40 },
			{ 0x1FBA, 2, 41 }, { 0x1FBC, 1, 42 }, { 0x1FC8, 4, 43 },
			{ 0x1FCC, 1, 42 }, { 0x1FD8, 2, 40 }, { 0x1FDA, 2, 44 },
			{ 0x1FE8, 2, 40 }, { 0x1FEA, 2, 45 }, { 0x1FEC, 1, 33 },
			{ 0x1FF8, 2, 46 }, { 0x1FFA, 2, 47 }, { 0x1FFC, 1, 42 },
			{ 0x2132, 1, 48 }, { 0x2160, 16, 49 }, { 0x2183, 1, 0 },
			{ 0x24B6, 26, 50 }, { 0x2C00, 47, 36 }, { 0x2C60, 1, 0 },
			{ 0x2C62, 1, 51 }, { 0x2C63, 1, 52 }, { 0x2C64, 1, 53 },
			{ 0x2C67, 5, 0 }, { 0x2C6D, 1, 54 }, { 0x2C6E, 1, 55 },
			{ 0x2C6F, 1, 56 }, { 0x2C70, 1, 57 }, { 0x2C72, 1, 0 },
			{ 0x2C75, 1, 0 }, { 0x2C7E, 2, 58 }, { 0x2C80, 99, 0 },
			{ 0x2CEB, 3, 0 }, { 0x2CF2, 1, 0 }
		}, s_SimpleToLower_4[] = {
			{ 0xA640, 45, 0 }, { 0xA680, 27, 0 }, { 0xA722, 13, 0 },
			{ 0xA732, 61, 0 }, { 0xA779, 3, 0 }, { 0xA77D, 1, 59 },
			{ 0xA77E, 9, 0 }, { 0xA78B, 1, 0 }, { 0xA78D, 1, 60 },
			{ 0xA790, 3, 0 }, { 0xA796, 19, 0 }, { 0xA7AA, 1, 61 },
			{ 0xA7AB, 1, 62 }, { 0xA7AC, 1, 63 }, { 0xA7AD, 1, 64 },
			{ 0xA7B0, 1, 65 }, { 0xA7B1, 1, 66 }, { 0xA7B2, 1, 67 },
			{ 0xA7B3, 1, 68 }, { 0xA7B4, 3, 0 }
			// values >0xFF00 (including non-BMP values > 0xFFFF) handled specially
			};

			// Pick the appropriate table section
			if ( uc < s_SimpleToLower_3[0].start )
			{
				if ( uc < s_SimpleToLower_2[0].start )
					return InternalUnicodeApplyDeltaFromTable( s_SimpleToLower_1, s_SimpleToLower_deltas, uc );
				else
					return InternalUnicodeApplyDeltaFromTable( s_SimpleToLower_2, s_SimpleToLower_deltas, uc );
			}
			else if ( uc < 0xFF21u )
			{
				if ( uc < s_SimpleToLower_4[0].start )
					return InternalUnicodeApplyDeltaFromTable( s_SimpleToLower_3, s_SimpleToLower_deltas, uc );
				else
					return InternalUnicodeApplyDeltaFromTable( s_SimpleToLower_4, s_SimpleToLower_deltas, uc );
			}
			else
			{
				// special section off the bottom end of the table
				if ( uc - 0xFF21u < 26u )	return uc + s_SimpleToLower_deltas[1];
				if ( uc - 0x10400u < 40u )	return uc + s_SimpleToLower_deltas[37];
				if ( uc - 0x10C80u < 51u )	return uc + s_SimpleToLower_deltas[30];
				if ( uc - 0x118A0u < 32u )	return uc + s_SimpleToLower_deltas[1];
				return uc;
			}
};

//-----------------------------------------------------------------------------
// Purpose: helpers for Unicode 8.0 case conversion
//-----------------------------------------------------------------------------
namespace
{
	static int UChar32ToUpper( uchar32 uc, uchar32( &rgOut )[3] )
	{
		uchar32 simple = V_towupper32( uc );
		if ( simple != uc || uc < 0xB5u )
		{
			rgOut[0] = simple;
			rgOut[1] = 0;
			rgOut[2] = 0;
			return 1;
		}

		// Fixup for special linguistic cases which are not reversible or 1-to-1
		// From an analysis of UnicodeData.txt for Unicode 8.0  -henryg Jan 2016
		static uchar16 s_SpecialUpperCaseValues[][3] = {
			{ 0x0000 }, { 0x039C }, { 0x0053, 0x0053 }, { 0x0049 },
			{ 0x02BC, 0x004E }, { 0x0053 }, { 0x01C4 }, { 0x01C7 },
			{ 0x01CA }, { 0x004A, 0x030C }, { 0x01F1 }, { 0x0399 },
			{ 0x0399, 0x0308, 0x0301 }, { 0x03A5, 0x0308, 0x0301 },
			{ 0x03A3 }, { 0x0392 }, { 0x0398 }, { 0x03A6 }, { 0x03A0 },
			{ 0x039A }, { 0x03A1 }, { 0x0395 }, { 0x0535, 0x0552 },
			{ 0x0048, 0x0331 }, { 0x0054, 0x0308 }, { 0x0057, 0x030A },
			{ 0x0059, 0x030A }, { 0x0041, 0x02BE }, { 0x1E60 },
			{ 0x03A5, 0x0313 }, { 0x03A5, 0x0313, 0x0300 },
			{ 0x03A5, 0x0313, 0x0301 }, { 0x03A5, 0x0313, 0x0342 },
			{ 0x1F08, 0x0399 }, { 0x1F09, 0x0399 }, { 0x1F0A, 0x0399 },
			{ 0x1F0B, 0x0399 }, { 0x1F0C, 0x0399 }, { 0x1F0D, 0x0399 },
			{ 0x1F0E, 0x0399 }, { 0x1F0F, 0x0399 }, { 0x1F28, 0x0399 },
			{ 0x1F29, 0x0399 }, { 0x1F2A, 0x0399 }, { 0x1F2B, 0x0399 },
			{ 0x1F2C, 0x0399 }, { 0x1F2D, 0x0399 }, { 0x1F2E, 0x0399 },
			{ 0x1F2F, 0x0399 }, { 0x1F68, 0x0399 }, { 0x1F69, 0x0399 },
			{ 0x1F6A, 0x0399 }, { 0x1F6B, 0x0399 }, { 0x1F6C, 0x0399 },
			{ 0x1F6D, 0x0399 }, { 0x1F6E, 0x0399 }, { 0x1F6F, 0x0399 },
			{ 0x1FBA, 0x0399 }, { 0x0391, 0x0399 }, { 0x0386, 0x0399 },
			{ 0x0391, 0x0342 }, { 0x0391, 0x0342, 0x0399 },
			{ 0x1FCA, 0x0399 }, { 0x0397, 0x0399 }, { 0x0389, 0x0399 },
			{ 0x0397, 0x0342 }, { 0x0397, 0x0342, 0x0399 },
			{ 0x0399, 0x0308, 0x0300 }, { 0x0399, 0x0342 },
			{ 0x0399, 0x0308, 0x0342 }, { 0x03A5, 0x0308, 0x0300 },
			{ 0x03A1, 0x0313 }, { 0x03A5, 0x0342 },
			{ 0x03A5, 0x0308, 0x0342 }, { 0x1FFA, 0x0399 },
			{ 0x03A9, 0x0399 }, { 0x038F, 0x0399 }, { 0x03A9, 0x0342 },
			{ 0x03A9, 0x0342, 0x0399 }, { 0x0046, 0x0046 },
			{ 0x0046, 0x0049 }, { 0x0046, 0x004C },
			{ 0x0046, 0x0046, 0x0049 }, { 0x0046, 0x0046, 0x004C },
			{ 0x0053, 0x0054 }, { 0x0544, 0x0546 }, { 0x0544, 0x0535 },
			{ 0x0544, 0x053B }, { 0x054E, 0x0546 }, { 0x0544, 0x053D }
		};
		static const InternalUnicodeRangeTableEntry_t k_SpecialUpperCaseLookup1[] = {
			{ 0x00b5, 1, 1 }, { 0x00df, 1, 2 }, { 0x0131, 1, 3 },
			{ 0x0149, 1, 4 }, { 0x017f, 1, 5 }, { 0x01c5, 1, 6 },
			{ 0x01c8, 1, 7 }, { 0x01cb, 1, 8 }, { 0x01f0, 1, 9 },
			{ 0x01f2, 1, 10 }
		}, k_SpecialUpperCaseLookup2[] = {
			{ 0x0345, 1, 11 }, { 0x0390, 1, 12 }, { 0x03b0, 1, 13 },
			{ 0x03c2, 1, 14 }, { 0x03d0, 1, 15 }, { 0x03d1, 1, 16 },
			{ 0x03d5, 1, 17 }, { 0x03d6, 1, 18 }, { 0x03f0, 1, 19 },
			{ 0x03f1, 1, 20 }, { 0x03f5, 1, 21 }, { 0x0587, 1, 22 }
			}, k_SpecialUpperCaseLookup3[] = {
				{ 0x1e96, 1, 23 }, { 0x1e97, 1, 24 }, { 0x1e98, 1, 25 },
				{ 0x1e99, 1, 26 }, { 0x1e9a, 1, 27 }, { 0x1e9b, 1, 28 },
				{ 0x1f50, 1, 29 }, { 0x1f52, 1, 30 }, { 0x1f54, 1, 31 },
				{ 0x1f56, 1, 32 }, { 0x1f80, 1, 33 }, { 0x1f81, 1, 34 },
				{ 0x1f82, 1, 35 }, { 0x1f83, 1, 36 }, { 0x1f84, 1, 37 },
				{ 0x1f85, 1, 38 }, { 0x1f86, 1, 39 }, { 0x1f87, 1, 40 },
				{ 0x1f88, 1, 33 }, { 0x1f89, 1, 34 }, { 0x1f8a, 1, 35 },
				{ 0x1f8b, 1, 36 }, { 0x1f8c, 1, 37 }, { 0x1f8d, 1, 38 },
				{ 0x1f8e, 1, 39 }, { 0x1f8f, 1, 40 }, { 0x1f90, 1, 41 },
				{ 0x1f91, 1, 42 }, { 0x1f92, 1, 43 }, { 0x1f93, 1, 44 },
				{ 0x1f94, 1, 45 }, { 0x1f95, 1, 46 }, { 0x1f96, 1, 47 },
				{ 0x1f97, 1, 48 }, { 0x1f98, 1, 41 }, { 0x1f99, 1, 42 },
				{ 0x1f9a, 1, 43 }, { 0x1f9b, 1, 44 }, { 0x1f9c, 1, 45 },
				{ 0x1f9d, 1, 46 }, { 0x1f9e, 1, 47 }, { 0x1f9f, 1, 48 },
				{ 0x1fa0, 1, 49 }, { 0x1fa1, 1, 50 }, { 0x1fa2, 1, 51 },
				{ 0x1fa3, 1, 52 }, { 0x1fa4, 1, 53 }, { 0x1fa5, 1, 54 },
				{ 0x1fa6, 1, 55 }, { 0x1fa7, 1, 56 }, { 0x1fa8, 1, 49 },
				{ 0x1fa9, 1, 50 }, { 0x1faa, 1, 51 }, { 0x1fab, 1, 52 },
				{ 0x1fac, 1, 53 }, { 0x1fad, 1, 54 }, { 0x1fae, 1, 55 },
				{ 0x1faf, 1, 56 }, { 0x1fb2, 1, 57 }, { 0x1fb3, 1, 58 },
				{ 0x1fb4, 1, 59 }, { 0x1fb6, 1, 60 }, { 0x1fb7, 1, 61 },
				{ 0x1fbc, 1, 58 }, { 0x1fbe, 1, 11 }, { 0x1fc2, 1, 62 },
				{ 0x1fc3, 1, 63 }, { 0x1fc4, 1, 64 }, { 0x1fc6, 1, 65 },
				{ 0x1fc7, 1, 66 }, { 0x1fcc, 1, 63 }, { 0x1fd2, 1, 67 },
				{ 0x1fd3, 1, 12 }, { 0x1fd6, 1, 68 }, { 0x1fd7, 1, 69 },
				{ 0x1fe2, 1, 70 }, { 0x1fe3, 1, 13 }, { 0x1fe4, 1, 71 },
				{ 0x1fe6, 1, 72 }, { 0x1fe7, 1, 73 }, { 0x1ff2, 1, 74 },
				{ 0x1ff3, 1, 75 }, { 0x1ff4, 1, 76 }, { 0x1ff6, 1, 77 },
				{ 0x1ff7, 1, 78 }, { 0x1ffc, 1, 75 }
			}, k_SpecialUpperCaseLookup4[] = {
				{ 0xfb00, 1, 79 }, { 0xfb01, 1, 80 }, { 0xfb02, 1, 81 },
				{ 0xfb03, 1, 82 }, { 0xfb04, 1, 83 }, { 0xfb05, 2, 84 },
				{ 0xfb13, 1, 85 }, { 0xfb14, 1, 86 }, { 0xfb15, 1, 87 },
				{ 0xfb16, 1, 88 }, { 0xfb17, 1, 89 }
				};

				int nIndex = 0;
				if ( uc < k_SpecialUpperCaseLookup3[0].start )
				{
					if ( uc < k_SpecialUpperCaseLookup2[0].start )
					{
						nIndex = InternalUnicodeTableLookup( k_SpecialUpperCaseLookup1, uc );
					}
					else
					{
						nIndex = InternalUnicodeTableLookup( k_SpecialUpperCaseLookup2, uc );
					}
				}
				else
				{
					if ( uc < k_SpecialUpperCaseLookup4[0].start )
					{
						nIndex = InternalUnicodeTableLookup( k_SpecialUpperCaseLookup3, uc );
					}
					else
					{
						nIndex = InternalUnicodeTableLookup( k_SpecialUpperCaseLookup4, uc );
					}
				}

				// Do we have an uppercase entry for this uchar32 value?
				if ( nIndex >= 0 )
				{
					rgOut[0] = s_SpecialUpperCaseValues[nIndex][0];
					rgOut[1] = s_SpecialUpperCaseValues[nIndex][1];
					rgOut[2] = 0;
					return (rgOut[1] == 0) ? 1 : 2;
				}
				else
				{
					rgOut[0] = uc;
					rgOut[1] = 0;
					rgOut[2] = 0;
					return 1;
				}
	}

	int UChar32ToLower( uchar32 uc, uchar32( &rgOut )[3] )
	{
		uchar32 simple = V_towlower32( uc );
		if ( simple != uc || uc < 0x130u )
		{
			rgOut[0] = simple;
			rgOut[1] = 0;
			rgOut[2] = 0;
			return 1;
		}

		// Fixup for special linguistic cases which are not reversible or 1-to-1
		// From an analysis of UnicodeData.txt for Unicode 8.0  -henryg Jan 2016
		static const uint16 k_SpecialLowerCaseValues[38][2] = {
			{ 0x0000 }, { 0x0069, 0x0307 }, { 0x01C6 }, { 0x01C9 },
			{ 0x01CC }, { 0x01F3 }, { 0x03B8 }, { 0x00DF }, { 0x1F80 },
			{ 0x1F81 }, { 0x1F82 }, { 0x1F83 }, { 0x1F84 }, { 0x1F85 },
			{ 0x1F86 }, { 0x1F87 }, { 0x1F90 }, { 0x1F91 }, { 0x1F92 },
			{ 0x1F93 }, { 0x1F94 }, { 0x1F95 }, { 0x1F96 }, { 0x1F97 },
			{ 0x1FA0 }, { 0x1FA1 }, { 0x1FA2 }, { 0x1FA3 }, { 0x1FA4 },
			{ 0x1FA5 }, { 0x1FA6 }, { 0x1FA7 }, { 0x1FB3 }, { 0x1FC3 },
			{ 0x1FF3 }, { 0x03C9 }, { 0x006B }, { 0x00E5 }
		};
		static const InternalUnicodeRangeTableEntry_t k_SpecialLowerCaseLookup1[] = {
			{ 0x0130, 1, 1 }, { 0x01c5, 1, 2 }, { 0x01c8, 1, 3 },
			{ 0x01cb, 1, 4 }, { 0x01f2, 1, 5 }, { 0x03f4, 1, 6 }
		}, k_SpecialLowerCaseLookup2[] = {
			{ 0x1e9e, 1, 7 }, { 0x1f88, 1, 8 }, { 0x1f89, 1, 9 },
			{ 0x1f8a, 1, 10 }, { 0x1f8b, 1, 11 }, { 0x1f8c, 1, 12 },
			{ 0x1f8d, 1, 13 }, { 0x1f8e, 1, 14 }, { 0x1f8f, 1, 15 },
			{ 0x1f98, 1, 16 }, { 0x1f99, 1, 17 }, { 0x1f9a, 1, 18 },
			{ 0x1f9b, 1, 19 }, { 0x1f9c, 1, 20 }, { 0x1f9d, 1, 21 },
			{ 0x1f9e, 1, 22 }, { 0x1f9f, 1, 23 }, { 0x1fa8, 1, 24 },
			{ 0x1fa9, 1, 25 }, { 0x1faa, 1, 26 }, { 0x1fab, 1, 27 },
			{ 0x1fac, 1, 28 }, { 0x1fad, 1, 29 }, { 0x1fae, 1, 30 },
			{ 0x1faf, 1, 31 }, { 0x1fbc, 1, 32 }, { 0x1fcc, 1, 33 },
			{ 0x1ffc, 1, 34 }, { 0x2126, 1, 35 }, { 0x212a, 1, 36 },
			{ 0x212b, 1, 37 }
			};

			int nIndex = -1;
			if ( uc < k_SpecialLowerCaseLookup2[0].start )
			{
				nIndex = InternalUnicodeTableLookup( k_SpecialLowerCaseLookup1, uc );
			}
			else
			{
				nIndex = InternalUnicodeTableLookup( k_SpecialLowerCaseLookup2, uc );
			}

			// Do we have a lowercase entry for this uchar32 value?
			if ( nIndex >= 0 )
			{
				rgOut[0] = k_SpecialLowerCaseValues[nIndex][0];
				rgOut[1] = k_SpecialLowerCaseValues[nIndex][1];
				rgOut[2] = 0;
				return (rgOut[1] == 0) ? 1 : 2;
			}
			else
			{
				rgOut[0] = uc;
				rgOut[1] = 0;
				rgOut[2] = 0;
				return 1;
			}
	}

	template < typename SrcType, int( &DecodeFunc )(const SrcType *pUTF, uchar32 &uValueOut, bool &bErrorOut) >
	struct LinguisticToUpperFunctor
	{
		uchar32 decoded[3];
		uint8 decodepos;
		uint8 decodemax;
		int8 delayedadvance;

		LinguisticToUpperFunctor() { decoded[0] = 0; decodepos = 0; decodemax = 0; delayedadvance = 0; }

		int operator()( const SrcType *pUTF, uchar32 &uValueOut, bool &bErrorOut )
		{
			if ( decodepos < decodemax && decoded[decodepos] != 0 )
			{
				uValueOut = decoded[decodepos++];
				bErrorOut = false;
				if ( decodepos == decodemax )
					return delayedadvance;
				return 0;
			}

			uchar32 uc;
			NOTE_UNUSED( uc ); // Squelch warning about unused, but allow for whatever decode func is used to supply the default/error value.
			int ret = DecodeFunc( pUTF, uc, bErrorOut );
			if ( bErrorOut )
			{
				uValueOut = uc;
			}
			else
			{
				decodemax = (uint8)UChar32ToUpper( uc, decoded );
				uValueOut = decoded[0];
				decodepos = 1;
				if ( decodemax != 1 )
				{
					delayedadvance = (int8)ret;
					ret = 0;
				}
			}
			return ret;
		}
	};

	template < typename SrcType, int( &DecodeFunc )(const SrcType *pUTF, uchar32 &uValueOut, bool &bErrorOut) >
	struct LinguisticToLowerFunctor
	{
		uchar32 decoded[3];
		uint8 decodepos;
		uint8 decodemax;

		LinguisticToLowerFunctor() { decoded[0] = 0; decodepos = 0; decodemax = 0; }

		int operator()( const SrcType *pUTF, uchar32 &uValueOut, bool &bErrorOut )
		{
			if ( decodepos < decodemax && decoded[decodepos] != 0 )
			{
				uValueOut = decoded[decodepos++];
				bErrorOut = false;
				return 0;
			}

			uchar32 uc;
			NOTE_UNUSED( uc ); // Squelch warning about unused, but allow for whatever decode func is used to supply the default/error value.
			int ret = DecodeFunc( pUTF, uc, bErrorOut );
			if ( bErrorOut )
			{
				uValueOut = uc;
			}
			else if ( uc == 0x03A3u ) // SPECIAL CASE: GREEK SIGMA
			{
				// Greek sigma 03A3 has a special one-of-a-kind Unicode rule:
				// if it is the last character in a 2+ character word, then it
				// lowercases to 03C2 ("final sigma") instead of 03C3.
				uValueOut = 0x03C3u;

				// Getting this right actually requires arbitrary lookahead and lookbehind
				// because you need to handle combining characters, diacritics, joiners, etc.
				// We won't bother to get it absolutely right, just a conservative "we tried".
				// Absolute perfection would require having a greek dictionary and knowing
				// the difference between a full word and an abbreviation!
				if ( decodemax > 1 || (decodemax == 1 && (decoded[0] < 0xFFFF && iswalpha( (wint_t)decoded[0] ))) )
				{
					// Previous uchar32 was alphabetic - either a long table entry, or iswalpha().
					// Now we assume that we want a final sigma, unless we lookahead and can find
					// another alphanumeric character before the first space or end of string.
					uValueOut = 0x03C2u;

					const SrcType *pLookahead = pUTF + ret;
					for ( int n = 0; *pLookahead && n < 5; ++n )
					{
						uchar32 ucAhead;
						bool bErrorAhead;
						NOTE_UNUSED( bErrorAhead );
						NOTE_UNUSED( ucAhead );
						pLookahead += DecodeFunc( pLookahead, ucAhead, bErrorAhead );
						if ( bErrorAhead || (ucAhead < 0xFFFF && iswalnum( (wint_t)ucAhead )) )
						{
							// Found another alphanumeric character before end of string.
							uValueOut = 0x03C3u;
							break;
						}
						if ( ucAhead < 0xFFFF && iswspace( (wint_t)ucAhead ) )
						{
							break;
						}
					}
				}
			}
			else
			{
				decodemax = (uint8)UChar32ToLower( uc, decoded );
				uValueOut = decoded[0];
				decodepos = 1;
			}
			return ret;
		}
	};

	template <typename SrcType, int( &DecodeFunc )(const SrcType *pUTF, uchar32 &uValueOut, bool &bErrorOut), uchar32( &CaseMap )(uchar32 uc) >
	struct WrapSimpleCaseMapFunctor
	{
		int operator()( const SrcType *pUTF, uchar32 &uValueOut, bool &bErrorOut )
		{
			uchar32 uc;
			int ret = DecodeFunc( pUTF, uc, bErrorOut );
			uValueOut = CaseMap( uc );
			return ret;
		}
	};

	template < typename T, int( &DecFn )(const T*, uchar32&, bool&), int( &EncLenFn )(uchar32), int( &EncFn )(uchar32, T*) >
	static int UnicodeCaseConvertT( const T *in, T *out, int cchOut, int nStringCaseFlags, EStringConvertErrorPolicy ePolicy )
	{
		// NOTE: linguistic conversion requires read-ahead for context, so we require in to be null-terminated for code simplicity

		Assert( in != out );
		Assert( !(nStringCaseFlags & STRINGCASE_FLAG_TURKISH) ); // NOT IMPLEMENTED YET

		EncodeLenFuncAdapter< EncLenFn > EncLen;
		EncodeFuncAdapter< T, EncFn > Enc;

		int ret = 0;
		switch ( nStringCaseFlags & (3 | STRINGCASE_FLAG_LINGUISTIC) )
		{
		case STRINGCASE_LOWER | STRINGCASE_FLAG_LINGUISTIC:
		{
			LinguisticToLowerFunctor< T, DecFn > Dec;
			ret = UnicodeConvertT_Src2<>( in, 0, out, cchOut * sizeof( T ), ePolicy, Dec, EncLen, Enc ) / sizeof( T );
		}
		break;

		case STRINGCASE_UPPER | STRINGCASE_FLAG_LINGUISTIC:
		{
			LinguisticToUpperFunctor< T, DecFn > Dec;
			ret = UnicodeConvertT_Src2<>( in, 0, out, cchOut * sizeof( T ), ePolicy, Dec, EncLen, Enc ) / sizeof( T );
		}
		break;

		case STRINGCASE_LOWER:
		{
			WrapSimpleCaseMapFunctor< T, DecFn, V_towlower32 > Dec;
			ret = UnicodeConvertT_Src2<>( in, 0, out, cchOut * sizeof( T ), ePolicy, Dec, EncLen, Enc ) / sizeof( T );
		}
		break;

		case STRINGCASE_UPPER:
		{
			WrapSimpleCaseMapFunctor< T, DecFn, V_towupper32 > Dec;
			ret = UnicodeConvertT_Src2<>( in, 0, out, cchOut * sizeof( T ), ePolicy, Dec, EncLen, Enc ) / sizeof( T );
		}
		break;

		case STRINGCASE_TITLE_ALL_WORDS | STRINGCASE_FLAG_LINGUISTIC:
		case STRINGCASE_TITLE_ALL_WORDS:
		case STRINGCASE_TITLE_FIRST_WORD | STRINGCASE_FLAG_LINGUISTIC:
		case STRINGCASE_TITLE_FIRST_WORD:
		default:
		{
			Assert( false ); // NOT IMPLEMENTED YET
			WrapSimpleCaseMapFunctor< T, DecFn, V_towupper32 > Dec;
			ret = UnicodeConvertT_Src2<>( in, 0, out, cchOut * sizeof( T ), ePolicy, Dec, EncLen, Enc ) / sizeof( T );
		}
		break;
		}

		return ret;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Case-convert a Unicode string using simple one-to-one or complex linguistic rules
//-----------------------------------------------------------------------------
int V_UnicodeCaseConvert( const char *in, char *out, int cchOut, int nStringCaseFlags, EStringConvertErrorPolicy ePolicy )
{
	return UnicodeCaseConvertT< char, V_UTF8ToUChar32, V_UChar32ToUTF8Len, V_UChar32ToUTF8 >( in, out, cchOut, nStringCaseFlags, ePolicy );
}


//-----------------------------------------------------------------------------
// Purpose: Case-convert a Unicode string using simple one-to-one or complex linguistic rules
//-----------------------------------------------------------------------------
int V_UnicodeCaseConvert( const uchar16 *in, uchar16 *out, int cchOut, int nStringCaseFlags, EStringConvertErrorPolicy ePolicy )
{
	return UnicodeCaseConvertT< uchar16, V_UTF16ToUChar32, V_UChar32ToUTF16Len, V_UChar32ToUTF16 >( in, out, cchOut, nStringCaseFlags, ePolicy );
}


//-----------------------------------------------------------------------------
// Purpose: Case-convert a Unicode string using simple one-to-one or complex linguistic rules
//-----------------------------------------------------------------------------
int V_UnicodeCaseConvert( const uchar32 *in, uchar32 *out, int cchOut, int nStringCaseFlags, EStringConvertErrorPolicy ePolicy )
{
	return UnicodeCaseConvertT< uchar32, V_UTF32ToUChar32, V_UChar32ToUTF32Len, V_UChar32ToUTF32 >( in, out, cchOut, nStringCaseFlags, ePolicy );
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if UTF-8 string contains invalid sequences.
//-----------------------------------------------------------------------------
bool Q_UnicodeValidate( const char *pUTF8 )
{
	bool bError = false;
	while ( *pUTF8 )
	{
		uchar32 uVal;
		// Our UTF-8 decoder silently fixes up 6-byte CESU-8 (improperly re-encoded UTF-16) sequences.
		// However, these are technically not valid UTF-8. So if we eat 6 bytes at once, it's an error.
		int nCharSize = Q_UTF8ToUChar32( pUTF8, uVal, bError );
		if ( bError || nCharSize == 6 )
			return false;
		pUTF8 += nCharSize;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if UTF-16 string contains invalid sequences.
//-----------------------------------------------------------------------------
bool Q_UnicodeValidate( const uchar16 *pUTF16 )
{
	bool bError = false;
	while ( *pUTF16 )
	{
		uchar32 uVal;
		pUTF16 += Q_UTF16ToUChar32( pUTF16, uVal, bError );
		if ( bError )
			return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if UTF-32 string contains invalid sequences.
//-----------------------------------------------------------------------------
bool Q_UnicodeValidate( const uchar32 *pUTF32 )
{
	while ( *pUTF32 )
	{
		if ( !Q_IsValidUChar32( *pUTF32++ ) )
			return false;
		++pUTF32;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns number of Unicode code points (aka glyphs / characters) encoded in the UTF-8 string
//-----------------------------------------------------------------------------
int Q_UnicodeLength( const char *pUTF8 )
{
	int nChars = 0;
	while ( *pUTF8 )
	{
		bool bError;
		uchar32 uVal;
		pUTF8 += Q_UTF8ToUChar32( pUTF8, uVal, bError );
		++nChars;
	}
	return nChars;
}

//-----------------------------------------------------------------------------
// Purpose: Returns number of Unicode code points (aka glyphs / characters) encoded in the UTF-16 string
//-----------------------------------------------------------------------------
int Q_UnicodeLength( const uchar16 *pUTF16 )
{
	int nChars = 0;
	while ( *pUTF16 )
	{
		bool bError;
		uchar32 uVal;
		pUTF16 += Q_UTF16ToUChar32( pUTF16, uVal, bError );
		++nChars;
	}
	return nChars;
}

//-----------------------------------------------------------------------------
// Purpose: Returns number of Unicode code points (aka glyphs / characters) encoded in the UTF-32 string
//-----------------------------------------------------------------------------
int Q_UnicodeLength( const uchar32 *pUTF32 )
{
	int nChars = 0;
	while ( *pUTF32++ )
		++nChars;
	return nChars;
}

//-----------------------------------------------------------------------------
// Purpose: Advance a UTF-8 string pointer by a certain number of Unicode code points, stopping at end of string
//-----------------------------------------------------------------------------
char *Q_UnicodeAdvance( char *pUTF8, int nChars )
{
	while ( nChars > 0 && *pUTF8 )
	{
		uchar32 uVal;
		bool bError;
		pUTF8 += Q_UTF8ToUChar32( pUTF8, uVal, bError );
		--nChars;
	}
	return pUTF8;
}

//-----------------------------------------------------------------------------
// Purpose: Advance a UTF-16 string pointer by a certain number of Unicode code points, stopping at end of string
//-----------------------------------------------------------------------------
uchar16 *Q_UnicodeAdvance( uchar16 *pUTF16, int nChars )
{
	while ( nChars > 0 && *pUTF16 )
	{
		uchar32 uVal;
		bool bError;
		pUTF16 += Q_UTF16ToUChar32( pUTF16, uVal, bError );
		--nChars;
	}
	return pUTF16;
}
	
//-----------------------------------------------------------------------------
// Purpose: Advance a UTF-32 string pointer by a certain number of Unicode code points, stopping at end of string
//-----------------------------------------------------------------------------
uchar32 *Q_UnicodeAdvance( uchar32 *pUTF32, int nChars )
{
	while ( nChars > 0 && *pUTF32 )
	{
		++pUTF32;
		--nChars;
	}
	return pUTF32;
}

//-----------------------------------------------------------------------------
// Purpose: Perform conversion. Returns number of *bytes* required if output pointer is NULL.
//-----------------------------------------------------------------------------
int Q_UTF8ToUTF16( const char *pUTF8, uchar16 *pUTF16, int cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy )
{
	return Q_UnicodeConvertT< char, uchar16, true, Q_UTF8ToUChar32, Q_UChar32ToUTF16Len, Q_UChar32ToUTF16 >( pUTF8, 0, pUTF16, cubDestSizeInBytes, ePolicy );
}

//-----------------------------------------------------------------------------
// Purpose: Perform conversion. Returns number of *bytes* required if output pointer is NULL.
//-----------------------------------------------------------------------------
int Q_UTF8ToUTF32( const char *pUTF8, uchar32 *pUTF32, int cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy )
{
	return Q_UnicodeConvertT< char, uchar32, true, Q_UTF8ToUChar32, Q_UChar32ToUTF32Len, Q_UChar32ToUTF32 >( pUTF8, 0, pUTF32, cubDestSizeInBytes, ePolicy );
}

//-----------------------------------------------------------------------------
// Purpose: Perform conversion. Returns number of *bytes* required if output pointer is NULL.
//-----------------------------------------------------------------------------
int Q_UTF16ToUTF8( const uchar16 *pUTF16, char *pUTF8, int cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy )
{
	return Q_UnicodeConvertT< uchar16, char, true, Q_UTF16ToUChar32, Q_UChar32ToUTF8Len, Q_UChar32ToUTF8 >( pUTF16, 0, pUTF8, cubDestSizeInBytes, ePolicy );
}

//-----------------------------------------------------------------------------
// Purpose: Perform conversion. Returns number of *bytes* required if output pointer is NULL.
//-----------------------------------------------------------------------------
int Q_UTF16ToUTF32( const uchar16 *pUTF16, uchar32 *pUTF32, int cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy )
{
	return Q_UnicodeConvertT< uchar16, uchar32, true, Q_UTF16ToUChar32, Q_UChar32ToUTF32Len, Q_UChar32ToUTF32 >( pUTF16, 0, pUTF32, cubDestSizeInBytes, ePolicy );
}

//-----------------------------------------------------------------------------
// Purpose: Perform conversion. Returns number of *bytes* required if output pointer is NULL.
//-----------------------------------------------------------------------------
int Q_UTF32ToUTF8( const uchar32 *pUTF32, char *pUTF8, int cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy )
{
	return Q_UnicodeConvertT< uchar32, char, true, Q_UTF32ToUChar32, Q_UChar32ToUTF8Len, Q_UChar32ToUTF8 >( pUTF32, 0, pUTF8, cubDestSizeInBytes, ePolicy );
}

//-----------------------------------------------------------------------------
// Purpose: Perform conversion. Returns number of *bytes* required if output pointer is NULL.
//-----------------------------------------------------------------------------
int Q_UTF32ToUTF16( const uchar32 *pUTF32, uchar16 *pUTF16, int cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy )
{
	return Q_UnicodeConvertT< uchar32, uchar16, true, Q_UTF32ToUChar32, Q_UChar32ToUTF16Len, Q_UChar32ToUTF16 >( pUTF32, 0, pUTF16, cubDestSizeInBytes, ePolicy );
}

//-----------------------------------------------------------------------------
// Purpose: Perform conversion. Returns number of *bytes* required if output pointer is NULL.
//-----------------------------------------------------------------------------
int Q_UTF32ToUTF32( const uchar32 *pUTF32Source, uchar32 *pUTF32Dest, int cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy )
{
	return Q_UnicodeConvertT< uchar32, uchar32, true, Q_UTF32ToUChar32, Q_UChar32ToUTF32Len, Q_UChar32ToUTF32 >( pUTF32Source, 0, pUTF32Dest, cubDestSizeInBytes, ePolicy );
}

//-----------------------------------------------------------------------------
// Purpose: Perform conversion. Returns number of *bytes* required if output pointer is NULL.
//-----------------------------------------------------------------------------
int Q_UTF8CharsToUTF16( const char *pUTF8, int nElements, uchar16 *pUTF16, int cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy )
{
	return Q_UnicodeConvertT< char, uchar16, false, Q_UTF8ToUChar32, Q_UChar32ToUTF16Len, Q_UChar32ToUTF16 >( pUTF8, nElements, pUTF16, cubDestSizeInBytes, ePolicy );
}

//-----------------------------------------------------------------------------
// Purpose: Perform conversion. Returns number of *bytes* required if output pointer is NULL.
//-----------------------------------------------------------------------------
int Q_UTF8CharsToUTF32( const char *pUTF8, int nElements, uchar32 *pUTF32, int cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy )
{
	return Q_UnicodeConvertT< char, uchar32, false, Q_UTF8ToUChar32, Q_UChar32ToUTF32Len, Q_UChar32ToUTF32 >( pUTF8, nElements, pUTF32, cubDestSizeInBytes, ePolicy );
}

//-----------------------------------------------------------------------------
// Purpose: Perform conversion. Returns number of *bytes* required if output pointer is NULL.
//-----------------------------------------------------------------------------
int Q_UTF16CharsToUTF8( const uchar16 *pUTF16, int nElements, char *pUTF8, int cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy )
{
	return Q_UnicodeConvertT< uchar16, char, false, Q_UTF16ToUChar32, Q_UChar32ToUTF8Len, Q_UChar32ToUTF8 >( pUTF16, nElements, pUTF8, cubDestSizeInBytes, ePolicy );
}

//-----------------------------------------------------------------------------
// Purpose: Perform conversion. Returns number of *bytes* required if output pointer is NULL.
//-----------------------------------------------------------------------------
int Q_UTF16CharsToUTF32( const uchar16 *pUTF16, int nElements, uchar32 *pUTF32, int cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy )
{
	return Q_UnicodeConvertT< uchar16, uchar32, false, Q_UTF16ToUChar32, Q_UChar32ToUTF32Len, Q_UChar32ToUTF32 >( pUTF16, nElements, pUTF32, cubDestSizeInBytes, ePolicy );
}

//-----------------------------------------------------------------------------
// Purpose: Perform conversion. Returns number of *bytes* required if output pointer is NULL.
//-----------------------------------------------------------------------------
int Q_UTF32CharsToUTF8( const uchar32 *pUTF32, int nElements, char *pUTF8, int cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy )
{
	return Q_UnicodeConvertT< uchar32, char, false, Q_UTF32ToUChar32, Q_UChar32ToUTF8Len, Q_UChar32ToUTF8 >( pUTF32, nElements, pUTF8, cubDestSizeInBytes, ePolicy );
}
	
//-----------------------------------------------------------------------------
// Purpose: Perform conversion. Returns number of *bytes* required if output pointer is NULL.
//-----------------------------------------------------------------------------
int Q_UTF32CharsToUTF16( const uchar32 *pUTF32, int nElements, uchar16 *pUTF16, int cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy )
{
	return Q_UnicodeConvertT< uchar32, uchar16, false, Q_UTF32ToUChar32, Q_UChar32ToUTF16Len, Q_UChar32ToUTF16 >( pUTF32, nElements, pUTF16, cubDestSizeInBytes, ePolicy );
}

//-----------------------------------------------------------------------------
// Purpose: Repair a UTF-8 string by removing or replacing invalid seqeuences. Returns non-zero on success.
//-----------------------------------------------------------------------------
int Q_UnicodeRepair( char *pUTF8, EStringConvertErrorPolicy ePolicy )
{
	return Q_UnicodeConvertT< char, char, true, Q_UTF8ToUChar32, Q_UChar32ToUTF8Len, Q_UChar32ToUTF8 >( pUTF8, 0, pUTF8, INT_MAX, ePolicy );
}

//-----------------------------------------------------------------------------
// Purpose: Repair a UTF-16 string by removing or replacing invalid seqeuences. Returns non-zero on success.
//-----------------------------------------------------------------------------
int Q_UnicodeRepair( uchar16 *pUTF16, EStringConvertErrorPolicy ePolicy )
{
	return Q_UnicodeConvertT< uchar16, uchar16, true, Q_UTF16ToUChar32, Q_UChar32ToUTF16Len, Q_UChar32ToUTF16 >( pUTF16, 0, pUTF16, INT_MAX/sizeof(uchar16), ePolicy );
}
	
//-----------------------------------------------------------------------------
// Purpose: Repair a UTF-32 string by removing or replacing invalid seqeuences. Returns non-zero on success.
//-----------------------------------------------------------------------------
int Q_UnicodeRepair( uchar32 *pUTF32, EStringConvertErrorPolicy ePolicy )
{
	return Q_UnicodeConvertT< uchar32, uchar32, true, Q_UTF32ToUChar32, Q_UChar32ToUTF32Len, Q_UChar32ToUTF32 >( pUTF32, 0, pUTF32, INT_MAX/sizeof(uchar32), ePolicy );
}

