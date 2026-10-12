//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "csgo_hudvoicestatus.h"
#include "c_cs_playerresource.h"
#include "c_cs_player.h"
#include "utlvector.h"
#include "vgui/ILocalize.h"
#include "voice_status.h"
#include "hud_basechat.h"
#include "cs_hud_chat.h"
#include "engineinterface.h"
#include "modinfo.h"
#include "gameui_interface.h"
#include "gameui_util.h"
#include "hud_macros.h"
#include "csgo_hudchat.h"
#include "usermessages.h"

//#include "gc_clientsystem.h"
#include "cstrike15_gcmessages.pb.h"
#include "bannedwords.h"
#include "c_cs_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

DECLARE_HUD_MESSAGE(CCSGO_HudVoiceStatus, RadioText);
DECLARE_HUD_MESSAGE(CCSGO_HudVoiceStatus, SayText);
DECLARE_HUD_MESSAGE(CCSGO_HudVoiceStatus, SayText2);
DECLARE_HUD_MESSAGE(CCSGO_HudVoiceStatus, TextMsg);
DECLARE_HUD_MESSAGE(CCSGO_HudVoiceStatus, RawAudio);

REGISTER_PANEL2D_FACTORY(CCSGO_HudVoiceStatus, CSGOHudVoiceStatus);
REGISTER_PANEL2D(CCSGO_HudNotice, CSGOHudNotice);
REGISTER_PANEL2D( CCSGO_VoiceNotice, CSGOVoiceNotice);

using namespace panorama;

extern ConVar cl_draw_only_deathnotices;
extern ConVar voice_all_icons;

static const float NOTICE_UPDATE_INTERVAL = 0.05f;

#define HEX_COLOR_CHAR_SIZE 8
#define VOICE_NOTICE_TEXT_MAX		1024 // max number of characters in a voice status text
#define ALERT_NOTICE_FONT_STRING	"<font color=\"%s\">%s</font>"
#define ALERT_NOTICE_FONT_COLORCODE_STRING	"#%s"
#define ALERT_NOTICE_FONT_TEAMMATE_STRING	"<font color=\"%s\">%s </font><font color=\"%s\">%s</font>"
#define VOICE_NOTICE_LOCATION	"%s @ %s"

#define ALERT_NOTICE_CACHE_SIZE 16

#ifdef _WIN32

static bool s_bRunningVoiceStatusTest = false;
static int s_nVoiceStatusTest = 0;
static bool s_bIncludeChineseChars = false;
static uintp VoiceStatusTest(void* pData)
{
	while ( s_bRunningVoiceStatusTest )
	{
		bool bTeamMsg = false;
		bool bIME = s_bIncludeChineseChars;
		int nChars = 10;

		switch( s_nVoiceStatusTest )
		{
		case 1:
			nChars = 80;
			break;
		case 2:
			bTeamMsg = true;
			nChars = 100;
			break;
		case 3:
			bIME = false;
			break;
		default:
			break;
		}
		++s_nVoiceStatusTest;
		if( s_nVoiceStatusTest == 4 )
		{
			s_nVoiceStatusTest = 0;
		}

		char pCmd[1024];

		if( bTeamMsg )
		{
			V_strcpy( pCmd, "say_team \"" );
		}
		else
		{
			V_strcpy( pCmd, "say \"" );
		}

		int wordLength = 0;
		for( int i = 0; i < nChars; ++i )
		{
			wordLength++;
			// Break up into random length words
			if( wordLength > 3 && (RandomInt( 0, 3 ) == 3) )
			{
				V_strncat( pCmd, " ", SIZE_OF_ARRAY(pCmd) );
				wordLength = 0;
			}
			else
			{
				char pChar[4];
				if( bIME )
				{
					uchar32 chineseChar = RandomInt( 0x4E00, 0x9FFF );
					int byteLength = V_UChar32ToUTF8( chineseChar, pChar );
					pChar[byteLength] = 0;
				}
				else
				{
					pChar[0] = RandomInt( 0x21, 0x7E );
					pChar[1] = 0;
				}
				V_strcat( pCmd, pChar, SIZE_OF_ARRAY( pCmd ) );
			}
		}

		V_strcat( pCmd, "\"", SIZE_OF_ARRAY( pCmd ) );
		engine->ClientCmd_Unrestricted( pCmd );
		ThreadSleep( 700 );
	}
	return 0;
}

CON_COMMAND_F( voice_status_test_toggle, "Test voice and status notices", FCVAR_DEVELOPMENTONLY )
{
	if( s_bRunningVoiceStatusTest )
	{
		s_bRunningVoiceStatusTest = false;
	}
	else
	{
		s_bRunningVoiceStatusTest = true;
		CreateSimpleThread( VoiceStatusTest, 0 );
	}
}
#endif

CCSGO_HudNotice::CCSGO_HudNotice(CPanel2D *pParent, const char *pchID)
	: panorama::CPanel2D(pParent, pchID),
	m_bRemove(false),
	m_pLabel(NULL)
{
	BLoadLayoutSnippet("HudNotice");

	CPanel2D* pLabel = GetChild(1);
	if (pLabel)
	{
		m_pLabel = (CLabel*)pLabel;
	}

	RegisterEventHandler(AnimationEnd(), this, &CCSGO_HudNotice::EventAnimationEnd);
}

void CCSGO_HudNotice::SetNoticeText(const char *pchValue, CLabel::ETextType eTextType)
{
	if (m_pLabel)
	{
		// We localize our text ourself and guarantee it is HTML-safe.
		m_pLabel->SetAllowRawText( true );
		m_pLabel->SetText(pchValue, eTextType);
		m_pLabel->SetAllowRawText( false );
	}
}

bool CCSGO_HudNotice::EventAnimationEnd(const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr, CPanoramaSymbol symAnimation)
{
	static const CPanoramaSymbol k_symShowAndHide("ShowAndHide");
	static const CPanoramaSymbol k_symQuickHide("QuickHide");

	if (symAnimation == k_symQuickHide)
	{
//		Msg("Setting remove flag on %s following QuickHide\n", GetID());
		m_bRemove = true;
	}
	else if ((symAnimation == k_symShowAndHide) && !BHasClass(k_symQuickHide))
	{
//		Msg("Setting remove flag on %s following ShowAndHide\n", GetID());
		m_bRemove = true;
	}
	return true;
}

CCSGO_VoiceNotice::CCSGO_VoiceNotice(CPanel2D *pParent, const char *pchID)
	: panorama::CPanel2D(pParent, pchID),
	m_pLabel(NULL),
	m_pAvatarImage(NULL)
{
	BLoadLayoutSnippet("VoiceNotice");

	for (int i = 0; i < GetChildCount(); ++i)
	{
		CPanel2D* pChild = GetChild(i);
		if( pChild->BHasClass( "VoiceText" ) )
		{
			m_pLabel = (CLabel*)pChild;
		}
		else if( pChild->BHasClass( "AvatarPanel" ) )
		{
			for (int j = 0; j < GetChildCount(); ++j)
			{
				CPanel2D* pAvatarChild = pChild->GetChild(j);
				if (pAvatarChild->BHasClass( "SteamAvatar" ))
				{
					m_pAvatarImage = (CCSGO_AvatarImage*)pAvatarChild;
				}
			}
		}
		else if( pChild->BHasClass( "SoundAnim" ) )
		{
			m_pSoundAnim = pChild;
		}
	}
}

