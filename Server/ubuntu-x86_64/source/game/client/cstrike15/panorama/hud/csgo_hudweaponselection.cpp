//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
// SF Differences:
//
//=============================================================================//

#include "cbase.h"
#include "csgo_hudweaponselection.h"
#include "csgo_hudspectator.h"
#include "csgo_hud.h"

#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "cs_gamerules.h"
#include "weapon_cache.h"

//#include "uicomponents/uicomponent_friendslist.h"

#include "clientsteamcontext.h"

#include "panorama/csgo_panel_debug.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


REGISTER_PANEL2D( CCSGO_WeaponSelectionView, CSGOWeaponSelectionViewAbstract );

REGISTER_PANEL2D_FACTORY( CCSGO_HudWeaponSelectionDebug, CSGOWeaponSelectionView );
REGISTER_PANEL2D_FACTORY( CCSGO_HudWeaponSelection, CSGOHudWeaponSelection );

extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_drawhud;
extern ConVar cl_showloadout;
extern ConVar cl_hud_bomb_under_radar;
extern ConVar cl_hud_color;

// Animation times -- must match weapon selection CSS
static const float kWeaponSelectAnimationTime_Drop = 0.2f;
static const float kWeaponSelectAnimationTime_DeleteRow = 0.2f;

// How long to remain visible if no changes occuring
static const float kWeaponSelectTimeUntilFade = 5.0f;

// tinting via cl_hud_color (values taken from SF/.as for now)
static const Color kDefaultIconColor = Color( 0x95, 0x95, 0x77, 0xff );
static const Color kSelectedIconColor = Color( 0xff, 0xff, 0xff, 0xff );
static const float kIconColorBrightness = 0.5f; 
static const float kIconColorSaturation = 0.75f;

//////////////////////////////////////////////////////////////////////////
// CCSGO_WeaponSelectionSymbols: singleton containing panorama symbols
// used by weapon selection panel
void CCSGO_WeaponSelectionSymbols::Initialize()
{
	if ( m_bInitialized )
		return;

	m_bInitialized = true;

	WEAPON_ICON = "WEAPON-ICON";

	weapon_selection__armsrace = "weapon-selection--armsrace";
	weapon_selection__hidden = "weapon-selection--hidden";
	weapon_selection__fade = "weapon-selection--fade";
	weapon_selection__animation_enable = "weapon-selection--animation-enable";
	weapon_selection__demolition = "weapon-selection--demolition";
	weapon_selection__demolition_reward = "weapon-selection--demolition-reward";
	weapon_selection__bomb_zone = "weapon-selection--bomb-zone";
	weapon_selection__parachuteicon_enable = "weapon-selection--parachuteicon-enable";
	weapon_row__active = "weapon-row--active";
	weapon_row__deleted = "weapon-row--deleted";
	weapon_row__selected = "weapon-row--selected";
	progression_pip__current = "progression-pip--current";
	progression_pip__earned = "progression-pip--earned";

	for ( int i = 0; i < kNumProgressionItemNextSymbols; ++i )
	{
		weapon_selection_progression_item__next[i] = CFmtStr( "weapon-selection-progression-item--next%d", i ).Get();
	}
	weapon_selection_progression_item__earned = "weapon-selection-progression-item--earned";
	weapon_selection_progression_item__future = "weapon-selection-progression-item--future";

	weapon_selection_item__active = "weapon-selection-item--active";
	weapon_selection_item__deleted = "weapon-selection-item--deleted";
	weapon_selection_item__selected = "weapon-selection-item--selected";
	weapon_selection_item__has_multiple = "weapon-selection-item--has-multiple";
	weapon_selection_item__show_owner = "weapon-selection-item--show-owner";
	weapon_selection_item__fade_ok = "weapon-selection-item--fade-ok";

	weapon_selection_demolition_item__active = "weapon-selection-demolition-item--active";
}


//-----------------------------------------------------------------------------
// Purpose: Debug viewer for weapons
//-----------------------------------------------------------------------------
CCSGO_HudWeaponSelectionDebug::CCSGO_HudWeaponSelectionDebug( panorama::CPanel2D *pParent, const char *pchID )
	: CCSGO_WeaponSelectionView( pParent, pchID )
{
}

CCSGO_HudWeaponSelectionDebug::~CCSGO_HudWeaponSelectionDebug()
{
}

// Debug-only
void CCSGO_HudWeaponSelectionDebug::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "Debug_Reset", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DeleteDynamicElements ) );
	RegisterJSMethod( "Debug_ToggleFade", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugToggleFade ) );
	RegisterJSMethod( "Debug_EnableAnimation", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugEnableAnimation ) );
	RegisterJSMethod( "Debug_Add", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugAddWeapon ) );
	RegisterJSMethod( "Debug_Remove", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugRemoveWeapon ) );
	RegisterJSMethod( "Debug_Select", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugSelectWeapon ) );
	RegisterJSMethod( "Debug_SetFadeEnabled", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugSetFadeEnabled ) );
	RegisterJSMethod( "Debug_ArmsRaceMode", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugInitArmsRace ) );
	RegisterJSMethod( "Debug_ARWeapon", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugAddArmsRaceWeapon ) );
	RegisterJSMethod( "Debug_SetARLevel", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugSetArmsRaceXp ) );
	RegisterJSMethod( "Debug_InitTR", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugInitDemolition ) );
	RegisterJSMethod( "Debug_AddTRWeapon", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugAddDemolitionWeapon ) );
	RegisterJSMethod( "Debug_RemoveTRWeapon", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugRemoveDemolitionWeapon ) );
	RegisterJSMethod( "BuildDebugPanel", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::BuildDebugPanel ) );
	RegisterJSMethod( "Debug_ShowParachute", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugShowParachute ) );
}

void CCSGO_HudWeaponSelectionDebug::BuildDebugPanel( panorama::CPanel2D* pPanel )
{
	CPanelDebugHelper debugHelper;
	debugHelper.Init();
	debugHelper.AddDebugMethod( "Debug_Reset", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DeleteDynamicElements ) );
	debugHelper.AddDebugMethod( "Debug_ToggleFade", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugToggleFade ) );
	debugHelper.AddCheckbox( "Debug_EnableAnimation", true, PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugEnableAnimation ) );
	debugHelper.AddDebugMethod( "Debug_Add", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugAddWeapon ) );
	debugHelper.AddDebugMethod( "Debug_Remove", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugRemoveWeapon ) );
	debugHelper.AddDebugMethod( "Debug_Select", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugSelectWeapon ) );
	debugHelper.AddDebugMethod( "Debug_SetFadeEnabled", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugSetFadeEnabled ) );
	debugHelper.AddDebugMethod( "Debug_ArmsRaceMode", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugInitArmsRace ) );
	debugHelper.AddDebugMethod( "Debug_ARWeapon", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugAddArmsRaceWeapon ) );
	debugHelper.AddDebugMethod( "Debug_SetARLevel", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugSetArmsRaceXp ) );
	debugHelper.AddDebugMethod( "Debug_InitTR", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugInitDemolition ) );
	debugHelper.AddDebugMethod( "Debug_AddTRWeapon", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugAddDemolitionWeapon ) );
	debugHelper.AddDebugMethod( "Debug_RemoveTRWeapon", PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugRemoveDemolitionWeapon ) );
	debugHelper.AddCheckbox( "Debug_ShowParachute", false, PANORAMA_DELEGATE( &CCSGO_HudWeaponSelectionDebug::DebugShowParachute ) );
	debugHelper.Finish();

	pPanel->BCreateChildren( debugHelper.mXML );
}

