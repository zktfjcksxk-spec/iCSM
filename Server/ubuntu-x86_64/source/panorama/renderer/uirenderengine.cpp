//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uirenderengine.h"
#include "iui3dsurface.h"

#ifndef OSX
#include "google/protobuf/descriptor.h"
#endif

#include "vstdlib/vstrtools.h"

#include <google/protobuf/text_format.h>
#include "tier1/utlstack.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

ConVar s_convarPanoramaTrackRenderCommands( "@panorama_track_render_commands", "0" );
extern ConVar s_convarSuspendPaint;

CThreadMutex CUIRenderEngine::s_CommandStatsLock;
RenderCommandListStats_t CUIRenderEngine::s_averagePaintCommandListStats;
RenderCommandListStats_t CUIRenderEngine::s_averageRenderCommandListStats;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
#ifdef _WIN32
#pragma warning( push )
#pragma warning( disable: 4355 ) // this' : used in base member initializer list
#endif
CUIRenderEngine::CUIRenderEngine( CUIEngine *pUIEngineParent, IUI3DSurface *pSurface, CUIWindowInput *pInput, CTopLevelWindow *pWindow, uint32 unSurfaceWidth, uint32 unSurfaceHeight ) : 
	m_RenderThread( this, pSurface ), 
	m_AnimationThread( this, pInput ), 
	m_pInput( pInput ), 
	m_pUIEngineParent( pUIEngineParent ),
	m_paintListsQueue( 0, 2 )
{
#if defined ( _WIN32 ) && !defined ( DX_TO_GL_ABSTRACTION )
	DbgVerify( TIMERR_NOERROR  == timeBeginPeriod( 1 ) );	// set timing granularity to 1 ms.  
#endif

	m_treePanelScreenspaceQuads = NULL;

	m_pCurrentPaintList = nullptr;
	m_pCurrentAnimationList = nullptr;
	m_pCurrentRenderList = nullptr;

	m_nLastFrameMillisecondsIndex = -1;
	for ( int i = 0; i < V_ARRAYSIZE( m_rgflMillisecondsFrame ); ++i )
	{
		m_rgflMillisecondsFrame[ i ] = FLT_MAX;
	}

	m_pWindow = pWindow;

#if !defined( SOURCE2_PANORAMA )
	m_AnimationThread.SetName( "UIEngineAnimationThread" );
	m_AnimationThread.Start();

	m_RenderThread.SetName( "UIEngineRenderThread" );
	m_RenderThread.Start();
#endif

	m_unSurfaceWidth = unSurfaceWidth;
	m_unSurfaceHeight = unSurfaceHeight;


#ifdef DBGFLAG_VALIDATE
	m_bRenderAndAnimationThreadPausedForValidate = false;
#endif

	m_nFramesRendered = 0;
	m_flFramePaintTime = 0.0f;
}
#ifdef _WIN32
#pragma warning( pop )
#endif


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUIRenderEngine::~CUIRenderEngine()
{
#if defined ( _WIN32 ) && !defined ( DX_TO_GL_ABSTRACTION )
	timeEndPeriod( 1 );
#endif

	m_AnimationThread.TriggerShutdown();
	m_AnimationThread.Join( 20 *k_nThousand );

#if !defined( SOURCE2_PANORAMA )
	m_RenderThread.TriggerShutdown();
	m_RenderThread.Join( 20 *k_nThousand );
#endif

	{
		AUTO_LOCK( m_MutexScreenspaceQuadTree );
		SAFE_DELETE( m_treePanelScreenspaceQuads );
	}

	SAFE_DELETE( m_pCurrentPaintList );
	SAFE_DELETE( m_pCurrentAnimationList );
	SAFE_DELETE( m_pCurrentRenderList );
}


//-----------------------------------------------------------------------------
// Purpose: Handle panel being deleted, need to let render thread know
//-----------------------------------------------------------------------------
void CUIRenderEngine::OnPanelDeleted( IUIPanel *pPanel )
{
	CPanelPtr<IUIPanel> safeptr( pPanel );
	m_vecDeletedPanelsThisFrame.AddToTail( safeptr.GetHandleAsUInt64() );
}


//-----------------------------------------------------------------------------
// Purpose: Handle window resize
//-----------------------------------------------------------------------------
void CUIRenderEngine::OnWindowResize( uint32 unSurfaceWidth, uint32 unSurfaceHeight )
{
	m_unSurfaceWidth = unSurfaceWidth;
	m_unSurfaceHeight = unSurfaceHeight;
}


//-----------------------------------------------------------------------------
// Purpose: Reset the swap buffers event
//-----------------------------------------------------------------------------
void CUIRenderEngine::ClearSwapBuffersEvent()
{
	m_SwapBuffersEvent.Reset();
}


//-----------------------------------------------------------------------------
// Purpose: Set the swap buffers event
//-----------------------------------------------------------------------------
void CUIRenderEngine::SetSwapBuffersEvent()
{
	m_SwapBuffersEvent.Set();
}


//-----------------------------------------------------------------------------
// Purpose: Wait on the swap buffers event
//-----------------------------------------------------------------------------
void CUIRenderEngine::WaitOnSwapBuffersEvent( float flMillisecondsTimeout )
{
	m_SwapBuffersEvent.Wait( flMillisecondsTimeout );
}


