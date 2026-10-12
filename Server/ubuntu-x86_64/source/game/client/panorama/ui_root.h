//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"

class CUI_PopupManager;
class CUI_TooltipManager;
class CUI_ContextMenuManager;

DECLARE_PANORAMA_EVENT0( GameSystemInit );
DECLARE_PANORAMA_EVENT0( GameSystemShutdown );
DECLARE_PANORAMA_EVENT0( GameEvent_CSMatchEndRestart )

//-----------------------------------------------------------------------------
// Purpose: Base class for the root panel within an IUIWindow.
//-----------------------------------------------------------------------------
class CUI_Root : public panorama::CPanel2D, public CGameEventListener
{
	DECLARE_PANEL2D( CUI_Root, panorama::CPanel2D );

public:
	CUI_Root( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CUI_Root();

	static CUI_Root *GetRootForWindow( panorama::IUIWindow *pWindow );

	static int GetRootCount() { return s_vecWindowRootMap.Count(); }
	static CUI_Root *GetRootByIndex( int i ) { return ( i >= 0 && i < s_vecWindowRootMap.Count() ) ? s_vecWindowRootMap[ i ].pRoot : nullptr; }

	void ShowGenericPopup( const char *pszTitle, const char *pszMessage );
	CUI_PopupManager *GetPopupManager( void ) { return m_pPopupManager; }
	void SetPopupManager( CUI_PopupManager *pPopupManager ) { m_pPopupManager = pPopupManager; }

	CUI_TooltipManager *GetTooltipManager( void ) { return m_pTooltipManager; }
	void SetTooltipManager( CUI_TooltipManager *pTooltipManager ) { m_pTooltipManager = pTooltipManager; }

	CUI_ContextMenuManager *GetContextMenuManager( void ) { return m_pContextMenuManager; }
	void SetContextMenuManager( CUI_ContextMenuManager *pContextMenuManager ) { m_pContextMenuManager = pContextMenuManager; }

	virtual void OnUIScaleFactorChanged( const Vector &vOldScaleFactor, const Vector &vNewScaleFactor ) OVERRIDE;

	virtual void FireGameEvent( IGameEvent * event ) OVERRIDE;

private:
	void UpdateVisualQuality();
	void UpdatePartnerType();
	void UpdateCurrentSeason();

	bool EventSettingsChanged();
	bool EventWelcomeMessageReceived();
	bool EventIngameEventsUpdated();

	bool OnTopLevelWindowVisibilityChanged( panorama::IUIWindow* pWindow );

	int m_event_colorblind_mode_changed;

	CUI_PopupManager *m_pPopupManager;
	CUI_TooltipManager *m_pTooltipManager;
	CUI_ContextMenuManager *m_pContextMenuManager;

	// Mapping of IUIWindow -> Root. Using a vector rather than a map because
	// given that the max size is probably ~4 it's likely quicker to just do
	// the linear search.
	struct SWindowRootEntry
	{
		panorama::IUIWindow *pWindow;
		CUI_Root *pRoot;
	};
	static CUtlVector< SWindowRootEntry > s_vecWindowRootMap;
};

void UpdateAutomaticVideoSettingClasses( panorama::CPanel2D *pPanel );
