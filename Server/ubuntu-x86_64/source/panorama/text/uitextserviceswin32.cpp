//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifdef OSX
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif
#include "stdafx.h"
#include "mathlib/mathlib.h"
#include "uitextserviceswin32.h"
#include "text/texttexturecache.h"
#include "text/textlayoutdrawcache.h"

using namespace panorama;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUITextServicesWin32::CUITextServicesWin32() :
	m_poolTextLayout( 256 )
{
}


//-----------------------------------------------------------------------------
// Purpose: startup, make sure win32 is configured
//-----------------------------------------------------------------------------
void CUITextServicesWin32::InitializeServices()
{
}


//-----------------------------------------------------------------------------
// Purpose: shutdown
//-----------------------------------------------------------------------------
void CUITextServicesWin32::ShutdownServices()
{
}


//-----------------------------------------------------------------------------
// Purpose: Register a custom font collection
//-----------------------------------------------------------------------------
bool CUITextServicesWin32::BLoadCustomFontCollection( const char *pchContainerDir, const char *pchPathForCustomFonts )
{
	CUtlString strPath;

    if ( pchContainerDir )
    {
        strPath = pchContainerDir;
        strPath += "/";
        strPath += pchPathForCustomFonts;
    }
    else
    {
        strPath = pchPathForCustomFonts;
    }
	V_FixSlashes( strPath.Access() );
	V_FixDoubleSlashes( strPath.Access() );

	return CUITextLayoutWin32::BLoadCustomFontCollection( strPath );
}


//-----------------------------------------------------------------------------
// Purpose: Register a single custom font file.
//-----------------------------------------------------------------------------
bool CUITextServicesWin32::BLoadCustomFontFile( const char *pchFontName, const char *pchFullPath )
{
	CUtlString strPath = pchFullPath;
	V_FixSlashes( strPath.Access() );
	V_FixDoubleSlashes( strPath.Access() );

	return CUIFontLoaderLinux::GetInstance().RegisterFile( pchFontName, strPath );
}


//-----------------------------------------------------------------------------
// Purpose: Create a text layout object and return
//-----------------------------------------------------------------------------
IUITextLayout *CUITextServicesWin32::CreateTextLayout( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const TextLayoutParams_t *pParams, UITextLayoutFontMetrics_t *pLayoutMetrics )
{
	if( pParams->m_flMaxWidth < 0.0f || pParams->m_flMaxHeight < 0.0f || pParams->m_flSize < 0.0f )
		return NULL;

	CUITextLayoutWin32 *pTextLayout = m_poolTextLayout.Alloc(); 
	if ( pTextLayout->BInitialize( pRawText, cbRawText, cTextChars, eTextEncoding, pParams, pLayoutMetrics ) )
	{
		return pTextLayout;
	}
	else
	{
		m_poolTextLayout.Free( pTextLayout );
		return NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Free a text layout object
//-----------------------------------------------------------------------------
void CUITextServicesWin32::FreeTextLayout( IUITextLayout *pLayout )
{
	m_poolTextLayout.Free( (CUITextLayoutWin32*)pLayout );
}


//-----------------------------------------------------------------------------
// Purpose: Get a list of all the valid font names for use
//-----------------------------------------------------------------------------
const CUtlSortVector< CUtlString > &CUITextServicesWin32::GetSortedValidFontNames()
{
	return CUITextLayoutWin32::GetSortedValidFontNames();
}


//-----------------------------------------------------------------------------
// Purpose: Create a text alpha texture cache
//-----------------------------------------------------------------------------
IUITextTextureCache *CUITextServicesWin32::CreateTextTextureCache( IUITextTextureProvider *pProvider )
{
	CTextTextureCache *pCache = new CTextTextureCache();
	pCache->SetTextureProvider( pProvider );
	return pCache;
}


//-----------------------------------------------------------------------------
// Purpose: Free a text alpha texture cache
//-----------------------------------------------------------------------------
void CUITextServicesWin32::FreeTextTextureCache( IUITextTextureCache *pCache )
{
	delete (CTextTextureCache*)pCache;
}


//-----------------------------------------------------------------------------
// Purpose: Create a drawn-text-layout image cache
//-----------------------------------------------------------------------------
IUITextLayoutDrawCache *CUITextServicesWin32::CreateTextLayoutDrawCache( IUITextTextureStorage *pStorage )
{
	CTextLayoutDrawCache *pCache = new CTextLayoutDrawCache();
	pCache->SetTextServices( this );
	pCache->SetTextureStorage( pStorage );
	return pCache;
}


//-----------------------------------------------------------------------------
// Purpose: Free a drawn-text-layout image cache
//-----------------------------------------------------------------------------
void CUITextServicesWin32::FreeTextLayoutDrawCache( IUITextLayoutDrawCache *pCache )
{
	delete (CTextLayoutDrawCache*)pCache;
}


