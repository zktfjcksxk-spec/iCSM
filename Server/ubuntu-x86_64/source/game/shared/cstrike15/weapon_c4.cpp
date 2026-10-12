//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_c4.h"
#include "in_buttons.h"
#include "cs_gamerules.h"
#include "decals.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "keyvalues.h"
#include "fx_cs_shared.h"
#include "obstacle_pushaway.h"
#include "particle_parse.h"
#include "mathlib/vector.h"

#if defined( CLIENT_DLL )
	#include "c_cs_player.h"

#ifdef INCLUDE_SCALEFORM
	#include "HUD/sfweaponselection.h"
#endif	// INCLUDE_SCALEFORM

	#include "gameui_interface.h"
#else
	#include "cs_player.h"
	#include "explode.h"
	#include "mapinfo.h"
	#include "team.h"
	#include "func_bomb_target.h"
	#include "vguiscreen.h"
	#include "bot.h"
	#include "cs_player.h"

	#include "cs_gamestats.h"
	#include "cs_achievement_constants.h"

	#include "cvisibilitymonitor.h"
	#include "cs_entity_spotting.h"

	#include "item_cash.h"
	#include "weapon_tablet.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define BLINK_INTERVAL 2.0
#define PLANTED_C4_MODEL "models/weapons/w_ied_dropped.mdl"
#define HEIST_MODE_C4_TIME 25

#define WEAPON_C4_ARM_TIME	3.0

ConVar mp_plant_c4_anywhere(
	"mp_plant_c4_anywhere",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"" );

DEVELOPMENT_ONLY_CONVAR( dev_test_close_defuse, -100 );

#ifndef CLIENT_DLL

#define WEAPON_C4_UPDATE_LAST_VALID_PLAYER_HELD_POSITION_INTERVAL	0.2

// amount of time a player can stop defusing and continue
const float C4_DEFUSE_GRACE_PERIOD = 0.5f;

// amount of time a player is forced to continue defusing after not USEing. this effects other player's ability to interrupt
const float C4_DEFUSE_LOCKIN_PERIOD = 0.05f;	

extern ConVar mp_anyone_can_pickup_c4;
extern ConVar mp_c4_cannot_be_defused;

CON_COMMAND_F( clear_bombs, "", FCVAR_CHEAT )
{
	FOR_EACH_VEC( g_PlantedC4s, iBomb )
	{
		CPlantedC4 * pC4 = g_PlantedC4s[iBomb];
		if ( pC4 )
		{
			UTIL_Remove( pC4 );
		}
	}
	g_PlantedC4s.RemoveAll();
}

LINK_ENTITY_TO_CLASS( planted_c4, CPlantedC4 );
PRECACHE_REGISTER( planted_c4 );

BEGIN_DATADESC( CPlantedC4 )
	DEFINE_FUNCTION( C4Think ),
	//Outputs
	DEFINE_OUTPUT( m_OnBombBeginDefuse, "OnBombBeginDefuse" ),
	DEFINE_OUTPUT( m_OnBombDefused, "OnBombDefused" ),
	DEFINE_OUTPUT( m_OnBombDefuseAborted, "OnBombDefuseAborted" ),
END_DATADESC()
	

IMPLEMENT_SERVERCLASS_ST( CPlantedC4, DT_PlantedC4 )
	SendPropBool( SENDINFO(m_bBombTicking) ),
	SendPropInt( SENDINFO(m_nBombSite)),
	SendPropFloat( SENDINFO(m_flC4Blow), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flTimerLength), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flDefuseLength), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flDefuseCountDown), 0, SPROP_NOSCALE ),
	SendPropBool( SENDINFO(m_bBombDefused) ),
	SendPropEHandle( SENDINFO(m_hBombDefuser) ),
