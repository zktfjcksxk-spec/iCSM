//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
// 
// Functions for UCS/UTF/Unicode string operations. These functions are in vstdlib
// instead of tier1, because on PS/3 they need to load and initialize a system module,
// which is more frugal to do from a single place rather than multiple times in different PRX'es.
// The functions themselves aren't supposed to be called frequently enough for the DLL/PRX boundary
// marshalling, if any, to have any measureable impact on performance.
//
#ifndef VSTRTOOLS_HDR
#define VSTRTOOLS_HDR

#include "tier0/platform.h"
#include "tier0/basetypes.h"
#include "tier1/strtools.h"
#include "tier0/memalloc.h"

#ifdef VSTDLIB_DLL_EXPORT
#define VSTRTOOLS_INTERFACE DLL_EXPORT
#else
#define VSTRTOOLS_INTERFACE DLL_IMPORT
#endif


// conversion functions wchar_t <-> char, returning the number of characters converted
VSTRTOOLS_INTERFACE int V_UTF8ToUnicode( const char *pUTF8, OUT_Z_BYTECAP(cubDestSizeInBytes) wchar_t *pwchDest, int cubDestSizeInBytes );
VSTRTOOLS_INTERFACE int V_UnicodeToUTF8( const wchar_t *pUnicode, OUT_Z_BYTECAP(cubDestSizeInBytes) char *pUTF8, int cubDestSizeInBytes );
VSTRTOOLS_INTERFACE int V_UCS2ToUnicode( const ucs2 *pUCS2, OUT_Z_BYTECAP(cubDestSizeInBytes) wchar_t *pUnicode, int cubDestSizeInBytes );
VSTRTOOLS_INTERFACE int V_UCS2ToUTF8( const ucs2 *pUCS2, OUT_Z_BYTECAP(cubDestSizeInBytes) char *pUTF8, int cubDestSizeInBytes );
VSTRTOOLS_INTERFACE int V_UnicodeToUCS2( const wchar_t *pUnicode, int cubSrcInBytes, OUT_Z_BYTECAP(cubDestSizeInBytes) char *pUCS2, int cubDestSizeInBytes );
VSTRTOOLS_INTERFACE int V_UTF8ToUCS2( const char *pUTF8, int cubSrcInBytes, OUT_Z_BYTECAP(cubDestSizeInBytes) ucs2 *pUCS2, int cubDestSizeInBytes );

// copy at most n bytes into destination, will not corrupt utf-8 multi-byte sequences
VSTRTOOLS_INTERFACE void * V_UTF8_strncpy( OUT_Z_BYTECAP(nMaxBytes) char *pDest, const char *pSrc, size_t nMaxBytes );


//
// This utility class is for performing UTF-8 <-> UTF-16 conversion.
// It is intended for use with function/method parameters.
//
// For example, you can call
//     FunctionTakingUTF16( CStrAutoEncode( utf8_string ).ToWString() )
// or
//     FunctionTakingUTF8( CStrAutoEncode( utf16_string ).ToString() )
//
// The converted string is allocated off the heap, and destroyed when
// the object goes out of scope.
//
// if the string cannot be converted, NULL is returned.
//
// This class doesn't have any conversion operators; the intention is
// to encourage the developer to get used to having to think about which
// encoding is desired.
//
class CStrAutoEncode
{
public:

	// ctor
	explicit CStrAutoEncode( const char *pch )
	{
		m_pch = pch;
		m_pwch = NULL;
#if !defined( WIN32 ) && !defined(_WIN32)
		m_pucs2 = NULL;
		m_bCreatedUCS2 = false;
#endif
		m_bCreatedUTF16 = false;
	}

	// ctor
	explicit CStrAutoEncode( const wchar_t *pwch )
	{
		m_pch = NULL;
		m_pwch = pwch;
#if !defined( WIN32 ) && !defined(_WIN32)
		m_pucs2 = NULL;
		m_bCreatedUCS2 = false;
#endif
		m_bCreatedUTF16 = true;
	}

#if !defined(WIN32) && !defined(_WINDOWS) && !defined(_WIN32) && !defined(_PS3)
	explicit CStrAutoEncode( const ucs2 *pwch )
	{
		m_pch = NULL;
		m_pwch = NULL;
		m_pucs2 = pwch;
		m_bCreatedUCS2 = true;
		m_bCreatedUTF16 = false;
	}
#endif

