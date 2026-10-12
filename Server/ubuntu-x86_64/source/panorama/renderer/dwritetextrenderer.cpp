//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "d3d10d2dsurface.h"
#define INITGUID 1 
#include "D3Dcommon.h"
#include "panorama/layout/csshelpers.h"
#include "color.h"
#include "tier1/checksum_crc.h"
#include "uienginewin32.h"

// Needed for D3DPERF_ calls
#include "d3d9.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextDrawingEffect::CTextDrawingEffect( int iColorIndex )
{
	m_cRefCount = 1;
	m_iColorIndex = iColorIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTextDrawingEffect::~CTextDrawingEffect()
{
}

//-----------------------------------------------------------------------------
// Purpose: Increment object ref
//-----------------------------------------------------------------------------
STDMETHODIMP_(unsigned long) CTextDrawingEffect::AddRef()
{
	return ++m_cRefCount;
}


//-----------------------------------------------------------------------------
// Purpose: Decrement object ref
//-----------------------------------------------------------------------------
STDMETHODIMP_(unsigned long) CTextDrawingEffect::Release()
{
	m_cRefCount--;
	if ( m_cRefCount == 0 )
	{
		delete this;
		return 0;
	}

	return m_cRefCount;
}


//-----------------------------------------------------------------------------
// Purpose: Query interface implementation
//-----------------------------------------------------------------------------
STDMETHODIMP CTextDrawingEffect::QueryInterface( IID const& riid, void** ppvObject )
{
if (__uuidof(IUnknown) == riid)
	{
		*ppvObject = this;
	}
	else
	{
		*ppvObject = NULL;
		return E_FAIL;
	}

	AddRef();
	return S_OK;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDWriteTextRenderer::CDWriteTextRenderer( CD3D10D2DSurface *pSurface )
{
	m_cRefCount = 1;
	m_pSurface = pSurface;
	m_bDrawingEllipsis = false;
	m_pCurrentDrawRanges = false;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDWriteTextRenderer::~CDWriteTextRenderer()
{
}


//-----------------------------------------------------------------------------
// Purpose: Increment object ref
//-----------------------------------------------------------------------------
STDMETHODIMP_(unsigned long) CDWriteTextRenderer::AddRef()
{
	return ++m_cRefCount;
}


//-----------------------------------------------------------------------------
// Purpose: Decrement object ref
//-----------------------------------------------------------------------------
STDMETHODIMP_(unsigned long) CDWriteTextRenderer::Release()
{
	m_cRefCount--;
	if ( m_cRefCount == 0 )
	{
		delete this;
		return 0;
	}

	return m_cRefCount;
}


//-----------------------------------------------------------------------------
// Purpose: Query interface implementation
//-----------------------------------------------------------------------------
STDMETHODIMP CDWriteTextRenderer::QueryInterface( IID const& riid, void** ppvObject )
{
	if (__uuidof(IDWriteTextRenderer) == riid)
	{
		*ppvObject = this;
	}
	else if (__uuidof(IDWritePixelSnapping) == riid)
	{
		*ppvObject = this;
	}
	else if (__uuidof(IUnknown) == riid)
	{
		*ppvObject = this;
	}
	else
	{
		*ppvObject = NULL;
		return E_FAIL;
	}

	AddRef();
	return S_OK;
}


//-----------------------------------------------------------------------------
// Purpose: Determines whether pixel snapping is disabled. The recommended default is FALSE, unless doing animation that requires subpixel vertical placement.
//-----------------------------------------------------------------------------
STDMETHODIMP CDWriteTextRenderer::IsPixelSnappingDisabled( __maybenull void* clientDrawingContext, __out BOOL* isDisabled )
{
	*isDisabled = FALSE;
	return S_OK;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the current transform applied to the render target.
//-----------------------------------------------------------------------------
STDMETHODIMP CDWriteTextRenderer::GetCurrentTransform(	__maybenull void* clientDrawingContext,	__out DWRITE_MATRIX* transform)
{
	// forward the render target's transform
	ID2D1RenderTarget *pTarget = m_pSurface->AccessD2DRenderTarget();
	if ( !pTarget )
		return E_FAIL;

	pTarget->GetTransform( reinterpret_cast<D2D1_MATRIX_3X2_F*>( transform ) );
	return S_OK;
}


//-----------------------------------------------------------------------------
// Purpose: This returns the number of pixels per DIP.
//-----------------------------------------------------------------------------
STDMETHODIMP CDWriteTextRenderer::GetPixelsPerDip( __maybenull void* clientDrawingContext, __out FLOAT* pixelsPerDip )
{
	ID2D1RenderTarget *pTarget = m_pSurface->AccessD2DRenderTarget();
	if ( !pTarget )
		return E_FAIL;

	float x, y;
	pTarget->GetDpi( &x, &y );
	*pixelsPerDip = x / 96.0f;

	return S_OK;
}


//-----------------------------------------------------------------------------
// Purpose: Gets GlyphRun outlines via IDWriteFontFace::GetGlyphRunOutline
//			and then draws and fills them using Direct2D path geometries
//-----------------------------------------------------------------------------
STDMETHODIMP CDWriteTextRenderer::DrawGlyphRun( __maybenull void* clientDrawingContext,	FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_MEASURING_MODE measuringMode, __in DWRITE_GLYPH_RUN const* glyphRun, __in DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription, IUnknown* clientDrawingEffect )
{
	const float flPadding = 1;
	if ( !clientDrawingContext )
		return E_FAIL;

	DWriteTextRendererContext_t *pContext = (DWriteTextRendererContext_t*)clientDrawingContext;

	// get character indexes for the text we are going to draw
	uint iFirstChar = glyphRunDescription->textPosition;
	uint iLastChar = glyphRunDescription->textPosition + glyphRunDescription->stringLength - 1;

	// don't draw text if clipped
	if ( iLastChar < pContext->m_textClip.iFirstChar || iFirstChar > pContext->m_textClip.iLastChar )
		return S_OK;

	D2D1_RECT_F rectPadding;
	UITextOpacityMaskDataRange_t *pRangeData = NULL;
	if ( m_bDrawingEllipsis )
	{
		// should try to render only 1 unicode char
		if ( !glyphRun || !glyphRun->fontFace || glyphRun->glyphCount != 1 )
			return E_FAIL;

		// need height we are going to draw the ellipse with
		DWRITE_FONT_METRICS fontMetrics;
		glyphRun->fontFace->GetMetrics( &fontMetrics );
		float flFontScale = glyphRun->fontEmSize / fontMetrics.designUnitsPerEm;

		DWRITE_GLYPH_METRICS glyphMetrics;
		HRESULT hr = glyphRun->fontFace->GetDesignGlyphMetrics( glyphRun->glyphIndices, glyphRun->glyphCount, &glyphMetrics, glyphRun->isSideways );
		if ( FAILED( hr ) )
			return E_FAIL;

		float flEllipsisWidth = glyphMetrics.advanceWidth * flFontScale;
		float flEllipsisHeight = glyphMetrics.advanceHeight * flFontScale;

		D2D1_SIZE_U alphaTargetSize = D2D1::SizeU( ceil( flEllipsisWidth ) + flPadding * 2, ceil( flEllipsisHeight ) + flPadding * 2 );
		UITextTextureRegion_t alphaTarget = m_pSurface->GetTextureRegion( alphaTargetSize.width, alphaTargetSize.height );

		// Probably tried to draw something way too wide, we should assert in the layout thread about that and catch the source of such errors
		if ( alphaTarget.m_hTexture == NULL )
			return E_FAIL;

		float flXOffset = alphaTarget.m_rect.m_iLeft + flPadding;
		float flYOffset = alphaTarget.m_rect.m_iTop + flPadding;

		pRangeData = &(m_pCurrentDrawRanges->Element(m_pCurrentDrawRanges->AddToTail()));
		pRangeData->m_hTexture = (ID2D1BitmapRenderTarget*)alphaTarget.m_hTexture;
		pRangeData->m_x0 = flXOffset;
		pRangeData->m_y0 = flYOffset;
		pRangeData->m_x1 = flXOffset + flEllipsisWidth;
		pRangeData->m_y1 = flYOffset + flEllipsisHeight;		
		pRangeData->m_flStringOffsetX = baselineOriginX;
		pRangeData->m_flStringOffsetY = baselineOriginY - (fontMetrics.ascent * flFontScale);
		pRangeData->m_iColorIndex = -1;								// draw ellipsis with default color

		rectPadding.left = alphaTarget.m_rect.m_iLeft;
		rectPadding.top = alphaTarget.m_rect.m_iTop;
		rectPadding.right = alphaTarget.m_rect.m_iRight;
		rectPadding.bottom = alphaTarget.m_rect.m_iBottom;
	}
	else	
	{	
		if ( !clientDrawingEffect )
			return E_FAIL;

		CTextDrawingEffect *pEffect = (CTextDrawingEffect*)clientDrawingEffect;
	
		// bugbug - adding 1 because of bug in GetCharacterRangeCoordinates
		CUtlVector<IUITextLayout::HitTestRegionRect_t> vecHitTestRegions;
		pContext->m_pTextLayout->GetCharacterRangeCoordinates( iFirstChar, iLastChar + 1, vecHitTestRegions );

		// create a place to draw each region
		FOR_EACH_VEC( vecHitTestRegions, iHitTestRegion )
		{
			IUITextLayout::HitTestRegionRect_t &hitTestRect = vecHitTestRegions[iHitTestRegion];
			float flHitTestWidth = hitTestRect.bottomRight.x - hitTestRect.topLeft.x;
			float flHitTestHeight = hitTestRect.bottomRight.y - hitTestRect.topLeft.y;

			// skip any regions which are not going to be drawn
			if ( !hitTestRect.bIsText || hitTestRect.bIsTrimmed || flHitTestWidth <= 0.0 || flHitTestHeight <= 0.0 )
				continue;
			
			D2D1_SIZE_U alphaTexPixelSize = D2D1::SizeU( ceil( flHitTestWidth ) + flPadding * 2, ceil( flHitTestHeight ) + flPadding * 2 );
			UITextTextureRegion_t alphaTarget = m_pSurface->GetTextureRegion( alphaTexPixelSize.width, alphaTexPixelSize.height );

			// Probably tried to draw something way too wide, we should assert in the layout thread about that and catch the source of such errors
			if ( alphaTarget.m_hTexture == NULL )
				return E_FAIL;

			float flXOffset = alphaTarget.m_rect.m_iLeft + flPadding;
			float flYOffset = alphaTarget.m_rect.m_iTop + flPadding;

			int iRangeData = m_pCurrentDrawRanges->AddToTail();
			pRangeData = &(m_pCurrentDrawRanges->Element( iRangeData ));
			pRangeData->m_hTexture = (ID2D1BitmapRenderTarget*)alphaTarget.m_hTexture;
			((ID2D1BitmapRenderTarget*)pRangeData->m_hTexture)->SetTextRenderingParams( m_pSurface->AccessDWriteRenderingParams() );
			pRangeData->m_x0 = flXOffset;
			pRangeData->m_x1 = flXOffset + flHitTestWidth;
			pRangeData->m_y0 = flYOffset;
			pRangeData->m_y1 = flYOffset + flHitTestHeight;
			pRangeData->m_flStringOffsetX = hitTestRect.topLeft.x;
			pRangeData->m_flStringOffsetY = hitTestRect.topLeft.y;
			pRangeData->m_iColorIndex = pEffect->GetColorIndex();

			rectPadding.left = alphaTarget.m_rect.m_iLeft;
			rectPadding.top = alphaTarget.m_rect.m_iTop;
			rectPadding.right = alphaTarget.m_rect.m_iRight;
			rectPadding.bottom = alphaTarget.m_rect.m_iBottom;
			
			pEffect->AddRangeDataIndex( iRangeData );

			// throwing out other data.. hope this works
			break;
		}
	}

	// if no effect specified, the text was clipped
	if ( !pRangeData )
		return S_OK;

	ID2D1BitmapRenderTarget *pAlphaOnlyTarget = (ID2D1BitmapRenderTarget*)pRangeData->m_hTexture;

	// call begin draw only once per target.. when EndDraw() is called we will call end once
	if ( !m_treeActiveRenderTargets.HasElement( pAlphaOnlyTarget ) )
	{
		pAlphaOnlyTarget->AddRef();
		m_treeActiveRenderTargets.Insert( pAlphaOnlyTarget );

		pAlphaOnlyTarget->BeginDraw();
	}

	ID2D1Brush *pBlackBrush = m_pSurface->GetSolidColorBrush( 0xffffffff );	
	pAlphaOnlyTarget->PushAxisAlignedClip( rectPadding, D2D1_ANTIALIAS_MODE_ALIASED );
	pAlphaOnlyTarget->Clear( D2D1::ColorF( D2D1::ColorF::Black, 0.0f ) );

	D2D1_POINT_2F origin = { baselineOriginX + pRangeData->m_x0 - pRangeData->m_flStringOffsetX, baselineOriginY + pRangeData->m_y0 - pRangeData->m_flStringOffsetY };
	pAlphaOnlyTarget->DrawGlyphRun( origin, glyphRun, pBlackBrush, measuringMode );
	pAlphaOnlyTarget->PopAxisAlignedClip();


	SAFE_RELEASE( pBlackBrush );

	return S_OK;
}


//-----------------------------------------------------------------------------
// Purpose: Draws underlines
//-----------------------------------------------------------------------------
STDMETHODIMP CDWriteTextRenderer::DrawUnderline( __maybenull void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, __in DWRITE_UNDERLINE const* underline, IUnknown* clientDrawingEffect )
{
	D2D1_RECT_F rect = D2D1::RectF( 0, underline->offset, underline->width, underline->offset + underline->thickness );
	
	if ( !clientDrawingEffect )
		return E_FAIL;

	DWriteTextRendererContext_t *pContext = (DWriteTextRendererContext_t*)clientDrawingContext;
	CTextDrawingEffect *pEffect = (CTextDrawingEffect*)clientDrawingEffect;
	
	return DrawUnderlineOrStrikethrough( pContext, pEffect, rect, baselineOriginX, baselineOriginY );	
}


//-----------------------------------------------------------------------------
// Purpose: Draws strikethrough
//-----------------------------------------------------------------------------
STDMETHODIMP CDWriteTextRenderer::DrawStrikethrough( __maybenull void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, __in DWRITE_STRIKETHROUGH const* strikethrough, IUnknown* clientDrawingEffect )
{
	D2D1_RECT_F rect = D2D1::RectF( 0, strikethrough->offset, strikethrough->width, strikethrough->offset + strikethrough->thickness );

	if ( !clientDrawingEffect )
		return E_FAIL;

	DWriteTextRendererContext_t *pContext = (DWriteTextRendererContext_t*)clientDrawingContext;
	CTextDrawingEffect *pEffect = (CTextDrawingEffect*)clientDrawingEffect;

	return DrawUnderlineOrStrikethrough( pContext, pEffect, rect, baselineOriginX, baselineOriginY );	
}


//-----------------------------------------------------------------------------
// Purpose: Helper to draw underline and strikethrough
//-----------------------------------------------------------------------------
HRESULT CDWriteTextRenderer::DrawUnderlineOrStrikethrough( DWriteTextRendererContext_t *pContext, CTextDrawingEffect *pEffect, D2D1_RECT_F &rect, FLOAT baselineOriginX, FLOAT baselineOriginY )
{
	// need to find the appropriate section of the texture to draw this line into. Could have multiple ranges if the draw effect was applied to text that wrapped.
	FOR_EACH_VEC( pEffect->GetRangeDataIndexes(), i )
	{
		int iRangeData = pEffect->GetRangeDataIndexes().Element( i );

		UITextOpacityMaskDataRange_t *pRangeData = NULL;
		if ( m_pCurrentDrawRanges->IsValidIndex( iRangeData ) )
			pRangeData = &(m_pCurrentDrawRanges->Element(iRangeData));

		if ( !pRangeData )
			return E_FAIL;

		// check if point is within the bounds of this range
		float flRangeWidth = pRangeData->m_x1 - pRangeData->m_x0;
		float flRangeHeight = pRangeData->m_y1 - pRangeData->m_y0;
		if (	baselineOriginX < pRangeData->m_flStringOffsetX || baselineOriginX >= (pRangeData->m_flStringOffsetX + flRangeWidth) ||
				baselineOriginY < pRangeData->m_flStringOffsetY || baselineOriginY >= (pRangeData->m_flStringOffsetY + flRangeHeight ) )
		{
			continue;
		}

		// see if our underline is within the range
		HRESULT hr = DrawRect( (ID2D1BitmapRenderTarget*)pRangeData->m_hTexture, rect, baselineOriginX + pRangeData->m_x0 - pRangeData->m_flStringOffsetX, baselineOriginY + pRangeData->m_y0 - pRangeData->m_flStringOffsetY );
		if ( FAILED( hr ) )
			return hr;
	}

	return S_OK;
}

//-----------------------------------------------------------------------------
// Purpose: Draws a rectangle
//-----------------------------------------------------------------------------
HRESULT CDWriteTextRenderer::DrawRect( ID2D1BitmapRenderTarget *pRenderTarget, D2D1_RECT_F rectTarget, FLOAT baselineOriginX, FLOAT baselineOriginY )
{
	if ( !pRenderTarget )
		return E_NOTIMPL;

	ID2D1RectangleGeometry* pRectangleGeometry = NULL;
	HRESULT hr = m_pSurface->AccessD2D1Factory()->CreateRectangleGeometry( &rectTarget, &pRectangleGeometry );

	// Initialize a matrix to translate the origin of the underline
	D2D1::Matrix3x2F const matrix = D2D1::Matrix3x2F( 1.0f, 0.0f, 0.0f, 1.0f, baselineOriginX, baselineOriginY );

	ID2D1TransformedGeometry* pTransformedGeometry = NULL;
	if ( SUCCEEDED(hr) )
		hr = m_pSurface->AccessD2D1Factory()->CreateTransformedGeometry( pRectangleGeometry, &matrix, &pTransformedGeometry );

	if ( FAILED( hr ) )
		return hr;
 
	// Draw outline & fill
	ID2D1Brush *pBlackBrush = m_pSurface->GetSolidColorBrush( 0xffffffff );
	DbgAssert( m_treeActiveRenderTargets.HasElement( pRenderTarget ) );
	pRenderTarget->DrawGeometry( pTransformedGeometry, pBlackBrush );
	pRenderTarget->FillGeometry( pTransformedGeometry, pBlackBrush );

	SAFE_RELEASE( pBlackBrush );
	SAFE_RELEASE( pRectangleGeometry );
	SAFE_RELEASE( pTransformedGeometry );

	return S_OK;
}


//-----------------------------------------------------------------------------
// Purpose: Draws inline objects (like images), not implemented in our renderer
//-----------------------------------------------------------------------------
STDMETHODIMP CDWriteTextRenderer::DrawInlineObject( __maybenull void* clientDrawingContext, FLOAT originX, FLOAT originY, IDWriteInlineObject* inlineObject, BOOL isSideways, BOOL isRightToLeft, IUnknown* clientDrawingEffect	)
{
	if ( !clientDrawingContext || !inlineObject )
		return E_FAIL;

	DWriteTextRendererContext_t *pContext = (DWriteTextRendererContext_t*)clientDrawingContext;

	DWRITE_TRIMMING trimming;
	IDWriteInlineObject *pOurTrimmingObject = NULL;
	HRESULT hr = pContext->m_pTextLayout->GetDWriteTextLayout()->GetTrimming( &trimming, &pOurTrimmingObject );
	if ( !SUCCEEDED( hr ) )
		return E_FAIL;

	// if not our ellipsis object, return not implemented
	if( pOurTrimmingObject != inlineObject )
	{
		SAFE_RELEASE( pOurTrimmingObject );
		return E_NOTIMPL;
	}
	SAFE_RELEASE( pOurTrimmingObject );
	
	m_bDrawingEllipsis = true;
	hr = inlineObject->Draw( clientDrawingContext, this, originX, originY, isSideways, isRightToLeft, NULL );
	m_bDrawingEllipsis = false;

	return hr;
}


//-----------------------------------------------------------------------------
// Purpose: Called before rendering
//-----------------------------------------------------------------------------
void CDWriteTextRenderer::BeginDraw( CUtlVector<UITextOpacityMaskDataRange_t> *pDrawRanges )
{
	DbgAssert( m_pCurrentDrawRanges == NULL );
	DbgAssert( m_treeActiveRenderTargets.Count() == 0 );

	m_pCurrentDrawRanges = pDrawRanges;
}


//-----------------------------------------------------------------------------
// Purpose: Called after rendering
//-----------------------------------------------------------------------------
void CDWriteTextRenderer::EndDraw()
{
	// call end draw on any open targets
	FOR_EACH_RBTREE_FAST( m_treeActiveRenderTargets, i )
	{
		m_treeActiveRenderTargets.Element( i )->EndDraw();
		SAFE_RELEASE( m_treeActiveRenderTargets.Element( i ) );
	}

	m_treeActiveRenderTargets.RemoveAll();
	m_pCurrentDrawRanges = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Validation
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CDWriteTextRenderer::Validate( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_treeActiveRenderTargets );
}
#endif
