//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose:  PanoramaSurvival panel showed during end-of-match screen.
//
//=====================================================================================//

#include "cbase.h"
#include "csgo_survival_endofmatch.h"
#include "panorama/uijsregistration.h"

#include "netmessages.h"
#include "usermessages.h"
#include "hltvreplaysystem.h"

#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "clientmode_csnormal.h"

// #include "uicomponents/uicomponent_lobby.h"
// #include "uicomponents/uicomponent_matchstats.h"
// #include "uicomponents/uicomponent_competitivematch.h"

#include "c_cs_survival_funfacts.h"

#include "csgo_endofmatch.h"


REGISTER_PANEL2D_FACTORY( CCSGO_SurvivalEndOfMatch, CSGOSurvivalEndOfMatch );

DECLARE_PANORAMA_EVENT0( EndOfMatch_Survival_Requeue_Clicked );
DEFINE_PANORAMA_EVENT( EndOfMatch_Survival_Requeue_Clicked );
DEFINE_PANORAMA_EVENT( CSGOSurvivalStatsUpdated );

ConVar cl_dz_playagain_auto_spectate( "cl_dz_playagain_auto_spectate", "0", FCVAR_RELEASE | FCVAR_ARCHIVE, "Automatically switch to spectate mode after clicking the 'Play Again' button in end of match screen" );

CCSGO_SurvivalEndOfMatch::CCSGO_SurvivalEndOfMatch( panorama::CPanel2D *pParent, const char *pchID )
	: panorama::CPanel2D( pParent, pchID )
{
	RequireLoadLayout( "file://{resources}/layout/survival/survival_endofmatch.xml" );

	m_pSpectateButton = panorama::panel_cast< panorama::CButton* >( FindChildInLayoutFile( "eom-survival_spectate" ) );
	m_pQueueButton = panorama::panel_cast< panorama::CButton* >( FindChildInLayoutFile( "eom-survival_queue" ) );
	m_pWarningContainer = panorama::panel_cast< panorama::CPanel2D* >( FindChildInLayoutFile( "id-eom-survival-warning" ) );
	m_pWarningLabel = panorama::panel_cast< panorama::CLabel* >( FindChildInLayoutFile( "id-eom-survival-warning-label" ) );

	RegisterForUnhandledEvent( CSGOFrameUpdate(), this, &CCSGO_SurvivalEndOfMatch::EventCSGOFrameUpdate );
	RegisterForUnhandledEvent( EndOfMatch_Survival_Requeue_Clicked(), this, &CCSGO_SurvivalEndOfMatch::OnEndOfMatchSurvivalRequeueClicked );

	UpdateButtonStates();
}

CCSGO_SurvivalEndOfMatch::~CCSGO_SurvivalEndOfMatch()
{
	UnregisterForUnhandledEvent( EndOfMatch_Survival_Requeue_Clicked(), this, &CCSGO_SurvivalEndOfMatch::OnEndOfMatchSurvivalRequeueClicked );
	UnregisterForUnhandledEvent( CSGOFrameUpdate(), this, &CCSGO_SurvivalEndOfMatch::EventCSGOFrameUpdate );
}

void CCSGO_SurvivalEndOfMatch::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "GetInfo", PANORAMA_DELEGATE( &CCSGO_SurvivalEndOfMatch::GetInfo ) );
	RegisterJSMethod( "UpdateButtonStates", PANORAMA_DELEGATE( &CCSGO_SurvivalEndOfMatch::UpdateButtonStates ) );
}

struct CSurvivalEOMPlayerInfo {
	XUID xuid;
	bool alive;
	bool bLocalPlayer;

	// damage data
	int damageTo;
	int damageFrom;

	int teamIdx; // index into team array
};

struct CSurvivalEOMTeamInfo {
	int teamnum;  // team number from survival data. -1 for solos
	int position;
	int numAlive;
	int order;
};

