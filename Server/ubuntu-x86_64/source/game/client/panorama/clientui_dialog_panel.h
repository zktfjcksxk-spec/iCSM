//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
//
//=============================================================================//
#pragma once

#include "gameeventlistener.h"
#include "panorama/controls/panel2d.h"
#include "panorama/layout/panel2dfactory.h"
#include "clientui.h"

DECLARE_PANORAMA_EVENT0( ClientUI_CloseDialog );
DECLARE_PANORAMA_EVENT1( ClientUI_FireOutput, const char * );
DECLARE_PANORAMA_EVENT2( ClientUI_FireOutputStr, const char *, const char * );


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class IClientUIDialogListener
{
public:
	virtual void OnDialogFireOutput( const char *pOutput, const char *pArg ) = 0;
	virtual void OnDialogEnded() = 0;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CClientUIDialogPanel: public panorama::CPanel2D
{
	DECLARE_PANEL2D( CClientUIDialogPanel, panorama::CPanel2D );

public:
	CClientUIDialogPanel( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CClientUIDialogPanel();

	void BeginDialog( IClientUIDialogListener *pListener, bool bIsUIPopup );
	void EndDialog();

	bool EventClientUI_CloseDialog();
	bool EventClientUI_FireOutput( const char *pOutput );
	bool EventClientUI_FireOutputStr( const char *pOutput, const char *pStr );
private:
	IClientUIDialogListener *m_pListener = nullptr;
	bool m_bActive = false;
	bool m_bIsUIPopup = false;
};
