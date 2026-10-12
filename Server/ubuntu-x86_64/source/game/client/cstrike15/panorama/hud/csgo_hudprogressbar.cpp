//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Progress bar: Display value of local player's m_iProgressBarDuration, 
// set by server for defuse or hostage rescue.
//
//=============================================================================//

#include "cbase.h"
#include "csgo_hudprogressbar.h"
#include "c_cs_player.h"
#include "panorama/uievents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_drawhud;

REGISTER_PANEL2D_FACTORY( CCSGO_HudProgressBar, CSGOHudProgressBar );

CCSGO_HudProgressBar::CCSGO_HudProgressBar( CPanel2D *pParent, const char *pchID )
	: CPanoramaHudElement( pchID, this ),
	CPanel2D( pParent, pchID ),
	m_AnimatingPanels(),
	m_pActionLabel(),
	m_pActionIcon(),
	m_nDuration( 0 ),
	m_flStartTime( 0.0f ),
	m_bAnimationHackOffsetNextAnimation( false )
{
	RequireLoadLayout( "file://{resources}/layout/hud/hudprogressbar.xml" );
	SetupPanels();

	// We need to think every tick to make sure that our timer is perfectly accurate
	SetAllowAlternateTicks( false );
}

CCSGO_HudProgressBar::~CCSGO_HudProgressBar()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudProgressBar::ShouldDraw( void )
{
	if ( cl_draw_only_deathnotices.GetBool() )
		return false;

	return cl_drawhud.GetBool() && CHudElement::ShouldDraw();
}

void CCSGO_HudProgressBar::SetupPanels()
{
	m_pActionIcon = panel_cast< CImagePanel* > ( FindChildInLayoutFile( "ActionIcon" ) );
	m_pActionLabel = panel_cast< CLabel* > ( FindChildInLayoutFile( "ActionLabel" ) );

	CUtlVector<CPanel2D*> animatingPanels;
	FindChildrenWithClassTraverse( "progress-bar-animating", &animatingPanels );

	// Need to copy here since we have CPanelPtr instead of raw pointers
	m_AnimatingPanels.RemoveAll();
	FOR_EACH_VEC( animatingPanels, i )
	{
		m_AnimatingPanels.AddToTail( animatingPanels[i] );
	}
}

void CCSGO_HudProgressBar::OnLayoutReloaded()
{
	BaseClass::OnLayoutReloaded();

	SetupPanels();

	m_nDuration = 0; // force update if currently visible
}

