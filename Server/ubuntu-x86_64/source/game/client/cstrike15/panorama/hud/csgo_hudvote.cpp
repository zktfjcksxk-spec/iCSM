//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Vote panel: displays current vote issue
//
//=============================================================================//

#include "cbase.h"
#include "csgo_hudvote.h"
#include "panorama/uievents.h"
#include "hud_macros.h"
#include "c_cs_player.h"
#include "gcsdk/enumutils.h"
#include "panorama/localization/ilocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;	

ENUMSTRINGS_START( vote_create_failed_t )
	{ VOTE_FAILED_GENERIC, "#SFUI_vote_failed" },
	{ VOTE_FAILED_TRANSITIONING_PLAYERS, "#SFUI_vote_failed_transition_vote" },
	{ VOTE_FAILED_RATE_EXCEEDED, "#Panorama_vote_failed_vote_spam" },
	{ VOTE_FAILED_YES_MUST_EXCEED_NO, "#SFUI_vote_failed_yesno" },
	{ VOTE_FAILED_QUORUM_FAILURE, "#SFUI_vote_failed_quorum" },
	{ VOTE_FAILED_ISSUE_DISABLED, "#SFUI_vote_failed_disabled_issue" },
	{ VOTE_FAILED_MAP_NOT_FOUND, "#SFUI_vote_failed_map_not_found" },
	{ VOTE_FAILED_MAP_NAME_REQUIRED, "#SFUI_vote_failed_map_name_required" },
	{ VOTE_FAILED_FAILED_RECENTLY, "#Panorama_vote_failed_recently" },
	{ VOTE_FAILED_FAILED_RECENT_KICK, "#Panorama_vote_failed_recent_kick" },
	{ VOTE_FAILED_FAILED_RECENT_CHANGEMAP, "#Panorama_vote_failed_recent_changemap" },
	{ VOTE_FAILED_FAILED_RECENT_SWAPTEAMS, "#Panorama_vote_failed_recent_swapteams" },
	{ VOTE_FAILED_FAILED_RECENT_SCRAMBLETEAMS, "#Panorama_vote_failed_recent_scrambleteams" },
	{ VOTE_FAILED_FAILED_RECENT_RESTART, "#Panorama_vote_failed_recent_restart" },
	{ VOTE_FAILED_TEAM_CANT_CALL, "#SFUI_vote_failed_team_cant_call" },
	{ VOTE_FAILED_WAITINGFORPLAYERS, "#SFUI_vote_failed_waitingforplayers" },
	{ VOTE_FAILED_PLAYERNOTFOUND, "#SFUI_vote_failed" },
	{ VOTE_FAILED_CANNOT_KICK_ADMIN, "#SFUI_vote_failed_cannot_kick_admin" },
	{ VOTE_FAILED_SCRAMBLE_IN_PROGRESS, "#SFUI_vote_failed_scramble_in_prog" },
	{ VOTE_FAILED_SWAP_IN_PROGRESS, "#SFUI_vote_failed_swap_in_prog" },
	{ VOTE_FAILED_SPECTATOR, "#SFUI_vote_failed_spectator" },
	{ VOTE_FAILED_DISABLED, "#SFUI_vote_failed_disabled" },
	{ VOTE_FAILED_NEXTLEVEL_SET, "#SFUI_vote_failed_nextlevel_set" },
	{ VOTE_FAILED_REMATCH, "#SFUI_vote_failed_rematch" },
	{ VOTE_FAILED_TOO_EARLY_SURRENDER, "#SFUI_vote_failed_surrender_too_early" },
	{ VOTE_FAILED_CONTINUE, "#SFUI_vote_failed_continue" },
	{ VOTE_FAILED_MATCH_PAUSED, "#SFUI_vote_failed_paused" },
	{ VOTE_FAILED_MATCH_NOT_PAUSED, "#SFUI_vote_failed_not_paused" },
	{ VOTE_FAILED_NOT_IN_WARMUP, "#SFUI_vote_failed_not_in_warmup" },
	{ VOTE_FAILED_NOT_10_PLAYERS, "#SFUI_vote_failed_not_10_players" },
	{ VOTE_FAILED_TIMEOUT_ACTIVE, "#SFUI_vote_failed_timeout_active" },
	{ VOTE_FAILED_TIMEOUT_INACTIVE, "#SFUI_vote_failed" },
	{ VOTE_FAILED_TIMEOUT_EXHAUSTED, "#SFUI_vote_failed_timeouts_exhausted" },
	{ VOTE_FAILED_CANT_ROUND_END, "#SFUI_vote_failed_cant_round_end" },
	{ VOTE_FAILED_MAX, "#SFUI_vote_failed" },
