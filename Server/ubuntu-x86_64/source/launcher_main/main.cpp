//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.
//
//=====================================================================================//
#if defined( _WIN32 ) && !defined( _X360 )
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <direct.h>
#endif
#if defined( _X360 )
#define _XBOX
#include <xtl.h>
#include <xbdm.h>
#undef _XBOX
#include <stdio.h>
#include <assert.h>
#include "xbox\xbox_core.h"
#include "xbox\xbox_launch.h"
#elif defined( SN_TARGET_PS3 )
#include "../public/ps3_pathinfo.h"
#include <stddef.h>
#include <cell/fios/fios_common.h>
#include <cell/fios/fios_memory.h>
#include <cell/fios/fios_configuration.h>
#include <cell/fios/fios_time.h>
#include <sys/tty.h>
#include <sys/ppu_thread.h>
#include "tier0/vprof_sn.h"
#include "errorrenderloop.h"
//#if defined( VPROF_SN_LEVEL )
#include "sn/libsntuner.h"
#include "libsn.h"
//#endif
#endif
#ifdef POSIX
#include <stdio.h>
#include <stdlib.h>
#ifndef SN_TARGET_PS3
#include <dlfcn.h>
#endif // SN_TARGET_PS3
#include <limits.h>
#include <string.h>
#define MAX_PATH PATH_MAX
#endif

#include "tier0/platform.h"
#include "tier0/basetypes.h"

#if defined( VPCGAME )
#define _VPCGAME_STRING_HACK2(x) #x
#define _VPCGAME_STRING_HACK1(x) _VPCGAME_STRING_HACK2(x)
#define VPCGAME_STRING _VPCGAME_STRING_HACK1(VPCGAME)
#endif

#ifdef WIN32
typedef int (*LauncherMain_t)( int nSecure, HINSTANCE hInstance, HINSTANCE hPrevInstance, 
							  LPSTR lpCmdLine, int nCmdShow );
#elif POSIX
typedef int (*LauncherMain_t)( int argc, char **argv );
#else
#error
#endif


#ifdef WIN32
// hinting the nvidia driver to use the dedicated graphics card in an optimus configuration
// for more info, see: http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
extern "C" { _declspec( dllexport ) DWORD NvOptimusEnablement = 0x00000001; }

// same thing for AMD GPUs using v13.35 or newer drivers
extern "C" { __declspec( dllexport ) int AmdPowerXpressRequestHighPerformance = 1; }

#include "tier0/threadtools.h"
#include "detour/detourfunc.h"

#define SELFCHECK_VERBOSE 0
// inline void SELFCHECK_REPORT( const wchar_t * wszMsgText ) { OutputDebugStringW( wszMsgText ); }
// inline void SELFCHECK_REPORT( const wchar_t * wszMsgText ) { MessageBoxW( NULL, wszMsgText, L"Report", MB_OK ); }

#define SELFCHECK_HOOK_NTOPENFILE 1
#if SELFCHECK_HOOK_NTOPENFILE
#include "winternl.h"
#endif

// Half of the Windows binaries and odd tools/drivers are not digitally signed,
// also WinVerifyTrust is expensive performance-wise, so we cannot use it for retail
#include "softpub.h"
#include "wintrust.h"

//
// Actual checksum validation
//
#include "tier1/checksum_sha1.h"
#include "tier1/checksum_crc.h"

#endif



//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------
#if !defined( _X360 )

#ifdef WIN32

static wchar_t *g_pwchBaseDir = NULL;
static int g_numBaseDirChars = 0;
static bool g_bRunningPerfectWorld = false;

#if SELFCHECK_HOOK_NTOPENFILE

typedef NTSTATUS ( NTAPI *NtOpenFile_t )(
	_Out_ PHANDLE            FileHandle,
	_In_  ACCESS_MASK        DesiredAccess,
	_In_  POBJECT_ATTRIBUTES ObjectAttributes,
	_Out_ PIO_STATUS_BLOCK   IoStatusBlock,
	_In_  ULONG              ShareAccess,
	_In_  ULONG              OpenOptions
	);
NtOpenFile_t g_pNtOpenFileReal = NULL;
#define NTOPENFILE_STATUS_OBJECT_NAME_NOT_FOUND 0xc0000034

#endif

struct ValidationFile_t
{
	wchar_t m_wchFile[MAX_PATH];
	int m_numBaseNameWChars;
	wchar_t m_wchBaseName[64];
	bool m_bComputedSignature;
	bool m_bMismatchingSignature;
	SHADigest_t m_sha;
	CRC32_t m_crc;
	bool m_bCheckNextAlternativeSignature;
};

