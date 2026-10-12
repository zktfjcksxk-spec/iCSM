//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================//

#pragma once

#include "panorama/csgo_panorama.h"
#include "gameui_interface.h"
#include "cs_shareddefs.h"
#include "cstrike15_item_inventory.h"
#include "csgo_inventory_item_list.h"
#include "cstrikeloadout.h"
#include "csgo_radial_selector.h"
#include "uicomponents/uicomponent_loadout.h"
#include "csgo_item_image_panel.h"

DECLARE_PANORAMA_EVENT2( ShowLoadout, const char*, int );

class CCSGO_Loadout : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_Loadout, panorama::CPanel2D );

public:
	CCSGO_Loadout( panorama::CPanel2D *pParent, const char *pchID );
	bool EventOnLoadoutChanged( int nSlot, const char* szSlotName, itemid_t oldItem, itemid_t newItem );
	bool EventShowLoadout( const char* szLoadoutSlot, int nTeam );
	bool EventFilterForPosition( int nTeam, int nPosition );
	void UpdatePanelsForLoadout( int nTeam, loadout_positions_t nStart, loadout_positions_t nEnd );

private:
	panorama::CPanelPtr< CCSGO_RadialSelector > m_pItemWheel;
	panorama::CPanelPtr< panorama::CImagePanel > m_pTeamLogo;
	panorama::CPanelPtr< CCSGO_InventoryItemList > m_pItemList;
};
