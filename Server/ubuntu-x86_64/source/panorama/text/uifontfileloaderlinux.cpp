//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#if defined(_WIN32) && !defined(_X360)
#define WINDOWS_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0403
#include <windows.h>
#include <direct.h>
#endif
#include <errno.h>
#ifdef SOURCE_USE_SYSTEM_PANGO
#include <unistd.h>
#include <cstdlib>
#endif
#include <assert.h>
#include "tier0/platform.h"


#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "uifontfileloaderlinux.h"

#include "tier1/fileio.h"
#include "tier1/fmtstr.h"
#include "tier1/interface.h"
#include "tier1/utlstringtoken.h"

#include "uifontfile.h"

#if defined( SOURCE2_PANORAMA )
#include "filesystem.h"
#include "valvefont.h"
#endif

#include "fasttimer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

#if 0
#define FontDevMsg(...)																		\
	do {																					\
		static bool bPanFontLogging = CommandLine()->HasParm( "-panorama_font_logging");	\
		if ( bPanFontLogging ) { DevMsg(__VA_ARGS__); }										\
	} while(0)
#else
#define FontDevMsg( ... ) (void)(0)
#endif


typedef FT_Error( *PangoFT2NewFaceOverrideFunc ) ( FT_Library, const char *, FT_Long, FT_Face * );
typedef FT_Error( *FontConfigFT2NewFaceOverrideFunc ) ( FT_Library, const char *, FT_Long, FT_Face * );

#if defined( LINUX )

typedef FT_Error( *PFNFT_New_Memory_Face)( FT_Library , const FT_Byte*  , FT_Long , FT_Long  , FT_Face * );
typedef FcBool ( *PFNFcConfigAppFontAddFile)(FcConfig    *, const FcChar8  *);
typedef FcBool ( *PFNFcConfigAppFontAddDir )(FcConfig    *, const FcChar8  *);
typedef void  ( *PFNpango_ft2_new_face_substitute)( PangoFT2NewFaceOverrideFunc  func );
typedef void  ( *PFNfontconfig_ft2_new_face_substitute)( FontConfigFT2NewFaceOverrideFunc  func );

extern "C" PFNFT_New_Memory_Face s_pfnFT_New_Memory_Face;
extern "C" PFNFcConfigAppFontAddFile s_pfnFcConfigAppFontAddFile;
extern "C" PFNFcConfigAppFontAddDir s_pfnFcConfigAppFontAddDir;
extern "C" PFNpango_ft2_new_face_substitute s_pfnpango_ft2_new_face_substitute;
extern "C" PFNfontconfig_ft2_new_face_substitute s_pfnfontconfig_ft2_new_face_substitute;

#define FT_New_Memory_Face s_pfnFT_New_Memory_Face
#define FcConfigAppFontAddFile s_pfnFcConfigAppFontAddFile
#define FcConfigAppFontAddDir s_pfnFcConfigAppFontAddDir

#define pango_ft2_new_face_substitute s_pfnpango_ft2_new_face_substitute
#define fontconfig_ft2_new_face_substitute s_pfnfontconfig_ft2_new_face_substitute


#else
DLL_IMPORT void  pango_ft2_new_face_substitute( PangoFT2NewFaceOverrideFunc  func );
DLL_IMPORT void  fontconfig_ft2_new_face_substitute( FontConfigFT2NewFaceOverrideFunc  func );
#endif

#ifdef PANORAMA_USE_S1WRAPPER

int Plat_LoadModuleRaw( const char *pModule, PlatModule_t *phModule );


struct FixedModuleNameWithPath_t

{

	char szPathOnly[MAX_PATH];

	char szFixedFileName[MAX_PATH];

};



static void FixModuleName( const char *pModuleRawName, FixedModuleNameWithPath_t *pOut )

{
	pOut->szPathOnly[0] = 0;
	pOut->szFixedFileName[0] = 0;


	const char *pFileName = nullptr;

	const char *pModFileName = V_UnqualifiedFileName( pModuleRawName );

	if ( pModFileName )

	{

		// We want to include the slash and then we do a +1

		// because V_tier0_strncpy counts the terminator in the given limit

		// so the char after the slash will be turned into a zero.

		V_tier0_strncpy( pOut->szPathOnly, pModuleRawName, MIN( pModFileName - pModuleRawName + 1, sizeof( pOut->szPathOnly ) ) );

		pFileName = pModFileName;

	}

	else

	{

		pOut->szPathOnly[0] = 0;

		pFileName = pModuleRawName;

	}



	// Enforce lib prefix.

#if defined( PLATFORM_LINUX ) || defined( PLATFORM_OSX )

	V_tier0_strncpy( pOut->szFixedFileName, "lib", sizeof( pOut->szFixedFileName ) );

#endif

	V_tier0_strncat( pOut->szFixedFileName, pFileName, sizeof( pOut->szFixedFileName ) );



	// Enforce extension.

#ifdef PLATFORM_LINUX

	V_SetExtension( pOut->szFixedFileName, ".so", sizeof( pOut->szFixedFileName ) );

#elif defined( PLATFORM_OSX )

	V_SetExtension( pOut->szFixedFileName, ".dylib", sizeof( pOut->szFixedFileName ) );

#elif defined( PLATFORM_WINDOWS)

	V_SetExtension( pOut->szFixedFileName, ".dll", sizeof( pOut->szFixedFileName ) );

#else

#error Unsupported platform

#endif

}

