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
#include "uitextservicespango.h"
#include "uifontfileloaderlinux.h"
#include "texttexturecache.h"
#include "textlayoutdrawcache.h"
#include "jobthread.h"
#include "utlsymbol.h"

using namespace panorama;

static CUtlSymbolTable s_fontNames;

namespace panorama
{
	//
	// Job to draw text layout into freetype bitmap using pango
	//
	class CTextLayoutPangoJob : public CJob
	{
	public:
		CTextLayoutPangoJob( CUITextServicesPango &textServices, const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, UITextLayoutProperties_t &key, const char *pchFontName, const char **pchRangeFontNames, int nMaxStringSizePixels );
		virtual ~CTextLayoutPangoJob();
		virtual JobStatus_t DoExecute();

		CUITextServicesPango			&m_textServices;
		void *							m_pRawText;
		int								m_cbRawText;
		int								m_cTextChars;
		EPanoramaTextEncoding			m_eTextEncoding;
		UITextLayoutProperties_t		&m_key;
		CUtlSymbol						m_fontName;
		CUtlVector<	CUtlSymbol >		m_rangeFontNames;
		int								m_nMaxStringSizePixels;

		CUtlVector< TextLayoutPangoBitmapRange_t > m_vecBitmapRanges;
	};
}

//-----------------------------------------------------------------------------
CTextLayoutPangoJob::CTextLayoutPangoJob( CUITextServicesPango &textServices, const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, UITextLayoutProperties_t &key, const char *pchFontName, const char **pchRangeFontNames, int nMaxStringSizePixels ) :
	m_textServices( textServices ),
	m_cbRawText( cbRawText ),
	m_cTextChars( cTextChars ),
	m_eTextEncoding( eTextEncoding ),
	m_key( key ),
	m_nMaxStringSizePixels( nMaxStringSizePixels )
{
	//Msg( "CTextLayoutDrawJob constructor 0x%08x\n", this );
	m_pRawText = malloc( m_cbRawText );
	memcpy( (char*)m_pRawText, (char*)pRawText, m_cbRawText );
	m_fontName = s_fontNames.AddString( pchFontName );
	int nRangeFontNames = m_key.m_arrayFormats.Count();
	m_rangeFontNames.EnsureCount( nRangeFontNames );
	for( int i = 0; i < nRangeFontNames; ++i )
	{
		m_rangeFontNames[i] = pchRangeFontNames[i] ? (UtlSymId_t)s_fontNames.AddString( pchRangeFontNames[i] ) : UTL_INVAL_SYMBOL;
	}
}

//-----------------------------------------------------------------------------
CTextLayoutPangoJob::~CTextLayoutPangoJob()
{
	//Msg( "CTextLayoutDrawJob destructor 0x%08x\n", this );
	// Free all bitmap buffers
	for( int i = 0; i < m_vecBitmapRanges.Count(); ++i )
	{
		if( m_vecBitmapRanges[i].m_pBuffer )
		{
			free( m_vecBitmapRanges[i].m_pBuffer );
		}
	}

	free( m_pRawText );
}

