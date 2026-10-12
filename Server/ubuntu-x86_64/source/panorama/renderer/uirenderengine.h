//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UIRENDERENGINE_H
#define UIRENDERENGINE_H
#pragma once

#include "iui3dsurface.h"
#include "iuirenderengine.h"
#include "tier0/threadtools.h"
#include "tier1/utlbuffer.h"
#include "panorama/renderer/rendercommands.h"
#include "panorama/transformations.h"
#include "uianimationengine.h"
#include "styles.h"
#include "panorama/text/iuitextlayout.h"
#include "tier0/tslist.h"

namespace panorama
{

class CUIWindowInput;
class CTopLevelWindow;
class CUIEngine;
class IUI3DSurface;
class IUITexture;
class CRenderCommandList;

#define DEBUG_SCREENSPACE_QUAD_OUTPUT 0

struct ScreenSpacePanelQuad_t
{
public:

	ScreenSpacePanelQuad_t( ScreenSpacePanelQuad_t  *pParent )
	{
		m_pQuad = NULL;

		m_pParent = pParent;
		m_pNextPeer = NULL;
		m_pFirstChild = NULL;
	}

	~ScreenSpacePanelQuad_t()
	{
		if ( m_pQuad )
		{
			delete[] m_pQuad;
			m_pQuad = NULL;
		}

		if ( m_pNextPeer )
		{
			delete m_pNextPeer;
			m_pNextPeer = NULL;
		}

		if ( m_pFirstChild )
		{
			delete m_pFirstChild;
			m_pFirstChild = NULL;
		}

		// Don't delete upwards, delete starts from top level parent
		m_pParent = NULL;
	}

	uint64 m_ulPanelContextID;
	Vector2D * m_pQuad;
	ScreenSpacePanelQuad_t *m_pParent;
	ScreenSpacePanelQuad_t *m_pNextPeer;
	ScreenSpacePanelQuad_t *m_pFirstChild;

#if DEBUG_SCREENSPACE_QUAD_OUTPUT
	CUtlString m_strPanel;
#endif
};


//-----------------------------------------------------------------------------
// Purpose: Helper to check if point is in a transformed but still convex quad
//-----------------------------------------------------------------------------
template< class T >
bool BPointInsideConvexQuad( float x, float y, T *pQuad )
{
	bool bPointInside = true;

	float flSide = 0; // 0 means on a line
	for ( int i = 0; i < 4; i++ )
	{
		int iPrev = (i == 0) ? 4 - 1 : i - 1;
		float flCheck = ((y - pQuad[iPrev].y) * (pQuad[i].x - pQuad[iPrev].x)) - ((x - pQuad[iPrev].x) * (pQuad[i].y - pQuad[iPrev].y));

		// if we haven't found a side yet, update and continue
		if ( flSide == 0 )
		{
			flSide = flCheck;
			continue;
		}

		// if previously calculated side doesn't equal our value, point is outside of quad. If nCheck is 0, treat this iteration as matching
		if ( flSide > 0 && flCheck < 0 )
		{
			bPointInside = false;
			break;
		}
		else if ( flSide < 0 && flCheck > 0 )
		{
			bPointInside = false;
			break;
		}
	}

	return bPointInside;
}

class CUIRenderEngine : public IUIRenderEngine
{
public:

	// Constructor
	CUIRenderEngine( CUIEngine *pUIEngineParent, IUI3DSurface *pSurface, CUIWindowInput *pInput, CTopLevelWindow *pWindow, uint32 unSurfaceWidth, uint32 unSurfaceHeight );
	~CUIRenderEngine();

	void OnPanelDeleted( IUIPanel *pPanel );

	// Functions for UI thread to render a frame
	void BeginFrame( uint32 unSurfaceWidth, uint32 unSurfaceHeight, IUIEngine::ERenderTarget eTarget, Color clearColor, float flUIScaleFactor, bool bEmptyFrame, bool bClearGPUResourcesBeforeFrame );
	void EndFrame();