void CCSGO_VoiceNotice::ShowVoiceNotice(const char *pchVoiceText, ActiveSpeaker& activeSpeaker, uint64 xuid, char* pchPlayerName, bool bIsLocalPlayer)
{
	static const CPanoramaSymbol k_symHidden("Hidden");
	static const CPanoramaSymbol k_symDynamicAvatar("DynamicAvatar");
	static const CPanoramaSymbol k_symDefaultAvatarCT("DefaultAvatarCT");
	static const CPanoramaSymbol k_symDefaultAvatarT("DefaultAvatarT");
	static const CPanoramaSymbol k_symSpectatorAvatar( "SpectatorAvatar" );
	static const CPanoramaSymbol k_symShowSkull( "ShowSkull" );
	static const CPanoramaSymbol k_symLooping( "Looping" );
	static const CPanoramaSymbol k_symSoundLow( "SoundLow" );
	static const CPanoramaSymbol k_symSoundHigh( "SoundHigh" );
	static const CPanoramaSymbol k_symSoundOff( "SoundOff" );

	if (m_pLabel)
	{
		m_pLabel->SetText(pchVoiceText, CLabel::k_ETextTypeHTML);
	}

	bool bShowDynamicAvatar = (xuid && m_pAvatarImage);
	
	if( bShowDynamicAvatar )
	{
		m_pAvatarImage->SetSteamID(CSteamID(xuid));
		SwitchClass( "AvatarType", k_symDynamicAvatar );
	}
	else if(activeSpeaker.nTeamNum == 2) // Terrorist
	{
		SwitchClass( "AvatarType", k_symDefaultAvatarT );
	}
	else if( activeSpeaker.nTeamNum == 3 ) // CT
	{
		SwitchClass( "AvatarType", k_symDefaultAvatarCT );
	}
	else
	{
		SwitchClass( "AvatarType", k_symSpectatorAvatar );
	}

	// Toggle the Dead icon based on alive status
	bool bShowSkull = false;
	if( activeSpeaker.nTeamNum == 2 || activeSpeaker.nTeamNum == 3 )
	{
		bShowSkull = !activeSpeaker.bIsAlive;
	}
	SetHasClass( k_symShowSkull, bShowSkull );

	// Play or jump to the appropriate offset in the speaker anim:
	//  	SpeakerState: 0 = autoplay (non-local player), 1 = jump to low speaker frame, 2 = jump to high speaker frame
	if( m_pSoundAnim )
	{
		switch( activeSpeaker.nSpeakerState )
		{
		case 0: // play looping
			m_pSoundAnim->SwitchClass( "SoundAnim", k_symLooping );
			break;
		case 1: // show low speaker
			m_pSoundAnim->SwitchClass( "SoundAnim", k_symSoundLow );
			break;
		case 2: // show high speaker
			m_pSoundAnim->SwitchClass( "SoundAnim", k_symSoundHigh );
			break;
		default:
			m_pSoundAnim->SwitchClass( "SoundAnim", k_symSoundOff );
			break;
		}
	}

	RemoveClass(k_symHidden);
}

void CCSGO_VoiceNotice::HideVoiceNotice()
{
	static const CPanoramaSymbol k_symHidden("Hidden");
	AddClass(k_symHidden);
}

CCSGO_HudVoiceStatus::CCSGO_HudVoiceStatus(CPanel2D *pParent, const char *pchID)
	: CPanoramaHudElement("CCSGO_HudVoiceStatus", this),
	panorama::CPanel2D( pParent, pchID ),
	m_flStatusPanelHeight(0),
	m_flMaxAlertHeight(0),
	m_fNotificationLifetime(15.5f),
	m_lastChattingEntIdx(-1),
	m_lastChatterWasAlive(false),
	m_lastChatterTeamNum(-1),
	m_lastChatterSpeakerState(SpeakerState_Invalid),
	m_pStatusPanel(NULL),
	m_pVoicePanel(NULL),
	m_fNextUpdateTime(0.f),
	m_fNotificationScrollLength(0.1f),
	m_bGlobalChatSubscriptionActive(false),
	m_dblNextSubscriptionHeartbeat(0),
	m_CallbackOnPersonaStateChange(this, &CCSGO_HudVoiceStatus::Steam_OnPersonaStateChange)
{
	SetHiddenBits(HIDEHUD_CHAT);

	m_vecNoticeText.EnsureCapacity(ALERT_NOTICE_CACHE_SIZE);
	m_vecPendingNoticeText.EnsureCapacity(ALERT_NOTICE_CACHE_SIZE);
	m_vecNoticeHandleCache.EnsureCapacity(ALERT_NOTICE_CACHE_SIZE);

	HOOK_HUD_MESSAGE_REALTIME_PASSTHROUGH(CCSGO_HudVoiceStatus, RadioText);
	HOOK_HUD_MESSAGE_REALTIME_PASSTHROUGH(CCSGO_HudVoiceStatus, SayText);
	HOOK_HUD_MESSAGE_REALTIME_PASSTHROUGH(CCSGO_HudVoiceStatus, SayText2);
	HOOK_HUD_MESSAGE_REALTIME_PASSTHROUGH(CCSGO_HudVoiceStatus, TextMsg);
	HOOK_HUD_MESSAGE_REALTIME_PASSTHROUGH(CCSGO_HudVoiceStatus, RawAudio);

	SetInputNamespace( "csgo_hudvoicestatus" );

	DbgVerify( BLoadLayout( "file://{resources}/layout/hud/hudvoicestatus.xml" ) );
	DbgVerify( ( m_pStatusPanel = GetChild( 0 ) ) != nullptr );
	DbgVerify( ( m_pVoicePanel = GetChild( 1 ) ) != nullptr );

	GetLayoutDefines();

	// Hide the panel initially
	ShowPanel(false);

	static const CPanoramaSymbol k_symAlertHidden("AlertHidden");

	// Populate cache
	for (int i = 0; i < ALERT_NOTICE_CACHE_SIZE; ++i)
	{
		CCSGO_HudNotice* pNotice = new CCSGO_HudNotice(m_pStatusPanel,CFmtStr("AlertPanel%d",i+1));
		pNotice->SwitchClass("alertstate", k_symAlertHidden);

		// Starting position at bottom of panel
		pNotice->SetPositionWithoutTransition(CUILength::ZeroLength(), CUILength(m_flStatusPanelHeight - m_flMaxAlertHeight, CUILength::k_EUILengthLength), CUILength::ZeroLength());

		m_vecNoticeHandleCache.AddToTail(pNotice);
	}

	// Set up voice notices
	for (int i = 0; i < 3; ++i)
	{
		CCSGO_VoiceNotice* pVoiceNotice = new CCSGO_VoiceNotice(m_pVoicePanel, CFmtStr("VoiceNotice%d", i + 1));
		pVoiceNotice->HideVoiceNotice();
	}

	ListenForGameEvent("player_reset_vote");

}

void CCSGO_HudVoiceStatus::OnStylesChanged()
{
//	Msg("CCSGO_HudVoiceStatus On Styles Changed\n");
	BaseClass::OnStylesChanged();
	GetLayoutDefines();
}

void CCSGO_HudVoiceStatus::GetLayoutDefines()
{
	m_fNotificationScrollLength = GetLayoutFileDefineFloat("AlertNoticeScrollInTime", 0.2f);
	m_fNotificationLifetime = GetLayoutFileDefineFloat("AlertNoticeLifetime", 15.5f);
	m_flStatusPanelHeight = GetLayoutFileDefineFloat("StatusPanelHeight", 360.0f);
	m_flMaxAlertHeight = GetLayoutFileDefineFloat("MaxAlertHeight", 120.0f);
}

CCSGO_HudVoiceStatus::~CCSGO_HudVoiceStatus()
{
	StopListeningForAllEvents();

	for (int nPos = 0; nPos < m_vecNoticeHandleCache.Count(); nPos++)
	{
		delete m_vecNoticeHandleCache[nPos];
		m_vecNoticeHandleCache[nPos] = NULL;
	}

	for (int i = 0; i < 3; ++i)
	{
		CPanel2D* pVoiceNotice = m_pVoicePanel->GetChild(i);
		if (pVoiceNotice)
		{
			delete pVoiceNotice;
		}
	}

	m_vecNoticeText.Purge();
	m_vecPendingNoticeText.Purge();
	m_vecNoticeHandleCache.Purge();

	m_mapPendingChat.PurgeAndDeleteElements();
}


