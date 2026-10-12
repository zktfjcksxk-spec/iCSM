//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef VREVENTS_H
#define VREVENTS_H

#ifdef _WIN32
#pragma once
#endif

#include "uievent.h"
#include "uieventcodes.h"
#include "layout/stylesymbol.h"
#include "localization/ilocalize.h"


namespace panorama
{
	// dashboard events
	DECLARE_PANORAMA_EVENT2( VRDashboardRequested, vr::TrackedDeviceIndex_t, vr::VROverlayHandle_t );
	DECLARE_PANORAMA_EVENT1( VRDashboardThumbSelected, vr::VROverlayHandle_t );
	DECLARE_PANORAMA_EVENT1( VROverlayShown, vr::VROverlayHandle_t );
	DECLARE_PANORAMA_EVENT1( VROverlayHidden, vr::VROverlayHandle_t );
	DECLARE_PANORAMA_EVENT3( VRNotificationShown, vr::VROverlayHandle_t, vr::VRNotificationId, uint64 );
	DECLARE_PANORAMA_EVENT3( VRNotificationHidden, vr::VROverlayHandle_t, vr::VRNotificationId, uint64 );
	DECLARE_PANORAMA_EVENT3( VRNotificationBeginInteraction, vr::VROverlayHandle_t, vr::VRNotificationId, uint64 );
	DECLARE_PANORAMA_EVENT3( VRNotificationDestroyed, vr::VROverlayHandle_t, vr::VRNotificationId, uint64 );
	DECLARE_PANORAMA_EVENT0( VRResetDashboard );
	DECLARE_PANORAMA_EVENT1( VRRenderToast, vr::VRNotificationId );
	DECLARE_PANORAMA_EVENT0( VRShowKeyboard );
	DECLARE_PANORAMA_EVENT0( VRHideKeyboard );
	DECLARE_PANORAMA_EVENT1( VROverlayImageLoaded, vr::VROverlayHandle_t );
	DECLARE_PANORAMA_EVENT1( VRStatusUpdate, vr::VRState_t );
	DECLARE_PANORAMA_EVENT1( VRUnknownEvent, vr::VREvent_t );
	DECLARE_PANORAMA_EVENT0( VRApplicationMenuButtonDown );
	DECLARE_PANORAMA_EVENT0( VRApplicationMenuButtonUp );
	DECLARE_PANORAMA_EVENT0( VRApplicationTransitionStarted );
	DECLARE_PANORAMA_EVENT0( VRApplicationTransitionAborted );
	DECLARE_PANORAMA_EVENT2( VRApplicationTransitionNewAppStarted, uint32, uint32 );
	DECLARE_PANORAMA_EVENT1( VRProcessQuit, uint32 );
	DECLARE_PANORAMA_EVENT1( VRGamepadFocusGained, vr::VROverlayHandle_t );
	DECLARE_PANORAMA_EVENT1( VRGamepadFocusLost, vr::VROverlayHandle_t );
	DECLARE_PANORAMA_EVENT1( VRDashboardVisibilityChanged, bool );
	DECLARE_PANORAMA_EVENT0( VRChaperoneChanged );

}

#endif // VREVENTS_H