	// Pushes animation/transform context, ie, tells us we've descended into a child context with a new
	// set of transformations/animations that will apply to all rendering operations until the corresponding pop call.
	void PushAnimationAndTransformContext( uint64 unContextID, uint32 unChildPanelCount, uint32 unStylesPresentFlags, float wide, float tall, CPanelStyle *pStyle, bool bNoChildrenOutsideBounds, bool bChildrenHave3DTransforms,
		 EPanelRepaint ePanelRepaint, bool bNoClip, bool bWantsHitTest, bool bWantsHitTestChildren, bool bNeedsIntermediateTexture, bool bClipAfterTransform, bool bWantsScreenspaceQuadOutput, const char *pszCompositionLayerTextureName,
		 bool bRequireCompositionLayer, bool bForceNoCompositionLayer, bool bAlwaysCacheCompositionLayer, bool bOffscreenCompositionLayer, EFractionalPixelPositions eFractionalPixelPositions );

	// Push transform operations within the current animation/transform context
	void Push3DTransformMatrix( const VMatrix &transformMatrix );

	// Pops animation/transform context, ie, tells us we've left a child context, and should clear the transforms/animations
	// from that context and stop applying them to future rendering operations.
	void PopAnimationAndTransformContext( uint64 unContextID );

	// Signal beginning of painting background contents for current context
	void BeginPaintBackground();

	// Signal ending of painting background contents for current context
	void EndPaintBackground();

	// Signal beginning of painting contents for current context which should be drawn last after normal children
	void BeginPaintLast();

	// Signal ending of painting contents for current context which should be drawn last after normal children
	void EndPaintLast();

	// Tell animation thread to stop further interpolating property and get back the time we stopped it at for layout 
	// thread to finish transition in matching manner.
	float StopAnimationOfPropertyUntilFrameUpdateAndGetStopTime( uint64 ulPanelContextID, uint32 hStyleSymbol );

	// Draw a filled quad. It is the caller's responsibility to fill in the returned FillBrushCollectionWithTransition_t with the appropriate colors.
	virtual FillBrushCollectionWithTransition_t *DrawFilledRect( uint64 unContextID, float x0, float y0, float x1, float y1, EAntialiasing antialiasing = k_EAntialisingEnabled );

	// Draw a filled quad with a solid color
	virtual void DrawSolidColorRect( float x0, float y0, float x1, float y1, uint32 unColorRGBA, EAntialiasing antialiasing = k_EAntialisingEnabled ) OVERRIDE;
	// Send down blur rectangles
	void PushBlurPanels( const CUtlVector<uint64> &blurPanels );


	// Draw a textured quad
	virtual void DrawTexturedRect( IUITexture *pTexture, ETextureSampleMode eSampleMode, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1 ) OVERRIDE;

	// Draw a textured quad with an opacity that might change over time
	virtual void DrawTexturedRectOpacity( IUITexture *pTexture, ETextureSampleMode eSampleMode, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, OpacityWithTransition_t *pOpacityData );

	// Draw one of the special synchronized textures
	virtual void DrawSyncronizedTexturedRect( IUITexture *pTexture, ETextureSampleMode eSampleMode, int32 unSerialize, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1 ) OVERRIDE;

	// Draw text. Versions for UTF8, UTF16, and UTF32. It is the caller's responsibility to fill out default_format.fill_brush_collection and optionally range_formats
	virtual DrawTextRegionRenderCommand_t *DrawTextRegion( const char *pchText, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 ) OVERRIDE;
	virtual DrawTextRegionRenderCommand_t *DrawTextRegion( const uchar16 *pchText, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 ) OVERRIDE;
	virtual DrawTextRegionRenderCommand_t *DrawTextRegion( const uchar32 *pchText, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 ) OVERRIDE;
	DrawTextRegionRenderCommand_t *DrawTextRegionInternal( const void *pRawText, uint32 nTextBytes, int cTextChars, EPanoramaTextEncoding eTextEncoding, const char *pchFontName, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 );