void CCSGO_HudProgressBar::Think( void )
{
	C_CSPlayer* pPlayer = ToCSPlayer( GetLocalOrObservedPlayer() );
	if ( !pPlayer || cl_draw_only_deathnotices.GetBool() == true )
		return;

	float servertime = engine->GetServerTick() * gpGlobals->interval_per_tick;

	bool bPlayerChanged = m_hPlayer.ChangedFrom( pPlayer );
	m_hPlayer = pPlayer;

	// Server sets m_iProgressBarDuration to 0 to hide, and >0 to show. Perform initial display 
	// setup of this panel when we detect a change in that network var.
	if ( bPlayerChanged || pPlayer->m_iProgressBarDuration != m_nDuration
		|| ( pPlayer->m_iProgressBarDuration > 0 && pPlayer->m_flProgressBarStartTime != m_flStartTime ) )
	{
		//SetVisible( pPlayer->m_iProgressBarDuration > 0 ); // controlled by animation now

		static const CPanoramaSymbol k_symHudProgressBarVisible( "hud-progress-bar--visible" );
		SetHasClass( k_symHudProgressBarVisible, pPlayer->m_iProgressBarDuration > 0 );

		// If server has stopped progress, set a class for successful completion or early termination 
		// FIXME: Server decides if we succeeded this action or not... would be better to send that down along with the player vars.
		// However, by using server time here we can be pretty confident that we are correct (modulo update order on the server?)
		static const CPanoramaSymbol k_symHudProgressBarSwitched( "hud-progress-bar--switched" );
		static const CPanoramaSymbol k_symHudProgressBarCanceled( "hud-progress-bar--canceled" );
		static const CPanoramaSymbol k_symHudProgressBarFinished( "hud-progress-bar--finished" );
		static const CPanoramaSymbol k_symHudProgressBarClose( "hud-progress-bar-close" );
		if ( pPlayer->m_iProgressBarDuration <= 0 )
		{
			EPlayerProgressBarReason eReason = pPlayer->GetProgressBarCloseReason();

			bool bComplete = false;

			if ( eReason == k_EPlayerProgressBarReason_Unknown )
			{
				// Old demos.  Guess whether we are complete based on the timing of the event
				bComplete = ( servertime - m_flStartTime ) >= m_nDuration;
			}
			else if ( eReason == k_EPlayerProgressBarReason_Complete )
			{
				// server authoritative completion
				bComplete = true;
			}
			// else we didn't finish

			if ( bPlayerChanged )
				SwitchClass( k_symHudProgressBarClose, k_symHudProgressBarSwitched );
			else if ( bComplete )
				SwitchClass( k_symHudProgressBarClose, k_symHudProgressBarFinished );
			else
				SwitchClass( k_symHudProgressBarClose, k_symHudProgressBarCanceled );

			// Set timer to 0 if we finished
			if ( bComplete && !bPlayerChanged )
			{
				SetDialogVariable( "duration", ( time_t )0 );
				SetDialogVariable( "milliseconds", "000" );
			}

			// HACK: Setting the animation property to the same values doesn't reset the animation.
			// So force animation-delay to slightly change each time we start a progress bar.
			m_bAnimationHackOffsetNextAnimation = !m_bAnimationHackOffsetNextAnimation;
		}
		else
		{
			m_flStartTime = pPlayer->m_flProgressBarStartTime;
			
			SwitchClass( k_symHudProgressBarClose, nullptr );
			SetupDialogFromPlayerAction( pPlayer );
		}

		m_nDuration = pPlayer->m_iProgressBarDuration;

		// Force immediate animation update
		m_flNextAnimationTick = gpGlobals->curtime;
	}

	if ( BIsVisible() && m_nDuration > 0 )
	{
		float flElapsed = servertime - m_flStartTime;

		// cap to 1 tick below completion in case server is 'late' in canceling the bar
		// (probably should fix server code to think more often in this case)
		flElapsed = MIN( flElapsed, m_nDuration - gpGlobals->interval_per_tick );

		if ( gpGlobals->curtime >= m_flNextAnimationTick )
		{
			// TODO: Use realtime?  Or something else that isn't affected by timescale?
			// Right now we use ::Think() to control this which is only on ticks anyways,
			// so I think it's OK to use curtime here until we fix that
			m_flNextAnimationTick = gpGlobals->curtime + 0.5f;

			// animate at correct speed, or 'very slowly' if paused
			float flTimescale = engine->GetTimescale();
			if ( flTimescale < 0.0001 )
				flTimescale = 0.0001;

			// Set animation
			float flDuration = m_nDuration / flTimescale;
			float flProgress = flElapsed / flTimescale;

			FOR_EACH_VEC( m_AnimatingPanels, i )
			{
				panorama::CPanel2D* pPanel = m_AnimatingPanels[i].Get();
				if ( !pPanel )
					continue;

				const char* szAnimationName = pPanel->GetAttribute( "progress-animation", "animation-progress-bar" );

				// REI: for some reason this doesn't work
				//pPanel->SetAnimation( szAnimationName, flDuration, -flProgress, panorama::k_EAnimationLinear, panorama::k_EAnimationDirectionNormal, panorama::k_EAnimationFillModeBoth, 1 );

				// HACK: Make an unnoticable change to the animation state because panorama doesn't reset the animation when we set the style property to something that exactly matches the previous value.
				//       .0002 is chosen because we use %.4f in our format string below and this guarantees a difference even with whatever rounding might happen.
				if ( m_bAnimationHackOffsetNextAnimation )
					flDuration += 0.0002f;

				IUIPanelClient* pPanelClient = pPanel;
				pPanelClient->BSetProperty( "style", CFmtStr( "animation-name: %s; animation-duration: %.4fs; animation-delay: %.4fs; animation-timing-function: linear; animation-fill-mode: both;", szAnimationName, flDuration, -flProgress ) );
			}
		}

		// Set text
		float flRemaining = m_nDuration - flElapsed;
		int nSeconds = Floor2Int( flRemaining );
		int nMilliseconds = Floor2Int( ( flRemaining - nSeconds ) * 1000.0f );
		SetDialogVariable( "duration", ( time_t )nSeconds );
		SetDialogVariable( "milliseconds", CFmtStr( "%03d", nMilliseconds ) );
	}
}