v8::Local<v8::Value> CCSGO_SurvivalEndOfMatch::GetInfo()
{
	v8::Isolate* pIsolate = panorama::GetV8Isolate();

	CSurvivalGameRules* pSurvivalRules = CSGameRules() ? CSGameRules()->GetSurvivalRules() : nullptr;
	if ( !pSurvivalRules || CSGameRules()->IsWarmupPeriod() || pSurvivalRules->GetSpawnStage() != CSurvivalGameRules::SPAWN_STAGE_NONE )
		return v8::Undefined( pIsolate );

	v8::Local<v8::Object> pObject = v8::Object::New( pIsolate );

	// Get survival facts from server message
	const CCSUsrMsg_SurvivalStats* pStatsMsg = nullptr;
	pStatsMsg = pSurvivalRules->GetLocalPlayerStatMsg();
	uint64 xuidLocalPlayer = 0;

	if ( pStatsMsg )
	{
		// Set local player stats
		xuidLocalPlayer = pStatsMsg->xuid();

		v8::HandleScope handleScope( pIsolate );
		v8::Local<v8::Object> pObjLocal = v8::Object::New( pIsolate );
		pObject->Set( pIsolate->GetCurrentContext(), v8::String::NewFromUtf8( pIsolate, "localPlayer" ).ToLocalChecked(), pObjLocal );

		bool bCanImprove = false;
		int nTeamPosition = pSurvivalRules->GetPlacementFromStatMsgs( xuidLocalPlayer, true, nullptr, &bCanImprove ); // get position including team position
		// if any teammates still alive, set 0 (unknown position -- team still alive) here
		if ( bCanImprove )
			nTeamPosition = 0;
		pObjLocal->Set( pIsolate->GetCurrentContext(), v8::String::NewFromUtf8( pIsolate, "position" ).ToLocalChecked(), v8::Integer::New( pIsolate, nTeamPosition ) );

		v8::Local<v8::String> pStrPositionLoc = v8::String::NewFromUtf8( pIsolate, "position_loc" ).ToLocalChecked();
		if ( nTeamPosition <= 0 )
			pObjLocal->Set( pIsolate->GetCurrentContext(), pStrPositionLoc, v8::String::NewFromUtf8( pIsolate, "#EOM_Position_Unknown" ).ToLocalChecked() );
		else if ( nTeamPosition >= 17 )
			pObjLocal->Set( pIsolate->GetCurrentContext(), pStrPositionLoc, v8::String::NewFromUtf8( pIsolate, CFmtStr( "%d", nTeamPosition ) ).ToLocalChecked() );
		else
			pObjLocal->Set( pIsolate->GetCurrentContext(), pStrPositionLoc, v8::String::NewFromUtf8( pIsolate, CFmtStr( "#EOM_Position_%d", nTeamPosition ) ).ToLocalChecked() );

		pObjLocal->Set( pIsolate->GetCurrentContext(), v8::String::NewFromUtf8( pIsolate, "xuid" ).ToLocalChecked(), v8::String::NewFromUtf8( pIsolate, CFmtStr( "%lld", xuidLocalPlayer ) ).ToLocalChecked() );

		int nSoloPosition = pSurvivalRules->GetPlacementFromStatMsgs( xuidLocalPlayer, false );
		bool bSurvived = ( nSoloPosition <= 1 ); // Because user can invoke match stats early before they are dead in replay, use position to check if they were still alive when the message was sent
		pObjLocal->Set( pIsolate->GetCurrentContext(), v8::String::NewFromUtf8( pIsolate, "alive" ).ToLocalChecked(), v8::Boolean::New( pIsolate, bSurvived ) );
		pObjLocal->Set( pIsolate->GetCurrentContext(), v8::String::NewFromUtf8( pIsolate, "alive_loc" ).ToLocalChecked(),
			v8::String::NewFromUtf8( pIsolate, bSurvived ? "#EOM_Survival_Survived" : bCanImprove ? "#EOM_Survival_TeammateAlive" : "#EOM_Survival_Eliminated" ).ToLocalChecked() );

		v8::Local<v8::Array> pStats = v8::Array::New( pIsolate );
		pObjLocal->Set( pIsolate->GetCurrentContext(), v8::String::NewFromUtf8( pIsolate, "stats" ).ToLocalChecked(), pStats );

		v8::Local<v8::String> pStrName = v8::String::NewFromUtf8( pIsolate, "name" ).ToLocalChecked();
		v8::Local<v8::String> pStrValue = v8::String::NewFromUtf8( pIsolate, "value" ).ToLocalChecked();
		v8::Local<v8::String> pStrValueLoc = v8::String::NewFromUtf8( pIsolate, "value_loc" ).ToLocalChecked();

		int iStatArraySize = 0;
		for ( int i = 0; i < pStatsMsg->facts_size(); ++i )
		{
			const CCSUsrMsg_SurvivalStats_Fact& fact = pStatsMsg->facts( i );

			if ( fact.type() < 0 || fact.type() >= ESurvivalFact_Count )
				continue;
			if ( fact.display() < 0 || fact.display() >= ESurvivalFactDisplay_Count )
				continue;

			v8::HandleScope handleScopeLoop( pIsolate );
			v8::Local<v8::Object> pStat = v8::Object::New( pIsolate );
			pStat->Set( pIsolate->GetCurrentContext(), pStrName, v8::String::NewFromUtf8( pIsolate, CFmtStr( "#Survival_StatName_%s", kSurvivalFactNames[fact.type()] ) ).ToLocalChecked() );
			pStat->Set( pIsolate->GetCurrentContext(), pStrValue, v8::Integer::New( pIsolate, fact.value() ) );
			pStat->Set( pIsolate->GetCurrentContext(), pStrValueLoc, v8::String::NewFromUtf8( pIsolate, CFmtStr( "#Survival_StatDisplay_%s", kSurvivalFactDisplayNames[fact.display()] ) ).ToLocalChecked() );

			pStats->Set( pIsolate->GetCurrentContext(), iStatArraySize++, pStat );
		}

		Assert( pStats->Length() == ( uint32 )iStatArraySize );
	}

	// Construct player and team info
	// Get all player xuids
	CUtlVector< CSurvivalEOMTeamInfo > vecTeamInfo;
	CUtlVector< CSurvivalEOMPlayerInfo > vecPlayerInfo;
	CUtlVector< CSurvivalEOMTeamInfo* > vecTeamOrder;

	for ( int i = 1; i <= MAX_PLAYERS; ++i )
	{
		XUID xuid = pSurvivalRules->GetPlayerXuid( i );
		if ( xuid == 0 )
			continue;

		int teamNum = -1;
		int position = pSurvivalRules->GetPlacementFromStatMsgs( xuid, false, &teamNum );
		if ( position < 0 )
			continue;

		int teamIdx = -1;
		if ( teamNum >= 0 )
		{
			// look up existing team if any
			FOR_EACH_VEC( vecTeamInfo, iTeam )
			{
				if ( vecTeamInfo[iTeam].teamnum == teamNum )
				{
					teamIdx = iTeam;
					break;
				}
			}
		}

		// either no team with this number, or teamNum < 0 (i.e. solos)
		if ( teamIdx < 0 )
		{
			// make a new team
			teamIdx = vecTeamInfo.AddToTail( CSurvivalEOMTeamInfo{ teamNum, -2 /*unknown*/, 0, -1 /*unset*/ });
		}

		Assert( teamIdx >= 0 );
		CSurvivalEOMTeamInfo& teamInfo = vecTeamInfo[teamIdx];

		// Lookup position of another player at the time local player died
		bool bAlive = ( position <= 1 );

		// update team position
		if ( teamInfo.position < 0 || position < teamInfo.position )
			teamInfo.position = position;

		if ( bAlive )
			teamInfo.numAlive++;

		int damageTo = 0;
		int damageFrom = 0;

		if ( pStatsMsg )
		{
			for ( int iDamage = 0; iDamage < pStatsMsg->damages_size(); ++iDamage )
			{
				const CCSUsrMsg_SurvivalStats_Damage &damage = pStatsMsg->damages( iDamage );

				if ( damage.xuid() == xuid )
				{
					damageTo = damage.to();
					damageFrom = damage.from();
				}
			}
		}
		
		// add player info for this team
		vecPlayerInfo.AddToTail( CSurvivalEOMPlayerInfo{ xuid, bAlive, ( xuid == xuidLocalPlayer ), damageTo, damageFrom, teamIdx } );
	}

	FOR_EACH_VEC( vecTeamInfo, iTeam )
	{
		vecTeamOrder.AddToTail( &vecTeamInfo[iTeam] );
	}

	// sort teams in descending order by position
	vecTeamOrder.Sort( []( CSurvivalEOMTeamInfo* const *ppA, CSurvivalEOMTeamInfo* const *ppB ) -> int
	{
		const CSurvivalEOMTeamInfo* pA = *ppA;
		const CSurvivalEOMTeamInfo* pB = *ppB;

		if ( pB->position != pA->position )
			return pB->position - pA->position;		// teams with lower position# are ranked higher
		else if ( pB->numAlive != pA->numAlive )
			return -( pB->numAlive - pA->numAlive ); // teams with more live players are ranked higher
		else
			return int( pB - pA );
	} );

	// update ordering
	FOR_EACH_VEC( vecTeamOrder, iTeamOrder )
	{
		vecTeamOrder[iTeamOrder]->order = iTeamOrder;
	}

	v8::Local<v8::Array> pTeamInfos = v8::Array::New( pIsolate, vecTeamInfo.Count() );
	pObject->Set( pIsolate->GetCurrentContext(), v8::String::NewFromUtf8( pIsolate, "teams" ).ToLocalChecked(), pTeamInfos );

	v8::Local<v8::String> pStrPosition = v8::String::NewFromUtf8( pIsolate, "position" ).ToLocalChecked();
	v8::Local<v8::String> pStrPlayers = v8::String::NewFromUtf8( pIsolate, "players" ).ToLocalChecked();
	FOR_EACH_VEC( vecTeamOrder, iTeamOrder )
	{
		v8::HandleScope handleScope( pIsolate );
		const CSurvivalEOMTeamInfo& eomTeamInfo = *vecTeamOrder[iTeamOrder];

		v8::Local<v8::Object> pTeamInfo = v8::Object::New( pIsolate );
		pTeamInfo->Set( pIsolate->GetCurrentContext(), pStrPosition, v8::Integer::New( pIsolate, eomTeamInfo.position ) );

		// empty array for now, we'll add to it as we go over the players
		v8::Local<v8::Array> pPlayers = v8::Array::New( pIsolate );
		pTeamInfo->Set( pIsolate->GetCurrentContext(), pStrPlayers, pPlayers );

		pTeamInfos->Set( pIsolate->GetCurrentContext(), (uint32_t)iTeamOrder, pTeamInfo );
	}

	v8::Local<v8::String> pStrXuid = v8::String::NewFromUtf8( pIsolate, "xuid" ).ToLocalChecked();
	v8::Local<v8::String> pStrAlive = v8::String::NewFromUtf8( pIsolate, "alive" ).ToLocalChecked();
	v8::Local<v8::String> pStrDamageTo = v8::String::NewFromUtf8( pIsolate, "damage_to" ).ToLocalChecked();
	v8::Local<v8::String> pStrDamageFrom = v8::String::NewFromUtf8( pIsolate, "damage_from" ).ToLocalChecked();

	FOR_EACH_VEC( vecPlayerInfo, i )
	{
		v8::HandleScope handleScope( pIsolate );
		const CSurvivalEOMPlayerInfo& eomPlayerInfo = vecPlayerInfo[i];
		const CSurvivalEOMTeamInfo& eomTeamInfo = vecTeamInfo[eomPlayerInfo.teamIdx];

		v8::Local<v8::Object> pTeamInfo = pTeamInfos->Get( pIsolate->GetCurrentContext(), ( uint32_t )eomTeamInfo.order ).ToLocalChecked().As<v8::Object>();
		v8::Local<v8::Array> pPlayerInfos = pTeamInfo->Get( pIsolate->GetCurrentContext(), pStrPlayers ).ToLocalChecked().As<v8::Array>();

		v8::Local<v8::Object> pPlayerInfo = v8::Object::New( pIsolate );
		pPlayerInfo->Set( pIsolate->GetCurrentContext(), pStrXuid, v8::String::NewFromUtf8( pIsolate, CFmtStr( "%lld", eomPlayerInfo.xuid ) ).ToLocalChecked() );
		pPlayerInfo->Set( pIsolate->GetCurrentContext(), pStrAlive, v8::Boolean::New( pIsolate, eomPlayerInfo.alive ) );

		if ( eomPlayerInfo.damageTo || eomPlayerInfo.damageFrom )
		{
			pPlayerInfo->Set( pIsolate->GetCurrentContext(), pStrDamageTo, v8::Integer::New( pIsolate, eomPlayerInfo.damageTo ) );
			pPlayerInfo->Set( pIsolate->GetCurrentContext(), pStrDamageFrom, v8::Integer::New( pIsolate, eomPlayerInfo.damageFrom ) );
		}

		pPlayerInfos->Set( pIsolate->GetCurrentContext(), pPlayerInfos->Length(), pPlayerInfo );
	}

	return pObject;
}

