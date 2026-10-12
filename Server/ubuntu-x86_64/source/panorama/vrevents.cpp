//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "panorama/vrevents.h"

using namespace panorama;


// VR dashboard events
DEFINE_PANORAMA_EVENT( VRDashboardRequested );
DEFINE_PANORAMA_EVENT( VRDashboardThumbSelected );
DEFINE_PANORAMA_EVENT( VRResetDashboard );
DEFINE_PANORAMA_EVENT( VRRenderToast );
DEFINE_PANORAMA_EVENT( VRShowKeyboard );
DEFINE_PANORAMA_EVENT( VRHideKeyboard );
DEFINE_PANORAMA_EVENT( VROverlayShown );
DEFINE_PANORAMA_EVENT( VROverlayHidden );
DEFINE_PANORAMA_EVENT( VROverlayImageLoaded );
DEFINE_PANORAMA_EVENT( VRNotificationShown );
DEFINE_PANORAMA_EVENT( VRNotificationHidden );
DEFINE_PANORAMA_EVENT( VRNotificationBeginInteraction );
DEFINE_PANORAMA_EVENT( VRNotificationDestroyed );
DEFINE_PANORAMA_EVENT( VRStatusUpdate );
DEFINE_PANORAMA_EVENT( VRUnknownEvent );
DEFINE_PANORAMA_EVENT( VRApplicationMenuButtonDown );
DEFINE_PANORAMA_EVENT( VRApplicationMenuButtonUp );
DEFINE_PANORAMA_EVENT( VRApplicationTransitionStarted );
DEFINE_PANORAMA_EVENT( VRApplicationTransitionAborted );
DEFINE_PANORAMA_EVENT( VRApplicationTransitionNewAppStarted );
DEFINE_PANORAMA_EVENT( VRProcessQuit );
DEFINE_PANORAMA_EVENT( VRGamepadFocusGained );
DEFINE_PANORAMA_EVENT( VRGamepadFocusLost );
DEFINE_PANORAMA_EVENT( VRDashboardVisibilityChanged );
DEFINE_PANORAMA_EVENT( VRChaperoneChanged );
//