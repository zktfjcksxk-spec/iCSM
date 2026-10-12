//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CSGO_HUDWEAPONSELECTION_H_
#define CSGO_HUDWEAPONSELECTION_H_

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?

class C_WeaponCSBase;

struct CCSGO_WeaponSelectionSymbols
{
	static const int kNumProgressionItemNextSymbols = 6;
	typedef panorama::CPanoramaSymbol Symbol;

	CCSGO_WeaponSelectionSymbols() : m_bInitialized(false) {}
	void Initialize();
	
	bool m_bInitialized;

	// snippet ids - not symbol-ized in panorama
	//Symbol ProgressionPip;
	//Symbol ProgressionIcon;
	//Symbol WeaponRow;
	//Symbol WeaponIcon;
	//Symbol WeaponIcon__Bomb;

	// css classes
	Symbol WEAPON_ICON; // used for all <Image> that need their src replaced with a particular weapon's icon

	Symbol weapon_selection__armsrace;
	Symbol weapon_selection__hidden;
	Symbol weapon_selection__fade;
	Symbol weapon_selection__animation_enable;

	Symbol weapon_selection__demolition;
	Symbol weapon_selection__demolition_reward;

	Symbol weapon_selection__bomb_zone;

	Symbol weapon_selection__parachuteicon_enable;

	Symbol weapon_row__active;
	Symbol weapon_row__deleted;
	Symbol weapon_row__selected;

	Symbol progression_pip__current;
	Symbol progression_pip__earned;
	
	Symbol weapon_selection_progression_item__next[kNumProgressionItemNextSymbols];
	Symbol weapon_selection_progression_item__earned;
	Symbol weapon_selection_progression_item__future;

	Symbol weapon_selection_item__active;
	Symbol weapon_selection_item__deleted;
	Symbol weapon_selection_item__selected;
	Symbol weapon_selection_item__has_multiple;
	Symbol weapon_selection_item__show_owner;
	Symbol weapon_selection_item__fade_ok;

	Symbol weapon_selection_demolition_item__active;

	// weapon_selection_item__rarity%d based on pItem->GetRarity

	// xml ids - currently not symbol-ized in panorama
	//Symbol icon_container;
	//Symbol weapon;
	//Symbol weaponname;
	//Symbol weaponglow;
	//Symbol weapon_selection_progression;
	//Symbol weapon_selection_xp;
	//Symbol weapon_selection_list;

	// dialog variables - currently not symbol-ized in panorama
	//Symbol HudWeaponSelection__progression_maxlevel;
	//Symbol ProgressionIcon__level;
	//Symbol WeaponIcon__name;
	//Symbol WeaponIcon__itemcount;
	//Symbol WeaponRow__binding;
	//Symbol WeaponRow__number;
	//Symbol WeaponRow__name;
};

//////////////////////////////////////////////////////////////////////////
// CCSGO_WeaponSelectionView
//
// A version of the weapon-selection HUD that works in the front-end
class CCSGO_WeaponSelectionView : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_WeaponSelectionView, panorama::CPanel2D );

public:
	CCSGO_WeaponSelectionView( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_WeaponSelectionView();

protected:
	struct CArmsRacePipInfo
	{
		panorama::CPanel2D* m_pPip;
	};

	struct CWeaponRowPanelInfo
	{
		gear_slot_t m_nGearSlot;
		panorama::CPanel2D* m_pRow;

		panorama::CPanel2D* m_pContainer;
		panorama::CPanel2D* m_pItemName; // probably a Label
	};

	struct CWeaponPanelInfo
	{
		gear_slot_t m_nGearSlot;
		gear_slotposition_t m_nGearSlotPosition;
		panorama::CPanel2D* m_pItem;
		CUtlVector<panorama::CImagePanel*> m_Icons;
		CHandle<C_BaseCombatWeapon> m_hWeapon;	// used to detect changes in the in-game version of this structure
		int m_nItemDefIndex;
		int m_nItemCount;		// For items like flashbangs where we might have more than 1
	};

	struct CArmsRaceWeapon
	{
		int m_nXpRequired;
		item_definition_index_t m_nDefIndex;

		panorama::CPanel2D* m_pItem;
	};

	struct CDemolitionWeapon
	{
		item_definition_index_t m_nDefIndex;
		panorama::CPanel2D* m_pItem;
	};

	// Instantly deletes all elements.  Used to 'reset' to a blank state.
	void DeleteDynamicElements();

	int AddAnimationDisable();
	int RemoveAnimationDisable();

	int AddWeaponPanel( CHandle<C_BaseCombatWeapon> hWeapon, const CEconItemView* pWeaponItem, uint64 xuidShownOwner );
	bool SetSelectedPanel( int iPanel, const CEconItemView* pWeaponItem ); // returns true if selected panel changed
	void SetWeaponPanelItemCount( int iPanel, int iCount );
	void SetPanelFadeEnabled( int iPanel, bool bFadeEnable );

	int DeleteWeaponPanel( int iPanel );
	void CleanupDeletedRows(); // Call after DeleteWeaponPanel() to make sure empty rows are removed

	int FindWeaponRow( int iPanel );

	void InitArmsRace( int nMaxLevel, int nMaxPips );
	void AddArmsRaceWeapon( int iXpRequired, const CEconItemDefinition* pItemDef );
	void SetArmsRaceLevel( int iLevel, int iXp );

	void InitDemolition();
	int AddDemolitionWeapon( const CEconItemDefinition* pItemDef );
	int RemoveDemolitionWeaponPanel( int iPanelIndex );
	void ShowDemolitionGrenadeReward();

	void ShowParachuteIcon( bool bShow );

	void Fade( bool bFade );
	bool IsFaded();

	virtual void OnLayoutReloaded() OVERRIDE;

	panorama::CPanel2D* m_pProgressionWeapons;
	panorama::CPanel2D* m_pProgressionXp;
	panorama::CPanel2D* m_pDemolitionWeapons;
	panorama::CPanel2D* m_pWeaponList;

	CUtlVector< CArmsRacePipInfo > m_ArmsRacePips;
	CUtlVector< CArmsRaceWeapon > m_ArmsRaceWeapons;

	CUtlVector< CDemolitionWeapon > m_DemolitionWeapons;

	CUtlVector< CWeaponRowPanelInfo> m_WeaponRows;
	CUtlVector< CWeaponPanelInfo > m_Weapons;

	int m_nSelectedWeapon;
	int m_nSelectedRow;

	int m_nArmsRacePips;
	int m_nArmsRaceLevel;

	int m_nAnimationDisabledCount;
	bool m_bFaded;

public:
	FORCEINLINE const CCSGO_WeaponSelectionSymbols& Symbols() { return s_symbols; }

private:
	static CCSGO_WeaponSelectionSymbols s_symbols; // initialized in 1st constructor
};