// Debug code to find a weapon
const CEconItemDefinition* CCSGO_HudWeaponSelectionDebug::GetWeaponDef( const char* weaponName )
{
	if ( const CEconItemDefinition* pItemDef = GetItemSchema()->GetItemDefinitionByName( weaponName ) )
		return pItemDef;

	if ( const CEconItemDefinition* pItemDef = GetItemSchema()->GetItemDefinitionByName( CFmtStr( "weapon_%s", weaponName ) ) )
		return pItemDef;

	return nullptr;
}

bool CCSGO_HudWeaponSelectionDebug::GetWeaponPanel( int* outPanelIdx, const char* weaponName )
{
	const CEconItemDefinition* pItemDef = GetWeaponDef( weaponName );
	if ( !pItemDef )
		return false;

	int nDefIndex = pItemDef->GetDefinitionIndex();
	FOR_EACH_VEC( m_Weapons, i )
	{
		if ( m_Weapons[i].m_nItemDefIndex == nDefIndex )
		{
			*outPanelIdx = i;
			return true;
		}
	}

	return false;
}

// Debug code to enable/disable animation
bool CCSGO_HudWeaponSelectionDebug::DebugEnableAnimation( bool bEnable )
{
	if ( m_bAnimationEnabledCheckbox != bEnable )
	{
		m_bAnimationEnabledCheckbox = bEnable;

		if ( m_bAnimationEnabledCheckbox )
		{
			int count = RemoveAnimationDisable();
			Assert( count == 0 );
		}
		else
		{
			int count = AddAnimationDisable();
			Assert( count == 1 );
		}
	}

	return false;
}

// Debug code to fade/unfade
void CCSGO_HudWeaponSelectionDebug::DebugToggleFade()
{
	Fade( !IsFaded() );
}

bool CCSGO_HudWeaponSelectionDebug::DebugSetFadeEnabled( const char* weaponName, bool bEnabled )
{
	int iPanel;
	if ( !GetWeaponPanel( &iPanel, weaponName ) )
		return false;

	SetPanelFadeEnabled( iPanel, bEnabled );
	return true;
}

// Debug code to add a new weapon
bool CCSGO_HudWeaponSelectionDebug::DebugAddWeapon( const char* weaponName, int level, int count, bool bOwned )
{
	const CEconItemDefinition* pItemDef = GetWeaponDef( weaponName );
	if ( !pItemDef )
		return false;

	CEconItemView itemView;
	itemView.Init( pItemDef->GetDefinitionIndex(), AE_UNIQUE, AE_USE_SCRIPT_VALUE, true );
	if(level >= 0)
	{
		itemView.SetItemRarityOverride( level );
	}

	uint64 xuidOwner = 0;
	if ( bOwned )
	{
		xuidOwner = ClientSteamContext().GetLocalPlayerSteamID().ConvertToUint64();
	}

	int iPanelIndex = AddWeaponPanel( nullptr, &itemView, xuidOwner );
	if ( count >= 0 && iPanelIndex >= 0 )
	{
		SetWeaponPanelItemCount( iPanelIndex, count );
	}

	return iPanelIndex >= 0;
}

// Debug code to remove a weapon
bool CCSGO_HudWeaponSelectionDebug::DebugRemoveWeapon( const char* weaponName )
{
	int iPanel;
	if ( !GetWeaponPanel( &iPanel, weaponName ) )
		return false;

	DeleteWeaponPanel( iPanel );
	CleanupDeletedRows();
	return true;
}

// Debug code to select a weapon
bool CCSGO_HudWeaponSelectionDebug::DebugSelectWeapon( const char* weaponName )
{
	int iPanel;
	if ( !GetWeaponPanel( &iPanel, weaponName ) )
		return false;

	int iWeaponDefIdx = m_Weapons[iPanel].m_nItemDefIndex;
	CEconItemView itemView;
	itemView.Init( iWeaponDefIdx, AE_UNIQUE, AE_USE_SCRIPT_VALUE, true );
	SetSelectedPanel( iPanel, &itemView );
	return true;
}

// Debug code to set up arms race mode
void CCSGO_HudWeaponSelectionDebug::DebugInitArmsRace( int nMaxLevel, int nMaxPips )
{
	if ( nMaxLevel < 0 )
		nMaxLevel = 16;

	if ( nMaxPips < 0 )
		nMaxPips = 31;

	InitArmsRace( nMaxLevel, nMaxPips );
}

void CCSGO_HudWeaponSelectionDebug::DebugAddArmsRaceWeapon( const char* weaponName, int xpRequired )
{
	const CEconItemDefinition* pWeaponDef = GetWeaponDef( weaponName );
	if ( !pWeaponDef )
		return;

	static const CSchemaItemDefHandle knifegg( "weapon_knifegg" );
	if ( xpRequired < 0 )
	{
		xpRequired = 2;
		if ( pWeaponDef == knifegg )
			xpRequired = 1;
	}

	AddArmsRaceWeapon( xpRequired, pWeaponDef );
}

void CCSGO_HudWeaponSelectionDebug::DebugSetArmsRaceXp( int iLevel, int iXp )
{
	if ( iLevel < 0 )
		iLevel = 0;
	if ( iXp < 0 )
		iXp = 0;

	SetArmsRaceLevel( iLevel, iXp );
}

void CCSGO_HudWeaponSelectionDebug::DebugInitDemolition()
{
	InitDemolition();
}

void CCSGO_HudWeaponSelectionDebug::DebugAddDemolitionWeapon( const char* weaponName )
{
	const CEconItemDefinition* pItemDef = GetWeaponDef( weaponName );
	if ( !pItemDef )
		return;

	// make sure we don't add more than 2 weapons (simulate 'grenade upgrade')
	if ( m_DemolitionWeapons.Count() >= 2 )
	{
		RemoveDemolitionWeaponPanel( m_DemolitionWeapons.Count() - 1 );
	}

	int iPanel = AddDemolitionWeapon( pItemDef );
	if ( iPanel > 0 )
	{
		// it's a 2nd item, notify
		ShowDemolitionGrenadeReward();
	}
}

void CCSGO_HudWeaponSelectionDebug::DebugRemoveDemolitionWeapon( const char* weaponName )
{
	const CEconItemDefinition* pItemDef = GetWeaponDef( weaponName );
	if ( !pItemDef )
		return;

	int nDefIndex = pItemDef->GetDefinitionIndex();

	FOR_EACH_VEC( m_DemolitionWeapons, i )
	{
		if ( m_DemolitionWeapons[i].m_nDefIndex == nDefIndex )
		{
			i = RemoveDemolitionWeaponPanel( i );
			break;
		}
	}

	// clear grenade anim whenever we remove an item
	SetHasClass( Symbols().weapon_selection__demolition_reward, false );
}

void CCSGO_HudWeaponSelectionDebug::DebugShowParachute( bool bShow )
{
	ShowParachuteIcon( bShow );
}

//////////////////////////////////////////////////////////////////////////
// CCSGO_WeaponSelectionView: view-only logic for weapon selection hud
CCSGO_WeaponSelectionSymbols CCSGO_WeaponSelectionView::s_symbols;

CCSGO_WeaponSelectionView::CCSGO_WeaponSelectionView( panorama::CPanel2D* pParent, const char* pchID )
	: panorama::CPanel2D( pParent, pchID )
	, m_nSelectedWeapon( -1 )
	, m_nSelectedRow( -1 )
	, m_nArmsRacePips( -1 )
	, m_nArmsRaceLevel( -1 )
	, m_nAnimationDisabledCount( 0 )
	, m_bFaded( false )
{
	s_symbols.Initialize();

	RequireLoadLayout( "file://{resources}/layout/hud/hudweaponselection.xml" );

	m_pProgressionWeapons = FindChildTraverse( "weapon-selection-progression" );
	m_pProgressionXp = FindChildTraverse( "weapon-selection-xp" );
	m_pWeaponList = FindChildTraverse( "weapon-selection-list" );
	m_pDemolitionWeapons = FindChildTraverse( "weapon-selection-demolition" );

	SetHasClass( Symbols().weapon_selection__animation_enable, ( m_nAnimationDisabledCount == 0 ) );

	SetDialogVariable( "HudWeaponSelection--progression-maxlevel", 0 );
}