END_SEND_TABLE()

	
BEGIN_PREDICTION_DATA( CPlantedC4 )
END_PREDICTION_DATA()



	CUtlVector< CPlantedC4* > g_PlantedC4s;


	CPlantedC4::CPlantedC4()
	{
		g_PlantedC4s.AddToTail( this );

		// [tj] No planter initially
		m_pPlanter = NULL;
		m_pBombDefuser = NULL; //No Defuser Initially

		// [tj] Assume this is the original owner
		m_bPlantedAfterPickup = false;

		m_bTrainingPlacedByPlayer = false;
		m_bVoiceAlertFired = false;

		for ( int i = 0; i < V_ARRAYSIZE( m_bVoiceAlertPlayed ); ++i )
		{
			m_bVoiceAlertPlayed[i] = false;
		}

		m_bPlantedAtQuestTarget = false;

		SetSpotRules( CCSEntitySpotting::SPOT_RULE_CT | CCSEntitySpotting::SPOT_RULE_ALWAYS_SEEN_BY_T );
	}

	CPlantedC4::~CPlantedC4()
	{
		g_PlantedC4s.FindAndRemove( this );

		RemoveControlPanels();
	}

	void CPlantedC4::Spawn()
	{
		BaseClass::Spawn();

		SetMoveType( MOVETYPE_NONE );
		SetSolid( SOLID_NONE );
		AddFlag( FL_OBJECT );

		SetModel( PLANTED_C4_MODEL );
		SetSequence(1);	// this sequence keeps the toggle switch in the 'up' position

		SetHolidayBodyGroup( this );

		SetCollisionBounds( Vector( 0, 0, 0 ), Vector( 8, 8, 8 ) );

		// Detonate in "time" seconds
		SetThink( &CPlantedC4::C4Think );

		SetNextThink( gpGlobals->curtime + 0.1f );
		m_flNextBotBeepTime = gpGlobals->curtime;

		if ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() && mp_anyone_can_pickup_c4.GetBool() )
			m_flTimerLength = 9999;
		else
			m_flTimerLength = mp_c4timer.GetInt();

		m_flC4Blow = gpGlobals->curtime + m_flTimerLength;
		m_fLastDefuseTime = 0.0f;
		m_bBeingDefused = false;
		m_bHasExploded = false;
		m_bBombDefused = false;

		m_bCannotBeDefused = false;

		SetFriction( 0.9 );

		m_flDefuseLength = 0.0f;

		SpawnControlPanels();

		VisibilityMonitor_AddEntity( this, 600.0f, NULL, NULL );
	}

	int CPlantedC4::UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_FULLCHECK );
	}

	int CPlantedC4::ShouldTransmit( const CCheckTransmitInfo *pInfo )
	{
		// Terrorists always need this object for the radar
		// Everybody needs it for hiding the round timer and showing the planted C4 scenario icon
		return FL_EDICT_ALWAYS;
	}

	void CPlantedC4::Precache()
	{
		PrecacheModel( "models/weapons/w_c4_planted.mdl" ); // old, unused
		PrecacheModel( PLANTED_C4_MODEL );
		PrecacheModel( "models/props/de_overpass/balloon.mdl" );
		PrecacheParticleSystem( "weapon_confetti_balloons" );
		PrecacheModel( "models/weapons/w_eq_multimeter.mdl" );
		PrecacheVGuiScreen( "c4_panel" );
	}

	void CPlantedC4::SetHolidayBodyGroup( CBaseAnimating* pBomb )
	{
		if ( !pBomb )
			return;

		if ( CSGameRules()->IsPlayingSurvival() )
			return;
	
//		if ( !CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && EconHolidays_IsHolidayActive( kHoliday_Christmas ) )
//			pBomb->SetBodygroupPreset( "show_gift" );
	}

	void CPlantedC4::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
	{
		pPanelName = "c4_panel";
	}

	void CPlantedC4::GetControlPanelClassName( int nPanelIndex, const char *&pPanelName )
	{
		pPanelName = "vgui_screen";
	}

	//-----------------------------------------------------------------------------
	// This is called by the base object when it's time to spawn the control panels
	//-----------------------------------------------------------------------------
	void CPlantedC4::SpawnControlPanels()
	{
		char buf[64];

		// FIXME: Deal with dynamically resizing control panels?

		// If we're attached to an entity, spawn control panels on it instead of use
		CBaseAnimating *pEntityToSpawnOn = this;
		char *pOrgLL = "controlpanel%d_ll";
		char *pOrgUR = "controlpanel%d_ur";
		char *pAttachmentNameLL = pOrgLL;
		char *pAttachmentNameUR = pOrgUR;

		Assert( pEntityToSpawnOn );

		// Lookup the attachment point...
		int nPanel;
		for ( nPanel = 0; true; ++nPanel )
		{
			Q_snprintf( buf, sizeof( buf ), pAttachmentNameLL, nPanel );
			int nLLAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
			if (nLLAttachmentIndex <= 0)
			{
				// Try and use my panels then
				pEntityToSpawnOn = this;
				Q_snprintf( buf, sizeof( buf ), pOrgLL, nPanel );
				nLLAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
				if (nLLAttachmentIndex <= 0)
					return;
			}

			Q_snprintf( buf, sizeof( buf ), pAttachmentNameUR, nPanel );
			int nURAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
			if (nURAttachmentIndex <= 0)
			{
				// Try and use my panels then
				Q_snprintf( buf, sizeof( buf ), pOrgUR, nPanel );
				nURAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
				if (nURAttachmentIndex <= 0)
					return;
			}

			const char *pScreenName;
			GetControlPanelInfo( nPanel, pScreenName );
			if (!pScreenName)
				continue;

			const char *pScreenClassname;
			GetControlPanelClassName( nPanel, pScreenClassname );
			if ( !pScreenClassname )
				continue;

			// Compute the screen size from the attachment points...
			matrix3x4_t	panelToWorld;
			pEntityToSpawnOn->GetAttachment( nLLAttachmentIndex, panelToWorld );

			matrix3x4_t	worldToPanel;
			MatrixInvert( panelToWorld, worldToPanel );

			// Now get the lower right position + transform into panel space
			Vector lr, lrlocal;
			pEntityToSpawnOn->GetAttachment( nURAttachmentIndex, panelToWorld );
			MatrixGetColumn( panelToWorld, 3, lr );
			VectorTransform( lr, worldToPanel, lrlocal );

			float flWidth = fabs( lrlocal.x );
			float flHeight = fabs( lrlocal.y );

			CVGuiScreen *pScreen = CreateVGuiScreen( pScreenClassname, pScreenName, pEntityToSpawnOn, this, nLLAttachmentIndex );
			pScreen->ChangeTeam( GetTeamNumber() );
			pScreen->SetActualSize( flWidth, flHeight );
			pScreen->SetActive( true );
			pScreen->MakeVisibleOnlyToTeammates( false );
			int nScreen = m_hScreens.AddToTail( );
			m_hScreens[nScreen].Set( pScreen );			
		}
	}

	void CPlantedC4::RemoveControlPanels()
	{
		// Clear off any screens that are still live.
		for ( int ii = m_hScreens.Count(); --ii >= 0; )
		{
			DestroyVGuiScreen( m_hScreens[ii].Get() );
		}
		m_hScreens.RemoveAll();
	}

	void CPlantedC4::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
	{
		// Are we already marked for transmission?
		if ( pInfo->m_pTransmitEdict->Get( entindex() ) )
			return;

		BaseClass::SetTransmit( pInfo, bAlways );

		// Force our screens to be sent too.
		for ( int i=0; i < m_hScreens.Count(); i++ )
		{
			CVGuiScreen *pScreen = m_hScreens[i].Get();
			pScreen->SetTransmit( pInfo, bAlways );
		}
	}

	CPlantedC4* CPlantedC4::ShootSatchelCharge( CCSPlayer *pevOwner, Vector vecStart, QAngle vecAngles )
	{
		CPlantedC4 *pGrenade;
		bool bTrainingPlacedByPlayer = false;
		if ( CSGameRules()->IsPlayingTraining() )
		{
			pGrenade = dynamic_cast< CPlantedC4* >( CreateEntityByName( "planted_c4_training" ) );
			bTrainingPlacedByPlayer = true;
		}
		else if ( CSGameRules()->IsPlayingSurvival() )
		{
			pGrenade = dynamic_cast< CPlantedC4* >( CreateEntityByName( "planted_c4_survival" ) );
		}
		else // classic mode
		{
			pGrenade = dynamic_cast< CPlantedC4* >( CreateEntityByName( "planted_c4" ) );
		}

		if ( pGrenade )
		{
			vecAngles[0] = 0;
			vecAngles[2] = 0;
			pGrenade->Init( pevOwner, vecStart, vecAngles, bTrainingPlacedByPlayer );
			return pGrenade;
		}
		else
		{
			Warning( "Can't create planted_c4 entity!\n" );
			return NULL;
		}
	}


	void CPlantedC4::Init( CCSPlayer *pevOwner, Vector vecStart, QAngle vecAngles, bool bTrainingPlacedByPlayer )
	{
		SetAbsOrigin( vecStart );
		SetAbsAngles( vecAngles );
		SetOwnerEntity( pevOwner );
		SetPlanter( pevOwner );

		if( pevOwner != nullptr )
		{
			m_nBombSite = pevOwner->m_iBombSiteIndex;
			if ( UTIL_EntityByIndex(m_nBombSite) )
			{
				CBombTarget* pBombTarget = dynamic_cast<CBombTarget*>(UTIL_EntityByIndex(m_nBombSite));
				if ( pBombTarget != nullptr ) {
					m_nBombSite = pBombTarget->IsBombSiteA() ? 0 : 1;
				}
			}
		}
		else
		{
			m_nBombSite = 0;
		}

		m_bTrainingPlacedByPlayer = bTrainingPlacedByPlayer;
		if ( !m_bTrainingPlacedByPlayer )
			m_bBombTicking = true;

		Spawn();
	}


	void CPlantedC4::C4Think()
	{
		if ( !IsInWorld() )
		{
			UTIL_Remove( this );
			return;
		}

		// network the defuser handle
		if ( m_bBeingDefused )
		{
			m_hBombDefuser = m_pBombDefuser;
			if ( GetModelPtr() )
				SetBodygroupPreset( "show_clips" );
		}
		else
		{
			m_hBombDefuser = INVALID_EHANDLE;
			if ( GetModelPtr() )
				SetBodygroupPreset( "hide_clips" );
		}

		if ( m_bHasExploded )
		{
			Assert( !m_bBeingDefused );
			return;
		}

		//Bomb is not ticking, don't think anymore
		if ( !IsBombActive() )
		{
			SetThink( NULL );
			return;
		}

		// Bomb thinks every tick if about to be defused/explode, otherwise rarely
		float nextThinkTime = gpGlobals->curtime + 0.25f;
		if ( nextThinkTime >= m_flC4Blow || ( m_bBeingDefused && nextThinkTime >= m_flDefuseCountDown ) )
			nextThinkTime = gpGlobals->curtime;
		SetNextThink( nextThinkTime );

		// let the bots hear the bomb beeping
		// BOTPORT: Emit beep events at same time as client effects
		if ( gpGlobals->curtime >= m_flNextBotBeepTime )
		{
			m_flNextBotBeepTime = gpGlobals->curtime + 0.5f;

			IGameEvent * event_beep = gameeventmanager->CreateEvent( "bomb_beep" );
			if ( event_beep )
			{
				event_beep->SetInt( "entindex", entindex() );
				gameeventmanager->FireEvent( event_beep, true ); // server-side event only
			}
		}

		// 4 seconds before the bomb blows up, have anyone close by on each team say something about it going to blow
		if ( m_flC4Blow - 3.0 <= gpGlobals->curtime && !m_bVoiceAlertFired )
		{
			m_bVoiceAlertFired = true;

			// CTs don't say anything if they know they are going to defuse the bomb.
			//
			// For 'close' failed defuses (< 0.5 s) we don't have them radio until
			// they actually fail to defuse, to add tension.
			if ( !m_bBeingDefused || ( m_flDefuseCountDown >= m_flC4Blow + 0.5f ) )
				RadioAboutToExplode( TEAM_CT );

			RadioAboutToExplode( TEAM_TERRORIST );
		}

		// If the defuser doesn't exist (maybe they disconnected?), cancel the defuse.
		// $$$REI Note that this doesn't trigger a cancellation event... what should happen here?
		if ( m_bBeingDefused && ( m_pBombDefuser == NULL ) )
		{
			m_bBeingDefused = false;
		}

		// Defuse cancellation logic
		if ( m_bBeingDefused )
		{
			Assert( m_pBombDefuser != NULL ); // should have been handled above

			// Can't defuse the bomb if it's not defusable
			bool bUndefusable = mp_c4_cannot_be_defused.GetBool();

			// Can't defuse the bomb if dead
			bool bAlive = m_pBombDefuser->IsAlive();

			// Can't defuse if the T's already won the round (not sure how this could happen?)
			bool bRoundWonByTs = ( CSGameRules()->m_iRoundWinStatus == WINNER_TER );

			// Can only defuse the bomb while standing on the ground
			bool bOnGround = FBitSet( m_pBombDefuser->GetFlags(), FL_ONGROUND ) != 0;

			// Can't defuse if you stopped holding use.  We latch the use key for a bit here; you can't instantly let go.
			const CUserCmd *pCmd = m_pBombDefuser->GetLastUserCommand();
			bool bPlayerStoppedHoldingUse = !( pCmd->buttons & IN_USE ) && ( gpGlobals->curtime > m_fLastDefuseTime + C4_DEFUSE_LOCKIN_PERIOD );

			// Can't defuse if your use would target another entity. (Also not sure how this could happen)
			CConfigurationForHighPriorityUseEntity_t cfgUseEntity;
			bool bPlayerUseIsValidNow = m_pBombDefuser->GetUseConfigurationForHighPriorityUseEntity( this, cfgUseEntity ) &&
				( cfgUseEntity.m_pEntity == this ) && cfgUseEntity.UseByPlayerNow( m_pBombDefuser, cfgUseEntity.k_EPlayerUseType_Progress );

			// Handle all cases that lead to an aborted defuse
			if ( bUndefusable || !bAlive || bRoundWonByTs || !bOnGround || bPlayerStoppedHoldingUse || !bPlayerUseIsValidNow )
			{
				if ( bAlive && !bOnGround )
					ClientPrint( m_pBombDefuser, HUD_PRINTCENTER, "#SFUI_Notice_C4_Defuse_Must_Be_On_Ground" );

				// Bomb defuse cancelled
				m_OnBombDefuseAborted.FireOutput( this, m_pBombDefuser );

				// tell the bots someone has aborted defusing
				IGameEvent * event_abort = gameeventmanager->CreateEvent( "bomb_abortdefuse" );
				if ( event_abort )
				{
					event_abort->SetInt( "userid", m_pBombDefuser->GetUserID() );
					event_abort->SetInt( "priority", 5 ); // bomb_abortdefuse
					gameeventmanager->FireEvent( event_abort );
				}

				//cancel the progress bar
				m_pBombDefuser->CloseProgressBar( k_EPlayerProgressBarReason_Incomplete );
				m_pBombDefuser->OnCanceledDefuse();

				// release the player from being frozen
				m_pBombDefuser->m_bIsDefusing = false;
				m_bBeingDefused = false;
				m_pBombDefuser = NULL;
			}
		}

		// Handle bomb defuse/explode
		// We fall through to the below case if m_flC4Blow < m_flDefuseCountdown (the bomb should have blown up already)
		if ( m_bBeingDefused && gpGlobals->curtime >= m_flDefuseCountDown && m_flDefuseCountDown <= m_flC4Blow  )
		{
			Assert( m_pBombDefuser != NULL ); // see above, should still be set

			m_pBombDefuser->m_bIsDefusing = false;
			m_pBombDefuser->CloseProgressBar( k_EPlayerProgressBarReason_Complete );
			m_bBeingDefused = false;
			m_bBombTicking = false;
			m_bBombDefused = true;

			Vector soundPosition = m_pBombDefuser->GetAbsOrigin() + Vector( 0, 0, 5 );
			CPASAttenuationFilter filter( soundPosition );
			EmitSound( filter, 0, "c4.disarmfinish", &GetAbsOrigin() );

			// mode-specific logic goes in here
			OnDefuse( m_pBombDefuser );
		}
		else if ( gpGlobals->curtime >= m_flC4Blow )
		{
			// The timer has expired ! blow this bomb up!

			if ( m_bBeingDefused )
			{
				Assert( m_pBombDefuser != NULL );

				// let CTs yell about the bomb now if they haven't already
				// (for close defuses they don't yell until they fail to defuse)
				RadioAboutToExplode( TEAM_CT );

				// kick off anyone trying to defuse the bomb
				m_pBombDefuser->m_bIsDefusing = false;
				m_pBombDefuser->CloseProgressBar( k_EPlayerProgressBarReason_Incomplete );
				m_pBombDefuser->OnCanceledDefuse();
				m_bBeingDefused = false;
				m_pBombDefuser = NULL;
			}

			// for the music, we added a tension filled second after the bomb has ceased to be defusable and explode 1 second after
			if ( m_flC4Blow + 1.0f <= gpGlobals->curtime )
			{
				Explode();
			}
		}
	}

	void CPlantedC4::RadioAboutToExplode( int teamNumber )
	{
		if ( m_bVoiceAlertPlayed[teamNumber] )
			return;

		CCSPlayer *pPlayer = NULL;

		// If someone is defusing the bomb on CT side, they are the best person to tell everyone it's going to fail.
		if ( teamNumber == TEAM_CT && m_pBombDefuser && m_pBombDefuser->IsAlive() && m_pBombDefuser->GetTeamNumber() == TEAM_CT )
		{
			pPlayer = m_pBombDefuser;
		}

		Vector origin = GetAbsOrigin();

		for ( int i = 1; pPlayer == NULL && i <= MAX_PLAYERS; i++ )
		{
			CCSPlayer *pTestPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
			if ( !pTestPlayer )
				continue;
			if ( !pTestPlayer->IsAlive() )
				continue;
			if ( pTestPlayer->GetTeamNumber() != teamNumber )
				continue;
			if ( ( origin - pTestPlayer->GetAbsOrigin() ).AsVector2D().IsLengthGreaterThan( 1200.0 ) )
				continue;

			pPlayer = pTestPlayer;
		}

		if ( pPlayer )
		{
			m_bVoiceAlertPlayed[teamNumber] = true;
			pPlayer->Radio( "Radio.GetOutOfThere", "", true );
		}
	}

	void CPlantedC4::SetBombTarget( CBombTarget* pBombTarget )
	{
		m_pBombSite = pBombTarget;
	}

	CBombTarget* CPlantedC4::GetBombTarget()
	{
		return m_pBombSite;
	}

	void CPlantedC4::OnDefuse( CCSPlayer *pDefuser ) // "Classic" mode only, other modes can and do override this behavior.
	{
		// The defuse process has ended; kill the C4.
		bool roundWasAlreadyWon = ( CSGameRules()->m_iRoundWinStatus != WINNER_NONE );	// This condition checks whether CTs already won the round
																						// Check if there are Terrorists alive
		bool bTerroristsAlive = false;
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CCSPlayer* pCheckPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( i );
			if ( !pCheckPlayer )
				continue;
			if ( pCheckPlayer->GetTeamNumber() != TEAM_TERRORIST )
				continue;
			if ( pCheckPlayer->IsAlive() )
			{
				bTerroristsAlive = true;
				break;
			}
		}

		// set down-to-the-wire defuse fun fact
		pDefuser->SetDefusedBombWithThisTimeRemaining( m_flC4Blow - gpGlobals->curtime );

		CCS_GameStats.Event_BombDefused( pDefuser );
		CSGameRules()->ScoreBombDefuse( pDefuser, ( bTerroristsAlive && !roundWasAlreadyWon ) );
		pDefuser->AddAccountAward( PlayerCashAward::BOMB_DEFUSED );

		int iCurrentRound = CSGameRules()->GetCurrentRoundIndexForMatchStats();

		// record in matchstats
		if ( CSGameRules()->ShouldRecordMatchStats( iCurrentRound ) )
		{
			
			++pDefuser->m_iMatchStats_Objective.GetForModify( iCurrentRound );

			// Keep track of Match stats in QMM data
			if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( pDefuser->GetHumanPlayerAccountID() ) )
			{
				pQMM->m_iMatchStats_Objective[iCurrentRound] = pDefuser->m_iMatchStats_Objective.Get( iCurrentRound );
			}
		}

		++pDefuser->m_iMatchStats_Objective_Total;

		// Keep track of Match stats in QMM data
		if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( pDefuser->GetHumanPlayerAccountID() ) )
		{
			pQMM->m_iMatchStats_Objective_Total = pDefuser->m_iMatchStats_Objective_Total;
		}



		if ( !roundWasAlreadyWon )
		{
			// All alive CTs also get assistance credit for bomb defuse.
			// This way if all Terrorists are eliminated then all alive CTs get 1pt and defuser gets 2pt;
			// if a Terrorist remains alive then the defuser gets 5 pts and all other alive
			// teammates get 1 pt for assist / suppressing fire.
			for ( int i = 1; i <= MAX_PLAYERS; i++ )
			{
				CCSPlayer* pCheckPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( i );
				if ( !pCheckPlayer )
					continue;
				if ( pCheckPlayer->GetTeamNumber() != TEAM_CT )
					continue;
				if ( pCheckPlayer->IsAlive() )
					CSGameRules()->ScoreBombDefuse( pCheckPlayer, false );
			}
		}

		IGameEvent * event_defused = gameeventmanager->CreateEvent( "bomb_defused" );
		if ( event_defused )
		{
			CBombTarget *pBombTarget = GetBombTarget();
			event_defused->SetInt( "userid", pDefuser->GetUserID() );
			event_defused->SetInt( "site", pBombTarget ? pBombTarget->entindex() : -1 );
			event_defused->SetInt( "priority", 5 ); // bomb_defused
			gameeventmanager->FireEvent( event_defused );

			pDefuser->AwardAchievement( CSWinBombDefuse );

			float timeToDetonation = ( m_flC4Blow - gpGlobals->curtime );

			if ( ( timeToDetonation > 0.0f ) && ( timeToDetonation <= AchievementConsts::BombDefuseCloseCall_MaxTimeRemaining ) )
			{
				// Give achievement for defusing with < 1 second before detonation
				pDefuser->AwardAchievement( CSBombDefuseCloseCall );
			}

			// [dwenger] Added for fun-fact support
			if ( pDefuser->PickedUpDefuser() )
			{
				// Defuser kit was picked up, so set the fun fact
				pDefuser->SetDefusedWithPickedUpKit( true );
			}
		}

		Vector soundPosition = pDefuser->GetAbsOrigin() + Vector( 0, 0, 5 );
		CPASAttenuationFilter filter( soundPosition );
		EmitSound( filter, 0, "c4.disarmfinish", &GetAbsOrigin() );

		// The bomb has just been disarmed.. Check to see if the round should end now
		m_bBombTicking = false;

		// release the player from being frozen
		pDefuser->m_bIsDefusing = false;

		if ( CSGameRules() && CSGameRules()->IsCSGOBirthday() )
		{
			DispatchParticleEffect( "weapon_confetti_balloons", GetAbsOrigin(), QAngle( 0, 0, 0 ) );
			CPASAttenuationFilter filter_confetti( this );
			filter_confetti.UsePredictionRules();
			EmitSound( filter_confetti, entindex(), "Weapon_PartyHorn.Single" );
		}

		// Setup MVP granting class in case round wasn't already won
		class CPlantedC4DefusedMVP : public CCSGameRules::ICalculateEndOfRoundMVPHook_t
		{
		public:
			virtual CCSPlayer* CalculateEndOfRoundMVP() OVERRIDE
			{
				if ( m_pBombDefuser->HasControlledBotThisRound() )
				{
					// [dkorus] if we controlled a bot this round, use standard MVP conditions
					return CSGameRules()->CalculateEndOfRoundMVP();
				}

				bool bTerroristsAlive = false;
				for ( int i = 1; i <= MAX_PLAYERS; i++ )
				{
					CCSPlayer* pCheckPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( i );
					if ( !pCheckPlayer )
						continue;
					if ( pCheckPlayer->GetTeamNumber() != TEAM_TERRORIST )
						continue;
					if ( pCheckPlayer->IsAlive() )
					{
						bTerroristsAlive = true;
						break;
					}
				}

				if ( bTerroristsAlive || ( m_pBombDefuser->GetNumRoundKills() && !m_pBombDefuser->m_iNumRoundTKs ) )
				{
					m_pBombDefuser->IncrementNumMVPs( CSMVP_BOMBDEFUSE );
					return m_pBombDefuser;
				}

				if ( CCSPlayer *pDefaultMvp = CSGameRules()->CalculateEndOfRoundMVP() )
					return pDefaultMvp;

				m_pBombDefuser->IncrementNumMVPs( CSMVP_BOMBDEFUSE );
				return m_pBombDefuser;
			}
			CHandle<CCSPlayer> m_pBombDefuser;
		} mvpHook;
		mvpHook.m_pBombDefuser = pDefuser;

		if ( !roundWasAlreadyWon )
			CSGameRules()->m_pfnCalculateEndOfRoundMVPHook = &mvpHook;

		// [menglish] Give the bomb defuser an mvp if they ended the round
		CSGameRules()->CheckWinConditions();

		// Reset the MVP hook
		if ( !roundWasAlreadyWon )
			CSGameRules()->m_pfnCalculateEndOfRoundMVPHook = NULL;

		CSGameRules()->m_bBombDropped = false;
		CSGameRules()->m_bBombPlanted = false;
	}

	void CPlantedC4Training::OnDefuse( CCSPlayer* pDefuser )
	{
		// training has to pick elements
		IGameEvent * event_defuse = gameeventmanager->CreateEvent( "bomb_defused" );
		if ( event_defuse )
		{
			CBombTarget* pBombTarget = GetBombTarget();
			event_defuse->SetInt( "userid", pDefuser->GetUserID() );
			event_defuse->SetInt( "site", pBombTarget ? pBombTarget->entindex() : -1 );
			event_defuse->SetInt( "priority", 5 ); // bomb_defused
			gameeventmanager->FireEvent( event_defuse );
		}

		m_OnBombDefused.FireOutput( this, pDefuser );
	}

	void CPlantedC4::Explode()
	{
		// give the bomber credit for planting the bomb
		CCSPlayer* pBombOwner = ToCSPlayer( GetOwnerEntity() );

		CSGameRules()->m_bBombDropped = false;
		CSGameRules()->m_bBombPlanted = false;

		m_bBombTicking = false;
		m_bHasExploded = true;
		m_bBombDefused = false;

		IGameEvent * event = gameeventmanager->CreateEvent( "bomb_exploded" );
		if ( event )
		{
			CBombTarget* pTarget = GetBombTarget();
			event->SetInt( "userid", pBombOwner ? pBombOwner->GetUserID() : -1 );
			event->SetInt( "site", pTarget ? pTarget->entindex() : -1 );
			event->SetInt( "priority", 20 ); // bomb_exploded
			gameeventmanager->FireEvent( event );
		}

		RemoveControlPanels();

		// Find ground under bomb
		trace_t tr;
		Vector vecSpot = GetAbsOrigin();
		vecSpot[2] += 8;
		UTIL_TraceLine( vecSpot, vecSpot + Vector( 0, 0, -40 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

		// mode-specific explosion code
		OnExplode( &tr );
	}

	void CPlantedC4::DoExplosionDamage( float flDamage )
	{
		float flRadius = flDamage * 3.5f;

		CSGameRules()->RadiusDamage(
			CTakeDamageInfo( this, GetOwnerEntity(), flDamage, DMG_BLAST ),
			GetAbsOrigin(),
			flRadius,
			CLASS_NONE,
			true );	// IGNORE THE WORLD!!
	}

	void CPlantedC4::DoExplosionEffects( trace_t* pGroundTrace )
	{
		// Pull out of the ground a bit
		if ( pGroundTrace->fraction != 1.0 )
		{
			SetAbsOrigin( pGroundTrace->endpos + ( pGroundTrace->plane.normal * 0.6 ) );
		}

		// Explosion particles
		{
			Vector pos = GetAbsOrigin() + Vector( 0, 0, 8 );

			// Make sure that the bomb exploding is a reliable effect that all clients on this server and GOTV receive
			CReliableBroadcastRecipientFilter filterBombExplodeReliable;

			// Try using the new particle system instead of temp ents
			QAngle	vecAngles;
			DispatchParticleEffect( "explosion_c4_500", pos, vecAngles, ( CBaseEntity * )NULL, int( -1 ), &filterBombExplodeReliable );
		}

		// Sound! for everyone
		CBroadcastRecipientFilter filter;
		EmitSound( filter, 0, "c4.explode", &GetAbsOrigin() );

		// Decal!
		UTIL_DecalTrace( pGroundTrace, "Scorch" );

		// Shake!
		UTIL_ScreenShake( pGroundTrace->endpos, 25.0, 150.0, 1.0, 3000, SHAKE_START );
	}

	// Classic mode -- explosion ends the round
	void CPlantedC4::OnExplode( trace_t *pGroundTrace )
	{
		// Check to see if the round is over after the bomb went off...
		CSGameRules()->m_bTargetBombed = true;

		bool roundWasAlreadyWon = ( CSGameRules()->m_iRoundWinStatus != WINNER_NONE );

		// MVP hook to award the MVP to person who planted the bomb
		class CPlantedC4ExplodedMVP : public CCSGameRules::ICalculateEndOfRoundMVPHook_t
		{
		public:
			virtual CCSPlayer* CalculateEndOfRoundMVP() OVERRIDE
			{
				// All alive Terrorists also get credit for bomb exploding,
				// this intentionally may include the original planter.
				// This way if bomb planter survives until explosion he will
				// get 2 pts and all other alive teammates will get 1 pt
				// If bomb planter doesn't survive then he gets 1 pt and all
				// teammates who remained alive defending the bomb get 1 pt
				for ( int i = 1; i <= MAX_PLAYERS; i++ )
				{
					CCSPlayer* pCheckPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
					if ( !pCheckPlayer )
						continue;
					if ( pCheckPlayer->GetTeamNumber() != TEAM_TERRORIST )
						continue;
					if ( pCheckPlayer->IsAlive() )
						CSGameRules()->ScoreBombExploded( pCheckPlayer );
				}

				if ( !pBombOwner )
					return CSGameRules()->CalculateEndOfRoundMVP();

				// Person who planted the bomb gets credit for the explosion
				CSGameRules()->ScoreBombExploded( pBombOwner );

				if( pBombOwner->HasControlledBotThisRound() )
				{ 
					// [dkorus] if we controlled a bot this round, use standard MVP conditions
					return CSGameRules()->CalculateEndOfRoundMVP();
				}
				else 
				{
					pBombOwner->IncrementNumMVPs( CSMVP_BOMBPLANT );
					return pBombOwner;
				}
			}
			CCSPlayer *pBombOwner;
		} mvpHook;
		mvpHook.pBombOwner = ToCSPlayer( GetOwnerEntity() );

		if ( !roundWasAlreadyWon )
			CSGameRules()->m_pfnCalculateEndOfRoundMVPHook = &mvpHook;

		bool bWin = CSGameRules()->CheckWinConditions();
		if ( bWin && mvpHook.pBombOwner )
		{
			mvpHook.pBombOwner->AwardAchievement( CSWinBombPlant );

			//[tj]more specific achievement for planting the bomb after recovering it.
			if ( m_bPlantedAfterPickup )
			{
				mvpHook.pBombOwner->AwardAchievement( CSWinBombPlantAfterRecovery );
			}
		}

		if ( !roundWasAlreadyWon )
			CSGameRules()->m_pfnCalculateEndOfRoundMVPHook = NULL;

		// Output to the bomb target ent
		if ( CBombTarget* pBombTarget = GetBombTarget() )
		{
			pBombTarget->AcceptInput( "BombExplode", this, this, variant_t(), 0 );
		}

		DoExplosionEffects( pGroundTrace );

		// Do the Damage
		float flBombDamage; // in our implementation radius is proportional to damage, but you take less damage the further you are from the center
		
		if ( g_pMapInfo )
			flBombDamage = g_pMapInfo->m_flBombRadius; // MISNOMER
		else if ( CSGameRules()->IsPlayingGunGameTRBomb() )
			flBombDamage = 300;
		else
			flBombDamage = 500;

		// C4 explosion doesn't count kills or go through any friendly fire logic
		SetOwnerEntity( NULL );

		DoExplosionDamage( flBombDamage );

		// send director message, that something important happed here
		/*
		MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
			WRITE_BYTE ( 9 );	// command length in bytes
			WRITE_BYTE ( DRC_CMD_EVENT );	// bomb explode
			WRITE_SHORT( ENTINDEX(this->edict()) );	// index number of primary entity
			WRITE_SHORT( 0 );	// index number of secondary entity
			WRITE_LONG( 15 | DRC_FLAG_FINAL );   // eventflags (priority and flags)
		MESSAGE_END();
		*/

		CCSPlayer* pBombOwner = ToCSPlayer( GetOwnerEntity() );
		CCS_GameStats.Event_BombExploded( pBombOwner );
	}

	// For CTs to defuse the c4
	void CPlantedC4::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		//Can't defuse if its already defused or if it has blown up (or training)
		if( !IsBombActive() || m_bHasExploded || m_flC4Blow < gpGlobals->curtime || mp_c4_cannot_be_defused.GetBool() == true || m_bCannotBeDefused )
		{
			SetUse( NULL );
			return;
		}

		CCSPlayer *player = dynamic_cast< CCSPlayer* >( pActivator );

		// Can't defuse a bomb if we're not CT or we're in a no defuse area.
		if ( !player || player->m_bInNoDefuseArea )
			return;

		if ( player->IsPlayerGhost() )
			return;

		if ( player->GetTeamNumber() != TEAM_CT )
			return;

		// make sure human players can see the bomb (or the bomb's nearby line-of-sight points)
		// to ensure we aren't defusing from the room below or something
		if ( !player->IsBot() )
		{
			trace_t result;
			bool bFoundBomb = false;

			Vector vecLineOfSightPoints[] = {
				Vector(  0,  0,  1 ),
				Vector(  5,  5,  5 ),
				Vector( -5,  5,  5 ),
				Vector(  5, -5,  5 ),
				Vector( -5, -5,  5 ),
				Vector(  0,  0,  5 )
			};
			int nNumLOSpts = ARRAYSIZE( vecLineOfSightPoints );

			for ( int i=0; i<nNumLOSpts; i++ )
			{
				Vector vecLineOfSightTarget = VectorTransform( vecLineOfSightPoints[i], EntityToWorldTransform() );
				//debugoverlay->AddBoxOverlay( vecLineOfSightTarget, Vector(-0.5f,-0.5f,-0.5f), Vector(0.5f,0.5f,0.5f), QAngle(0,0,0), 0,0,255,255, 5 );

				UTIL_TraceLine( player->EyePosition(), vecLineOfSightTarget, CONTENTS_SOLID, this, COLLISION_GROUP_NONE, &result );

				if ( result.fraction < 1.0f )
				{
					//debugoverlay->AddLineOverlay( result.startpos, result.endpos, 255,0,0, 255, 0.15f, 10 );
					continue;
				}
				//debugoverlay->AddLineOverlay( result.startpos, result.endpos, 0,255,0, 255, 0.15f, 10 );

				// this trace succeeded, so we're ok to defuse.
				bFoundBomb = true;
				break;
			}

			if ( !bFoundBomb )
				return;
		}

		if ( m_bBeingDefused )
		{
			if ( player != m_pBombDefuser )
			{
				if ( player->m_iNextTimeCheck < gpGlobals->curtime )
				{
					ClientPrint( player, HUD_PRINTCENTER, "#SFUI_Notice_Bomb_Already_Being_Defused" );
					player->m_iNextTimeCheck = gpGlobals->curtime + 1.f;
				}
				return;
			}

			m_fLastDefuseTime = gpGlobals->curtime;
		}
		else
		{
			// freeze the player in place while defusing

			IGameEvent * event = gameeventmanager->CreateEvent("bomb_begindefuse" );
			if( event )
			{
				event->SetInt( "userid", player->GetUserID() );
				if ( player->HasDefuser() )
				{
					event->SetInt( "haskit", 1 );
				}
				else
				{
					event->SetInt( "haskit", 0 );
				}
				event->SetInt( "priority", 5 ); // bomb_begindefuse
				gameeventmanager->FireEvent( event );
			}

			Vector soundPosition = player->GetAbsOrigin() + Vector( 0, 0, 5 );
			CPASAttenuationFilter filter( soundPosition );
			EmitSound( filter, 0, "c4.disarmstart", &GetAbsOrigin() );

/*
			if ( m_pBombDefuser == player && gpGlobals->curtime < ( m_fLastDefuseTime + C4_DEFUSE_GRACE_PERIOD ) )
			{
				// if we're allowing the player to continue defusing, push the completion time ahead by the appropriate amount
				float fTimeGap = m_fLastDefuseTime - gpGlobals->curtime;
				m_flDefuseCountDown += fTimeGap;

				SetProgressBarDuration( m_flDefuseLength );
				// we don't have a method for setting up the progress bar "in progress", so do it manually
				player->m_flProgressBarStartTime = m_flDefuseCountDown - m_flDefuseLength;
			}
			else
*/
			{
				m_flDefuseLength = player->HasDefuser() ? 5 : 10;
				m_flDefuseCountDown = gpGlobals->curtime + m_flDefuseLength;
				player->SetProgressBarTime( m_flDefuseLength );
			}

			m_pBombDefuser = player;
			m_bBeingDefused = TRUE;
			player->m_bIsDefusing = true;
			
			m_fLastDefuseTime = gpGlobals->curtime;

			//start the progress bar

			player->OnStartedDefuse();

			m_OnBombBeginDefuse.FireOutput(this, player);
		}
	}

	void CPlantedC4::ActivateSetTimerLength( float flTimerLength )
	{
		if ( flTimerLength < 0 )
		{
			Error( "ActivateSetTimerLength value is less than 0!" );
			return;
		}

		// Detonate in "time" seconds
		m_flTimerLength = flTimerLength;
		m_flC4Blow = gpGlobals->curtime + m_flTimerLength;

		SetThink( &CPlantedC4::C4Think );
		SetNextThink( gpGlobals->curtime + 0.1f );

		m_bBombTicking = true;
	}

///////////////////////////////////////////////////////////
// Training version of the C4 - doesn't explode
///////////////////////////////////////////////////////////

	LINK_ENTITY_TO_CLASS( planted_c4_training, CPlantedC4Training );
	PRECACHE_REGISTER( planted_c4_training );

	BEGIN_PREDICTION_DATA( CPlantedC4Training )
	END_PREDICTION_DATA()

	BEGIN_DATADESC( CPlantedC4Training )

	// Inputs
	DEFINE_INPUTFUNC( FIELD_FLOAT, "ActivateSetTimerLength", InputActivateSetTimerLength ),
	//Outputs
	DEFINE_OUTPUT( m_OnBombExploded, "OnBombExploded" ),

	END_DATADESC()

	void CPlantedC4Training::InputActivateSetTimerLength( inputdata_t &inputdata )
	{
		ActivateSetTimerLength( inputdata.value.Float() );
	}

	void CPlantedC4Training::OnExplode( trace_t *pGroundTrace )
	{
		// Check to see if the round is over after the bomb went off...
		CSGameRules()->m_bTargetBombed = true;

		// Pull out of the ground a bit
		if ( pGroundTrace->fraction != 1.0 )
		{
			SetAbsOrigin( pGroundTrace->endpos + ( pGroundTrace->plane.normal * 0.6 ) );
		}

		// Training bomb has different effects.
		QAngle	vecAngles;
		DispatchParticleEffect( "c4_train_ground_effect", GetAbsOrigin(), vecAngles );

		CBroadcastRecipientFilter filter;
		EmitSound( filter, 0, "tr.C4Explode", &GetAbsOrigin() );

		m_OnBombExploded.FireOutput(this, this);

		// Remove in 1 second
		SetThink( &CBaseEntity::SUB_Remove );
		SetNextThink( gpGlobals->curtime + 1.0f );
	}

#endif // #ifndef CLIENT_DLL



// -------------------------------------------------------------------------------- //
// Tables.
// -------------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( C4, DT_WeaponC4 )

BEGIN_NETWORK_TABLE( CC4, DT_WeaponC4 )
	#ifdef CLIENT_DLL
		RecvPropBool( RECVINFO( m_bStartedArming ) ),
		RecvPropBool( RECVINFO( m_bBombPlacedAnimation ) ),
		RecvPropFloat( RECVINFO( m_fArmedTime ) ),
		RecvPropBool( RECVINFO( m_bShowC4LED ) ),
		RecvPropBool( RECVINFO( m_bIsPlantingViaUse ) ),
	#else
		SendPropBool( SENDINFO( m_bStartedArming ) ),
		SendPropBool( SENDINFO( m_bBombPlacedAnimation ) ),
		SendPropFloat( SENDINFO( m_fArmedTime ), 0, SPROP_NOSCALE ),
		SendPropBool( SENDINFO( m_bShowC4LED ) ),
		SendPropBool( SENDINFO( m_bIsPlantingViaUse ) ),
	#endif
END_NETWORK_TABLE()

#if defined CLIENT_DLL
BEGIN_PREDICTION_DATA( CC4 )
	DEFINE_PRED_FIELD( m_bStartedArming, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bBombPlacedAnimation, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_fArmedTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bShowC4LED, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bIsPlantingViaUse, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS_ALIASED( weapon_c4, C4 );
PRECACHE_REGISTER( weapon_c4 );



// -------------------------------------------------------------------------------- //
// Globals.
// -------------------------------------------------------------------------------- //

const float DROPPED_LIGHT_INTERVAL = 1.0f;

// -------------------------------------------------------------------------------- //
// CC4 implementation.
// -------------------------------------------------------------------------------- //

CUtlVector< CC4* > g_WeaponC4s;

CC4::CC4()
{
	g_WeaponC4s.AddToTail( this );

	m_bDroppedFromDeath = false;

#if defined( CLIENT_DLL )
	m_szScreenText[0] = '\0';
	m_bShowC4LED = false;
#else
	SetSpotRules( CCSEntitySpotting::SPOT_RULE_CT | CCSEntitySpotting::SPOT_RULE_ALWAYS_SEEN_BY_T );
	m_vecLastValidPlayerHeldPosition = Vector(0,0,0);
#endif

	m_bIsPlantingViaUse = false;
}


CC4::~CC4()
{
	g_WeaponC4s.FindAndRemove( this );
}

void CC4::Spawn()
{
	BaseClass::Spawn();

	//Don't allow players to shoot the C4 around
	SetCollisionGroup( COLLISION_GROUP_DEBRIS );

	//Don't be damaged / moved by explosions
	m_takedamage = DAMAGE_NO;

	m_bBombPlanted = false;
	
#if defined( CLIENT_DLL )
	SetNextClientThink( gpGlobals->curtime + DROPPED_LIGHT_INTERVAL );
#else
	SetNextThink( gpGlobals->curtime );
#endif

}

void CC4::ItemPostFrame()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	// Disable all the firing code.. the C4 grenade is all custom.
	if ( pPlayer->m_nButtons & IN_ATTACK || (pPlayer->m_nButtons & IN_USE && m_bIsPlantingViaUse) )
	{
		if ( gpGlobals->curtime >= m_flNextPrimaryAttack ) 
			PrimaryAttack();
	}
	else
	{
		WeaponIdle();
	}

	if ( !(pPlayer->m_nButtons & IN_USE) )
		m_bIsPlantingViaUse = false;
	
}

void CC4::WeaponReset( void )
{
	m_bShowC4LED = false;

	BaseClass::WeaponReset();
}

#if defined( CLIENT_DLL )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
	void CC4::OnDataChanged( DataUpdateType_t type )
	{
		BaseClass::OnDataChanged( type );
	}

	void CC4::UpdateOnRemove( void )
	{
		BaseClass::UpdateOnRemove();

#ifdef INCLUDE_SCALEFORM
		if ( !GameUI().IsPanoramaEnabled() )
		{
			// when a c4 is removed, force the local player to update thier inventory screen
			if ( !C_BasePlayer::GetLocalPlayer() || !engine->IsLocalPlayerResolvable() )
				return; // early out if local player does not exsist

			C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
			if ( pPlayer )
			{
				SFWeaponSelection *pHudWS = GET_HUDELEMENT( SFWeaponSelection );
				if ( pHudWS )
				{
					pHudWS->ShowAndUpdateSelection( WEPSELECT_SWITCH, NULL );
				}
			}
		}
#endif	// INCLUDE_SCALEFORM
	}

	void CC4::ClientThink( void )
	{
		BaseClass::ClientThink();
		
		SetNextClientThink( gpGlobals->curtime + DROPPED_LIGHT_INTERVAL );
		
		// This think function is just for updating the blinking light of the dropped bomb.
		// So if we have an owner, we don't want to blink.
		if ( IsDormant() || NULL != GetPlayerOwner() || !CSGameRules()->m_bBombDropped )
		{
			return;
		}
		

		int ledAttachmentIndex = LookupAttachment("led");
		DispatchParticleEffect( "c4_timer_light_dropped", PATTACH_POINT_FOLLOW, this, ledAttachmentIndex, false, -1 );

	}

	bool CC4::OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
	{
		if( event == 7001 )
		{
			// "Custom code" c4 disabled
			//
			/*//if we're clearing the screen text, or have no custom description, set the text normally using the passed param.
			if (   !V_strcmp( options, "" )
				|| !V_strcmp( options, "*******" )
				|| GetEconItemView() == NULL
				|| CALL_ATTRIB_HOOK_BOOL( custom_bombcode ) == false
				|| GetEconItemView()->GetCustomDesc() == NULL
				)
			{
				Q_strncpy( m_szScreenText, options, 16 );
			}
			else
			{
				//otherwise we have a custom code to display
				CEconItemView *pItem = GetEconItemView();
				if ( pItem && pItem->GetCustomDesc() )
				{
					if ( strlen(m_szScreenText) >= 7 )
					{
						//if the 7-char code is full, replace it with asterisks (this will happen at the end of the plant animation anyway)
						Q_strncpy( m_szScreenText, "*******\0", 8 );
					}
					else
					{
						//we're still under 7 chars, so add an additional char of the custom code
						Q_strncpy( m_szScreenText, pItem->GetCustomDesc(), strlen(m_szScreenText)+2 );
					}
				}
			}*/

			//set the screen text to the string in 'options'
			Q_strncpy( m_szScreenText, options, 16 );

			return true;
		}
		return BaseClass::OnFireEvent( pViewModel, origin, angles, event, options );
	}

	char *CC4::GetScreenText( void )
	{
		return m_szScreenText;
	}

	void CC4::CreateLEDEffect( void )
	{
		if ( GetPlayerOwner() )
			GetPlayerOwner()->CreateC4Effect( GetWeaponForEffect(), true );
	}

	void CC4::RemoveLEDEffect( void )
	{
		if ( GetPlayerOwner() )
			GetPlayerOwner()->RemoveC4Effect( true );
	}

#else

void CC4::PhysicsTouchTriggers(const Vector *pPrevAbsOrigin)
{
	// Normally items like ammo or weapons aren't expected to touch other triggers, but C4 is a special case
	edict_t *pEntity = edict();

	if (pEntity && !IsWorld())
	{
		Assert(CollisionProp());

		//Dropped bombs are both solid and have no owner. In this state, unlike other weapons, they can  
		//now touch triggers so long they haven't had their position reset by a bomb reset trigger.
		if ( IsSolid() && (GetPlayerOwner() == NULL) )
		{
			SetCheckUntouch(true);
			engine->SolidMoved(pEntity, CollisionProp(), pPrevAbsOrigin, sm_bAccurateTriggerBboxChecks);
		}
	}
}

#endif //CLIENT_DLL

#ifdef GAME_DLL

	void CC4::Think()
	{
		//If the bomb is held by an alive player standing on the ground, then we can use this
		//position as the last known valid position to respawn the bomb if it gets reset.
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( pPlayer && pPlayer->IsAlive() && FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
		{
			m_vecLastValidPlayerHeldPosition = pPlayer->GetAbsOrigin();
		}

		if ( pPlayer && CSGameRules() && CSGameRules()->IsPlayingSurvival() && pPlayer->GetActiveCSWeapon() == this )
		{
			CBaseEntity *pTarget = CSGameRules()->GetSurvivalRules()->GetNearestC4Target( pPlayer->GetAbsOrigin() );
			if ( pTarget )
			{
				CBaseViewModel *pViewModel = pPlayer->GetViewModel();
				if ( pViewModel )
				{
					pViewModel->SetBodygroup( 1, 1 );
				}
			}
		}

		SetNextThink( gpGlobals->curtime + WEAPON_C4_UPDATE_LAST_VALID_PLAYER_HELD_POSITION_INTERVAL );
	}

	void CC4::ResetToLastValidPlayerHeldPosition()
	{
		//When reset, the bomb returns to its last known valid position.
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer && GetAbsOrigin() != m_vecLastValidPlayerHeldPosition )
		{
			// Teleport the bomb facing up so the flashing light is clearly visible.
			Vector vecResetPos = m_vecLastValidPlayerHeldPosition + Vector(0,0,8);
			QAngle angResetAng = QAngle( 0, RandomInt(0,360), 0 );

			// trace to ground
			trace_t c4TeleportTrace;
			UTIL_TraceHull( vecResetPos, vecResetPos + Vector(0,0,-8), Vector(-3,-3,-1), Vector(3,3,1), MASK_PLAYERSOLID, NULL, COLLISION_GROUP_PLAYER_MOVEMENT, &c4TeleportTrace );
			if ( !c4TeleportTrace.startsolid && c4TeleportTrace.DidHit() )
			{
				vecResetPos += ( c4TeleportTrace.fraction * Vector(0,0,-8) );
			}

			Teleport( &vecResetPos, &angResetAng, NULL );

			// Set the physics object asleep so it doesn't tumble off precarious ledges and keep resetting.
			IPhysicsObject *pObj = VPhysicsGetObject();
			if ( pObj )
				pObj->Sleep();

		}
	}

	unsigned int CC4::PhysicsSolidMaskForEntity( void ) const
	{
		return BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_PLAYERCLIP;
	}

	void CC4::Precache()
	{
		PrecacheVGuiScreen( "c4_view_panel" );

		PrecacheScriptSound( "c4.disarmfinish" );
		PrecacheScriptSound( "c4.explode" );
		PrecacheScriptSound( "c4.disarmstart" );
		PrecacheScriptSound( "c4.plant" );
		PrecacheScriptSound( "C4.PlantSound" );
		PrecacheScriptSound( "c4.click" );

		// training
		PrecacheParticleSystem( "c4_train_ground_effect" );
		PrecacheScriptSound( "tr.C4Explode" );

		BaseClass::Precache();
	}

	int CC4::UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_FULLCHECK );
	}

	int CC4::ShouldTransmit( const CCheckTransmitInfo *pInfo )
	{
		CBasePlayer *pPlayer = ToBasePlayer( CBaseEntity::Instance( pInfo->m_pClientEnt ) );
		if ( pPlayer && pPlayer->GetTeamNumber() == TEAM_CT )
		{
			return FL_EDICT_PVSCHECK;
		}
		else
		{
			// This is needed for the instructor message
			return FL_EDICT_ALWAYS;
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: Gets info about the control panels
	//-----------------------------------------------------------------------------
	void CC4::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
	{
		pPanelName = "c4_view_panel";
	}

	bool CC4::ShouldRemoveOnRoundRestart()
	{
		// Doesn't matter if we have an owner or not.. always remove the C4 when the round restarts.
		// The gamerules will give another C4 to some lucky player.
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( pPlayer && pPlayer->GetActiveWeapon() == this )
			engine->ClientCommand( pPlayer->edict(), "lastinv reset\n" );
		return true;
	}
#endif

bool CC4::Deploy( )
{
	//m_bShowC4LED = true;
#ifdef CLIENT_DLL
	//CreateLEDEffect();
#endif

	return BaseClass::Deploy();
}

bool CC4::Holster( CBaseCombatWeapon *pSwitchingTo )
{

#ifdef GAME_DLL
	if ( m_bStartedArming )
	{
		AbortBombPlant();
	}
#else
	//RemoveLEDEffect();
#endif

	//m_bShowC4LED = false;

	return BaseClass::Holster( pSwitchingTo );
}

void CC4::PrimaryAttack()
{
	bool	bArmingTimeSatisfied = false;
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

#ifndef CLIENT_DLL

	bool bNotSurvival = !(CSGameRules() && CSGameRules()->IsPlayingSurvival());

#endif

	int onGround = FBitSet( pPlayer->GetFlags(), FL_ONGROUND );
	CBaseEntity *groundEntity = (onGround) ? pPlayer->GetGroundEntity() : NULL;
	trace_t trPlant;
	if ( groundEntity )
	{
		// Don't let us stand on players, breakables, or pushaway physics objects to plant
		if ( groundEntity->IsPlayer() ||
			IsPushableEntity( groundEntity ) ||
#ifndef CLIENT_DLL
			IsBreakableEntity( groundEntity ) ||
#endif // !CLIENT_DLL
			IsPushAwayEntity( groundEntity ) )
		{
			onGround = false;
		}

#ifndef CLIENT_DLL
		if ( onGround == false && CSGameRules() && CSGameRules()->IsPlayingSurvival() )
		{
			onGround = groundEntity->ClassMatches( "func_survival_c4_target" );
		}
#endif

		if ( onGround )
		{
			Vector vecStart = GetAbsOrigin() + Vector(0,0,8);
			Vector vecEnd = GetAbsOrigin() + Vector(0,0,-38);
			UTIL_TraceHull( vecStart, vecEnd, Vector( -3, -3, 0 ), Vector( 3, 3, 16 ), MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_PLAYER_MOVEMENT, &trPlant );
			if ( trPlant.fraction == 1.0 )
			{
				onGround = false;
			}
		}
	}

	if( m_bStartedArming == false && m_bBombPlanted == false )
	{
		if( (pPlayer->m_bInBombZone || mp_plant_c4_anywhere.GetBool()) && onGround )
		{
			m_bStartedArming = true;
			m_fArmedTime = gpGlobals->curtime + WEAPON_C4_ARM_TIME;
			m_bBombPlacedAnimation = false;

			pPlayer->m_bDuckOverride = true;

#if !defined( CLIENT_DLL )			
			pPlayer->SetAttemptedBombPlace();

			// init the beep flags
			int i;
			for( i=0;i<NUM_BEEPS;i++ )
				m_bPlayedArmingBeeps[i] = false;

			// freeze the player in place while planting

			// player "arming bomb" animation
			pPlayer->SetAnimation( PLAYER_ATTACK1 );
	
			pPlayer->SetNextAttack( gpGlobals->curtime );

			IGameEvent * event = gameeventmanager->CreateEvent( "bomb_beginplant" );
			if( event )
			{
				event->SetInt("userid", pPlayer->GetUserID() );
				event->SetInt("site", pPlayer->m_iBombSiteIndex );
				event->SetInt( "priority", 5 ); // bomb_beginplant
				gameeventmanager->FireEvent( event );
			}

			PlayPlantInitSound();

			if ( pPlayer && !pPlayer->IsBot() && pPlayer->m_flC4PlantTalkTimer < gpGlobals->curtime && bNotSurvival )
			{
				// for console, we don't want to show the chat text because it almost always overlaps 
				// with the bomb planted alert text in the center of the screen
				if ( IsGameConsole() || engine->IsDedicatedServerForXbox() || engine->IsDedicatedServerForPS3() )
					pPlayer->Radio( "PlantingBomb", "", true );
				else
					pPlayer->Radio( "PlantingBomb", "#Cstrike_TitlesTXT_Planting_Bomb", true );
				pPlayer->m_flC4PlantTalkTimer = gpGlobals->curtime + 10.0f;
			}
#endif

			SendWeaponAnim( ACT_VM_PRIMARYATTACK );

			#ifndef CLIENT_DLL
			if ( pPlayer && !pPlayer->IsDormant() )
			{
				pPlayer->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );
			}
			#endif
			//FX_PlantBomb( pPlayer->entindex(), pPlayer->Weapon_ShootPosition(), PLANTBOMB_PLANT );
		}
		else
		{
			if ( !pPlayer->GetUseEntity() )
			{
				if ( !pPlayer->m_bInBombZone && !mp_plant_c4_anywhere.GetBool() )
				{
					ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_C4_Plant_At_Bomb_Spot");

#if defined( CLIENT_DLL )
					STEAMWORKS_TESTSECRET_AMORTIZE(5);
#endif
				}
				else
				{
					ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_C4_Plant_Must_Be_On_Ground");

#if defined( CLIENT_DLL )
					STEAMWORKS_TESTSECRET_AMORTIZE(7);
#endif
				}
			}

			m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
			return;
		}
	}
	else
	{
		if ( !onGround || !(pPlayer->m_bInBombZone || mp_plant_c4_anywhere.GetBool()) )
		{
			if( !(pPlayer->m_bInBombZone || mp_plant_c4_anywhere.GetBool()) )
			{
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_C4_Arming_Cancelled" );
			}
			else
			{
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_C4_Plant_Must_Be_On_Ground" );

#if defined( CLIENT_DLL )
				STEAMWORKS_TESTSECRET_AMORTIZE(9);
#endif
			}

			AbortBombPlant();

			if(m_bBombPlacedAnimation == true) //this means the placement animation is canceled
			{
				SendWeaponAnim( ACT_VM_DRAW );
			}
			else
			{
				SendWeaponAnim( ACT_VM_IDLE );
			}
			
			return;
		}
		else
		{
		// we no longer play arming beeps to all player, we only play the initialization sound
// #ifndef CLIENT_DLL
// 			PlayArmingBeeps();
// #endif

			if( gpGlobals->curtime >= m_fArmedTime ) //the c4 is ready to be armed
			{
				//check to make sure the player is still in the bomb target area
				bArmingTimeSatisfied = true;
			}
			else if( ( gpGlobals->curtime >= (m_fArmedTime - 0.75) ) && ( !m_bBombPlacedAnimation ) )
			{
				//call the c4 Placement animation 
				m_bBombPlacedAnimation = true;

				SendWeaponAnim( ACT_VM_SECONDARYATTACK );
				
#if !defined( CLIENT_DLL )
				// player "place" animation
				//pPlayer->SetAnimation( PLAYER_HOLDBOMB );
#endif
			}
		}
	}

	if ( bArmingTimeSatisfied && m_bStartedArming )
	{
		m_bStartedArming = false;
		m_fArmedTime = 0;
		
		if( (pPlayer->m_bInBombZone || mp_plant_c4_anywhere.GetBool()) )
		{
#if !defined( CLIENT_DLL )
			CPlantedC4 *pC4 = CPlantedC4::ShootSatchelCharge( pPlayer, pPlayer->GetAbsOrigin(), pPlayer->GetAbsAngles() );

			if ( pC4 && trPlant.fraction < 1.0 )
			{
				CBombTarget* pBombTarget = dynamic_cast< CBombTarget* >( UTIL_EntityByIndex( pPlayer->m_iBombSiteIndex ) );
				pC4->SetBombTarget( pBombTarget );
				pC4->SetAbsOrigin( trPlant.endpos );

				//bomb aligns to planted surface normal within a threshold
				if ( fabs(trPlant.plane.normal.Dot(Vector(0,0,1))) > 0.65f )
				{
					//get the player forward vector
					Vector vecFlatForward;
					VectorCopy( pPlayer->Forward(), vecFlatForward );
					vecFlatForward.z = 0;

					//derive c4 forward and right
					Vector vecC4Right = CrossProduct( vecFlatForward.Normalized(), trPlant.plane.normal );
					Vector vecC4Forward = CrossProduct( vecC4Right, trPlant.plane.normal );
					
					QAngle C4Angle;
					VectorAngles( -vecC4Forward, trPlant.plane.normal, C4Angle );
					pC4->SetAbsAngles( C4Angle );
				}

				if ( pBombTarget )
				{
					CBaseEntity *pAttachPoint = gEntList.FindEntityByName( NULL, pBombTarget->GetBombMountTarget() );

					if ( pAttachPoint )
					{
						pC4->SetAbsOrigin( pAttachPoint->GetAbsOrigin() );
						pC4->SetAbsAngles( pAttachPoint->GetAbsAngles() );
						pC4->SetParent( pAttachPoint );
					}

					variant_t emptyVariant;
					pBombTarget->AcceptInput( "BombPlanted", pC4, pC4, emptyVariant, 0 );
				}

				// [tj] If the bomb is planted by someone that picked it up after the 
				//      original owner was killed, pass that along to the planted bomb
				pC4->SetPlantedAfterPickup( m_bDroppedFromDeath );

				if ( CSGameRules() && CSGameRules()->IsPlayingSurvival() )
				{
					pC4->m_bPlantedAtQuestTarget = CSGameRules()->GetSurvivalRules()->IsInRangeOfNearestC4Target( pPlayer->GetAbsOrigin() );

					if ( pC4->m_bPlantedAtQuestTarget )
					{
						UTIL_PushGlobalTabletNotification( TABLET_NOTIFICATION_BOMB_PLANTED_ON_TARGET );
					}

					// play the old 'thunk!' sound when planting c4 in survival. Players thought it was odd that planting the bomb was silent;
					// while it's also silent in regular CS, players don't notice that because it's always immediately followed by the
					// "BOMB HAS BEEN PLANTED" voice line.
					EmitSound( "c4.plant" );
				}

			}

			// Determine how elapsed time from start of round until the bomb was planted
			float plantingTime = gpGlobals->curtime - CSGameRules()->GetRoundStartTime();

			if ( bNotSurvival )
			{
				// Award achievement to bomb planter if time <= 25 seconds
				if ( (plantingTime > 0.0f) &&
					(plantingTime <= AchievementConsts::FastBombPlant_Time) &&
					!CSGameRules()->IsPlayingGunGameTRBomb() )
				{
					pPlayer->AwardAchievement( CSPlantBombWithin25Seconds );
				}
			}

			pPlayer->SetLastWeaponBeforeAutoSwitchToC4( NULL ); // completed a bomb plant, this clears out our saved value for switching back to a saved weapon
			pPlayer->SetBombPlacedTime( gpGlobals->curtime );

			if ( bNotSurvival )
			{
				CCS_GameStats.Event_BombPlanted( pPlayer );
				CSGameRules()->ScoreBombPlant( pPlayer );

				int iCurrentRound = CSGameRules()->GetCurrentRoundIndexForMatchStats();

				// record in matchstats
				if ( CSGameRules()->ShouldRecordMatchStats( iCurrentRound ) )
				{
					++pPlayer->m_iMatchStats_Objective.GetForModify( iCurrentRound );

					// Keep track of Match stats in QMM data
					if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( pPlayer->GetHumanPlayerAccountID() ) )
					{
						pQMM->m_iMatchStats_Objective[iCurrentRound] = pPlayer->m_iMatchStats_Objective.Get( iCurrentRound );
					}
				}

				pPlayer->AddAccountAward( PlayerCashAward::BOMB_PLANTED );

				IGameEvent * event_planted = gameeventmanager->CreateEvent( "bomb_planted" );
				if ( event_planted )
				{
					event_planted->SetInt( "userid", pPlayer->GetUserID() );
					event_planted->SetInt( "site", pPlayer->m_iBombSiteIndex );
					event_planted->SetInt( "posx", pPlayer->GetAbsOrigin().x );
					event_planted->SetInt( "posy", pPlayer->GetAbsOrigin().y );
					event_planted->SetInt( "priority", 5 ); // bomb_planted
					gameeventmanager->FireEvent( event_planted );
				}

			}

			// Fire a beep event also so the bots have a chance to hear the bomb
			IGameEvent * event_beep = gameeventmanager->CreateEvent( "bomb_beep" );
			if ( event_beep )
			{
				event_beep->SetInt( "entindex", entindex() );
				gameeventmanager->FireEvent( event_beep, true ); // server-side event only
			}

			CSGameRules()->m_bBombDropped = false;
			CSGameRules()->m_bBombPlanted = true;


			// Play the plant sound.
			// don't play a bomb plant sound for everyone anymore ?
			Vector plantPosition = pPlayer->GetAbsOrigin() + Vector( 0, 0, 5 );
			CPASAttenuationFilter filter( plantPosition );
			EmitSound( filter, 0, "c4.plantquiet", &GetAbsOrigin() );

			// No more c4!
			pPlayer->Weapon_Drop( this, NULL, NULL );
			UTIL_Remove( this );

			pPlayer->m_bDuckOverride = false;
#endif
			//don't allow the planting to start over again next frame.
			m_bBombPlanted = true;

#if !defined( CLIENT_DLL )
			if ( bNotSurvival && CSGameRules()->IsPlayingCooperativeGametype() )
				CSGameRules()->CheckWinConditions();// %plant bomb%
#endif

			return;
		}
		else
		{
			ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_C4_Activated_At_Bomb_Spot" );

#if !defined( CLIENT_DLL )
			//pPlayer->SetAnimation( PLAYER_HOLDBOMB );

			IGameEvent * event = gameeventmanager->CreateEvent( "bomb_abortplant" );
			if( event )
			{
				event->SetInt("userid", pPlayer->GetUserID() );
				event->SetInt("site", pPlayer->m_iBombSiteIndex );
				event->SetInt( "priority", 5 ); // bomb_abortplant
				gameeventmanager->FireEvent( event );
			}
#endif

			m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
			return;
		}
	}

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.3;
	SetWeaponIdleTime( gpGlobals->curtime + SharedRandomFloat("C4IdleTime", 10, 15 ) );
}

