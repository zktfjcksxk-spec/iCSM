//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
// Buymenu panel mostly cloning the old scaleform one.
//
//=============================================================================//

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/csgo_panorama_script_bindings.h"
#include "gameui_interface.h"
#include "cs_shareddefs.h"
#include "cstrike15_item_inventory.h"
#include "cstrikeloadout.h"
#include "csgo_radial_selector.h"
#include "csgo_timers.h"
//#include "uicomponents/uicomponent_loadout.h"
#include "csgo_item_image_panel.h"
#include "panorama/ui_itempreview_panel.h"
#include "panorama/hud/csgo_hudmoney.h"
#include "panorama/uiinputcapture.h"

DECLARE_PANORAMA_EVENT0( EventOpenBuyMenu );
DECLARE_PANORAMA_EVENT0( EventCloseBuyMenu );
DECLARE_PANORAMA_EVENT1( LocalPlayerMoneyChanged, int );
DECLARE_PANORAMA_EVENT1( LocalPlayerBuyZoneChange, bool );
DECLARE_PANORAMA_EVENT1( PlayerEquipmentChange, C_CSPlayer* );

class CCSGO_BuyMenu : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_BuyMenu, panorama::CPanel2D );

public:
	CCSGO_BuyMenu( panorama::CPanel2D *pParent, const char *pchID );
	~CCSGO_BuyMenu();
	enum BuyWheelCategory
	{
		k_ePistol = 0,
		k_eHeavy, 
		k_eSMG,
		k_eRifle,
		k_eGear,
		k_eGrenade,
		k_eCategoryCount
	};

	const char* m_szKeyBindings[ k_eCategoryCount ] = { "1", "2", "3", "4", "5", "6" };

	bool CloseBuyMenu( void );
	bool OpenBuyMenu( void );

	virtual bool OnMouseButtonUp( const panorama::MouseData_t &code ) OVERRIDE;
	virtual bool OnKeyDown( const panorama::KeyData_t & code ) OVERRIDE;
	virtual void OnLayoutReloading() OVERRIDE;
	virtual void OnLayoutReloaded() OVERRIDE;

private:

	struct ItemPanelsForUpdate_t
	{
		ItemPanelsForUpdate_t() 
		{ 
			m_pItem = nullptr; 
			m_nSlot = LOADOUT_POSITION_INVALID;
		}
		panorama::CPanelPtr< panorama::CImagePanel > m_pItemIcon;
		panorama::CPanelPtr< panorama::CLabel > m_pItemName;
		panorama::CPanelPtr< panorama::CLabel > m_pItemPrice;
		panorama::CPanelPtr< panorama::CButton > m_pItemButton;
		loadout_positions_t m_nSlot;
		const CEconItemView* m_pItem; // Used to detect loadout changes, not intended to be dereferenced in this class 
	};
	struct BuyWheelInfo_t 
	{
		const char* szName;
		const char* szDisplayName;
		loadout_positions_t nLoadoutFirst;
		loadout_positions_t nLoadoutLast;
		panorama::CPanelPtr< CCSGO_RadialSelector > m_pBuyWheel;
		CUtlVector< ItemPanelsForUpdate_t > m_vecItems;
		//CUtlVector< panorama::CPanelPtr< panorama::CImagePanel > > m_vecCategoryIcons;
	};

	void CreateBuyWheels( void );
	void CreateCategoryPanels( panorama::CPanel2D *pParent, panorama::CButton* pButton, BuyWheelCategory nCategory, loadout_positions_t nLoadoutFirst, loadout_positions_t nLoadoutLast );
	void CreateBuyWheelPanels( CPanel2D * pParent, BuyWheelInfo_t & wheelInfo );
	void HookEvents();
	void UnhookEvents();
	void RefreshItems( void );
	void CreateTeamEquipmentPanels( void );
	bool AreWeaponsFree() const;
	bool EventSelectCategory( BuyWheelCategory nCategory );
	bool EventPurchaseSlot( loadout_positions_t nSlot );
	bool EventPurchaseFailSlot(loadout_positions_t nSlot);
	bool EventNavigateBack( void );
	bool EventTeamChange( uint64 steamID, int32 nOldTeam, int32 nNewTeam );
	bool EventLocalPlayerMoneyChanged( int iCurrentMoney );
	bool EventLocalPlayerBuyZoneChange( bool bInBuyZone );
	bool EventServerSpawn();
	bool EventWindowGotFocus( panorama::IUIWindow * pTopLevelWindow );
	bool EventActivateWedge( int nWedgeNum );
	bool EventHoverPanelChange( const panorama::CPanelPtr< panorama::IUIPanel > & pPanel, panorama::CPanelPtr< panorama::CPanel2D > pNewHover );
	bool EventPlayerEquipmentChange( C_CSPlayer *pPlayer );
	bool EventFrameUpdate();

	bool OnPurchaseButtonHover( loadout_positions_t pos );

	void UpdatePurchaseFailureLabel();

	// Buy wheels. Category wedges activate one of the subwedges. Only one of the seven 
	// are visible at a time, pointed to by the visible buy wheel member
	BuyWheelInfo_t m_buyWheels[ k_eCategoryCount ];
	panorama::CPanelPtr< CCSGO_RadialSelector > m_pCategoryWheel;
	panorama::CPanelPtr< CCSGO_RadialSelector > m_pVisibleBuyWheel;
	panorama::CPanelPtr< panorama::CPanel2D > m_pHoveredWedge;
	panorama::CPanelPtr< panorama::CImagePanel > m_pTeamLogo;
	panorama::CPanelPtr< panorama::CLabel > m_pPurchaseFailureLabel;

	// Panels with player equipment previews
	panorama::CPanelPtr< panorama::CPanel2D > m_pTeamEquipment;
	panorama::CPanelPtr< panorama::CPanel2D > m_pLocalPlayerEquipment;

	panorama::CPanelPtr< CCSGO_CountdownTimer > m_pCountdownTimer;
	panorama::CPanelPtr< CCSGOMoneyPanel > m_pMoneyPanel;

	// Info about the item wedge currently being hovered. Shows icons for the category if 
	// hovering a category wedge, or weapon details if hovering over an item purchase wedge. 
	panorama::CPanelPtr< panorama::CPanel2D > m_pItemInfo;
	panorama::CPanelPtr< CUI_ItemPreviewPanel > m_pItemPreview; 

	panorama::CGameInputCapture m_Capture;
};
