//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUIRENDERENGINE_H
#define IUIRENDERENGINE_H
#pragma once

#include "panoramatypes.h"
#include "panorama/renderer/rendercommands.h"
#include "tier1/refcount.h"
#include "iuirenderdevice.h"
#if defined( SOURCE2_PANORAMA )
class IRenderContext;
#endif


class Vector4D;
namespace panorama
{

//
// Render thread callback object interface
//
#if defined( SOURCE2_PANORAMA )
class CRenderThreadCallback : public CRefCount
{
public:

	// Callback function to override and perform direct rendering within
// re-define the signature for S1 Panorama, can't use PANORAMA_USE_S1WRAPPER as the client code is reaching here too. This is likely a unique case.
//	virtual void RenderThreadCallback(ISceneView *pSceneView, IRenderContext **pRenderContext, ISceneLayer *pSceneLayer, float x0, float y0, float x1, float y1) = 0;
	virtual void RenderThreadCallback( Vector4D *pScissorRect, float x0, float y0, float x1, float y1, bool bEnableSSAA ) = 0;
};
#endif

//-----------------------------------------------------------------------------
// Purpose: Interface to do drawing onto windows.  This is the full publically exposed
// drawing interface that new panel types can use for drawing, the implementation has
// more stuff available inside the framework.
//-----------------------------------------------------------------------------
class IUIRenderEngine
{
public:

	// Draw a filled quad with a single color that doesn't transition
	virtual void DrawSolidColorRect( float x0, float y0, float x1, float y1, uint32 unColorRGBA, EAntialiasing antialiasing = k_EAntialisingEnabled ) = 0;

	// Draw a filled quad. It is the caller's responsibility to fill in the returned FillBrushCollectionWithTransition_t with the appropriate colors.
	virtual FillBrushCollectionWithTransition_t *DrawFilledRect( uint64 unContextID, float x0, float y0, float x1, float y1, EAntialiasing antialiasing = k_EAntialisingEnabled ) = 0;

	// Draw a textured quad
	virtual void DrawTexturedRect( IUITexture *pTexture, ETextureSampleMode eSampleMode, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1 ) = 0;

	// Draw text. Versions for UTF8, UTF16, and UTF32. It is the caller's responsibility to fill out default_format.fill_brush_collection and optionally range_formats
	virtual DrawTextRegionRenderCommand_t *DrawTextRegion( const char *pchText, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 ) = 0;
	virtual DrawTextRegionRenderCommand_t *DrawTextRegion( const uchar16 *pchText, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 ) = 0;
	virtual DrawTextRegionRenderCommand_t *DrawTextRegion( const uchar32 *pchText, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 ) = 0;

	// Draw solid colored text that doesn't have any animation/transition
	virtual void DrawSolidColorTextRegion( const char *pchText, const char *pchFontName, uint32 unColor, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 ) = 0;
	virtual void DrawSolidColorTextRegion( const uchar16 *pchText, const char *pchFontName, uint32 unColor, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 ) = 0;
	virtual void DrawSolidColorTextRegion( const uchar32 *pchText, const char *pchFontName, uint32 unColor, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 ) = 0;

	// Draw one of the special syncronized textures
	virtual void DrawSyncronizedTexturedRect( IUITexture *pTexture, ETextureSampleMode eSampleMode, int32 unSerialize, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1 ) = 0;

#if defined( SOURCE2_PANORAMA )
	// Tell the render thread to call the panel back on the specified method, which will then be able to do 
	// direct render system calls on the render thread
	virtual void RequestRenderCallback( CRenderThreadCallback *pCallbackObj, float x0, float y0, float x1, float y1,
		float flPaddingLeft, float flPaddingRight, float flPaddingTop, float flPaddingBottom, ERenderCallbackFlags eFlags, IUITexture *pPanelRT ) = 0;
#endif
	
	virtual void UpdateSteamPadPointers( SteamPadPointer_t* leftPointer, SteamPadPointer_t* rightPointer ) = 0;
	
	//virtual void PushBlurPanels( const CUtlVector<uint64> &blurPanels ) = 0;
	virtual IUIPanel * HitTestCoordsAgainstLatestScreenspaceQuadCoordinates( float xSurface, float ySurface ) = 0;


	// When painting, use this command list as the allocator for render commands
	virtual CRenderCommandList &GetCommandList() = 0;

	virtual void PushBlurPanels( const CUtlVector<uint64> &blurPanels ) = 0;
};

}
#endif // IUIRENDERENGINE_H