void CC4::WeaponIdle()
{
	// if the player releases the attack button cancel the arming sequence
	if ( m_bStartedArming )
	{
		AbortBombPlant();

		CCSPlayer *pPlayer = GetPlayerOwner();

		pPlayer->m_bDuckOverride = false;

		// TODO: make this use SendWeaponAnim and activities when the C4 has the activities hooked up.
		if ( pPlayer )
		{
			SendWeaponAnim( ACT_VM_IDLE );
			pPlayer->SetNextAttack( gpGlobals->curtime );
		}

		if(m_bBombPlacedAnimation == true) //this means the placement animation is canceled
			SendWeaponAnim( ACT_VM_DRAW );
		else
			SendWeaponAnim( ACT_VM_IDLE );
	}
}

void CC4::UpdateShieldState( void )
{
	//ADRIANTODO
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;
	
	if ( pPlayer->HasShield() )
	{
		pPlayer->SetShieldDrawnState( false );

		CBaseViewModel *pVM = pPlayer->GetViewModel( 1 );

		if ( pVM )
		{
			pVM->AddEffects( EF_NODRAW );
		}
			//pPlayer->SetHitBoxSet( 3 );
	}
	else
		BaseClass::UpdateShieldState();
}


int m_iBeepFrames[NUM_BEEPS] = { 20, 29, 37, 44, 50, 59, 65 };
int iNumArmingAnimFrames = 83;

