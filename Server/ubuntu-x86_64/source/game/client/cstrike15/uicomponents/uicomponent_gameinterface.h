//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
// Purpose: Helper for access to game related global functions 
//
// $NoKeywords: $
//=============================================================================//

#pragma once

#include "uicomponent_common.h"

class CUiComponent_GameInterface : public CUiComponentGlobalInstanceHelper< CUiComponent_GameInterface >
{
	UI_COMPONENT_DECLARE_GLOBAL_INSTANCE_ONLY( CUiComponent_GameInterface );

public:
#define UI_COMPONENT_FUNCTIONLIST_ELEMENT( returntype, fnname ) UI_COMPONENT_FUNCTION( returntype, fnname );
#include "uicomponent_gameinterface.functions.inc"
#undef UI_COMPONENT_FUNCTIONLIST_ELEMENT

	// UI Preferences access
	char const * GetSettingString( char const *szSettingKey );
	void SetSettingString( char const *szSettingKey, char const *szSettingValue );
};