CCSGO_WeaponSelectionView::~CCSGO_WeaponSelectionView()
{
}

void CCSGO_WeaponSelectionView::OnLayoutReloaded()
{
	BaseClass::OnLayoutReloaded();

	// All our raw pointers might be garbage, just kill everything
	DeleteDynamicElements();
}

// Destroy all dynamically-created panels and clean up to 'base' state.
void CCSGO_WeaponSelectionView::DeleteDynamicElements()
{
	FOR_EACH_VEC( m_ArmsRacePips, i )
	{
		CArmsRacePipInfo& pipData = m_ArmsRacePips[i];
		delete pipData.m_pPip;
	}
	m_ArmsRacePips.RemoveAll();

	FOR_EACH_VEC( m_ArmsRaceWeapons, i )
	{
		CArmsRaceWeapon& arWeapon = m_ArmsRaceWeapons[i];
		delete arWeapon.m_pItem;
	}
	m_ArmsRaceWeapons.RemoveAll();

	FOR_EACH_VEC( m_DemolitionWeapons, i )
	{
		CDemolitionWeapon& demoWeapon = m_DemolitionWeapons[i];
		delete demoWeapon.m_pItem;
	}
	m_DemolitionWeapons.RemoveAll();

	FOR_EACH_VEC( m_WeaponRows, i )
	{
		CWeaponRowPanelInfo& rowData = m_WeaponRows[i];
		delete rowData.m_pRow;
	}
	m_WeaponRows.RemoveAll();
	m_Weapons.RemoveAll();

	m_nSelectedWeapon = -1;
	m_nSelectedRow = -1;

	m_nArmsRaceLevel = -1;
	m_nArmsRacePips = -1;

	SetHasClass( Symbols().weapon_selection__armsrace, false );
	SetHasClass( Symbols().weapon_selection__demolition, false );
	SetHasClass( Symbols().weapon_selection__demolition_reward, false );
	SetHasClass( Symbols().weapon_selection__animation_enable, ( m_nAnimationDisabledCount == 0 ) );

	SetDialogVariable( "HudWeaponSelection--progression-maxlevel", 0 );
}

int CCSGO_WeaponSelectionView::AddAnimationDisable()
{
	if ( !m_nAnimationDisabledCount )
	{
		SetHasClass( Symbols().weapon_selection__animation_enable, false );
	}

	return ++m_nAnimationDisabledCount;
}

int CCSGO_WeaponSelectionView::RemoveAnimationDisable()
{
	int result = --m_nAnimationDisabledCount;
	Assert( result >= 0 ); // mismatched disable/enable pair

	if ( !result )
	{
		SetHasClass( Symbols().weapon_selection__animation_enable, true );

		// Skip all animations when re-enabling animations.
		AccessStyle()->SkipAnimations();

		IterateChildrenTraverse( []( CPanel2D* pPanel ) {
			pPanel->AccessStyle()->SkipAnimations();
			return true;
		} );
	}

	return result;
}

// Set up arms race view
void CCSGO_WeaponSelectionView::InitArmsRace( int nMaxLevel, int nMaxPips )
{
	SetHasClass( Symbols().weapon_selection__armsrace, true ); // TODO: Split into arms race layout?
	SetDialogVariable( "HudWeaponSelection--progression-maxlevel", nMaxLevel );

	// Clear dynamic elements for arms race
	FOR_EACH_VEC( m_ArmsRacePips, i )
	{
		CArmsRacePipInfo& pipData = m_ArmsRacePips[i];
		delete pipData.m_pPip;
	}
	m_ArmsRacePips.RemoveAll();

	FOR_EACH_VEC( m_ArmsRaceWeapons, i )
	{
		CArmsRaceWeapon& arWeapon = m_ArmsRaceWeapons[i];
		delete arWeapon.m_pItem;
	}
	m_ArmsRaceWeapons.RemoveAll();

	// Initialize pips
	if ( m_pProgressionXp )
	{
		for ( int i = 0; i < nMaxPips; ++i )
		{
			CArmsRacePipInfo pipData;
			pipData.m_pPip = new panorama::CPanel2D( m_pProgressionXp, nullptr );
			DbgVerify( pipData.m_pPip->BLoadLayoutSnippet( "ProgressionPip" ) );
			m_ArmsRacePips.AddToTail( pipData );
		}
	}

	m_nArmsRaceLevel = -1;
	m_nArmsRacePips = -1;
}

// Add a new weapon to armsrace mode
void CCSGO_WeaponSelectionView::AddArmsRaceWeapon( int iXpRequired, const CEconItemDefinition* pItemDef )
{
	panorama::CPanel2D* pContainer = m_pProgressionWeapons;
	if ( !pContainer )
		pContainer = this;

	CArmsRaceWeapon weapon;
	weapon.m_pItem = new panorama::CPanel2D( pContainer, nullptr );
	DbgVerify( weapon.m_pItem->BLoadLayoutSnippet( "ProgressionIcon" ) );
	weapon.m_nXpRequired = iXpRequired;
	weapon.m_nDefIndex = pItemDef->GetDefinitionIndex();

	// Set up label
	int iLevel = m_ArmsRaceWeapons.Count();
	weapon.m_pItem->SetDialogVariable( "ProgressionIcon--level", iLevel );

	// Set up icon
	const char* szWeaponIconName = pItemDef->GetDefinitionName();
	const char* skip_ = strchr( szWeaponIconName, '_' );
	if ( skip_ )
		szWeaponIconName = skip_ + 1;

	CUtlVector< panorama::CPanel2D* > icons;
	weapon.m_pItem->FindChildrenWithClassTraverse( Symbols().WEAPON_ICON, &icons );
	FOR_EACH_VEC( icons, i )
	{
		panorama::CImagePanel* pIcon = panorama::panel_cast< panorama::CImagePanel* >( icons[i] );
		if ( pIcon )
		{
			pIcon->SetImageJS( CFmtStr( "file://{images}/icons/equipment/%s.svg", szWeaponIconName ) );
			pIcon->SetScaling( panorama::k_EImageScalingNone );
		}
	}

	// done, add it to our list
	m_ArmsRaceWeapons.AddToTail( weapon );
}