void CCSGO_SurvivalEndOfMatch::SetPlayerInQueueButtonStates()
{
	if ( panorama::CButton* pQueueButton = m_pQueueButton.Get() )
		pQueueButton->SetEnabled( false );
	if ( panorama::CLabel *pLbl = m_pWarningLabel.Get() )
		pLbl->SetText( "#EOM_PlayAgain_Error_searching" );
	if ( panorama::CPanel2D *pPnl = m_pWarningContainer.Get() )
	{
		pPnl->RemoveClass( "warningColor" );
		pPnl->RemoveClass( "hidden" );
	}
}

void CCSGO_SurvivalEndOfMatch::UpdateButtonStates()
{
	m_flLastButtonUpdate = Plat_FloatTime();

	if ( panorama::CButton* pSpectateButton = m_pSpectateButton.Get() )
	{
		bool bEnableSpectate = ( CSGameRules() && CSGameRules()->GetGamePhase() != GAMEPHASE_MATCH_ENDED );
		pSpectateButton->SetEnabled( bEnableSpectate );
	}

	if ( IsInQueue() )
	{
		SetPlayerInQueueButtonStates();
	}
	else
	{
		char const *szSoloError = CanRequeueSoloError();
		if ( szSoloError && !V_strcmp( "na", szSoloError ) )
			szSoloError = CanRequeueDuosError();
		if ( szSoloError && !V_strcmp( "na", szSoloError ) )
			szSoloError = "notingame";

		// If we are alive and not in killer replay, can't requeue
		if ( !szSoloError && C_CSPlayer::GetLocalCSPlayer() && C_CSPlayer::GetLocalCSPlayer()->IsAlive() && !g_HltvReplaySystem.GetHltvReplayDelay() && !g_HltvReplaySystem.IsInPermanentReplay() )
			szSoloError = "squadalive";

		if ( panorama::CButton* pQueueButton = m_pQueueButton.Get() )
			pQueueButton->SetEnabled( !szSoloError );

		if ( szSoloError )
		{
			if ( panorama::CLabel *pLbl = m_pWarningLabel.Get() )
				pLbl->SetText( CFmtStr( "#EOM_PlayAgain_Error_%s", szSoloError ) );
			
			if ( panorama::CPanel2D *pPnl = m_pWarningContainer.Get() )
			{
				pPnl->AddClass( "warningColor" );
				pPnl->RemoveClass( "hidden" );
			}
		}
		else
		{
			if ( panorama::CLabel *pLbl = m_pWarningLabel.Get() )
				pLbl->SetText( "" );

			if ( panorama::CPanel2D *pPnl = m_pWarningContainer.Get() )
				pPnl->AddClass( "hidden" );
		}
	}
}