ENUMSTRINGS_END( vote_create_failed_t )

const char* Helper_Localize( const char* szToken )
{
	return UILocalize()->PchFindRawString( szToken );
}

bool Helper_IsVoteMapRelated( int iVoteIssueType )
{
	switch ( iVoteIssueType )
	{
		case VOTEISSUE_CHANGELEVEL:
		case VOTEISSUE_NEXTLEVEL:
			return true;
		default:
			return false;
	}
}

REGISTER_PANEL2D_FACTORY( CCSGO_HudVote, CSGOHudVote );

DECLARE_HUD_MESSAGE( CCSGO_HudVote, CallVoteFailed );
DECLARE_HUD_MESSAGE( CCSGO_HudVote, VoteStart );
DECLARE_HUD_MESSAGE( CCSGO_HudVote, VotePass );
DECLARE_HUD_MESSAGE( CCSGO_HudVote, VoteFailed );
DECLARE_HUD_MESSAGE( CCSGO_HudVote, VoteSetup );

#if defined ( CSTRIKE_TRUNK_BUILD )
DECLARE_PANORAMA_EVENT1( DbgTestHudVote, const char* );
DEFINE_PANORAMA_EVENT_DOC( DbgTestHudVote, "string: test type", "Internal testing event." );
#endif

DEFINE_PANORAMA_EVENT_DOC( PanoramaCastVoteYes, "", "Cast a YES vote for the currently active vote issue" );
DEFINE_PANORAMA_EVENT_DOC( PanoramaCastVoteNo, "", "Cast a NO vote for the currently active vote issue" );

DEFINE_PANORAMA_EVENT_DOC( ShowVoteContextMenu, "", "Show vote context menu in pause menu" );

static ConVar cl_drawhud_specvote( "cl_drawhud_specvote", "1", FCVAR_RELEASE, "1: default; 0: disables vote UI for spectators" );

CCSGO_HudVote::CCSGO_HudVote( CPanel2D *pParent, const char *pchID )
	: CPanoramaHudElement( "CCSGO_HudVote", this ),
	CPanel2D( pParent, pchID )
{
	RequireLoadLayout( "file://{resources}/layout/hud/hudvote.xml" );
	m_pVoteHeaderIcon	= panel_cast< CImagePanel*> ( FindChildInLayoutFile( "VoteHeaderIcon" ) );
	m_pVoteDescription	= panel_cast< CLabel* > ( FindChildInLayoutFile( "VoteDescLabel" ) );

	SetDialogVariable( "yes_votes", 0 );
	SetDialogVariable( "no_votes", 0 );

	SetInputNamespace( "vote_issue_active" );

	if ( GameUI().IsPanoramaEnabled() )
	{
		ListenForGameEvent( "vote_changed" );
		//ListenForGameEvent( "vote_options" );
		ListenForGameEvent( "vote_cast" );

		HOOK_HUD_MESSAGE( CCSGO_HudVote, CallVoteFailed );
		HOOK_HUD_MESSAGE( CCSGO_HudVote, VoteStart );
		HOOK_HUD_MESSAGE( CCSGO_HudVote, VotePass );
		HOOK_HUD_MESSAGE( CCSGO_HudVote, VoteFailed );
		HOOK_HUD_MESSAGE( CCSGO_HudVote, VoteSetup );

		RegisterEventHandler( AnimationEnd(), this, &CCSGO_HudVote::EventAnimationEnd );
		RegisterEventHandler( PanoramaCastVoteYes(), this, &CCSGO_HudVote::EventCastVoteYes );
		RegisterEventHandler( PanoramaCastVoteNo(), this, &CCSGO_HudVote::EventCastVoteNo );

#if defined ( CSTRIKE_TRUNK_BUILD )
		RegisterForUnhandledEvent( DbgTestHudVote(), this, &CCSGO_HudVote::Event_DbgTestHudVote );
#endif
	}
}
CCSGO_HudVote::~CCSGO_HudVote()
{
	if ( UIInputEngine()->GetInputCapture().HasElement( this ) )
		UIInputEngine()->ReleaseInputCapture( this );
}

