//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Matchmaking status dialog interface class,
//          and SF/Panorama implementations
//
//=============================================================================//

#include <cbase.h>
#include "gameui_matchmakingstatus.h"
#include "gameui_interface.h"
#include "gameui/basepanel.h"
#include "gameui/modinfo.h"
#include "matchmaking/imatchframework.h"
//#include "uicomponents/uicomponent_friendslist.h"

#ifdef PANORAMA_ENABLE
#include "panorama/controls/label.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

/**************************************************************************
* Matchmaking status dialog interface class
**************************************************************************/

IMatchmakingStatus::IMatchmakingStatus()
{ 
	m_bErrorEncountered = false;
	m_dblTimeToAutoCancel = 0;

	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
}

IMatchmakingStatus::~IMatchmakingStatus() 
{ 
	if ( g_pMatchFramework )
		g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
}

void IMatchmakingStatus::SetTimeToAutoCancel( double dblPlatFloatTime )
{
	m_dblTimeToAutoCancel = dblPlatFloatTime;
}

void IMatchmakingStatus::OnEvent( KeyValues *pEvent )
{
	char pszMessageBuffer[1024];

	char const *pszEvent = pEvent->GetName();

	if( !V_stricmp( "OnEngineLevelLoadingStarted", pszEvent ) || !V_stricmp( "LoadingScreenOpened", pszEvent ) )
	{
		Hide();
		return;
	}
	else if( !Q_stricmp( "OnMatchSessionUpdate", pszEvent ) )
	{
		char const *pszState = pEvent->GetString( "state" );
		DevMsg( "Matchmaking Status State = %s\n", pszState );

		char const *pszMessage = NULL;

		if( !Q_stricmp( pszState, "progress" ) )
		{
			char const *pszDetails = pEvent->GetString( pszState );
			DevMsg( "Matchmaking Status Details = %s\n", pszDetails );
			if( !Q_stricmp( pszDetails, "searching" ) )
			{
				pszMessage = "#SFUI_MMStatus_Searching";
			}
			else if( !Q_stricmp( pszDetails, "creating" ) )
			{
				pszMessage = "#SFUI_MMStatus_Creating";
			}
			else if( !Q_stricmp( pszDetails, "matchdldownloading" ) )
			{
				pszMessage = "#SFUI_GameUI_MatchDlDownloading";
			}
			else if( !Q_stricmp( pszDetails, "gotvrelaystart" ) )
			{
				pszMessage = "#SFUI_MMStatus_GOTVRelayStart";
			}
			else if( !Q_stricmp( pszDetails, "gotvrelaystarting" ) )
			{
				pszMessage = "#SFUI_MMStatus_GOTVRelayStarting";
			}
			else if( !Q_stricmp( pszDetails, "searchresult" ) )
			{
				// We will wait for the user to dismiss the error, or a level load to commence...
			}
			else if( !Q_stricmp( pszDetails, "searchempty" ) )
			{
				if( !Q_stricmp( ((KeyValues *)pEvent->GetPtr( "settingsptr" ))->GetString( "options/searchempty" ), "close" ) )
				{
					pszMessage = "#SFUI_MMStatus_JoinFailed";
				}
			}
		}
		else if( !Q_stricmp( pszState, "created" ) )
		{
			pszMessage = "#SFUI_MMStatus_Creating";
		}
		else if( !Q_stricmp( pszState, "ready" ) )
		{
			pszMessage = "#SFUI_MMStatus_Joining";
		}
		else if( !Q_stricmp( pszState, "closed" ) )
		{
			// We will wait for the user to dismiss the error, or a level load to commence...
		}
		else if( !Q_stricmp( pszState, "error" ) )
		{
			char const *pszErrorDetails = pEvent->GetString( pszState );
			DevMsg( "Matchmaking Error Details = %s\n", pszErrorDetails );

			m_bErrorEncountered = true;

			if( !Q_stricmp( pszErrorDetails, "connect" ) )
			{
				const wchar_t *pLocString = g_pVGuiLocalize->Find( "#SFUI_GameUI_LostServerXLSP" );

				char ansiLocString[1024];
				g_pVGuiLocalize->ConvertUnicodeToANSI( pLocString, ansiLocString, sizeof( ansiLocString ) );

				Q_snprintf( pszMessageBuffer, sizeof( pszMessageBuffer ), ansiLocString, ModInfo().GetGameName() );
				pszMessage = pszMessageBuffer;
			}
			else if( !Q_stricmp( pszErrorDetails, "nomap" ) )
			{
				pszMessage = "#SFUI_GameUI_DedicatedSearchFailed";
			}
			else
			{
				pszMessage = "#SFUI_MMStatus_JoinFailed";
			}
		}

		if( pszMessage )
		{
			SetMessage( pszMessage );
		}
	}

	if( m_dblTimeToAutoCancel && (Plat_FloatTime() > m_dblTimeToAutoCancel) )
	{
		DevMsg( "Matchmaking Status State = %s\n", "Auto Cancel" );
		SetMessage( "#SFUI_MMStatus_JoinFailed" );
		m_bErrorEncountered = true;
// 		extern XUID g_xuidFriendWatchSessionJoiningXUID;
// 		g_xuidFriendWatchSessionJoiningXUID = 0ull; // if we were requesting join info then null out the pending join
	}
}