/*static*/ bool CCSGO_SurvivalEndOfMatch::IsInQueue()
{
	IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pIMatchSession ) return false;
	char const *szMMqueue = pIMatchSession->GetSessionSettings()->GetString( "game/mmqueue" );
	if ( !szMMqueue || !*szMMqueue ) return false;
	if ( !V_strcmp( szMMqueue, "connect" ) ) return false;
	return true;
}

/*static*/ char const * CCSGO_SurvivalEndOfMatch::CanRequeueSoloError()
{
	if ( !CSGameRules() ) return "notingame";
	if ( !CSGameRules()->GetSurvivalRules() ) return "notingame";
	if ( !CSGameRules()->GetSurvivalRules()->IsPlayingSoloMode() )
	{
		// Even when not playing in solo gameserver, the user may be solo
		// for the purposes of playing again
		if ( g_pMatchFramework->GetMatchSession() ) return "na";
		CUtlVector< CCSPlayer * > arrTeammates;
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		CSurvivalGameRules *pSGR = CSGameRules()->GetSurvivalRules();
		if ( pSGR && pLocalPlayer )
			pSGR->GetPlayerTeammates( ToCSPlayer( pLocalPlayer ), arrTeammates );
		if ( !arrTeammates.IsEmpty() ) return "na";
		// otherwise fall through and treat this user as a solo
	}
	if ( !CSGameRules()->IsQueuedMatchmaking() ) return "notingame";

	// if ( CUiComponent_Lobby::GetInstance()->IsSessionActive() ) return false; // Solo sessions are created from scratch every time, enforce it
	if ( g_pMatchFramework->GetMatchSession() ) return "inparty"; // Solo sessions are created from scratch every time, enforce it
	
	return NULL;
}

