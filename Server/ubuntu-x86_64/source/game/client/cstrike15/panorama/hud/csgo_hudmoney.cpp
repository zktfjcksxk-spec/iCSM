//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Display money balance and buy zone 
//
// SF differences :
//		* nothing working yet
//=============================================================================//

#include "cbase.h"
#include "csgo_hudmoney.h"
#include "csgo_hud.h"
#include "c_cs_player.h"
#include "clientmode_csnormal.h"

#if defined( INCLUDE_SCALEFORM )
#include "HUD/sfhudfreezepanel.h"
#endif

#include "panorama/uievents.h"
#include "panorama/controls/label.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


REGISTER_PANEL2D_FACTORY( CCSGOMoneyPanel, CSGOMoneyPanel );
REGISTER_PANEL2D_FACTORY( CCSGO_HudMoney, CSGOHudMoney );
REGISTER_PANEL2D_FACTORY( CCSGOMoneyAnimLabel, CSGOMoneyAnimLabel );

extern ConVar cl_drawhud;
extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_hud_color;
extern bool IsTakingAFreezecamScreenshot( void );
extern ConVar cl_hud_background_alpha;

class CCSGOMoneyPanelSymbols
{
public:
	panorama::CPanoramaSymbol AddMoney;
	panorama::CPanoramaSymbol AddMoneyFlash;
	panorama::CPanoramaSymbol RemoveMoney;
	panorama::CPanoramaSymbol InBuyZone;

	CCSGOMoneyPanelSymbols()
		: AddMoney( "money-anim__add" )
		, AddMoneyFlash( "money-anim__add-flash" )
		, RemoveMoney( "money-anim__remove" )
		, InBuyZone( "money__in-buy-zone" )
	{
	}
};