	// Draw solid colored text that doesn't have any animation/transition
	virtual void DrawSolidColorTextRegion( const char *pchText, const char *pchFontName, uint32 unColor, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 ) OVERRIDE;
	virtual void DrawSolidColorTextRegion( const uchar16 *pchText, const char *pchFontName, uint32 unColor, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 ) OVERRIDE;
	virtual void DrawSolidColorTextRegion( const uchar32 *pchText, const char *pchFontName, uint32 unColor, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 ) OVERRIDE;
	void DrawSolidColorTextRegionInternal( const void *pRawText, uint32 nTextBytes, int cTextChars, EPanoramaTextEncoding eTextEncoding, const char *pchFontName, uint32 unColor, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1 );

#if defined( SOURCE2_PANORAMA )
	// Tell the render thread to call the panel back on the specified method, which will then be able to do 
	// direct render system calls on the render thread
	virtual void RequestRenderCallback( CRenderThreadCallback *pCallbackObj, float x0, float y0, float x1, float y1, 
		float flPaddingLeft, float flPaddingRight, float flPaddingTop, float flPaddingBottom, ERenderCallbackFlags eFlags, IUITexture *pPanelRT ) OVERRIDE;
#endif
	
	// Draw an already cached command list
	void DrawCachedCommandList( CRenderCommandList &commandList );

	// Start rendering to a nested command list
	CRenderCommandList &PushPaintCommandList();
	
	// Stop rendering to a nested command list
	void PopPaintCommandList();

	virtual void UpdateSteamPadPointers( SteamPadPointer_t* leftPointer, SteamPadPointer_t* rightPointer ) { Access3DSurface()->UpdateSteamPadPointers( leftPointer, rightPointer); }

	// Queue to delete a particle system, this is how the animation thread finds out about particle systems going away
	void QueueParticleSystemDelete( uint64 ulPanelHandle, uint32 unIndex );

	// Handle window resizing
	void OnWindowResize( uint32 unWidth, uint32 unHeight );

	// Used by paint thread to swap in a new empty paint buffer, must own the lock before calling!
	void StartNewPaintBuffer();

	// Functions for synchronizing on swap buffers for a window
	void ClearSwapBuffersEvent();
	void SetSwapBuffersEvent();
	void WaitOnSwapBuffersEvent( float flMillisecondsTimeout );
	void WakeThreads();
	void WakeAnimationThread();

	double GetLastPaintFrameTime() { return m_LastPaintFrameTime; }
	double GetLastPaintFrameTimeRendered() { return m_LastPaintFrameTimeRendered; }

	// Get FPS to show in debug UI
	void GetFPSAverages( float &fpsPaint, float &fpsAnimation, float &fpsRender );
	// FPS average across the whole process lifetime
	void GetSessionFPSAverages( float &fpsPaint, float &fpsAnimation, float &fpsRender );

	IUI3DSurface * Access3DSurface() { return m_RenderThread.Access3DSurface(); }

	void ReloadChangedFile( const char *pchFile );

	void SetMaxFPS( float flMaxFPS ) { m_RenderThread.SetMaxFPS( flMaxFPS ); }

	void GetNumPeriodsBelowMinFPS( int &nSlowPeriods ) { m_RenderThread.GetNumPeriodsBelowMinFPS( nSlowPeriods ); }

	void UpdatePanelScreenspaceQuadCoordinates( ScreenSpacePanelQuad_t *pTreeScreenspaceQuads ); 

	IUIPanel * HitTestCoordsAgainstLatestScreenspaceQuadCoordinates( float xSurface, float ySurface );
	
#if defined( SOURCE2_PANORAMA )
	void RunRenderThreadFrame( ISceneView *pView, IRenderContext *pRenderContext, ISceneLayer *pLayer, bool bAdvanceAnimation );
#endif

	// Return the currently active paint command list. These methods do the same thing, but for code internal
	// to panorama prefer GetCurrentCommandList to avoid the virtual overhead.
	CRenderCommandList &GetCurrentCommandList() { return *m_vecPaintCommandListStack.Tail(); }
	virtual CRenderCommandList &GetCommandList() OVERRIDE { return GetCurrentCommandList(); }

	static void DumpCommandListStats();

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );

	bool PauseAnimationAndRenderThreadForValidate();
	bool ResumeAnimationAndRenderThreadFromValidate();
#endif

private:

	void DrawTexturedRectCore( IUITexture *pTexture, ETextureSampleMode eSampleMode, int32 unSerialize, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, OpacityWithTransition_t *pOpacityData );

	struct RenderCommandList_t
	{
		RenderCommandList_t();
		~RenderCommandList_t();

		uint32 m_unRenderCount;
		CSmartPtr< CRenderCommandList > m_pCommandList;
	};

	template < typename T > T *PushPaintCommand()
	{
		return GetCurrentCommandList().PushCommand< T >();
	}

	template < typename T > void SetupOptionalPropertyPaintData( CRenderDataListBaseBuilder &listProperties, EStyleOptionalProperty eOptionalProperty, CPanelStyle *pStyle )
	{
		OptionalProperty_t< T > *pProperty = listProperties.AddToTailTyped< OptionalProperty_t< T > >();
		pProperty->property_type = eOptionalProperty;
		pStyle->GetRenderData( pProperty->property_data, GetCurrentCommandList() );
	}

	const char *CopyPaintString( const char *pszInput );

	static void UpdateCommandListStatsAverages( RenderCommandListStats_t &averages, const RenderCommandListStats_t &stats );
	static void DumpCommandListStatsAverages( const char *pszType, const RenderCommandListStats_t &averages );

	// Actual animation thread, which performs animation interpolation at fixed frame rate before passing on to render thread
	class CUIAnimationThread : public CValidatableThread
	{
	public:
		CUIAnimationThread( CUIRenderEngine *pRenderEngine, CUIWindowInput *pInput )
		{
			m_bExit = false;
			m_pRenderEngine = pRenderEngine;
			m_pInput = pInput;
			m_pAnimationEngine = new CUIAnimationEngine( m_pRenderEngine );
			m_unSurfaceWidth = 0;
			m_unSurfaceHeight = 0;
		}

		virtual ~CUIAnimationThread() { delete m_pAnimationEngine;  m_pAnimationEngine = NULL; }

		void TriggerShutdown() { m_bExit = true; m_WakeEvent.Set(); }
		void WakeThread() { m_WakeEvent.Set(); }

		virtual int Run();

		void RunSingleFrame();

		float GetFPSAverage() { return m_pAnimationEngine->GetFPSAverage(); }
		float GetSessionFPSAverages() { return m_pAnimationEngine->GetSessionFPSAverages(); }

		// Tell animation thread to stop further interpolating property and get back the time we stopped it at for layout 
		// thread to finish transition in matching manner.
		float StopAnimationOfPropertyUntilFrameUpdateAndGetStopTime( uint64 ulPanelContextID, uint32 hStyleSymbol )
		{
			return m_pAnimationEngine->StopAnimationOfPropertyUntilFrameUpdateAndGetStopTime( ulPanelContextID, hStyleSymbol );
		}

		void HandleRenderCommand( RenderCommand_t &renderCommand, CRenderCommandList &outputCommandList );

#ifdef DBGFLAG_VALIDATE
		virtual void Validate( CValidator &validator, const tchar *pchName )
		{
			VALIDATE_SCOPE();
			ValidatePtr( m_pAnimationEngine );
		}
#endif

	private:
		bool m_bExit;
		CUIRenderEngine *m_pRenderEngine;
		CUIAnimationEngine *m_pAnimationEngine;
		CThreadEvent m_WakeEvent;
		CUIWindowInput *m_pInput;

		CThreadMutex m_SurfaceSizeLock;

		uint32 m_unSurfaceWidth;
		uint32 m_unSurfaceHeight;

		RenderCommandListStats_t m_commandStats;
	};
	friend class CUIAnimationThread;

	// Actual render thread, which performs actual drawing at fixed frame rate, repeating prior ui state
	// or performing animation interpolation on it as appropriate if a full new ui frame has not completed.
	class CUIRenderThread : public CValidatableThread
	{
	public:
		CUIRenderThread( CUIRenderEngine *pRenderEngine, IUI3DSurface *p3DSurface )
		{
			m_bExit = false;
			m_pRenderEngine = pRenderEngine;
			m_p3DSurface = p3DSurface;
			m_flMaxFPS = 120.0f;
		}