void CCSGO_HudVoiceStatus::LevelInit(void)
{
	m_fNextUpdateTime = 0.f;
	ClearActiveList();

	for (int i = 0; i < ARRAYSIZE(m_SFSpeakerLabels); i++)
	{
		m_SFSpeakerLabels[i].bIsAlive = false;
		m_SFSpeakerLabels[i].bSpeaking = false;
		m_SFSpeakerLabels[i].fAlpha = 0;
		m_SFSpeakerLabels[i].nSpeakerState = SpeakerState_Invalid;
		m_SFSpeakerLabels[i].nTeamNum = -1;
		m_SFSpeakerLabels[i].playerId = -1;
	}
}

void CCSGO_HudVoiceStatus::LevelShutdown(void)
{
	ClearActiveList();

	ChatUnsubscribe();
}


void CCSGO_HudVoiceStatus::ShowPanel(const bool bShow)
{
	if (bShow)
	{
		if (m_bActive)
			SaveHideRestoreShow(false);
	}
	else
	{
		SaveHideRestoreShow(true);
	}
}

extern bool IsTakingAFreezecamScreenshot();
extern ConVar cl_drawhud;


bool CCSGO_HudVoiceStatus::ShouldDraw(void)
{
	if (IsTakingAFreezecamScreenshot() || !C_BasePlayer::GetLocalPlayer() || !engine->IsLocalPlayerResolvable())
		return false;

	CCSGO_HudChat* pChat = GET_HUDELEMENT(CCSGO_HudChat);

	if ( pChat && pChat->IsActive() )
		return false;

	return cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false && CHudElement::ShouldDraw();
}

void CCSGO_HudVoiceStatus::SetActive(bool bActive)
{
	if (bActive != m_bActive)
	{
		m_bActive = bActive;
		ShowPanel(bActive);
	}

 	CHudElement::SetActive(bActive);
}


static char *GetTeamColorCode(int team)
{
	if (team == TEAM_CT)
		return "#a2c6ff";
	else if (team == TEAM_TERRORIST)
		return "#ffdf93";
	else
		return "#ba81f0";
}

static char *GetTeamColorCodeForAdd(int team)
{
	if (team == TEAM_CT)
		return "#729bdd";
	else if (team == TEAM_TERRORIST)
		return "#e0b756";
	else
		return "#ffffff";
}

static char *GetTeammateColorCode(int pidx)
{
	int r = 0;
	int g = 0;
	int b = 0;

	switch (pidx)
	{
	case 0:
	{
		static ConVarRef cl_teammate_color_1("cl_teammate_color_1");
		sscanf(cl_teammate_color_1.GetString(), "%i %i %i", &r, &g, &b);
		break;
	}
	case 1:
	{
		static ConVarRef cl_teammate_color_2("cl_teammate_color_2");
		sscanf(cl_teammate_color_2.GetString(), "%i %i %i", &r, &g, &b);
		break;
	}
	case 2:
	{
		static ConVarRef cl_teammate_color_3("cl_teammate_color_3");
		sscanf(cl_teammate_color_3.GetString(), "%i %i %i", &r, &g, &b);
		break;
	}
	case 3:
	{
		static ConVarRef cl_teammate_color_4("cl_teammate_color_4");
		sscanf(cl_teammate_color_4.GetString(), "%i %i %i", &r, &g, &b);
		break;
	}
	case 4:
	{
		static ConVarRef cl_teammate_color_5("cl_teammate_color_5");
		sscanf(cl_teammate_color_5.GetString(), "%i %i %i", &r, &g, &b);
		break;
	}
	}

	static char szColor[HEX_COLOR_CHAR_SIZE];
	V_sprintf_safe(szColor, "#%02X%02X%02X", uint32(uint8(r)), uint32(uint8(g)), uint32(uint8(b)));
	return szColor;
}

static const char *GetColorCode(int code, int team)
{
	// code is -1 and we don't have a team, it should just be white
	if (code < 0 && team != TEAM_TERRORIST && team != TEAM_CT)
		return "#ffffff";

	// if we want a playername or if we have a team specified, do this
	if (code == COLOR_PLAYERNAME || code < 0)
		return GetTeamColorCode(team);

	int nRarity = -1;

	switch (code)
	{
	case COLOR_LOCATION:
		return "#40ff40"; // g_ColorDarkGreen as defined in hud_basechat.cpp

	case COLOR_ACHIEVEMENT:
		return "#bfff90";

	case COLOR_NORMAL:
		return "#ffffff";

	case COLOR_AWARD:
		return "#a2ff47";

	case COLOR_PENALTY:
		return "#FF4040";

	case COLOR_SILVER:
		return "#c5cad0";

	case COLOR_GOLD:
		return "#ede47a";

	case COLOR_COMMON:
	case COLOR_UNCOMMON:
	case COLOR_RARE:
	case COLOR_MYTHICAL:
	case COLOR_LEGENDARY:
	case COLOR_ANCIENT:
	case COLOR_IMMORTAL:
		nRarity = code - COLOR_RARITY_FIRST + 1;
		break;

	default: // unknown color code??
		return "#ff0000";
	}

	return GetItemSchema()->GetRarityColor(nRarity);
}

static const char *GetChatTeammateColorGlyph(int pidx)
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if (!pLocalPlayer)
		return "";

	bool bShowLetter = pLocalPlayer->ShouldShowTeamPlayerColorLetters();

	if (bShowLetter)
	{
		switch (pidx)
		{
		case 0:
			return g_pVGuiLocalize->FindAsUTF8( "#CSGO_Competitive_Dot_Y" );
			break;
		case 1:
			return g_pVGuiLocalize->FindAsUTF8( "#CSGO_Competitive_Dot_P" );
			break;
		case 2:
			return g_pVGuiLocalize->FindAsUTF8( "#CSGO_Competitive_Dot_G" );
			break;
		case 3:
			return g_pVGuiLocalize->FindAsUTF8( "#CSGO_Competitive_Dot_B" );
			break;
		case 4:
			return g_pVGuiLocalize->FindAsUTF8( "#CSGO_Competitive_Dot_O" );
			break;
		}
	}

	return g_pVGuiLocalize->FindAsUTF8( "#CSGO_Competitive_Dot" );
}

static inline bool IsColorChar(const char* pChar)
{
	if ((*pChar > 0) && (*pChar < COLOR_MAX))
	{
		return true;
	}
	return false;
}