void CCSGO_WeaponSelectionView::SetArmsRaceLevel( int iLevel, int iLevelXp )
{
	// count total pips
	int totalXp = 0;
	FOR_EACH_VEC( m_ArmsRaceWeapons, iWeaponLevel )
	{
		if ( iWeaponLevel >= iLevel )
			break;

		totalXp += m_ArmsRaceWeapons[iWeaponLevel].m_nXpRequired;
	}
	totalXp += iLevelXp;

	totalXp = Clamp( totalXp, 0, m_ArmsRacePips.Count() );

	if ( totalXp != m_nArmsRacePips )
	{
		// Update pips
		// TODO: Animate pips in order?
		// TODO: Add 'lost' class for animation?
		if ( totalXp > m_nArmsRacePips )
		{
			for ( int iPip = MAX(m_nArmsRacePips,0); iPip < totalXp; ++iPip )
			{
				m_ArmsRacePips[iPip].m_pPip->SetHasClass( Symbols().progression_pip__current, false );
				m_ArmsRacePips[iPip].m_pPip->SetHasClass( Symbols().progression_pip__earned, true );
			}
		}
		else // ( totalXp < m_nArmsRacePips )
		{
			int topPip = m_nArmsRacePips;
			if ( m_nArmsRacePips < m_ArmsRacePips.Count() )
				++topPip;

			for ( int iPip = totalXp; iPip < topPip; ++iPip )
			{
				m_ArmsRacePips[iPip].m_pPip->SetHasClass( Symbols().progression_pip__current, false );
				m_ArmsRacePips[iPip].m_pPip->SetHasClass( Symbols().progression_pip__earned, false );
			}
		}

		// set current pip
		if ( totalXp < m_ArmsRacePips.Count() )
		{
			m_ArmsRacePips[totalXp].m_pPip->SetHasClass( Symbols().progression_pip__current, true );
			m_ArmsRacePips[totalXp].m_pPip->SetHasClass( Symbols().progression_pip__earned, false );
		}

		m_nArmsRacePips = totalXp;
	}

	if ( m_nArmsRaceLevel != iLevel )
	{
		// Update weapons
		FOR_EACH_VEC( m_ArmsRaceWeapons, iWeaponLevel )
		{
			const CArmsRaceWeapon& weapon = m_ArmsRaceWeapons[iWeaponLevel];
			if ( iWeaponLevel <= iLevel )
			{
				weapon.m_pItem->SetHasClass( Symbols().weapon_selection_progression_item__earned, true );
				weapon.m_pItem->SetHasClass( Symbols().weapon_selection_progression_item__future, false );
				for ( int nNext = 0; nNext < Symbols().kNumProgressionItemNextSymbols; ++nNext )
				{
					weapon.m_pItem->SetHasClass( Symbols().weapon_selection_progression_item__next[nNext], false );
				}
			}
			else if ( iWeaponLevel > iLevel + Symbols().kNumProgressionItemNextSymbols )
			{
				weapon.m_pItem->SetHasClass( Symbols().weapon_selection_progression_item__earned, false );
				weapon.m_pItem->SetHasClass( Symbols().weapon_selection_progression_item__future, true );
				for ( int nNext = 0; nNext < Symbols().kNumProgressionItemNextSymbols; ++nNext )
				{
					weapon.m_pItem->SetHasClass( Symbols().weapon_selection_progression_item__next[nNext], false );
				}
			}
			else
			{
				int nextIdx = iWeaponLevel - iLevel - 1;
				weapon.m_pItem->SetHasClass( Symbols().weapon_selection_progression_item__earned, false );
				weapon.m_pItem->SetHasClass( Symbols().weapon_selection_progression_item__future, false );
				for ( int nNext = 0; nNext < Symbols().kNumProgressionItemNextSymbols; ++nNext )
				{
					weapon.m_pItem->SetHasClass( Symbols().weapon_selection_progression_item__next[nNext], ( nNext == nextIdx ) );
				}
			}
		}

		m_nArmsRaceLevel = iLevel;
	}
}

// Add a new weapon as a demolition reward mode
int CCSGO_WeaponSelectionView::AddDemolitionWeapon( const CEconItemDefinition* pItemDef )
{
	panorama::CPanel2D* pContainer = m_pDemolitionWeapons;
	if ( !pContainer )
		pContainer = this;

	CDemolitionWeapon weapon;
	weapon.m_pItem = new panorama::CPanel2D( pContainer, nullptr );
	DbgVerify( weapon.m_pItem->BLoadLayoutSnippet( "DemolitionIcon" ) );
	weapon.m_nDefIndex = pItemDef->GetDefinitionIndex();

	// Set up icon
	const char* szWeaponIconName = pItemDef->GetDefinitionName();
	const char* skip_ = strchr( szWeaponIconName, '_' );
	if ( skip_ )
		szWeaponIconName = skip_ + 1;

	CUtlVector< panorama::CPanel2D* > icons;
	weapon.m_pItem->FindChildrenWithClassTraverse( Symbols().WEAPON_ICON, &icons );
	FOR_EACH_VEC( icons, i )
	{
		panorama::CImagePanel* pIcon = panorama::panel_cast< panorama::CImagePanel* >( icons[i] );
		if ( pIcon )
		{
			pIcon->SetImageJS( CFmtStr( "file://{images}/icons/equipment/%s.svg", szWeaponIconName ) );
			pIcon->SetScaling( panorama::k_EImageScalingNone );
		}
	}

	// animate it on
	weapon.m_pItem->SetHasClass( Symbols().weapon_selection_demolition_item__active, true );

	// Make sure we show that we are awarding a weapon
	SetHasClass( Symbols().weapon_selection__demolition, true );

	// done, add it to our list
	return m_DemolitionWeapons.AddToTail( weapon );
}

int CCSGO_WeaponSelectionView::RemoveDemolitionWeaponPanel( int iPanelIndex )
{
	if ( iPanelIndex < 0 || iPanelIndex >= m_DemolitionWeapons.Count() )
		return iPanelIndex;

	CDemolitionWeapon& weapon = m_DemolitionWeapons[iPanelIndex];
	delete weapon.m_pItem; // no animation for now
	m_DemolitionWeapons.Remove( iPanelIndex );

	// hide list when no weapon awarded
	if ( !m_DemolitionWeapons.Count() )
		SetHasClass( Symbols().weapon_selection__demolition, false );

	// update iterator after remove
	--iPanelIndex;
	return iPanelIndex;
}

void CCSGO_WeaponSelectionView::InitDemolition()
{
	// nothing needs to happen here
}

void CCSGO_WeaponSelectionView::ShowDemolitionGrenadeReward()
{
	TriggerClass( Symbols().weapon_selection__demolition_reward );
}

void CCSGO_WeaponSelectionView::ShowParachuteIcon( bool bShow )
{
	SetHasClass( Symbols().weapon_selection__parachuteicon_enable, bShow );
}