void CC4::PlayArmingBeeps( void )
{
	float flStartTime = m_fArmedTime - WEAPON_C4_ARM_TIME;

	float flProgress = ( gpGlobals->curtime - flStartTime ) / ( WEAPON_C4_ARM_TIME - 0.75 );

	int currentFrame = (int)( (float)iNumArmingAnimFrames * flProgress );

	int iBeep;
	for ( iBeep = 0; iBeep < NUM_BEEPS; iBeep++ )
	{
		if( currentFrame <= m_iBeepFrames[iBeep] )
		{
			break;
		}
		else if( !m_bPlayedArmingBeeps[iBeep] )
		{
			m_bPlayedArmingBeeps[iBeep] = true;

			CCSPlayer *owner = GetPlayerOwner();
			if ( !owner && !owner->IsAlive() )
				break;

			Vector soundPosition = owner->GetAbsOrigin() + Vector( 0, 0, 5 );
			CPASAttenuationFilter filter( soundPosition );

			filter.RemoveRecipient( owner );

			// remove anyone that is first person spec'ing the planter
			int iPlayer;
			CBasePlayer *pPlayer;
			for ( iPlayer = 1; iPlayer <= gpGlobals->maxClients; iPlayer++ )
			{
				pPlayer = UTIL_PlayerByIndex( iPlayer );

				if ( !pPlayer )
					continue;

				if( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pPlayer->GetObserverTarget() == GetOwner() )
				{
					filter.RemoveRecipient( pPlayer );
				}
			}

			EmitSound(filter, 0, "c4.click", &GetAbsOrigin() );
			
			break;
		}
	}
}