// Initialize on first use
const CCSGOMoneyPanelSymbols& CCSGOMoneyPanel::Symbols()
{
	static const CCSGOMoneyPanelSymbols s_symbols;
	return s_symbols;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGOMoneyAnimLabel::CCSGOMoneyAnimLabel( panorama::CPanel2D *pParent, const char *pchID )
	: BaseClass( pParent, pchID )
	, m_CurrentAnimation()
	, m_nAnimationCount( 0 )
	, m_pOwner( nullptr )
{
	RegisterEventHandler( panorama::AnimationEnd(), this, &CCSGOMoneyAnimLabel::EventAnimationEnd );
}

void CCSGOMoneyAnimLabel::StartAnimation( panorama::CPanoramaSymbol symClassName )
{
	if ( m_CurrentAnimation != symClassName )
	{
		// Changing animations, we'll automatically ignore the EventAnimationEnd because
		// it doesn't refer to the current class
		StopAnimation();
	}

	// If we have an animation count then we should have a valid current animation
	Assert( m_nAnimationCount == 0 || m_CurrentAnimation.IsValid() );

	++m_nAnimationCount;
	m_CurrentAnimation = symClassName;
	TriggerClass( symClassName );
}

void CCSGOMoneyAnimLabel::StopAnimation()
{
	if ( m_CurrentAnimation.IsValid() )
	{
		panorama::CPanoramaSymbol symCurrentAnim = m_CurrentAnimation;

		m_nAnimationCount = 0;
		m_CurrentAnimation = panorama::CPanoramaSymbol(); // invalidate

		RemoveClass( symCurrentAnim );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGOMoneyAnimLabel::EventAnimationEnd( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr, panorama::CPanoramaSymbol symAnimation )
{
	if ( symAnimation != m_CurrentAnimation )
		return true;

	// We need to ignore the spurious EventAnimationEnd() that comes from TriggerClass() resetting our current animation to the beginning
	if ( --m_nAnimationCount > 0 )
		return true;

	Assert( m_nAnimationCount == 0 ); // somehow we ended more animations than we started?
	m_nAnimationCount = 0;
	m_CurrentAnimation = panorama::CPanoramaSymbol(); // invalidate

	// kill the animation
	// NOTE: This relies on the @keyframe animation name being the same as the class name that triggers that animation
	RemoveClass( symAnimation );

	// if relevant, tell our parent that our animation is done
	if ( symAnimation == Symbols().AddMoney )
		m_pOwner->DoneAnimatingAdd();

	if ( symAnimation == Symbols().RemoveMoney )
		m_pOwner->DoneAnimatingSub();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGOMoneyPanel::CCSGOMoneyPanel( panorama::CPanel2D *pParent, const char *pchID )
	: BaseClass( pParent, pchID )
	, m_bShowBuyZoneIcon( false )
	, m_nLastMoney( 0 )
	, m_hPlayer( INVALID_EHANDLE )
	, m_nTotalAdd( 0 )
	, m_nTotalSub( 0 )
{
 	RequireLoadLayout( "file://{resources}/layout/hud/hudmoney.xml" );

	m_pAddMoneyLabel = panorama::panel_cast< CCSGOMoneyAnimLabel * >( RequireChildInLayoutFile( "Money-Add" ) );
	m_pAddMoneyFlashLabel = panorama::panel_cast< CCSGOMoneyAnimLabel * >( RequireChildInLayoutFile( "Money-AddFlash" ) );
	m_pRemoveMoneyLabel = panorama::panel_cast< CCSGOMoneyAnimLabel * >( RequireChildInLayoutFile( "Money-Remove" ) );
	m_pMoneyBG = RequireChildInLayoutFile( "MoneyBG" );	

	SetDialogVariable( "hud-money-amount", 0 );

	m_pRemoveMoneyLabel->m_pOwner = this;
	m_pAddMoneyLabel->m_pOwner = this;
	m_pAddMoneyFlashLabel->m_pOwner = this;

	Reset();

	RegisterForUnhandledEvent( CSGOFrameUpdate(), this, &CCSGOMoneyPanel::EventCSGOFrameUpdate );
}

CCSGO_HudMoney::CCSGO_HudMoney( panorama::CPanel2D* parent, const char* pchID )
	: BaseClass( parent, pchID )
	, CPanoramaHudElement( "CCSGO_HudMoney", this )
{
	// 	SetHiddenBits( HIDEHUD_PLAYERDEAD );
	SetIgnoreGlobalHudDisable( true );

	// Hide panel initially
	ShowPanel( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudMoney::SetActive( bool bActive )
{
	if ( bActive != m_bActive )
	{
		ShowPanel( bActive );
	}

	CPanoramaHudElement::SetActive( bActive );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this hud element should be visible in the current hud state
//-----------------------------------------------------------------------------
bool CCSGO_HudMoney::ShouldDraw()
{
	if ( IsTakingAFreezecamScreenshot() )
 		return false;

	if ( !CSGameRules() )
		return false;

	if ( CSGameRules()->IsPlayingTraining() || !CSGameRules()->CanSpendMoneyInMap() )
		return false;

	IViewPortPanel *scoreboard = NULL;
	if ( GetViewPortInterface() )
	{
		scoreboard = GetViewPortInterface()->FindPanelByName( PANEL_SCOREBOARD );
	}

	if ( CSGameRules()->GetGamePhase() == GAMEPHASE_MATCH_ENDED && scoreboard && scoreboard->IsVisible() )
		return false;

	bool bGloballyHidden = GetHud().HudDisabled();

	return cl_drawhud.GetBool() && !bGloballyHidden && cl_draw_only_deathnotices.GetBool() == false && CPanoramaHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGOMoneyPanel::ShowPanel( bool bShow )
{
	SetVisible( bShow );
}

//-----------------------------------------------------------------------------
// Purpose: Reset all transient data
//-----------------------------------------------------------------------------
void CCSGOMoneyPanel::Reset()
{
	// Clear all animations
	m_pAddMoneyLabel->StopAnimation();
	m_pAddMoneyFlashLabel->StopAnimation();
	m_pRemoveMoneyLabel->StopAnimation();

	// Reset state (will update next Update())
	m_nLastMoney = 0;
	m_nTotalAdd = 0;
	m_nTotalSub = 0;
	m_hPlayer = nullptr;

	m_bShowBuyZoneIcon = false;
	SetHasClass( Symbols().InBuyZone, m_bShowBuyZoneIcon );

	UpdateCurrentMoneyText();
}

//-----------------------------------------------------------------------------
// Purpose: Called once per frame to update our data
//-----------------------------------------------------------------------------
bool CCSGOMoneyPanel::EventCSGOFrameUpdate()
{
	if ( BIsVisible() )
		Update();

	return false;
}

void CCSGOMoneyPanel::Update()
{
	CCSGameRules* pGameRules = CSGameRules();
	if ( !pGameRules )
		return;

	C_CSPlayer* pPlayer = ToCSPlayer( CHudElement::GetLocalOrObservedPlayer( 0 ) );
	if ( pPlayer && pPlayer->IsControllingBot() )
		pPlayer = ToCSPlayer( UTIL_PlayerByIndex( pPlayer->GetControlledBotIndex() ) );

	// Get the player's money
	int nMoney = pPlayer ? pPlayer->GetAccount() : 0;

	if ( m_hPlayer.ChangedFrom( pPlayer ) )
	{
		// we changed who we're observing, so just update money amount directly
		Reset();
		m_hPlayer = pPlayer;
		UpdateCurrentMoneyText();
	}
	else if ( m_nLastMoney != nMoney )
	{
		// let's always draw attention to when the player's money has changed

		// if this is the start of the very first round, don't show the change that can happen from the warmup round to the start round
		if ( pGameRules->GetTotalRoundsPlayed() == 0 && pGameRules->GetRoundElapsedTime() < 1 )
		{
			UpdateCurrentMoneyText();
		}
		else
		{
			UpdateMoneyChange( nMoney - m_nLastMoney );
		}
	}

	m_nLastMoney = nMoney;

	bool bShowBuyZoneIcon = CSGameRules()->CanSpendMoneyInMap()
		&& !pGameRules->IsBuyTimeElapsed()
		&& pPlayer
		&& pPlayer->IsInBuyZone();

	if ( m_bShowBuyZoneIcon != bShowBuyZoneIcon )
	{
		SetHasClass( Symbols().InBuyZone, bShowBuyZoneIcon );
		m_bShowBuyZoneIcon = bShowBuyZoneIcon;
	}

	m_pMoneyBG->SetOpacitySimple( cl_hud_background_alpha.GetFloat() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGOMoneyPanel::UpdateMoneyChange( int moneyDelta )
{
	if ( !( m_pAddMoneyLabel && m_pRemoveMoneyLabel && m_pAddMoneyFlashLabel ) )
	{
		UpdateCurrentMoneyText();
		return;
	}
	
	bool bIsPlayingSurvival = CSGameRules()->IsPlayingSurvival();

	if ( moneyDelta > 0 )
	{
		m_nTotalAdd += moneyDelta;

		m_pAddMoneyLabel->SetText( CFmtStr( "+$%d", m_nTotalAdd ) );
		m_pAddMoneyLabel->StartAnimation( Symbols().AddMoney );
		
		m_pAddMoneyFlashLabel->SetText( CFmtStr( "+$%d", m_nTotalAdd ) );
		m_pAddMoneyFlashLabel->StartAnimation( Symbols().AddMoneyFlash );
	}
	else if ( moneyDelta < 0 )
	{
		m_nTotalSub += -moneyDelta; // 'abs'

		m_pRemoveMoneyLabel->SetText( CFmtStr( "-$%d", m_nTotalSub ) );
		m_pRemoveMoneyLabel->StartAnimation( Symbols().RemoveMoney );

		UpdateCurrentMoneyText();
	}
}

void CCSGOMoneyPanel::DoneAnimatingAdd()
{
	m_nTotalAdd = 0;
	UpdateCurrentMoneyText();
}

void CCSGOMoneyPanel::DoneAnimatingSub()
{
	m_nTotalSub = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGOMoneyPanel::UpdateCurrentMoneyText()
{
	C_CSPlayer *pPlayer = m_hPlayer.Get();
	int nMoney = pPlayer ? pPlayer->GetAccount() : 0;
	int nAmount = nMoney - m_nTotalAdd; // added money isn't displayed in text yet
	SetDialogVariable( "hud-money-amount", nAmount );
}