//-----------------------------------------------------------------------------
// Purpose: Wake animation/render threads if they are sleeping for fps limiting
//-----------------------------------------------------------------------------
void CUIRenderEngine::WakeThreads()
{
	m_AnimationThread.WakeThread();
#if !defined( SOURCE2_PANORAMA )
	m_RenderThread.WakeThread();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: have the animation thread wake up from sleep
//-----------------------------------------------------------------------------
void CUIRenderEngine::WakeAnimationThread()
{
	m_AnimationThread.WakeThread();
}


//-----------------------------------------------------------------------------
// Purpose: Start a new frame
//-----------------------------------------------------------------------------
void CUIRenderEngine::BeginFrame( uint32 unSurfaceWidth, uint32 unSurfaceHeight, IUIEngine::ERenderTarget eTarget, Color clearColor, float flUIScaleFactor, bool bEmptyFrame, bool bClearGPUResourcesBeforeFrame )
{
	VPROF_BUDGET( "CUIRenderEngine::BeginFrame", VPROF_BUDGETGROUP_TENFOOT );

	// We should have a fresh command buffer at this point
	Assert( m_pCurrentPaintList->m_pCommandList->GetFirstCommand() == nullptr );

	m_LastPaintFrameTime = Plat_FloatTime();

	BeginFrameRenderCommand_t *pBeginCommand = PushPaintCommand< BeginFrameRenderCommand_t >();
	pBeginCommand->frame_paint_time = m_LastPaintFrameTime;
	pBeginCommand->surface_width = unSurfaceWidth;
	pBeginCommand->surface_height = unSurfaceHeight;
	pBeginCommand->render_target = eTarget;
	pBeginCommand->ui_scale_factor = flUIScaleFactor;
	pBeginCommand->empty_frame = bEmptyFrame;
	pBeginCommand->clear_gpu_resources_before_frame = bClearGPUResourcesBeforeFrame;

	if ( !bEmptyFrame )
	{
		ClearBackbufferRenderCommand_t *pClearCommand = PushPaintCommand< ClearBackbufferRenderCommand_t >();
		pClearCommand->clear_color_rgba = clearColor.GetRawColor();
	}
}


//-----------------------------------------------------------------------------
// Purpose: End a frame
//-----------------------------------------------------------------------------
void CUIRenderEngine::EndFrame()
{
	VPROF_BUDGET( "CUIRenderEngine::EndFrame", VPROF_BUDGETGROUP_TENFOOT );

	FOR_EACH_VEC( m_vecDeletedPanelsThisFrame, i )
	{
		DeletePanelRenderCommand_t *pDeleteCommand = PushPaintCommand< DeletePanelRenderCommand_t >();
		pDeleteCommand->context_id = m_vecDeletedPanelsThisFrame[ i ];
	}
	m_vecDeletedPanelsThisFrame.RemoveAll();

	// Push any queued particle system deletes next
	FOR_EACH_VEC( m_vecParticleSystemsToDelete, i )
	{
		DeleteParticleSystemRenderCommand_t *pDeleteCommand = PushPaintCommand< DeleteParticleSystemRenderCommand_t >();
		pDeleteCommand->panel_handle = m_vecParticleSystemsToDelete[ i ].ulPanelHandle;
		pDeleteCommand->brush_index = m_vecParticleSystemsToDelete[ i ].unIndex;
	}
	m_vecParticleSystemsToDelete.RemoveAll();

	EndFrameRenderCommand_t *pEndFrameCommand = PushPaintCommand< EndFrameRenderCommand_t >();

	if ( m_pWindow->BUseCustomMouseCursor() )
	{
		// get the texture id for the mouse cursor
		Vector2D ptHotspot;
		IImageSource *pMouseImageSource = m_pWindow->GetMouseCursorTexture( &ptHotspot );
		if ( pMouseImageSource  && pMouseImageSource->BIsValid() )
		{
			pEndFrameCommand->mouse_cursor_texture.SetTexture( pMouseImageSource->GetTexture(), GetCurrentCommandList() );
			pEndFrameCommand->mouse_cursor_hotspot.x = ptHotspot.x;
			pEndFrameCommand->mouse_cursor_hotspot.y = ptHotspot.y;
		}
	}

	m_FrameTimer.End();

	m_nLastFrameMillisecondsIndex++;
	if ( m_nLastFrameMillisecondsIndex >= V_ARRAYSIZE( m_rgflMillisecondsFrame ) )
	{
		// accumulate these frametimes into the global counters
		for ( int i=0; i < V_ARRAYSIZE( m_rgflMillisecondsFrame ); ++i )
		{
			if ( m_rgflMillisecondsFrame[i] < 1000.0f && ( m_nFramesRendered == 0.0f  || Plat_IsInDebugSession() ) ) // ignore 1 sec or longer frames if in the debugger and the very first frame
				m_flFramePaintTime += m_rgflMillisecondsFrame[i];
		}
		m_nFramesRendered += m_nLastFrameMillisecondsIndex;
		m_nLastFrameMillisecondsIndex = 0;
	}

	m_rgflMillisecondsFrame[ m_nLastFrameMillisecondsIndex ] = m_FrameTimer.GetDuration().GetMillisecondsF();

	m_FrameTimer.Start();

/*
	if ( m_nLastFrameMillisecondsIndex == 0 )
	{
		float flPaint, flAnimate, flRender;
		GetFPSAverages( flPaint, flAnimate, flRender );
		Msg( "Framerates: Paint( %1.2f ), Animate( %1.2f ), Render( %1.2f )\n", flPaint, flAnimate, flRender );
	}
	*/

	// Push the current paint list on to queue for anim to process

	m_PaintingLock.Lock();
	
	m_paintListsQueue.Insert( m_pCurrentPaintList );
	m_pCurrentPaintList = nullptr;
	m_vecPaintCommandListStack.RemoveAll();
	
	// The max entries we should have in the paint list queue is 2, on a frame where the 
	// anim thread has not yet run by the time paint reaches this point
	if( m_paintListsQueue.Count() > 2 )
	{
		Warning( "CUIRenderEngine::EndFrame. Paint list queue is full\n" );
	}

	m_PaintingLock.Unlock();
}


//-----------------------------------------------------------------------------
// Purpose: Gets framerate averages for painting, animating, and render threads for the last few frames
//-----------------------------------------------------------------------------
void CUIRenderEngine::GetFPSAverages( float &fpsPaint, float &fpsAnimation, float &fpsRender )
{
	double flSum = 0.0f;
	int nDivisor = 0;
	for ( int i=0; i < V_ARRAYSIZE( m_rgflMillisecondsFrame ); ++i )
	{
		if ( m_rgflMillisecondsFrame[i] != FLT_MAX )
		{
			++nDivisor;
			flSum += m_rgflMillisecondsFrame[i];
		}
	}

	fpsPaint = 1000.0f / ((float)((double)flSum / (double)nDivisor));

	fpsAnimation = m_AnimationThread.GetFPSAverage();
	fpsRender = m_RenderThread.GetFPSAverage();
}


//-----------------------------------------------------------------------------
// Purpose: Gets framerate averages for painting, animating, and render threads across the whole process lifetime
//-----------------------------------------------------------------------------
void CUIRenderEngine::GetSessionFPSAverages( float &fpsPaint, float &fpsAnimation, float &fpsRender )
{
	fpsPaint = 1000.0f*m_nFramesRendered / m_flFramePaintTime;
	fpsAnimation = m_AnimationThread.GetSessionFPSAverages();
	fpsRender = m_RenderThread.GetSessionFPSAverages();
}


//-----------------------------------------------------------------------------
// Purpose: Gets framerate averages for painting, animating, and render threads across the whole process lifetime
//-----------------------------------------------------------------------------
void CUIRenderEngine::UpdatePanelScreenspaceQuadCoordinates( ScreenSpacePanelQuad_t *pTreeScreenspaceQuads )
{
	AUTO_LOCK( m_MutexScreenspaceQuadTree );
	if ( m_treePanelScreenspaceQuads )
	{
		SAFE_DELETE( m_treePanelScreenspaceQuads );
	}

	m_treePanelScreenspaceQuads = pTreeScreenspaceQuads;
}


//-----------------------------------------------------------------------------
// Purpose: Helper to recurse 
//-----------------------------------------------------------------------------
IUIPanel * HitTestCoordinatesAgainstQuadTree( ScreenSpacePanelQuad_t *pPanelQuadTree, float x, float y, bool bTraversePeers )
{
	if ( pPanelQuadTree == NULL )
		return NULL;

	bool bCurPanelPasses = false;

	// Check if the current panel to test against is an axis aligned quad, then the hit-test on it is simple
	Vector2D *pCurQuad = pPanelQuadTree->m_pQuad;
	if ( pCurQuad[0].x == pCurQuad[3].x
		&& pCurQuad[0].y == pCurQuad[1].y
		&& pCurQuad[1].x == pCurQuad[2].x
		&& pCurQuad[2].y == pCurQuad[3].y )
	{
		if ( x >= Min( pCurQuad[0].x, pCurQuad[2].x ) && x <= Max( pCurQuad[2].x, pCurQuad[0].x )
			&& y >= Min( pCurQuad[0].y, pCurQuad[2].y ) && y <= Max( pCurQuad[2].y, pCurQuad[0].y ) )
		{
			bCurPanelPasses = true;
		}
	}
	else
	{
		// Need to do more expensive hittest on non-axis aligned quad
		bCurPanelPasses = BPointInsideConvexQuad( x, y, pCurQuad );
	}

	if ( bCurPanelPasses )
	{
		CPanelPtr<IUIPanel> panel;
		panel.SetFromUInt64( pPanelQuadTree->m_ulPanelContextID );

		if ( !panel.Get() )
		{
			bCurPanelPasses = false;
		}
		else
		{
			IUIPanel *pChildPasses = HitTestCoordinatesAgainstQuadTree( pPanelQuadTree->m_pFirstChild, x, y, true );
			if ( pChildPasses )
				return pChildPasses;
			else
				return panel.Get();
		}
	}

	if ( !bCurPanelPasses && bTraversePeers )
	{
		// Traverse through peers now
		ScreenSpacePanelQuad_t *pNextPeer = pPanelQuadTree->m_pNextPeer;
		while ( pNextPeer )
		{
			IUIPanel *pPeerPasses = HitTestCoordinatesAgainstQuadTree( pNextPeer, x, y, false );
			if ( pPeerPasses )
				return pPeerPasses;

			pNextPeer = pNextPeer->m_pNextPeer;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Hit tests screen space coordinates against latest coordinates from render thread and returns 
// the panel that is hit
//-----------------------------------------------------------------------------
IUIPanel * CUIRenderEngine::HitTestCoordsAgainstLatestScreenspaceQuadCoordinates( float xSurface, float ySurface )
{
	AUTO_LOCK( m_MutexScreenspaceQuadTree );
	return HitTestCoordinatesAgainstQuadTree( m_treePanelScreenspaceQuads, xSurface, ySurface, true );
}


//-----------------------------------------------------------------------------
// Purpose:  Tell animation thread to stop further interpolating property and get back the time we stopped 
// it at for layout thread to finish transition in matching manner.
//-----------------------------------------------------------------------------
float CUIRenderEngine::StopAnimationOfPropertyUntilFrameUpdateAndGetStopTime( uint64 ulPanelContextID, uint32 hStyleSymbol )
{
	return m_AnimationThread.StopAnimationOfPropertyUntilFrameUpdateAndGetStopTime( ulPanelContextID, hStyleSymbol );
}

//-----------------------------------------------------------------------------
// Purpose:  Tell animation thread to stop further interpolating property and get back the time we stopped 
// it at for layout thread to finish transition in matching manner.
//-----------------------------------------------------------------------------
const char *CUIRenderEngine::CopyPaintString( const char *pszInput )
{
	return GetCurrentCommandList().CopyString( pszInput );
}

//-----------------------------------------------------------------------------
// Purpose: Pushes animation/transform context, ie, tells us we've descended into a 
// child context with a new set of transformations/animations that will apply to all 
// rendering operations until the corresponding pop call.
//-----------------------------------------------------------------------------
void CUIRenderEngine::PushAnimationAndTransformContext( uint64 unContextID, uint32 unChildPanelCount, uint32 unStylesPresentFlags, float wide, float tall, CPanelStyle *pStyle, bool bNoChildrenOutsideBounds, bool bChildrenHave3DTransforms,
	EPanelRepaint ePanelRepaint, bool bNoClip, bool bWantsHitTest, bool bWantsHitTestChildren, bool bNeedsIntermediateTexture, bool bClipAfterTransform, bool bWantsScreenspaceQuadOutput, const char *pszCompositionLayerTextureName,
	bool bRequireCompositionLayer, bool bForceNoCompositionLayer, bool bAlwaysCacheCompositionLayer, bool bOffscreenCompositionLayer, EFractionalPixelPositions eFractionalPixelPositions )
{
	VPROF_BUDGET_DETAILED( "CUIRenderEngine::PushAnimationAndTransformContext", VPROF_BUDGETGROUP_TENFOOT );

	PushAAndTContextRenderCommand_t *pCommand = PushPaintCommand< PushAAndTContextRenderCommand_t >();

	AssertMsg( IsFinite( wide ), "Invalid width pushed" );
	AssertMsg( IsFinite( tall ), "Invalid height pushed" );

	pCommand->context_id = unContextID;
	pCommand->width = wide;
	pCommand->height = tall;
	pCommand->has_children = unChildPanelCount > 0;
	pCommand->children_have_3dtransforms = bChildrenHave3DTransforms;
	pCommand->needs_full_repaint = ePanelRepaint;
	pCommand->wants_hit_test = bWantsHitTest;
	pCommand->wants_hit_test_children = bWantsHitTestChildren;
	pCommand->needs_intermediate_texture = bNeedsIntermediateTexture;
	pCommand->clip_after_transform = bClipAfterTransform;
	pCommand->wants_screenspace_quad_output = bWantsScreenspaceQuadOutput;
	pCommand->require_composition_layer = bRequireCompositionLayer;
	pCommand->always_cache_composition_layer = bAlwaysCacheCompositionLayer;
	pCommand->force_no_composition_layer = bForceNoCompositionLayer;
	pCommand->offscreen_composition_layer = bOffscreenCompositionLayer;
	pCommand->fractional_pixel_positions = eFractionalPixelPositions;

	if ( pszCompositionLayerTextureName )
		pCommand->composition_layer_texture_name = CopyPaintString( pszCompositionLayerTextureName );

	if ( bNoClip || ( bNoChildrenOutsideBounds && ( unStylesPresentFlags & k_EStylePresentBorderRadius ) == 0 ) )
		pCommand->suppress_clip_to_bounds = true;

	if ( pStyle )
	{
		pStyle->GetZIndex( pCommand->zindex );

		pStyle->GetRenderData( pCommand->panel_position, GetCurrentCommandList() );

		CRenderDataListBaseBuilder optionalPropertiesBuilder( pCommand->optional_properties, &GetCurrentCommandList() );

		if ( unStylesPresentFlags & k_EStylePresentClip )
			SetupOptionalPropertyPaintData< ClipWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_Clip, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentTransformMatrix )
			SetupOptionalPropertyPaintData< TransformMatrixWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_TransformMatrix, pStyle );

		// transform-origin uses the matrix flag because even the default must be pushed as it depends on widths only
		// available in the layout thread.  However, it's only ever used if we have a transform so it's cheap to always push
		// in this way rather than push more width data to animation.
		if ( unStylesPresentFlags & k_EStylePresentTransformMatrix )
			SetupOptionalPropertyPaintData< TransformOriginWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_TransformOrigin, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentPerspective )
			SetupOptionalPropertyPaintData< TransformPerspectiveWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_TransformPerspective, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentPerspectiveOrigin )
			SetupOptionalPropertyPaintData< TransformPerspectiveOriginWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_TransformPerspectiveOrigin, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentOpacity )
			SetupOptionalPropertyPaintData< OpacityWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_Opacity, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentScale2DCentered )
			SetupOptionalPropertyPaintData< Scale2DWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_Scale2D, pStyle );
			
		if ( unStylesPresentFlags & k_EStylePresentRotate2DCentered )
			SetupOptionalPropertyPaintData< Rotate2DWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_Rotate2D, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentWashColor )
			SetupOptionalPropertyPaintData< WashColorWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_WashColor, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentMixBlendMode )
			pCommand->mix_blend_mode = pStyle->GetMixBlendMode();

		if ( unStylesPresentFlags & k_EStylePresentHueShift )
			SetupOptionalPropertyPaintData< HueShiftWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_HueShift, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentSaturation )
			SetupOptionalPropertyPaintData< SaturationWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_Saturation, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentBrightness )
			SetupOptionalPropertyPaintData< BrightnessWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_Brightness, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentContrast )
			SetupOptionalPropertyPaintData< ContrastWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_Contrast, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentBlur )
			SetupOptionalPropertyPaintData< GaussianBlurWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_GaussianBlur, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentBorderRadius )
			SetupOptionalPropertyPaintData< BorderRadiusWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_BorderRadius, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentBorder )
			SetupOptionalPropertyPaintData< BorderWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_Border, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentBoxShadow )
			SetupOptionalPropertyPaintData< BoxShadowWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_BoxShadow, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentOpacityMaskImage )
			SetupOptionalPropertyPaintData< OpacityMaskWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_OpacityMask, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentTextShadow )
			SetupOptionalPropertyPaintData< TextShadowWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_TextShadow, pStyle );

		if ( unStylesPresentFlags & k_EStylePresentImageShadow )
			SetupOptionalPropertyPaintData< ImageShadowWithTransition_t >( optionalPropertiesBuilder, k_EStyleOptionalProperty_ImageShadow, pStyle );

		pCommand->opaque_background = pStyle->BHasConstantOpaqueBackground();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Pops animation/transform context, ie, tells us we've left a child context,