void CC4::PlayPlantInitSound( void )
{
	CCSPlayer *owner = GetPlayerOwner();
	if ( !owner && !owner->IsAlive() )
		return;

	Vector soundPosition = owner->GetAbsOrigin() + Vector( 0, 0, 5 );
	CPASAttenuationFilter filter( soundPosition );

	filter.RemoveRecipient( owner );

	// remove anyone that is first person spec'ing the planter
	int i;
	CBasePlayer *pPlayer;
	for( i=1;i<=gpGlobals->maxClients;i++ )
	{
		pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;

		if( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pPlayer->GetObserverTarget() == GetOwner() )
		{
			filter.RemoveRecipient( pPlayer );
		}
	}

	EmitSound(filter, 0, "c4.initiate", &GetAbsOrigin() );
}

float CC4::GetMaxSpeed() const
{
	if ( m_bStartedArming )
		return CS_PLAYER_SPEED_STOPPED;
	else
		return BaseClass::GetMaxSpeed();
}


void CC4::OnPickedUp( CBaseCombatCharacter *pNewOwner )
{
	BaseClass::OnPickedUp( pNewOwner );

#if !defined( CLIENT_DLL )
	CCSPlayer *pPlayer = dynamic_cast<CCSPlayer *>( pNewOwner );

	IGameEvent * event = gameeventmanager->CreateEvent( "bomb_pickup" );
	if ( event )
	{
		event->SetInt( "userid", pPlayer->GetUserID() );
		event->SetInt( "priority", 5 ); // bomb_pickup
		gameeventmanager->FireEvent( event );
	}

	CSGameRules()->m_bBombDropped = false;

	if ( !CSGameRules()->IsPlayingTraining() && !CSGameRules()->IsPlayingSurvival() )
		ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_Got_Bomb" );

	pPlayer->SetBombPickupTime( gpGlobals->curtime );
#endif
}