	// returns the UTF-8 string, converting on the fly.
	const char* ToString()
	{
		PopulateUTF8();
		return m_pch;
	}

	// Same as ToString() but here to match Steam's interface for this class
	const char *ToUTF8() { return ToString(); }


	// returns the UTF-8 string - a writable pointer.
	// only use this if you don't want to call const_cast
	// yourself. We need this for cases like CreateProcess.
	char* ToStringWritable()
	{
		PopulateUTF8();
		return const_cast< char* >( m_pch );
	}

	// returns the UTF-16 string, converting on the fly.
	const wchar_t* ToWString()
	{
		PopulateUTF16();
		return m_pwch;
	}

#if !defined( WIN32 ) && !defined(_WIN32)
	// returns the UTF-16 string, converting on the fly.
	const ucs2* ToUCS2String()
	{
		PopulateUCS2();
		return m_pucs2;
	}
#endif

	// returns the UTF-16 string - a writable pointer.
	// only use this if you don't want to call const_cast
	// yourself. We need this for cases like CreateProcess.
	wchar_t* ToWStringWritable()
	{
		PopulateUTF16();
		return const_cast< wchar_t* >( m_pwch );
	}

	// dtor
	~CStrAutoEncode()
	{
		// if we're "native unicode" then the UTF-8 string is something we allocated,
		// and vice versa.
		if ( m_bCreatedUTF16 )
		{
			delete [] m_pch;
		}
		else
		{
			delete [] m_pwch;
		}
#if !defined( WIN32 ) && !defined(_WIN32)
		if ( !m_bCreatedUCS2 && m_pucs2 )
			delete [] m_pucs2;
#endif
	}

private:
	// ensure we have done any conversion work required to farm out a
	// UTF-8 encoded string.
	//
	// We perform two heap allocs here; the first one is the worst-case
	// (four bytes per Unicode code point). This is usually quite pessimistic,
	// so we perform a second allocation that's just the size we need.
	void PopulateUTF8()
	{
		if ( !m_bCreatedUTF16 )
			return;					// no work to do
		if ( m_pwch == NULL )
			return;					// don't have a UTF-16 string to convert
		if ( m_pch != NULL )
			return;					// already been converted to UTF-8; no work to do

		// each Unicode code point can expand to as many as four bytes in UTF-8; we
		// also need to leave room for the terminating NUL.
		uint32 cbMax = 4 * static_cast<uint32>( V_wcslen( m_pwch ) ) + 1;
		char *pchTemp = new char[ cbMax ];
		if ( V_UnicodeToUTF8( m_pwch, pchTemp, cbMax ) )
		{
			uint32 cchAlloc = static_cast<uint32>( V_strlen( pchTemp ) ) + 1;
			char *pchHeap = new char[ cchAlloc ];
			V_strncpy( pchHeap, pchTemp, cchAlloc );
			delete [] pchTemp;
			m_pch = pchHeap;
		}
		else
		{
			// do nothing, and leave the UTF-8 string NULL
			delete [] pchTemp;
		}
	}