void CCSGO_HudVote::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "BuildDebugPanel", PANORAMA_DELEGATE( &CCSGO_HudVote::BuildDebugPanel ) );
}

// LEGACY BADNESS: We have always swallowed f1/f2 for voting. 
bool CCSGO_HudVote::OnCapturedKeyDown( panorama::IUIPanel *pPanel, const panorama::KeyData_t &code )
{
	if ( code.m_eSource == k_ePanelEventSourceKeyboard )
	{
		if ( code.m_KeyCode == KeyCode::KEY_F1 )
		{
			//engine->ClientCmd( "vote option1" );
			DispatchEvent( PanoramaCastVoteYes(), this );
			return true;
		}
		else if ( code.m_KeyCode == KeyCode::KEY_F2 )
		{
			//engine->ClientCmd( "vote option2" );
			DispatchEvent( PanoramaCastVoteNo(), this );
			return true;
		}
	}
	return false;
}

void CCSGO_HudVote::BuildDebugPanel( panorama::CPanel2D* pPanel )
{
	pPanel->BCreateChildren( R"XML( 
	<Panel class="top-bottom-flow">
		<Button onactivate="$.DispatchEvent('DbgTestHudVote', 'callvote' )">
			<Label text="Call Vote"/>
		</Button>
		<Button onactivate="$.DispatchEvent('DbgTestHudVote', 'vote_yes' )">
			<Label text="Vote Yes"/>
		</Button>
		<Button onactivate="$.DispatchEvent('DbgTestHudVote', 'vote_no' )">
			<Label text="Vote No"/>
		</Button>
		<Button onactivate="$.DispatchEvent('DbgTestHudVote', 'pass' )">
			<Label text="Vote Passes"/>
		</Button>
		<Button onactivate="$.DispatchEvent('DbgTestHudVote', 'fail' )">
			<Label text="Vote Fails"/>
		</Button>
		<Button onactivate="$.DispatchEvent('DbgTestHudVote', 'callvotefailed' )">
			<Label text="Error Message"/>
		</Button>
		<Button onactivate="$.DispatchEvent('DbgTestHudVote', 'longstrings' )">
			<Label text="Long Strings"/>
		</Button>
	</Panel>
	)XML" );
}

