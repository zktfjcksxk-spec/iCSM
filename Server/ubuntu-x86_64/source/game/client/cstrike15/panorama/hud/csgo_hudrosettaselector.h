//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose:  Use mouse control to select among displayed options
//
//=====================================================================================//
#pragma once

#include "hud.h"
#include "hud_element_helper.h"

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?
#include "panorama/input/iuiinput.h"

class CCSGO_HudRosettaInventoryItem;
class CCSGO_HudRosettaSelector;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

class CRosettaInventoryItemPanel
{
public:
	CRosettaInventoryItemPanel();
	~CRosettaInventoryItemPanel();

	bool OnActivate();

	CCSGO_HudRosettaSelector *m_pRosettaSelector;

	panorama::CPanel2D *m_pSlot;

	// m_pSlot is parent of these:
	panorama::CLabel *m_pInvCount;
	panorama::CImagePanel *m_pSelection;
	panorama::CImagePanel *m_pImage;

	itemid_t m_itemID;
};

class CCSGO_HudRosettaSelector : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudRosettaSelector, panorama::CPanel2D );

public:

	explicit CCSGO_HudRosettaSelector( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudRosettaSelector();

	// These overload the CHudElement class
	virtual void	ProcessInput( void ) OVERRIDE;
	virtual void	LevelInit( void ) OVERRIDE;
	virtual void	LevelShutdown( void ) OVERRIDE;
	virtual bool 	ShouldDraw( void ) OVERRIDE;
	virtual void	SetActive( bool bActive ) OVERRIDE;
	

	virtual void	Reset( void ) OVERRIDE;

	bool Visible() { return m_bVisible; }
	void SetShowRosetta( bool bShow, const char* szType );

	void SetUpSpray();
	bool EventSetAutoToggle();
	bool InventoryPrev();
	bool InventoryNext();

	bool KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );
	void EnableCursor( bool bEnable );

private:

	void UpdateAutoToggle();
	void UpdateSpray( bool bAutoApply );
	void EnableDisablePageButtons();
	void HasCoolDown( bool bAutoApply );
	itemid_t GetEquippedItemID() const;
	bool InvItemAndEquippedItemAreTheSame( itemid_t iInventoryItemID );
	void LoadSprayIcon( panorama::CImagePanel *pImage, itemid_t iItemID );
	void MakeCursorHintString();
	void NoSprayEquipped();
	void PlayerHasNoSprays();
	void ResetPages();
	char *SeparateName( char *szSprayName );
	void SetItems();
	void ShowInventory( bool bShowAnySpray );
	void ShowPanel( bool bShow );
	bool EventOnMouseEnableBinding();

	uint64			m_hDenyInputToGame;
	bool			m_bVisible;
	bool			m_bEnableCursor;
	int				m_nTotalSprays;
	int				m_nItemTiles;
	int				m_nTilePos;
	int				m_nActiveIndex;
	int				m_nPage;
	const char *	m_XUID;

	panorama::CPanel2D *m_pBGGradient;

	panorama::CPanel2D *m_pCountdownBG;
	panorama::CPanel2D *m_pCountdownPie;
	panorama::CLabel *m_pCountdownTimer;
	panorama::CImagePanel *m_pSprayImage;
	panorama::CPanel2D *m_pSprayImageBG;
	panorama::CLabel *m_pSprayChargesRemainingLabel;
	panorama::CLabel *m_pSprayHintLabel;

	panorama::CPanel2D *m_pInventory;
	panorama::CPanel2D *m_pInventoryItems;

	panorama::CImagePanel *m_pInventoryPrev;
	panorama::CImagePanel *m_pInventoryNext;

	CUtlVector< CRosettaInventoryItemPanel * > m_aInventorySlots;

	panorama::CToggleButton *m_pQuickSprayToggle;
	panorama::CPanel2D *m_pSprayInfo;
	panorama::CLabel *m_pSprayInfoText;
};

extern bool Helper_CanUseSprays_Panorama();