void CCSGO_HudVoiceStatus::ColorizeNotice( const char *szNotice, char *colorStr, int clientId )
{
	if ( !g_PR )
		return;

	// Default team color is 0 = "Blue" for neutral/spectator/console messages
	int team = 0;
	int pidx = -1;
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	// Determine the team of the client printing out this notice; if clientID is 0 then
	//  this is a message from console, and should be printed "blue" (see hud_basechat.cpp, ChatPrintf function)
	if ( clientId > 0 )
	{
		team = g_PR->GetTeam( clientId) ;

		if ( pLocalPlayer && pLocalPlayer->ShouldShowTeamPlayerColors( team ) )
		{
			C_CS_PlayerResource* pCSPR = (C_CS_PlayerResource*)g_PR;
			if (pCSPR)
				pidx = pCSPR->GetCompTeammateColor(clientId);
		}

		if ( team == TEAM_SPECTATOR && g_PR->GetCoachingTeam( clientId ) != 0 )
		{
			team = g_PR->GetCoachingTeam(clientId);
		}
	}

	const char *szSrc = szNotice;
	char *szDest = 0;

	int lastColorCode = COLOR_NORMAL;
	int colorStrIndex = 0; // write head for the output string

						   // If we don't start with a color token, assume we want to use the team of the client as our first color
	if (szSrc && *szSrc > 0 && *szSrc < COLOR_MAX)
	{
		lastColorCode = *szSrc++;
	}
	else
	{
		lastColorCode = -1;
	}

	int nCount = 0;
	char subStr[ALERT_NOTICE_TEXT_MAX];
	char* pEndSubStr = subStr + ALERT_NOTICE_TEXT_MAX - 7; //Leaves room for longest character substitution ("&quot;") and null-terminating character

	// Repeat this process until we hit end of the notice string:
	while (szSrc && *szSrc != 0 && nCount < ALERT_NOTICE_TEXT_MAX)
	{
		szDest = subStr;

		// this will help us clip off any whitespace at the end of the string
		char* putTerminatorHere = szDest;
		const char* pNextUTF8 = NULL;

		while (szSrc && *szSrc != 0 && !IsColorChar(szSrc) && szDest <= pEndSubStr)
		{
			pNextUTF8 = V_UnicodeAdvance(szSrc, 1);

			// Replace any restricted HTML characters with their proper substitutions
			switch (*szSrc)
			{
			case '&':	// becomes "&amp;"
				*szDest++ = '&'; *szDest++ = 'a'; *szDest++ = 'm'; *szDest++ = 'p'; *szDest++ = ';';
				break;

			case '"':	// becomes "&quot;"
				*szDest++ = '&'; *szDest++ = 'q'; *szDest++ = 'u'; *szDest++ = 'o'; *szDest++ = 't'; *szDest++ = ';';
				break;

			case '\'':	// becomes "&apos;"
				*szDest++ = '&'; *szDest++ = 'a'; *szDest++ = 'p'; *szDest++ = 'o'; *szDest++ = 's'; *szDest++ = ';';
				break;

			case '>':	// becomes "&gt;"
				*szDest++ = '&'; *szDest++ = 'g'; *szDest++ = 't'; *szDest++ = ';';
				break;

			case '<':	// becomes "&lt;"
				*szDest++ = '&'; *szDest++ = 'l'; *szDest++ = 't'; *szDest++ = ';';
				break;

			default:	// all other chars just copy over
				memcpy(szDest, szSrc, pNextUTF8-szSrc);
				szDest = V_UnicodeAdvance(szDest, 1);
				break;

			}

			putTerminatorHere = szDest;
			nCount += (pNextUTF8 - szSrc);
			szSrc = pNextUTF8;
		}

		*putTerminatorHere = 0;

		//GetChatTeammateColorGlyph( int pidx )

		// Now write this markup into our final string:
		char colorToken[ALERT_NOTICE_TEXT_MAX];
		if ((lastColorCode == COLOR_PLAYERNAME || lastColorCode < 0) && pidx > -1)
		{
			int nColorID = (pidx % 5);
			// put a colored dot next to a player name if they are on our team
			V_sprintf_safe(colorToken, ALERT_NOTICE_FONT_TEAMMATE_STRING, GetTeammateColorCode(nColorID), GetChatTeammateColorGlyph(nColorID), GetColorCode(lastColorCode, team), subStr);
		}
		else
			V_sprintf_safe(colorToken, ALERT_NOTICE_FONT_STRING, GetColorCode(lastColorCode, team), subStr);

		// Detect if we would overrun our buffer and abort
		int tokenCount = V_strlen(colorToken);
		if (colorStrIndex + tokenCount > ALERT_NOTICE_TEXT_MAX)
			break;

		V_strncpy(&colorStr[colorStrIndex], colorToken, (tokenCount + 1) * sizeof(char));

		// increment write head by the length of the last written token
		colorStrIndex += tokenCount;

		if (*szSrc != 0)
		{
			lastColorCode = *szSrc++;
		}
	}
}

void CCSGO_HudVoiceStatus::PushGlobalNotice(const char * szNoticeText, uint32 unAccountID)
{
	static const CSteamID s_steamid(steamapicontext->SteamUser()->GetSteamID());
	if (unAccountID == s_steamid.GetAccountID())
	{
		// This is our own user sending the message, also subscribe them to the chat
		if (uint64 umid = Helper_GlobalChat_UMID())
		{
			GCSDK::CProtoBufMsg< CMsgGCCStrike15_v2_ClientToGCChat > msg(k_EMsgGCCStrike15_v2_GlobalChat);
			msg.Body().set_match_id(umid);
			msg.Body().set_text(szNoticeText);
			ChatMsgSendBroadcast(&msg);
		}
	}

	CSteamID steamid(s_steamid);
	steamid.SetAccountID(unAccountID);


	int idxExisting = m_mapPendingChat.Find(unAccountID);
	if (idxExisting != m_mapPendingChat.InvalidIndex())
	{
		free(m_mapPendingChat.Element(idxExisting));
		m_mapPendingChat.RemoveAt(idxExisting);
	}

	if (steamapicontext->SteamFriends()->RequestUserInformation(steamid, true))
	{
		m_mapPendingChat.InsertOrReplace(unAccountID, strdup(szNoticeText));
	}
	else
	{
		char const *szNameResolved = Helper_GetFriendPersonaNameSanitized(steamid);
		PushGlobalNotice(szNoticeText, unAccountID, szNameResolved);
	}
}

void CCSGO_HudVoiceStatus::Steam_OnPersonaStateChange(PersonaStateChange_t *pParam)
{
	if (pParam->m_nChangeFlags & k_EPersonaChangeName)
	{
		uint32 unAccountID = CSteamID(pParam->m_ulSteamID).GetAccountID();
		int idxExisting = m_mapPendingChat.Find(unAccountID);
		if (idxExisting != m_mapPendingChat.InvalidIndex())
		{
			char const *szNameResolved = Helper_GetFriendPersonaNameSanitized(CSteamID(pParam->m_ulSteamID));
			PushGlobalNotice(m_mapPendingChat.Element(idxExisting), unAccountID, szNameResolved);

			free(m_mapPendingChat.Element(idxExisting));
			m_mapPendingChat.RemoveAt(idxExisting);
		}
	}
}

void CCSGO_HudVoiceStatus::PushGlobalNotice(const char * szNoticeText, uint32 unAccountID, char const *szNameResolved)
{
	char safePlayerName[ALERT_NOTICE_TEXT_MAX];
	safePlayerName[0] = COLOR_MYTHICAL;

	V_strncpy(safePlayerName + 1, szNameResolved, ALERT_NOTICE_TEXT_MAX - 1);
	int nLen = V_strlen(safePlayerName);
	// Replace any potential control characters to prevent colorization/crlfs/etc.
	for (char* pChar = safePlayerName + 1; *pChar != 0; pChar = V_UnicodeAdvance(pChar, 1))
	{
		if (*pChar > 0 && *pChar < ' ')
			*pChar = ' ';
	}

	if (nLen < ALERT_NOTICE_TEXT_MAX - 10)
	{
		safePlayerName[nLen++] = COLOR_NORMAL;
		safePlayerName[nLen++] = ':';
		safePlayerName[nLen++] = ' ';
		safePlayerName[nLen] = 0;

		V_strncpy(safePlayerName + nLen, szNoticeText, ALERT_NOTICE_TEXT_MAX - nLen - 1);
		// Replace any potential control characters to prevent colorization/crlfs/etc.
		for (char* pChar = safePlayerName + nLen; *pChar != 0; pChar = V_UnicodeAdvance(pChar, 1))
		{
			if (*pChar > 0 && *pChar < ' ')
				*pChar = ' ';
		}

		g_BannedWords.CensorBannedWordsInplace(safePlayerName);

		PushNotice(safePlayerName, -1);
	}
}