DEVELOPMENT_ONLY_CONVAR( debug_ccsgo_survivalendofmatch_partyqq_quick, 0 );
/*static*/ char const * CCSGO_SurvivalEndOfMatch::CanRequeueDuosError()
{
	if ( !CSGameRules() ) return "notingame";
	if ( !CSGameRules()->GetSurvivalRules() ) return "notingame";
	if ( !CSGameRules()->GetSurvivalRules()->IsPlayingTeamMode() ) return "na";

	if ( !debug_ccsgo_survivalendofmatch_partyqq_quick.GetBool() )
	{

	if ( !CSGameRules()->IsQueuedMatchmaking() ) return "notingame";
	
	if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
	{
		if ( !pMatchSession ) return "mustbeinparty"; // Duos always must have a session, enforce it

		// Must have all players in the session, and both players must be in the server and DEAD
		extern ConVar sv_dz_team_count; // number of players per team
		int numPlayersPerTeam = sv_dz_team_count.GetInt();
		if ( numPlayersPerTeam < 2 || numPlayersPerTeam > 5 ) return "na"; // safety checks

		numPlayersPerTeam = pMatchSession->GetSessionSettings()->GetInt( "members/numPlayers" );
		if ( numPlayersPerTeam < 2 ) return "mustbeinparty";
		if ( numPlayersPerTeam > 3 ) return "toomanyplayers";

		uint64 *ullPlayers = StackAlloc( uint64, numPlayersPerTeam );
		for ( int k = 0; k < numPlayersPerTeam; ++k )
			ullPlayers[ k ] = pMatchSession->GetSessionSettings()->GetUint64( CFmtStr( "members/machine%d/player0/xuid", k ) );

		AccountID_t *unAccountID = StackAlloc( AccountID_t, numPlayersPerTeam );
		for ( int k = 0; k < numPlayersPerTeam; ++k )
		{
			CSteamID steamID( ullPlayers[ k ] );
			if ( !steamID.IsValid() ) return "mustbeinparty";
			if ( !steamID.BIndividualAccount() ) return "mustbeinparty";
			unAccountID[ k ] = steamID.GetAccountID();
			if ( !unAccountID[ k ] ) return "mustbeinparty";

			for ( int j = 0; j < k; ++j )
			{
				if ( unAccountID[ j ] == unAccountID[ k ] ) return "mustbeinparty"; // humans must be different
			}
		}
	}

	}

	// Squad mates check
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer ) return "notingame";
	if ( pLocalPlayer->IsAlive() && CSGameRules()->m_iRoundWinStatus == WINNER_NONE ) return "squadalive";
	if ( CSurvivalGameRules *pSGR = CSGameRules()->GetSurvivalRules() )
	{
		CUtlVector< CCSPlayer * > arrTeammates;
		pSGR->GetPlayerTeammates( ToCSPlayer( pLocalPlayer ), arrTeammates );
		FOR_EACH_VEC( arrTeammates, iTM )
		{
			if ( arrTeammates[iTM]->IsAlive() && CSGameRules()->m_iRoundWinStatus == WINNER_NONE ) return "squadalive";
		}
		static ConVarRef spec_replay_enable( "spec_replay_enable" );
		static ConVarRef spec_replay_leadup_time( "spec_replay_leadup_time" );
		if ( spec_replay_enable.GetInt() == 2 && spec_replay_leadup_time.GetInt() > 0 )
		{
			FOR_EACH_VEC( arrTeammates, iTM )
			{
				if ( gpGlobals->curtime - arrTeammates[ iTM ]->GetDeathTime() <= spec_replay_leadup_time.GetInt()/2 ) return "squadstats";
			}
		}
	}
	
	// Looks like all checks passed, so good to go
	return NULL;
}