bool CCSGO_HudVote::Event_DbgTestHudVote( const char* szTestString )
{
	static CPanoramaSymbol k_symCallVoteFailed( "hud-vote--callvotefailed" );
	SetHasClass( k_symCallVoteFailed, false );

	if ( FStrEq( szTestString, "callvote" ) )
	{
		CCSUsrMsg_VoteStart msg;
		msg.set_ent_idx( 99 );
		msg.set_team( -1 );
		msg.set_vote_type( VOTEISSUE_CHANGELEVEL );
		msg.set_disp_str( "#SFUI_vote_changelevel" );
		msg.set_details_str( "de_inferno" );
		msg.set_other_team_str( "#SFUI_otherteam_vote_unimplemented" );
		msg.set_is_yes_no_vote( true );
		SetDialogVariable( "yes_votes", 0 );
		SetDialogVariable( "no_votes", 0 );
		MsgFunc_VoteStart( msg );
	}
	else if ( FStrEq( szTestString, "vote_yes" ) )
	{
		static CPanoramaSymbol k_symVoteCast( "hud-vote--cast" );
		SetHasClass( k_symVoteCast, true );
		SetDialogVariable( "yes_votes", 1 );
		SetDialogVariable( "player_vote", Helper_Localize( "#Panorama_Vote_Yes" ) );
	}
	else if ( FStrEq( szTestString, "vote_no" ) )
	{
		static CPanoramaSymbol k_symVoteCast( "hud-vote--cast" );
		SetHasClass( k_symVoteCast, true );
		SetDialogVariable( "no_votes", 1 );
		SetDialogVariable( "player_vote", Helper_Localize( "#Panorama_Vote_No" ) );
	}
	else if ( FStrEq( szTestString, "pass" ) )
	{
		CCSUsrMsg_VotePass msg;
		msg.set_team( -1 );
		msg.set_vote_type( VOTEISSUE_CHANGELEVEL );
		msg.set_disp_str( "#SFUI_vote_passed_changelevel" );
		msg.set_details_str( "de_inferno" );
		MsgFunc_VotePass(  msg );
	}
	else if ( FStrEq( szTestString, "fail" ) )
	{
		CCSUsrMsg_VoteFailed msg;
		msg.set_team( -1 );
		msg.set_reason( VOTE_FAILED_YES_MUST_EXCEED_NO );
		MsgFunc_VoteFailed( msg );
	}
	else if ( FStrEq( szTestString, "callvotefailed" ) )
	{
		SetHasClass( k_symCallVoteFailed, true );

		CCSUsrMsg_CallVoteFailed msg;
		msg.set_time( 234 );
		msg.set_reason( RandomInt( 0, VOTE_FAILED_MAX - 1 ) );
		MsgFunc_CallVoteFailed( msg );
	}
	else if ( FStrEq( szTestString, "longstrings" ) )
	{
		if ( m_pVoteDescription.Get() )
		{
			static CUtlStringBuilder strLongName;
			if ( strLongName.Length() == 0 )
				strLongName.AppendRepeat( 'X', k_cchPersonaNameMax );
			SetDialogVariable( "vote_caller", strLongName.Get() );
			SetDialogVariable( "s1", strLongName.Get() );
			CUtlStringBuilder strTest;
			strTest.Append( Helper_Localize( "#SFUI_vote_changelevel" ) );
			strTest.Append( Helper_Localize( "#SFUI_vote_nextlevel" ) );
			strTest.Append( Helper_Localize( "#SFUI_vote_kick_player_cheating" ) );
			strTest.Append( Helper_Localize( "#SFUI_vote_scramble_teams" ) );
			strTest.Append( Helper_Localize( "#SFUI_vote_kick_player_scamming" ) );
			strTest.Append( Helper_Localize( "#SFUI_vote_kick_player_idle" ) );
			m_pVoteDescription->SetText( strTest.Get() );
		}
	}
	return false;
}


void CCSGO_HudVote::FireGameEvent( IGameEvent* pEvent )
{
	const char *szEventName = pEvent->GetName();
	if ( FStrEq( szEventName, "vote_changed" ) )
	{
		// Fired by vote controller when the number of vote options ( eg yes, no ) changes or if the vote type ( ie Change Map vs Kick Player ) changes. 
		//int iPotentialVotes = pEvent->GetInt( "potentialVotes", 0 ) == 2;
		SetDialogVariable( "yes_votes", pEvent->GetInt( "vote_option1", 0 ) );
		SetDialogVariable( "no_votes", pEvent->GetInt( "vote_option2", 0 ) );
	}
	/*
	else if ( FStrEq( szEventName, "vote_options" ) )
	{

	}
	*/
	else if ( FStrEq( szEventName, "vote_cast" ) )
	{
		int iEntIdx = pEvent->GetInt( "entityid", 0 );
		if ( C_CSPlayer::GetLocalCSPlayer() && iEntIdx == C_CSPlayer::GetLocalCSPlayer()->entindex() )
		{
			int iVoteOption = pEvent->GetInt( "vote_option", 0 );
			if ( iVoteOption == VOTE_OPTION1 || iVoteOption == VOTE_OPTION2 )
			{
				static CPanoramaSymbol k_symVoteCast( "hud-vote--cast" );
				SetHasClass( k_symVoteCast, true );
				if ( iVoteOption == VOTE_OPTION1 )
					SetDialogVariable( "player_vote", Helper_Localize( "#Panorama_Vote_Yes" ) );
				else if ( iVoteOption == VOTE_OPTION2 )
					SetDialogVariable( "player_vote", Helper_Localize( "#Panorama_Vote_No" ) );
				else
					AssertMsg( 0, "Only yes/no votes currently supported." );
			}
		}
	}
}

// Called when the local player tries to call a vote but fails. 
bool CCSGO_HudVote::MsgFunc_CallVoteFailed( const CCSUsrMsg_CallVoteFailed &msg )
{
	vote_create_failed_t nReason = (vote_create_failed_t)msg.reason();
	int nCooldownTimeRemaining = msg.time();

	// LEGACY: Strings we display due to CallVoteFailed use s1 as time.
	SetDialogVariable( "num_second", nCooldownTimeRemaining );
	UpdateShared( k_eVoteCallFailed, PchNameFromvote_create_failed_t( nReason ) );

	return true;
}

