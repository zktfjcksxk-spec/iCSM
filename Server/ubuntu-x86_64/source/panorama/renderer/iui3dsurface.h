//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUI3DSURFACE_H
#define IUI3DSURFACE_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/renderer/rendercommands.h"
#include "color.h"
#if !defined( SOURCE2_PANORAMA )
#include "../../overlay/common/shmemstream.h"
#include "uitoplevelwindowoverlay.h"
#endif

namespace panorama
{

#define PANORAMA_FRAMES_FOR_FPS_AVERAGES 10

class IUI3DSurface
{
public:

	virtual ~IUI3DSurface() {}

	// Called to access the paint time of the last rendered frame (ie, time it was first generated in paint thread)
	virtual double GetLastFramePaintTime() = 0;

	// is the cursor currently visible, the render thread manages cursor state so this lets other threads discover it
	virtual bool BCursorVisible() = 0;

	// wakeup the mouse cursor
	virtual void WakeupMouseCursor() = 0;

	// fadeout the mouse cursor
	virtual void FadeOutCursorNow() = 0;

	// main engine noticed that a file on disk changed, see if we want to reload it
	virtual void ReloadChangedFile( const char * ) = 0;
	
	// Called at the start of every frame. Should setup general state for the frame, call beginscene(), etc.
	virtual void BeginFrame( const BeginFrameRenderCommand_t &renderCommand ) = 0;

	// Called at the end of every frame.  Should do any per-frame cleanup, call endscene(), etc.
	virtual void EndFrame( const EndFrameRenderCommand_t &renderCommand ) = 0;

	// Called to clear the backbuffer to a solid color.
	virtual void ClearBackbuffer( const ClearBackbufferRenderCommand_t &renderCommand ) = 0;

	// Called to draw a filled quad
	virtual void DrawFilledRect( const RenderFilledRectRenderCommand_t &renderCommand ) = 0;

	// Handle drawing text region
	virtual void DrawTextRegion( const RenderTextRegionCommand_t &renderCommand ) = 0;
	virtual void DrawTextRegionOriginal( const RenderTextRegionCommand_t &renderCommand ) = 0;

	// Called to push a new compositing layer
	virtual void PushCompositingLayer( const PushCompositingLayerRenderCommand_t &renderCommand ) = 0;

	// Called to pop a compositing layer, which leads to rendering it to the parent layer or backbuffer with transforms/opacity/etc applied
	virtual void PopCompositingLayer( const PopCompositingLayerRenderCommand_t &renderCommand ) = 0;

	// Called to push a clip layer
	virtual void PushClipLayer( const PushClipLayerRenderCommand_t &renderCommand ) = 0;

	// Called to pop a clip layer
	virtual void PopClipLayer( const PopClipLayerRenderCommand_t &renderCommand ) = 0;

	// Called to push a transform matrix
	virtual void PushPanelContextInLayer( const PushPanelContextInLayerRenderCommand_t &renderCommand ) = 0;
	
	// Called to track panels for selective blur
	virtual void ResetBlurPanels() = 0;
	virtual void AddBlurPanel( uint64 panelID, const CRenderDataList< uint64 > &srcPanels ) = 0;
	virtual void AddBlurPanelData( uint64 panelID, const Vector2D &topLeft, const Vector2D &bottomRight, const RenderMatrix4x4_t &matrix  ) = 0;
	virtual void AddSourcePanel( uint64 panelID ) = 0;
	virtual void AddSourcePanelData( uint64 panelID, const Vector2D &topLeft, const Vector2D &bottomRight, const RenderMatrix4x4_t &matrix ) = 0;

	// Called to pop a transform matrix
	virtual void PopPanelContextInLayer( const PopPanelContextInLayerRenderCommand_t &renderCommand ) = 0;

	// Handle render callback request (basically calls back raw panel code to draw direct to surface, source 2 only noop elsewhere)
	virtual void RequestRenderCallback( const RequestRenderCallbackCommand_t &renderCommand ) = 0;

	// Called to draw a textured quad with a double buffered texture object
	virtual void DrawTexturedRect( const RenderTexturedRectRenderCommand_t &renderCommand ) = 0;

	// Just lock a texture, used to increment it's draw serial when it's actually being culled
	virtual void LockTexture( const LockTextureRenderCommand_t &renderCommand ) = 0;

	// Thread safe function which lets another thread verify we have a cached representation of a given layer
	// so it can be re-rendered without a full redraw, and also makes sure we don't LRU it right away moving it
	// to the end of our list for throw out.
	virtual bool PingCompositingLayer( uint64 ulLayerID, float flWidth, float flHeight ) = 0;

	// Thread safe function which lets another thread get the render target name of a given layer
	// Returns nullptr if not found
	virtual const char *GetCompositionLayerRenderTargetName( uint64 ulLayerID ) = 0;

	// Force throwing out of a compositing layer, called from animation thread.  This makes sure we stop re-using a layer
	// which the animation thread knows it's culling render operations to and thus knows is now dirty and not re-usuable
	virtual void FreeCompositingLayer( const FreeCompositingLayerRenderCommand_t &renderCommand ) = 0;

	// Get the FPS average from the render thread from the last few frames
	virtual float GetFPSAverage() = 0;

	// Get the FPS average since creation
	virtual float GetSessionFPSAverages() = 0;

	// Ask if VSync is enabled
	virtual bool BVsyncEnabled() = 0;

	// Get the minimum frame time allowed, which is used to limit max fps, too fast and we'll get lots of lock 
	// contention with layout threads on movie texture updates and such
	virtual uint64 GetMinFrameTimeInMicroseconds() = 0;

	// If returns true, there's nothing to draw, there's no visibility to the frame
	// right now.
	virtual bool BSurfaceOccluded() = 0;

	// Get the scaling factor that is being applied to all UI by the framework
	virtual float GetWindowScaleFactor() = 0;

	// Set the scaling factor that is being applied to all UI by the framework
	virtual void SetWindowScaleFactor( float flScaleFactor ) = 0;

	virtual void PushOverlayRenderCmdStream( CSharedMemStream *pRenderStream, DWORD dwPID, float flOpacity, uint32 unGameWidth, uint32 unGameHeight, EOverlayWindowAlignment alignment ) = 0;
	// Set desired output texture format for overlay surfaces
	virtual void SetOverlayTextureFormat( int32 eTextureFormat ) = 0;

	// Set the letterboxing color used by the overlay, only implemented by overlay surfaces
	virtual void SetOverlayLetterboxColor( Color c ) { }

	// return the number of periods we saw below the min fps since panorama creation
	virtual volatile int GetNumPeriodsBelowMinFPS() = 0;

	// Helper to tell surface directly where steam pad pointer is... thread safe
	virtual void UpdateSteamPadPointers( SteamPadPointer_t* leftPointer, SteamPadPointer_t* rightPointer ) = 0;

#if defined( SOURCE2_PANORAMA )
	virtual void SetRenderContext( ISceneView *pView, IRenderContext *pRenderContext, ISceneLayer *pLayer ) = 0;

	// If the specified text region exists in the "text layout draw cache", sets its timestamp and
	// moves it to the end of the LRU list
	virtual void TouchTextRegionInCache( const RenderTextRegionCommand_t &renderCommand ) = 0;

	// If the specified text region doesn't exist in the "text layout draw cache", kick off
	// an async job to populate it.
	virtual void AsyncAddTextRegionToCache( const DrawTextRegionRenderCommand_t &renderCommand ) {}

#endif 
};

} // namespace panorama

#endif // IUI3DSURFACE_H
