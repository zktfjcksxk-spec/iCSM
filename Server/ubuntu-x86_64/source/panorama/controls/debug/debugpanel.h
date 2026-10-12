//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef DEBUGPANEL_H
#define DEBUGPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/iuiengine.h"
#include "panorama/input/iuiinput.h"
#include "panorama/controls/panel2d.h"
#include "panorama/controls/panelptr.h"
#include "panorama/uievent.h"
#include "debugpanelstyle.h"

namespace panorama
{

DECLARE_PANORAMA_EVENT0( ShowDebugStyle );
DECLARE_PANORAMA_EVENT0( ShowDebugComputed );

class CButton;
class CLabel;
class CToggleButton;

//-----------------------------------------------------------------------------
// Purpose: Base panel that contains all other displayed debug panels
//-----------------------------------------------------------------------------
class CDebugPanel : public CPanel2D
{
	DECLARE_PANEL2D( CDebugPanel, CPanel2D );

public:
	CDebugPanel( CPanel2D *pParent, const char *pchName );
	virtual ~CDebugPanel();

	// events
	bool OnShowDebugStyle();
	bool OnShowDebugComputed();
	bool OnDebugStyleStaus( bool bUpdated );

private:
	void HideAllPages();

	CPanel2D *m_pPanelPages;
	CPanel2D *m_pStyle;
	CPanel2D *m_pComputed;
	CLabel *m_pPanelStyleInvalid;
};


//-----------------------------------------------------------------------------
// Purpose: Shows computed styles for a panel
//-----------------------------------------------------------------------------
class CDebugPanelComputed : public CPanel2D
{
	DECLARE_PANEL2D( CDebugPanelComputed, CPanel2D );

public:
	CDebugPanelComputed( CPanel2D *pParent, const char *pchName );
	virtual ~CDebugPanelComputed();

	void Build();
	bool OnSetDebugTarget( CPanelPtr< CPanel2D > pPanel );

	void AutoReload();

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif

private:
	CLabel *AddRow( const char *pchName );
	void CreateControls();

	CPanelPtr< CPanel2D > m_pDebugPanel;
	CUIScheduledDel m_scheduledReload;

	bool m_bCreatedControls;
	
	// map for property value pointers
	CUtlMap< CStyleSymbol, CLabel *, int, CDefLess< CStyleSymbol > > m_mapPropertyLabels;

	// measurement labels
	CLabel *m_pDesiredLayoutWidth;
	CLabel *m_pDesiredLayoutHeight;
	CLabel *m_pActualLayoutWidth;
	CLabel *m_pActualLayoutHeight;
	CLabel *m_pContentWidth;
	CLabel *m_pContentHeight;
	CLabel *m_pActualRenderWidth;
	CLabel *m_pActualRenderHeight;
	CLabel *m_pActualXOffset;
	CLabel *m_pActualYOffset;
	CLabel *m_pContentsXScrollOffset;
	CLabel *m_pContentsYScrollOffset;
	CLabel *m_pActualUIScaleX;
	CLabel *m_pActualUIScaleY;
	CLabel *m_pActualUIScaleZ;
	CLabel *m_pMiscLayoutFile;
	CLabel *m_pMiscTabIndex;
	CLabel *m_pMiscSelectionPos;
	CLabel *m_pHitTest;
	CLabel *m_pHitTestChildren;
	CLabel *m_pCachedCommandList;
	CLabel *m_pCommandListBytesSize;
	CLabel *m_pRepaintRate;
};

} // namespace panorama

#endif // DEBUGPANEL_H
