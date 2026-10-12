//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Matchmaking status dialog interface
//          along with SF and Panorama implementations
//
//=============================================================================//

#ifndef GAMEUI_MATCHMAKINGSTATUS_H
#define GAMEUI_MATCHMAKINGSTATUS_H

#include "matchmaking/imatchevents.h"

#ifdef PANORAMA_ENABLE
#include "panorama/popups/ui_popup_generic.h"
#endif

#ifdef INCLUDE_SCALEFORM
#include "messagebox_scaleform.h"
#endif

class IMatchmakingStatus : public IMatchEventsSink
{
public:
	IMatchmakingStatus();
	~IMatchmakingStatus();

	void SetTimeToAutoCancel( double dblPlatFloatTime );

	// Implementation to be overridden in Panorama/Scaleform-specific classes
	virtual void Hide() = 0;
	virtual void SetMessage( const char *pszMessage ) = 0;

protected:
	// OnCancel - cancels pending matchmaking requests
	void OnCancel();

	// IMatchEventsSink override
	virtual void OnEvent( KeyValues *pEvent ) OVERRIDE;

protected:
	bool	m_bErrorEncountered;
	double	m_dblTimeToAutoCancel;

};

#ifdef PANORAMA_ENABLE

class CUI_MMStatus_Popup : public IMatchmakingStatus, public CUI_Popup_Generic
{
public:
	CUI_MMStatus_Popup( panorama::CPanel2D *pParent, const char *pchID, CPanel2D *pEventParent, char const *szCustomTitle, char const *szCustomText );

	// CUI_Popup_Generic overrides
	virtual void HandlePopupButtonClicked( const char *pchEventText );

protected:

	// IMatchmakingStatus overrides
	virtual void Hide();
	virtual void SetMessage( const char *pszMessage );

private:

	panorama::CLabel * m_pMessageLabel;

};

#endif

#ifdef INCLUDE_SCALEFORM

class CMatchmakingStatus : public IMatchmakingStatus, public IMessageBoxEventCallback
{
public:
	CMatchmakingStatus( char const *szCustomTitle, char const *szCustomText );
	~CMatchmakingStatus();

	// IMessageBoxEventsCallback implementation
	virtual bool OnMessageBoxEvent( MessageBoxFlags_t buttonPressed );
	CMessageBoxScaleform *m_pMessageBoxInstance;

protected:
	// IMatchmakingStatus overrides
	virtual void Hide();
	virtual void SetMessage( const char *pszMessage );

};

#endif

#endif // GAMEUI_MATCHMAKINGSTATUS_H