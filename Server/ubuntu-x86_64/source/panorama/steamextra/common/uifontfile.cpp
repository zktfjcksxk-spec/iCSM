//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uifontfile.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;
#if defined( SOURCE2_PANORAMA )
const int k_nSymmetricKeyLen = 32;						// length in bytes of keys used for symmetric encryption
const int k_nSymmetricBlockSize = 16;
// openssl optimized AES routines
#include "openssl/aes.h"
#include "openssl/sha.h"

/*
#if defined(_M_IX86) || defined (_M_X64) || defined(__i386__) || defined(__x86_64__)
#define ENABLE_AESNI_INSTRINSIC_PATH 1
#include <emmintrin.h>
// stupid: can't use wmmintrin.h on gcc/clang, requires AESNI codegen to be globally enabled!
#if defined(__clang__) || defined(__GNUC__)
static FORCEINLINE __m128i _mm_aesenc_si128( __m128i a, __m128i b ) { asm( "aesenc %1, %0" : "+x"( a ) : "x"( b ) ); return a; }
static FORCEINLINE __m128i _mm_aesenclast_si128( __m128i a, __m128i b ) { asm( "aesenclast %1, %0" : "+x"( a ) : "x"( b ) ); return a; }
static FORCEINLINE __m128i _mm_aesdec_si128( __m128i a, __m128i b ) { asm( "aesdec %1, %0" : "+x"( a ) : "x"( b ) ); return a; }
static FORCEINLINE __m128i _mm_aesdeclast_si128( __m128i a, __m128i b ) { asm( "aesdeclast %1, %0" : "+x"( a ) : "x"( b ) ); return a; }
#else
#include <wmmintrin.h>
#endif
#endif*/


