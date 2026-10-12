//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CONTEXTMENU_H
#define CONTEXTMENU_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/controls/panel2d.h"

DECLARE_PANEL_EVENT1( ContextMenuEvent, const char * )
DECLARE_PANEL_EVENT1( ContextMenuEventDirect, panorama::IUIEvent * );
DECLARE_PANORAMA_EVENT0( DismissAllContextMenus );

namespace panorama 
{

//-----------------------------------------------------------------------------
// Purpose: Helper class to derive from for creating context menus
//-----------------------------------------------------------------------------
class CContextMenu : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CContextMenu, panorama::CPanel2D );

public:
	CContextMenu( CPanel2D *pParent, const char *pchName, CPanel2D *pEventParent );
	CContextMenu( IUIWindow *pParent, const char *pchName, CPanel2D *pEventParent );
	virtual ~CContextMenu();
	virtual bool OnClick( IUIPanel *pPanel, const panorama::MouseData_t &code );

	void SetMenuTarget( const CPanelPtr< IUIPanel >& targetPanelPtr );

	void CalculatePosition() { m_bReposition = true; InvalidateSizeAndPosition(); }

	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE;

	CPanel2D *GetEventParent() { return m_pEventParent.Get(); }

	virtual void Close();

	void SetDismissEvent( IUIEvent *pEvent );

	void SetFadeoutTime( float flFadeoutTime ) { m_flFadeoutTime = flFadeoutTime; }
	float GetFadeoutTime() const { return m_flFadeoutTime; }

private:
	void Initialize( CPanel2D *pEventParent );

	bool OnFireEvent( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, const char *pchEventText );
	bool OnFireEvent( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, IUIEvent *pEvent );
	bool OnCancelled( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource );
	bool OnDismissAll();

	CPanelPtr< CPanel2D > m_pEventParent;
	CPanelPtr< IUIPanel > m_pMenuTarget;
	IUIEvent *m_pDismissEvent;
	double m_flCreateTime;
	float m_flFadeoutTime;
	bool m_bReposition;
};


//-----------------------------------------------------------------------------
// Purpose: Helper class for simple context menus that doesn't require derivation
//-----------------------------------------------------------------------------
class CSimpleContextMenu : public panorama::CContextMenu
{
	DECLARE_PANEL2D( CSimpleContextMenu, panorama::CContextMenu );

public:
	CSimpleContextMenu( CPanel2D *pParent, const char *pchName, CPanel2D *pEventParent  );
	CSimpleContextMenu( IUIWindow *pParent, const char *pchName, CPanel2D *pEventParent  );
	virtual ~CSimpleContextMenu();

	panorama::CPanel2D* AddMenuItem( const char *pchLabelText, const char *pchEventText );
	panorama::CPanel2D* AddMenuItemEvent( const char *pchLabel, IUIEvent *pEvent );

private:

};

} // namespace panorama

#endif // CONTEXTMENU_H