/*static*/ bool CCSGO_SurvivalEndOfMatch::RequestSurvivalEndOfMatchDisconnect( uint32 unValue )
{
	//
	// Send the user message to server
	// - it will come back to us and cause the disconnect
	//
	CSVCMsg_UserMessage_t um;
	CCSUsrMsg_DisconnectToLobby gameMsg;
	if ( unValue )
		gameMsg.set_dummy( unValue );

	bool bValid = BSerializeUserMessageToSVCMSG( um, CS_UM_DisconnectToLobby, gameMsg );
	if ( bValid )
		engine->SendMessageToServer( &um );
	return bValid;
}

bool CCSGO_SurvivalEndOfMatch::EventCSGOFrameUpdate()
{
	double flNow = Plat_FloatTime();
	if ( fabs( flNow - m_flLastButtonUpdate ) > 1.5 )
	{
		UpdateButtonStates();
	}
	return false;
}

bool CCSGO_SurvivalEndOfMatch::OnEndOfMatchSurvivalRequeueClicked()
{
	char const *szRequeueMethod = NULL;
	if ( !CanRequeueSoloError() )
		szRequeueMethod = "solo";
	else if ( !CanRequeueDuosError() )
		szRequeueMethod = "duos";

	if ( szRequeueMethod )
	{	// It's also possible that the other user disconnected already...
		IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
		if ( !pMatchSession )	// when playing duos with fill squad mode "Play Again" = solo
			szRequeueMethod = "solo";
//		CUiComponent_CompetitiveMatch::GetInstance()->ActionMatchmaking( "survival", engine->GetMapGroupName(), "search", szRequeueMethod );
		
		if ( cl_dz_playagain_auto_spectate.GetBool() )
		{
			// Start spectating.
			DispatchEvent( EndOfMatch_Survival_Spectate_Clicked(), this );
		}
	}
	
	// explain maybe that we cannot requeue because e.g. partner already left the game, etc.
	UpdateButtonStates();

	// we handled this event
	return true;
}


