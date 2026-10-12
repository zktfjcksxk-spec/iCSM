//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UITOPLEVELWINDOWOPENVROVERLAY_H
#define UITOPLEVELWINDOWOPENVROVERLAY_H

#ifdef _WIN32
#pragma once
#endif

#include <openvr.h>
#include "controller/trackpad_kalman.h"
#include "controller/trackpad_weightedfilter.h"

namespace panorama
{
class CTopLevelWindowOverlay;


class CTopLevelWindowOpenVROverlay : public CTopLevelWindow
{
	typedef CTopLevelWindow BaseClass;
public:
	CTopLevelWindowOpenVROverlay( CUIEngine *pUIEngineParent );
	virtual ~CTopLevelWindowOpenVROverlay();

	// Initialize backing surface for window
	virtual bool BInitializeSurface( int nWidth, int nHeight, vr::VROverlayHandle_t ulOverlayHandle, bool bKeepInputFocusOnGamepadFocusLost, bool bIgnoreGamepadFocus );

	// Run any per window frame func logic
	virtual void RunPlatformFrame();

	// Resize the window to specified dimensions
	virtual void OnWindowResize( uint32 nWidth, uint32 nHeight );

	// Window position management
	virtual void SetWindowPosition( float x, float y );
	virtual void GetWindowPosition( float &x, float &y );
	virtual void GetWindowBounds( float &left, float &top, float &right, float &bottom );
	virtual void GetClientDimensions( float &width, float &height );
	virtual void Activate( bool bForceful );
	virtual void Minimize();
	virtual void SetTopMost( bool bTopMost ) { }
	virtual bool BHasFocus() { return m_bFocus; } 
	virtual bool BIsFullscreen() { return m_bFullScreen; }
	virtual void* GetNativeWindowHandle() { return 0; }
	virtual void ForceHideWindow() { }

	virtual bool BAllowInput( InputMessage_t &msg );
	virtual bool BIsVisible() OVERRIDE;
	virtual bool BIsVROverlay() OVERRIDE { return true; }
	virtual bool BIsVROverlayFocused() OVERRIDE;
	virtual uint64_t GetVROverlayHandle() { return m_ulOverlayHandle; }
	virtual bool AddVROverlayHandleToProcess( uint64_t ulOverlayHandle );
	virtual bool RemoveVROverlayHandleToProcess( uint64_t ulOverlayHandle );

	virtual void SetVisible( bool bVisible ) { AssertMsg( false, "SetVisible not implemented on CTopLevelWindowOverlay" ); }

	// Clear color for the window, normally black, transparent for overlay
	virtual Color GetClearColor() { return Color( 0, 0, 0, 0 ); }

	virtual bool BOnMoveEdge( panorama::EFocusMoveDirection moveType ) OVERRIDE;
		
	// Necessary for generating mouse enter & leave events on windows
	virtual bool IsMouseOver() OVERRIDE { return m_bMouseOverWindow; }
	virtual void SetMouseCursor( EMouseCursors eCursor ) OVERRIDE;
	virtual IImageSource *GetMouseCursorTexture( Vector2D *pptHotspot ) OVERRIDE;
	virtual void EnableControllerCursor( bool bEnable );

	void PushOverlayRenderCmdStream( CSharedMemStream *pRenderStream, unsigned long dwPID, float flOpacity, EOverlayWindowAlignment alignment );

	void SetGameWindowSize( uint32 nWidth, uint32 nHeight );
	void SetFixedSurfaceSize( uint32 unSurfaceWidth, uint32 unSurfaceHeight );

	void OnMouseMove( float x, float y );

	void SetFocus( bool bFocus ); 

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif
protected:
	virtual void Shutdown();

private:
	void ProcessVROverlayEvents( vr::VROverlayHandle_t ulOverlayHandle );
	void OnMouseEnter();
	void OnMouseLeave();

	IUI3DSurface *m_p3DSurface;
	bool m_bMouseOverWindow;

	bool m_bFocus;
	bool m_bCanShareSurfaces;

	bool m_bVisibleThisFrame;
	bool m_bVisibleLastFrame;

	uint32 m_unGameWidth;
	uint32 m_unGameHeight;

	bool m_bFullScreen;
	bool m_bKeepInputFocusOnGamepadFocusLost;
	bool m_bIgnoreGamepadFocus;

	struct VRTouchPadData_t
	{
		VRTouchPadData_t()
		{
			m_bLastFingerOnTouchpad = false;
			m_flFingerDownTime = 0.0f;
			m_TrackpadFilter.Init( vec2_origin );
			m_vecLastFingerPos.Init();
			m_vecFingerVel.Init();
			m_MomentumVelFilter.SetWeightType( WeightedMovingAverageFilter::k_EWeightDistribSimple );
			m_MomentumVelFilter.SetNumSamples( 6 );
			m_MomentumVelFilter.SetExcludeSamples( 2 ); // skip the last couple of samples for momentum, they're usually more noisy
		}
		
		KalmanFilter m_TrackpadFilter;
		WeightedMovingAverageFilter m_MomentumVelFilter;
		Vector2D m_vecLastFingerPos;
		Vector2D m_vecFingerVel;
		bool m_bLastFingerOnTouchpad;
		double m_flFingerDownTime;
	};

	VRTouchPadData_t m_TouchPadData[vr::k_unMaxTrackedDeviceCount];
	CUtlVector<int> m_vecMomentumPads;

	vr::VROverlayHandle_t m_ulOverlayHandle;
	CUtlVector< vr::VROverlayHandle_t > m_ulVecAdditionalOverlayHandles;
	bool m_bOculusHMD;

	uint64_t m_ulLastKeyboardHandle;
	uint32 m_unLastKeyboardEvent;
};



} // namespace panorama

#endif // UITOPLEVELWINDOWOPENVROVERLAY_H