// As with scaleform, we try to figure out why this dialog is being shown by looking at network state on the local or observed player
// It may be better to have the server network an explicit reason for showing the progress bar along with the duration, but for now 
// we just figure out what the player is doing the same way sfhudinfopanel did.
bool CCSGO_HudProgressBar::SetupDialogFromPlayerAction( C_CSPlayer* pPlayer ) 
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	bool bIsSpectating = ( pLocalPlayer != pPlayer );

	const char* szIconPath = nullptr;
	const char* szActionDescription = nullptr;

	static const struct HudProgressBarDataForBlockingActions_t
	{
		CSPlayerBlockingUseAction_t m_eAction;
		CPanoramaSymbol m_symStyle;
		char const *m_szIconPath;
		char const *m_szActionDescriptionSpec;
		char const *m_szActionDescription;
	}
	k_progressbardata[ k_CSPlayerBlockingUseAction_MaxCount ] =
	{
		{ k_CSPlayerBlockingUseAction_None, "", "", "" },
		{ k_CSPlayerBlockingUseAction_DefusingDefault,		"hud-progress-bar__defuse-nokit",		"file://{images}/icons/equipment/c4.svg",		"#SFUIHUD_InfoPanel_Spec_DefuseText_NoKit",		"#SFUIHUD_InfoPanel_DefuseText_NoKit"		},
		{ k_CSPlayerBlockingUseAction_DefusingWithKit,		"hud-progress-bar__defuse",				"file://{images}/icons/equipment/defuser.svg",	"#SFUIHUD_InfoPanel_Spec_DefuseText",			"#SFUIHUD_InfoPanel_DefuseText"			},
		{ k_CSPlayerBlockingUseAction_HostageGrabbing,		"hud-progress-bar__pickup-hostage",		"file://{images}/icons/ui/hostage_transit.svg",	"#SFUIHUD_InfoPanel_Spec_HostageText",			"#SFUIHUD_InfoPanel_HostageText"			},
		{ k_CSPlayerBlockingUseAction_HostageDropping,		"hud-progress-bar__drop-hostage",		"file://{images}/icons/ui/hostage_transit.svg",	"#SFUIHUD_InfoPanel_Spec_HostageDropping",		"#SFUIHUD_InfoPanel_HostageDropping"			},
		{ k_CSPlayerBlockingUseAction_OpeningSafe,			"hud-progress-bar__open-survival-safe",	"file://{images}/icons/ui/survival_safe.svg",	"#SFUIHUD_InfoPanel_Spec_OpeningSafe",			"#SFUIHUD_InfoPanel_OpeningSafe"			},
		{ k_CSPlayerBlockingUseAction_EquippingParachute,	"hud-progress-bar__pickup-hostage",		"file://{images}/icons/ui/parachute.svg",	"#SFUIHUD_InfoPanel_Spec_EquippingParachute",	"#SFUIHUD_InfoPanel_EquippingParachute"			},
		{ k_CSPlayerBlockingUseAction_EquippingHeavyArmor,	"hud-progress-bar__drop-heavyarmor",	"file://{images}/icons/equipment/heavy_armor.svg",		"#SFUIHUD_InfoPanel_Spec_EquippingHeavyArmor",	"#SFUIHUD_InfoPanel_EquippingHeavyArmor"			},
		{ k_CSPlayerBlockingUseAction_EquippingContract,	"hud-progress-bar__pickup-hostage",		"file://{images}/icons/ui/dominated.svg",		"#SFUIHUD_InfoPanel_Spec_EquippingContract",	"#SFUIHUD_InfoPanel_EquippingContract"			},
		{ k_CSPlayerBlockingUseAction_EquippingTabletUpgrade, "hud-progress-bar__pickup-hostage",	"file://{images}/icons/ui/save.svg",			"#SFUIHUD_InfoPanel_Spec_EquippingTabletUpgrade", "#SFUIHUD_InfoPanel_EquippingTabletUpgrade"			},
		{ k_CSPlayerBlockingUseAction_TakingOffHeavyArmor,	"hud-progress-bar__drop-heavyarmor",	"file://{images}/icons/equipment/heavy_armor.svg",		"#SFUIHUD_InfoPanel_Spec_TakingOffHeavyArmor",	"#SFUIHUD_InfoPanel_TakingOffHeavyArmor"			},
		{ k_CSPlayerBlockingUseAction_PayingToOpenDoor,		"hud-progress-bar__pickup-hostage",		"file://{images}/icons/ui/dollar_sign.svg",		"#SFUIHUD_InfoPanel_Spec_OpeningSecurityDoor",	"#SFUIHUD_InfoPanel_OpeningSecurityDoor"			},
		{ k_CSPlayerBlockingUseAction_CancelingSpawnRappelling,		"hud-progress-bar__defuse",		"file://{images}/icons/equipment/fists.svg",	"#SFUIHUD_InfoPanel_Spec_CuttingRappelRope",	"#SFUIHUD_InfoPanel_CuttingRappelRope" },
	};
	COMPILE_TIME_ASSERT( ARRAYSIZE( k_progressbardata ) == k_CSPlayerBlockingUseAction_MaxCount );