void CCSGO_HudVoiceStatus::PushNotice(const char * szColorNotice, int clientId, bool bAlreadyFormattedAsCensoredAndSafeHTML )
{
	extern ConVar cl_chatfilters;	// Legacy support for disabling all chat output
	if (cl_chatfilters.GetInt() == 0)
	{
		static double s_dblLastWarning = 0;
		double dblTimeNow = Plat_FloatTime();
		if (dblTimeNow - s_dblLastWarning > 15)
		{
			DevMsg("cl_chatfilters = 0: chat is disabled.\n");
			s_dblLastWarning = dblTimeNow;
		}
		return;
	}

	NoticeText_t notice;

	if ( bAlreadyFormattedAsCensoredAndSafeHTML )
	{
		V_strcpy_safe( notice.m_szNotice, szColorNotice );
	}
	else
	{
		// Begin marking-up the text to indicate desired font colors
		ColorizeNotice( szColorNotice, notice.m_szNotice, clientId );
		g_BannedWords.CensorBannedWordsInplace( notice.m_szNotice );
	}
	CCSGO_HudNotice* pNoticePanel = NULL;

	if (m_vecNoticeHandleCache.Count())
	{
		pNoticePanel = m_vecNoticeHandleCache.Tail();
		m_vecNoticeHandleCache.FastRemove(m_vecNoticeHandleCache.Count() - 1);
	}
	else if( m_vecPendingNoticeText.Count() )
	{
		// pull the oldest pending notice
		// this happens when we are skipping in demos, or when the chat window is active
		int numPending = m_vecPendingNoticeText.Count() - 1;
		NoticeText_t noticeToRemove = m_vecPendingNoticeText[numPending];
		m_vecPendingNoticeText.Remove( numPending );

		pNoticePanel = noticeToRemove.m_pPanel;
	}
	else
	{
		Msg( "CCSGO_HudVoiceStatus::PushNotice() no notice panels available, ignoring message %s\n", szColorNotice );
	}

	if (pNoticePanel)
	{
		notice.m_pPanel = pNoticePanel;

		pNoticePanel->SetNoticeText(notice.m_szNotice, CLabel::k_ETextTypeHTML);

		notice.m_fSpawnTime = gpGlobals->curtime;
		notice.m_fY = 0.0f;
		notice.m_fTextHeight = 0.0f;

		m_vecPendingNoticeText.AddToHead(notice);

		CCSGO_HudChat* pChatElement = GET_HUDELEMENT( CCSGO_HudChat );
		if (pChatElement)
		{
			pChatElement->AddStringToHistory( notice.m_szNotice );
		}
	}
}

#ifdef _DEBUG
// CON_COMMAND_F(debug_global_chat, "Display global server stats", FCVAR_CLIENTCMD_CAN_EXECUTE)
// {
// 	if (args.ArgC() < 2)
// 	{
// 		GCSDK::CProtoBufMsg< CMsgGCCStrike15_v2_ClientToGCChat > msg( k_EMsgGCCStrike15_v2_GlobalChat_Unsubscribe );
// 		GCClientSystem()->GetGCClient()->BSendMessage(msg);
// 		Msg("debug_global_chat: unsubscribed.\n");
// 	}
// 	else if (args.ArgC() == 2)
// 	{
// 		GCSDK::CProtoBufMsg< CMsgGCCStrike15_v2_ClientToGCChat > msg( args.Arg( 1 )[0] ? k_EMsgGCCStrike15_v2_GlobalChat : k_EMsgGCCStrike15_v2_GlobalChat_Subscribe );
// 		msg.Body().set_match_id(1);
// 		msg.Body().set_text(args.Arg(1));
// 		GCClientSystem()->GetGCClient()->BSendMessage(msg);
// 		Msg("debug_global_chat: sent message \"%s\" on implicit channel #%llu.\n", msg.Body().text().c_str(), msg.Body().match_id());
// 	}
// 	else if ((args.ArgC() == 3) && V_isdigit(args.Arg(2)[0]))
// 	{
// 		GCSDK::CProtoBufMsg< CMsgGCCStrike15_v2_ClientToGCChat > msg( args.Arg( 1 )[0] ? k_EMsgGCCStrike15_v2_GlobalChat : k_EMsgGCCStrike15_v2_GlobalChat_Subscribe );
// 		msg.Body().set_match_id(V_atoui64(args.Arg(2)));
// 		msg.Body().set_text(args.Arg(1));
// 		GCClientSystem()->GetGCClient()->BSendMessage(msg);
// 		Msg("debug_global_chat: sent message \"%s\" on explicit channel #%llu.\n", msg.Body().text().c_str(), msg.Body().match_id());
// 	}
// 	else
// 	{
// 		Warning("Usage: debug_global_chat \"text\" <channel>");
// 	}
// }
#endif


#ifndef NO_STEAM
class ClientJob_EMsgGCCStrike15_v2_GCToClientChat : public GCSDK::CGCClientJob
{
public:

	explicit ClientJob_EMsgGCCStrike15_v2_GCToClientChat(GCSDK::CGCClient *pGCClient) : GCSDK::CGCClientJob(pGCClient)
	{
	}

	virtual bool BYieldingRunJobFromMsg(GCSDK::IMsgNetPacket *pNetPacket)
	{
		if (!Helper_GlobalChat_UMID())
			return true;

		GCSDK::CProtoBufMsg< CMsgGCCStrike15_v2_GCToClientChat > msg(pNetPacket);

		CHudElement *pElement = GetHud().FindElement("CCSGO_HudVoiceStatus");
		if (pElement)
		{
			((CCSGO_HudVoiceStatus *)pElement)->PushGlobalNotice(msg.Body().text().c_str(), msg.Body().account_id());
		}

		return true;
	}
};
GC_REG_CLIENT_JOB(ClientJob_EMsgGCCStrike15_v2_GCToClientChat, k_EMsgGCCStrike15_v2_GlobalChat);
#endif


void CCSGO_HudVoiceStatus::Think(void)
{
	UpdateNotices();

	UpdateVoiceStatus();

	if (uint64 umid = Helper_GlobalChat_UMID())
	{
		double dblTimeNow = Plat_FloatTime();
		if (!m_dblNextSubscriptionHeartbeat)
		{
			m_dblNextSubscriptionHeartbeat = dblTimeNow + RandomFloat(1.0f, 25.0f);
			m_mapPendingChat.PurgeAndDeleteElements();
		}
		else if (dblTimeNow > m_dblNextSubscriptionHeartbeat)
		{	// Just a subscription re-heartbeat
			GCSDK::CProtoBufMsg< CMsgGCCStrike15_v2_ClientToGCChat > msg(k_EMsgGCCStrike15_v2_GlobalChat_Subscribe);
			msg.Body().set_match_id(umid);
			ChatMsgSendBroadcast(&msg);
		}
	}
	else
	{
		ChatUnsubscribe();
	}
}

bool CCSGO_HudVoiceStatus::ChatMsgSendBroadcast(void * /* GCSDK::CProtoBufMsg< CMsgGCCStrike15_v2_ClientToGCChat > * */ pmsg)
{
// 	GCSDK::CProtoBufMsg< CMsgGCCStrike15_v2_ClientToGCChat > *pRealGcMsg = reinterpret_cast<GCSDK::CProtoBufMsg< CMsgGCCStrike15_v2_ClientToGCChat > *>(pmsg);
// 	m_bGlobalChatSubscriptionActive = GCClientSystem()->GetGCClient()->BSendMessage(*pRealGcMsg);
// 	m_dblNextSubscriptionHeartbeat = Plat_FloatTime() + RandomFloat(60.0f, 120.0f);
	return m_bGlobalChatSubscriptionActive;
}

void CCSGO_HudVoiceStatus::ChatUnsubscribe()
{
	if (m_dblNextSubscriptionHeartbeat)
	{
		m_dblNextSubscriptionHeartbeat = 0;
		m_mapPendingChat.PurgeAndDeleteElements();

		if (m_bGlobalChatSubscriptionActive)
		{
			m_bGlobalChatSubscriptionActive = false;
// 			GCSDK::CProtoBufMsg< CMsgGCCStrike15_v2_ClientToGCChat > msg(k_EMsgGCCStrike15_v2_GlobalChat_Unsubscribe);
// 			GCClientSystem()->GetGCClient()->BSendMessage(msg);
		}
	}
}