// Local helper to perform AES+CBC decryption using optimized OpenSSL AES routines
static bool BDecryptAESUsingOpenSSL( const uint8 *pubEncryptedData, uint32 cubEncryptedData, uint8 *pubPlaintextData, uint32 *pcubPlaintextData, AES_KEY *key, const uint8 *pIV )
{
	COMPILE_TIME_ASSERT( k_nSymmetricBlockSize == 16 );

	// Block cipher encrypted text must be a multiple of the block size
	if ( cubEncryptedData % k_nSymmetricBlockSize != 0 )
		return false;

	// Enough input? Requirement is one padded final block
	if ( cubEncryptedData < k_nSymmetricBlockSize )
		return false;

	// Enough output space for all the full non-final blocks?
	if ( *pcubPlaintextData < cubEncryptedData - k_nSymmetricBlockSize )
		return false;

	uint8 rgubWorking[ k_nSymmetricBlockSize ];
	uint32 nDecrypted = 0;

#ifdef ENABLE_AESNI_INSTRINSIC_PATH
	// 4-at-a-time AESNI instructions loop is 10-20x faster than software AES decryption
	uint32 roundKeysAsU32[ 15 * 4 ];
	if ( GetCPUInformation().m_bAES && cubEncryptedData >= 80 && BExtractAES256RoundKeys( key, true, roundKeysAsU32 ) )
	{
		COMPILE_TIME_ASSERT( k_nSymmetricBlockSize * 4 == 64 );
		while ( nDecrypted + 63 < cubEncryptedData - k_nSymmetricBlockSize )
		{
			__m128i workData1 = _mm_loadu_si128( ( __m128i* )( pubEncryptedData + nDecrypted ) );
			__m128i workData2 = _mm_loadu_si128( ( __m128i* )( pubEncryptedData + nDecrypted + 16 ) );
			__m128i workData3 = _mm_loadu_si128( ( __m128i* )( pubEncryptedData + nDecrypted + 32 ) );
			__m128i workData4 = _mm_loadu_si128( ( __m128i* )( pubEncryptedData + nDecrypted + 48 ) );

			const int nRounds = 14;
			__m128i roundKey = _mm_loadu_si128( ( __m128i* )&roundKeysAsU32[ 0 ] );
			workData1 = _mm_xor_si128( workData1, roundKey );
			workData2 = _mm_xor_si128( workData2, roundKey );
			workData3 = _mm_xor_si128( workData3, roundKey );
			workData4 = _mm_xor_si128( workData4, roundKey );
			for ( int iRound = 1; iRound < nRounds; ++iRound )
			{
				roundKey = _mm_loadu_si128( ( __m128i* )&roundKeysAsU32[ 4 * iRound ] );
				workData1 = _mm_aesdec_si128( workData1, roundKey );
				workData2 = _mm_aesdec_si128( workData2, roundKey );
				workData3 = _mm_aesdec_si128( workData3, roundKey );
				workData4 = _mm_aesdec_si128( workData4, roundKey );
			}
			roundKey = _mm_loadu_si128( ( __m128i* )&roundKeysAsU32[ 4 * nRounds ] );
			workData1 = _mm_aesdeclast_si128( workData1, roundKey );
			workData2 = _mm_aesdeclast_si128( workData2, roundKey );
			workData3 = _mm_aesdeclast_si128( workData3, roundKey );
			workData4 = _mm_aesdeclast_si128( workData4, roundKey );

			workData1 = _mm_xor_si128( workData1, _mm_loadu_si128( ( __m128i* )( pIV ) ) );
			workData2 = _mm_xor_si128( workData2, _mm_loadu_si128( ( __m128i* )( pubEncryptedData + nDecrypted ) ) );
			workData3 = _mm_xor_si128( workData3, _mm_loadu_si128( ( __m128i* )( pubEncryptedData + nDecrypted + 16 ) ) );
			workData4 = _mm_xor_si128( workData4, _mm_loadu_si128( ( __m128i* )( pubEncryptedData + nDecrypted + 32 ) ) );

			_mm_storeu_si128( ( __m128i* )( pubPlaintextData + nDecrypted ), workData1 );
			_mm_storeu_si128( ( __m128i* )( pubPlaintextData + nDecrypted + 16 ), workData2 );
			_mm_storeu_si128( ( __m128i* )( pubPlaintextData + nDecrypted + 32 ), workData3 );
			_mm_storeu_si128( ( __m128i* )( pubPlaintextData + nDecrypted + 48 ), workData4 );

			pIV = pubEncryptedData + nDecrypted + 48;
			nDecrypted += 64;
		}
		SecureZeroMemory( roundKeysAsU32, sizeof( roundKeysAsU32 ) );
	}
#endif

	// Decrypt blocks (or finish decryption of non-multiple-of-4 blocks in AESNI case) 
	while ( nDecrypted < cubEncryptedData - k_nSymmetricBlockSize )
	{
		AES_decrypt( pubEncryptedData + nDecrypted, rgubWorking, key );
		uint64 temp1 = ( ( UNALIGNED uint64* )pIV )[ 0 ] ^ ( ( UNALIGNED uint64* )rgubWorking )[ 0 ];
		uint64 temp2 = ( ( UNALIGNED uint64* )pIV )[ 1 ] ^ ( ( UNALIGNED uint64* )rgubWorking )[ 1 ];
		( ( UNALIGNED uint64* )( pubPlaintextData + nDecrypted ) )[ 0 ] = temp1;
		( ( UNALIGNED uint64* )( pubPlaintextData + nDecrypted ) )[ 1 ] = temp2;
		pIV = pubEncryptedData + nDecrypted;
		nDecrypted += k_nSymmetricBlockSize;
	}

	// Process final block into rgubWorking for padding inspection
	Assert( nDecrypted == cubEncryptedData - k_nSymmetricBlockSize );
	AES_decrypt( pubEncryptedData + nDecrypted, rgubWorking, key );
	for ( int i = 0; i < k_nSymmetricBlockSize; ++i )
		rgubWorking[ i ] ^= pIV[ i ];

	// Get final block padding length and make sure it is backfilled properly (PKCS#5)
	uint8 pad = rgubWorking[ k_nSymmetricBlockSize - 1 ];
	uint8 checkBits = 0;
	for ( int i = ( k_nSymmetricBlockSize - pad ) & 15; i < k_nSymmetricBlockSize; ++i )
		checkBits |= rgubWorking[ i ] ^ pad;
	if ( checkBits != 0 )
		return false;

	// Check that we have enough space for final bytes
	if ( *pcubPlaintextData < nDecrypted + k_nSymmetricBlockSize - pad )
		return false;

	// Write any non-pad bytes from rgubWorking to pubPlaintextData
	for ( int i = 0; i < k_nSymmetricBlockSize - pad; ++i )
		pubPlaintextData[ nDecrypted++ ] = rgubWorking[ i ];

	// The old CryptoPP path zeros out the entire destination buffer, but that
	// behavior isn't documented or even expected. We'll just zero out one byte
	// in case anyone relies on string termination, but that zero isn't counted.
	if ( *pcubPlaintextData > nDecrypted )
		pubPlaintextData[ nDecrypted ] = 0;

	*pcubPlaintextData = nDecrypted;
	return true;
}

