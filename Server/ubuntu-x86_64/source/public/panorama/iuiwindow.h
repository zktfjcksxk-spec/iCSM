//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUIWINDOW_H
#define IUIWINDOW_H

#ifdef _WIN32
#pragma once
#endif


#include "panoramatypes.h"
#include "iuirenderengine.h"
#include "input/mousecursors.h"

#if defined( SOURCE2_PANORAMA )
#include "rendersystem/irendercontext.h"
// #include "scenesystem/isceneview.h"
#endif

#if defined( _WIN32 ) && !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
#define PANORAMA_STEAMUI_STREAMING_CAPTURE_WIN32
typedef void( *SteamUIStreamingCaptureCallback_t )( void *p3D10Device, void *pUnknownBackBuffer );
#endif

class CSharedMemStream;

namespace panorama
{

class CLayoutFile;
class IUIPanelStyle;
class IUIOverlayWindow;
class IUIWindowInput;

//-----------------------------------------------------------------------------
// Purpose: Basic window interface exposing operations used inside of panorama, rather
// than operations that are part of building/laying out controls in the panorama_client module
//-----------------------------------------------------------------------------
class IUIWindow
{
public:

#if defined( SOURCE2_PANORAMA )
	virtual void RenderWindow( PlatWindow_t hwnd, bool bDebugger ) = 0;
	virtual bool BUseAutoMouseUpBehavior() = 0;

	virtual void SetAnimationDisabled( bool bAnimate ) = 0;

	virtual void SetForceConsumeKBAndMouseInputEvents( bool bForce ) = 0;
	virtual bool BForceConsumeKBAndMouseInputEvents() = 0;
#endif

#ifdef PLATFORM_WINDOWS
	// src1 panorama debugger needs to grab the render target
	virtual void CopyDebuggerRtBits( uint32* pBitsOut, uint32 nWd, uint32 nHt ) = 0;
#endif

	// Delete the window object
	virtual void Delete() = 0;

	// Access input system for the window
	virtual IUIWindowInput *UIWindowInput() = 0;

	// Access the rendering interface you use to draw onto this window
	virtual IUIRenderEngine * UIRenderEngine() = 0;

	// Access the rendering device you use to create textures for this window
	virtual IUIRenderDevice *UIRenderDevice() = 0;

	// Set scaling factor that applies to all x/y values in the UI for the window, used so we can 
	// author content at say 1080p but pass 0.6666666f for this to render in 720p on cards with poor 
	// fill rates or TVs without 1080p support.
	virtual void SetWindowScaleFactor( float flScaleFactor ) = 0;
	virtual float GetWindowScaleFactor() = 0;

	// Check if the mouse cursor is currently visible within the window
	virtual bool BCursorVisible() = 0;

	// Get the surface width for the window, this may be larger or smaller than the actual window if we are rendering lower res and scaling output for instance
	virtual uint32 GetSurfaceWidth() = 0;

	// Get the surface height for the window, this may be larger or smaller than the actual window if we are rendering lower res and scaling output for instance
	virtual uint32 GetSurfaceHeight() = 0;

	// Get the actual window width
	virtual uint32 GetWindowWidth() = 0;

	// Get the actual window height
	virtual uint32 GetWindowHeight() = 0;

	// Get the client area of the window, minus window borders, etc
	virtual void GetClientDimensions( float &width, float &height ) = 0;

	// Handle resize event, window has resized, backing surface needs to now resize to match
	virtual void OnWindowResize( uint32 width, uint32 height ) = 0;

	// Set window position on screen
	virtual void SetWindowPosition( float x, float y ) = 0;

	// Get window position on screen
	virtual void GetWindowPosition( float &x, float &y ) = 0;

	// bForceful in this call means try to bypass OS rules that may prevent focus (like our app wasn't last to receive input), 
	// use this sparingly and only if we really know stealing focus from some other app is what the user definately desires!
	virtual void Activate( bool bForceful ) = 0;
	
	virtual void Minimize() = 0;

	virtual void SetTopMost( bool bTopMost ) = 0;

	// Check if this window has focus currently
	virtual bool BHasFocus() = 0;

	// Get the number of visible top level panels in the window
	virtual uint32 GetNumVisibleTopLevelPanels() const = 0;

	// Access list of visible panels
	virtual const CUtlLinkedList< IUIPanel* > &GetTopLevelVisiblePanels() const = 0;

	// Check if mouse is over the window
	virtual bool IsMouseOver() = 0;

	// used by the os specific case to update the cursor
	virtual void SetMouseCursor( EMouseCursors eCursor ) = 0;

	// Helper for letting client code tell us about daisywheel usage
	virtual void RecordDaisyWheelUsage( float flEntryTimeInSeconds, int nWordsEntered, bool bViaKeyboard, bool bViaGamepad ) = 0;

	// Helper for tracking daisywheel usage
	virtual void GetDaisyWheelWPM( int &nWordsTyped, float &flMixedWPM, float &flKeyboardOnlyWPM, float &flGamepadOnlyWPM ) = 0;
	
	// Check if the window is inside it's own layout pass
	virtual bool BIsWindowInLayoutPass() = 0;

	// Set a context ptr that is attached to the window, just lets other code (panels) that
	// has access to the window access some shared state across the window.
	virtual void SetContextPtr( void *pv ) = 0;

	// Get context ptr value for this window,
	virtual void * GetContextPtr() const = 0;