int CCSGO_HudVoiceStatus::FindActiveSpeaker(int playerId)
{
	FOR_EACH_LL(m_SpeakingList, i)
	{
		if (m_SpeakingList[i].playerId == playerId)
			return i;
	}
	return m_SpeakingList.InvalidIndex();
}

void CCSGO_HudVoiceStatus::ClearActiveList()
{
	m_SpeakingList.RemoveAll();
	if( m_pVoicePanel )
	{
		for( int i = 0; i < m_pVoicePanel->GetChildCount(); ++i )
		{
			CCSGO_VoiceNotice* pVoiceNotice = (CCSGO_VoiceNotice*)m_pVoicePanel->GetChild( i );
			pVoiceNotice->HideVoiceNotice();
		}
	}
}

void CCSGO_HudVoiceStatus::UpdateVoiceStatus(void)
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

	for (int iPlayerIndex = 1; iPlayerIndex <= gpGlobals->maxClients; iPlayerIndex++)
	{
		bool bIsLocalPlayer = pLocalPlayer && (iPlayerIndex == pLocalPlayer->entindex());
		int activeSpeakerIndex = FindActiveSpeaker(iPlayerIndex);
		bool bSpeaking = bIsLocalPlayer ? GetClientVoiceMgr()->IsLocalPlayerSpeaking(pLocalPlayer->GetSplitScreenPlayerSlot()) : GetClientVoiceMgr()->IsPlayerSpeaking(iPlayerIndex);

		if (activeSpeakerIndex != m_SpeakingList.InvalidIndex())
		{
			// update their speaking status
			m_SpeakingList[activeSpeakerIndex].bSpeaking = bSpeaking;
		}
		else
		{
			// if they are talking and not in the list, add them to the end
			if (bSpeaking)
			{
				ActiveSpeaker activeSpeaker;
				activeSpeaker.playerId = iPlayerIndex;
				activeSpeaker.bSpeaking = true;
				activeSpeaker.fAlpha = 1.0f;
				activeSpeaker.nSpeakerState = SpeakerState_Auto;

				// add the local player to the top
				if (bIsLocalPlayer)
					m_SpeakingList.AddToHead(activeSpeaker);
				else
					m_SpeakingList.AddToTail(activeSpeaker);
			}
		}
	}

	float fTime = gpGlobals->frametime;
	float fFadeOutTime = 0.5;

	for (int i = m_SpeakingList.Head(); i != m_SpeakingList.InvalidIndex(); )
	{
		ActiveSpeaker& activeSpeaker = m_SpeakingList[i];

		if (!activeSpeaker.bSpeaking)
		{
			if (fFadeOutTime > 0.0f)
			{
				activeSpeaker.fAlpha -= fTime / fFadeOutTime;
			}
			else
			{
				activeSpeaker.fAlpha = 0.0f;
			}

			if (activeSpeaker.fAlpha <= 0.0f)
			{
				for (int j = 0; j < ARRAYSIZE(m_SFSpeakerLabels); j++)
				{
					if (m_SFSpeakerLabels[j].playerId == activeSpeaker.playerId)
					{
						CCSGO_VoiceNotice* pVoiceNotice = (CCSGO_VoiceNotice*)m_pVoicePanel->GetChild( j );
						if( pVoiceNotice )
						{
							pVoiceNotice->HideVoiceNotice();
						}

						m_SFSpeakerLabels[j].nSpeakerState = SpeakerState_Invalid;
						m_SFSpeakerLabels[j].playerId = -1;
						break;
					}
				}

				int iNext = m_SpeakingList.Next(i);
				m_SpeakingList.Remove(i);
				i = iNext;
				continue;
			}
		}
		i = m_SpeakingList.Next(i);
	}

	C_BasePlayer *pPlayerToDisplay = NULL;
	//int iDisplayVoiceChat = -1;
	wchar_t wszChatterName[MAX_DECORATED_PLAYER_NAME_LENGTH];
	wszChatterName[0] = L'\0';
	char szChatterName[MAX_DECORATED_PLAYER_NAME_LENGTH];

	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();

	for (int j = m_SpeakingList.Head(); j != m_SpeakingList.InvalidIndex(); )
	{
		ActiveSpeaker& activeSpeaker = m_SpeakingList[j];

		//bool bIsBot = pCSPR->IsFakePlayer( activeSpeaker.playerId );

		if (activeSpeaker.bSpeaking)
		{
			bool bIsLocalPlayer = pLocalPlayer && (activeSpeaker.playerId == pLocalPlayer->entindex());

			if (pCSPR)
			{
				// $$$REI Use dialog variables instead of html escaping here
				pCSPR->GetDecoratedPlayerName( activeSpeaker.playerId, wszChatterName, sizeof( wszChatterName ), k_EDecoratedPlayerNameFlag_Simple | k_EDecoratedPlayerNameFlag_HTMLEscapeString );
				activeSpeaker.bIsAlive = pCSPR->IsAlive(activeSpeaker.playerId);
				activeSpeaker.nTeamNum = pCSPR->GetTeam(activeSpeaker.playerId);
				V_UnicodeToUTF8(wszChatterName, szChatterName, MAX_DECORATED_PLAYER_NAME_LENGTH);
			}
			else
			{
				wszChatterName[0] = L'\0';
				szChatterName[0] = '\0';
			}
			// try to get the player pointer, but if they're not on our team this may return NULL
			pPlayerToDisplay = UTIL_PlayerByIndex(activeSpeaker.playerId);

			// Determine player's name, to use in special-case demo avatar code
			char playerName[64];
			if (g_PR && pPlayerToDisplay)
			{
				V_strncpy(playerName, g_PR->GetPlayerName(activeSpeaker.playerId), ARRAYSIZE(playerName));
			}
			else
			{
				V_snprintf(playerName, ARRAYSIZE(playerName), "Unknown");
			}

			if (activeSpeaker.playerId != -1)
			{
				int nCurrentSlot = -1;
				// find out if this speaker is already being shown
				for (int i = 0; i < ARRAYSIZE(m_SFSpeakerLabels); i++)
				{
					if (m_SFSpeakerLabels[i].playerId == activeSpeaker.playerId)
					{
						nCurrentSlot = i;
						break;
					}
				}

				if (nCurrentSlot == -1)
				{
					for (int i = 0; i < ARRAYSIZE(m_SFSpeakerLabels); i++)
					{
						if (m_SFSpeakerLabels[i].playerId == -1)
						{
							nCurrentSlot = i;
							break;
						}
					}
				}

				if (bIsLocalPlayer)
				{
					activeSpeaker.nSpeakerState = GetClientVoiceMgr()->IsLocalPlayerSpeakingAboveThreshold(pLocalPlayer->GetSplitScreenPlayerSlot()) ? SpeakerState_HighAudio : SpeakerState_LowAudio;
				}

				if( voice_all_icons.GetBool() )
				{
					// Use this to test the three possible icon animations (looping/SoundLow/SoundHigh) 
					activeSpeaker.nSpeakerState = nCurrentSlot;
				}

				// Detect any changes that require us to update the voice status window:
				if (nCurrentSlot != -1 && ((activeSpeaker.playerId != m_SFSpeakerLabels[nCurrentSlot].playerId) ||
					(activeSpeaker.bIsAlive != m_SFSpeakerLabels[nCurrentSlot].bIsAlive) ||
					(activeSpeaker.nTeamNum != m_SFSpeakerLabels[nCurrentSlot].nTeamNum) ||
					(activeSpeaker.nSpeakerState != m_SFSpeakerLabels[nCurrentSlot].nSpeakerState)))
				{

					m_SFSpeakerLabels[nCurrentSlot] = activeSpeaker;

					char cString[VOICE_NOTICE_TEXT_MAX];
					cString[0] = 0x0;

					GenerateVoiceText(cString, sizeof(cString), pPlayerToDisplay, szChatterName, activeSpeaker.bIsAlive, activeSpeaker.nTeamNum);

					// Get the player xuid.
					uint64 xuid = 0;

					if (pCSPR)
					{
						xuid = pCSPR->GetXuid(activeSpeaker.playerId);
//						V_snprintf(xuidText, ARRAYSIZE(xuidText), "%llu", pCSPR->GetXuid(activeSpeaker.playerId));
					}

					if (activeSpeaker.playerId != -1)
					{
						static const CPanoramaSymbol k_symHidden("Hidden");

						CCSGO_VoiceNotice* pVoiceNotice = (CCSGO_VoiceNotice*)m_pVoicePanel->GetChild(nCurrentSlot);
						if (pVoiceNotice)
						{
							pVoiceNotice->ShowVoiceNotice(cString, activeSpeaker, xuid, playerName, bIsLocalPlayer);
						}
					}
				}
			}
		}

		j = m_SpeakingList.Next(j);
	}
}

