//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef DWRITETEXTRENDERER_H
#define DWRITETEXTRENDERER_H

#ifdef _WIN32
#pragma once
#endif

#include <WinSock2.h>
#include <windows.h>
#include "iui3dsurface.h"
#include "mathlib/vmatrix.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlmap.h"
#include "tier1/utllinkedlist.h"
#include "../text/uitextlayoutwin32.h"
#include "../input/mousecursor.h"

#include "tier0/memdbgoff.h"
#include <d3dcommon.h>
#include <d3d10_1.h>
#include <d2d1.h>
#include <DWrite.h>
#include "tier0/memdbgon.h"


namespace panorama
{

	class CD3D10D2DSurface;



//-----------------------------------------------------------------------------
// Purpose: Custom drawing effect that tells our text renderer where to draw
//-----------------------------------------------------------------------------
class CTextDrawingEffect : public IUnknown
{
public:
	CTextDrawingEffect( int iColorIndex );		
	~CTextDrawingEffect();

	// IUnknown
	STDMETHOD_(unsigned long, AddRef)();
	STDMETHOD(QueryInterface)( IID const& riid, void** ppvObject );
	STDMETHOD_(unsigned long, Release)();

	// CTextDrawingEffect		
	int GetColorIndex() { return m_iColorIndex; }
	void AddRangeDataIndex( int iRangeData ) { m_vecRangeData.AddToTail( iRangeData ); }
	const CUtlVector< int > &GetRangeDataIndexes() { return m_vecRangeData; }

private:
	unsigned long m_cRefCount;
	int m_iColorIndex;
	CUtlVector< int > m_vecRangeData;
};


//-----------------------------------------------------------------------------
// Purpose: Results of BCalculateTextClip()
//-----------------------------------------------------------------------------
struct CalculatedTextClip_t
{
	uint iFirstChar;
	uint iLastChar;
	float flFirstLineOffset;
	float flLastLineOffset;
};


//-----------------------------------------------------------------------------
// Purpose: Struct passed as the client drawing context to our dwrite text renderer
//-----------------------------------------------------------------------------
struct DWriteTextRendererContext_t
{
	CUITextLayoutWin32 *m_pTextLayout;
	CalculatedTextClip_t m_textClip;
};


//-----------------------------------------------------------------------------
// Purpose: Custom texture renderer so we can split drawing text into different opacity masks so custom colors can be applied
//			to each mask.
//-----------------------------------------------------------------------------
class CDWriteTextRenderer : public IDWriteTextRenderer
{
public:
	CDWriteTextRenderer( CD3D10D2DSurface *pSurface );
	~CDWriteTextRenderer();

	// IUnknown
	STDMETHOD_(unsigned long, AddRef)();
	STDMETHOD(QueryInterface)( IID const& riid, void** ppvObject );
	STDMETHOD_(unsigned long, Release)();

	// IDWritePixelSnapping
	STDMETHOD(GetCurrentTransform)( void* clientDrawingContext, DWRITE_MATRIX* transform );
	STDMETHOD(GetPixelsPerDip)( void* clientDrawingContext, FLOAT* pixelsPerDip	);
	STDMETHOD(IsPixelSnappingDisabled)(	void* clientDrawingContext, BOOL* isDisabled );

	// IDWriteTextRenderer
	STDMETHOD(DrawGlyphRun)( void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_MEASURING_MODE measuringMode, DWRITE_GLYPH_RUN const* glyphRun, DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription, IUnknown* clientDrawingEffect );
	STDMETHOD(DrawInlineObject)( void* clientDrawingContext, FLOAT originX, FLOAT originY, IDWriteInlineObject* inlineObject, BOOL isSideways, BOOL isRightToLeft, IUnknown* clientDrawingEffect );
	STDMETHOD(DrawStrikethrough)( __maybenull void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, __in DWRITE_STRIKETHROUGH const* strikethrough, IUnknown* clientDrawingEffect );
	STDMETHOD(DrawUnderline)( __maybenull void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, __in DWRITE_UNDERLINE const* underline, IUnknown* clientDrawingEffect );	

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

	void BeginDraw( CUtlVector<UITextOpacityMaskDataRange_t> *pDrawRanges );
	void EndDraw();

private:
	HRESULT DrawUnderlineOrStrikethrough( DWriteTextRendererContext_t *pContext, CTextDrawingEffect *pEffect, D2D1_RECT_F &rect, FLOAT baselineOriginX, FLOAT baselineOriginY );
	HRESULT DrawRect( ID2D1BitmapRenderTarget *pRenderTarget, D2D1_RECT_F rectTarget, FLOAT baselineOriginX, FLOAT baselineOriginY );
	unsigned long m_cRefCount;
	CD3D10D2DSurface *m_pSurface;
	bool m_bDrawingEllipsis;

	CUtlVector<UITextOpacityMaskDataRange_t> *m_pCurrentDrawRanges;

	CUtlRBTree< ID2D1BitmapRenderTarget*, int, CDefLess< ID2D1BitmapRenderTarget* > > m_treeActiveRenderTargets;
};

} // namespace panorama

#endif // D3D10SURFACE_H