	// ensure we have done any conversion work required to farm out a
	// UTF-16 encoded string.
	//
	// We perform two heap allocs here; the first one is the worst-case
	// (one code point per UTF-8 byte). This is sometimes pessimistic,
	// so we perform a second allocation that's just the size we need.
	void PopulateUTF16()
	{
		if ( m_bCreatedUTF16 )
			return;					// no work to do
		if ( m_pch == NULL )
			return;					// no UTF-8 string to convert
		if ( m_pwch != NULL )
			return;					// already been converted to UTF-16; no work to do

		uint32 cchMax = static_cast<uint32>( V_strlen( m_pch ) ) + 1;
		wchar_t *pwchTemp = new wchar_t[ cchMax ];
		if ( V_UTF8ToUnicode( m_pch, pwchTemp, cchMax * sizeof( wchar_t ) ) )
		{
			uint32 cchAlloc = static_cast<uint32>( V_wcslen( pwchTemp ) ) + 1;
			wchar_t *pwchHeap = new wchar_t[ cchAlloc ];
			V_wcsncpy( pwchHeap, pwchTemp, cchAlloc * sizeof( wchar_t ) );
			delete [] pwchTemp;
			m_pwch = pwchHeap;
		}
		else
		{
			// do nothing, and leave the UTF-16 string NULL
			delete [] pwchTemp;
		}
	}

#if !defined( WIN32 ) && !defined(_WIN32)
	// ensure we have done any conversion work required to farm out a
	// UTF-16 encoded string.
	//
	// We perform two heap allocs here; the first one is the worst-case
	// (one code point per UTF-8 byte). This is sometimes pessimistic,
	// so we perform a second allocation that's just the size we need.
	void PopulateUCS2()
	{
		if ( m_bCreatedUCS2 )
			return;
		if ( m_pch == NULL )
			return;					// no UTF-8 string to convert
		if ( m_pucs2 != NULL )
			return;					// already been converted to UTF-16; no work to do

		uint32 cchMax = static_cast<uint32>( V_strlen( m_pch ) ) + 1;
		ucs2 *pwchTemp = new ucs2[ cchMax ];
		if ( V_UTF8ToUCS2( m_pch, cchMax, pwchTemp, cchMax * sizeof( ucs2 ) ) )
		{
			uint32 cchAlloc = cchMax;
			ucs2 *pwchHeap = new ucs2[ cchAlloc ];
			memcpy( pwchHeap, pwchTemp, cchAlloc * sizeof( ucs2 ) );
			delete [] pwchTemp;
			m_pucs2 = pwchHeap;
		}
		else
		{
			// do nothing, and leave the UTF-16 string NULL
			delete [] pwchTemp;
		}
	}
#endif

	// one of these pointers is an owned pointer; whichever
	// one is the encoding OTHER than the one we were initialized
	// with is the pointer we've allocated and must free.
	const char *m_pch;
	const wchar_t *m_pwch;
#if !defined( WIN32 ) && !defined(_WIN32)
	const ucs2 *m_pucs2;
	bool m_bCreatedUCS2;
#endif
	// "created as UTF-16", means our owned string is the UTF-8 string not the UTF-16 one.
	bool m_bCreatedUTF16;

};


// Source2 version of CStrAutoEncode
// TODO : Consider changing over to this new version. Note the implementation is very different and so requires rigorous testing.
//
// This utility class is for performing UTF-8 <-> UTF-16 conversion.
// It is intended for use with function/method parameters.
//
// For example, you can call
//     FunctionTakingUTF16( CStrAutoEncode( utf8_string ).ToWString() )
// or
//     FunctionTakingUTF8( CStrAutoEncode( utf16_string ).ToString() )
//
// The converted string is allocated off the heap, and destroyed when
// the object goes out of scope.
//
// if the string cannot be converted, NULL is returned.
//
// This class doesn't have any conversion operators; the intention is
// to encourage the developer to get used to having to think about which
// encoding is desired.
//
class CStrAutoEncodeSrc2
{
public:
	// ctor
	explicit CStrAutoEncodeSrc2( const char *pch )
	{
		InitEmpty();
		Set( pch );
	}

	// ctor
	explicit CStrAutoEncodeSrc2( const uchar16 *pch16 )
	{
		InitEmpty();
		Set( pch16 );
	}

	explicit CStrAutoEncodeSrc2( const uchar32 *pch32 )
	{
		InitEmpty();
		Set( pch32 );
	}

	// dtor
	~CStrAutoEncodeSrc2()
	{
		Clear();
	}

	void Set( const char *pch )
	{
		Clear();

		m_pch = pch;
		m_bHaveUTF8 = true;
		m_bIsEmpty = !pch || !pch[0];
	}

	void Set( const uchar16 *pch16 )
	{
		Clear();

		m_pch16 = pch16;
		m_bHaveUTF16 = true;
		m_bIsEmpty = !pch16 || !pch16[0];
	}

	void Set( const uchar32 *pch32 )
	{
		Clear();

		m_pch32 = pch32;
		m_bHaveUTF32 = true;
		m_bIsEmpty = !pch32 || !pch32[0];
	}