PlatModule_t Plat_LoadModule( const char *pModule )
{
	FixedModuleNameWithPath_t fixedName;

	FixModuleName( pModule, &fixedName );

	char pFullPathFixed[MAX_PATH];

	// Push path and file back together.

	V_tier0_strncpy( pFullPathFixed, fixedName.szPathOnly, sizeof( pFullPathFixed ) );

	V_tier0_strncat( pFullPathFixed, fixedName.szFixedFileName, sizeof( pFullPathFixed ) );

	V_FixSlashes( pFullPathFixed );

	PlatModule_t hModule;
	int nErr = Plat_LoadModuleRaw( pFullPathFixed, &hModule );
	return nErr == 0 ? hModule : PLAT_MODULE_INVALID;
}

void *Plat_GetModuleProcAddress( PlatModule_t hModule, const char *pszName )
{
#if defined( PLATFORM_WINDOWS )
	return ( hModule ) ? ::GetProcAddress( (HMODULE)hModule, pszName ) : NULL;
#elif defined( PLATFORM_POSIX )
	return ( hModule ) ? dlsym( hModule, pszName ) : NULL;
#else
#error Unsupported platform
#endif
}
#endif



FT_NewFace_Ptr GetOriginal_FT_New_Face()
{
#if !defined( SOURCE2_PANORAMA )
#if defined(WIN32)
	CSysModule *hFreeType = Sys_LoadModule( "libfreetype-6", SYS_NOLOAD );
#elif defined(OSX)
	CSysModule *hFreeType = Sys_LoadModule( "libfreetype", SYS_NOLOAD );
#elif defined(LINUX)
	CSysModule *hFreeType = Sys_LoadModule( "libpangoft2-1.0", SYS_NOLOAD );
#else
#error "needs place to get freetype func pointer from"
#endif
	void *pFunc = Sys_GetProcAddress( hFreeType, "FT_New_Face" );
#else
#if defined(WIN32)
	PlatModule_t hFreeType = Plat_LoadModule( "libfreetype-6" );
#elif defined(OSX)
	PlatModule_t hFreeType = Plat_LoadModule( "freetype" );
#elif defined(LINUX)
    // We have to include the .so here so that the extension detection
    // doesn't think the .0 is an extension.
	PlatModule_t hFreeType = Plat_LoadModule( "pangoft2-1.0.so" );
#else
#error "needs place to get freetype func pointer from"
#endif
	void *pFunc = Plat_GetModuleProcAddress( hFreeType, "FT_New_Face" );
#endif
	return (FT_NewFace_Ptr)pFunc;
}