// HACK - Ask Mike Booth...
#ifndef CLIENT_DLL
	#include "cs_bot.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void CC4::Drop( const Vector &vecVelocity )
{
#if !defined( CLIENT_DLL )
	if ( !CSGameRules()->m_bBombPlanted ) // its not dropped if its planted
	{
		// tell the bots about the dropped bomb
		TheCSBots()->SetLooseBomb( this );

		// $$$REI What if there are multiple C4s
		CSGameRules()->m_bBombDropped = true;

		CBasePlayer *pPlayer = dynamic_cast<CBasePlayer *>(GetOwnerEntity());
		Assert( pPlayer );
		if ( pPlayer )
		{
			CCSPlayer *pCCSPlayer = dynamic_cast<CCSPlayer *>( pPlayer );

			pCCSPlayer->SetBombDroppedTime( gpGlobals->curtime );
			IGameEvent * event = gameeventmanager->CreateEvent( "bomb_dropped" );
			if ( event )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				event->SetInt( "entindex", entindex() );
				event->SetInt( "priority", 5 ); // bomb_dropped
				gameeventmanager->FireEvent( event );

				//event->SetInt( "entindex", iTest );
			}
		}
	}
#endif

#if defined( CLIENT_DLL )
	STEAMWORKS_TESTSECRET_AMORTIZE( 13 );