	void SetCopy( const char *pStr, int nChars = -1 )
	{
		char *pCopy = NULL;
		if ( pStr )
		{
			if ( nChars < 0 )
			{
				nChars = V_strlen( pStr );
			}
			int nBytes = nChars * sizeof( *pStr );
			pCopy = (char*)MemAlloc_Alloc( nBytes + sizeof( *pStr ) );
			memcpy( pCopy, pStr, nBytes );
			pCopy[nChars] = 0;
		}

		Set( pCopy );

		if ( pStr )
		{
			m_bAllocatedUTF8 = true;
		}
	}

	void SetCopy( const uchar16 *pStr, int nChars = -1 )
	{
		uchar16 *pCopy = NULL;
		if ( pStr )
		{
			if ( nChars < 0 )
			{
				nChars = V_strlen16( pStr );
			}
			int nBytes = nChars * sizeof( *pStr );
			pCopy = (uchar16*)MemAlloc_Alloc( nBytes + sizeof( *pStr ) );
			memcpy( pCopy, pStr, nBytes );
			pCopy[nChars] = 0;
		}

		Set( pCopy );

		if ( pStr )
		{
			m_bAllocatedUTF16 = true;
		}
	}

	void SetCopy( const uchar32 *pStr, int nChars = -1 )
	{
		uchar32 *pCopy = NULL;
		if ( pStr )
		{
			if ( nChars < 0 )
			{
				nChars = V_strlen32( pStr );
			}
			int nBytes = nChars * sizeof( *pStr );
			pCopy = (uchar32*)MemAlloc_Alloc( nBytes + sizeof( *pStr ) );
			memcpy( pCopy, pStr, nBytes );
			pCopy[nChars] = 0;
		}

		Set( pCopy );

		if ( pStr )
		{
			m_bAllocatedUTF32 = true;
		}
	}

	CStrAutoEncodeSrc2& Copy( const CStrAutoEncodeSrc2& other )
	{
		// Select an existing encoding instead of
		// using a To method to avoid causing any
		// conversion that we're not sure we'll need.
		if ( other.m_pch )
		{
			SetCopy( other.m_pch );
		}
		else if ( other.m_pch16 )
		{
			SetCopy( other.m_pch16 );
		}
		else if ( other.m_pch32 )
		{
			SetCopy( other.m_pch32 );
		}
		else
		{
			Clear();
		}
		return *this;
	}

	// returns the UTF-8 string, converting on the fly.
	const char* ToString() const
	{
		PopulateUTF8();
		return m_pch;
	}

	// Same as ToString() but here to match Steam's interface for this class
	const char *ToUTF8() const { return ToString(); }

	// returns the UTF-8 string - a writable pointer.
	// only use this if you don't want to call const_cast
	// yourself. We need this for cases like CreateProcess.
	char* ToStringWritable()
	{
		PopulateUTF8();
		return const_cast<char*>(m_pch);
	}

	const uchar16* ToUTF16() const
	{
		PopulateUTF16();
		return m_pch16;
	}
	uchar16* ToUTF16Writable()
	{
		PopulateUTF16();
		return const_cast<uchar16*>(m_pch16);
	}

	const uchar32* ToUTF32() const
	{
		PopulateUTF32();
		return m_pch32;
	}
	uchar32* ToUTF32Writable()
	{
		PopulateUTF32();
		return const_cast<uchar32*>(m_pch32);
	}

	// returns the wchar_t string, converting on the fly.
	const wchar_t* ToWString() const
	{
#ifdef _WIN32
		return ToUTF16();
#else
		return ToUTF32();
#endif
	}

	// returns the wchar_t string - a writable pointer.
	// only use this if you don't want to call const_cast
	// yourself. We need this for cases like CreateProcess.
	wchar_t* ToWStringWritable()
	{
#ifdef _WIN32
		return ToUTF16Writable();
#else
		return ToUTF32Writable();
#endif
	}

	// Release memory and reset
	void Clear()
	{
		if ( m_bAllocatedUTF8 )
		{
			g_pMemAlloc->Free( const_cast<char *>(m_pch));
		}
		if ( m_bAllocatedUTF16 )
		{
			g_pMemAlloc->Free( const_cast<uchar16 *>(m_pch16) );
		}
		if ( m_bAllocatedUTF32 )
		{
			g_pMemAlloc->Free( const_cast<uchar32 *>(m_pch32) );
		}

		InitEmpty();
	}