// Add a new weapon panel.  Returns the index of the panel in m_Weapons.
int CCSGO_WeaponSelectionView::AddWeaponPanel( CHandle<C_BaseCombatWeapon> hWeapon, const CEconItemView* pItem, uint64 xuidShownOwner )
{
	if ( !pItem || !pItem->IsValid() )
		return -1;

	const CWeaponData* pWeaponData = g_pWeaponSystem->GetWeaponData( pItem->GetStaticData()->GetDefinitionIndex() );
	if ( !pWeaponData )
		return -1;

	// Get info we care about from the weapon
	gear_slot_t weaponSlot = ( gear_slot_t )pWeaponData->m_GearSlot;
	gear_slotposition_t weaponSlotPosition = ( gear_slotposition_t )pWeaponData->m_GearSlotPosition;

	// New weapon, create it
	int iWeaponRowPanel = -1;
	FOR_EACH_VEC( m_WeaponRows, i )
	{
		if ( m_WeaponRows[i].m_nGearSlot == weaponSlot )
		{
			iWeaponRowPanel = i;
			break;
		}
	}

	if ( iWeaponRowPanel == -1 )
	{
		// Need to create a new row
		CWeaponRowPanelInfo rowInfo;
		rowInfo.m_nGearSlot = weaponSlot;
		rowInfo.m_pRow = new panorama::CPanel2D( m_pWeaponList, nullptr );
		DbgVerify( rowInfo.m_pRow->BLoadLayoutSnippet( "WeaponRow" ) );

		rowInfo.m_pItemName = rowInfo.m_pRow->FindChildInLayoutFile( "weaponname" );
		rowInfo.m_pContainer = rowInfo.m_pRow->FindChildInLayoutFile( "icon-container" );
		if ( !rowInfo.m_pContainer )
			rowInfo.m_pContainer = rowInfo.m_pRow;

		// move it to the correct location
		panorama::CPanel2D* pBefore = nullptr;
		gear_slot_t nMaxSmallerGearSlot = GEAR_SLOT_INVALID;

		FOR_EACH_VEC( m_WeaponRows, i )
		{
			CWeaponRowPanelInfo& checkInfo = m_WeaponRows[i];
			if ( checkInfo.m_nGearSlot < weaponSlot && checkInfo.m_nGearSlot > nMaxSmallerGearSlot )
			{
				pBefore = m_WeaponRows[i].m_pRow;
				nMaxSmallerGearSlot = checkInfo.m_nGearSlot;
			}
		}
		if ( pBefore )
		{
			m_pWeaponList->MoveChildBefore( rowInfo.m_pRow, pBefore );
		}

		int nBindingWeaponSlot = ( ( int )weaponSlot ) + 1;
		const char* keyBinding = engine->Key_LookupBinding( CFmtStr( "slot%d", nBindingWeaponSlot ) );

		// HACK: Special case for tablet slot in survival -- you can switch to the tablet with TAB and M
		// Note that this doesn't make sense if you can have more than 1 item in your UTILITY slot, since it
		// always switches to the tablet.
		if ( !keyBinding && weaponSlot == GEAR_SLOT_UTILITY && CSGameRules() && CSGameRules()->IsPlayingSurvival() )
		{
			keyBinding = engine->Key_LookupBinding( "+showscores" );
			if ( !keyBinding )
				keyBinding = engine->Key_LookupBinding( "teammenu" );
		}

		CFmtStr strBinding;
		if ( keyBinding )
		{
			strBinding.Append( keyBinding );
			V_strupr( strBinding.Access() );
		}
		else
		{
			strBinding = ""; // row with no key binding shows nothing
		}

		rowInfo.m_pRow->SetDialogVariable( "WeaponRow--binding", strBinding.Get() );
		rowInfo.m_pRow->SetDialogVariable( "WeaponRow--number", nBindingWeaponSlot );
		rowInfo.m_pRow->SetDialogVariable( "WeaponRow--name", " " );

		// animate it on
		rowInfo.m_pRow->SetHasClass( Symbols().weapon_row__active, true );

		// and add it to our list
		iWeaponRowPanel = m_WeaponRows.AddToTail( rowInfo );
	}

	panorama::CPanel2D* pContainer = m_WeaponRows[iWeaponRowPanel].m_pContainer;

	int nWeaponIdx = m_Weapons.AddToTail();
	CWeaponPanelInfo& panelInfo = m_Weapons[nWeaponIdx];
	panelInfo.m_nGearSlot = weaponSlot;
	panelInfo.m_nGearSlotPosition = weaponSlotPosition;
	panelInfo.m_hWeapon = hWeapon;
	panelInfo.m_nItemDefIndex = pItem->GetStaticData()->GetDefinitionIndex();
	panelInfo.m_pItem = new panorama::CPanel2D( pContainer, nullptr );
	panelInfo.m_nItemCount = 1;

	// The bomb has a slightly different layout than other items
	static const CSchemaItemDefHandle kWeaponC4ItemDefinition( "weapon_c4" );
	if ( kWeaponC4ItemDefinition && panelInfo.m_nItemDefIndex == kWeaponC4ItemDefinition->GetDefinitionIndex() )
		DbgVerify( panelInfo.m_pItem->BLoadLayoutSnippet( "WeaponIcon--Bomb" ) );
	else
		DbgVerify( panelInfo.m_pItem->BLoadLayoutSnippet( "WeaponIcon" ) );

	panelInfo.m_pItem->SetDialogVariable( "WeaponIcon--itemcount", 1 );

	// Find icons
	CUtlVector< panorama::CPanel2D* > icons;
	panelInfo.m_pItem->FindChildrenWithClassTraverse( Symbols().WEAPON_ICON, &icons );
	FOR_EACH_VEC( icons, i )
	{
		panorama::CImagePanel* pIcon = panorama::panel_cast< panorama::CImagePanel* >( icons[i] );
		if ( pIcon )
		{
			panelInfo.m_Icons.AddToTail( pIcon );
		}
	}

	const char* szWeaponIconName = pItem->GetStaticData()->GetDefinitionName();
	const char* skip_ = strchr( szWeaponIconName, '_' );
	if ( skip_ )
		szWeaponIconName = skip_ + 1;

	FOR_EACH_VEC( panelInfo.m_Icons, i )
	{
		panorama::CImagePanel* pIcon = panelInfo.m_Icons[i];
		pIcon->SetImageJS( CFmtStr( "file://{images}/icons/equipment/%s.svg", szWeaponIconName ) );
		pIcon->SetScaling( panorama::k_EImageScalingNone );
	}

	// Set rarity style
	{
		int rarity = pItem->GetRarity();
		panelInfo.m_pItem->SetHasClass( CFmtStr( "weapon-selection-item--rarity-%d", rarity ), true );
	}

	// Set name
	{
		char weaponNameUtf8[512];
		V_UnicodeToUTF8( pItem->GetItemName(), weaponNameUtf8, sizeof( weaponNameUtf8 ) );
		panelInfo.m_pItem->SetDialogVariable( "WeaponIcon--name", weaponNameUtf8 );
	}

	// Set owner's name
	{
		panelInfo.m_pItem->SetHasClass( Symbols().weapon_selection_item__show_owner, false );
		panelInfo.m_pItem->SetDialogVariable( "WeaponIcon--owner", "" );
	}

	// Find the correct position in the row for the new item
	panorama::CPanel2D* pBefore = nullptr;
	gear_slotposition_t nMinBiggerGearSlotPosition = GEAR_SLOTPOSITION_MAX;
	FOR_EACH_VEC( m_Weapons, i )
	{
		CWeaponPanelInfo& checkInfo = m_Weapons[i];
		if ( checkInfo.m_nGearSlot == weaponSlot
			&& checkInfo.m_nGearSlotPosition > weaponSlotPosition
			&& checkInfo.m_nGearSlotPosition < nMinBiggerGearSlotPosition )
		{
			pBefore = checkInfo.m_pItem;
			nMinBiggerGearSlotPosition = checkInfo.m_nGearSlotPosition;
		}
	}

	if ( pBefore )
		pContainer->MoveChildBefore( panelInfo.m_pItem, pBefore );

	// By default items are allowed to fade out
	panelInfo.m_pItem->SetHasClass( Symbols().weapon_selection_item__fade_ok, true );

	// animate it on
	panelInfo.m_pItem->SetHasClass( Symbols().weapon_selection_item__active, true );

	return nWeaponIdx;
}

// Delete a weapon panel from the DOM.  Returns an updated panel index for use in FOR_EACH_VEC 
int CCSGO_WeaponSelectionView::DeleteWeaponPanel( int iPanel )
{
	CWeaponPanelInfo& weaponInfo = m_Weapons[iPanel];

	if(iPanel == m_nSelectedWeapon)
	{
		SetSelectedPanel( -1, nullptr );
	}

	Assert( iPanel != m_nSelectedWeapon );

	// Weapon no longer belongs to this player
	if ( !m_nAnimationDisabledCount )
	{
		weaponInfo.m_pItem->SetHasClass( Symbols().weapon_selection_item__deleted, true );
		weaponInfo.m_pItem->SetHasClass( Symbols().weapon_selection_item__active, false );
		weaponInfo.m_pItem->DeleteAsync( kWeaponSelectAnimationTime_Drop ); // delete after animation finishes
	}
	else
	{
		delete weaponInfo.m_pItem; // NOTE: pointers inside weaponInfo are all invalid now
	}

	// Remove from vector
	m_Weapons.FastRemove( iPanel );

	if ( m_nSelectedWeapon == m_Weapons.Count() )
		m_nSelectedWeapon = iPanel; // FastRemove() moved the last item here, update it
	
	// update iterator after remove
	--iPanel;

	return iPanel;
}

