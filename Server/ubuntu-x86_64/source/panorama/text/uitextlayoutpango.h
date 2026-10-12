//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UITEXTLAYOUTPANGO_H
#define UITEXTLAYOUTPANGO_H
#pragma once

#include <pango/pango-layout.h>
#include <pango/pangofc-fontmap.h>
#include <pango/pangoft2.h>
#include <pango/pango.h>
#if !defined( SOURCE2_PANORAMA )
#include <GL/gl.h>
#ifdef LINUX
#include <GL/glx.h>
#endif
#endif

#include "panorama/text/iuitextlayout.h"
#include "panorama/text/iuitextservices.h"
#include "tier0/threadtools.h"
#include "tier1/utlsortvector.h"

namespace panorama
{
class IUITexture;

// Contains all info related to a span of text rendered to a freetype bitmap
struct TextLayoutPangoBitmapRange_t
{
	int m_nWidth;
	int m_nRows;
	float m_flTextureHeight; // The height of the associated texture can differ from the pango bitmap if line spacing is used to squash lines together  
	float m_flStringOffsetX;		// Start X pos relative to top left of complete rendered text
	float m_flStringOffsetY;		// Start Y pos relative to top left of complete rendered text
	int m_iColorIndex;				// Index of color from provided per-range color indices. -1 if unset
	unsigned char* m_pBuffer;
};


//
// Interface that needs to be implemented for text layout on all platforms
//
class CUITextLayoutPango : public IUITextLayout
{
public:

	static void Initialize( const void * pZeroFillBuffer, int nZeroFillBufferSize );
	static void Shutdown();
	static bool GetFontList( CUtlSortVector< CUtlString > *pList );
	static void SetDebugFontSelection( bool bEnable );

	CUITextLayoutPango();
	~CUITextLayoutPango();

	bool BInitialize( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const IUITextServices::TextLayoutParams_t *pParams, UITextLayoutFontMetrics_t *pLayoutMetrics );
	void Clear();

	// IUITextLayout implementation
	virtual void GetRequiredSize( float &flWidth, float &flHeight ) OVERRIDE;
    virtual bool GetCharABCWidths( float *pA, float *pB, float *pC ) OVERRIDE;
    virtual bool GetCharKerning( float *pAdjust, float *pAdvance ) OVERRIDE;
	virtual void HitTestPoint( Vector2D point, uint32 &unFirstHitOffset, bool &bIsTrailingHit, bool &bIsInsideString ) OVERRIDE;
	virtual void GetCharacterCoordinates( uint32 unCharIndex, IUITextLayout::HitTestRegionRect_t &charRegionRect ) OVERRIDE;
	virtual void GetCharacterRangeCoordinates( uint32 unCharStartIndex, uint32 unCharEndIndex, CUtlVector<IUITextLayout::HitTestRegionRect_t> &vecRangeRegionRects ) OVERRIDE;

	// Set formatting for character ranges
	virtual void SetFontName( uint32 unCharStartIndex, uint32 unCharEndIndex, const char *pchFontName ) OVERRIDE;
	virtual void SetFontSize( uint32 unCharStartIndex, uint32 unCharEndIndex, float flFontSize ) OVERRIDE;
	virtual void SetFontStyle( uint32 unCharStartIndex, uint32 unCharEndIndex, EFontStyle eFontStyle ) OVERRIDE;
	virtual void SetFontWeight( uint32 unCharStartIndex, uint32 unCharEndIndex, EFontWeight eFontWeight ) OVERRIDE;
	virtual void SetUnderline( uint32 unCharStartIndex, uint32 unCharEndIndex, bool bUnderline ) OVERRIDE;
	virtual void SetStrikethrough( uint32 unCharStartIndex, uint32 unCharEndIndex, bool bStrikethrough ) OVERRIDE;
	virtual void SetInlineObject( uint32 unCharIndex, float flWidth, float flHeight ) OVERRIDE;
	virtual void MarkColorRangeForMeasurement( uint32 unCharStartIndex, uint32 unCharEndIndex, int iColorIndex ) OVERRIDE;

	virtual bool BDraw( CUtlVector<UITextOpacityMaskDataRange_t> &drawRanges, const UITextFormatProperties_t *pFormatProps, int cFormatProps, IUITextTextureStorage *pStorage, float flHeight, void *pRenderContext ) OVERRIDE;
	virtual bool Resize( float flMaxWidth, float flMaxHeight ) OVERRIDE;

private:

	friend class CTextLayoutPangoJob;
	friend class CUITextServicesPango;

	static UITextTextureHandle_t	UpdateTextureForRun( UITextOpacityMaskDataRange_t &cacheRange, TextLayoutPangoBitmapRange_t &bitmapRange, IUITextTextureStorage *pStorage );

	bool	BDrawBitmaps( CUtlVector<TextLayoutPangoBitmapRange_t> &bitmapRanges, const UITextFormatProperties_t *pFormatProps, int cFormatProps, int nMaxStringSizePixels );

	int FindNextTextRangeWithColorAndInRange( const UITextFormatProperties_t *pFormatProps, int cFormatProps, uint iStartIndex, uint iEndIndex, int iCurrent = -1 );
	PangoStyle GetPangoFontStyle( EFontStyle style );
	PangoAlignment GetPangoAlignment( ETextAlign align );
	PangoWeight GetPangoFontWeight( EFontWeight weight );
	void SetAttribute( PangoAttribute *pAttr );
	bool HasUnderlineAttr( PangoLayoutRun *pLayoutRun );
	bool HasStrikethroughAttr( PangoLayoutRun *pLayoutRun );
	unsigned char*  DrawRun( PangoLayoutRun *pLayoutRun, int yPUBaseline, TextLayoutPangoBitmapRange_t& bitmapRange, int cyPUTextDecorationLineWidth );
	void DrawRect( FT_Bitmap *pBitmap, int xPU, int yPU, int yPUOffset, int cxPU, int cyPU );

	uint32 CharIndexToLayoutByteIndex( uint32 unCharIndex );
	uint32 LayoutByteIndexToCharIndex( uint32 unByteIndex );

	static CThreadMutex		s_GlobalMutex;
	static int s_nInitializeCount;
	static PangoFontMap *s_pFontmap;
	static PangoContext *s_pContext;

	static const void *s_pZeroFillBuffer;
	static int s_nZeroFillBufferSize;
	
	static bool s_bDebugFontSelection;

	static PangoWrapMode s_eWordWrapMode;
	
	PangoLayout *m_pLayout;
	int	m_nTextChars;
	bool m_bEllipsisNoWrap;
	int m_iWidth;
    int m_iFirstLineHeightAdjust;
	float m_flMaxHeight;
	float m_flLineSpacing;
	float m_flLineSpacingAscent;
	int m_nLetterSpacing;
	EFontStyle m_eFontStyle;
	ETextAlign m_eAlign;
    // If the primary font can be resolved to a FontConfig
    // font we'll keep a pointer around for extra
    // metrics information.
    PangoFcFont *m_pPangoFcFont;
};

} // namespace panorama

#endif // UITEXTLAYOUTPANGO_H
