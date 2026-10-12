//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"
#include "panorama/csgo_avatarimage.h"

#define ALERT_NOTICE_TEXT_MAX		2048 // max number of characters in a notice text

struct ActiveSpeaker
{
	int					playerId;
	bool				bSpeaking;
	float				fAlpha;
	bool				bIsAlive;
	int					nTeamNum;
	int					nSpeakerState;
};

class CCSGO_HudNotice : public panorama::CPanel2D
{
	DECLARE_PANEL2D(CCSGO_HudNotice, panorama::CPanel2D);
public:
	CCSGO_HudNotice(panorama::CPanel2D *pParent, const char *pchID);
	void SetNoticeText(const char *pchValue, panorama::CLabel::ETextType eTextType = panorama::CLabel::k_ETextTypeNone);
	bool m_bRemove;	// Removal flag
private:
	panorama::CLabel* m_pLabel;
	bool EventAnimationEnd(const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr, panorama::CPanoramaSymbol symAnimation);
};

class CCSGO_VoiceNotice : public panorama::CPanel2D
{
	DECLARE_PANEL2D(CCSGO_VoiceNotice, panorama::CPanel2D);
public:
	CCSGO_VoiceNotice(panorama::CPanel2D *pParent, const char *pchID);
	void ShowVoiceNotice(const char *pchVoiceText, ActiveSpeaker& activeSpeaker, uint64 xuid, char* pchPlayerName, bool bIsLocalPlayer);
	void HideVoiceNotice();
private:
	panorama::CLabel* m_pLabel;
	CCSGO_AvatarImage* m_pAvatarImage;
	panorama::CPanel2D* m_pSoundAnim;
};

//-----------------------------------------------------------------------------
// Purpose: CSGO Hud Voice and Status message panel
//-----------------------------------------------------------------------------
class CCSGO_HudVoiceStatus : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudVoiceStatus, panorama::CPanel2D );

public:
	CCSGO_HudVoiceStatus(panorama::CPanel2D *pParent, const char *pchID);
	virtual ~CCSGO_HudVoiceStatus();

	// CPanel2D overrides
	virtual void OnStylesChanged() OVERRIDE;

	// These overload the CHudElement class
	virtual void Think(void);
	virtual void LevelInit(void);
	virtual void LevelShutdown(void);
	virtual void SetActive(bool bActive);
	virtual bool ShouldDraw(void);

	virtual void FireGameEvent(IGameEvent *pEvent) OVERRIDE;

	void ClearNotices(void);

	int FindActiveSpeaker(int playerId);
	void ClearActiveList(void);

	// Add the text notice to our internal queue and trigger scroll/fade in animation
	void PushGlobalNotice(const char *szNoticeText, uint32 unAccountID);
	void PushGlobalNotice(const char *szNoticeText, uint32 unAccountID, char const *szNameResolved);
	void PushNotice(const char *szNoticeText, int clientId, bool bAlreadyFormattedAsCensoredAndSafeHTML = false);

	bool MsgFunc_RadioText(const CCSUsrMsg_RadioText &msg);
	bool MsgFunc_SayText(const CCSUsrMsg_SayText &msg);
	bool MsgFunc_SayText2(const CCSUsrMsg_SayText2 &msg);
	bool MsgFunc_TextMsg(const CCSUsrMsg_TextMsg &msg);
	bool MsgFunc_RawAudio(const CCSUsrMsg_RawAudio &msg);

	CUserMessageBinder m_UMCMsgRadioText;
	CUserMessageBinder m_UMCMsgSayText;
	CUserMessageBinder m_UMCMsgSayText2;
	CUserMessageBinder m_UMCMsgTextMsg;
	CUserMessageBinder m_UMCMsgRawAudio;


protected:
	void ShowPanel(const bool bShow);

	void ColorizeNotice(const char *szNotice, char *colorStr, int clientId);

	void GenerateVoiceText(OUT_Z_BYTECAP(strLength) char *pString, int strLength, C_BasePlayer *pChatter, const char *pChatterName, bool bIsAlive, int iTeam);

	// handles updating the location of current alert notices
	void UpdateNotices(void);
	void UpdateVoiceStatus(void);

	// Saves the notice state and hides, or restores the notice state and shows
	// bSave to true for save and hide, bSave to false for restore and show
	void SaveHideRestoreShow(bool bSaveAndHide);

	bool ChatMsgSendBroadcast(void * /* GCSDK::CProtoBufMsg< CMsgGCCStrike15_v2_ClientToGCChat > * */ pmsg);
	void ChatUnsubscribe();
	STEAM_CALLBACK(CCSGO_HudVoiceStatus, Steam_OnPersonaStateChange, PersonaStateChange_t, m_CallbackOnPersonaStateChange);

	void GetLayoutDefines();

	CUtlLinkedList< ActiveSpeaker > m_SpeakingList;
	ActiveSpeaker	m_SFSpeakerLabels[3];

	enum
	{
		NS_PENDING,
		NS_SPAWNING,
		NS_IDLE,
		NS_FORCED_OUT,
	} NOTICE_STATE;

	struct NoticeText_t
	{
		NoticeText_t() :
			m_pPanel(NULL),
			m_fTextHeight(0.f),
			m_fSpawnTime(0.f),
			m_fStateTime(0.f),
			m_fY(0.f),
			m_iState(NS_PENDING)
		{
			V_memset(m_szNotice, 0, sizeof(m_szNotice));
		}

		char					m_szNotice[ALERT_NOTICE_TEXT_MAX]; // Notice text
		CCSGO_HudNotice*		m_pPanel;
		float					m_fTextHeight;
		int						m_iState;
		float					m_fSpawnTime;
		float					m_fStateTime;
		float					m_fY;
	};

	panorama::CPanel2D*			m_pStatusPanel;
	panorama::CPanel2D*			m_pVoicePanel;
	float						m_flStatusPanelHeight;
	float						m_flMaxAlertHeight;
	float						m_fNotificationLifetime;	// Notification display time before fade out complete
	float						m_fNotificationScrollLength;
	float						m_fNextUpdateTime;
	CUtlVector<NoticeText_t>	m_vecNoticeText;			// Oldest notices at the end
	CUtlVector<NoticeText_t>	m_vecPendingNoticeText;		// Notices that are queued up to be displayed.
	CUtlVector<CCSGO_HudNotice*>		m_vecNoticeHandleCache;		// Cache of notice panel handles to reuse
															// (to avoid creating a new notice from scratch)

	int							m_lastChattingEntIdx;		// ID of the last player that was voice chatting
	bool						m_lastChatterWasAlive;
	int							m_lastChatterTeamNum;
	int							m_lastChatterSpeakerState;

	bool						m_bGlobalChatSubscriptionActive;
	double						m_dblNextSubscriptionHeartbeat;
	CUtlMap< uint32, char *, int, CDefLess< uint32 > > m_mapPendingChat;

	enum	SpeakerState
	{
		SpeakerState_Invalid = -1,
		SpeakerState_Auto = 0,
		SpeakerState_LowAudio = 1,
		SpeakerState_HighAudio = 2
	};

};