// Find the index in m_WeaponRows for a particular panel
int CCSGO_WeaponSelectionView::FindWeaponRow( int iPanel )
{
	if ( iPanel < 0 || iPanel >= m_Weapons.Count() )
		return -1;

	gear_slot_t gearSlot = m_Weapons[iPanel].m_nGearSlot;

	FOR_EACH_VEC( m_WeaponRows, i )
	{
		if ( m_WeaponRows[i].m_nGearSlot == gearSlot )
			return i; // found it
	}

	AssertMsg1( false, "No row created for weapon panel in slot %d", ( int )gearSlot );
	return -1;
}

// Set the currently selected weapon panel
bool CCSGO_WeaponSelectionView::SetSelectedPanel( int iPanel, const CEconItemView* pWeaponItem )
{
	if ( iPanel == m_nSelectedWeapon )
		return false;

	// Unselect previous item
	if ( m_nSelectedWeapon >= 0 )
	{
		Assert( m_nSelectedWeapon < m_Weapons.Count() );
		Assert( m_nSelectedRow >= 0 && m_nSelectedRow < m_WeaponRows.Count() );

		CWeaponPanelInfo& weaponPanel = m_Weapons[m_nSelectedWeapon];
		CWeaponRowPanelInfo& weaponRow = m_WeaponRows[m_nSelectedRow];

		weaponRow.m_pRow->SetDialogVariable( "WeaponRow--name", " " );
		weaponRow.m_pRow->SetHasClass( Symbols().weapon_row__selected, false );
		weaponPanel.m_pItem->SetHasClass( Symbols().weapon_selection_item__selected, false );
	
		m_nSelectedWeapon = -1;
		m_nSelectedRow = -1;
	}

	// Select new item
	if ( iPanel >= 0 && iPanel < m_Weapons.Count() )
	{
		m_nSelectedWeapon = iPanel;
		m_nSelectedRow = FindWeaponRow( m_nSelectedWeapon );
		Assert( m_nSelectedRow >= 0 && m_nSelectedRow < m_WeaponRows.Count() ); // weapon in nonexistant row somehow?

		CWeaponPanelInfo& weaponPanel = m_Weapons[m_nSelectedWeapon];
		CWeaponRowPanelInfo& weaponRow = m_WeaponRows[m_nSelectedRow];
		weaponPanel.m_pItem->SetHasClass( Symbols().weapon_selection_item__selected, true );
		weaponRow.m_pRow->SetHasClass( Symbols().weapon_row__selected, true );

		if( pWeaponItem && pWeaponItem->IsValid() )
		{
			char weaponNameUtf8[512];
			V_UnicodeToUTF8( pWeaponItem->GetItemName(), weaponNameUtf8, sizeof( weaponNameUtf8 ) );
			weaponRow.m_pRow->SetDialogVariable( "WeaponRow--name", weaponNameUtf8 );
		}
		else
		{
			weaponRow.m_pRow->SetDialogVariable( "WeaponRow--name", " " );
		}

		// set label x position -- solved this in a different way
		//if ( weaponRow.m_pItemName )
		//{
		//}
	}

	return true;
}

void CCSGO_WeaponSelectionView::SetWeaponPanelItemCount( int iPanel, int iCount )
{
	if ( iPanel < 0 || iPanel >= m_Weapons.Count() )
		return;

	CWeaponPanelInfo& weaponPanel = m_Weapons[iPanel];

	if ( iCount > 1 && iCount > weaponPanel.m_nItemCount )
	{
		// blink again when we pick up more copies of an item
		weaponPanel.m_pItem->TriggerClass( Symbols().weapon_selection_item__active );
	}

	if ( weaponPanel.m_nItemCount != iCount )
	{
		weaponPanel.m_nItemCount = iCount;
		weaponPanel.m_pItem->SetHasClass( Symbols().weapon_selection_item__has_multiple, iCount > 1 );
		weaponPanel.m_pItem->SetDialogVariable( "WeaponIcon--itemcount", iCount );
	}
}

void CCSGO_WeaponSelectionView::SetPanelFadeEnabled( int iPanel, bool bEnabled )
{
	if ( iPanel < 0 || iPanel >= m_Weapons.Count() )
		return;

	CWeaponPanelInfo& weaponPanel = m_Weapons[iPanel];
	weaponPanel.m_pItem->SetHasClass( Symbols().weapon_selection_item__fade_ok, bEnabled );
}

void CCSGO_WeaponSelectionView::CleanupDeletedRows()
{
	// Remove any rows that now have no weapons
	FOR_EACH_VEC( m_WeaponRows, iRow )
	{
		gear_slot_t weaponSlot = m_WeaponRows[iRow].m_nGearSlot;
		bool anyFound = false;
		FOR_EACH_VEC( m_Weapons, iWeapon )
		{
			if ( m_Weapons[iWeapon].m_nGearSlot == weaponSlot )
			{
				anyFound = true;
				break;
			}
		}

		if ( !anyFound )
		{
			// Shouldn't be possible to delete a selected row since it has no weapons in it to be selected
			Assert( m_nSelectedRow != iRow );

			// delete this row
			if ( !m_nAnimationDisabledCount )
			{
				m_WeaponRows[iRow].m_pRow->SetHasClass( Symbols().weapon_row__deleted, true );
				m_WeaponRows[iRow].m_pRow->SetHasClass( Symbols().weapon_row__active, false );
				m_WeaponRows[iRow].m_pRow->DeleteAsync( kWeaponSelectAnimationTime_DeleteRow );
			}
			else
			{
				delete m_WeaponRows[iRow].m_pRow; // NOTE: panel pointers inside m_WeaponRows[iRow] are all invalidated here
			}

			// remove from vector
			m_WeaponRows.FastRemove( iRow );

			if ( m_nSelectedRow == m_WeaponRows.Count() )
				m_nSelectedRow = iRow; // FastRemove() moved the last row to this slot so update it

			// update iterator after remove
			--iRow;
		}
	}
}


void CCSGO_WeaponSelectionView::Fade( bool bFade )
{
	m_bFaded = bFade;
	SetHasClass( Symbols().weapon_selection__fade, bFade );
}

bool CCSGO_WeaponSelectionView::IsFaded()
{
	return m_bFaded;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudWeaponSelection::CCSGO_HudWeaponSelection( panorama::CPanel2D *pParent, const char *pchID )
	: CCSGO_WeaponSelectionView( pParent, pchID )
	, CPanoramaHudElement( "CCSGO_HudWeaponSelection", this )
	, m_bVisible( false )
	, m_bHudBombInWeaponSelection( false )
	, m_bAnimationDisabledDueToRoundStart( false )
{
	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_MISCSTATUS | HIDEHUD_WEAPONSELECTION );
	ListenForGameEvent( "weaponhud_selection" );
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "spec_target_updated" );
	ListenForGameEvent( "gg_player_impending_upgrade" );
	ListenForGameEvent( "ggprogressive_player_levelup" );

	m_fNextUpdateTime = -1;
	m_nArmsRaceWeapons = -1;
	m_nDemolitionKillPoints = -1;
	m_nDemolitionCurWeapon = -1;
	m_nDemolitionTeam = -1;

	m_fFadeTime = gpGlobals->curtime + kWeaponSelectTimeUntilFade;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudWeaponSelection::~CCSGO_HudWeaponSelection()
{

}


