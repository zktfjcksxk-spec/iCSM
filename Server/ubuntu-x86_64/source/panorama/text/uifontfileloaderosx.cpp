//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "uifontfileloaderosx.h"
#include "tier1/fileio.h"
#include "tier1/utlstringtoken.h"
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

// ATS was deprecated in the 10.8 SDK.
#pragma clang diagnostic warning "-Wdeprecated-declarations"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUIFontLoaderOSX::CUIFontLoaderOSX() 
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUIFontLoaderOSX::~CUIFontLoaderOSX()
{
	FOR_EACH_VEC( m_vecLoadedFonts, i )
	{
		ATSFontDeactivate( m_vecLoadedFonts[i], NULL, kATSOptionFlagsDoNotNotify );
	}
	ATSFontNotify( kATSFontNotifyActionFontsChanged, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Return singleton
//-----------------------------------------------------------------------------
CUIFontLoaderOSX &CUIFontLoaderOSX::GetInstance()
{
	static CUIFontLoaderOSX s_loader;
	return s_loader;
}

//-----------------------------------------------------------------------------
// Purpose: Register fonts in a directory. Register encrypted fonts specially.
//-----------------------------------------------------------------------------
bool CUIFontLoaderOSX::RegisterDir( const char *pszDirPath )
{
	CUtlString strDirPath( pszDirPath );
	strDirPath += "*";
	CDirIterator dirIter( strDirPath );
	while ( dirIter.BNextFile() )
	{
		if ( dirIter.BCurrentIsDir() )
			continue;

		// Register encrypted font packages differently than individual unencrypted fonts
		if ( V_stristr( dirIter.CurrentFileName(), ".ttf" ) != NULL || V_stristr( dirIter.CurrentFileName(), ".otf" ) != NULL )
		{
			// Non-encrypted font that fontconfig will know what to do with
			CUtlString strFullPath = pszDirPath;
			strFullPath += dirIter.CurrentFileName();
			
			FSRef ref;
			OSStatus err = FSPathMakeRef( (const UInt8 *)strFullPath.String(), &ref, NULL );
			if ( err == noErr )
				err = ATSFontActivateFromFileReference( &ref, kATSFontContextLocal, kATSFontFormatUnspecified, NULL, kATSOptionFlagsDefault, NULL );
		}
		else if ( V_stristr( dirIter.CurrentFileName(), ".uifont" ) != NULL )
		{
			// Package of 1 or more encrypted fonts. Enumerate and add them.
			CUtlString strFullPath = pszDirPath;
			strFullPath += dirIter.CurrentFileName();
			CUIFontPackage package( strFullPath );

			// Register the fonts using identifying yet dummy filenames
			int iFont;
			while ( ( iFont = package.GetNextFileIndex() ) != -1 )
			{
				CUtlString strFontPath = CFmtStr( "%s:%d", strFullPath.String(), iFont ).Get();
				if ( RegisterFont( strFontPath, package, iFont ) )
				{
					const CUtlBuffer *pBuf = FindFontData( strFontPath );
					
					ATSFontContainerRef container;
					OSStatus err = ATSFontActivateFromMemory( (void *)pBuf->Base(), pBuf->TellPut(), kATSFontContextLocal, kATSFontFormatUnspecified, NULL, kATSOptionFlagsDefault, &container );
					if ( err == noErr )
						m_vecLoadedFonts.AddToTail( container );
					
					/*
					 // Debug code to let you find out the name of a font we pull in from a memory buffer
					 // Count the number of fonts that were loaded.
					 ItemCount fontCount = 0;
					 err = ATSFontFindFromContainer(container, kATSOptionFlagsDefault, 0,
					 NULL, &fontCount);
					 
					 if (err != noErr || fontCount < 1) {
					 return false;
					 }
					 
					 // Load font from container.
					 ATSFontRef font_ref_ats = 0;
					 ATSFontFindFromContainer(container, kATSOptionFlagsDefault, 1,
					 &font_ref_ats, NULL);
					 
					 if (!font_ref_ats) {
					 return false;
					 }
					 
					 CFStringRef name;
					 ATSFontGetPostScriptName( font_ref_ats, kATSOptionFlagsDefault, &name );
					 
					 const char *font_name = CFStringGetCStringPtr( name, CFStringGetSystemEncoding());
					REFERENCE( font_name );
					Msg( "Font: %s\n", font_name );
					
					CTFontRef font = CTFontCreateWithPlatformFont(font_ref_ats, 12.0, NULL, NULL);
					name = CTFontCopyFamilyName( font );
					font_name = CFStringGetCStringPtr( name, CFStringGetSystemEncoding());
					Msg( "Font: %s\n", font_name );*/
				}
			}
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Register an encrypted font
//-----------------------------------------------------------------------------
bool CUIFontLoaderOSX::RegisterFont( const CUtlString &strFontPath, CUIFontPackage &package, int iFont )
{
	CAutoLock lock( m_lock );

	uint32 unHashCode = CUtlStringToken( CUtlStringToken::ST_DYNAMIC, strFontPath ).m_nHashCode;
	if ( !m_mapFonts.HasElement( unHashCode ) )
	{
		// Load the font data now since the FT_New_Face override gets called on any thread
		CUtlString strFontName;
		CUtlBuffer *pBuf = new CUtlBuffer();
		if ( !package.BGetFontNameAndData( iFont, strFontName, pBuf ) )
		{
			delete pBuf;
			return false;
		}

		m_mapFonts.Insert( unHashCode, pBuf );
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Find a reference to an encrypted font
//-----------------------------------------------------------------------------
const CUtlBuffer *CUIFontLoaderOSX::FindFontData( const char *pszFilePathname )
{
	CAutoLock lock( m_lock );

	uint32 unHashCode = CUtlStringToken( CUtlStringToken::ST_DYNAMIC, pszFilePathname ).m_nHashCode;
	int iMap = m_mapFonts.Find( unHashCode );
	if ( m_mapFonts.IsValidIndex( iMap ) )
		return m_mapFonts.Element( iMap );
	return NULL;
}
