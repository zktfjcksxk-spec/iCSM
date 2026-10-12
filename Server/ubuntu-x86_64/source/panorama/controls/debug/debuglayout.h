//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef DEBUGLAYOUTPANEL_H
#define DEBUGLAYOUTPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/iuiengine.h"
#include "panorama/input/iuiinput.h"
#include "panorama/controls/panel2d.h"
#include "panorama/controls/panelptr.h"
#include "tier1/utlhashmap.h"

namespace panorama
{

class CLabel;
class CToggleButton;

//-----------------------------------------------------------------------------
// Purpose: Displays panel hierarchy in XML
//-----------------------------------------------------------------------------
class CDebugLayout : public CPanel2D
{
	DECLARE_PANEL2D( CDebugLayout, CPanel2D );

public:
	CDebugLayout( CPanel2D *pParent, const char *pchName );
	virtual ~CDebugLayout();

	void ClearLayout();
	void Build( bool bForce = false );

	bool IsPanelOrParentInLayout( CPanel2D *pPanel );

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif

protected:
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );

	// kb/mouse management
	virtual bool OnKeyDown( const KeyData_t &code );

private:
	struct PanelDebugInfo_t
	{
		CPanelPtr< CPanel2D > m_pPanel;
		CPanelPtr< CPanel2D > m_pOpen;
		CPanelPtr< CPanel2D > m_pClose;
		CPanelPtr< CToggleButton > m_pToggle;
	};

	// events
	bool EventSetDebugTarget( CPanelPtr< CPanel2D > pPanel );
	bool EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool EventUpdateDirtyPanelInfo();
	bool EventUpdatePanelPaintInfo();
	bool EventPanelStyleClassesChanged( const CPanelPtr< IUIPanel > &pPanel );
	bool EventPanelStyleFlagsChanged( const CPanelPtr< IUIPanel > &pPanel );
	bool EventShowDebugDevInfo( bool bShow );
	bool EventBrowserGoToURL( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, const char * );

	void ExpandOrCollapse( CPanel2D *pPanel, bool bExpand );

	void AddLayoutForPanel( CPanel2D *pPanel );
	void AddPanelChildrenToLayout( CPanel2D *pPanel );
	PanelDebugInfo_t *AddCollapsedPanelToLayout( CPanel2D *pPanel, CPanel2D *pAfter, int nDepth );
	void AppendElementOpenTag( CLabel *pParent, CPanel2D *pPanel, bool bShowChildren );
	CPanel2D *AppendElementCloseRow( CPanel2D *pPanel, int nDepth );
	void UpdatePanelInfo( CPanel2D *pPanel );

	PanelDebugInfo_t *FindDebugInfo( CPanel2D *pTarget );
	PanelDebugInfo_t *FindDebugInfoForRow( CPanel2D *pRow );	

	CPanelPtr< CPanel2D > m_pDebugPanel;
	CPanelPtr< CPanel2D > m_pTopLevel;
	CUtlHashMap< CPanelPtr< CPanel2D >, PanelDebugInfo_t, CDefEquals< CPanelPtr< CPanel2D > > > m_mapPanelDebugInfo;
	bool m_bScrollToDebugPanel;
	bool m_bShowDevInfo;

	CUtlRBTree< CPanelPtr< IUIPanel >, int, CDefLess< CPanelPtr< IUIPanel > > > m_treePanelsDirty;
};

} // namespace panorama

#endif // DEBUGLAYOUTPANEL_H