	bool IsEmpty() const
	{
		return m_bIsEmpty;
	}

	bool HaveUTF8() const
	{
		return m_bHaveUTF8;
	}
	bool HaveUTF16() const
	{
		return m_bHaveUTF16;
	}
	bool HaveUTF32() const
	{
		return m_bHaveUTF32;
	}

private:
	void InitEmpty()
	{
		m_pch = NULL;
		m_pch16 = NULL;
		m_pch32 = NULL;

		m_bHaveUTF8 = false;
		m_bAllocatedUTF8 = false;
		m_bHaveUTF16 = false;
		m_bAllocatedUTF16 = false;
		m_bHaveUTF32 = false;
		m_bAllocatedUTF32 = false;

		m_bIsEmpty = true;
	}

	// ensure we have done any conversion work required to farm out a
	// UTF-8 encoded string.
	//
	// We perform two heap allocs here; the first one is the worst-case
	// (four bytes per Unicode code point). This is usually quite pessimistic,
	// so we perform a second allocation that's just the size we need.
	void PopulateUTF8() const
	{
		if ( m_bHaveUTF8 )
			return;					// no work to do
		if ( m_pch != NULL )
			return;					// should never happen but don't overwrite a rogue pointer
		if ( m_pch16 == NULL && m_pch32 == NULL )
			return;					// don't have a string to convert

		int nCharGuess;
		if ( m_pch32 )
		{
			nCharGuess = V_strlen32( m_pch32 );
		}
		else
		{
			nCharGuess = V_strlen16( m_pch16 );
		}

		// ach Unicode code point can use as many as four bytes in UTF-8; we
		// also need to leave room for the terminating NUL.
		uint32 cbMax = 4 * static_cast<uint32>(nCharGuess)+1;
		bool bTempOnHeap = cbMax > (64 * 1024);
		char *pchTemp = bTempOnHeap ? (char *)MemAlloc_Alloc( cbMax ) : StackAlloc( char, cbMax );

		int nOutBytes;
		if ( m_pch32 )
		{
			nOutBytes = V_UTF32ToUTF8( m_pch32, pchTemp, cbMax );
		}
		else
		{
			nOutBytes = V_UTF16ToUTF8( m_pch16, pchTemp, cbMax );
		}
		if ( nOutBytes )
		{
			uint32 cbAlloc = (static_cast<uint32>(V_strlen( pchTemp )) + 1) * sizeof( char );
			char *pchHeap = (char *)MemAlloc_Alloc( cbAlloc );
			memcpy( pchHeap, pchTemp, cbAlloc );
			m_pch = pchHeap;
			m_bHaveUTF8 = true;
			m_bAllocatedUTF8 = true;
		}
		//else do nothing, and leave the UTF-8 string NULL

		if ( bTempOnHeap )
		{
			g_pMemAlloc->Free( pchTemp );
		}
	}