// Called when a vote fails to pass, sent to all players
bool CCSGO_HudVote::MsgFunc_VoteFailed( const CCSUsrMsg_VoteFailed &msg )
{
	vote_create_failed_t nReason = (vote_create_failed_t)msg.reason();
	UpdateShared( k_eVoteFailed, PchNameFromvote_create_failed_t( nReason ) );
	return true;
}

bool Helper_IsPlayingAnonymousDemo( void )
{
	if ( CDemoPlaybackParameters_t const *pParams = engine->GetDemoPlaybackParameters() )
	{
		if ( pParams->m_bAnonymousPlayerIdentity )
			return true;
	}

	return false;
}

bool CCSGO_HudVote::MsgFunc_VoteStart( const CCSUsrMsg_VoteStart &msg )
{
	// Set style if this isn't for our team
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	bool bAllowedToVote = ( pLocalPlayer && ( msg.team() == pLocalPlayer->GetTeamNumber() ) || ( msg.team() == TEAM_INVALID ) );
	if ( !bAllowedToVote )
	{
		//const char *szOtherTeam = msg.other_team_str().c_str();
		// FIXME: Old behavior was to send a chat line about it with this string as param
		return true;
	}

	// Set vote caller name
	int iEntityCallingVote = msg.ent_idx();
	C_BasePlayer *pVoteCaller = UTIL_PlayerByIndex( iEntityCallingVote );
	if ( pVoteCaller && !Helper_IsPlayingAnonymousDemo() )
		SetDialogVariable( "vote_caller", pVoteCaller->GetPlayerName() );
	else
	{
		SetDialogVariable( "vote_caller", Helper_Localize( "#Panorama_Vote_Server" ) );
	}

	const char *szIssue = msg.disp_str().c_str();
	const char *szParam1 = msg.details_str().c_str();
	UpdateShared( k_eVoteStart, szIssue, msg.vote_type(), szParam1 );


	AssertMsg( msg.is_yes_no_vote(), "Hud vote panel currently only supports yes/no votes." );

	return true;
}

bool CCSGO_HudVote::MsgFunc_VotePass( const CCSUsrMsg_VotePass &msg )
{
	const char *szResult = msg.disp_str().c_str();
	const char *szParam1 = msg.details_str().c_str();
	UpdateShared( k_eVotePassed, szResult, msg.vote_type(), szParam1 );
	return true;
}

// Currently only used to stitch together svg filenames, so it's a partial list
const char* Helper_VoteIssueToCmd( int iVoteIssue )
{
	switch ( iVoteIssue )
	{
		case VOTEISSUE_KICK:
			return "kick";
		case VOTEISSUE_CHANGELEVEL:
			return "changelevel";
		case VOTEISSUE_SURRENDER:
			return "surrender";
		case VOTEISSUE_PAUSEMATCH:
			return "pausematch";
		case VOTEISSUE_UNPAUSEMATCH:
			return "unpausematch";
		case VOTEISSUE_LOADBACKUP:
			return "loadbackup";
		case VOTEISSUE_READYFORMATCH:
			return "readyformatch";
		case VOTEISSUE_NOTREADYFORMATCH:
			return "notreadyformatch";
		case VOTEISSUE_STARTTIMEOUT:
			return "starttimeout";
	}
	return nullptr;
}