// and should clear the transforms/animations from that context and stop applying them 
// to future rendering operations.
//-----------------------------------------------------------------------------
void CUIRenderEngine::PopAnimationAndTransformContext( uint64 unContextID )
{
	VPROF_BUDGET_DETAILED( "CUIRenderEngine::PopAnimationAndTransformContext", VPROF_BUDGETGROUP_TENFOOT );

	PopAAndTContextRenderCommand_t *pCommand = PushPaintCommand< PopAAndTContextRenderCommand_t >();
	pCommand->context_id = unContextID;
}


//-----------------------------------------------------------------------------
// Purpose: Signal beginning of painting background contents for current context
//-----------------------------------------------------------------------------
void CUIRenderEngine::BeginPaintBackground()
{
	VPROF_BUDGET_THREAD( "CUIRenderEngine::BeginPaintBackground", VPROF_BUDGETGROUP_TENFOOT );

	PushPaintCommand< BeginPaintBackgroundRenderCommand_t >();
}


//-----------------------------------------------------------------------------
// Purpose: Signal ending of painting background contents for current context
//-----------------------------------------------------------------------------
void CUIRenderEngine::EndPaintBackground()
{
	VPROF_BUDGET_THREAD( "CUIRenderEngine::EndPaintBackground", VPROF_BUDGETGROUP_TENFOOT );

	PushPaintCommand< EndPaintBackgroundRenderCommand_t >();
}


//-----------------------------------------------------------------------------
// Purpose: Signal beginning of painting contents for current context which should be drawn last after normal children
//-----------------------------------------------------------------------------
void CUIRenderEngine::BeginPaintLast()
{
	VPROF_BUDGET_DETAILED( "CUIRenderEngine::BeginPaintLast", VPROF_BUDGETGROUP_TENFOOT );

	PushPaintCommand< BeginPaintLastRenderCommand_t >();
}


//-----------------------------------------------------------------------------
// Purpose: Signal ending of painting contents for current context which should be drawn last after normal children
//-----------------------------------------------------------------------------
void CUIRenderEngine::EndPaintLast()
{
	VPROF_BUDGET_DETAILED( "CUIRenderEngine::EndPaintLast", VPROF_BUDGETGROUP_TENFOOT );
	
	PushPaintCommand< EndPaintLastRenderCommand_t >();
}


//-----------------------------------------------------------------------------
// Purpose: Draw a filled quad
//-----------------------------------------------------------------------------
FillBrushCollectionWithTransition_t *CUIRenderEngine::DrawFilledRect( uint64 unContextID, float x0, float y0, float x1, float y1, EAntialiasing antialiasing )
{
	VPROF_BUDGET_DETAILED( "CUIRenderEngine::DrawFilledRect", VPROF_BUDGETGROUP_TENFOOT );

	if ( x1 - x0 < 0.0000001f || y1 - y0 < 0.0000001f )
		return nullptr;

	DrawFilledRectRenderCommand_t *pCommand = PushPaintCommand< DrawFilledRectRenderCommand_t >();
	pCommand->context_id = unContextID;
	pCommand->antialiasing = antialiasing;
	pCommand->top_left.x = x0;
	pCommand->top_left.y = y0;
	pCommand->bottom_right.x = x1;
	pCommand->bottom_right.y = y1;
	return &pCommand->fill_brush_collection;
}


//-----------------------------------------------------------------------------
// Purpose: Draw a solid color rectangle
//-----------------------------------------------------------------------------
void CUIRenderEngine::DrawSolidColorRect( float x0, float y0, float x1, float y1, uint32 unColorRGBA, EAntialiasing antialiasing /* = k_EAntialisingEnabled */ )
{
	// If fully transparent, don't bother
	if ( ( unColorRGBA & 0xff000000 ) == 0 )
		return;

	FillBrushCollectionWithTransition_t *pFillBrushCollection = DrawFilledRect( 0 /* panel not tracked */, x0, y0, x1, y1, antialiasing );
	if ( pFillBrushCollection )
	{
		pFillBrushCollection->AddSolidColorFillBrushNoTransition( unColorRGBA, GetCommandList() );
	}
}

//--------------------------------------------------------------------------------------------------
// Push blur rectangles
//--------------------------------------------------------------------------------------------------

