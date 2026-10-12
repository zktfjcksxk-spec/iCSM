//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUITEXTSERVICES_H
#define IUITEXTSERVICES_H
#pragma once

#include "tier1/utlsortvector.h"
#include "language.h"
#include "panorama/panoramatypes.h"
#include "panorama/text/uitexttypes.h"
#include "panorama/layout/uilength.h"

#if defined(SOURCE2_PANORAMA)
#include "appframework/iappsystem.h"
#endif

class CJob;

namespace panorama
{

class IUITextLayout;

//
// Interface that provides low-level text services
//
#if defined(SOURCE2_PANORAMA)
class IUITextServices : public IAppSystem
#else
class IUITextServices
#endif
{
public:
	virtual ~IUITextServices() {}

	virtual void InitializeServices() = 0;
	virtual void ShutdownServices() = 0;

	// pchContainerDir is a convenience for when you have a separate root and relative
	// path as they can be specified as the container and custom font path.
	// If you have an absolute path pchContainerDir can be NULL.
	virtual bool BLoadCustomFontCollection( const char *pchContainerDir, const char *pchPathForCustomFonts ) = 0;

	// Register a single custom font file.
	virtual bool BLoadCustomFontFile( const char *pchFontName, const char *pchFullPath ) = 0;

	struct TextLayoutParams_t
	{
		TextLayoutParams_t()
		{
			m_displayLanguage = k_Lang_None;
			m_pchFontName = NULL;
			m_flSize = k_flFloatNotSet;
			m_flLineHeight = k_flFloatNotSet;
			m_weight = k_EFontWeightUnset;
			m_style = k_EFontStyleUnset;
			m_align = k_ETextAlignUnset;
			m_bWrap = false;
			m_bEllipsis = false;
			m_nLetterSpacing = 0;
			m_flMaxWidth = k_flFloatNotSet;
			m_flMaxHeight = k_flFloatNotSet;
		}
		TextLayoutParams_t( ELanguage displayLanguage,
							const char *pchFontName,
							float flSize,
							float flLineHeight,
							float flMaxWidth,
							float flMaxHeight )
		{
			m_displayLanguage = displayLanguage;
			m_pchFontName = pchFontName;
			m_flSize = flSize;
			m_flLineHeight = flLineHeight;
			m_weight = k_EFontWeightUnset;
			m_style = k_EFontStyleUnset;
			m_align = k_ETextAlignUnset;
			m_bWrap = false;
			m_bEllipsis = false;
			m_nLetterSpacing = 0;
			m_flMaxWidth = flMaxWidth;
			m_flMaxHeight = flMaxHeight;
		}
		
		ELanguage m_displayLanguage;
		const char *m_pchFontName;
		float m_flSize;
		float m_flLineHeight;
		EFontWeight m_weight;
		EFontStyle m_style;
		ETextAlign m_align;
		bool m_bWrap;
		bool m_bEllipsis;
		int m_nLetterSpacing;
		float m_flMaxWidth;
		float m_flMaxHeight;
	};
		
	virtual IUITextLayout *CreateTextLayout( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const TextLayoutParams_t *pParams, UITextLayoutFontMetrics_t *pLayoutMetrics = nullptr ) = 0;
	virtual void FreeTextLayout( IUITextLayout *pLayout ) = 0;

	virtual const CUtlSortVector< CUtlString > &GetSortedValidFontNames() = 0;

	virtual IUITextTextureCache *CreateTextTextureCache( IUITextTextureProvider *pProvider ) = 0;
	virtual void FreeTextTextureCache( IUITextTextureCache *pCache ) = 0;

	virtual IUITextLayoutDrawCache *CreateTextLayoutDrawCache( IUITextTextureStorage *pStorage ) = 0;
	virtual void FreeTextLayoutDrawCache( IUITextLayoutDrawCache *pCache ) = 0;

	// Optional Async interface
	virtual CJob* BDrawAsync( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, UITextLayoutProperties_t &key, const char *pchFontName, const char **pchRangeFontNames, IUITextTextureStorage *pStorage ) { return NULL; }
	virtual bool BDrawComplete( CJob* pJob, UITextOpacityMaskData_t& data, IUITextTextureStorage* pTextureStorage, bool bBlockUntilComplete ) { return false; }

	virtual void SetDebugFontSelection( bool bEnable ) {}
};

} // namespace panorama

#endif // IUITEXTSERVICES_H