	virtual bool BIsOverlay() = 0;
	virtual bool BIsSteamWMOverlay() = 0;

	// Get low level native window handle, used by streaming, should generally not be something you use
	virtual void* GetNativeWindowHandle() = 0;

	// Forces the window to be hidden - no icon/taskbar/etc.
	virtual void ForceHideWindow() = 0;

	// Add a class to every top level panel in the window
	virtual void AddClass( const char *pchName ) = 0;

	// Add a class from every top level panel in the window
	virtual void RemoveClass( const char *pchName ) = 0;

	// Add or remove a class from every top level panel in the window.
	virtual void SetHasClass( const char *pchName, bool bHasClass ) = 0;

	// Force a full repaint on all top level panels in the window.
	virtual void ForceFullRepaint() = 0;

	virtual void SetInhibitInput( bool bInhibitInput ) = 0;
	virtual void SetPreventForceWindowOnTop( bool bPreventForceTopLevel ) = 0;

	// Fade out the mouse cursor immediately in this window
	virtual void FadeOutCursorNow() = 0;

	// Wakeup and show the mouse cursor immediately
	virtual void WakeupMouseCursor() = 0;

	virtual void EnableControllerCursor( bool bEnable ) = 0;
	virtual bool BIsControllerCursorEnabled() = 0;
	virtual void SetHideCursorOnInactivity( bool bHide ) = 0;

	// Set a min FPS for the window, this actually just prevents setting the max lower
	virtual void SetMinFPS( float flMinFPS ) = 0;

	// Stats tracking
	virtual void GetSessionFPSAverages( float &fpsPaint, float &fpsAnimation, float &fpsRender ) = 0;

	// Stats tracking
	virtual void GetNumPeriodsBelowMinFPS( int &nSlowPeriods ) = 0;

	// access data about how the gamepad was used
	virtual bool BWasGamepadConnectedThisSession() = 0;
	virtual bool BWasGamepadUsedThisSession() = 0;

	virtual bool BWasSteamControllerConnectedThisSession() = 0;
	virtual bool BWasSteamControllerUsedThisSession() = 0;

	// Access overlay window interface for this window, NULL on non Steam Overlay windows
	virtual IUIOverlayWindow *GetOverlayInterface() = 0;

	// Change visibility of this window
	virtual bool BIsVisible() = 0;
	virtual void SetVisible( bool bVisible ) = 0;

	// Change the focus behavior for controls in this window
	virtual EWindowFocusBehavior GetFocusBehavior() = 0;
	virtual void SetFocusBehavior( EWindowFocusBehavior eFocusBehavior ) = 0;

	virtual void OnDeviceLost() = 0;
	virtual void OnDeviceRestored() = 0;
	virtual bool BDeviceLost() = 0;

	// Clears the GPU resources associated with the window before the next render frame
	virtual void ClearGPUResourcesBeforeNextFrame() = 0;

	// Returns true if an attempt to navigate off the window was handled and some other window has focus now
	virtual bool BOnMoveEdge( panorama::EFocusMoveDirection moveType ) = 0;

	// Show whether this window is used for VR overlay
	virtual bool BIsVROverlay() = 0;
	virtual uint64_t GetVROverlayHandle() = 0;
	virtual bool BIsVROverlayFocused() = 0;
	virtual bool AddVROverlayHandleToProcess( uint64_t ulOverlayHandle ) = 0;
	virtual bool RemoveVROverlayHandleToProcess( uint64_t ulOverlayHandle ) = 0;


#ifdef PANORAMA_STEAMUI_STREAMING_CAPTURE_WIN32
	// For Steam Big Picture mode w/ In-Home Streaming: set a callback to be triggered before D3D present or device teardown
	virtual void SetSteamUIStreamingCaptureCallback( SteamUIStreamingCaptureCallback_t pCallback ) = 0;
#endif	

	// The window priority determines the order that overlapping windows are drawn and receive input.
	// Lower priority is drawn at the back and receives input last.  Higher priority is drawn at the
	// front and receives input first.
	virtual void SetWindowPriority( int nPriority ) = 0;
	virtual int GetWindowPriority() = 0;

	virtual void AsyncAddTextRegionToCache( const DrawTextRegionRenderCommand_t &renderCommand ) = 0;

	virtual void TextEntryFocusChange( IUIPanel *pPanel ) = 0;
	virtual void TextEntryInvalid( IUIPanel *pPanel ) = 0;

};


//
// Overlay window interface for Steam overlay specific windows
//
class IUIOverlayWindow
{
public:
#if !defined( SOURCE2_PANORAMA )
	virtual void PushOverlayRenderCmdStream( CSharedMemStream *pRenderStream, unsigned long dwPID, float flOpacity, EOverlayWindowAlignment alignment ) = 0;

	virtual void SetFocus( bool bFocus ) = 0;

	virtual bool SetGameProcessInfo( AppId_t nAppId, bool bCanSharedSurfaces, int32 eTextureFormat ) = 0;

	virtual void SetInputEnabled( bool bEnabled ) = 0;

	virtual void SetGameWindowSize( uint32 nWidth, uint32 nHeight ) = 0;

	virtual void SetFixedSurfaceSize( uint32 unSurfaceWidth, uint32 unSurfaceHeight ) = 0;

	virtual void OnMouseEnter() = 0;

	virtual void SetLetterboxColor( Color c ) = 0;
#endif
};
}
#endif // IUIWINDOW_H
