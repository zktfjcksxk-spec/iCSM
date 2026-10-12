//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CSGO_HUDDEMOLITIONPROGRESSION_H_
#define CSGO_HUDDEMOLITIONPROGRESSION_H_

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?

struct CCSGO_DemolitionProgressionSymbols
{
	typedef panorama::CPanoramaSymbol Symbol;

	CCSGO_DemolitionProgressionSymbols() : m_bInitialized(false) {}
	void Initialize();
	
	bool m_bInitialized;

	// snippet ids - not symbol-ized in panorama

	// css classes
	Symbol gg_progress__active;
	Symbol gg_progress__fade;
	Symbol gg_weapon__active;
	Symbol gg_weapon__past;
};

//////////////////////////////////////////////////////////////////////////
// CCSGO_WeaponSelectionView
//
// A version of the weapon-selection HUD that works in the front-end
class CCSGO_DemolitionProgressionView : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_DemolitionProgressionView, panorama::CPanel2D );

public:
	CCSGO_DemolitionProgressionView( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_DemolitionProgressionView();

	// Note: *skipped* by CCSGO_HudWeaponSelection::SetupJavascriptObjectTemplate.
	// Only used for debugging in the front-end
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// JS functions for debugging
	void DebugInitDemolitionMode();
	bool DebugAddWeapon( const char* weaponName );
	void DebugSetDemolitionLevel( int level );
	void DebugDefaultCT();
	void DebugDefaultT();

	void BuildDebugPanel( panorama::CPanel2D* pPanel );

	// debug helpers
	const CEconItemDefinition* GetWeaponDef( const char* name );

protected:
	struct CWeaponPanelInfo
	{
		int m_nItemDefIndex;
		panorama::CPanel2D* m_pContainer;
	};

	// Instantly deletes all elements.  Used to 'reset' to a blank state.
	void DeleteDynamicElements();

	void InitForDemolition();
	int AddDemolitionWeapon( const CEconItemDefinition* pWeaponDef );
	bool SetLevel( int iLevel );
	void ShowAndHighlightCurrent();
	void FadeOut();

	CUtlVector< CWeaponPanelInfo > m_Weapons;
	panorama::CPanel2D* m_pWeaponsPanel;
	int m_nLevel;

public:
	FORCEINLINE const CCSGO_DemolitionProgressionSymbols& Symbols() { return s_symbols; }

private:
	static CCSGO_DemolitionProgressionSymbols s_symbols; // initialized in 1st constructor
};

//-----------------------------------------------------------------------------
// Purpose: CSGO Hud Weapon Selection Panel
//-----------------------------------------------------------------------------
class CCSGO_HudDemolitionProgression : public CCSGO_DemolitionProgressionView, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudDemolitionProgression, CCSGO_DemolitionProgressionView );

public:

	CCSGO_HudDemolitionProgression( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudDemolitionProgression();

	// CHudElement overrides
	virtual void LevelInit( void ) OVERRIDE;
	virtual void LevelShutdown( void ) OVERRIDE;
	virtual void SetActive( bool bActive ) OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;
	virtual void Think() OVERRIDE;

	// IGameEventListener2
	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

	// Special JS Setup that avoids the debug methods in CCSGO_DemolitionProgressionView
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

private:
	void SetupDemolitionWeapons();
	void UpdateDemolitionWeaponLevel();

private:
	bool m_bVisible;

	// cached data to check if weapon list changes
	int m_nTeam;
	int m_nWeapons;

	// handle game_event "round_start" being sent before data has been received by client
	bool m_bShowNextThink;
};

#endif	// CSGO_HUDDEMOLITIONPROGRESSION_H_