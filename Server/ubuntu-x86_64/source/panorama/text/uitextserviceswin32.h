//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UITEXTSERVICESWIN32_H
#define UITEXTSERVICESWIN32_H

#include "uitextlayoutwin32.h"

namespace panorama
{

//
// Interface that provides low-level text services
//
class CUITextServicesWin32 : public IUITextServices
{
public:
    CUITextServicesWin32();
    
	virtual void InitializeServices() OVERRIDE;
	virtual void ShutdownServices() OVERRIDE;
	
	virtual bool BLoadCustomFontCollection( const char *pchContainerDir, const char *pchPathForCustomFonts ) OVERRIDE;
	virtual bool BLoadCustomFontFile( const char *pchFontName, const char *pchFullPath ) OVERRIDE;

	virtual IUITextLayout *CreateTextLayout( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const TextLayoutParams_t *pParams, UITextLayoutFontMetrics_t *pLayoutMetrics = nullptr );
	virtual void FreeTextLayout( IUITextLayout *pLayout ) OVERRIDE;

	virtual const CUtlSortVector< CUtlString > &GetSortedValidFontNames() OVERRIDE;

	virtual IUITextTextureCache *CreateTextTextureCache( IUITextTextureProvider *pProvider ) OVERRIDE;
	virtual void FreeTextTextureCache( IUITextTextureCache *pCache ) OVERRIDE;

	virtual IUITextLayoutDrawCache *CreateTextLayoutDrawCache( IUITextTextureStorage *pStorage ) OVERRIDE;
	virtual void FreeTextLayoutDrawCache( IUITextLayoutDrawCache *pCache ) OVERRIDE;

protected:
	CThreadSafeClassMemoryPool< CUITextLayoutWin32 > m_poolTextLayout;
};

} // namespace panorama

#endif // UITEXTSERVICESWIN32_H