#endif

	if ( m_bStartedArming )
		AbortBombPlant();  // stop arming sequence

	BaseClass::Drop( vecVelocity );
}

void CC4::AbortBombPlant()
{
	m_bStartedArming = false; 

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

#if !defined( CLIENT_DLL )

	m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;

	IGameEvent * event = gameeventmanager->CreateEvent( "bomb_abortplant" );
	if( event )
	{
		event->SetInt("userid", pPlayer->GetUserID() );
		event->SetInt("site", pPlayer->m_iBombSiteIndex );
		event->SetInt( "priority", 5 ); // bomb_abortplant
		gameeventmanager->FireEvent( event );
	}

	if( pPlayer->GetLastWeaponBeforeAutoSwitchToC4() != NULL )
	{
		CBaseViewModel *vm = pPlayer->GetViewModel();
		if ( vm )
		{
			vm->AddEffects( EF_NODRAW );
		}

		pPlayer->Weapon_Switch( pPlayer->GetLastWeaponBeforeAutoSwitchToC4() );
		pPlayer->SetLastWeaponBeforeAutoSwitchToC4( NULL );
	}

#else

	// Clear the numbers from the screen if we just aborted.
	m_szScreenText[0] = '\0';

#endif

	#ifndef CLIENT_DLL
	if ( pPlayer && !pPlayer->IsDormant() )
	{
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_CLEAR_FIRING );
	}
	#endif
	//FX_PlantBomb( pPlayer->entindex(), pPlayer->Weapon_ShootPosition(), PLANTBOMB_ABORT );
}