#endif // SOURCE2_PANORAMA

static uint8 rgubFontKey[k_nSymmetricKeyLen] = { 19,	230,	33,		20,		199,	250,	60,		185, 
												62,		134,	244,	118,	246,	179,	44,		32, 
												77,		130,	164,	25,		175,	243,	19,		174, 
												187,	161,	175,	146,	231,	160,	172,	141 
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUIFontPackage::CUIFontPackage( const char *pchFontPackagePath )
{
	m_strPackageFile = pchFontPackagePath;
	m_pFontPackagePB = NULL;
	m_iCurIndex = -1;
	CUtlBuffer bufFontData;

#ifdef SOURCE2_PANORAMA
	bufFontData.SetBufferType( false, false );
	if ( !g_pFullFileSystem->ReadFile( pchFontPackagePath, NULL, bufFontData ) )
		return;
#else
	if ( !UIEngine()->UIFileSystem()->LoadFileIntoBuffer( pchFontPackagePath, bufFontData, false ) ) 
		return;
#endif

	m_pFontPackagePB = new CUIFontFilePackagePB();
	if ( !m_pFontPackagePB->ParseFromArray( bufFontData.Base(), bufFontData.TellPut() ) )
		m_pFontPackagePB = NULL;
	else
		Assert( m_pFontPackagePB->package_version() ==  1 );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUIFontPackage::~CUIFontPackage()
{
	SAFE_DELETE( m_pFontPackagePB );
}


//-----------------------------------------------------------------------------
// Purpose: Get next file index
//-----------------------------------------------------------------------------
int CUIFontPackage::GetNextFileIndex()
{
	if ( !m_pFontPackagePB )
		return InvalidFileIndex();
	else
	{
		++m_iCurIndex;
		if ( m_iCurIndex < m_pFontPackagePB->encrypted_font_files_size() )
			return m_iCurIndex;
		else
		{
			m_iCurIndex = -1;
			return m_iCurIndex;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get data for a font by index
//-----------------------------------------------------------------------------
bool CUIFontPackage::BGetFontNameAndData( int iIndex, CUtlString &strFontName, CUtlBuffer *pBufFontData )
{
	Assert( m_pFontPackagePB );
	if ( m_pFontPackagePB )
		Assert( iIndex >= 0 && iIndex < m_pFontPackagePB->encrypted_font_files_size() );

	if ( iIndex < 0 || m_pFontPackagePB == NULL || iIndex >= m_pFontPackagePB->encrypted_font_files_size() )
		return false;

	CUIFontFilePackagePB::CUIEncryptedFontFilePB const &fontFile = m_pFontPackagePB->encrypted_font_files( iIndex );
	
	CUtlBuffer bufDecryptedData;
	bufDecryptedData.EnsureCapacity( ( int )fontFile.encrypted_contents().size() );

#if defined( SOURCE2_PANORAMA )
	AES_KEY key;
	if ( AES_set_decrypt_key( rgubFontKey,  Q_ARRAYSIZE( rgubFontKey ) * 8, &key ) < 0 )
		return false;

	// Our first block is straight AES block encryption of IV with user key, no XOR.
	uint8 rgubIV[ k_nSymmetricBlockSize ];
	const unsigned char * pubEncryptedData = (const unsigned char *)fontFile.encrypted_contents().data();
	int cubEncryptedData = ( int )fontFile.encrypted_contents().size();
	AES_decrypt( pubEncryptedData, rgubIV, &key );
	pubEncryptedData += k_nSymmetricBlockSize;
	cubEncryptedData -= k_nSymmetricBlockSize;

#ifndef PANORAMA_USE_S1WRAPPER
	uint32 unDecryptedBytes = bufDecryptedData.SizeAllocated();
#else
	uint32 unDecryptedBytes = bufDecryptedData.Size();
#endif

	bool bDecrypted = BDecryptAESUsingOpenSSL( pubEncryptedData, cubEncryptedData, (uint8*)bufDecryptedData.Base(), &unDecryptedBytes, &key, rgubIV );

#ifdef PLATFORM_WINDOWS
	SecureZeroMemory( &key, sizeof(key) );
	SecureZeroMemory( rgubIV, sizeof( rgubIV ) ); 
#else
	bzero( &key, sizeof(key) );
	bzero( rgubIV, sizeof( rgubIV ) );
	
#endif
	
#else
	uint32 unDecryptedBytes = fontFile.encrypted_contents().size();
	bool bDecrypted = CCrypto::SymmetricDecrypt( (uint8*)fontFile.encrypted_contents().data(), fontFile.encrypted_contents().size(), (uint8*)bufDecryptedData.Base(), &unDecryptedBytes, rgubFontKey, Q_ARRAYSIZE( rgubFontKey ) ) )
#endif
	if ( bDecrypted )
	{

		bufDecryptedData.SeekPut( CUtlBuffer::SEEK_HEAD, unDecryptedBytes );

		CUIFontFilePB fontFileDecrypted;
		if ( fontFileDecrypted.ParseFromArray( bufDecryptedData.Base(), bufDecryptedData.TellPut() ) )
		{
			strFontName = fontFileDecrypted.font_file_name().c_str();
			pBufFontData->Put( fontFileDecrypted.opentype_font_data().data(), ( int )fontFileDecrypted.opentype_font_data().size() );
			return true;
		}
	}

	return false;
}


#ifdef ALLOW_FONT_PACKAGE_CREATION
bool BGenerateFontPackage( const char *pchFontPackageSourcePath, const char *pchFontPackageOutputFile )
{
	CUtlString strPath = pchFontPackageSourcePath;
	if ( strPath[strPath.Length()-1] != CORRECT_PATH_SEPARATOR )
		strPath += CORRECT_PATH_SEPARATOR_S;

	CUtlString strSearch = strPath;
	strSearch += "*";
	CDirIterator fontIterator( strSearch.String() );
		
	CUIFontFilePackagePB packagePB;
	packagePB.set_package_version( 1 );

	uint32 unCountAdded = 0;
	while ( fontIterator.BNextFile() )
	{
		if ( !fontIterator.BCurrentIsDir() )
		{
			if ( Q_stristr( fontIterator.CurrentFileName(), ".ttf" ) != NULL || Q_stristr( fontIterator.CurrentFileName(), ".otf" ) != NULL )
			{
				CUtlString strFullPath = strPath;
				strFullPath += fontIterator.CurrentFileName();

				CUtlBuffer bufData;
				if ( UIEngine()->UIFileSystem()->LoadFileIntoBuffer( strFullPath, bufData, false ) )
				{
					CUIFontFilePB fontFile;
					fontFile.set_font_file_name( fontIterator.CurrentFileName() );
					fontFile.set_opentype_font_data( bufData.Base(), bufData.TellPut() );

					CUtlBuffer bufSerialized;
					uint32 unBytesSerialized = fontFile.ByteSize();
					bufSerialized.EnsureCapacity( unBytesSerialized );
					fontFile.SerializeWithCachedSizesToArray( (uint8*)bufSerialized.Base() );
					bufSerialized.SeekPut( CUtlBuffer::SEEK_HEAD, unBytesSerialized );

					CUtlBuffer bufEncrypted;
					uint32 unBytesEncrypted = CCrypto::GetSymmetricEncryptedSize( unBytesSerialized );
					bufEncrypted.EnsureCapacity( unBytesEncrypted );

					if ( CCrypto::SymmetricEncrypt( (uint8*)bufSerialized.Base(), bufSerialized.TellPut(), (uint8*)bufEncrypted.Base(), &unBytesEncrypted, rgubFontKey, Q_ARRAYSIZE( rgubFontKey ) ) && unBytesEncrypted )
					{
						++unCountAdded;
						CUIFontFilePackagePB_CUIEncryptedFontFilePB *pEncryptedFile = packagePB.add_encrypted_font_files();
						pEncryptedFile->set_encrypted_contents( bufEncrypted.Base(), unBytesEncrypted );
					}
				}
			}
		}
	}

	if ( unCountAdded )
	{
		CUtlBuffer bufOutput;
		uint32 unBytesPackage = packagePB.ByteSize();
		bufOutput.EnsureCapacity( unBytesPackage );
		packagePB.SerializeWithCachedSizesToArray( (uint8*)bufOutput.Base() );
		bufOutput.SeekPut( CUtlBuffer::SEEK_HEAD, unBytesPackage );

		return SaveBufferToFile( bufOutput, pchFontPackageOutputFile );
	}
	else
	{
		return false;
	}


}
#endif