//-----------------------------------------------------------------------------
// Purpose: Handle hotloading of our panel data
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponSelection::OnLayoutReloaded()
{
	BaseClass::OnLayoutReloaded();

	// Just re-use level-init code to clear everything and display new data
	LevelInit();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponSelection::LevelInit( void )
{
	CPanoramaHudElement::LevelInit();

	m_nArmsRaceWeapons = -1;
	m_nDemolitionKillPoints = -1;
	m_nDemolitionCurWeapon = -1;
	m_nDemolitionTeam = -1;

	ShowPanel( false );
	ResetUpdateTime();

	DeleteDynamicElements();
	SetupArmsRace();
	SetupDemolition();
}

void CCSGO_HudWeaponSelection::SetupArmsRace()
{
	if ( !CSGameRules() )
		return;

	if ( !CSGameRules()->IsPlayingGunGameProgressive() )
		return;

	// Set up weapons
	// Note: We currently use the same weapons for both teams in arms race
	int maxLevel = CSGameRules()->GetNumProgressiveGunGameWeapons( TEAM_CT ) - 1; // if 17 weapons, then max level is 16 (0-16)
	int maxXp = 0;

	for ( int i = 0; i < CSGameRules()->GetNumProgressiveGunGameWeapons( TEAM_CT ); ++i )
	{
		maxXp += CSGameRules()->GetGunGameNumKillsRequiredForWeapon( i, TEAM_CT );
	}
	InitArmsRace( maxLevel, maxXp );

	// now add weapons
	for ( int i = 0; i < CSGameRules()->GetNumProgressiveGunGameWeapons( TEAM_CT ); ++i )
	{
		AddArmsRaceWeapon( CSGameRules()->GetGunGameNumKillsRequiredForWeapon( i, TEAM_CT ), CSGameRules()->GetProgressiveGunGameWeapon( i, TEAM_CT ) );
	}

	// Cache in case this changes
	m_nArmsRaceWeapons = CSGameRules()->GetNumProgressiveGunGameWeapons( TEAM_CT );
}


void CCSGO_HudWeaponSelection::SetupDemolition()
{
	if ( !CSGameRules() )
		return;

	if ( !CSGameRules()->IsPlayingGunGameTRBomb() )
		return;

	InitDemolition();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponSelection::LevelShutdown( void )
{
	CPanoramaHudElement::LevelShutdown();

	ShowPanel( false );
	ResetUpdateTime();
	DeleteDynamicElements();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponSelection::SetActive( bool bActive )
{
	if ( bActive != m_bVisible )
	{
		ShowPanel( bActive );
	}

	if ( bActive == false && m_bActive == true )
	{
		// We want to continue to run ProcessInput while the HUD element is hidden
		// so that the notifications continue advancing down the screen
		return;
	}

	CPanoramaHudElement::SetActive( bActive );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudWeaponSelection::ShouldDraw( void )
{
	return cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false && CPanoramaHudElement::ShouldDraw();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponSelection::ShowPanel( bool bShow )
{
	if ( m_bVisible != bShow )
	{
		m_bVisible = bShow;
		ResetUpdateTime();
	}

	SetHasClass( Symbols().weapon_selection__hidden, ( m_bVisible == false ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponSelection::Think()
{
	CPanoramaHudElement::Think();

	if ( gpGlobals->curtime >= m_fNextUpdateTime )
	{
		// Update at least 4 times per second; more if an event happens in the intervening time
		// Parts of this 'think' function might set the next update time to sometime in the past; that just means
		// that we will update again instantly on the next frame.
		m_fNextUpdateTime = gpGlobals->curtime + 0.25f;

		C_CSPlayer* pOldPlayer = m_hPlayer.Get();
		C_CSPlayer* pPlayer = ToCSPlayer( GetLocalOrObservedPlayer() );
		bool bChangedPlayer = false;

		if ( ( !pOldPlayer && m_hPlayer.IsValid() ) || pPlayer != pOldPlayer )
		{
			bChangedPlayer = true;
			AddAnimationDisable(); // needs to be paired with RemoveAnimationDisable() later in this function
			m_hPlayer = pPlayer;
		}

		UpdateWeapons( pPlayer, bChangedPlayer );
		UpdateArmsRaceXp( pPlayer, bChangedPlayer );
		UpdateArmsRaceWeapons( pPlayer, bChangedPlayer );
		UpdateDemolitionWeapons( pPlayer, bChangedPlayer );

		ShowParachuteIcon( pPlayer ? pPlayer->m_bHasParachute.Get() : false );

		// Handle toggling cl_showloadout
		if ( IsFaded() && ShouldNeverFade() )
		{
			Fade( false );
		}

		// Handle fade out
		if ( m_fFadeTime > 0 && gpGlobals->curtime >= m_fFadeTime )
		{
			m_fFadeTime = -1;

			if ( !ShouldNeverFade() )
				Fade( true );
		}

		// Handle bomb zone
		SetHasClass( Symbols().weapon_selection__bomb_zone, pPlayer ? pPlayer->m_bInBombZone : false );

		// re-enable animations
		if ( bChangedPlayer )
		{
			RemoveAnimationDisable();
		}

		// re-enable animations once we have initialized starting weapons for the round
		if ( m_bAnimationDisabledDueToRoundStart )
		{
			RemoveAnimationDisable();
			m_bAnimationDisabledDueToRoundStart = false;
		}
	}

}


void CCSGO_HudWeaponSelection::UpdateWeapons( C_CSPlayer* pPlayer, bool bPlayerChanged )
{
	if ( !pPlayer )
		return;

	// Do nothing if weapons haven't changed
	// TODO: is this count check good enough?
	//if ( !bActiveWeaponChanged && pPlayer->WeaponCount() == m_Weapons.Count() )
	//	return;

	bool anyWeaponsRemoved = false;
	bool anyChanges = false;

	// Remove weapons that have been dropped
	FOR_EACH_VEC( m_Weapons, i )
	{
		CWeaponPanelInfo& weaponInfo = m_Weapons[i];
		C_BaseCombatWeapon* pWeapon = weaponInfo.m_hWeapon;

		if ( !pWeapon
			|| pWeapon->GetOwner() != pPlayer
			|| pWeapon->IsDormant()
			|| !pWeapon->VisibleInWeaponSelection()
			/* || pWeapon->WeaponState() == WEAPON_NOT_CARRIED */ )
		{
			i = DeleteWeaponPanel( i );
			anyWeaponsRemoved = true;
			anyChanges = true;
		}
	}

	C_BaseCombatWeapon* pActiveWeapon = pPlayer->GetActiveWeapon();

	// Handle change in bomb rendering state
	bool bInitBombFade = false;
	bool bHudBombInWeaponSelection = !cl_hud_bomb_under_radar.GetBool();
	if ( m_bHudBombInWeaponSelection != bHudBombInWeaponSelection )
	{
		m_bHudBombInWeaponSelection = bHudBombInWeaponSelection;
		bInitBombFade = true;
	}

	// Add new weapons
	for ( int iWeapon = 0; iWeapon < pPlayer->WeaponCount(); ++iWeapon )
	{
		C_WeaponCSBase* pWeapon = assert_cast<C_WeaponCSBase*>( pPlayer->GetWeapon( iWeapon ) );
		if ( !pWeapon 
			|| !pWeapon->VisibleInWeaponSelection() )
			continue;

		Assert( pWeapon->GetOwner() == pPlayer );
		//Assert( pWeapon->WeaponState() != WEAPON_NOT_CARRIED );
		Assert( !pWeapon->IsDormant() );

		int iWeaponPanel = -1;
		FOR_EACH_VEC( m_Weapons, i )
		{
			if ( m_Weapons[i].m_hWeapon == pWeapon )
			{
				iWeaponPanel = i;
				break;
			}
		}

		if ( iWeaponPanel == -1 )
		{
			iWeaponPanel = AddWeaponPanel( pWeapon, pWeapon->GetEconItemView(), GetOwnerXuid( pPlayer, pWeapon ) );
			anyChanges = true;
			if ( pWeapon->IsA( WEAPON_C4 ) )
				bInitBombFade = true; // force initialization of fade state on create
		}
		Assert( iWeaponPanel >= 0 );

		gear_slot_t nSlot = pWeapon->GetGearSlot();
		if ( nSlot == GEAR_SLOT_GRENADES || pWeapon->GetWeaponType() == WEAPONTYPE_STACKABLEITEM )
		{
			// Display the ammo count
			int ammo = pWeapon->UsesPrimaryAmmo() ? pWeapon->Clip1() : 0;
			if ( ammo < 0 )
				ammo = pWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );

			SetWeaponPanelItemCount( iWeaponPanel, ammo );
		}

		if ( pWeapon == pActiveWeapon )
		{
			if ( SetSelectedPanel( iWeaponPanel, pActiveWeapon ? pActiveWeapon->GetEconItemView() : nullptr ) )
				anyChanges = true;
		}

		if ( bInitBombFade && pWeapon->IsA( WEAPON_C4 ) )
		{
			// Don't fade the bomb if we are always drawing it in weapon selection
			SetPanelFadeEnabled( iWeaponPanel, !bHudBombInWeaponSelection );
		}
	}

	if ( anyWeaponsRemoved )
		CleanupDeletedRows();

	if ( anyChanges )
	{
		Fade( false );
		m_fFadeTime = gpGlobals->curtime + kWeaponSelectTimeUntilFade;
	}
}

void CCSGO_HudWeaponSelection::UpdateArmsRaceXp( C_CSPlayer* pPlayer, bool bPlayerChanged )
{
	if ( !CSGameRules() )
		return;
	if ( !CSGameRules()->IsPlayingGunGameProgressive() )
		return;

	if ( !pPlayer )
		return;

	int iLevel = pPlayer->GetPlayerGunGameWeaponIndex();
	int iXp = pPlayer->GetNumGunGameKillsWithCurrentWeapon();

	if ( pPlayer->MadeFinalGunGameProgressiveKill() )
	{
		iLevel++;
		iXp = 0;
	}

	SetArmsRaceLevel( iLevel, iXp );
}

void CCSGO_HudWeaponSelection::UpdateArmsRaceWeapons( C_CSPlayer* pPlayer, bool bPlayerChanged )
{
	// We don't have the weapon list on the client when we initialize, so if it changes we update it here.

	if ( !CSGameRules() )
		return;

	if ( !CSGameRules()->IsPlayingGunGameProgressive() )
		return;

	if ( CSGameRules()->GetNumProgressiveGunGameWeapons( TEAM_CT ) != m_nArmsRaceWeapons )
		SetupArmsRace();
}

extern ConVar mp_ggtr_bomb_pts_for_upgrade;

void CCSGO_HudWeaponSelection::UpdateDemolitionWeapons( C_CSPlayer* pPlayer, bool bPlayerChanged )
{
	if ( !CSGameRules() )
		return;

	if ( !CSGameRules()->IsPlayingGunGameTRBomb() )
		return;

	if ( !pPlayer )
	{
		// clear everything if we aren't observing a player
		while ( m_DemolitionWeapons.Count() > 0 )
			RemoveDemolitionWeaponPanel( 0 );

		return;
	}

	// Figure out next items
	int nKillPoints = pPlayer->GetNumGunGameTRKillPoints();
	int nCurWeapon = pPlayer->GetPlayerGunGameWeaponIndex();
	int nTeam = pPlayer->GetTeamNumber();

	if ( bPlayerChanged || nKillPoints != m_nDemolitionKillPoints || nCurWeapon != m_nDemolitionCurWeapon || nTeam != m_nDemolitionTeam )
	{
		const CEconItemDefinition* pNextGun = nullptr;
		if ( nKillPoints >= mp_ggtr_bomb_pts_for_upgrade.GetInt() )
			pNextGun = CSGameRules()->GetNextGunGameWeapon( nCurWeapon, nTeam );

		const CEconItemDefinition* pNextGrenade = nullptr;
		if ( pNextGun ) // only can be awarded a grenade if we are being awarded a new gun
			pNextGrenade = CSGameRules()->GetGunGameTRBonusGrenade( pPlayer );

		// Last round of each half doesn't have a next weapon.
		extern ConVar mp_maxrounds;
		int nextRound = CSGameRules()->GetTotalRoundsPlayed() + 1;
		if ( nextRound >= mp_maxrounds.GetInt()
			|| ( CSGameRules()->HasHalfTime() && nextRound == mp_maxrounds.GetInt() / 2 ) )
		{
			pNextGun = pNextGrenade = nullptr;
		}

		if ( !pNextGun || m_DemolitionWeapons.Count() < 1 || m_DemolitionWeapons[0].m_nDefIndex != pNextGun->GetDefinitionIndex() )
		{
			// Update gun.  Need to clear panel completely first
			while ( m_DemolitionWeapons.Count() > 0 )
				RemoveDemolitionWeaponPanel( 0 );

			if ( pNextGun )
				AddDemolitionWeapon( pNextGun );
		}

		if ( !pNextGrenade || m_DemolitionWeapons.Count() < 2 || m_DemolitionWeapons[1].m_nDefIndex != pNextGrenade->GetDefinitionIndex() )
		{
			// Update grenade.  Remove any existing grenades
			while ( m_DemolitionWeapons.Count() > 1 )
				RemoveDemolitionWeaponPanel( 1 );

			if ( pNextGrenade )
				AddDemolitionWeapon( pNextGrenade );
		}

		// If we just earned a new grenade, flash reward
		if ( !bPlayerChanged && nKillPoints > m_nDemolitionKillPoints && pNextGrenade )
		{
			ShowDemolitionGrenadeReward();
		}

		m_nDemolitionKillPoints = nKillPoints;
		m_nDemolitionCurWeapon = nCurWeapon;
		m_nDemolitionTeam = nTeam;
	}
}




bool CCSGO_HudWeaponSelection::ShouldNeverFade()
{
	if ( cl_showloadout.GetBool() )
		return true;
	
	if ( CSGameRules() && ( CSGameRules()->IsPlayingGunGameProgressive() || CSGameRules()->IsPlayingSurvival() ) )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Gameplay business logic for when to show owner of a weapon
//-----------------------------------------------------------------------------
uint64 CCSGO_HudWeaponSelection::GetOwnerXuid( C_CSPlayer* pHudPlayer, C_WeaponCSBase* pWeapon )
{
	/* Removed for partner depot */
	return 0;
}



//-----------------------------------------------------------------------------
// Purpose: Update instantly when game code reports a weapon switch instead
//          of waiting for next think() tick
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponSelection::FireGameEvent( IGameEvent* pEvent )
{
	CPanoramaHudElement::FireGameEvent( pEvent );
	
	const char* szEventType = pEvent->GetName();
	if ( !V_strcmp( szEventType, "weaponhud_selection" )
		|| !V_strcmp( szEventType, "round_start" )
		|| !V_strcmp( szEventType, "spec_target_updated" )
		|| !V_strcmp( szEventType, "gg_player_impending_upgrade" )
		|| !V_strcmp( szEventType, "ggprogressive_player_levelup" ) )
	{
		// Update on next think()
		ResetUpdateTime();
	}

	if ( !V_strcmp( szEventType, "round_start" ) )
	{
		if ( !m_bAnimationDisabledDueToRoundStart )
		{
			m_bAnimationDisabledDueToRoundStart = true;
			AddAnimationDisable(); // paired with RemoveAnimationDisable() in next think()
		}
	}
}
