//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UITEXTLAYOUTWIN32_H
#define UITEXTLAYOUTWIN32_H

#if defined(_WIN32) || defined(SOURCE2_PANORAMA)
#pragma once
#endif

#include <DWrite.h>
#include "panorama/text/iuitextlayout.h"
#include "tier0/threadtools.h"

namespace panorama
{

//
// Interface that needs to be implemented for text layout on all platforms
//
class CUITextLayoutWin32 : public IUITextLayout
{
public:

	CUITextLayoutWin32();
	~CUITextLayoutWin32();

	static bool BInitGlobals();
	static void FreeGlobals();

	static bool BLoadCustomFontCollection( const char *pchPathForCustomFonts );

	bool BInitialize( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const IUITextServices::TextLayoutParams_t *pParams, UITextLayoutFontMetrics_t *pLayoutMetrics );
	
	// IUITextLayout implementation
	void GetRequiredSize( float &flWidth, float &flHeight );
	
    virtual bool GetCharABCWidths( float *pA, float *pB, float *pC ) OVERRIDE { Assert( false ); return false; }
    virtual bool GetCharKerning( float *pAdjust, float *pAdvance ) OVERRIDE { Assert( false ); return false; }
	virtual bool BDraw( CUtlVector<UITextOpacityMaskDataRange_t> &drawRanges, const UITextFormatProperties_t *pFormatProps, int cFormatProps, IUITextTextureStorage *pStorage, float flHeight, void *pRenderContext ) OVERRIDE;

	void HitTestPoint( Vector2D point, uint32 &unFirstHitOffset, bool &bIsTrailingHit, bool &bIsInsideString );
	void GetCharacterCoordinates( uint32 unCharIndex, IUITextLayout::HitTestRegionRect_t &charRegionRect );
	void GetCharacterRangeCoordinates( uint32 unCharStartIndex, uint32 unCharEndIndex, CUtlVector<IUITextLayout::HitTestRegionRect_t> &vecRangeRegionRects );

	// Function for rendering, called only on render thread!  Individual CUITextLayoutWin32 objects should not be shared between threads,
	// but paint and render threads may create and use instances.
	IDWriteTextLayout *GetDWriteTextLayout() { return m_pTextLayout; }

	// Set formatting for character ranges
	virtual void SetFontName( uint32 unCharStartIndex, uint32 unCharEndIndex, const char *pchFontName );
	virtual void SetFontSize( uint32 unCharStartIndex, uint32 unCharEndIndex, float flFontSize );
	virtual void SetFontStyle( uint32 unCharStartIndex, uint32 unCharEndIndex, EFontStyle eFontStyle );
	virtual void SetFontWeight( uint32 unCharStartIndex, uint32 unCharEndIndex, EFontWeight eFontWeight );
	virtual void SetUnderline( uint32 unCharStartIndex, uint32 unCharEndIndex, bool bUnderline );
	virtual void SetStrikethrough( uint32 unCharStartIndex, uint32 unCharEndIndex, bool bStrikethrough );
	virtual void SetInlineObject( uint32 unCharIndex, float flWidth, float flHeight );
	virtual void MarkColorRangeForMeasurement( uint32 unCharStartIndex, uint32 unCharEndIndex, int iColorIndex );

	static const CUtlSortVector< CUtlString > &GetSortedValidFontNames();

private:
	uint32 CharIndexToLayoutWCharIndex( uint32 unCharIndex );
	uint32 LayoutWCharIndexToCharIndex( uint32 unWCharIndex );

	static CUtlSortVector< CUtlString > m_vecSortedValidFontNames;
	static IDWriteFactory *s_pDWriteFactory;
	static IDWriteFontCollection *s_pCustomFontCollection;

	IDWriteTextFormat *m_pTextFormat;
	IDWriteTextLayout *m_pTextLayout;

	CUtlVector< uchar16 > m_vecLayoutText16;
};

} // namespace panorama

#endif // IUITEXTLAYOUT_H
