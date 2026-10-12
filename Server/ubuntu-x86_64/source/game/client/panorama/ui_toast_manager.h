//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"

DECLARE_PANEL_EVENT0( DismissToast );
DECLARE_PANEL_EVENT0( ToastShown );
DECLARE_PANEL_EVENT0( ToastHidden );

class CUI_ToastManager : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CUI_ToastManager, panorama::CPanel2D );

public:
	CUI_ToastManager( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CUI_ToastManager();

	void QueueToast( panorama::CPanel2D *pToast );

	void RemoveToast( panorama::CPanel2D *pToast );
	void RemoveAllToasts();

	void SetToastDuration( float flDuration );
	void SetMaxToastsVisible( int nMaxToastsVisible );

	bool IsToastVisible( panorama::CPanel2D *pToast ) const;

	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

private:
	void CheckShowNewToasts();
	void AsyncDeleteToast( panorama::CPanel2D *pToast );


	bool EventCheckShowNewToasts();
	bool EventDismissToast( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr );
	bool EventReadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr );

	float m_flToastDuration;
	float m_flLastToastShowTime;
	int m_nMaxToastsVisible;
};