void CUIRenderEngine::PushBlurPanels( const CUtlVector<uint64> &blurPanels )
{
	BlurPanelsCommand_t *pCommand = PushPaintCommand< BlurPanelsCommand_t >();

	if ( blurPanels.Count() )
	{
		pCommand->blurPanel = blurPanels[ 0 ];
		
		CRenderDataListBuilder< uint64 > listBuilder( pCommand->srcPanels, &GetCommandList() );
		for ( int i = 0; i < ( blurPanels.Count() - 1 ); i++ )
		{
			uint64 *pPanelHandle = listBuilder.AddToTail();
			*pPanelHandle = blurPanels[ i + 1 ];
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draw a textured quad that synchronizes the texture data used with this serial number
//-----------------------------------------------------------------------------
void CUIRenderEngine::DrawTexturedRectCore( IUITexture *pTexture, ETextureSampleMode eSampleMode, int32 unSerialize, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, OpacityWithTransition_t *pOpacityData )
{
	VPROF_BUDGET_DETAILED( "CUIRenderEngine::DrawTexturedRectCore", VPROF_BUDGETGROUP_TENFOOT );

	DrawTexturedRectRenderCommand_t *pCommand = PushPaintCommand< DrawTexturedRectRenderCommand_t >();

	pCommand->texture.SetTexture( pTexture, GetCurrentCommandList() );
	pCommand->texture_serial = unSerialize;
	pCommand->texture_sample_mode = eSampleMode;

	pCommand->texture_opacity = pOpacityData;

	pCommand->top_left.x = x0;
	pCommand->top_left.y = y0;
	pCommand->bottom_right.x = x1;
	pCommand->bottom_right.y = y1;

	pCommand->texture_top_left.x = u0;
	pCommand->texture_top_left.y = v0;
	pCommand->texture_bottom_right.x = u1;
	pCommand->texture_bottom_right.y = v1;
}

//-----------------------------------------------------------------------------
// Purpose: Draw a textured quad that synchronizes the texture data used with this serial number
//-----------------------------------------------------------------------------
void CUIRenderEngine::DrawSyncronizedTexturedRect( IUITexture *pTexture, ETextureSampleMode eSampleMode, int32 unSerialize, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1 )
{
	DrawTexturedRectCore( pTexture, eSampleMode, unSerialize, x0, y0, x1, y1, u0, v0, u1, v1, nullptr );
}


//-----------------------------------------------------------------------------
// Purpose: Draw a textured quad
//-----------------------------------------------------------------------------
void CUIRenderEngine::DrawTexturedRect( IUITexture *pTexture, ETextureSampleMode eSampleMode, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1 )
{
	DrawTexturedRectCore( pTexture, eSampleMode, 0, x0, y0, x1, y1, u0, v0, u1, v1, nullptr );
}


//-----------------------------------------------------------------------------
// Purpose: Draw a textured quad with an opacity
//-----------------------------------------------------------------------------
void CUIRenderEngine::DrawTexturedRectOpacity( IUITexture *pTexture, ETextureSampleMode eSampleMode, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, OpacityWithTransition_t *pOpacityData )
{
	DrawTexturedRectCore( pTexture, eSampleMode, 0, x0, y0, x1, y1, u0, v0, u1, v1, pOpacityData );
}


//-----------------------------------------------------------------------------
// Purpose: Draw text into a rect region
//
// NOTE: The contents of pmsgRangeFormats will be swapped into the message. pmsgRangeFormats should be treated as invalid after this call
//-----------------------------------------------------------------------------
DrawTextRegionRenderCommand_t *CUIRenderEngine::DrawTextRegion( const char *pchText, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 )
{
	VPROF_BUDGET_DETAILED( "CUIRenderEngine::DrawTextRegionUTF8", VPROF_BUDGETGROUP_TENFOOT );
	int cTextChars = V_UnicodeLength( pchText );
	int cTextBytes = V_strlen( pchText ) + 1;
	return DrawTextRegionInternal( pchText, cTextBytes, cTextChars, k_EPanoramaTextEncodingUTF8, pchFontName, flSize, flLineHeight, weight, style, align, decoration, bWrap, bEllipsis, nLetterSpacing, x0, y0, x1, y1 );
}


//-----------------------------------------------------------------------------
// Purpose: Draw text into a rect region
//
// NOTE: The contents of pmsgRangeFormats will be swapped into the message. pmsgRangeFormats should be treated as invalid after this call
//-----------------------------------------------------------------------------
DrawTextRegionRenderCommand_t *CUIRenderEngine::DrawTextRegion( const uchar16 *pch16Text, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 )
{
	VPROF_BUDGET_DETAILED( "CUIRenderEngine::DrawTextRegionUChar16", VPROF_BUDGETGROUP_TENFOOT );
	int cTextChars = V_UnicodeLength( pch16Text );
	int cTextBytes = ( V_strlen16( pch16Text ) + 1 ) * sizeof( *pch16Text );
	return DrawTextRegionInternal( pch16Text, cTextBytes, cTextChars, k_EPanoramaTextEncodingUChar16, pchFontName, flSize, flLineHeight, weight, style, align, decoration, bWrap, bEllipsis, nLetterSpacing, x0, y0, x1, y1 );
}


//-----------------------------------------------------------------------------
// Purpose: Draw text into a rect region
//
// NOTE: The contents of pmsgRangeFormats will be swapped into the message. pmsgRangeFormats should be treated as invalid after this call
//-----------------------------------------------------------------------------
DrawTextRegionRenderCommand_t *CUIRenderEngine::DrawTextRegion( const uchar32 *pch32Text, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 )
{
	VPROF_BUDGET_DETAILED( "CUIRenderEngine::DrawTextRegionUChar32", VPROF_BUDGETGROUP_TENFOOT );
	int cTextChars = V_UnicodeLength( pch32Text );
	int cTextBytes = ( V_strlen32( pch32Text ) + 1 ) * sizeof( *pch32Text );
	return DrawTextRegionInternal( pch32Text, cTextBytes, cTextChars, k_EPanoramaTextEncodingUChar32, pchFontName, flSize, flLineHeight, weight, style, align, decoration, bWrap, bEllipsis, nLetterSpacing, x0, y0, x1, y1 );
}


//-----------------------------------------------------------------------------
// Purpose: Draw text into a rect region
//
// NOTE: The contents of pmsgRangeFormats will be swapped into the message. pmsgRangeFormats should be treated as invalid after this call
//-----------------------------------------------------------------------------
DrawTextRegionRenderCommand_t *CUIRenderEngine::DrawTextRegionInternal( const void *pRawText, uint32 nTextBytes, int cTextChars, EPanoramaTextEncoding eTextEncoding, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 )
{
	VPROF_BUDGET_DETAILED( "CUIRenderEngine::DrawTextRegionInternal", VPROF_BUDGETGROUP_TENFOOT );

	// Early out right away if there is no width/height to the text
	if ( x1 <= x0 || y1 <= y0 )
		return nullptr;

	DrawTextRegionRenderCommand_t *pCommand = PushPaintCommand< DrawTextRegionRenderCommand_t >();

	if ( pRawText && nTextBytes > 0 )
	{
		pCommand->raw_text = GetCurrentCommandList().Alloc( nTextBytes );
		V_memcpy( pCommand->raw_text, pRawText, nTextBytes );
		pCommand->raw_text_bytes = ( uint32 )nTextBytes;
	}

	pCommand->text_chars = cTextChars;
	pCommand->text_encoding = eTextEncoding;
	pCommand->text_align = align;
	pCommand->wrapping = bWrap;
	pCommand->ellipsis = bEllipsis;

	if ( flLineHeight != k_flFloatNotSet )
		pCommand->line_height = flLineHeight;

	pCommand->top_left.x = x0;
	pCommand->top_left.y = y0;
	pCommand->bottom_right.x = x1;
	pCommand->bottom_right.y = y1;

	// set default format
	pCommand->default_format.font_name = GetCurrentCommandList().CopyString( pchFontName );
	pCommand->default_format.font_size = flSize;
	pCommand->default_format.font_weight = weight;
	pCommand->default_format.font_style = style;
	pCommand->default_format.letter_spacing = nLetterSpacing;
	pCommand->default_format.text_decoration = decoration;

	return pCommand;
}


//-----------------------------------------------------------------------------
// Purpose: Draw solid colored UTF8 text that doesn't have any animation/transition
//-----------------------------------------------------------------------------
void CUIRenderEngine::DrawSolidColorTextRegion( const char *pchText, const char *pchFontName, uint32 unColor, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 )
{
	VPROF_BUDGET_DETAILED( "CUIRenderEngine::DrawTextRegionUTF8", VPROF_BUDGETGROUP_TENFOOT );
	int cTextChars = V_UnicodeLength( pchText );
	int cTextBytes = V_strlen( pchText ) + 1;
	DrawSolidColorTextRegionInternal( pchText, cTextBytes, cTextChars, k_EPanoramaTextEncodingUTF8, pchFontName, unColor, flSize, flLineHeight, weight, style, align, decoration, bWrap, bEllipsis, nLetterSpacing, x0, y0, x1, y1 );
}

//-----------------------------------------------------------------------------
// Purpose: Draw solid colored UTF16 text that doesn't have any animation/transition
//-----------------------------------------------------------------------------
void CUIRenderEngine::DrawSolidColorTextRegion( const uchar16 *pch16Text, const char *pchFontName, uint32 unColor, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 )
{
	VPROF_BUDGET_DETAILED( "CUIRenderEngine::DrawTextRegionUChar16", VPROF_BUDGETGROUP_TENFOOT );
	int cTextChars = V_UnicodeLength( pch16Text );
	int cTextBytes = ( V_strlen16( pch16Text ) + 1 ) * sizeof( *pch16Text );
	DrawSolidColorTextRegionInternal( pch16Text, cTextBytes, cTextChars, k_EPanoramaTextEncodingUTF8, pchFontName, unColor, flSize, flLineHeight, weight, style, align, decoration, bWrap, bEllipsis, nLetterSpacing, x0, y0, x1, y1 );
}

//-----------------------------------------------------------------------------
// Purpose: Draw solid colored UTF32 text that doesn't have any animation/transition
//-----------------------------------------------------------------------------
void CUIRenderEngine::DrawSolidColorTextRegion( const uchar32 *pch32Text, const char *pchFontName, uint32 unColor, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 )
{
	VPROF_BUDGET_DETAILED( "CUIRenderEngine::DrawTextRegionUChar32", VPROF_BUDGETGROUP_TENFOOT );
	int cTextChars = V_UnicodeLength( pch32Text );
	int cTextBytes = ( V_strlen32( pch32Text ) + 1 ) * sizeof( *pch32Text );
	DrawSolidColorTextRegionInternal( pch32Text, cTextBytes, cTextChars, k_EPanoramaTextEncodingUTF8, pchFontName, unColor, flSize, flLineHeight, weight, style, align, decoration, bWrap, bEllipsis, nLetterSpacing, x0, y0, x1, y1 );
}


//-----------------------------------------------------------------------------
// Purpose: Draw solid colored text that doesn't have any animation/transition
//-----------------------------------------------------------------------------
void CUIRenderEngine::DrawSolidColorTextRegionInternal( const void *pRawText, uint32 nTextBytes, int cTextChars, EPanoramaTextEncoding eTextEncoding, const char *pchFontName, uint32 unColor, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 )
{
	DrawTextRegionRenderCommand_t *pCommand = DrawTextRegionInternal( pRawText, nTextBytes, cTextChars, eTextEncoding, pchFontName, flSize, flLineHeight, weight, style, align, decoration, bWrap, bEllipsis, nLetterSpacing, x0, y0, x1, y1 );
	pCommand->default_format.fill_brush_collection.AddSolidColorFillBrushNoTransition( unColor, GetCommandList() );
}


//-----------------------------------------------------------------------------
// Purpose: Tell the render thread to call the panel back on the specified method, 
// which will then be able to do direct render system calls on the render thread
//-----------------------------------------------------------------------------
#if defined( SOURCE2_PANORAMA )
void CUIRenderEngine::RequestRenderCallback( CRenderThreadCallback * pCallbackObj, float x0, float y0, float x1, float y1,
	float flPaddingLeft, float flPaddingRight, float flPaddingTop, float flPaddingBottom, ERenderCallbackFlags eFlags, IUITexture *pPanelRT )
{
	RequestRenderCallbackCommand_t *pCommand = PushPaintCommand< RequestRenderCallbackCommand_t >();

	// Track that there is a ref count in the command stream
	GetCurrentCommandList().AddObjectReferenceDelayedRelease( pCallbackObj );

	pCommand->pCallbackObj = pCallbackObj;
	pCommand->top_left.x = x0;
	pCommand->top_left.y = y0;
	pCommand->bottom_right.x = x1;
	pCommand->bottom_right.y = y1;
	pCommand->top_left_padding.x = flPaddingLeft;
	pCommand->top_left_padding.y = flPaddingTop;
	pCommand->bottom_right_padding.x = flPaddingRight;
	pCommand->bottom_right_padding.y = flPaddingBottom;
	pCommand->flags = eFlags;
	pCommand->panelRT.SetTexture( pPanelRT, GetCurrentCommandList() );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Draw an already cached command list
//-----------------------------------------------------------------------------
void CUIRenderEngine::DrawCachedCommandList( CRenderCommandList &commandList )
{
	NestedCommandListCommand_t *pCommand = PushPaintCommand< NestedCommandListCommand_t >();
	pCommand->command_list = &commandList;
	GetCurrentCommandList().AddObjectReference( &commandList );
}

CRenderCommandList &CUIRenderEngine::PushPaintCommandList()
{
	m_vecPaintCommandListStack.AddToTail( new CRenderCommandList() );
	return GetCurrentCommandList();
}

void CUIRenderEngine::PopPaintCommandList()
{
	CRenderCommandList *pCommandList = m_vecPaintCommandListStack.Tail();
	m_vecPaintCommandListStack.Remove( m_vecPaintCommandListStack.Count() - 1 );
	DrawCachedCommandList( *pCommandList );
	pCommandList->Release();
}

//-----------------------------------------------------------------------------
// Purpose: Queue to delete a particle system, this is how the animation thread
// finds out about particle systems going away
//-----------------------------------------------------------------------------
void CUIRenderEngine::QueueParticleSystemDelete( uint64 ulPanelHandle, uint32 unIndex )
{
	ParticleSystemDelete_t del;
	del.ulPanelHandle = ulPanelHandle;
	del.unIndex = unIndex;

	m_vecParticleSystemsToDelete.AddToTail( del );
}


//-----------------------------------------------------------------------------
// Purpose: Start a fresh paint buffer on the paint thread - YOU MUST OWN THE PAINT LOCK WHEN CALLING THIS!!!
//-----------------------------------------------------------------------------
void CUIRenderEngine::StartNewPaintBuffer()
{
	// YOU MUST OWN THE PAINT LOCK WHEN CALLING THIS!!!
	// No sync considerations here on CS:GO. This function is always called on the main thread, and does not in any
	// way touch any data that the anim thread might be using. Paint buffers built by main thread are added to 
	// a queue in CUIRenderEngine::EndFrame, from where they are picked up by the anim thread next frame.
	
	// Swap in new buffer for painting to
	Assert( m_pCurrentPaintList == nullptr );
	m_pCurrentPaintList = new RenderCommandList_t();
	
	Assert( m_vecPaintCommandListStack.IsEmpty() );
	m_vecPaintCommandListStack.AddToTail( m_pCurrentPaintList->m_pCommandList.Get() );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUIRenderEngine::RenderCommandList_t::RenderCommandList_t()
	: m_unRenderCount( 0 )
	, m_pCommandList( kNoAddRef, new CRenderCommandList() )
{
}

const float k_flStatsAverageDecay = 0.95f;		// How fast should older values decay
const int k_unStatsAveragesMultiplier = 100;	// Since we're storing values in integral types, multiply all of the averages by 100 so that we don't lose as much due to precision issues.

//-----------------------------------------------------------------------------
// Purpose: Update the running averages of command list stats
//-----------------------------------------------------------------------------
template < typename T > void UpdateAverageValue( T &average, T newValue )
{
	average = ( average * k_flStatsAverageDecay ) + ( ( newValue * k_unStatsAveragesMultiplier ) * ( 1.0f - k_flStatsAverageDecay ) );
}

//-----------------------------------------------------------------------------
// Purpose: Update the running averages of command list stats
//-----------------------------------------------------------------------------
void CUIRenderEngine::UpdateCommandListStatsAverages( RenderCommandListStats_t &averages, const RenderCommandListStats_t &stats )
{
	AUTO_LOCK( s_CommandStatsLock );

	const float k_flAveragePercent = 0.95f;

	for ( int i = 0; i < ARRAYSIZE( averages.aCommandCounts ); ++i )
	{
		UpdateAverageValue( averages.aCommandCounts[ i ], stats.aCommandCounts[ i ] );
	}
	UpdateAverageValue( averages.unTotalBytesAllocated, stats.unTotalBytesAllocated );
	UpdateAverageValue( averages.unTotalCommandCount, stats.unTotalCommandCount );
}


//-----------------------------------------------------------------------------
// Purpose: Dump stats about a given set of command list averages
//-----------------------------------------------------------------------------
/*static*/ void CUIRenderEngine::DumpCommandListStatsAverages( const char *pszType, const RenderCommandListStats_t &averages )
{
	Msg( " Average %s commands per frame: %u, %u bytes\n", pszType, ( uint32 )( averages.unTotalCommandCount / k_unStatsAveragesMultiplier ), ( uint32 )( averages.unTotalBytesAllocated / k_unStatsAveragesMultiplier ) );
	for ( int i = 0; i < k_ERenderCommand_Count; ++i )
	{
		uint32 unCommandCount = ( averages.aCommandCounts[ i ] + k_unStatsAveragesMultiplier / 2.0f ) / k_unStatsAveragesMultiplier;
		if ( unCommandCount == 0 )
			continue;

		Msg( "    %4u: %s\n", unCommandCount, GetRenderCommandTypeName( ( ERenderCommand )i ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Dump stats about average usage of command lists
//-----------------------------------------------------------------------------
void CUIRenderEngine::DumpCommandListStats()
{
	if ( !s_convarPanoramaTrackRenderCommands.GetBool() )
	{
		Msg( "You must turn on @panorama_track_render_commands to track command usage.\n" );
		return;
	}

	AUTO_LOCK( s_CommandStatsLock );

	Msg( "-----------------------------------------------------------------------\n" );
	Msg( "Render Command Stats\n" );
	Msg( "-----------------------------------------------------------------------\n" );

	DumpCommandListStatsAverages( "Paint", s_averagePaintCommandListStats );
	Msg( "\n" );
	DumpCommandListStatsAverages( "Render", s_averageRenderCommandListStats );

	Msg( "-----------------------------------------------------------------------\n" );
	Msg( "-----------------------------------------------------------------------\n" );
}


//-----------------------------------------------------------------------------
// Purpose: Console command to dump out stats about 
//-----------------------------------------------------------------------------
CON_COMMAND( dump_panorama_render_command_stats, "" )
{
	CUIRenderEngine::DumpCommandListStats();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUIRenderEngine::RenderCommandList_t::~RenderCommandList_t()
{
}


//-----------------------------------------------------------------------------
// Purpose: Animation thread
//-----------------------------------------------------------------------------
int CUIRenderEngine::CUIAnimationThread::Run()
{
// NOTE - We do NOT use this thread on CS:GO. 
// CUIAnimationThread::RunSingleFrame and CUIRenderThread::RunSingleFrame are
// called by CPanoramaEngineHandler::PanoramaRenderFrame, which runs as a
// queued job on the rendering thread

//#if defined( VPROF_ENABLED ) && !defined( SOURCE2_PANORAMA )
//	CVProfile *pProfile = GetVProfProfileForCurrentThread();
//#endif
//
//	CLimitTimer timer;
//	while( !m_bExit )
//	{
//		if ( m_bSleepForValidate )
//		{
//			bool bOkToSleep = true;
//			{
//				AUTO_LOCK( m_pRenderEngine->m_PaintingLock );
//				if ( m_pRenderEngine->m_pCurrentAnimationList && m_pRenderEngine->m_pCurrentAnimationList->m_unRenderCount == 0  )
//				{
//					bOkToSleep = false;
//				}
//			}
//
//			// Don't sleep yet if we haven't rendered the buffer we have, as it will block the animation thread from sleeping.
//			if ( bOkToSleep )
//			{
//				while ( m_bSleepForValidate )
//				{
//					m_bSleepingForValidate = true;
//					ThreadSleep( 100 );
//				}
//			}
//		}
//		m_bSleepingForValidate = false;
//
//#if defined( VPROF_ENABLED ) && !defined( SOURCE2_PANORAMA )
//		if ( pProfile )
//			pProfile->MarkFrame( "Tenfoot Animation Thread" );
//#endif
//		// Limit animation frame rate
//		timer.SetLimit( 8333 ); // 120hz
//
//		RunSingleFrame();
//
//		// Sleep as needed, but always at least 1 millisecond to yield some time
//		// to the paint thread and ensure we don't spin fast if it's trying to lock
//		// for SwapBuffers() to give us new data.
//		{
//			VPROF_BUDGET_THREAD( "Sleep - FPS Limiting", VPROF_BUDGETGROUP_TENFOOT );
//			if( timer.BLimitReached() && !m_pRenderEngine->Access3DSurface()->BVsyncEnabled() )
//				ThreadSleep( 1 );
//			else
//			{
//				// Extra limiting logic here to make sure we never sleep really long because
//				// some bad/overclocked CPUs are known to break CMicroSecLeft and sometimes return crazy
//				// values.  We won't have optimal behavior on those, but we can try not to be crazy broken.
//				int nWaits = 2;
//				while( !timer.BLimitReached() && nWaits > 0 )
//				{
//					int64 msLeft = timer.CMicroSecLeft() / 1000;
//					m_WakeEvent.Wait( MIN( msLeft, 6 ) );
//					--nWaits;
//				}
//			}
//		}
//	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Animation thread
//-----------------------------------------------------------------------------
void CUIRenderEngine::CUIAnimationThread::RunSingleFrame()
{
	VPROF_BUDGET_THREAD( "Animation Thread Outer", VPROF_BUDGETGROUP_TENFOOT );
	VPROF_BUDGET( "Panorama Animation RunSingleFrame", VPROF_BUDGETGROUP_GAME );

	float mouseX, mouseY;
	m_pInput->GetSurfaceMousePosition( mouseX, mouseY );
	m_pAnimationEngine->SetMousePosition( mouseX, mouseY );
	m_pAnimationEngine->SetTrackingMousePanels( m_pInput->GetMouseTrackingHandles() );

	RenderCommandList_t *pRenderListOut = nullptr;
	{
		// Try to get the paint lock to swap in a new buffer if ready
		{
			VPROF_BUDGET_THREAD( "SwapInNewPaintBuffer", VPROF_BUDGETGROUP_TENFOOT );
			bool bGotNewFrame = false;
			
			m_pRenderEngine->m_PaintingLock.Lock();
			if ( !m_pRenderEngine->m_pCurrentAnimationList || m_pRenderEngine->m_pCurrentAnimationList->m_unRenderCount > 0 )
			{
				if ( m_pRenderEngine->m_paintListsQueue.Count() )
				{
					// Clear out the animation list if it exists
					SAFE_DELETE( m_pRenderEngine->m_pCurrentAnimationList );

					m_pRenderEngine->m_pCurrentAnimationList = m_pRenderEngine->m_paintListsQueue.RemoveAtHead();


					// we are done with the buffer from the paint/main thread, tell it to wake up and make us another one

					bGotNewFrame = true;
				}
			}
			m_pRenderEngine->m_PaintingLock.Unlock();

			// In source2 we synchronize all the threads, there should always be a new layout thread ready for us before we run
#if defined( SOURCE2_PANORAMA ) && !defined ( PANORAMA_USE_S1WRAPPER )
			Assert( bGotNewFrame || s_convarSuspendPaint.GetBool() );
#else
			REFERENCE( bGotNewFrame );
#endif
		}

		if ( m_pRenderEngine->m_pCurrentAnimationList )
		{
			RenderCommandList_t *pAnimList = m_pRenderEngine->m_pCurrentAnimationList;

			pRenderListOut = new RenderCommandList_t();

			pAnimList->m_unRenderCount++;
			m_pAnimationEngine->SetRenderCountThisFrame( pAnimList->m_unRenderCount );

			// Source 2 synchronizes layout, animation, and render thread, should only ever be 1 animation of a frame.  
			// Assert so we know if the synchronization contract is broken.
#if defined( SOURCE2_PANORAMA ) && !defined ( PANORAMA_USE_S1WRAPPER )
			Assert( pAnimList->m_unRenderCount == 1 || s_convarSuspendPaint.GetBool() );
#endif

			// AddRef ref counted objects in command stream and track
			pRenderListOut->m_pCommandList->CopyObjectReferences( *( pAnimList->m_pCommandList.Get() ) );

			if ( s_convarPanoramaTrackRenderCommands.GetBool() )
			{
				V_memset( &m_commandStats, 0, sizeof( m_commandStats ) );
				m_commandStats.unTotalBytesAllocated += pAnimList->m_pCommandList->GetTotalBytesAllocated();
			}

			RenderCommand_t *pCurrentRenderCommand = pAnimList->m_pCommandList->GetFirstCommand();
			while ( pCurrentRenderCommand != nullptr )
			{
				HandleRenderCommand( *pCurrentRenderCommand, *( pRenderListOut->m_pCommandList.Get() ) );
				pCurrentRenderCommand = pCurrentRenderCommand->pNextRenderCommand;
			}

			if ( s_convarPanoramaTrackRenderCommands.GetBool() )
			{
				// Skip command lists with less than 5 commands. These are probably just empty windows and can be ignored
				if ( m_commandStats.unTotalCommandCount >= 5 )
				{
					CUIRenderEngine::UpdateCommandListStatsAverages( CUIRenderEngine::s_averagePaintCommandListStats, m_commandStats );
				}
			}
		}
	}

	Vector2D lastHoverPosition = m_pAnimationEngine->GetLastHitTestPanelCoords();
	m_pInput->SetLastHover( m_pAnimationEngine->GetLastFinishedFrameTime(), m_pAnimationEngine->GetLastHitTestPanelPtr(), lastHoverPosition.x, lastHoverPosition.y );

	{
		CCopyableUtlVector<MouseTrackingResults_t> results = m_pAnimationEngine->GetMouseTrackingResults();
		m_pInput->SetMouseTrackingResults( results );
	}

	// SwapBuffers to the render thread
	{
		VPROF_BUDGET_THREAD( "Sleep - Wait on RenderThread for SwapBuffers", VPROF_BUDGETGROUP_TENFOOT );

		m_pRenderEngine->m_RenderingLock.Lock();
		while ( ( m_pRenderEngine->m_pCurrentRenderList && m_pRenderEngine->m_pCurrentRenderList->m_unRenderCount == 0 ) && !m_bExit )
		{
			m_pRenderEngine->m_RenderingLock.Unlock();
			{
				ThreadSleep( 5 );
			}
			m_pRenderEngine->m_RenderingLock.Lock();
		}
	}

	SAFE_DELETE( m_pRenderEngine->m_pCurrentRenderList );
	m_pRenderEngine->m_pCurrentRenderList = pRenderListOut;

	m_pRenderEngine->m_RenderingLock.Unlock();
}

void CUIRenderEngine::CUIAnimationThread::HandleRenderCommand( RenderCommand_t &renderCommand, CRenderCommandList &outputCommandList )
{
	switch ( renderCommand.eCommandType )
	{
		case k_EBeginFrame:
			m_pAnimationEngine->BeginFrame( static_cast< BeginFrameRenderCommand_t & >( renderCommand ), outputCommandList );
			break;

		case k_EEndFrame:
			m_pAnimationEngine->EndFrame( static_cast< EndFrameRenderCommand_t & >( renderCommand ), outputCommandList );
			break;

		case k_EPushBlurPanels:
		{
			BlurPanelsCommand_t &command = static_cast<BlurPanelsCommand_t &>( renderCommand );
			command.PushCommandCopy( outputCommandList );
			break;
		}

		case k_EPushAnimationAndTransformContext:
		{
			PushAAndTContextRenderCommand_t &command = static_cast< PushAAndTContextRenderCommand_t & >( renderCommand );
			m_pAnimationEngine->PushAnimationAndTransformContext( command, outputCommandList );
			// Reseting paint flag to "repaint none" as the render command could be cached and not regenerated every frame
			// By setting the repaint flag to none, we ensure that the composition layer can be reused the next frame
			// (instead of being regenerated every frame even though nothing is changing but we've just happened to use a 
			// cached render command)
			command.needs_full_repaint = k_EPanelRepaintNone;
			break;
		}

		case k_EPopAnimationAndTransformContext:
			m_pAnimationEngine->PopAnimationAndTransformContext( static_cast< PopAAndTContextRenderCommand_t & >( renderCommand ), outputCommandList );
			break;

		case k_EBeginPaintBackground:
			m_pAnimationEngine->BeginPaintBackground( static_cast< BeginPaintBackgroundRenderCommand_t & >( renderCommand ), outputCommandList );
			break;

		case k_EEndPaintBackground:
			m_pAnimationEngine->EndPaintBackground( static_cast< EndPaintBackgroundRenderCommand_t & >( renderCommand ), outputCommandList );
			break;

		case k_EBeginPaintLast:
			m_pAnimationEngine->BeginPaintLast( static_cast< BeginPaintLastRenderCommand_t & >( renderCommand ), outputCommandList );
			break;

		case k_EEndPaintLast:
			m_pAnimationEngine->EndPaintLast( static_cast< EndPaintLastRenderCommand_t & >( renderCommand ), outputCommandList );
			break;

		case k_ECmdDrawTexturedRect:
			m_pAnimationEngine->DrawTexturedRect( static_cast< DrawTexturedRectRenderCommand_t & >( renderCommand ), outputCommandList );
			break;

		case k_EDrawFilledRect:
			m_pAnimationEngine->DrawFilledRect( static_cast< DrawFilledRectRenderCommand_t & >( renderCommand ), outputCommandList );
			break;

		case k_EDrawTextRegion:
			m_pAnimationEngine->DrawTextRegion( static_cast< DrawTextRegionRenderCommand_t & >( renderCommand ), outputCommandList );
			break;

		case k_EDeleteParticleSystem:
			m_pAnimationEngine->DeleteParticleSystem( static_cast< DeleteParticleSystemRenderCommand_t & >( renderCommand ), outputCommandList );
			break;

		case k_EDeletePanel:
			m_pAnimationEngine->DeletePanel( static_cast< DeletePanelRenderCommand_t & >( renderCommand ), outputCommandList );
			break;

		case k_ERequestRenderCallback:
			m_pAnimationEngine->RequestRenderCallback( static_cast< RequestRenderCallbackCommand_t & >( renderCommand ), outputCommandList );
			break;

		case k_ENestedCommand:
		{
			NestedRenderCommand_t &nestedCommand = static_cast< NestedRenderCommand_t & >( renderCommand );
			HandleRenderCommand( *nestedCommand.command, outputCommandList );
			break;
		}

		case k_EClearBackBuffer:
		{
			ClearBackbufferRenderCommand_t &command = static_cast< ClearBackbufferRenderCommand_t & >( renderCommand );
			command.PushCommandCopy( outputCommandList );
			break;
		}

		case k_EPushCompositingLayer:
		{
			PushCompositingLayerRenderCommand_t &command = static_cast< PushCompositingLayerRenderCommand_t & >( renderCommand );
			command.PushCommandCopy( outputCommandList );
			break;
		}

		case k_EPopCompositingLayer:
		{
			PopCompositingLayerRenderCommand_t &command = static_cast< PopCompositingLayerRenderCommand_t & >( renderCommand );
			command.PushCommandCopy( outputCommandList );
			break;
		}

		case k_EPushClipLayer:
		{
			PushClipLayerRenderCommand_t &command = static_cast< PushClipLayerRenderCommand_t & >( renderCommand );
			command.PushCommandCopy( outputCommandList );
			break;
		}

		case k_EPopClipLayer:
		{
			PopClipLayerRenderCommand_t &command = static_cast< PopClipLayerRenderCommand_t & >( renderCommand );
			command.PushCommandCopy( outputCommandList );
			break;
		}

		case k_ECmdFreeCompositingLayer:
		{
			FreeCompositingLayerRenderCommand_t &command = static_cast< FreeCompositingLayerRenderCommand_t & >( renderCommand );
			command.PushCommandCopy( outputCommandList );
			break;
		}

		case k_ECmdLockTexture:
		{
			LockTextureRenderCommand_t &command = static_cast< LockTextureRenderCommand_t & >( renderCommand );
			command.PushCommandCopy( outputCommandList );
			break;
		}

		case k_EPushPanelContextInLayer:
		{
			PushPanelContextInLayerRenderCommand_t &command = static_cast< PushPanelContextInLayerRenderCommand_t & >( renderCommand );
			command.PushCommandCopy( outputCommandList );
			break;
		}

		case k_EPopPanelContextInLayer:
		{
			PopPanelContextInLayerRenderCommand_t &command = static_cast< PopPanelContextInLayerRenderCommand_t & >( renderCommand );
			command.PushCommandCopy( outputCommandList );
			break;
		}

		case k_ENestedCommandList:
		{
			NestedCommandListCommand_t &command = static_cast< NestedCommandListCommand_t & >( renderCommand );

			outputCommandList.CopyObjectReferences( *command.command_list );

			RenderCommand_t *pCurrentRenderCommand = command.command_list->GetFirstCommand();
			while ( pCurrentRenderCommand != nullptr )
			{
				HandleRenderCommand( *pCurrentRenderCommand, outputCommandList );
				pCurrentRenderCommand = pCurrentRenderCommand->pNextRenderCommand;
			}

			if ( s_convarPanoramaTrackRenderCommands.GetBool() )
			{
				m_commandStats.unTotalBytesAllocated += command.command_list->GetTotalBytesAllocated();
			}
			break;
		}

		default:
			AssertMsg1( false, "Unhandled command type %d", renderCommand.eCommandType );
			break;
	}

	if ( s_convarPanoramaTrackRenderCommands.GetBool() )
	{
		m_commandStats.aCommandCounts[ renderCommand.eCommandType ]++;
		m_commandStats.unTotalCommandCount++;
	}
}


//-----------------------------------------------------------------------------
// Purpose: We've just been told to directly run a render thread frame from calling thread, 
// this is used by source 2 which has it's own worker thread for this
//-----------------------------------------------------------------------------
#if defined( SOURCE2_PANORAMA )
void CUIRenderEngine::RunRenderThreadFrame( ISceneView *pView, IRenderContext *pRenderContext, ISceneLayer *pLayer, bool bAdvanceAnimation )
{
	// Set render context for frame
	m_RenderThread.Access3DSurface()->SetRenderContext( pView, pRenderContext, pLayer );

	// Run animation frame, we couple these in source2
	if ( bAdvanceAnimation )
	{
		m_AnimationThread.RunSingleFrame();
	}	

	// Run frame
	m_RenderThread.RunSingleFrame();

	// Ensure no further validity
	m_RenderThread.Access3DSurface()->SetRenderContext( NULL, NULL, NULL );
}
#endif

//--------------------------------------------------------------------------------------------------
// Walk the render queue and extract all clip(scissor rects) etc for blur panels
//--------------------------------------------------------------------------------------------------


void CUIRenderEngine::CUIRenderThread::ExtractBlurRectangles( RenderCommandList_t *pRenderList )
{
	static const VMatrix identityMatrix(
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1 );

	RenderMatrix4x4_t identity;
	VMatrixToRenderMatrix( identity, identityMatrix );

	m_p3DSurface->ResetBlurPanels();

	// Walk all render commands and extract blur panels

	for ( RenderCommand_t *pRenderCommand = pRenderList->m_pCommandList->GetFirstCommand(); pRenderCommand != nullptr; pRenderCommand = pRenderCommand->pNextRenderCommand )
	{
		if ( pRenderCommand->eCommandType == k_EPushBlurPanels )
		{
			BlurPanelsCommand_t & cmd = static_cast<BlurPanelsCommand_t &>( *pRenderCommand );
			m_p3DSurface->AddBlurPanel( cmd.blurPanel, cmd.srcPanels );
		}
	}

	// Second Pass looks for Clip layers, PushPanelInContext, or PushComposting for pos/size/matrix of panels
	
	for ( RenderCommand_t *pRenderCommandLp = pRenderList->m_pCommandList->GetFirstCommand(); pRenderCommandLp != nullptr; pRenderCommandLp = pRenderCommandLp->pNextRenderCommand )
	{

		RenderCommand_t *pRenderCommand = pRenderCommandLp;

		while ( pRenderCommand->eCommandType == k_ENestedCommand )
		{
			NestedRenderCommand_t &cmd = static_cast<NestedRenderCommand_t&>( *pRenderCommand );
			pRenderCommand = cmd.command;
		}

		if ( pRenderCommand->eCommandType == k_EDrawFilledRect )
		{
			RenderFilledRectRenderCommand_t & cmd = static_cast<RenderFilledRectRenderCommand_t &>(*pRenderCommand);

			Vector2D top_left = Vector2D( cmd.top_left.x, cmd.top_left.y );
			Vector2D bottom_right = Vector2D( cmd.bottom_right.x, cmd.bottom_right.y );

			m_p3DSurface->AddBlurPanelData( cmd.context_id, top_left, bottom_right, identity );
			m_p3DSurface->AddSourcePanelData( cmd.context_id, top_left, bottom_right, identity );
		}


		if ( pRenderCommand->eCommandType == k_EPushClipLayer )
		{
			PushClipLayerRenderCommand_t & cmd = static_cast<PushClipLayerRenderCommand_t &>( *pRenderCommand );

			Vector2D top_left = Vector2D( cmd.top_left.x, cmd.top_left.y );
			Vector2D bottom_right = Vector2D( cmd.bottom_right.x, cmd.bottom_right.y );

			m_p3DSurface->AddBlurPanelData( cmd.context_id, top_left, bottom_right, identity );
			m_p3DSurface->AddSourcePanelData( cmd.context_id, top_left, bottom_right, identity );
		}

		if ( pRenderCommand->eCommandType == k_EPushPanelContextInLayer )
		{
			PushPanelContextInLayerRenderCommand_t & cmd = static_cast<PushPanelContextInLayerRenderCommand_t &>( *pRenderCommand );

			Vector2D top_left = Vector2D( cmd.position.x, cmd.position.y );
			Vector2D bottom_right = Vector2D( cmd.position.x + cmd.width, cmd.position.y + cmd.height);

			m_p3DSurface->AddBlurPanelData( cmd.context_id, top_left, bottom_right, *cmd.transform );
			m_p3DSurface->AddSourcePanelData( cmd.context_id, top_left, bottom_right, *cmd.transform );
		}

		if ( pRenderCommand->eCommandType == k_EPushCompositingLayer )
		{
			PushCompositingLayerRenderCommand_t & cmd = static_cast<PushCompositingLayerRenderCommand_t &>( *pRenderCommand );

			Vector2D top_left = Vector2D( cmd.layer_quad_top_left.x, cmd.layer_quad_top_left.y );
			Vector2D bottom_right = Vector2D( cmd.layer_quad_bottom_right.x, cmd.layer_quad_bottom_right.y );

			m_p3DSurface->AddBlurPanelData( cmd.layer_id, top_left, bottom_right, (cmd.transform) );
			m_p3DSurface->AddSourcePanelData( cmd.layer_id, top_left, bottom_right, (cmd.transform) );
		}
	
	}

}

//-----------------------------------------------------------------------------
// Purpose: Rendering thread
//-----------------------------------------------------------------------------

void CUIRenderEngine::CUIRenderThread::RunSingleFrame()
{
#if defined( VPROF_ENABLED ) && !defined( SOURCE2_PANORAMA )
	CVProfile *pProfile = GetVProfProfileForCurrentThread();
#endif

#if defined( VPROF_ENABLED ) && !defined( SOURCE2_PANORAMA )
	if ( pProfile )
		pProfile->MarkFrame( "Tenfoot Render Thread" );
#endif

	EndFrameRenderCommand_t *pEndFrameCommand = nullptr;
	VPROF_BUDGET_THREAD( "Render Msg Loop Outer", VPROF_BUDGETGROUP_TENFOOT );
	VPROF_BUDGET( "Panorama Render RunSingleFrame", VPROF_BUDGETGROUP_GAME );

	{
		AUTO_LOCK( m_pRenderEngine->m_RenderingLock );

		if ( m_pRenderEngine->m_pCurrentRenderList )
		{
			VPROF_BUDGET_THREAD( "Render Msg Loop", VPROF_BUDGETGROUP_TENFOOT );
			RenderCommandList_t *pRenderList = m_pRenderEngine->m_pCurrentRenderList;
			pRenderList->m_unRenderCount++;

			// Source 2 synchronizes layout, animation, and render thread, should only ever be 1 render of a frame.  
			// Assert so we know if the synchronization contract is broken.
#if defined( SOURCE2_PANORAMA )
			// Skip updates breaks this contract, but it's ok.
			// Assert( pRenderList->m_unRenderCount == 1 );
#endif
//			DebugSingleFrame( pRenderBuffer );

			if ( s_convarPanoramaTrackRenderCommands.GetBool() )
			{
				V_memset( &m_commandStats, 0, sizeof( m_commandStats ) );
				m_commandStats.unTotalBytesAllocated += pRenderList->m_pCommandList->GetTotalBytesAllocated();
			}

			ExtractBlurRectangles( pRenderList );

			for ( RenderCommand_t *pRenderCommand = pRenderList->m_pCommandList->GetFirstCommand(); pRenderCommand != nullptr; pRenderCommand = pRenderCommand->pNextRenderCommand )
			{
				if ( pRenderCommand->eCommandType == k_EEndFrame )

				{
					// End Frame is a special case, and should always come last, so we can unlock when we hit it.  That's why
					// it gets special handling here, it's important, because otherwise we'd hold the lock while present blocks
					// on vsync if vsync is enabled.
					AssertMsg( pRenderCommand->pNextRenderCommand == nullptr, "BUG: More commands after receiving k_EEndFrame." );
					pEndFrameCommand = static_cast<EndFrameRenderCommand_t *>( pRenderCommand );
				}
				else
				{
					HandleRenderCommand( *pRenderCommand, *pRenderList );
				}
			}

			if ( s_convarPanoramaTrackRenderCommands.GetBool() )
			{
				// Skip command lists with less than 5 commands. These are probably just empty windows and can be ignored
				if ( m_commandStats.unTotalCommandCount >= 5 )
				{
					CUIRenderEngine::UpdateCommandListStatsAverages( CUIRenderEngine::s_averageRenderCommandListStats, m_commandStats );
				}
			}
		}
	}

	// we are done with the animation buffer, have it wake up and make us a new one
	m_pRenderEngine->WakeAnimationThread();

	if ( pEndFrameCommand )
	{
		m_p3DSurface->EndFrame( *pEndFrameCommand );
	}

	m_pRenderEngine->m_LastPaintFrameTimeRendered = m_p3DSurface->GetLastFramePaintTime();
	m_pRenderEngine->SetSwapBuffersEvent();
}

void CUIRenderEngine::CUIRenderThread::HandleRenderCommand( RenderCommand_t &command, const RenderCommandList_t &commandList )
{
	//VPROF_BUDGET( "Panorama Render HandleRenderCommand", VPROF_BUDGETGROUP_GAME );

	switch ( command.eCommandType )
	{
		case k_EBeginFrame:
			m_p3DSurface->BeginFrame( static_cast< BeginFrameRenderCommand_t & >( command ) );
			break;

		case k_EPushBlurPanels:
			break;

		case k_EClearBackBuffer:
			m_p3DSurface->ClearBackbuffer( static_cast< ClearBackbufferRenderCommand_t & >( command ) );
			break;

		case k_ECmdDrawTexturedRect:
			m_p3DSurface->DrawTexturedRect( static_cast< RenderTexturedRectRenderCommand_t & >( command ) );
			break;

		case k_ECmdLockTexture:
			m_p3DSurface->LockTexture( static_cast< LockTextureRenderCommand_t & >( command ) );
			break;

		case k_EDrawFilledRect:
			m_p3DSurface->DrawFilledRect( static_cast< RenderFilledRectRenderCommand_t & >( command ) );
			break;

		case k_EDrawTextRegion:
			m_p3DSurface->DrawTextRegion( static_cast< RenderTextRegionCommand_t & >( command ) );
			break;

		case k_EPushClipLayer:
			m_p3DSurface->PushClipLayer( static_cast< PushClipLayerRenderCommand_t & >( command ) );
			break;

		case k_EPopClipLayer:
			m_p3DSurface->PopClipLayer( static_cast< PopClipLayerRenderCommand_t & >( command ) );
			break;

		case k_EPushPanelContextInLayer:
			m_p3DSurface->PushPanelContextInLayer( static_cast< PushPanelContextInLayerRenderCommand_t & >( command ) );
			break;

		case k_EPopPanelContextInLayer:
			m_p3DSurface->PopPanelContextInLayer( static_cast< PopPanelContextInLayerRenderCommand_t & >( command ) );
			break;

		case k_EPushCompositingLayer:
			m_p3DSurface->PushCompositingLayer( static_cast< PushCompositingLayerRenderCommand_t & >( command ) );
			break;

		case k_EPopCompositingLayer:
			m_p3DSurface->PopCompositingLayer( static_cast< PopCompositingLayerRenderCommand_t & >( command ) );
			break;

		case k_ECmdFreeCompositingLayer:
			if ( commandList.m_unRenderCount == 1 )
			{
				m_p3DSurface->FreeCompositingLayer( static_cast< FreeCompositingLayerRenderCommand_t & >( command ) );
			}
			break;

		case k_ERequestRenderCallback:
			m_p3DSurface->RequestRenderCallback( static_cast< RequestRenderCallbackCommand_t & >( command ) );
			break;

		case k_ENestedCommand:
		{
			NestedRenderCommand_t &nestedCommand = static_cast< NestedRenderCommand_t & >( command );
			HandleRenderCommand( *nestedCommand.command, commandList );
			break;
		}

		default:
			Msg( "Render cmd %u unhandled!\n", command.eCommandType );
			break;
	}

	if ( s_convarPanoramaTrackRenderCommands.GetBool() )
	{
		m_commandStats.aCommandCounts[ command.eCommandType ]++;
		m_commandStats.unTotalCommandCount++;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Rendering thread
//-----------------------------------------------------------------------------
int CUIRenderEngine::CUIRenderThread::Run()
{
	CLimitTimer timer;

	while( !m_bExit )
	{
		// Don't sleep until we've rendered our buffer at least once, so render thread can proceed.
		if ( m_bSleepForValidate )
		{
			bool bOkToSleep = true;
			{
				AUTO_LOCK( m_pRenderEngine->m_RenderingLock );
				if ( m_pRenderEngine->m_pCurrentRenderList && m_pRenderEngine->m_pCurrentRenderList->m_unRenderCount == 0 )
				{
					bOkToSleep = false;
				}
			}

			// Don't sleep yet if we haven't rendered the buffer we have, as it will block the animation thread from sleeping.
			if ( bOkToSleep )
			{
				while ( m_bSleepForValidate )
				{
					m_bSleepingForValidate = true;
					ThreadSleep( 100 );
				}
			}
		}
		m_bSleepingForValidate = false;

		// This is the max framerate, which may really get limited down due to vsync, but with vsync off
		// this would be the real max, or if a user has some crazy high vsync, this is the max.
		timer.SetLimit( MAX( m_p3DSurface->GetMinFrameTimeInMicroseconds(), ((1.0f / m_flMaxFPS) * k_nMillion) ) );

		if ( m_pRenderEngine->m_pWindow->BIsVisible() == false )
		{
			ThreadSleep( 100 );
			continue;
		}

		RunSingleFrame();

		// Sleep as needed, but always at least 1 millisecond to yield some time
		// to the paint thread and ensure we don't spin fast if it's trying to lock
		// for SwapBuffers() to give us new data.
		{
			VPROF_BUDGET_THREAD( "Sleep - FPS Limiting", VPROF_BUDGETGROUP_TENFOOT );
			if ( timer.BLimitReached() )
				ThreadSleep( 1 );
			else
			{
				// Extra limiting logic here to make sure we never sleep really long because
				// some bad/overclocked CPUs are known to break CMicroSecLeft and sometimes return crazy
				// values.  We won't have optimal behavior on those, but we can try not to be crazy broken.
				int nWaits = 10;
				while ( !timer.BLimitReached() && nWaits > 0 )
				{
					int64 msLeft = timer.CMicroSecLeft() / 1000;
					m_WakeEvent.Wait( MIN( msLeft, 20 ) );
					--nWaits;
				}
			}
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: main engine is telling us a file changed
//-----------------------------------------------------------------------------
void CUIRenderEngine::ReloadChangedFile( const char *pchFile )
{
	Access3DSurface()->ReloadChangedFile( pchFile );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Suspend the Render and animation thread so we can validate their mem
//-----------------------------------------------------------------------------
bool CUIRenderEngine::PauseAnimationAndRenderThreadForValidate()
{
	m_AnimationThread.SleepForValidate();
	while( !m_AnimationThread.BSleepingForValidate() )
		ThreadSleep( 100 );

#if !defined( SOURCE2_PANORAMA )
	m_RenderThread.SleepForValidate();
	while( !m_RenderThread.BSleepingForValidate() )
		ThreadSleep( 100 );
#endif

	m_bRenderAndAnimationThreadPausedForValidate = true;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: turn the threads back on
//-----------------------------------------------------------------------------
bool CUIRenderEngine::ResumeAnimationAndRenderThreadFromValidate()
{
	Assert( m_bRenderAndAnimationThreadPausedForValidate );
	m_bRenderAndAnimationThreadPausedForValidate = false;
	m_AnimationThread.WakeFromValidate();
#if !defined( SOURCE2_PANORAMA )
	m_RenderThread.WakeFromValidate();
#endif
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CUIRenderEngine::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	ValidateObj( m_AnimationThread );
	ValidateObj( m_RenderThread );

	ValidateObj( m_vecTexturesToDelete );
	ValidateObj( m_vecParticleSystemsToDelete );
	ValidateObj( m_vecDeletedPanelsThisFrame );

	if ( m_pCurrentPaintBuffer )
	{
		validator.ClaimMemory_Aligned( (void*)m_pCurrentPaintBuffer );
		ValidatePtr( m_pCurrentPaintBuffer->elem );
	}

	if ( m_pCurrentAnimationBuffer )
	{
		validator.ClaimMemory_Aligned( (void*)m_pCurrentAnimationBuffer );
		ValidatePtr( m_pCurrentAnimationBuffer->elem );
	}


	if ( m_pCurrentRenderBuffer )
	{
		validator.ClaimMemory_Aligned( (void*)m_pCurrentRenderBuffer );
		ValidatePtr( m_pCurrentRenderBuffer->elem );
	}

	ValidateObj( m_tslUnusedBuffers );

	CTSList<RenderCommandBuffer_t*>::Node_t *pNode = m_tslUnusedBuffers.Detach();
	while ( pNode  )
	{
		CTSList<RenderCommandBuffer_t*>::Node_t *pNext = (CTSList<RenderCommandBuffer_t*>::Node_t *)pNode->Next;
		ValidatePtr( pNode->elem );
		m_tslUnusedBuffers.Push( pNode );
		pNode = pNext;
	}
}
#endif