bool BComputeFileSignature( const wchar_t *wszFile, uint8 *pSha, CRC32_t *pCrc )
{
	bool bResult = false;

	HANDLE hFile = ::CreateFileW( wszFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
#if SELFCHECK_VERBOSE
	const wchar_t *wszShimName = NULL;
#endif
	if ( !( hFile && ( hFile != INVALID_HANDLE_VALUE ) ) && ( GetLastError() == ERROR_INVALID_NAME ) &&
		wszFile[0] == '\\' && wszFile[1] == '?' && wszFile[2] == '?' && wszFile[3] == '\\' )
	{	// Windows XP shim: it seems to be unable to open data files by kernel object name, replace it as more common Unicode full path and try one more time
		int numWchars = wcslen( wszFile ) + 1;
		int numBytes = numWchars * sizeof( wchar_t );
		wchar_t *pwchCopy = ( wchar_t * ) stackalloc( numBytes );
		memcpy( pwchCopy, wszFile, numBytes );
		pwchCopy[1] = '\\'; // Switch to Windows NT \\?\ prefix for Unicode fully qualified path designation
		hFile = ::CreateFileW( pwchCopy, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
#if SELFCHECK_VERBOSE
		wszShimName = pwchCopy;
#endif
	}
	if ( hFile && ( hFile != INVALID_HANDLE_VALUE ) )
	{
		HANDLE hMap = CreateFileMapping( hFile, NULL, PAGE_READONLY, 0, 0, NULL );
		if ( hMap && ( hMap != INVALID_HANDLE_VALUE ) )
		{
			byte * const pubData = ( byte * ) MapViewOfFile( hMap, FILE_MAP_READ, 0, 0, 0 );
			if ( pubData )
			{
				LARGE_INTEGER liFileSize;
				if ( GetFileSizeEx( hFile, &liFileSize ) && !liFileSize.HighPart && ( ( int ) liFileSize.LowPart > 0 ) )
				{
					uint32 cubData = liFileSize.LowPart;

					SHADigest_t sha;
					GenerateHash( sha, pubData, cubData );

					CRC32_t crc = CRC32_ProcessSingleBuffer( pubData, cubData );
					
#if SELFCHECK_VERBOSE
					wchar_t chMessage[ 1024 ];
					wchar_t chSha[ 64 ];
					for ( int j = 0; j < k_cubHash; ++ j )
						wsprintfW( chSha + 2*j, L"%02X", sha[j] );
					wchar_t chCrc[ 64 ];
					for ( int j = 0; j < sizeof( CRC32_t ); ++j )
						wsprintfW( chCrc + 2 * j, L"%02X", ( (byte*)&crc )[ j ] );
					wsprintfW( chMessage, L"FileSignatures for%s %s computed SHA(%s) CRC(%s)\n", wszShimName ? L" shim" : L"", wszFile, chSha, chCrc );
					OutputDebugStringW( chMessage );
#endif

					if ( pSha )
						memcpy( pSha, sha, k_cubHash );
					if ( pCrc )
						*pCrc = crc;

					bResult = true;
				}

				UnmapViewOfFile( pubData );
			}
			else
			{
#if SELFCHECK_VERBOSE
				wchar_t chMessage[ 1024 ];
				wsprintfW( chMessage, L"FileSignatures for%s %s failed MapViewOfFile 0x%08X\n", wszShimName ? L" shim" : L"", wszFile, GetLastError() );
				SELFCHECK_REPORT( chMessage );
#endif
			}

			CloseHandle( hMap );
		}
		else
		{
#if SELFCHECK_VERBOSE
			wchar_t chMessage[ 1024 ];
			wsprintfW( chMessage, L"FileSignatures for%s %s failed CreateFileMapping 0x%08X\n", wszShimName ? L" shim" : L"", wszFile, GetLastError() );
			SELFCHECK_REPORT( chMessage );
#endif
		}

		CloseHandle( hFile );
	}
	else
	{
#if SELFCHECK_VERBOSE
		wchar_t chMessage[ 1024 ];
		wsprintfW( chMessage, L"FileSignatures for%s %s failed CreateFileW 0x%08X\n", wszShimName ? L" shim" : L"", wszFile, GetLastError() );
		SELFCHECK_REPORT( chMessage );
#endif
	}

	return bResult;
}

DWORD RVAOffsetLookupInSections( IMAGE_NT_HEADERS *pNtHeaders, DWORD dwRVA )
{
	for ( WORD iSection = 0; iSection < pNtHeaders->FileHeader.NumberOfSections; ++iSection )
	{
		IMAGE_SECTION_HEADER *pSection = ( reinterpret_cast< IMAGE_SECTION_HEADER * >( pNtHeaders + 1 ) ) + iSection;
		if ( dwRVA >= pSection->VirtualAddress && dwRVA < pSection->VirtualAddress + pSection->SizeOfRawData )
		{
			return dwRVA - pSection->VirtualAddress + pSection->PointerToRawData;
		}
	}
	return 0;
}

bool BCheckFileLayoutBinary( const wchar_t *wszFile )
{
	bool bResult = false;

	HANDLE hFile = ::CreateFileW( wszFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
#if SELFCHECK_VERBOSE
	const wchar_t *wszShimName = NULL;
#endif
	if ( hFile && ( hFile != INVALID_HANDLE_VALUE ) )
	{
		HANDLE hMap = CreateFileMapping( hFile, NULL, PAGE_READONLY, 0, 0, NULL );
		if ( hMap && ( hMap != INVALID_HANDLE_VALUE ) )
		{
			byte * const pubData = ( byte * ) MapViewOfFile( hMap, FILE_MAP_READ, 0, 0, 0 );
			if ( pubData )
			{
				LARGE_INTEGER liFileSize;
				if ( GetFileSizeEx( hFile, &liFileSize ) && !liFileSize.HighPart && ( ( int ) liFileSize.LowPart > 0 ) )
				{
					uint32 cubData = liFileSize.LowPart;

					PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER) pubData;
					if ( cubData > sizeof( IMAGE_DOS_HEADER ) && dosHeader->e_magic == IMAGE_DOS_SIGNATURE )
					{
						IMAGE_NT_HEADERS *pNtHeaders = ( IMAGE_NT_HEADERS * ) ( pubData + dosHeader->e_lfanew );
						if ( pNtHeaders->Signature == IMAGE_NT_SIGNATURE )
						{
							IMAGE_DATA_DIRECTORY const &idataSection = pNtHeaders->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_IMPORT ];
							bool bFoundBadImport = false;
							if ( idataSection.VirtualAddress )
							{
								if ( DWORD dwOffset = RVAOffsetLookupInSections( pNtHeaders, idataSection.VirtualAddress ) )
								{
									IMAGE_IMPORT_DESCRIPTOR *pImgImportDescriptor = ( IMAGE_IMPORT_DESCRIPTOR * ) ( pubData + dwOffset );
									for ( ; pImgImportDescriptor->Characteristics; ++pImgImportDescriptor )
									{
										char *pchName = ( char * ) pubData + RVAOffsetLookupInSections( pNtHeaders, pImgImportDescriptor->Name );
										if ( !memcmp( pchName, "MSVC", 4 ) )
										{
#if SELFCHECK_VERBOSE
											wchar_t chMessage[ 1024 ];
											wsprintfW( chMessage, L"BCheckFileLayoutBinary for%s %s found a bad import %*S\n", wszShimName ? L" shim" : L"", wszFile, 4, pchName );
											SELFCHECK_REPORT( chMessage );
#endif
											bFoundBadImport = true;
										}
									}
								}
							}
							bResult = !bFoundBadImport;
						}
					}
				}

				UnmapViewOfFile( pubData );
			}
			else
			{
#if SELFCHECK_VERBOSE
				wchar_t chMessage[ 1024 ];
				wsprintfW( chMessage, L"BCheckFileLayoutBinary for%s %s failed MapViewOfFile 0x%08X\n", wszShimName ? L" shim" : L"", wszFile, GetLastError() );
				SELFCHECK_REPORT( chMessage );
#endif
			}

			CloseHandle( hMap );
		}
		else
		{
#if SELFCHECK_VERBOSE
			wchar_t chMessage[ 1024 ];
			wsprintfW( chMessage, L"BCheckFileLayoutBinary for%s %s failed CreateFileMapping 0x%08X\n", wszShimName ? L" shim" : L"", wszFile, GetLastError() );
			SELFCHECK_REPORT( chMessage );
#endif
		}

		CloseHandle( hFile );
	}
	else
	{
#if SELFCHECK_VERBOSE
		wchar_t chMessage[ 1024 ];
		wsprintfW( chMessage, L"BCheckFileLayoutBinary for%s %s failed CreateFileW 0x%08X\n", wszShimName ? L" shim" : L"", wszFile, GetLastError() );
		SELFCHECK_REPORT( chMessage );
#endif
	}

	return bResult;
}

static int g_numModuleSigningEntries = 0;
ValidationFile_t *g_pModuleSigningEntries = NULL;

bool BValidateInstallFileSignature( const wchar_t *wszFile, ValidationFile_t const *pvf = NULL )
{
	if ( !pvf )
	{
		// Find an entry for this file in the signing entries array
		for ( int k = 0; k < g_numModuleSigningEntries; ++k )
		{
			if ( !wcsicmp( wszFile + g_numBaseDirChars, g_pModuleSigningEntries[ k ].m_wchFile ) )
			{
				pvf = g_pModuleSigningEntries + k;
				break;
			}
		}
	}
	
	if ( !pvf )
	{
#if SELFCHECK_VERBOSE
		wchar_t chMessage[ 1024 ];
		wsprintfW( chMessage, L"FileSignatures failed to find manifest entry for %s\n", wszFile );
		SELFCHECK_REPORT( chMessage );
#endif
		return false;
	}

	SHADigest_t sha;
	CRC32_t crc;
	if ( !BComputeFileSignature( wszFile, sha, &crc ) )
	{
#if SELFCHECK_VERBOSE
		wchar_t chMessage[ 1024 ];
		wsprintfW( chMessage, L"FileSignatures failed to compute signatures for %s\n", wszFile );
		SELFCHECK_REPORT( chMessage );
#endif
		return false;
	}

	bool bMatchedPVF = false;
	for ( ;; )
	{	// Check a sequence of signatures if same base file name can be loaded
		// from several locations (grouped together in the manifest)
		bMatchedPVF = ( !memcmp( sha, pvf->m_sha, k_cubHash ) && ( crc == pvf->m_crc ) );
		if ( bMatchedPVF )
			break;
		else if ( pvf->m_bCheckNextAlternativeSignature )
			++ pvf;
		else
			break;
	}

	if ( !bMatchedPVF )
	{
#if SELFCHECK_VERBOSE
		wchar_t chMessage[ 1024 ];
		wchar_t chSha[ 64 ];
		for ( int j = 0; j < k_cubHash; ++j )
			wsprintfW( chSha + 2 * j, L"%02X", sha[ j ] );
		wchar_t chCrc[ 64 ];
		for ( int j = 0; j < sizeof( CRC32_t ); ++j )
			wsprintfW( chCrc + 2 * j, L"%02X", ( ( byte* ) &crc )[ j ] );
		wsprintfW( chMessage, L"FileSignatures for %s computed mismatching SHA(%s) CRC(%s)\n", wszFile, chSha, chCrc );
		SELFCHECK_REPORT( chMessage );
#endif
		return false;
	}

	return true;
}

#define HexDigitFromChar( ch ) byte( ( ( ( ch ) >= '0' ) && ( ( ch ) <= '9' ) ) ? ( ( ch ) - '0' ) : ( ( ( ( ch ) >= 'A' ) && ( ( ch ) <= 'F' ) ) ? ( ( ch ) - 'A' + 10 ) : 0 ) )

typedef LONG ( WINAPI *WinVerifyTrust_t )(HWND hwnd, GUID *pgActionID, LPVOID pWVTData);
WinVerifyTrust_t g_pWinVerifyTrustReal = NULL;

enum EModuleVerificationCallbackError_t
{
	k_EModuleVerificationCallbackError_CannotOpenFile = 1,
	k_EModuleVerificationCallbackError_BadSignature = 2,
	k_EModuleVerificationCallbackError_StrayFile = 3,
};
typedef void ( *PfnModuleVerificationCallback_t )( const wchar_t *pwchFile, EModuleVerificationCallbackError_t eError );

int ComputeAllModuleSignatures( char const *szSigningPrivateKeyFile, PfnModuleVerificationCallback_t pfnCallback )
{
	bool bRecompute = !!szSigningPrivateKeyFile;
	bool bErrorsFound = false;

	ValidationFile_t arrAllFiles[100];
	int numFilesFound = 0;
	byte ubDigest[ 4096 ];
	int numDigestBytes = 0;

	HANDLE hFile = ::CreateFileW( L"csgo.signatures", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
	if ( hFile && ( hFile != INVALID_HANDLE_VALUE ) )
	{
		HANDLE hMap = CreateFileMapping( hFile, NULL, PAGE_READONLY, 0, 0, NULL );
		if ( hMap && ( hMap != INVALID_HANDLE_VALUE ) )
		{
			byte * const pubData = ( byte * ) MapViewOfFile( hMap, FILE_MAP_READ, 0, 0, 0 );
			if ( pubData )
			{
				LARGE_INTEGER liFileSize;
				if ( GetFileSizeEx( hFile, &liFileSize ) && !liFileSize.HighPart && ( ( int ) liFileSize.LowPart > 0 ) )
				{
					// Read the filename
					char const *pchData = ( char const * ) pubData;
					char const * const pchDataEnd = pchData + liFileSize.LowPart;
					while ( pchData < pchDataEnd && *pchData )
					{
						// Is this the "...\" sequence?
						if ( ( pchDataEnd - pchData > 4 ) && !memcmp( pchData, "...\\", 4 ) )
						{
							ValidationFile_t vfile;
							memset( &vfile, 0, sizeof( vfile ) );

							// This denotes a filename
							pchData += 4;
							int k = 0;
							for ( ; k < MAX_PATH - 1 && pchData < pchDataEnd && pchData[0] != 0xA && pchData[0] != 0xD && pchData[0] != '~'; ++ k )
								vfile.m_wchFile[k] = *( pchData ++ );

							bool bExpectMatchingSignature = false;
							if ( pchData[0] == '~' )
							{
								bExpectMatchingSignature = !bRecompute;

								++ pchData;
								for ( int j = 0; j < k_cubHash; ++ j )
								{
									vfile.m_sha[ j ] = ( HexDigitFromChar( pchData[ 0 ] ) << 4 ) | HexDigitFromChar( pchData[ 1 ] );
									pchData += 2;
								}
								for ( int j = 0; j < sizeof( vfile.m_crc ); ++j )
								{
									((byte*)(&vfile.m_crc))[ j ] = ( HexDigitFromChar( pchData[ 0 ] ) << 4 ) | HexDigitFromChar( pchData[ 1 ] );
									pchData += 2;
								}
							}
							
							ValidationFile_t vcheck;
							vfile.m_bComputedSignature = BComputeFileSignature( vfile.m_wchFile, vcheck.m_sha, &vcheck.m_crc );
							if ( !vfile.m_bComputedSignature )
							{
								bErrorsFound = true;
								if ( pfnCallback )
									pfnCallback( vfile.m_wchFile, k_EModuleVerificationCallbackError_CannotOpenFile );
							}
							else if ( bExpectMatchingSignature )
							{
								if ( memcmp( vcheck.m_sha, vfile.m_sha, sizeof( vfile.m_sha ) ) || vcheck.m_crc != vfile.m_crc )
								{
									vfile.m_bMismatchingSignature = true;
									bErrorsFound = true;
									if ( pfnCallback )
										pfnCallback( vfile.m_wchFile, k_EModuleVerificationCallbackError_BadSignature );
								}
							}
							else
							{
								if ( bRecompute )
								{
									//
									// Make sure that modules are digitally signed
									//
									WINTRUST_FILE_INFO FileData;
									memset( &FileData, 0, sizeof( FileData ) );
									FileData.cbStruct = sizeof( WINTRUST_FILE_INFO );
									FileData.pcwszFilePath = vfile.m_wchFile;

									GUID WVTPolicyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
									WINTRUST_DATA WinTrustData;
									memset( &WinTrustData, 0, sizeof( WinTrustData ) );
									WinTrustData.cbStruct = sizeof( WinTrustData );
									WinTrustData.dwUIChoice = WTD_UI_NONE;
									WinTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
									WinTrustData.dwUnionChoice = WTD_CHOICE_FILE;
									WinTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
									WinTrustData.pFile = &FileData;

									LONG lStatus = g_pWinVerifyTrustReal( NULL, &WVTPolicyGUID, &WinTrustData );
									bool bWinVerifyTrustSuccess = ( lStatus == ERROR_SUCCESS );

									WinTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
									lStatus = g_pWinVerifyTrustReal( NULL, &WVTPolicyGUID, &WinTrustData );

									if ( !bWinVerifyTrustSuccess
#ifdef _DEBUG
										&& wcscmp( FileData.pcwszFilePath, L"csgo.exe" )	// debug version of csgo.exe can sign its own module even without digital signature
#endif
										)
									{
										bErrorsFound = true;
#if SELFCHECK_VERBOSE
										wchar_t chMessage[ 1024 ];
										wsprintfW( chMessage, L"WinVerifyTrust for %s failed with status %d\n", FileData.pcwszFilePath, lStatus );
										SELFCHECK_REPORT( chMessage );
#endif
									}

									//
									// Make sure modules are not linking dynamic CRT
									//
									if ( !BCheckFileLayoutBinary( vfile.m_wchFile ) )
										bErrorsFound = true;
								}
								memcpy( vfile.m_sha, vcheck.m_sha, sizeof( vcheck.m_sha ) );
								vfile.m_crc = vcheck.m_crc;
							}

							// Determine the base name of the file
							{
								wchar_t const *pwchFirstChar = wcsrchr( vfile.m_wchFile, '\\' );
								if ( pwchFirstChar ) ++pwchFirstChar;
								else pwchFirstChar = vfile.m_wchFile;
								wchar_t const *pwchEnd = wcschr( pwchFirstChar, '.' );
								if ( !pwchEnd ) pwchEnd = pwchFirstChar + wcslen( pwchFirstChar );
								for ( int k = 0; k < ( pwchEnd - pwchFirstChar ); ++k )
									vfile.m_wchBaseName[ k ] = pwchFirstChar[ k ];
								vfile.m_numBaseNameWChars = pwchEnd - pwchFirstChar;
							}

							// Determine if this is an alternate signature to a previous manifest entry
							if ( ( numFilesFound > 0 ) &&
								( vfile.m_numBaseNameWChars > 0 ) &&
								( vfile.m_numBaseNameWChars == arrAllFiles[ numFilesFound - 1 ].m_numBaseNameWChars ) &&
								( !wcsnicmp( vfile.m_wchBaseName, arrAllFiles[ numFilesFound - 1 ].m_wchBaseName, vfile.m_numBaseNameWChars ) ) )
							{
								arrAllFiles[ numFilesFound - 1 ].m_bCheckNextAlternativeSignature = true;
							}

							arrAllFiles[ numFilesFound ++ ] = vfile;
							
							continue;
						}
						if ( ( pchDataEnd - pchData > 7 ) && !memcmp( pchData, "DIGEST:", 7 ) )
						{
							pchData += 7;

							int k = 0;
							for ( ; k < 4096 && pchData < pchDataEnd && pchData[ 0 ] != 0xA && pchData[ 0 ] != 0xD; ++k )
							{
								ubDigest[ k ] = ( HexDigitFromChar( pchData[ 0 ] ) << 4 ) | HexDigitFromChar( pchData[ 1 ] );
								pchData += 2;
							}
							numDigestBytes = k;
							continue;
						}
						if ( pchData[0] == 0xA || pchData[0] == 0xD )
						{
							pchData ++;
							continue;
						}
						// Bad file
						bErrorsFound = true;
						if ( pfnCallback )
							pfnCallback( L"csgo.signatures", k_EModuleVerificationCallbackError_BadSignature );
						break;
					}
				}
				UnmapViewOfFile( pubData );
			}
			CloseHandle( hMap );
		}
		CloseHandle( hFile );
	}

	if ( !bErrorsFound )
	{
		if ( bRecompute )
		{
			// Compute digest
			numDigestBytes = 0;
			HANDLE hFile = ::CreateFile( szSigningPrivateKeyFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
			if ( hFile && ( hFile != INVALID_HANDLE_VALUE ) )
			{
				HANDLE hMap = CreateFileMapping( hFile, NULL, PAGE_READONLY, 0, 0, NULL );
				if ( hMap && ( hMap != INVALID_HANDLE_VALUE ) )
				{
					byte * const pubData = ( byte * ) MapViewOfFile( hMap, FILE_MAP_READ, 0, 0, 0 );
					if ( pubData )
					{
						LARGE_INTEGER liFileSize;
						if ( GetFileSizeEx( hFile, &liFileSize ) && !liFileSize.HighPart && ( ( int ) liFileSize.LowPart > 0 ) )
						{
							// Read the private key
							extern size_t launcher_keypair_signdata( const byte *pubData, int cbData, const byte *pubPrivateKey, int cbPrivateKey, byte *pbSignatureBuffer );
							numDigestBytes = launcher_keypair_signdata( ( const byte * ) arrAllFiles, numFilesFound * sizeof( ValidationFile_t ), pubData, liFileSize.LowPart, ubDigest );
						}
						UnmapViewOfFile( pubData );
					}
					CloseHandle( hMap );
				}
				CloseHandle( hFile );
			}

			if ( !numDigestBytes )
				bErrorsFound = true;

			// Write the fully signed new file
			if ( !bErrorsFound && numDigestBytes )
			{
				HANDLE hFile;
				hFile = CreateFile( "csgo.signatures", GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, 0, NULL );
				if ( hFile && hFile != INVALID_HANDLE_VALUE )
				{
					bool bOk = true;
					char ch = 0;
					char chTemp[128] = {};
					DWORD dwBytes = 0;
					for ( int iFile = 0; iFile < numFilesFound; ++ iFile )
					{
						ValidationFile_t const &vf = arrAllFiles[iFile];
						bOk = bOk && WriteFile( hFile, "...\\", 4, &dwBytes, NULL );
						for ( int k = 0; k < MAX_PATH && vf.m_wchFile[k]; ++ k )
						{
							ch = vf.m_wchFile[k];
							bOk = bOk && WriteFile( hFile, &ch, 1, &dwBytes, NULL );
						}
						
						ch = '~';
						bOk = bOk && WriteFile( hFile, &ch, 1, &dwBytes, NULL );
						
						for ( int j = 0; j < k_cubHash; ++j )
							sprintf( chTemp + 2 * j, "%02X", vf.m_sha[ j ] );
						bOk = bOk && WriteFile( hFile, chTemp, 2*k_cubHash, &dwBytes, NULL );
						
						for ( int j = 0; j < sizeof( CRC32_t ); ++j )
							sprintf( chTemp + 2 * j, "%02X", ( ( byte* ) &vf.m_crc )[ j ] );
						bOk = bOk && WriteFile( hFile, chTemp, 2 * sizeof( CRC32_t ), &dwBytes, NULL );

						chTemp[0] = 0xD; chTemp[1] = 0xA;
						bOk = bOk && WriteFile( hFile, chTemp, 2, &dwBytes, NULL );
					}

					bOk = bOk && WriteFile( hFile, "DIGEST:", 7, &dwBytes, NULL );

					for ( int j = 0; j < numDigestBytes; ++j )
					{
						sprintf( chTemp, "%02X", ubDigest[ j ] );
						bOk = bOk && WriteFile( hFile, chTemp, 2, &dwBytes, NULL );
					}

					chTemp[ 0 ] = 0xD; chTemp[ 1 ] = 0xA;
					bOk = bOk && WriteFile( hFile, chTemp, 2, &dwBytes, NULL );

					CloseHandle( hFile );

					if ( !bOk )
						bErrorsFound = true;
				}
				else
				{
					bErrorsFound = true;
				}
			}
		}
		else
		{
			// Validate signatures
			if ( numFilesFound <= 0 || numDigestBytes <= 0 )
			{
				bErrorsFound = true;
				if ( pfnCallback )
					pfnCallback( L"csgo.signatures", k_EModuleVerificationCallbackError_CannotOpenFile );
			}

			if ( !bErrorsFound )
			{
				/* removed for partner depot */
			}
		}
	}

	if ( !bErrorsFound )
	{	// When successfully loaded, also set global state for DLL loader hook
		g_pModuleSigningEntries = new ValidationFile_t[ numFilesFound ];
		memcpy( g_pModuleSigningEntries, arrAllFiles, numFilesFound * sizeof( ValidationFile_t ) );
		g_numModuleSigningEntries = numFilesFound;
	}

	return bErrorsFound ? 1 : 0;
}

#if SELFCHECK_HOOK_NTOPENFILE

NTSTATUS NTAPI hookNtOpenFile(
	_Out_ PHANDLE            FileHandle,
	_In_  ACCESS_MASK        DesiredAccess,
	_In_  POBJECT_ATTRIBUTES ObjectAttributes,
	_Out_ PIO_STATUS_BLOCK   IoStatusBlock,
	_In_  ULONG              ShareAccess,
	_In_  ULONG              OpenOptions
	)
{
	if ( !ObjectAttributes->ObjectName->Buffer ) // weird cross pipe read has NULL buffer sometimes??
		return g_pNtOpenFileReal( FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, ShareAccess, OpenOptions );

	if ( DesiredAccess & FILE_EXECUTE )
	{
		//
		// Validate that we are allowed to load that file
		//

		// Determine the base name of the file
		wchar_t const *pwchFirstChar = wcsrchr( ObjectAttributes->ObjectName->Buffer, '\\' );
		if ( pwchFirstChar ) ++pwchFirstChar;
		else pwchFirstChar = ObjectAttributes->ObjectName->Buffer;
		wchar_t const *pwchEnd = wcschr( pwchFirstChar, '.' );
		if ( !pwchEnd ) pwchEnd = pwchFirstChar + wcslen( pwchFirstChar );
		ValidationFile_t const *pvf = NULL;
		for ( int iExisting = 0; iExisting < g_numModuleSigningEntries; ++iExisting )
		{
			if ( ( pwchEnd - pwchFirstChar == g_pModuleSigningEntries[ iExisting ].m_numBaseNameWChars ) &&
				!wcsnicmp( pwchFirstChar, g_pModuleSigningEntries[ iExisting ].m_wchBaseName, g_pModuleSigningEntries[ iExisting ].m_numBaseNameWChars ) )
			{
				pvf = g_pModuleSigningEntries + iExisting;
				break;
			}
		}

		// Now backtrack from first char looking for \\ or for X:
		wchar_t const *pwchFQPath = NULL;
		while ( pwchFirstChar > ObjectAttributes->ObjectName->Buffer )
		{
			if ( pwchFirstChar[ -1 ] == ':' && pwchFirstChar - 1 > ObjectAttributes->ObjectName->Buffer )
			{
				pwchFQPath = pwchFirstChar - 2;
				break;
			}
			else if ( pwchFirstChar[ -1 ] == '\\' && pwchFirstChar - 1 > ObjectAttributes->ObjectName->Buffer && pwchFirstChar[ -2 ] == '\\' )
			{
				pwchFQPath = pwchFirstChar - 2;
				break;
			}
			else
			{
				--pwchFirstChar;
				continue;
			}
		}

		// Check if it is within our executable directory
		bool bOurInstallDir = !wcsnicmp( g_pwchBaseDir, pwchFQPath, g_numBaseDirChars );
		if ( pvf )
		{
			if ( !bOurInstallDir )	// We know about the filename, e.g. "tier0", but loading it from outside of our install dir = NOT ALLOWED
			{
#if SELFCHECK_VERBOSE
				wchar_t chMessage[ 1024 ];
				wsprintfW( chMessage, L"hookNtOpenFile rejecting %s (matches known module %s)\n", ObjectAttributes->ObjectName->Buffer, pvf->m_wchBaseName );
				SELFCHECK_REPORT( chMessage );
#endif
				return NTOPENFILE_STATUS_OBJECT_NAME_NOT_FOUND;
			}
			else
			{
				// Verify file checksum before we allow it to get loaded
				if ( !BValidateInstallFileSignature( ObjectAttributes->ObjectName->Buffer, pvf ) )
				{
#if SELFCHECK_VERBOSE
					wchar_t chMessage[ 1024 ];
					wsprintfW( chMessage, L"hookNtOpenFile rejecting %s (signature mismatch for %s)\n", ObjectAttributes->ObjectName->Buffer, pvf->m_wchBaseName );
					SELFCHECK_REPORT( chMessage );
#endif
					return NTOPENFILE_STATUS_OBJECT_NAME_NOT_FOUND;
				}
			}
		}
		else
		{
			if ( bOurInstallDir )	// We don't know about the filename, e.g. "mytest.dll", but loading it from our install dir = NOT ALLOWED
			{
#if SELFCHECK_VERBOSE
				wchar_t chMessage[ 1024 ];
				wsprintfW( chMessage, L"hookNtOpenFile rejecting %s (unknown module in game dir)\n", ObjectAttributes->ObjectName->Buffer );
				SELFCHECK_REPORT( chMessage );
#endif
				return NTOPENFILE_STATUS_OBJECT_NAME_NOT_FOUND;
			}
			// We don't know about filename, and it is outside of our install dir = ALLOW
		}
	}
	return g_pNtOpenFileReal( FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, ShareAccess, OpenOptions );
}
#endif


static inline wchar_t const * HookRequiredExeFunctions()
{
#if SELFCHECK_HOOK_NTOPENFILE
	HMODULE hNtdll = GetModuleHandle( "ntdll.dll" );
	if ( !hNtdll )
		return g_bRunningPerfectWorld ? L"\x52A0\x8F7D\x7A0B\x5E8F\x8C03\x7528\x5931\x8D25\xFF01"
		: L"Loader call failed!";

	g_pNtOpenFileReal = ( NtOpenFile_t ) HookFunc(
		( BYTE* ) GetProcAddress( hNtdll, "NtOpenFile" ),
		( BYTE* ) hookNtOpenFile );
	if ( !g_pNtOpenFileReal )
		return g_bRunningPerfectWorld ? L"\x5185\x6838\x5F00\x5553\x6587\x4EF6\x8C03\x7528\x5931\x8D25\xFF01"
		: L"Kernel open file call failed!";

#endif

	return NULL; // Everything hooked and no error
}

static inline void UnhookRequiredExeFunctions()
{
	HMODULE hMod;

#if SELFCHECK_HOOK_NTOPENFILE
	hMod = GetModuleHandle( "ntdll.dll" );
	if ( hMod )
	{
		UnhookFunc( ( BYTE* ) GetProcAddress( hMod, "NtOpenFile" ),
			( BYTE* ) g_pNtOpenFileReal );
	}
#endif
}

void ModuleVerificationCallbackFn( const wchar_t *pwchFile, EModuleVerificationCallbackError_t eError )
{
	wchar_t wchMessage[1024];
	switch ( eError )
	{
	case k_EModuleVerificationCallbackError_BadSignature:
		wsprintfW( wchMessage, g_bRunningPerfectWorld ? L"\x4EE5\x4E0B\x6E38\x620F\x6587\x4EF6\x5DF2\x635F\x574F\xFF1A\n%s\n\n\x8BF7\x9A8C\x8BC1\x60A8\x7684\x6E38\x620F\x6587\x4EF6\xFF0C\x5E76\x786E\x4FDD\x60A8\x7684\x6E38\x620F\x6587\x4EF6\x4E0D\x88AB\x9632\x6BD2\x8F6F\x4EF6\x6216\x5176\x4ED6\x8F6F\x4EF6\x963B\x788D\x4E0E\x5E72\x6270\x3002"
			: L"The following game file is corrupt:\n%s\n\nPlease verify your game files, and make sure that your game files are not blocked by antivirus or other software.", pwchFile );
		break;
	case k_EModuleVerificationCallbackError_StrayFile:
		wsprintfW( wchMessage, g_bRunningPerfectWorld ? L"\x5728\x60A8\x7684\x6E38\x620F\x76EE\x5F55\x4E2D\xFF0C\x627E\x5230\x4E00\x90E8\x5206\x975E\x6E38\x620F\x6240\x53D1\x9001\x7684\x6587\x4EF6\xFF1A\n%s\n\n\x6E38\x620F\x76EE\x5F55\x4E2D\x7684\x7B2C\x4E09\x65B9\x6587\x4EF6\x53EF\x80FD\x4F1A\x963B\x788D\x6E38\x620F\x5B89\x5168\x7684\x542F\x52A8\x3002"
			: L"The following file was found in your game directory, but is not distributed as part of the game:\n%s\n\n3rd party files in your game directory may prevent game from starting securely.", pwchFile );
		break;
	default:
		wsprintfW( wchMessage, g_bRunningPerfectWorld ? L"\x65E0\x6CD5\x8BBF\x95EE\x6240\x9700\x7684\x6E38\x620F\x6587\x4EF6\xFF1A\n%s\n\n\x8BF7\x9A8C\x8BC1\x60A8\x7684\x6E38\x620F\x6587\x4EF6\xFF0C\x5E76\x786E\x4FDD\x60A8\x7684\x6E38\x620F\x6587\x4EF6\x4E0D\x88AB\x9632\x6BD2\x8F6F\x4EF6\x6216\x5176\x4ED6\x8F6F\x4EF6\x963B\x788D\x4E0E\x5E72\x6270\x3002"
			: L"Failed to access required game file:\n%s\n\nPlease verify your game files, and make sure that your game files are not blocked by antivirus or other software.", pwchFile );
		break;
	}

	::MessageBoxW( 0, wchMessage, g_bRunningPerfectWorld ? L"CS:GO \x9A8C\x8BC1\x8B66\x544A" : L"CS:GO Validation Warning", MB_OK | MB_ICONEXCLAMATION );
}

void VerifyGameInstall()
{
	if ( ComputeAllModuleSignatures( NULL, ModuleVerificationCallbackFn ) )
	{
		::MessageBoxW( 0, g_bRunningPerfectWorld ? L"\x60A8\x7684\x6E38\x620F\x5B89\x88C5\x51FA\x73B0\x95EE\x9898\x3002\x000A\x9664\x975E\x88AB\x4FEE\x590D\xFF0C\x5426\x5219\x6E38\x620F\x53EF\x80FD\x4EC5\x80FD\x4EE5 -insecure \x6807\x5FD7\x8FD0\x884C\x3002"
			: L"Problems were found with your game installation.\nThe game may only work with the -insecure flag unless repaired.",
			g_bRunningPerfectWorld ? L"CS:GO \x9A8C\x8BC1" : L"CS:GO Validation",
			MB_OK | MB_ICONHAND );
		return;
	}

	// If the process is still running then all the modules that we know about have been verified,
	// Verify stray files
	int numErrorsVerifyFound = 0;
	WIN32_FIND_DATAW fd;
	HANDLE hFind;
	wchar_t wchFilePath[ 2 * MAX_PATH ];
	wsprintfW( wchFilePath, L"%s*.dll", g_pwchBaseDir );
	hFind = FindFirstFileW( wchFilePath, &fd );
	if ( hFind && ( hFind != INVALID_HANDLE_VALUE ) )
	{
		// Should find nothing here!
		do {
			wsprintfW( wchFilePath, L"%s%s", g_pwchBaseDir, fd.cFileName );
			ModuleVerificationCallbackFn( wchFilePath, k_EModuleVerificationCallbackError_StrayFile );
			++ numErrorsVerifyFound;
		} while ( FindNextFileW( hFind, &fd ) );
		FindClose( hFind );
	}
	wsprintfW( wchFilePath, L"%sbin\\*.dll", g_pwchBaseDir );
	hFind = FindFirstFileW( wchFilePath, &fd );
	if ( hFind && ( hFind != INVALID_HANDLE_VALUE ) )
	{
		do {
			bool bFoundMatchingFileSignature = false;
			// Allow a short list of SDK dlls there:
			wchar_t const * arrSdkDlls[] = {
				L"AdminServer.dll", L"bsppack.dll", L"bugreporter_filequeue.dll", L"bugreporter_public.dll", L"crashhandler.dll", L"datacache.dll", L"dedicated.dll", L"dtlibwrapper.dll", L"engine.dll", L"engine_xlsp.dll",
				L"FileSystemOpenDialog.dll", L"FileSystem_Stdio.dll", L"FileSystem_Steam.dll", L"g15.dll", L"hammer_dll.dll", L"hlsl_to_glsl.dll", L"inputsystem.dll", L"launcher.dll", L"libcairo-2.dll", L"libfbxsdk.dll",
				L"libglib-2.0-0.dll", L"libgmodule-2.0-0.dll", L"libgobject-2.0-0.dll", L"libmySQL.dll", L"libmysqlr.dll", L"libpango-1.0-0.dll", L"libpangocairo-1.0-0.dll", L"libpangoft2-1.0-0.dll", L"libpangowin32-1.0-0.dll",
				L"libpng12-0.dll", L"localize.dll", L"MaterialSystem.dll", L"materialsystem2.dll", L"mdllib.dll", L"meshsystem.dll", L"missionchooser.dll", L"Mss32.dll", L"mysql_wrapper.dll", L"networksystem.dll",
				L"p4lib.dll", L"parsifal.dll", L"phonon3d.dll", L"python25v.dll", L"QtAssistantClient4.dll", L"QtAssistantClientd4.dll", L"QtCLucene4.dll", L"QtCLucened4.dll", L"QtCore4.dll", L"QtCored4.dll", L"QtDesigner4.dll",
				L"QtDesignerComponents4.dll", L"QtDesignerComponentsd4.dll", L"QtDesignerd4.dll", L"QtGui4.dll", L"QtGuid4.dll", L"QtHelp4.dll", L"QtHelpd4.dll", L"QtMultimedia4.dll", L"QtMultimediad4.dll", L"QtNetwork4.dll",
				L"QtNetworkd4.dll", L"QtOpenGL4.dll", L"QtOpenGLd4.dll", L"QtScript4.dll", L"QtScriptd4.dll", L"QtScriptTools4.dll", L"QtScriptToolsd4.dll", L"QtSolutions_MFCMigrationFramework-2.8.dll", L"QtSolutions_MFCMigrationFramework-2.8d.dll",
				L"QtSql4.dll", L"QtSqld4.dll", L"QtSvg4.dll", L"QtSvgd4.dll", L"QtTest4.dll", L"QtTestd4.dll", L"QtWebKit4.dll", L"QtWebKitd4.dll", L"QtXml4.dll", L"QtXmld4.dll", L"rendersystemdx11.dll", L"rendersystemdx9.dll", L"rendersystemgl.dll",
				L"resourcesystem.dll", L"scaleformui.dll", L"scenefilecache.dll", L"scenesystem.dll", L"ServerBrowser.dll", L"serverplugin_empty.dll", L"shaderapidx9.dll", L"shaderapiempty.dll", L"shadercompile_dll.dll",
				L"sixense.dll", L"sixense_utils.dll", L"SoundEmitterSystem.dll", L"soundsystem.dll", L"sqlwrapper.dll", L"stdshader_dbg.dll", L"stdshader_dx9.dll", L"Steam.dll", L"steamclient.dll", L"steamclient64.dll",
				L"steam_api.dll", L"steam_api64.dll", L"StudioRender.dll", L"telemetry32.dll", L"telemetry32c.dll", L"telemetry64.dll", L"telemetry64c.dll", L"texturecompile_dll.dll", L"tier0.dll", L"tier0_s.dll",
				L"tier0_s64.dll", L"tier0_s_cygwin.dll", L"unitlib.dll", L"valveaddin.dll", L"valve_avi.dll", L"vaudio_celt.dll", L"vaudio_miles.dll", L"vaudio_speex.dll", L"verifyn.dll", L"vgui2.dll", L"vguimatsurface.dll",
				L"vguirendersurface.dll", L"vjobs.dll", L"vphysics.dll", L"vrad_dll.dll", L"vscript.dll", L"vscript_python.dll", L"vstdlib.dll", L"vstdlib_s.dll", L"vstdlib_s64.dll", L"vtex_dll.dll", L"vvis_dll.dll",
				L"vwatch_service.dll", L"worldrenderer.dll", L"xinput1_3.dll", L"zlib1.dll"
			};
			for ( int kk = 0; kk < Q_ARRAYSIZE( arrSdkDlls ); ++kk )
			{
				if ( !wcsicmp( fd.cFileName, arrSdkDlls[ kk ] ) )
				{
					bFoundMatchingFileSignature = true;
					break;
				}
			}
			if ( !bFoundMatchingFileSignature )
			{
				wsprintfW( wchFilePath, L"%sbin\\%s", g_pwchBaseDir, fd.cFileName );
				ModuleVerificationCallbackFn( wchFilePath, k_EModuleVerificationCallbackError_StrayFile );
				++ numErrorsVerifyFound;
			}
		} while ( FindNextFileW( hFind, &fd ) );
		FindClose( hFind );
	}
	wsprintfW( wchFilePath, L"%scsgo\\bin\\*.dll", g_pwchBaseDir );
	hFind = FindFirstFileW( wchFilePath, &fd );
	if ( hFind && ( hFind != INVALID_HANDLE_VALUE ) )
	{
		do {
			bool bFoundMatchingFileSignature = false;
			if ( !bFoundMatchingFileSignature && !wcsnicmp( fd.cFileName, L"client", 6 ) )
				bFoundMatchingFileSignature = true;
			if ( !bFoundMatchingFileSignature && !wcsnicmp( fd.cFileName, L"server", 6 ) )
				bFoundMatchingFileSignature = true;
			if ( !bFoundMatchingFileSignature && !wcsnicmp( fd.cFileName, L"matchmaking", 11 ) )
				bFoundMatchingFileSignature = true;
			if ( !bFoundMatchingFileSignature )
			{
				wsprintfW( wchFilePath, L"%scsgo\\bin\\%s", g_pwchBaseDir, fd.cFileName );
				ModuleVerificationCallbackFn( wchFilePath, k_EModuleVerificationCallbackError_StrayFile );
				++ numErrorsVerifyFound;
			}
		} while ( FindNextFileW( hFind, &fd ) );
		FindClose( hFind );
	}

	if ( numErrorsVerifyFound > 0 )
		wsprintfW( wchFilePath, g_bRunningPerfectWorld ? L"\x6E38\x620F\x9A8C\x8BC1\x5B8C\x6210\x4F46\x5E26\x6709 %u \x5219\x8B66\x544A\x3002\n9664\x975E\x88AB\x4FEE\x590D\xFF0C\x5426\x5219\x6E38\x620F\x53EF\x80FD\x4EC5\x80FD\x4EE5 -insecure \x6807\x5FD7\x8FD0\x884C\x3002"
			: L"Game validation completed with %u warning(s).\nThe game may work with the -insecure flag unless repaired."
			, numErrorsVerifyFound );
	else
		wsprintfW( wchFilePath, g_bRunningPerfectWorld ? L"\x6E38\x620F\x9A8C\x8BC1\x6210\x529F\x5730\x5B8C\x6210\x3002\x000A\x82E5\x6E38\x620F\x4ECD\x7136\x65E0\x6CD5\x5B89\x5168\x8FD0\x884C\xFF0C\x60A8\x53EF\x4EE5\x5C1D\x8BD5\x4F7F\x7528 -insecure \x6807\x5FD7\x542F\x52A8\x8FD0\x884C\x3002"
			: L"Game validation completed without errors.\nIf the game still fails to run securely, you may try launching with the -insecure flag." );
	::MessageBoxW( 0, wchFilePath, g_bRunningPerfectWorld ? L"CS:GO \x9A8C\x8BC1" : L"CS:GO Validation", MB_OK );
}

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
#ifdef _DEBUG
	if ( strstr( lpCmdLine, "-csgo_crypto_keypair_generate" ) )
	{
		void debug_launcher_keypair_generate();
		debug_launcher_keypair_generate();
		return 0;
	}
#endif

	// Code building file signatures here
	if ( char *szSigningKey = strstr( lpCmdLine, "-csgo_checksum_and_sign_modules " ) )
	{
		HMODULE hWinTrust = LoadLibrary( "wintrust.dll" );
		if ( !hWinTrust ) return -2;
		g_pWinVerifyTrustReal = (WinVerifyTrust_t) GetProcAddress( hWinTrust, "WinVerifyTrust" );
		if ( !g_pWinVerifyTrustReal ) return -3;
		return ComputeAllModuleSignatures( szSigningKey + strlen( "-csgo_checksum_and_sign_modules " ), NULL );
	}

	if ( strstr( lpCmdLine, "-perfectworld" ) )
		g_bRunningPerfectWorld = true;

	// Use the .EXE name to determine the root directory (Unicode only!)
	wchar_t wchModuleName[ MAX_PATH ] = {};
	DWORD dwModuleFileNameResult = GetModuleFileNameW( hInstance, wchModuleName, MAX_PATH );
	if ( !dwModuleFileNameResult || dwModuleFileNameResult >= MAX_PATH - 1 || !wchModuleName[0] )
	{
		MessageBoxW( 0,
			g_bRunningPerfectWorld ? L"\x65E0\x6CD5\x786E\x8BA4\x6A21\x5757\x6587\x4EF6\x540D"
				: L"Failed to determine module file name",
			g_bRunningPerfectWorld ? L"\x542F\x52A8\x5668\x9519\x8BEF"
				: L"Launcher Error",
			MB_OK );
		return -5;
	}
	if ( wchar_t *pwchSlash = wcsrchr( wchModuleName, '\\' ) )
	{
		pwchSlash[1] = 0;
		g_pwchBaseDir = wchModuleName;
		g_numBaseDirChars = pwchSlash - wchModuleName + 1;
	}
	else
	{
		MessageBoxW( 0,
			g_bRunningPerfectWorld ? L"\x65E0\x6CD5\x89E3\x6790\x6A21\x5757\x6587\x4EF6\x540D"
				: L"Failed to parse module file name",
			g_bRunningPerfectWorld ? L"\x542F\x52A8\x5668\x9519\x8BEF"
				: L"Launcher Error",
			MB_OK );
		return -6;
	}

	// Validate that the executable install dir is not Unicode
	for ( wchar_t *pwchCheck = wchModuleName; *pwchCheck; ++ pwchCheck )
	{
		if ( ( ( *pwchCheck ) & 0x7F ) != *pwchCheck )
		{
			pwchCheck[1] = '.';
			pwchCheck[2] = '.';
			pwchCheck[3] = '.';
			pwchCheck[4] = 0;

			wchar_t wchErrorMessage[1024] = {};
			wsprintfW( wchErrorMessage,
				g_bRunningPerfectWorld ? L"\x76EE\x5F55\x8DEF\x5F84\x4E0D\x652F\x6301 Unicode\x3002\n\n\x9519\x8BEF 0x%04X \x4E8E\xFF1A\n%s\n\n\x8BF7\x5C06\x6E38\x620F\x5B89\x88C5\x5728\x4EC5\x542B\x62C9\x4E01\x5B57\x6BCD\x7684\x76EE\x5F55\x8DEF\x5F84\x4E0B\x3002"
					: L"Unicode directory path not supported.\n\nError 0x%04X at:\n%s\n\nPlease install game under directory path containing only Latin letters.",
				( unsigned int )( *pwchCheck ), g_pwchBaseDir );
			MessageBoxW( 0, wchErrorMessage, g_bRunningPerfectWorld ? L"\x542F\x52A8\x5668\x9519\x8BEF"
				: L"Launcher Error", MB_OK );
			return -7;
		}
	}

	// Switch to the directory where the executable is running
	_wchdir( g_pwchBaseDir );

	// See if we are requested to verify the install?
	if ( strstr( lpCmdLine, "-validate" ) )
	{
		VerifyGameInstall();
		return 0;
	}

	// If we are not generating checksums then go ahead and verify modules signatures upfront
	bool bSecure = true;
	if ( strstr( lpCmdLine, "-insecure" ) || ComputeAllModuleSignatures( NULL, NULL ) || !g_numModuleSigningEntries )
	{
		bSecure = false;
	}

	// Hook all required functions
	wchar_t const *szHookingError = bSecure ? HookRequiredExeFunctions() : NULL;
	if ( szHookingError )
	{
		MessageBoxW( 0, szHookingError, g_bRunningPerfectWorld ? L"\x542F\x52A8\x5668\x9519\x8BEF"
			: L"Launcher Error", MB_OK | MB_ICONHAND );
		UnhookRequiredExeFunctions();
		return 0;
	}

	// Assemble the full Unicode path to our "launcher.dll"
	wchar_t wszLauncherDllPath[ MAX_PATH + 64 ];
	wsprintfW( wszLauncherDllPath, L"%sbin%s\\launcher.dll", g_pwchBaseDir,
#ifdef _WIN64
		L"\\x64"
#else
		L""
#endif
		);
	HINSTANCE launcher = LoadLibraryExW( wszLauncherDllPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );
	if ( !launcher )
	{
		wchar_t *pszError;
		FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&pszError, 0, NULL);

		wchar_t szBuf[1024];
		wsprintfW(szBuf, g_bRunningPerfectWorld ? L"\x65E0\x6CD5\x52A0\x8F7D\x542F\x52A8\x5668 DLL:\n\n%s"
			: L"Failed to load the launcher DLL:\n\n%s", pszError);
		szBuf[sizeof( szBuf ) - 1] = '\0';
		MessageBoxW( 0, szBuf,
			g_bRunningPerfectWorld ? L"\x542F\x52A8\x5668\x9519\x8BEF"
			: L"Launcher Error", MB_OK );

		LocalFree(pszError);
		if ( bSecure )
			UnhookRequiredExeFunctions();
		return 0;
	}

	LauncherMain_t main = (LauncherMain_t)GetProcAddress( launcher, "LauncherMain" );
	int iResult = main( bSecure ? 1 : 0, hInstance, hPrevInstance, lpCmdLine, nCmdShow );
	if ( bSecure )
		UnhookRequiredExeFunctions();

	return iResult;
}

#elif defined( SN_TARGET_PS3 )

#if defined( __GCC__ )
#define COMPILER_GCC
#elif defined( __SNC__ )
#define COMPILER_SNC
#endif
#include "../public/tls_ps3.h"
#include "sys/process.h"

// We need to avoid printf before we
// configure our custom memory allocator
#define printf(...) ((void)0)
#include "../common/ps3/ps3_helpers.h"

#ifdef APPCHANGELISTVERSION
// write the changelist number into the executable so that the GUID changes between builds. 
// previously, setting the version number via the SYS_MODULE_INFO was good enough to do
// this, but not after sdk 350. 
volatile unsigned int clnumber = APPCHANGELISTVERSION;
__attribute__ ((noinline)) void DummyFuncForUpdatingGUIDs( char *pOut )
{
	sprintf( pOut, "%x", clnumber );
}
#else // absent appchangelistversion, invent one
volatile unsigned int clnumber = 0;
#define DUMMY_VER_STRING __DATE__ " " __TIME__
volatile char dummyVersionDateString[] = DUMMY_VER_STRING "\n";
__attribute__ ((noinline)) void DummyFuncForUpdatingGUIDs( char *pOut )
{
	sprintf( pOut, DUMMY_VER_STRING );
}
#undef DUMMY_VER_STRING
#endif

// 1 Mb stack is maximum allowed for the main thread
// and we will make good use of it
SYS_PROCESS_PARAM( 1000, 1 * 1024 * 1024 )

/////////////////////////////////////////////////////////////////////////////////////////////////////
// All thread-local storage must reside in the ELF and be exported for PRXes to use it
/////////////////////////////////////////////////////////////////////////////////////////////////////
__thread TLSGlobals gTLSGlobals =
{
	// TLS values/flags
	/*nThreadLocalStateIndex*/		0,
	/*TLSValues*/					{ NULL },
	/*TLSFlags*/					{ false },
	/*bWaitObjectsCreated*/			false,
	/*WaitObjectsSemaphore*/		0,
	/*pCurThread*/					NULL,
	/*nThreadID*/					0,
	
	// Engine TLS data (zip/console/splitslot)
	/*uiEngineZipLastErrorZ*/		0,
	/*bEngineConsoleIsInSpew*/		false,
	/*pEngineSplitSlot*/			NULL,

	// Malloc debugging TLS data
	/*pMallocDbgInfoStack*/			NULL,
	/*nMallocDbgInfoStackDepth*/	0,

	// Filesystem read filename buffer
	/*pFileSystemReadFilename*/		NULL,

	// Material system render context
	/*pMaterialSystemRenderContext*/ NULL,

	// Physics virtual mesh frame locks
	/*pPhysicsVirtualMeshFrameLocks*/ NULL,
	
	/*bNormalQuitRequested*/ false
};
TLSGlobals *GetTLSGlobals_ELF() { return &gTLSGlobals; }

extern CPs3ContentPathInfo g_Ps3GameDataPathInfo;

template< typename BaseStruct >
struct PS3MainParameters : public BaseStruct
{
	PS3MainParameters() { memset( this, 0, sizeof( BaseStruct ) ); BaseStruct::cbSize = sizeof( BaseStruct ); }
};

struct PS3_Launch_t
{
	explicit PS3_Launch_t( char const *szPrxName, PS3_PrxLoadParametersBase_t *pParams ) :
		m_szPrxName( szPrxName ), m_pPrxParams( pParams )
	{
		m_iResult = PS3_PrxLoad( m_szPrxName, m_pPrxParams );
		if ( m_iResult < CELL_OK ) 
		{
			printf( "ERROR: %s PRX load failed: 0x%08x\n", m_szPrxName, m_iResult );
		}
		else
		{
			printf( "Loaded: %s (0x%08x)\n", m_szPrxName, m_iResult );
		}
	}

	char const *m_szPrxName;
	PS3_PrxLoadParametersBase_t *m_pPrxParams;
	int m_iResult;
};

static const char *LauncherMainSPRXPath( const char *modulename, char *buf, int buflen = CELL_GAME_PATH_MAX ) // formats a path to the module. returns a pointer to the buf param for convenience. 
{
	snprintf( buf, buflen, "%s/%s" DLL_EXT_STRING, g_Ps3GameDataPathInfo.PrxPath(), modulename  );
	return buf;
}

#if defined( SN_TARGET_PS3 )
void TunerMarkerPush( const char * pName )
{
//#if defined( VPROF_SN_LEVEL )
	snPushMarker( pName );
//#endif
}

void TunerMarkerPop()
{
//#if defined( VPROF_SN_LEVEL )
	snPopMarker();
//#endif
}

// this is debug-only counter; never use it for anything other than debugging!
uint64_t g_nDebugSwapBufferCount = 0; 
void TunerSwapBufferMarker()
{
	// this dummy function is only required as a patch-through for Tuner that attaches to the game after the game has been started, as a convenience funciton/
	// it must be called at every frame boundary (presumably at/after psglSwap )
	g_nDebugSwapBufferCount++;
}
#endif

PS3_PrxModuleEntry_t *g_pPrxModulesList = NULL;
PS3_PrxModuleEntry_t ** PS3_PrxGetModulesList() { return &g_pPrxModulesList; }


void TestThreadProc( uint64_t id )
{
	printf( "Hello from PPU thread %lld\n", id );
	sys_ppu_thread_exit( id );
}

void TestThreads( int nLevel = 0)
{
	printf("testing threads\n");
	const int numThreads = 20;
	sys_ppu_thread_t id[numThreads];
	for( int i = 0;i < numThreads; ++i )
	{
		if( nLevel > 0 )
			TestThreads( nLevel - 1 );
		if( CELL_OK != sys_ppu_thread_create( &id[i], TestThreadProc, i, 1001, 64*1024, SYS_PPU_THREAD_CREATE_JOINABLE, "SimpleThread" ) )
		{
			printf("ERROR: cannot create thread\n");
			return;
		}
	}
	
	for( int i = 0;i < numThreads; ++i )
	{
		uint64_t res;
		sys_ppu_thread_join( id[i], &res );
		if( res != i )
		{
			printf("ERROR: invalid thread return value\n");
			return;
		}
	}
}

int MainImpl( int argc, char *argv[] )
{
// #ifdef _CERT // possibly enable it for ship to disable command line cheating?
#if 0
	// Disable command line support for shipping unless -certcmdline specified
	if ( ( argc > 1 ) && !strcmp( argv[1], "-certcmdline" ) )
	{
		argc = 1;
	}
#endif

	// this is the very first timing message, before tier0 is even initialized and we can use any 
	// logging or timing facilities; this is the baseline to measure loading times
	cell::fios::abstime_t fiosLaunchTime = cell::fios::FIOSGetCurrentTime();
	
#ifndef _CERT
	{
		double flTime = cell::fios::FIOSAbstimeToMicroseconds( fiosLaunchTime ) * 1e-6;
		char buffer[4096];
		int nMessageSize = snprintf(buffer, sizeof(buffer), "--------------------------------------\n 0.0000 / %8.4f : launcher_main(", flTime );
		for( int i = 0;i < argc && nMessageSize < sizeof(buffer)-4; ++i )
		{
			// add delimiters
			if( i > 0 )
				buffer[nMessageSize++] = ',';
			nMessageSize += snprintf( buffer + nMessageSize, sizeof(buffer) - nMessageSize, " \"%s\"", argv[i] );
		}																				    
		nMessageSize += snprintf(buffer + nMessageSize, sizeof(buffer) - nMessageSize, " )\n" );
		unsigned wrote;
		sys_tty_write( SYS_TTYP6, buffer, nMessageSize, &wrote );
	}
#endif

	// this is a dummy operation to force the compiler to not elide the
	// changelist GUID. It's really necessary, because otherwise the compiler
	// will notice the function isn't called, and elide it, and the GUID string,
	// altogether. Because the compiler "can't" know what argc will be, it has
	// to compile in the function call here. This is the only way to guarantee
	// that different builds will have different GUIDs, because we don't often
	// change launcher_main between versions, and the PRXes don't get individual
	// GUIDs in the dump. 
	// Don't pass in more than one million commandline 
	// parameters or this will corrupt the one millionth.
	if ( argc > 100000000 )
	{
		DummyFuncForUpdatingGUIDs( argv[100000000] );
	}



	char path[CELL_GAME_PATH_MAX];
	bool bDevHddCfgOnly = false;
	bool bRunLauncherMain = true;
	bool bSupportPathLegacyArgs = true;
	bool bEnableMlaa = true;

#ifdef HDD_BOOT
	unsigned int uiInitFlags = CPs3ContentPathInfo::INIT_PRX_ON_HDD | CPs3ContentPathInfo::INIT_IMAGE_ON_HDD;
#else
	unsigned int uiInitFlags = CPs3ContentPathInfo::INIT_RETAIL_MODE;		// assume that if no arguments are specified we are going to run in retail mode
#endif


	// uncomment the following in order to allow starting game in /app_home from XMB (shortcutting creating disk image)
	// uiInitFlags = CPs3ContentPathInfo::INIT_PRX_APP_HOME | CPs3ContentPathInfo::INIT_IMAGE_APP_HOME;

	for ( int k = 0; k < argc; ++ k )
	{
		if( !strcmp( "-noMlaa", argv[k] ) )
		{
			bEnableMlaa = false;
		}
		else if ( !strcmp( "-errorrenderloop", argv[k] ) )
		{
			return -1;
		}
		else if ( !strcmp( "-devhddcfgonly", argv[k] ) )
		{
			bDevHddCfgOnly = true;
		}
		else if ( !strcmp( "-nolaunchermain", argv[k] ) )
		{
			bRunLauncherMain = false;
		}
		else if ( !strcmp( "-syscacheclear", argv[k] ) )
		{
			uiInitFlags |= CPs3ContentPathInfo::INIT_SYS_CACHE_CLEAR;
		}
		else if ( !strncmp( "-path_retail", argv[k], 12 ) )
		{
			bSupportPathLegacyArgs = false;
			if ( argv[k][12] )
				uiInitFlags &=~CPs3ContentPathInfo::INIT_RETAIL_MODE;
		}
		else if ( !strncmp( "-path_prx_", argv[k], 10 ) )
		{
			char chPrxPathMode = argv[k][10];
			switch ( chPrxPathMode )
			{
			case 'h': uiInitFlags |= CPs3ContentPathInfo::INIT_PRX_ON_HDD; break;
			case 'b': uiInitFlags |= CPs3ContentPathInfo::INIT_PRX_ON_BDVD; break;
			case 'a': uiInitFlags |= CPs3ContentPathInfo::INIT_PRX_APP_HOME; break;
			}
		}
		else if ( !strncmp( "-path_img_", argv[k], 10 ) )
		{
			char chPrxPathMode = argv[k][10];
			switch ( chPrxPathMode )
			{
			case 'h': uiInitFlags |= CPs3ContentPathInfo::INIT_IMAGE_ON_HDD; break;
			case 'b': uiInitFlags |= CPs3ContentPathInfo::INIT_IMAGE_ON_BDVD; break;
			case 'a': uiInitFlags |= CPs3ContentPathInfo::INIT_IMAGE_APP_HOME; break;
			}
		}
				// LEGACY PARAMETERS because people are used to running with them (need to clean up some time later)
				else if ( bSupportPathLegacyArgs && !strcmp( "-dev", argv[k] ) )
				{
					uiInitFlags &=~CPs3ContentPathInfo::INIT_RETAIL_MODE;
					uiInitFlags |= CPs3ContentPathInfo::INIT_IMAGE_APP_HOME;
					uiInitFlags |= CPs3ContentPathInfo::INIT_PRX_APP_HOME;
				}
				else if ( bSupportPathLegacyArgs && !strcmp( "-ps3hd", argv[k] ) )
				{
					uiInitFlags &=~CPs3ContentPathInfo::INIT_RETAIL_MODE;
					uiInitFlags |= CPs3ContentPathInfo::INIT_IMAGE_ON_HDD;
					uiInitFlags |= CPs3ContentPathInfo::INIT_PRX_APP_HOME;
				}
				else if ( bSupportPathLegacyArgs && !strcmp( "-nops3hd", argv[k] ) )
				{
					uiInitFlags &=~CPs3ContentPathInfo::INIT_RETAIL_MODE;
					uiInitFlags |= CPs3ContentPathInfo::INIT_IMAGE_APP_HOME;
					uiInitFlags |= CPs3ContentPathInfo::INIT_PRX_APP_HOME;
				}
				else if ( bSupportPathLegacyArgs && !strcmp( "-dev_bdvd", argv[k] ) )
				{
					uiInitFlags &=~CPs3ContentPathInfo::INIT_RETAIL_MODE;
					uiInitFlags |= CPs3ContentPathInfo::INIT_IMAGE_ON_BDVD;
					uiInitFlags |= CPs3ContentPathInfo::INIT_PRX_APP_HOME;
				}
				// END LEGACY PARAMETERS
	}

	// uncomment the following to hard code for Eurogamer (as if running -dev)
#ifdef _PS3
//	uiInitFlags &=~CPs3ContentPathInfo::INIT_RETAIL_MODE;
#endif

	int iPathInfoInitResult = g_Ps3GameDataPathInfo.Init( uiInitFlags );
	if ( iPathInfoInitResult < 0 )
		return iPathInfoInitResult;

	if ( bDevHddCfgOnly )
		return 0;

	PS3MainParameters< PS3_LoadTier0_Parameters_t > tier0;
	tier0.pfnGetTlsGlobals = GetTLSGlobals_ELF;
	tier0.pPS3PathInfo = &g_Ps3GameDataPathInfo;
	tier0.fiosLaunchTime = fiosLaunchTime;
	tier0.nCLNumber = clnumber;
	tier0.pfnPushMarker = TunerMarkerPush;
	tier0.pfnPopMarker = TunerMarkerPop;
	tier0.pfnSwapBufferMarker = TunerSwapBufferMarker;
	tier0.ppPrxModulesList = PS3_PrxGetModulesList();
	tier0.m_pGcmSharedData = &g_gcmSharedData;
	

#ifndef _CERT

	tier0.snRawSPULockHandler = snRawSPULockHandler;
	tier0.snRawSPUUnlockHandler = snRawSPUUnlockHandler;
	tier0.snRawSPUNotifyCreation = snRawSPUNotifyCreation;
	tier0.snRawSPUNotifyDestruction = snRawSPUNotifyDestruction;
	tier0.snRawSPUNotifyElfLoad = snRawSPUNotifyElfLoad;
	tier0.snRawSPUNotifyElfLoadNoWait = snRawSPUNotifyElfLoadNoWait;
	tier0.snRawSPUNotifyElfLoadAbs = snRawSPUNotifyElfLoadAbs;
	tier0.snRawSPUNotifyElfLoadAbsNoWait = snRawSPUNotifyElfLoadAbsNoWait;
	tier0.snRawSPUNotifySPUStopped = snRawSPUNotifySPUStopped;
	tier0.snRawSPUNotifySPUStarted = snRawSPUNotifySPUStarted;

#endif

	(void)bEnableMlaa; // we'll use it if we need to init GCM and start rendering right away
/*
	g_gcmSharedData.m_nIoMemorySize = bEnableMlaa ? 5 * 1024 * 1024 : 1 * 1024 * 1024;
	sys_addr_t pIoAddress = NULL;
	int nError = sys_memory_allocate( g_gcmSharedData.m_nIoMemorySize, SYS_MEMORY_PAGE_SIZE_1M, &pIoAddress );
	if( CELL_OK != nError || !pIoAddress )
	{
		// cannot allocate IO memory
		return -2;
	}
	
	int32 result = cellGcmInit( m_nCmdSize, m_nIoSize, m_pIoAddress );
	if ( result < CELL_OK )
		return result;

	g_gcmSharedData.m_pIoMemory = ( void* )pIoAddress;
*/
	
	PS3_Launch_t tier0Launch( LauncherMainSPRXPath( "tier0", path ), &tier0 );
	if( tier0Launch.m_iResult < CELL_OK )
	{
		return -1;
	}

	int iAppRetCode = 0;
	PS3MainParameters< PS3_PrxLoadParametersBase_t > vstdlib;
	PS3_Launch_t vstdlibLaunch( LauncherMainSPRXPath( "vstdlib", path ), &vstdlib );
	if( vstdlibLaunch.m_iResult >= CELL_OK )
	{
		
	#ifndef NO_STEAM
		PS3MainParameters< PS3_PrxLoadParametersBase_t > steamapi;
		PS3_Launch_t steamapiLaunch( LauncherMainSPRXPath( "steam_api", path ), &steamapi );
		if ( steamapiLaunch.m_iResult >= CELL_OK )
		{
	#endif
			PS3MainParameters< PS3_LoadLauncher_Parameters_t > launcher;
			PS3_Launch_t launcherLaunch( LauncherMainSPRXPath( "launcher", path ), &launcher );

			if ( launcher.pfnLauncherMain )
			{
				printf( "Launching...\n" );
				iAppRetCode = bRunLauncherMain ? (*launcher.pfnLauncherMain)( argc, argv ) : 0;

				printf( "Shutting down...\n" );
				launcher.pfnLauncherShutdown();
			}
			else
			{
				printf( "ERROR: failed to obtain LauncherMain entry point!\n" );
			}
			PS3_PrxUnload( launcher.sysPrxId );
		
	#ifndef NO_STEAM
			PS3_PrxUnload( steamapi.sysPrxId );
		}
	#endif

		PS3_PrxUnload( vstdlib.sysPrxId );
	}

	// Before tier0 unloads make sure that there are no modules remaining loaded
	#if !defined( _CERT )
	for ( PS3_PrxModuleEntry_t *pEntry = *PS3_PrxGetModulesList(); pEntry; pEntry = pEntry->pNextModule )
	{
		if ( strstr( pEntry->chName, "/tier0" DLL_EXT_STRING ) )
			continue;

		unsigned int dummy;
		char const *szWarnMsg = "EXITING WITH PRX MODULE: ";
		sys_tty_write( SYS_TTYP6, szWarnMsg, strlen( szWarnMsg ), &dummy );
		
		sys_tty_write( SYS_TTYP6, pEntry->chName, strlen( pEntry->chName ), &dummy );
		
		szWarnMsg = "\n";
		sys_tty_write( SYS_TTYP6, szWarnMsg, strlen( szWarnMsg ), &dummy );
	}
	#endif

	tier0.pfnTier0Shutdown();
	PS3_PrxUnload( tier0.sysPrxId );

	return iAppRetCode;
}


int main( int argc, char *argv[] )
{

	// Init LibSn
#ifndef _CERT
	snInit();
#endif

	int nReturn = MainImpl( argc, argv );

#ifndef _PS3

// 	if( !gTLSGlobals.bNormalQuitRequested )
// 	{
// 		printf("no normal quit requested, starting error render loop\n");
// 		ErrorRenderLoop loop;
// 		loop.Run();
// 		printf("Error render loop finished\n");
// 	}

#endif


#if !defined( _CERT )
	if ( 1 )
	{
		unsigned int dummy;
		char const *szWarnMsg =
			(*PS3_PrxGetModulesList())
			? "------- WARNING: RETURNING FROM MAIN WITH PRX MODULES RUNNING --------\n"
			: "--------------------------------BYE-----------------------------------\n";
		sys_tty_write( SYS_TTYP6, szWarnMsg, strlen( szWarnMsg ), &dummy );
	}
#endif

	return nReturn;
}


#elif defined (POSIX)

int main( int argc, char *argv[] )
{
#ifdef PLATFORM_64BITS
	#ifdef OSX
		const char *pLauncherPath = "bin/osx64/launcher" DLL_EXT_STRING;
	#else
		const char *pLauncherPath = "bin/linux64/launcher" DLL_EXT_STRING;
	#endif
#else
	const char *pLauncherPath = "bin/launcher" DLL_EXT_STRING;
#endif

	void *launcher = dlopen( pLauncherPath, RTLD_NOW );
	
	if ( !launcher )
	{
		printf( "Failed to load the launcher (%s)\n", dlerror() );
		while(1);
		return 0;
	}
	
	LauncherMain_t main = (LauncherMain_t)dlsym( launcher, "LauncherMain" );
	if ( !main )
	{
		printf( "Failed to load the launcher entry proc\n" );
		while(1);
		return 0;
	}

	return main( argc, argv );
}

#else
#error
#endif // WIN32 || POSIX


#else // X360
//-----------------------------------------------------------------------------
// 360 Quick and dirty command line parsing. Returns true if key found,
// false otherwise. Caller can optionally get next argument.
//-----------------------------------------------------------------------------
bool ParseCommandLineArg( const char *pCmdLine, const char* pKey, char* pValueBuff = NULL, int valueBuffSize = 0 )
{
	int keyLen = (int)strlen( pKey );
	const char* pArg = pCmdLine;
	for ( ;; )
	{
		// scan for match
		pArg = strstr( (char*)pArg, pKey );
		if ( !pArg )
		{
			return false;
		}
		
		// found, but could be a substring
		if ( pArg[keyLen] == '\0' || pArg[keyLen] == ' ' )
		{
			// exact match
			break;
		}

		pArg += keyLen;
	}

	if ( pValueBuff )
	{
		// caller wants next token
		// skip past key and whitespace
		pArg += keyLen;
		while ( *pArg == ' ' )
		{
			pArg++;
		}

		int i;
		for ( i=0; i<valueBuffSize; i++ )
		{
			pValueBuff[i] = *pArg;
			if ( *pArg == '\0' || *pArg == ' ' )
				break;
			pArg++;
		}
		pValueBuff[i] = '\0';
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// 360 Quick and dirty command line arg stripping.
//-----------------------------------------------------------------------------
void StripCommandLineArg( const char *pCmdLine, char *pNewCmdLine, const char *pStripArg )
{
	// cannot operate in place
	assert( pCmdLine != pNewCmdLine );

	int numTotal = strlen( pCmdLine ) + 1;
	const char* pArg = strstr( pCmdLine, pStripArg );
	if ( !pArg )
	{
		strcpy( pNewCmdLine, pCmdLine );
		return;
	}

	int numDiscard = strlen( pStripArg );
	while ( pArg[numDiscard] && ( pArg[numDiscard] != '-' && pArg[numDiscard] != '+' ) )
	{
		// eat whitespace up to the next argument
		numDiscard++;
	}

	memcpy( pNewCmdLine, pCmdLine, pArg - pCmdLine );
	memcpy( pNewCmdLine + ( pArg - pCmdLine ), (void*)&pArg[numDiscard], numTotal - ( pArg + numDiscard - pCmdLine  ) );

	// ensure we don't leave any trailing whitespace, occurs if last arg is stripped
	int len = strlen( pNewCmdLine );
	while ( len > 0 &&  pNewCmdLine[len-1] == ' ' )
	{
		len--;
	}
	pNewCmdLine[len] = '\0';
}

//-----------------------------------------------------------------------------
// 360 Conditional spew
//-----------------------------------------------------------------------------
void Spew( const char *pFormat, ... )
{
#if defined( _DEBUG )
	char	msg[2048];
	va_list	argptr;

	va_start( argptr, pFormat );
	vsprintf( msg, pFormat, argptr );
	va_end( argptr );

	OutputDebugString( msg );
#endif
}

//-----------------------------------------------------------------------------
// Get the new entry point and command line
//-----------------------------------------------------------------------------
LauncherMain_t GetLaunchEntryPoint( char *pNewCommandLine )
{
	HMODULE		hModule;
	char		*pCmdLine;

	// determine source of our invocation, internal or external
	// a valid launch payload will have an embedded command line
	// command line could be from internal restart in dev or retail mode
	CXboxLaunch xboxLaunch;
	int payloadSize;
	unsigned int launchID;
	char *pPayload;
	bool bInternalRestart = xboxLaunch.GetLaunchData( &launchID, (void**)&pPayload, &payloadSize );
	if ( !bInternalRestart || !payloadSize || launchID != VALVE_LAUNCH_ID )
	{
		// could be first time, get command line from system
		pCmdLine = GetCommandLine();
		if ( !stricmp( pCmdLine, "\"default.xex\"" ) )
		{
			// matches retail xex and no arguments, mut be first time retail launch
			pCmdLine = "default.xex";
#if defined( _MEMTEST )
			pCmdLine = "default.xex +mat_picmip 2";
#endif
		}
	}
	else
	{
		// get embedded command line from payload
		pCmdLine = pPayload;
	}

	int launchFlags = 0;
	if ( launchID == VALVE_LAUNCH_ID )
	{
		launchFlags = xboxLaunch.GetLaunchFlags();
	}
#if !defined( _CERT )
	if ( launchFlags & LF_ISDEBUGGING )
	{
		while ( !DmIsDebuggerPresent() )
		{
		}

		Sleep( 1000 );
		Spew( "Resuming debug session.\n" );
	}
#endif

	// unforunately, the xbox erases its internal store upon first fetch
	// must re-establish it so the payload that contains other data (past command line) can be accessed by the game
	// the launch data will be owned by tier0 and supplied to game
	if ( launchID == VALVE_LAUNCH_ID )
	{
		xboxLaunch.SetLaunchData( pPayload, payloadSize, launchFlags );
	}
#if defined( _DEMO )
	else if ( pPayload && payloadSize )
	{
		// not our data
		// restore the launch data as expected
		xboxLaunch.SetLaunchData( pPayload, payloadSize, LF_UNKNOWNDATA );
	}
#endif

#if defined( _DEMO )
	// the demo version cannot trust launch environment
	// Kiosk or Magazines launch in unpredictable ways with unknown paths
	// MUST slam the command line!!!
#if !defined( _CERT )
	// take the command line as specified by the debugger
	if ( !DmIsDebuggerPresent() )
	{
		pCmdLine = "default.xex";
	}
#else
	pCmdLine = "default.xex";
#endif
#endif

	// The 360 has no paths and therefore the xex must reside in the same location as the dlls.
	// Only the xex must reside locally, on the box, but the dlls can be mounted from the remote share.
	// Resolve all known implicitly loaded dlls to be explicitly loaded now to allow their remote location.
	const char *pImplicitDLLs[] =
	{
		"tier0_360.dll",
		"vstdlib_360.dll",
		"vxbdm_360.dll",
		"launcher_360.dll",
	};

	// Corresponds to pImplicitDLLs. A dll load failure is only an error if that dll is tagged as required.
	const bool bDllRequired[] = 
	{
		true,	// tier0
		true,	// vstdlib
		false,	// vxbdm
		true,	// ???
	};

	char gameName[32];
	if ( !ParseCommandLineArg( pCmdLine, "-game", gameName, sizeof( gameName ) ) )
	{
#if defined( VPCGAME_STRING )
		strcpy( gameName, VPCGAME_STRING );
#endif
	}
	else
	{
		// sanitize a possible absolute game path back to expected game name
		char *pSlash = strrchr( gameName, '\\' );
		if ( pSlash )
		{
			memcpy( gameName, pSlash+1, strlen( pSlash+1 )+1 );
		}
	}

	// resolve which application gets launched
	// default is to application
	pImplicitDLLs[ARRAYSIZE( pImplicitDLLs )-1] = "launcher_360.dll";

	// the base path is the where the game is predominantly anchored
	// game runs from dvd only
	// this can only be the d: by definition on the xbox
	const char *pBasePath = "d:";

	// load all the dlls specified
	char dllPath[MAX_PATH];
	for ( int i=0; i<ARRAYSIZE( pImplicitDLLs ); i++ )
	{
		hModule = NULL;
		sprintf( dllPath, "%s\\bin\\%s", pBasePath, pImplicitDLLs[i] );
		hModule = LoadLibrary( dllPath );
		if ( !hModule && bDllRequired[i] )
		{
			Spew( "FATAL: Failed to load dll: '%s'\n", dllPath );
			return NULL;
		}
	}

	char cleanCommandLine[1024];
	char tempCommandLine[1024];
	StripCommandLineArg( pCmdLine, tempCommandLine, "-basedir" );
	StripCommandLineArg( tempCommandLine, cleanCommandLine, "-game" );

	// HACK: For ratings build, unlock everything. Remove this for later testing
	const char *pAdditionalArgs = "";
#if defined( RATINGSBUILD )
	pAdditionalArgs = "-dev -unlockchapters mp_mark_all_maps_complete";
#endif

	// set the alternate command line
	sprintf( pNewCommandLine, "%s -basedir %s -game %s\\%s %s", cleanCommandLine, pBasePath, pBasePath, gameName, pAdditionalArgs );

	// the 'main' export is guaranteed to be at ordinal 1
	// the library is already loaded, this just causes a lookup that will resolve against the shortname
	const char *pLaunchDllName = pImplicitDLLs[ARRAYSIZE( pImplicitDLLs )-1];
	hModule = LoadLibrary( pLaunchDllName );
	LauncherMain_t main = (LauncherMain_t)GetProcAddress( hModule, (LPSTR)1 );
	if ( !main )
	{
		Spew( "FATAL: 'LauncherMain' entry point not found in %s\n", pLaunchDllName );
		return NULL;
	}

	return main;
}

//-----------------------------------------------------------------------------
// 360 Application Entry Point.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
	char newCmdLine[1024];
	LauncherMain_t newMain = GetLaunchEntryPoint( newCmdLine );
	if ( newMain )
	{
		// 360 has no concept of instances, spoof one 
		newMain( (HINSTANCE)1, (HINSTANCE)0, (LPSTR)newCmdLine, 0 );
	}
}
#endif



#ifdef WIN32

//////////////////////////////////////////////////////////////////////////
//
// Cryptography support code
//
//////////////////////////////////////////////////////////////////////////

#ifdef Verify
#undef Verify
#endif

#define bswap_16 __bswap_16
#define bswap_64 __bswap_64

#include "cryptlib.h"
#include "rsa.h"
#include "osrng.h"

using namespace CryptoPP;
typedef AutoSeededX917RNG<AES> CAutoSeededRNG;

// Special usage here in the launcher without linking in tier0 tslist implementation
// list of auto-seeded RNG pointers
// these are very expensive to construct, so it makes sense to cache them

size_t launcher_keypair_signdata( const byte *pubData, int cbData, const byte *pubPrivateKey, int cbPrivateKey, byte *pbSignatureBuffer )
{
	try           // handle any exceptions crypto++ may throw
	{
		StringSource stringSourcePrivateKey( pubPrivateKey, cbPrivateKey, true );
		RSASSA_PKCS1v15_SHA_Signer rsaSigner( stringSourcePrivateKey );
		CAutoSeededRNG rng;
		// ( rsaSigner.MaxSignatureLength() );
		{
			size_t len = rsaSigner.SignMessage( rng, pubData, cbData, pbSignatureBuffer );
			return len;
		}
	}
	catch ( Exception e )
	{
	}
	catch ( ... )
	{
	}
	return 0;
}

bool launcher_keypair_verifymsg( const byte *pubData, int cbData, const byte *pubPublicKey, int cbPublicKey, const byte *pubSignature, int cbSignature )
{
	try           // handle any exceptions crypto++ may throw
	{
		StringSource stringSourcePublicKey( pubPublicKey, cbPublicKey, true );
		RSASSA_PKCS1v15_SHA_Verifier pub( stringSourcePublicKey );

		return pub.VerifyMessage( pubData, cbData, pubSignature, cbSignature );
	}
	catch ( Exception e )
	{
	}
	catch ( ... )
	{
	}
	return false;
}

#ifdef _DEBUG
void debug_launcher_keypair_generate()
{
	uint32 cKeyBits = 4096;

	bool bSuccess = false;
	std::string strPrivateKey;
	std::string strPublicKey;

	try           // handle any exceptions crypto++ may throw
	{
		// generate private key
		StringSink stringSinkPrivateKey( strPrivateKey );
		CAutoSeededRNG rng;
		RSAES_OAEP_SHA_Decryptor priv( rng, cKeyBits );
		priv.DEREncode( stringSinkPrivateKey );

		// generate public key
		StringSink stringSinkPublicKey( strPublicKey );
		RSAES_OAEP_SHA_Encryptor pub( priv );
		pub.DEREncode( stringSinkPublicKey );
		bSuccess = true;
	}
	catch ( Exception e )
	{
	}
	catch ( ... )
	{
	}

	if ( bSuccess )
	{
		bool bError = false;

		HANDLE hFile;
		hFile = CreateFile( "csgo.keypair.private", GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL );
		if ( hFile && hFile != INVALID_HANDLE_VALUE )
		{
			DWORD dwBytes = 0;
			if ( !WriteFile( hFile, strPrivateKey.c_str(), strPrivateKey.length(), &dwBytes, NULL ) )
				bError = true;
			CloseHandle( hFile );
		}
		else
		{
			bError = true;
		}

		hFile = CreateFile( "csgo.keypair.public", GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL );
		if ( hFile && hFile != INVALID_HANDLE_VALUE )
		{
			DWORD dwBytes = 0;
			if ( !WriteFile( hFile, strPublicKey.c_str(), strPublicKey.length(), &dwBytes, NULL ) )
				bError = true;
			CloseHandle( hFile );
		}
		else
		{
			bError = true;
		}

		MessageBox( 0, bError ? "Keypair generated but failed to save to file" : "Keypair files saved", "Keypair", MB_OK );
	}
	else
	{
		MessageBox( 0, "Failed to generate keypair files", "Error", MB_OK );
	}
}
#endif


#endif
