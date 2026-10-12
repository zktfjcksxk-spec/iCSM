//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UIDEBUGGER_H
#define UIDEBUGGER_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/iuiengine.h"
#include "panorama/input/iuiinput.h"
#include "panorama/controls/panel2d.h"
#include "panorama/controls/panelptr.h"
#include "panorama/uievent.h"

namespace panorama
{

DECLARE_PANORAMA_EVENT1( SetDebugTarget, CPanelPtr< CPanel2D > );
DECLARE_PANORAMA_EVENT2( OpenFileForEdit, const char *, uint );
DECLARE_PANORAMA_EVENT1( ShowDebugDevInfo, bool );

DECLARE_PANORAMA_EVENT0( UpdateJSConsolePriorHistory );
DECLARE_PANORAMA_EVENT0( UpdateJSConsoleNextHistory );

DECLARE_PANORAMA_EVENT0( PanoramaDebuggerOpened );
DECLARE_PANORAMA_EVENT0( PanoramaDebuggerClosed );

class CButton;
class CToggleButton;
class CDebugLayout;

bool GetDebugPanelName( char *pchBuffer, uint cubBuffer, IUIPanel *pPanel );


//-----------------------------------------------------------------------------
// Purpose: Base panel that contains all other displayed debug panels
//-----------------------------------------------------------------------------
class CDebugger : public CPanel2D, CDefaultInputCapture
{
	DECLARE_PANEL2D( CDebugger, CPanel2D );

public:
	CDebugger( IUIWindow *pWindow, const char *pchName );
	virtual ~CDebugger();

	bool OnRefresh();
	bool OnPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	void OnPanelAdded( IUIPanel *pPanel );
	void OnPanelDeleted( IUIPanel *pPanel, IUIPanel *pOldParent );

	void BeginInspect();
	void ForceEndInspect();
	float GetSplitterPosition();
	void SetSplitterPosition( float flParentFlowValue );

	// from IInputCapture
	virtual bool OnCapturedMouseHover( IUIPanel *pPanel ) OVERRIDE;
	virtual bool OnCapturedMouseButtonDown( IUIPanel *pPanel, const MouseData_t &code ) OVERRIDE;
	virtual bool OnCapturedMouseButtonUp( IUIPanel *pPanel, const MouseData_t &code ) OVERRIDE;
	virtual bool OnCapturedMouseButtonDoubleClick( IUIPanel *pPanel, const MouseData_t &code ) OVERRIDE;
	virtual bool OnCapturedMouseButtonTripleClick( IUIPanel *pPanel, const MouseData_t &code ) OVERRIDE;
	virtual bool OnCapturedMouseWheel( IUIPanel *pPanel, const MouseData_t &code ) OVERRIDE;

	// Handler for JS console input
	bool OnTextEntrySubmit( const CPanelPtr< IUIPanel > &pPanel, const char *pchText );
	bool OnJSConsoleOutput( CPanelPtr< IUIPanel > pPanel, const char *pchText );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

private:
	void ReleaseInputCapture();
	void SetDebugPanel( IUIPanel *pPanel, bool bNotifyOtherPanels = true );
	void RebuildLayoutAsync();

	// events
	bool OnBubbledSetDebugTarget( CPanelPtr< CPanel2D > pPanel );
	bool OnOpenFileForEdit( const char *pchFile, uint unLine );
	bool OnInMemoryFileUpdate( CPanoramaSymbol symFile, uint location, uint unOldSize, uint unNewSize );
	bool OnInMemoryFilesSaved();
	bool OnStyleFileReloaded( CPanoramaSymbol symFile );
	bool OnRebuildLayout();

	bool OnUpdateJSConsolePriorHistory();
	bool OnUpdateJSConsoleNextHistory();

	CPanelPtr< IUIPanel > m_pDebugPanel;
	CPanelPtr< IUIPanel > m_pDebugPanelLastNotify;
	CPanelPtr< IUIPanel > m_pLastJSContext;
	bool m_bHasInputCapture;
	bool m_bAsyncRebuildLayout;

	int m_iLastJSConsoleListIndex;
	CUtlLinkedList< CUtlString, int > m_listJSConsoleHistory;

	CButton *m_pInspectButton;
	CButton *m_pPaintInfoButton;
	CButton *m_pSaveButton;
	CButton *m_pRevertButton;
	CToggleButton *m_pDevInfoButton;
	CDebugLayout *m_pDebugLayout;
};


//-----------------------------------------------------------------------------
// Purpose: Implements a vertical splitter control
//-----------------------------------------------------------------------------
class CVerticalSplitter : public CPanel2D
{
	DECLARE_PANEL2D( CVerticalSplitter, CPanel2D );

public:
	CVerticalSplitter( CPanel2D *pParent, const char *pchName );
	virtual ~CVerticalSplitter();

	virtual bool OnMouseButtonDown( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonUp( const MouseData_t &code ) OVERRIDE;
	virtual void OnMouseMove( float flMouseX, float flMouseY ) OVERRIDE;

	virtual EMouseCursors GetMouseCursor() OVERRIDE { return eMouseCursor_SizeWE; }

private:
	double m_flLastUpdate;
	bool m_bMoving;
};



//-----------------------------------------------------------------------------
// Purpose: Implements a vertical splitter control
//-----------------------------------------------------------------------------
class CHorizontalSplitter : public CPanel2D
{
	DECLARE_PANEL2D( CHorizontalSplitter, CPanel2D );

public:
	CHorizontalSplitter( CPanel2D *pParent, const char *pchName );
	virtual ~CHorizontalSplitter();

	virtual bool OnMouseButtonDown( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonUp( const MouseData_t &code ) OVERRIDE;
	virtual void OnMouseMove( float flMouseX, float flMouseY ) OVERRIDE;

	virtual EMouseCursors GetMouseCursor() OVERRIDE { return eMouseCursor_SizeNS; }

private:
	float m_flLastUpdate;
	bool m_bMoving;
};


} // namespace panorama

#endif // UIDEBUGGER_H