	// ensure we have done any conversion work required to farm out a
	// UTF-16 encoded string.
	//
	// We perform two heap allocs here; the first one is the worst-case
	// (four bytes per Unicode code point). This is usually quite pessimistic,
	// so we perform a second allocation that's just the size we need.
	void PopulateUTF16() const
	{
		if ( m_bHaveUTF16 )
			return;					// no work to do
		if ( m_pch16 != NULL )
			return;					// should never happen but don't overwrite a rogue pointer
		if ( m_pch == NULL && m_pch32 == NULL )
			return;					// no string to convert

		int nCharGuess;
		if ( m_pch32 )
		{
			nCharGuess = V_strlen32( m_pch32 );
		}
		else
		{
			nCharGuess = V_strlen( m_pch );
		}

		// each Unicode code point can use as many as four bytes in UTF-16; we
		// also need to leave room for the terminating NUL.
		uint32 cbMax = 4 * static_cast<uint32>(nCharGuess)+sizeof( uchar16 );
		bool bTempOnHeap = cbMax > (64 * 1024);
		uchar16 *pch16Temp = bTempOnHeap ? (uchar16 *)MemAlloc_Alloc( cbMax ) : StackAlloc( uchar16, cbMax / sizeof( uchar16 ) );

		int nOutBytes;
		if ( m_pch32 )
		{
			nOutBytes = V_UTF32ToUTF16( m_pch32, pch16Temp, cbMax );
		}
		else
		{
			nOutBytes = V_UTF8ToUTF16( m_pch, pch16Temp, cbMax );
		}
		if ( nOutBytes )
		{
			uint32 cbAlloc = (static_cast<uint32>(V_strlen16( pch16Temp )) + 1) * sizeof( uchar16 );
			uchar16 *pch16Heap = (uchar16 *)MemAlloc_Alloc( cbAlloc );
			memcpy( pch16Heap, pch16Temp, cbAlloc );
			m_pch16 = pch16Heap;
			m_bHaveUTF16 = true;
			m_bAllocatedUTF16 = true;
		}
		//else do nothing, and leave the UTF-16 string NULL

		if ( bTempOnHeap )
		{
			g_pMemAlloc->Free( pch16Temp );
		}
	}

	// ensure we have done any conversion work required to farm out a
	// UTF-32 encoded string.
	//
	// We perform two heap allocs here; the first one is the worst-case
	// (four bytes per Unicode code point). This is usually quite pessimistic,
	// so we perform a second allocation that's just the size we need.
	void PopulateUTF32() const
	{
		if ( m_bHaveUTF32 )
			return;					// no work to do
		if ( m_pch32 != NULL )
			return;					// should never happen but don't overwrite a rogue pointer
		if ( m_pch == NULL && m_pch16 == NULL )
			return;					// no string to convert

		int nCharGuess;
		if ( m_pch16 )
		{
			nCharGuess = V_strlen16( m_pch16 );
		}
		else
		{
			nCharGuess = V_strlen( m_pch );
		}

		// each Unicode code point can use as many as four bytes in UTF-32; we
		// also need to leave room for the terminating NUL.
		uint32 cbMax = 4 * static_cast<uint32>(nCharGuess)+sizeof( uchar32 );
		bool bTempOnHeap = cbMax > (64 * 1024);
		uchar32 *pch32Temp = bTempOnHeap ? (uchar32 *)MemAlloc_Alloc( cbMax ) : StackAlloc( uchar32, cbMax / sizeof( uchar32 ) );

		int nOutBytes;
		if ( m_pch16 )
		{
			nOutBytes = V_UTF16ToUTF32( m_pch16, pch32Temp, cbMax );
		}
		else
		{
			nOutBytes = V_UTF8ToUTF32( m_pch, pch32Temp, cbMax );
		}
		if ( nOutBytes )
		{
			uint32 cbAlloc = (static_cast<uint32>(V_strlen32( pch32Temp )) + 1) * sizeof( uchar32 );
			uchar32 *pch32Heap = (uchar32 *)MemAlloc_Alloc( cbAlloc );
			memcpy( pch32Heap, pch32Temp, cbAlloc );
			m_pch32 = pch32Heap;
			m_bHaveUTF32 = true;
			m_bAllocatedUTF32 = true;
		}
		//else do nothing, and leave the UTF-32 string NULL

		if ( bTempOnHeap )
		{
			g_pMemAlloc->Free( pch32Temp );
		}
	}

	// Everything is mutable so that conversions can be
	// done even on a const instance.

	mutable const char *m_pch;
	mutable const uchar16 *m_pch16;
	mutable const uchar32 *m_pch32;

	// We have a string if it was set OR created via conversion.
	// Converted strings will be allocated.
	mutable bool m_bHaveUTF8 : 1;
	mutable bool m_bAllocatedUTF8 : 1;
	mutable bool m_bHaveUTF16 : 1;
	mutable bool m_bAllocatedUTF16 : 1;
	mutable bool m_bHaveUTF32 : 1;
	mutable bool m_bAllocatedUTF32 : 1;

	bool m_bIsEmpty : 1;
};


#define Q_UTF8ToUnicode			V_UTF8ToUnicode
#define Q_UnicodeToUTF8			V_UnicodeToUTF8


#endif 