// OnCancel - cancels pending matchmaking requests
void IMatchmakingStatus::OnCancel()
{
	// Kill GOTV theater mode
// 	extern ConVar gotv_theater_container;
// 	gotv_theater_container.SetValue( "" );

	// Kill pending retrying TV requests
//	g_pFriendsListWatchableFriendsInfo->CancelPendingRequest();

	// Flag this so we remember to restore the main menu when we go away
	m_bErrorEncountered = true;

	if( BasePanel()->InTeamLobby() )
	{
		// Cancel any pending queries, but DON'T close the session which terminates the lobby too
		if( g_pMatchFramework && g_pMatchFramework->GetMatchSession() )
		{
			g_pMatchFramework->GetMatchSession()->Command(
				KeyValues::AutoDeleteInline( new KeyValues( "Cancel", "run", "host" ) ) );
		}
	}
	else
	{
		// Cancel any outstanding session searches.
		g_pMatchFramework->CloseSession();
	}
}


/**************************************************************************
* Panorama implementation
**************************************************************************/

#ifdef PANORAMA_ENABLE

CUI_MMStatus_Popup::CUI_MMStatus_Popup( panorama::CPanel2D *pParent, const char *pchID, CPanel2D *pEventParent, char const *szCustomTitle, char const *szCustomText )
	: CUI_Popup_Generic( pParent, pchID, pEventParent )
{
	SetDisplayCancel( szCustomTitle, szCustomText, NULL );
	SetSpinnerVisible( true );
	m_pMessageLabel = panorama::panel_cast<panorama::CLabel *>(FindChildInLayoutFile( "MessageLabel" ));
}

void CUI_MMStatus_Popup::Hide()
{ 
	CloseAndDeleteAsync( 0.0f ); 
}

void CUI_MMStatus_Popup::SetMessage( const char *pszMessage )
{
	if( m_pMessageLabel ) m_pMessageLabel->SetText( pszMessage );
}

void CUI_MMStatus_Popup::HandlePopupButtonClicked( const char *pchEventText )
{
	OnCancel();
	CloseAndDeleteAsync( 0.0f );
}

#endif


/**************************************************************************
* Scaleform implementation
**************************************************************************/

#ifdef INCLUDE_SCALEFORM

CMatchmakingStatus::CMatchmakingStatus( char const *szCustomTitle, char const *szCustomText )
{
	m_pMessageBoxInstance = NULL;
	CMessageBoxScaleform::LoadDialog( szCustomTitle, szCustomText, "#SFUI_MMStatus_Legend", MESSAGEBOX_FLAG_CANCEL | MESSAGEBOX_FLAG_BOX_CLOSED, this, &m_pMessageBoxInstance );
}

CMatchmakingStatus::~CMatchmakingStatus()
{
	if( m_bErrorEncountered )
	{
		// Restore the appropriate menu now
		if( GameUI().IsInLevel() )
		{
			BasePanel()->RestorePauseMenu();
		}
		else
		{
			BasePanel()->RestoreMainMenuScreen();
		}
	}
}


bool CMatchmakingStatus::OnMessageBoxEvent( MessageBoxFlags_t buttonPressed )
{
// 	extern XUID g_xuidFriendWatchSessionJoiningXUID;
// 	g_xuidFriendWatchSessionJoiningXUID = 0ull; // if we were requesting join info then null out the pending join

	if( buttonPressed & MESSAGEBOX_FLAG_CANCEL )
	{
		OnCancel();

		// Returning true will tell the message box to close itself.
		return true;
	}
	else if( buttonPressed & MESSAGEBOX_FLAG_BOX_CLOSED )
	{
		// Sever the connection to the message box and free ourself
		m_pMessageBoxInstance = NULL;
		delete this;

		// No need to return true here - we're already closing the message box
	}

	// Do not tell the owner message box to close itself
	return false;
}

void CMatchmakingStatus::Hide()
{
	if( m_pMessageBoxInstance && m_pMessageBoxInstance->IsReady() )
	{
		m_pMessageBoxInstance->HideImmediate();
		m_pMessageBoxInstance = NULL;
		// The message box will release us when it is finally closed
	}
}

void CMatchmakingStatus::SetMessage( const char *pszMessage )
{
	if( m_pMessageBoxInstance && m_pMessageBoxInstance->IsReady() )
	{
		m_pMessageBoxInstance->SetMessage( pszMessage );
	}
}

#endif