//-----------------------------------------------------------------------------
// Purpose: CSGO Hud Weapon Selection Panel (debug mode, used in front-end)
//-----------------------------------------------------------------------------
class CCSGO_HudWeaponSelectionDebug : public CCSGO_WeaponSelectionView
{
	DECLARE_PANEL2D( CCSGO_HudWeaponSelectionDebug, CCSGO_WeaponSelectionView );

public:
	CCSGO_HudWeaponSelectionDebug( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudWeaponSelectionDebug();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// JS functions for debugging
	bool DebugAddWeapon( const char* weaponName, int level, int count, bool bOwned );
	bool DebugRemoveWeapon( const char* weaponName );
	bool DebugSelectWeapon( const char* weaponName );
	bool DebugSetFadeEnabled( const char* weaponName, bool bEnabled );
	bool DebugEnableAnimation( bool bEnabled );
	void DebugReset();
	void DebugToggleFade();

	void DebugInitArmsRace( int maxLevel, int maxPips ); // default 16*2+1
	void DebugAddArmsRaceWeapon( const char* weaponName, int xpRequired ); // default xp required=2 except knifegg=1
	void DebugSetArmsRaceXp( int iLevel, int iXp );

	void DebugInitDemolition();
	void DebugAddDemolitionWeapon( const char* weaponName );
	void DebugRemoveDemolitionWeapon( const char* weaponName );

	void DebugShowParachute( bool bShow );

	void BuildDebugPanel( panorama::CPanel2D* pPanel );

	// debug helpers
	const CEconItemDefinition* GetWeaponDef( const char* name );
	bool GetWeaponPanel( int* outPanelIdx, const char* weaponName );

private:
	bool m_bAnimationEnabledCheckbox;
};

//-----------------------------------------------------------------------------
// Purpose: CSGO Hud Weapon Selection Panel
//-----------------------------------------------------------------------------
class CCSGO_HudWeaponSelection : public CCSGO_WeaponSelectionView, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudWeaponSelection, CCSGO_WeaponSelectionView );

public:

	CCSGO_HudWeaponSelection( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudWeaponSelection();

	// CHudElement overrides
	virtual void LevelInit( void ) OVERRIDE;
	virtual void LevelShutdown( void ) OVERRIDE;
	virtual void SetActive( bool bActive ) OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;
	virtual void Think() OVERRIDE;

	// IGameEventListener2
	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

	virtual void OnLayoutReloaded() OVERRIDE;

private:
	void ShowPanel( bool bShow );

	// Force an update on next Think()
	void ResetUpdateTime() { m_fNextUpdateTime = -1.0f; }

	// Sets our update time if it's sooner than our next forced update
	void SetUpdateTime( float fUpdateTime )
	{
		if ( fUpdateTime < m_fNextUpdateTime )
			m_fNextUpdateTime = fUpdateTime;
	}

	void SetupArmsRace();
	void SetupDemolition();

	void UpdateWeapons( C_CSPlayer* pPlayer, bool bPlayerChanged );
	void UpdateArmsRaceXp( C_CSPlayer* pPlayer, bool bPlayerChanged );
	void UpdateArmsRaceWeapons( C_CSPlayer* pPlayer, bool bPlayerChanged );
	void UpdateDemolitionWeapons( C_CSPlayer* pPlayer, bool bPlayerChanged );

	bool ShouldNeverFade();

	uint64 GetOwnerXuid( C_CSPlayer* pHudPlayer, C_WeaponCSBase* pWeapon );

private:
	bool m_bVisible;
	float m_fNextUpdateTime;
	float m_fFadeTime;
	bool m_bHudBombInWeaponSelection;

	bool m_bAnimationDisabledDueToRoundStart;

	// cached data for checking for changes
	CHandle<C_CSPlayer> m_hPlayer;
	int m_nArmsRaceWeapons;
	int m_nDemolitionKillPoints;
	int m_nDemolitionCurWeapon;
	int m_nDemolitionTeam;
};

#endif	// CSGO_HUDWEAPONSELECTION_H_