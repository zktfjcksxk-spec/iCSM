//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UITEXTSERVICESPANGO_H
#define UITEXTSERVICESPANGO_H
#pragma once

#include "uitextlayoutposix.h"

#if defined(SOURCE2_PANORAMA)
#include <tier3/tier3.h>
#endif

namespace panorama
{

//
// Interface that provides low-level text services
//
#if defined(SOURCE2_PANORAMA)
class CUITextServicesPango : public CTier3AppSystem<IUITextServices>
#else
class CUITextServicesPango : public IUITextServices
#endif
{
#if defined(SOURCE2_PANORAMA)
	typedef CTier3AppSystem<IUITextServices> BaseClass;
#endif

public:
	CUITextServicesPango();

#if defined(SOURCE2_PANORAMA)
	// IAppSystem implementation
	virtual const AppSystemInfo_t *GetDependencies() OVERRIDE;
	virtual void *QueryInterface( const char *pInterfaceName ) OVERRIDE;
#endif

	virtual void InitializeServices() OVERRIDE;
	virtual void ShutdownServices() OVERRIDE;
	
	virtual bool BLoadCustomFontCollection( const char *pchContainerDir, const char *pchPathForCustomFonts ) OVERRIDE;
	virtual bool BLoadCustomFontFile( const char *pchFontName, const char *pchFullPath ) OVERRIDE;

	virtual IUITextLayout *CreateTextLayout( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const TextLayoutParams_t *pParams, UITextLayoutFontMetrics_t *pLayoutMetrics = nullptr ) OVERRIDE;
	virtual void FreeTextLayout( IUITextLayout *pLayout ) OVERRIDE;

	virtual const CUtlSortVector< CUtlString > &GetSortedValidFontNames() OVERRIDE;

	virtual IUITextTextureCache *CreateTextTextureCache( IUITextTextureProvider *pProvider ) OVERRIDE;
	virtual void FreeTextTextureCache( IUITextTextureCache *pCache ) OVERRIDE;

	virtual IUITextLayoutDrawCache *CreateTextLayoutDrawCache( IUITextTextureStorage *pStorage ) OVERRIDE;
	virtual void FreeTextLayoutDrawCache( IUITextLayoutDrawCache *pCache ) OVERRIDE;

	virtual CJob* BDrawAsync( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding,
		UITextLayoutProperties_t &key, const char *pchFontName, const char **pchRangeFontNames, IUITextTextureStorage *pStorage ) OVERRIDE;

	virtual bool BDrawComplete( CJob* pJob, UITextOpacityMaskData_t& data, IUITextTextureStorage* pTextureStorage, bool bBlockUntilComplete ) OVERRIDE;

	virtual void SetDebugFontSelection( bool bEnable ) OVERRIDE;

protected:
	CUtlSortVector< CUtlString > m_vecFontNames;
	bool m_bFontNamesValid;
#if defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_USE_S1WRAPPER )
	CClassMemoryPool< CUITextLayout, CUtlMemoryPoolMT<CUITextLayout> > m_poolTextLayout;
#elif defined( SOURCE2_PANORAMA ) && defined( PANORAMA_USE_S1WRAPPER )
	CClassMemoryPoolMT< CUITextLayout> m_poolTextLayout;
#else
	CThreadSafeClassMemoryPool< CUITextLayout > m_poolTextLayout;
#endif

	// Text layouts need a buffer of zeroes to zero-init textures.  As nothing writes
	// to the buffer the services keeps one buffer of zeroes that everybody can share.
	uint8 m_zeroFill[16384];
};

} // namespace panorama

#endif // UITEXTSERVICESPANGO_H