//-----------------------------------------------------------------------------
JobStatus_t CTextLayoutPangoJob::DoExecute()
{
	VPROF_BUDGET( "CDrawTextLayoutJob::DoExecute", VPROF_BUDGETGROUP_TENFOOT );

	const UITextFormatProperties_t &defaultFormat = m_key.m_defaultFormat;

	IUITextServices::TextLayoutParams_t params( m_key.m_language, s_fontNames.String( m_fontName ), m_key.m_defaultFormat.m_flFontSize, m_key.m_flLineHeight, m_key.m_flWidth, m_key.m_flHeight );
	params.m_weight = defaultFormat.m_eWeight;
	params.m_style = defaultFormat.m_eStyle;
	params.m_align = m_key.m_Align;
	params.m_bWrap = m_key.m_bWrap;
	params.m_bEllipsis = m_key.m_bEllipsis;
	params.m_nLetterSpacing = defaultFormat.m_iLetterSpacing;
	CUITextLayoutPango *pTextLayout = (CUITextLayoutPango *)m_textServices.CreateTextLayout( m_pRawText, m_cbRawText, m_cTextChars, m_eTextEncoding, &params );

	Assert( pTextLayout );
	if( pTextLayout )
	{
		// set any special defaults
		if( defaultFormat.m_bUnderline )
			pTextLayout->SetUnderline( 0, m_cTextChars - 1, true );
		if( defaultFormat.m_bStrikethrough )
			pTextLayout->SetStrikethrough( 0, m_cTextChars - 1, true );

		// add in all range formats
		for( int i = 0; (uint)i < m_key.m_arrayFormats.Count(); i++ )
		{
			const UITextFormatProperties_t &rangeFormat = m_key.m_arrayFormats[i];
			uint32 unStartIndex = rangeFormat.m_unCharStartIndex;
			uint32 unEndIndex = rangeFormat.m_unCharEndIndex;

			if( m_rangeFontNames[i].IsValid() )
				pTextLayout->SetFontName( unStartIndex, unEndIndex, s_fontNames.String(m_rangeFontNames[i]) );

			if( rangeFormat.m_flFontSize != defaultFormat.m_flFontSize )
				pTextLayout->SetFontSize( unStartIndex, unEndIndex, rangeFormat.m_flFontSize );

			if( rangeFormat.m_eStyle != defaultFormat.m_eStyle )
				pTextLayout->SetFontStyle( unStartIndex, unEndIndex, rangeFormat.m_eStyle );

			if( rangeFormat.m_eWeight != defaultFormat.m_eWeight )
				pTextLayout->SetFontWeight( unStartIndex, unEndIndex, rangeFormat.m_eWeight );

			if( rangeFormat.m_bUnderline != defaultFormat.m_bUnderline )
				pTextLayout->SetUnderline( unStartIndex, unEndIndex, rangeFormat.m_bUnderline );

			if( rangeFormat.m_bStrikethrough != defaultFormat.m_bStrikethrough )
				pTextLayout->SetStrikethrough( unStartIndex, unEndIndex, rangeFormat.m_bStrikethrough );

			if( rangeFormat.m_flInlineObjectWidth > 0.0f || rangeFormat.m_flInlineObjectHeight > 0.0f )
				pTextLayout->SetInlineObject( unStartIndex, rangeFormat.m_flInlineObjectWidth, rangeFormat.m_flInlineObjectHeight );
		}

		// before we make any measurements, we need to add drawing effects. Adding an effect can cause DWrite to slightly modify
		// the dimensions of a glyph run.
		CTextLayoutDrawCache::AddColorDrawEffects( pTextLayout, m_key.m_arrayFormats.Base(), m_key.m_arrayFormats.Count(), m_cTextChars );

		pTextLayout->BDrawBitmaps( m_vecBitmapRanges, m_key.m_arrayFormats.Base(), m_key.m_arrayFormats.Count(), m_nMaxStringSizePixels );

		m_textServices.FreeTextLayout( pTextLayout );
	}

	return JOB_OK;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUITextServicesPango::CUITextServicesPango() :
	m_poolTextLayout( 256 )
#if !defined( SOURCE2_PANORAMA )
	, m_vecFontNames( DefLessFuncCtx( CUtlString ) )
#endif
{
	m_bFontNamesValid = false;
	// This buffer is always kept zero and is provided
	// to text layouts for zero-filling glyph textures.
	memset( m_zeroFill, 0, sizeof(m_zeroFill) );
}


#if defined(SOURCE2_PANORAMA)

static AppSystemInfo_t s_pDependencies[] =
{
	{ "filesystem", FILESYSTEM_INTERFACE_VERSION },
	{ NULL, NULL }
};

//-----------------------------------------------------------------------------
// Purpose: return app system dependencies
//-----------------------------------------------------------------------------
const AppSystemInfo_t *CUITextServicesPango::GetDependencies()
{
	return s_pDependencies;
}


//-----------------------------------------------------------------------------
// Purpose: app system interface discovery
//-----------------------------------------------------------------------------
void *CUITextServicesPango::QueryInterface( const char *pInterfaceName )
{
	if ( !V_strncmp( pInterfaceName, PANORAMA_TEXT_SERVICES_INTERFACE_VERSION, V_strlen( PANORAMA_TEXT_SERVICES_INTERFACE_VERSION ) + 1 ) )
	{
		return static_cast<IUITextServices *>(this);
	}

	return BaseClass::QueryInterface( pInterfaceName );
}


#endif // SOURCE2_PANORAMA


//-----------------------------------------------------------------------------
// Purpose: startup, make sure pango is configured
//-----------------------------------------------------------------------------
void CUITextServicesPango::InitializeServices()
{
	CUITextLayoutPango::Initialize( &m_zeroFill, sizeof( m_zeroFill ) );

	m_bFontNamesValid = false;
}


//-----------------------------------------------------------------------------
// Purpose: shutdown
//-----------------------------------------------------------------------------
void CUITextServicesPango::ShutdownServices()
{
	CUITextLayoutPango::Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: Register a custom font collection
//-----------------------------------------------------------------------------
bool CUITextServicesPango::BLoadCustomFontCollection( const char *pchContainerDir, const char *pchPathForCustomFonts )
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

	if ( !CUIFontLoaderLinux::GetInstance().RegisterDir( strPath ) )
	{
		return false;
	}

	m_bFontNamesValid = false;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Register a single custom font file.
//-----------------------------------------------------------------------------
bool CUITextServicesPango::BLoadCustomFontFile( const char *pchFontName, const char *pchFullPath )
{
	CUtlString strPath = pchFullPath;
	V_FixSlashes( strPath.Access() );
	V_FixDoubleSlashes( strPath.Access() );

	return CUIFontLoaderLinux::GetInstance().RegisterFile( pchFontName, strPath );
}


//-----------------------------------------------------------------------------
// Purpose: Create a text layout object and return
//-----------------------------------------------------------------------------
IUITextLayout *CUITextServicesPango::CreateTextLayout( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const TextLayoutParams_t *pParams, UITextLayoutFontMetrics_t *pLayoutMetrics )
{
	VPROF_BUDGET( "CUITextLayoutPango::CreateTextLayout", VPROF_BUDGETGROUP_TENFOOT );
	if( pParams->m_flMaxWidth < 0.0f || pParams->m_flMaxHeight < 0.0f || pParams->m_flSize < 0.0f )
		return NULL;
	
	CUITextLayout *pTextLayout = m_poolTextLayout.Alloc();
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
void CUITextServicesPango::FreeTextLayout( IUITextLayout *pLayout )
{
	CUITextLayoutPango* pPangoLayout = (CUITextLayoutPango*)pLayout;
	pPangoLayout->Clear();
	m_poolTextLayout.Free( (CUITextLayout*)pLayout );
}


//-----------------------------------------------------------------------------
// Purpose: Return the current list of valid font names
//-----------------------------------------------------------------------------
const CUtlSortVector< CUtlString > &CUITextServicesPango::GetSortedValidFontNames()
{
	if ( !m_bFontNamesValid )
	{
		m_vecFontNames.RemoveAll();
		CUITextLayoutPango::GetFontList( &m_vecFontNames );
		m_bFontNamesValid = true;
	}

	return m_vecFontNames;
}


//-----------------------------------------------------------------------------
// Purpose: Create a text alpha texture cache
//-----------------------------------------------------------------------------
IUITextTextureCache *CUITextServicesPango::CreateTextTextureCache( IUITextTextureProvider *pProvider )
{
	CTextTextureCache *pCache = new CTextTextureCache();
	pCache->SetTextureProvider( pProvider );
	return pCache;
}


//-----------------------------------------------------------------------------
// Purpose: Free a text alpha texture cache
//-----------------------------------------------------------------------------
void CUITextServicesPango::FreeTextTextureCache( IUITextTextureCache *pCache )
{
	pCache->Purge();
	delete (CTextTextureCache*)pCache;
}


//-----------------------------------------------------------------------------
// Purpose: Create a drawn-text-layout image cache
//-----------------------------------------------------------------------------
IUITextLayoutDrawCache *CUITextServicesPango::CreateTextLayoutDrawCache( IUITextTextureStorage *pStorage )
{
	CTextLayoutDrawCache *pCache = new CTextLayoutDrawCache();
	pCache->SetTextServices( this );
	pCache->SetTextureStorage( pStorage );
	return pCache;
}


//-----------------------------------------------------------------------------
// Purpose: Free a drawn-text-layout image cache
//-----------------------------------------------------------------------------
void CUITextServicesPango::FreeTextLayoutDrawCache( IUITextLayoutDrawCache *pCache )
{
	delete (CTextLayoutDrawCache*)pCache;
}

//-----------------------------------------------------------------------------
// Purpose: Kick off a job to generate freetype bitmaps for the text layout
//-----------------------------------------------------------------------------
CJob* CUITextServicesPango::BDrawAsync( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, UITextLayoutProperties_t &key, const char *pchFontName, const char **pchRangeFontNames, IUITextTextureStorage *pStorage )
{
	int nMaxStringSizePixels = MAX( 1024, (pStorage->GetMaximumTextureWidth()*1.1) )*PANGO_SCALE;
	CJob* pDrawJob = new CTextLayoutPangoJob( *this, pRawText, cbRawText, cTextChars, eTextEncoding, key, pchFontName, pchRangeFontNames, nMaxStringSizePixels );

	if( pDrawJob )
	{
		pDrawJob->SetFlags( JF_QUEUE );
		g_pThreadPool->AddJob( pDrawJob );
		return pDrawJob;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: If draw job is complete, copy the generated bits to the output texture and return true
//-----------------------------------------------------------------------------
bool CUITextServicesPango::BDrawComplete( CJob* pJob, UITextOpacityMaskData_t &data, IUITextTextureStorage* pTextureStorage, bool bBlockUntilComplete )
{
	CTextLayoutPangoJob* pDrawJob = (CTextLayoutPangoJob*)pJob;
	if( pDrawJob && bBlockUntilComplete )
	{
		pDrawJob->WaitForFinish();
	}
	if( pDrawJob && pDrawJob->IsFinished() )
	{
		int rangeCount = pDrawJob->m_vecBitmapRanges.Count();
		if( rangeCount )
		{
			data.m_cRangeData = rangeCount;
			data.m_pRangeData = new UITextOpacityMaskDataRange_t[rangeCount];

			for( int i = 0; i < rangeCount; ++i )
			{
				CUITextLayoutPango::UpdateTextureForRun( data.m_pRangeData[i], pDrawJob->m_vecBitmapRanges[i], pTextureStorage );
			}
		}
		return true;
	}
	return false;
}

void CUITextServicesPango::SetDebugFontSelection( bool bEnable )
{
	CUITextLayoutPango::SetDebugFontSelection( bEnable );
}


