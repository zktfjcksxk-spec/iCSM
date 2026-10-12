//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UITEXTLAYOUTOSX_H
#define UITEXTLAYOUTOSX_H

#include "panorama/text/iuitextlayout.h"
#include "tier0/threadtools.h"
#include "tier1/utlsortvector.h"
#include <Carbon/Carbon.h>

namespace panorama
{

class COpenGLSurface;
//-----------------------------------------------------------------------------
// Purpose: Contains all info related to a span of text w/ alpha mask previously rendered to texture
//-----------------------------------------------------------------------------
struct GLTextOpacityMaskDataRange_t
{
	static const int k_iColorIndexUnset = -1;
	
	uint m_textureId;
	float m_texWidth;
	float m_texHeight;

	float m_x0;
	float m_y0;
	float m_x1;
	float m_y1;
	float m_flStringOffsetX;		// Start X pos relative to top left of complete rendered text
	float m_flStringOffsetY;		// Start Y pos relative to top left of complete rendered text
	int m_iColorIndex;				// index of color into provided brushes. -1 if unset
};


//
// Interface that needs to be implemented for text layout on all platforms
//
class CUITextLayoutOSX : public IUITextLayout
{
public:

	CUITextLayoutOSX();
	~CUITextLayoutOSX();

	static bool BInitGlobals();
	static void FreeGlobals();

	bool BInitialize( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const IUITextServices::TextLayoutParams_t *pParams );
	
	// IUITextLayout implementation
	void GetRequiredSize( float &flWidth, float &flHeight );
	void HitTestPoint( Vector2D point, uint32 &unFirstHitOffset, bool &bIsTrailingHit, bool &bIsInsideString );
	void GetCharacterCoordinates( uint32 unCharIndex, IUITextLayout::HitTestRegionRect_t &charRegionRect );
	void GetCharacterRangeCoordinates( uint32 unCharStartIndex, uint32 unCharEndIndex, CUtlVector<IUITextLayout::HitTestRegionRect_t> &vecRangeRegionRects );

	// Set formatting for character ranges
	virtual void SetFontName( uint32 unCharStartIndex, uint32 unCharEndIndex, const char *pchFontName );
	virtual void SetFontSize( uint32 unCharStartIndex, uint32 unCharEndIndex, float flFontSize );
	virtual void SetFontStyle( uint32 unCharStartIndex, uint32 unCharEndIndex, EFontStyle eFontStyle );
	virtual void SetFontWeight( uint32 unCharStartIndex, uint32 unCharEndIndex, EFontWeight eFontWeight );
	virtual void SetUnderline( uint32 unCharStartIndex, uint32 unCharEndIndex, bool bUnderline );
	virtual void SetStrikethrough( uint32 unCharStartIndex, uint32 unCharEndIndex, bool bStrikethrough );
	virtual void MarkColorRangeForMeasurement( uint32 unCharStartIndex, uint32 unCharEndIndex, int iColorIndex );

	bool Draw( CUtlVector<GLTextOpacityMaskDataRange_t> &drawRanges, const google::protobuf::RepeatedPtrField<CMsgRenderTextRangeFormat > &rangeFormats, COpenGLSurface *pSurface );

	static const CUtlSortVector< CUtlString > &GetSortedValidFontNames();

	static bool BLoadCustomFontCollection( const char *pchPathForCustomFonts );
	static bool BIsOSX107OrAbove();
private:
	bool CharacterCoordinatesHelper( uint32 unCharIndexStart, uint32 unCharIndexEnd, CUtlVector<IUITextLayout::HitTestRegionRect_t> &vecRangeRegionRects );
	CTFrameRef CreateFrameHelper( bool bUseMaxSize, float &flFrameSize, CGSize *pFrameSize = NULL );

	CFMutableAttributedStringRef m_attrString;
	CTFramesetterRef m_ctfFrameSetter;
	float m_flMaxWidth;
	float m_flMaxHeight;
	float m_flFontSize;
	size_t m_cchText;
	CUtlString m_sFontName;
	bool m_bEllipseOnTruncate;
	EFontWeight m_Weight;
	EFontStyle m_Style;
	bool m_bWrap;

	static CUtlSortVector< CUtlString > m_vecSortedValidFontNames;
};

} // namespace panorama

#endif // UITEXTLAYOUTOSX_H