// Code ported from CHudVoiceStatus::Paint() in hud_voicestatus.cpp:
void CCSGO_HudVoiceStatus::GenerateVoiceText(char *pString, int strLength, C_BasePlayer *pChatter, const char *pChatterName, bool bIsAlive, int iChatterTeam)
{
	char szDecoratedName[VOICE_NOTICE_TEXT_MAX];

	// New rules for location: always show this if you're on the same team.  Otherwise, just assume you can show it if you're NOT in all-talk
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	int iLocalPlayerTeam = pLocalPlayer ? pLocalPlayer->GetTeamNumber() : TEAM_UNASSIGNED;

	if ( /*( sf_sv_alltalk && !sf_sv_alltalk->GetBool() ) || */iChatterTeam == iLocalPlayerTeam)
	{
		const char *pszLocationText = pChatter ? pChatter->GetLastKnownPlaceName() : NULL;
		if (bIsAlive && pszLocationText && *pszLocationText/* && formatStr*/)
		{
// 			const wchar_t *unicodeLocation = g_pVGuiLocalize->Find(pszLocationText);
			char szColorLocation[VOICE_NOTICE_TEXT_MAX];
			V_snprintf(szColorLocation, ARRAYSIZE(szColorLocation), ALERT_NOTICE_FONT_STRING, "#40ff40", g_pVGuiLocalize->FindAsUTF8( pszLocationText ));

			V_sprintf_safe(szDecoratedName, VOICE_NOTICE_LOCATION, pChatterName, szColorLocation);

			pChatterName = szDecoratedName;	// point to the name now decorated with location

		}
	}

	bool bShowDot = false;

	char szfinalcolor[VOICE_NOTICE_TEXT_MAX];
	if (pLocalPlayer && pLocalPlayer->ShouldShowTeamPlayerColors(iChatterTeam))
	{
		int pidx = -1;
		int playerIndex = 0;
		for (int i = 0; i <= MAX_PLAYERS; i++)
		{
			CBasePlayer* pCheckPlayer = UTIL_PlayerByIndex(i);
			if (pCheckPlayer && pCheckPlayer == pChatter)
				playerIndex = i;
		}

		if (playerIndex > 0)
		{
			C_CS_PlayerResource* pCSPR = (C_CS_PlayerResource*)g_PR;
			if (pCSPR)
				pidx = pCSPR->GetCompTeammateColor(playerIndex);
		}

		if (pidx > -1)
		{
			int nColorID = (pidx % 5);
			// put a colored dot next to a player name if they are on our team
			V_sprintf_safe(szfinalcolor, ALERT_NOTICE_FONT_TEAMMATE_STRING, GetTeammateColorCode(nColorID), GetChatTeammateColorGlyph(nColorID), GetTeamColorCodeForAdd(iChatterTeam), pChatterName);
			bShowDot = true;
		}
	}

	if (!bShowDot)
		V_snprintf(szfinalcolor, ARRAYSIZE(szfinalcolor), ALERT_NOTICE_FONT_STRING, GetTeamColorCodeForAdd(iChatterTeam), pChatterName);

	V_strncpy(pString, szfinalcolor, strLength);
}

void CCSGO_HudVoiceStatus::UpdateNotices(void)
{
	static const CPanoramaSymbol k_symShowAndHide("ShowAndHide");
	static const CPanoramaSymbol k_symQuickHide("QuickHide");
	static const CPanoramaSymbol k_symAlertHidden("AlertHidden");

	// limit how often we update this stuff because it doesn't have to be too
	// smooth
	if (gpGlobals->curtime > m_fNextUpdateTime)
	{
		m_fNextUpdateTime = gpGlobals->curtime + NOTICE_UPDATE_INTERVAL;

		// go through the visible list, setting positions as appropriate

		bool bANoticeIsSpawning = false;

		float newTextHeight = 0.0f;

		for (int i = 0; i < m_vecNoticeText.Count(); i++)
		{

			if (m_vecNoticeText[i].m_iState == NS_SPAWNING)
			{
				// Scroll all panels up once we know the text height of the new panel (calculated in OnLayoutTraverse)
				bool yOffsetPending = !(m_vecNoticeText[i].m_fY < 0.0f);
				if (yOffsetPending && m_vecNoticeText[i].m_pPanel->IsSizeValid())
				{
					newTextHeight = m_vecNoticeText[i].m_pPanel->GetContentHeight() / m_vecNoticeText[i].m_pPanel->GetActualUIScaleY();
					m_vecNoticeText[i].m_fTextHeight = newTextHeight;
					yOffsetPending = false;
//					Msg("Time to get size %.2f of panel %s %.2f\n", newTextHeight, m_vecNoticeText[i].m_pPanel->GetID(), gpGlobals->curtime - m_vecNoticeText[i].m_fStateTime);

					// There can be a large gap between setting state NS_SPAWNING and getting valid size 
					// (e.g. when updates get paused while chat window is showing)
					// So need to update statetime here, otherwise NS_IDLE can get set before the scroll is complete 
					m_vecNoticeText[i].m_fStateTime = gpGlobals->curtime;

				}

				// This notice is fading in and scrolling into position at the bottom of the panel
				// Once the scroll is complete (after m_fNotificationScrollLength seconds), set the status to NS_IDLE
				float t = (gpGlobals->curtime + NOTICE_UPDATE_INTERVAL - m_vecNoticeText[i].m_fStateTime) / m_fNotificationScrollLength;

				if (t >= 1 && !yOffsetPending)
				{
					// done spawning, now move to normal mode ( text just sits, and scrolls up )
					t = 1;
					m_vecNoticeText[i].m_iState = NS_IDLE;
				}
				else
				{
					// keep track of whether a notice is currently spawning
					// we can only add one notice at a time, so we need to know
					// if this one is still scrolling in. We'll wait until this stays false
					// to move a notice over from the pending list.
					bANoticeIsSpawning = true;

					if (t < 0)
						t = 0;
				}

			}

		}

		// now remove all the guys that are marked for removal

		for (int i = m_vecNoticeText.Count() - 1; i >= 0; i--)
		{
			if (m_vecNoticeText[i].m_pPanel->m_bRemove)
			{
				m_vecNoticeText[i].m_pPanel->SwitchClass("alertstate", k_symAlertHidden);
				m_vecNoticeText[i].m_pPanel->SetNoticeText("");

				// Starting position at bottom of panel
				m_vecNoticeText[i].m_pPanel->SetPositionWithoutTransition(CUILength::ZeroLength(), CUILength(m_flStatusPanelHeight - m_flMaxAlertHeight, CUILength::k_EUILengthLength), CUILength::ZeroLength());

//				Msg("Removing panel %s\n", m_vecNoticeText[i].m_pPanel->GetID());

				m_vecNoticeText[i].m_pPanel->m_bRemove = false;
				m_vecNoticeHandleCache.AddToTail(m_vecNoticeText[i].m_pPanel);
				m_vecNoticeText.Remove(i);
			}
			else if (newTextHeight > 0.0f)
			{
				// Scroll all notices up
				m_vecNoticeText[i].m_fY -= newTextHeight;

				float newPanelPos = m_flStatusPanelHeight + m_vecNoticeText[i].m_fY - m_flMaxAlertHeight;

				// Check for notice scrolling past the top of the panel.
				if ((m_vecNoticeText[i].m_iState != NS_FORCED_OUT) && (newPanelPos <= 0) && ((m_vecNoticeText[i].m_fSpawnTime + m_fNotificationLifetime - gpGlobals->curtime) > m_fNotificationScrollLength))
				{
					m_vecNoticeText[i].m_iState = NS_FORCED_OUT;
					m_vecNoticeText[i].m_fStateTime = gpGlobals->curtime;

					m_vecNoticeText[i].m_pPanel->SwitchClass("alertstate", k_symQuickHide);

//					Msg("Setting quickhide on panel %s\n", m_vecNoticeText[i].m_pPanel->GetID());
				}

				m_vecNoticeText[i].m_pPanel->SetPosition(CUILength::ZeroLength(),
					CUILength(newPanelPos, CUILength::k_EUILengthLength),
					CUILength::ZeroLength());

			}

		}


		// if we aren't currently bringing in a message, then
		// see if there are any pending ones to add
		if (!bANoticeIsSpawning)
		{
			int numPending = m_vecPendingNoticeText.Count();
			if (numPending)
			{
				NoticeText_t notice = m_vecPendingNoticeText[numPending-1];

				// pull the next pending notice and put it on the bottom of the notice stack
				numPending -= 1;
				m_vecPendingNoticeText.Remove(numPending);

				if ((gpGlobals->curtime - notice.m_fSpawnTime) < m_fNotificationLifetime)
				{
					notice.m_iState = NS_SPAWNING;
					notice.m_fSpawnTime = gpGlobals->curtime;
					notice.m_fStateTime = gpGlobals->curtime;

					notice.m_pPanel->SwitchClass("alertstate", k_symShowAndHide);

					m_vecNoticeText.AddToHead(notice);

//					CUILength xPos, yPos, zPos;
//					notice.m_pPanel->GetPosition(xPos, yPos, zPos);
//					Msg("Spawning panel %s time %.2f position %.2f \n", notice.m_pPanel->GetID(), gpGlobals->curtime, yPos.GetValue());
				}
				else
				{
					m_vecNoticeHandleCache.AddToTail(notice.m_pPanel);
				}
			}
		}

	}
	else if (m_fNextUpdateTime > gpGlobals->curtime + NOTICE_UPDATE_INTERVAL)
	{
		m_fNextUpdateTime = gpGlobals->curtime + NOTICE_UPDATE_INTERVAL;
	}
}


