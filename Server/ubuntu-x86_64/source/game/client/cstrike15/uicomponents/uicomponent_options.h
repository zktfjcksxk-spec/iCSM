//========= Copyright (C) 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: Helper for options access in Scaleform and Panorama.
//
// $NoKeywords: $
//=============================================================================//

#ifndef UICOMPONENT_OPTIONS_H
#define UICOMPONENT_OPTIONS_H
#ifdef _WIN32
#pragma once
#endif

#include "uicomponent_common.h"

//
// Component
//
class CUiComponent_OptionsMenu : public CUiComponentGlobalInstanceHelper< CUiComponent_OptionsMenu >
{
	UI_COMPONENT_DECLARE_GLOBAL_INSTANCE_ONLY( CUiComponent_OptionsMenu );

public:

	void RestoreKeybdMouseBindingDefaults();
	bool ShowSteamControllerBindingsPanel();

private:
};


#endif // UICOMPONENT_OPTIONS_H