#if DEVELOPMENT_ONLY
	for ( int jj = 0; jj < k_CSPlayerBlockingUseAction_MaxCount; ++ jj )
	{
		AssertFatal( k_progressbardata[jj].m_eAction == jj );
	}
#endif

	// Reset all styles to "off"
	for ( int i = 1; i < ARRAYSIZE( k_progressbardata ); ++ i )
	{
		SetHasClass( k_progressbardata[i].m_symStyle, false );
	}

	// Set player name in case we show spec string
	SetDialogVariable( "s1", pPlayer->GetPlayerName() );

	// Determine which action we will render
	CSPlayerBlockingUseAction_t eAction = k_CSPlayerBlockingUseAction_None;
	if ( pPlayer->m_bIsDefusing )
	{	// Legacy handling of bomb defuse actions
		eAction = pPlayer->HasDefuser() ? k_CSPlayerBlockingUseAction_DefusingWithKit : k_CSPlayerBlockingUseAction_DefusingDefault;
	}
	else if ( pPlayer->m_bIsGrabbingHostage )
	{	// Legacy handling of picking up a hostage
		eAction = k_CSPlayerBlockingUseAction_HostageGrabbing;
	}
	else if ( pPlayer->m_iBlockingUseActionInProgress )
	{	// New extensible blocking use actions
		eAction = ( CSPlayerBlockingUseAction_t ) pPlayer->m_iBlockingUseActionInProgress;
	}

	// Check that the action is in valid range
	if ( eAction < 0 || eAction >= k_CSPlayerBlockingUseAction_MaxCount )
		return false;

	SetHasClass( k_progressbardata[eAction].m_symStyle, true );
	szIconPath = k_progressbardata[eAction].m_szIconPath;
	szActionDescription = bIsSpectating ? k_progressbardata[eAction].m_szActionDescriptionSpec : k_progressbardata[ eAction ].m_szActionDescription;

	if ( m_pActionIcon.Get() && szIconPath )
		m_pActionIcon->SetImageJS( szIconPath );

	if ( m_pActionLabel.Get() && szActionDescription )
	{
		CFmtStr fmtStrContainer;
		if ( ( eAction == k_CSPlayerBlockingUseAction_HostageGrabbing ) && CSGameRules() && CSGameRules()->IsPlayingSurvival() )
		{
			fmtStrContainer.Format( "%s_Survival", szActionDescription );
			szActionDescription = fmtStrContainer.Access();
		}
		m_pActionLabel->SetText( szActionDescription );
	}

	return true;
}