void CCSGO_HudVoiceStatus::SaveHideRestoreShow(bool bSaveAndHide)
{
	SetVisible(!bSaveAndHide);

	if (bSaveAndHide)
	{

		// TODO: Panorama call HideVoiceNotice

		// Hide voice status now - on restore/showPanel the next tick will take care of restoring it
// 		m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "HideVoiceNotice", NULL, 0);

		m_lastChattingEntIdx = -1;
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CCSGO_HudVoiceStatus::FireGameEvent(IGameEvent * event)
{
	const char *name = event->GetName();
	if (0 == V_strcmp(name, "player_reset_vote"))
	{
		int userId = engine->GetPlayerForUserID(event->GetInt("userid"));
		bool vote = event->GetBool("vote");
		int localIndex = GetLocalPlayerIndex();
		if (userId != localIndex && vote && g_PR)
		{
			wchar_t wszPlayerName[MAX_DECORATED_PLAYER_NAME_LENGTH];
			( ( C_CS_PlayerResource* )g_PR )->GetDecoratedPlayerName( userId, wszPlayerName, sizeof( wszPlayerName ), k_EDecoratedPlayerNameFlag_Simple | k_EDecoratedPlayerNameFlag_HTMLEscapeString ); // $$REI TODO figure out what to do here

			wchar_t wszMessage[1024] = { 0 };
			char szMessage[1024] = { 0 };
			g_pVGuiLocalize->ConstructString(wszMessage, sizeof(wszMessage), g_pVGuiLocalize->Find("#SFUI_Player_Wants_Restart"), 1, wszPlayerName);
			V_wcstostr(wszMessage, -1, szMessage, sizeof(szMessage));
			PushNotice(szMessage, localIndex);
		}
	}
}

void CCSGO_HudVoiceStatus::ClearNotices(void)
{
	static const CPanoramaSymbol k_symAlertHidden("AlertHidden");

	FOR_EACH_VEC_BACK(m_vecNoticeText, i)
	{
		m_vecNoticeText[i].m_pPanel->SetNoticeText("");
		m_vecNoticeText[i].m_pPanel->SwitchClass("alertstate", k_symAlertHidden);

		// Starting position at bottom of panel
		m_vecNoticeText[i].m_pPanel->SetPositionWithoutTransition(CUILength::ZeroLength(), CUILength(m_flStatusPanelHeight - m_flMaxAlertHeight, CUILength::k_EUILengthLength), CUILength::ZeroLength());

		m_vecNoticeHandleCache.AddToTail(m_vecNoticeText[i].m_pPanel);
		m_vecNoticeText.Remove(i);
	}

	FOR_EACH_VEC_BACK(m_vecPendingNoticeText, i)
	{
		m_vecNoticeHandleCache.AddToTail(m_vecPendingNoticeText[i].m_pPanel);
		m_vecPendingNoticeText.Remove(i);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reads in a player's Radio text from the server
//-----------------------------------------------------------------------------
bool CCSGO_HudVoiceStatus::MsgFunc_RadioText(const CCSUsrMsg_RadioText &msg)
{
	CHudChat* pChat = (CHudChat*)(GetHud(0).FindElement("CHudChat"));

	if (pChat)
	{
		pChat->MsgFunc_RadioText(msg);
	}

	return true;
}

bool CCSGO_HudVoiceStatus::MsgFunc_SayText(const CCSUsrMsg_SayText &msg)
{
	CHudChat* pChat = (CHudChat*)(GetHud(0).FindElement("CHudChat"));

	if (pChat)
	{
		pChat->MsgFunc_SayText(msg);
	}

	return true;
}

bool CCSGO_HudVoiceStatus::MsgFunc_SayText2(const CCSUsrMsg_SayText2 &msg)
{
	CHudChat* pChat = (CHudChat*)(GetHud(0).FindElement("CHudChat"));

	if (pChat)
	{
		pChat->MsgFunc_SayText2(msg);
	}

	return true;
}

bool CCSGO_HudVoiceStatus::MsgFunc_TextMsg(const CCSUsrMsg_TextMsg &msg)
{
	CHudChat* pChat = (CHudChat*)(GetHud(0).FindElement("CHudChat"));

	if (pChat)
	{
		pChat->MsgFunc_TextMsg(msg);
	}

	return true;
}

bool CCSGO_HudVoiceStatus::MsgFunc_RawAudio(const CCSUsrMsg_RawAudio &msg)
{
	CHudChat* pChat = (CHudChat*)(GetHud(0).FindElement("CHudChat"));

	if (pChat)
	{
		pChat->MsgFunc_RawAudio(msg);
	}

	return true;
}

// PANORAMA TODO: Refresh avatar image on device reset
// void CCSGO_HudVoiceStatus::DeviceReset(void *pDevice, void *pPresentParameters, void *pHWnd)
// {
// 	if (FlashAPIIsValid())
// 	{
// 		WITH_SLOT_LOCKED
// 		{
// 			m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "RefreshAvatarImage", NULL, 0);
// 		}
// 	}
// }