		void TriggerShutdown() { m_bExit = true; m_WakeEvent.Set(); }
		void WakeThread() { m_WakeEvent.Set(); }

		IUI3DSurface * Access3DSurface() { return m_p3DSurface; }

		float GetFPSAverage() { return m_p3DSurface->GetFPSAverage(); }
		float GetSessionFPSAverages() { return m_p3DSurface->GetSessionFPSAverages(); }

		void SetMaxFPS( float flMaxFPS ) { m_flMaxFPS = flMaxFPS; }

		void GetNumPeriodsBelowMinFPS( int &nSlowPeriods ) { nSlowPeriods = m_p3DSurface->GetNumPeriodsBelowMinFPS(); }
		virtual int Run();

		void ExtractBlurRectangles( RenderCommandList_t *pRenderList );
		void RunSingleFrame();

		void HandleRenderCommand( RenderCommand_t &command, const RenderCommandList_t &commandList );

#ifdef DBGFLAG_VALIDATE
		virtual void Validate( CValidator &validator, const tchar *pchName )
		{
			VALIDATE_SCOPE();
			// m_pRenderEngine validated from the parent
		}
#endif
	private:
		bool m_bExit;
		CUIRenderEngine *m_pRenderEngine;
		IUI3DSurface *m_p3DSurface;
		CThreadEvent m_WakeEvent;
		float m_flMaxFPS;

		RenderCommandListStats_t m_commandStats;
	};
	friend class CUIRenderThread;

	CUIWindowInput *m_pInput;

	CUIAnimationThread m_AnimationThread;
	CUIRenderThread m_RenderThread;

	// our container UIEngine object
	CUIEngine *m_pUIEngineParent;

	CTopLevelWindow *m_pWindow; // our container window

	// Track surface size
	uint32 m_unSurfaceWidth;
	uint32 m_unSurfaceHeight;

	double m_LastPaintFrameTime;
	double m_LastPaintFrameTimeRendered;
	CThreadEvent m_SwapBuffersEvent;

	CFastTimer m_FrameTimer;
	int m_nLastFrameMillisecondsIndex;
	float m_rgflMillisecondsFrame[ PANORAMA_FRAMES_FOR_FPS_AVERAGES ];
	int m_nFramesRendered;
	double m_flFramePaintTime;

	CThreadMutex m_PaintingLock;
	CThreadMutex m_RenderingLock;

	// Every frame, paint builds a cmd list on the main thread, which the anim job processes next frame on the render thread.
	// Need a double buffer of cmd list ptrs because, on a given frame, paint can be ready to start building a new cmd list, but the
	// anim job may not have run yet on the render thread, so the anim hasn't had a chance to save off the ptr to the list
	// that paint built last frame. The safe way to flip this double buffer is in MaterialSystem EndFrame, when both paint and 
	// anim are guaranteed to be finished, but hooking into this mechanism is complicated in this case, as there are multiple
	// instances of CUIRenderEngine, one per top level window. Using a queue instead.
	CUtlQueue < RenderCommandList_t * > m_paintListsQueue;
	RenderCommandList_t *m_pCurrentPaintList;
	RenderCommandList_t *m_pCurrentAnimationList;
	RenderCommandList_t *m_pCurrentRenderList;

	CUtlVector< CRenderCommandList * > m_vecPaintCommandListStack;

	static CThreadMutex s_CommandStatsLock;
	static RenderCommandListStats_t s_averagePaintCommandListStats;
	static RenderCommandListStats_t s_averageRenderCommandListStats;

	CUtlVector< uint64 > m_vecDeletedPanelsThisFrame;

	struct ParticleSystemDelete_t
	{
		uint64 ulPanelHandle;
		uint32 unIndex;
	};
	CUtlVector< ParticleSystemDelete_t > m_vecParticleSystemsToDelete;

	CThreadMutex m_MutexScreenspaceQuadTree;
	ScreenSpacePanelQuad_t *m_treePanelScreenspaceQuads;

#ifdef DBGFLAG_VALIDATE
	bool m_bRenderAndAnimationThreadPausedForValidate;
#endif
};



} // namespace panorama

#endif // UIRENDERENGINE_H
