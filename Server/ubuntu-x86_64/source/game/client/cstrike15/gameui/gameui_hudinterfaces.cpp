

#include <cbase.h>
#include "gameui_hudinterfaces.h"
#include "gameui_interface.h"
#include "hud_element_helper.h"

#ifdef PANORAMA_ENABLE
#include "panorama/hud/csgo_hudchat.h"
#endif

#ifdef INCLUDE_SCALEFORM
#include "Scaleform/HUD/sfhud_chat.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IHudChat* GetHudChat()
{
#ifdef PANORAMA_ENABLE
	if ( GameUI().IsPanoramaEnabled() )
	{
		return GET_HUDELEMENT( CCSGO_HudChat );
	}
#endif

#ifdef INCLUDE_SCALEFORM
	if ( !GameUI().IsPanoramaEnabled() )
	{
		return GET_HUDELEMENT( SFHudChat );
	}
#endif

	return nullptr;
}