//-----------------------------------------------------------------------------
// Purpose: Override of FT_New_Face from libfreetype. Load a new font face.
// Detect when it is an encrypted font
//-----------------------------------------------------------------------------
DLL_EXPORT FT_Error UI_Font_New_Face_Override( FT_Library library, const char *pszFilePathname, FT_Long iFace, FT_Face *pFace )
{
	// NOTE: This can get called from any thread.
	CUIFontLoaderLinux &loader = CUIFontLoaderLinux::GetInstance();
    return loader.NewFaceOverride( library, pszFilePathname, iFace, pFace );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUIFontLoaderLinux::CUIFontLoaderLinux() 
: m_pfnNewFaceOriginal( NULL )
{
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUIFontLoaderLinux::~CUIFontLoaderLinux()
{
	Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: setup our font face redirect functions
//-----------------------------------------------------------------------------
bool CUIFontLoaderLinux::Initialize()
{
#if defined( LINUX )
	if ( pango_ft2_new_face_substitute && fontconfig_ft2_new_face_substitute )
	{
#endif
#ifdef SOURCE_USE_SYSTEM_PANGO
		m_pfnNewFaceOriginal = &FT_New_Face;
#else
		m_pfnNewFaceOriginal = GetOriginal_FT_New_Face();
		pango_ft2_new_face_substitute( &UI_Font_New_Face_Override );
		fontconfig_ft2_new_face_substitute( &UI_Font_New_Face_Override );
#endif
		return true;
#if defined( LINUX )
	}
	return false;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: disconnect our func pointers from freetype/pango
//-----------------------------------------------------------------------------
void CUIFontLoaderLinux::Shutdown()
{
	CAutoLock lock( m_lock );

	m_mapFonts.PurgeAndDeleteElements();
#ifdef SOURCE_USE_SYSTEM_PANGO
	FOR_EACH_HASHMAP( m_mapNativeFonts, i )
		unlink( m_mapNativeFonts.Element( i ).String() );
	m_mapNativeFonts.Purge();
#else


#if defined( LINUX )
	if ( pango_ft2_new_face_substitute )
#endif
		pango_ft2_new_face_substitute( NULL );
#if defined( LINUX )
	if ( fontconfig_ft2_new_face_substitute )
#endif
		fontconfig_ft2_new_face_substitute( NULL );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Return singleton
//-----------------------------------------------------------------------------
CUIFontLoaderLinux &CUIFontLoaderLinux::GetInstance()
{
	static CUIFontLoaderLinux s_loader;
	return s_loader;
}


//-----------------------------------------------------------------------------
// Purpose: Register fonts in a directory. Register encrypted fonts specially.
//-----------------------------------------------------------------------------
bool CUIFontLoaderLinux::RegisterDir( const char *pszDirPath )
{
	if ( !m_pfnNewFaceOriginal )
		Initialize();

	bool bSuccess = true;
	CUtlString strDirPath( pszDirPath );
	CDirIterator dirIter( strDirPath, "*.*" );
	while ( dirIter.BNextFile() )
	{
		if ( dirIter.BCurrentIsDir() )
			continue;

		// Register .vfont files
		if ( V_striEndsWith( dirIter.CurrentFileName(), ".vfont" ) )
		{
			CUtlString strFullPath = pszDirPath;
			strFullPath += dirIter.CurrentFileName();
#ifdef SOURCE_USE_SYSTEM_PANGO
			if ( RegisterFont( strFullPath, strFullPath, nullptr, -1 ) )
				bSuccess = FcConfigAppFontAddFile( NULL, (const FcChar8 *)NativeFontPath( strFullPath ) ) && bSuccess;
			else
#else
			if( !RegisterFont( strFullPath, strFullPath, nullptr, -1 ))
#endif
			{
				bSuccess = false;
			}
		}
	}

	CFastTimer fontTimer;
	fontTimer.Start();
	FcConfigAppFontAddDir( NULL, (const FcChar8 *)pszDirPath );
	fontTimer.End();
	Msg( "FcConfigAppFontAddDir %s took %4.3f ms.\n", pszDirPath, (float)fontTimer.GetDuration().GetMillisecondsF() );

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Register a single font file. Register encrypted fonts specially.
//-----------------------------------------------------------------------------
bool CUIFontLoaderLinux::RegisterFile( const char *pszFontName, const char *pszFullPath )
{
	if ( !m_pfnNewFaceOriginal )
		Initialize();

	FontDevMsg( "CUIFontLoaderLinux::RegisterFile - Name=%s, Path=%s\n", pszFontName, pszFullPath );

	// Register encrypted font packages differently than individual unencrypted fonts
	if ( V_striEndsWith( pszFullPath, ".ttf" ) ||
		 V_striEndsWith( pszFullPath, ".otf" ) )
	{
		// Non-encrypted font that fontconfig will know what to do with
		FcBool bSuccess = FcConfigAppFontAddFile( NULL, (const FcChar8 * )pszFullPath );
		Assert( bSuccess == FcTrue );
	}
#if defined( SOURCE2_PANORAMA )
	else if ( V_striEndsWith( pszFullPath, ".vfont" ) )
	{
		const char *pszRegisterName = pszFontName ? pszFontName : pszFullPath;
		if ( RegisterFont( pszRegisterName, pszFullPath, nullptr, -1 ) )
		{
			FcBool bSuccess = FcConfigAppFontAddFile( NULL, (const FcChar8 * )
#ifdef SOURCE_USE_SYSTEM_PANGO
				NativeFontPath( pszRegisterName )
#else
				pszRegisterName
#endif
			);
			Assert( bSuccess == FcTrue );
		}
	}
#endif
	else if ( V_striEndsWith( pszFullPath, ".uifont" ) )
	{
		// Package of 1 or more encrypted fonts. Enumerate and add them.
		CUIFontPackage package( pszFullPath );

		// Register the fonts using identifying yet dummy filenames
		int iFont;
		while ( ( iFont = package.GetNextFileIndex() ) != -1 )
		{
			CUtlString strFontPath = CFmtStr( "%s:%d", pszFullPath, iFont ).Get();
			if ( RegisterFont( strFontPath, strFontPath, &package, iFont ) )
			{
				FcBool bSuccess = FcConfigAppFontAddFile( NULL, (const FcChar8 * )
#ifdef SOURCE_USE_SYSTEM_PANGO
					NativeFontPath( strFontPath.String() )
#else
					strFontPath.String()
#endif
				);
				Assert( bSuccess == FcTrue );
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Register an encrypted font
//-----------------------------------------------------------------------------
bool CUIFontLoaderLinux::RegisterFont( const char *pszRegisterName, const char *pszFullPath, CUIFontPackage *pPackage, int iPackageFont )
{
	CAutoLock lock( m_lock );

#if defined( SOURCE2_PANORAMA )
	uint32 unHashCode = MakeStringToken( pszRegisterName ).m_nHashCode;
#else
	uint32 unHashCode = CUtlStringToken( CUtlStringToken::ST_DYNAMIC, pszRegisterName ).m_nHashCode;
#endif
	FontDevMsg( "CUIFontLoaderLinux::RegisterFont - Name=%s, Path=%s, hashCode=%d\n", pszRegisterName, pszFullPath, unHashCode );
	if ( !m_mapFonts.HasElement( unHashCode ) )
	{
		// Load the font data now since the FT_New_Face override gets called on any thread
		CUtlBuffer *pBuf = new CUtlBuffer();
		if ( pPackage )
		{
			CUtlString strFontName;
			if ( !pPackage->BGetFontNameAndData( iPackageFont, strFontName, pBuf ) )
			{
				delete pBuf;
				return false;
			}
		}
#if defined( SOURCE2_PANORAMA )
		else
		{
			if ( !g_pFullFileSystem->ReadFile( pszFullPath, NULL, *pBuf ) ||
				 !ValveFont::DecodeFont( *pBuf ) )
			{
				delete pBuf;
				return false;
			}
		}
#else
		else
		{
			delete pBuf;
			return false;
		}
#endif

#ifdef SOURCE_USE_SYSTEM_PANGO
		// Retain the original ValveFont/package decoder. Upstream Fontconfig and
		// Pango open regular files, so keep a private decoded file until shutdown.
		char szTemporary[] = "/tmp/csgo-panorama-font-XXXXXX";
		int fd = mkstemp( szTemporary );
		if ( fd < 0 ) { delete pBuf; return false; }
		int written = 0;
		while ( written < pBuf->TellPut() )
		{
			ssize_t count = write( fd, (const byte *)pBuf->Base() + written, pBuf->TellPut() - written );
			if ( count < 0 && errno == EINTR ) continue;
			if ( count <= 0 ) { close( fd ); unlink( szTemporary ); delete pBuf; return false; }
			written += count;
		}
		close( fd );
		m_mapNativeFonts.Insert( unHashCode, CUtlString( szTemporary ) );
#endif
		m_mapFonts.Insert( unHashCode, pBuf );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Find a reference to an encrypted font
//-----------------------------------------------------------------------------
const CUtlBuffer *CUIFontLoaderLinux::FindFontData( const char *pszFilePathname )
{
	CAutoLock lock( m_lock );

#if defined( SOURCE2_PANORAMA )
	CPathString strPath( pszFilePathname ); // Fixup slashes
	uint32 unHashCode = MakeStringToken( strPath.GetUTF8Path() ).m_nHashCode;
#else
	uint32 unHashCode = CUtlStringToken( CUtlStringToken::ST_DYNAMIC, pszFilePathname ).m_nHashCode;
#endif
	FontDevMsg( "CUIFontLoaderLinux::FindFontData - Path=%s, hashCode=%d - ", pszFilePathname, unHashCode );
	int iMap = m_mapFonts.Find( unHashCode );
	if ( m_mapFonts.IsValidIndex( iMap ) )
	{
		FontDevMsg( "Found\n" );
		return m_mapFonts.Element( iMap );
	}
	FontDevMsg( "Not Found\n" );
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Override of FT_New_Face from libfreetype. Load a new font face.
// Detect when it is an encrypted font
//-----------------------------------------------------------------------------
FT_Error CUIFontLoaderLinux::NewFaceOverride( FT_Library library, const char *pszFilePathname, FT_Long iFace, FT_Face *pFace )
{
	// NOTE: This can get called from any thread.
	const CUtlBuffer *pBuf = FindFontData( pszFilePathname );
	if ( pBuf != NULL )
		return FT_New_Memory_Face( library, ( const FT_Byte * )pBuf->Base(), pBuf->TellPut(), iFace, pFace );
	return m_pfnNewFaceOriginal( library, pszFilePathname, iFace, pFace );
}

#ifdef SOURCE_USE_SYSTEM_PANGO
const char *CUIFontLoaderLinux::NativeFontPath( const char *pszRegisterName )
{
	CAutoLock lock( m_lock );
	uint32 hash = MakeStringToken( pszRegisterName ).m_nHashCode;
	int index = m_mapNativeFonts.Find( hash );
	return m_mapNativeFonts.IsValidIndex( index ) ? m_mapNativeFonts.Element( index ).String() : pszRegisterName;
}
#endif