void CCSGO_HudVote::UpdateShared( EVoteEvent eventType, const char* szDisplayString, int nVoteType /*=-1*/, const char* szParam /*=nullptr*/ )
{
	static CPanoramaSymbol k_symVoteFailed( "hud-vote--failed" );
	static CPanoramaSymbol k_symVoteCast(	"hud-vote--cast" );
	static CPanoramaSymbol k_symVotePassed( "hud-vote--passed" );
	static CPanoramaSymbol k_symVoteActive( "hud-vote--active" );
	static CPanoramaSymbol k_symCallVoteFailed( "hud-vote--callvotefailed" );

	if ( ( eventType != k_eVoteEnd ) && !cl_drawhud_specvote.GetBool() )
	{
		//
		// Allow spectators to opt out of all vote UI
		// The only flow currently for vote UI to appear on scren is to get hud-vote--active style
		// and that only happens for k_eVoteStart event. When spectating with vote UI disable,
		// ignore vote start event and force it look like vote end event to release any possible
		// capture, etc.
		//

		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		int nLocalPlayerTeam = pLocalPlayer ? pLocalPlayer->GetAssociatedTeamNumber() : TEAM_UNASSIGNED;
		bool bSpec = !pLocalPlayer || engine->IsHLTV() || ( nLocalPlayerTeam != TEAM_CT && nLocalPlayerTeam != TEAM_TERRORIST );
		if ( bSpec )
			eventType = k_eVoteEnd;
	}

	switch ( eventType )
	{
		case k_eVoteFailed:
			AddClass( k_symVoteFailed );
			break;

		case k_eVotePassed:
			AddClass( k_symVotePassed );
			break;

		case k_eVoteStart:
		case k_eVoteCallFailed:
		{
			RemoveClass( k_symVoteCast );
			RemoveClass( k_symVotePassed );
			SetHasClass( k_symVoteFailed, eventType == k_eVoteCallFailed );
			SetHasClass( k_symCallVoteFailed, eventType == k_eVoteCallFailed );
			AddClass( k_symVoteActive );
			SetTopOfInputContext( true );
		};
		break;
		case k_eVoteEnd:
		{
			RemoveClass( k_symVoteCast );
			RemoveClass( k_symVotePassed );
			RemoveClass( k_symVoteFailed );
			RemoveClass( k_symVoteActive );

		}
		break;
	}

	// Swallow f1/f2 to cast votes when first shown
	if ( eventType == k_eVoteStart )
	{
		UIInputEngine()->SetInputCapture( this );
	}
	else if ( UIInputEngine()->GetInputCapture().Find( this ) != UIInputEngine()->GetInputCapture().InvalidIndex() )
	{
		UIInputEngine()->ReleaseInputCapture( this );
	}


	if ( m_pVoteDescription.Get() && szDisplayString )
	{
		m_pVoteDescription->SetText( szDisplayString );
	}
	if ( m_pVoteHeaderIcon.Get() && nVoteType != VOTEISSUE_UNDEFINED )
	{
		if ( const char* szCmd = Helper_VoteIssueToCmd( nVoteType ) )
		{
			m_pVoteHeaderIcon->SetImage( CFmtStr( "file://{images}/icons/ui/vote%s.svg", szCmd ).Get() );
		}
	}

	if ( szParam )
	{
		// HACK: Some votes send the short map name (ie base filename) and expect client to turn that into a loc string, if available. 
		char szBuff[ 256 ];
		if ( Helper_IsVoteMapRelated( nVoteType ) && CCSGameRules::GetFriendlyMapNameToken( szParam, szBuff, sizeof( szBuff ) ) )
		{
			SetDialogVariable( "s1", Helper_Localize( szBuff ) );
		}
		else
		{
			const char *pszLoc = Helper_Localize( szParam );
			SetDialogVariable( "s1", pszLoc ? pszLoc : szParam );
		}
	}
}

bool CCSGO_HudVote::EventAnimationEnd( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::CPanoramaSymbol symAnimation )
{
	static const CPanoramaSymbol k_symhudVoteFinished( "hudVoteFinished" );
	if ( symAnimation == k_symhudVoteFinished )
	{
		UpdateShared( k_eVoteEnd );
	}

	return false;
}

bool CCSGO_HudVote::EventCastVoteYes( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	engine->ClientCmd( "vote option1" );
	UIInputEngine()->ReleaseInputCapture( this );
	return true;
}

bool CCSGO_HudVote::EventCastVoteNo( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	engine->ClientCmd( "vote option2" );
	UIInputEngine()->ReleaseInputCapture( this );
	return true;
}

bool CCSGO_HudVote::MsgFunc_VoteSetup( const CCSUsrMsg_VoteSetup &msg )
{
	// if we're in game and need to setup vote
	// bring up pause menu and show vote context menu
	if ( GameUI().GetGameUIState() == CSGO_GAME_UI_STATE_INGAME )
	{
		GameUI().ChangeGameUIState( CSGO_GAME_UI_STATE_PAUSEMENU );
		
		panorama::DispatchEvent( ShowVoteContextMenu(), nullptr );
	}

	return true;
}

