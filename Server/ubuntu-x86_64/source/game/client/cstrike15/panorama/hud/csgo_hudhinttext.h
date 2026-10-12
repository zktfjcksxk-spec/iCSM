//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Hint text: panel for displaying hint text sent from the server (through CEnvHudHint or CHintMessageQueue systems) or client (CCenterPrint)
// 
//=============================================================================//

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"
#include "panorama/hud/csgo_hud.h"
#include "vguicenterprint.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCSGO_HudHintText : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudHintText, panorama::CPanel2D );

public:

	enum { k_eMaxHintSizeBytes = 256 };

	/*
	enum EHintType
	{  
		k_eHintType_None = 0,		// No hint currently displayed
		k_eHintType_Info,			// Low priority info eg "You are carrying the hostage"
		k_eHintType_Priority,		// Higher priority info eg "You inujured a hostage!"
	};
	*/

	explicit CCSGO_HudHintText( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudHintText();

	virtual bool ShouldDraw( void ) OVERRIDE;

	// CHudElement overrides
	virtual void Think( void ) OVERRIDE;

	bool MsgFunc_HintText( const CCSUsrMsg_HintText &msg );
	bool MsgFunc_KeyHintText( const CCSUsrMsg_KeyHintText &msg );

protected:
	void SetShown( bool bShown );

	bool SetHintText( const char* szHintText, CCenterPrint::EPriority eType = CCenterPrint::k_EPriority_Low );
	bool OnPanoramaGameTimeJumpEvent( float flTimeJump );

	bool EventOnMatchStart();

	CUserMessageBinder m_UMCMsgHintText;
	CUserMessageBinder m_UMCMsgKeyHintText;
	CountdownTimer	m_hintDisplayTime;
	CCenterPrint::EPriority m_curHintType;

	panorama::CPanelPtr< panorama::CLabel > m_pHintLabel;
	panorama::CPanelPtr< panorama::CImagePanel > m_pHintIcon;

};

DECLARE_PANORAMA_EVENT2( ShowCenterPrintText, const char*, CCenterPrint::EPriority );