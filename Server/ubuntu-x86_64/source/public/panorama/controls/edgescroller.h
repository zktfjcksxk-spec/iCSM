//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef EDGE_SCROLLER_H
#define EDGE_SCROLLER_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/controls/panel2d.h"
#include "panorama/controls/scrollbar.h"

namespace panorama
{

class CEdgeScrollBar;
class CUIEdgeScrollBar;
class CButton;

DECLARE_PANORAMA_EVENT1( EdgeScrollerLeft, int );
DECLARE_PANORAMA_EVENT1( EdgeScrollerRight, int );
DECLARE_PANORAMA_EVENT1( EdgeScrollerUp, int );
DECLARE_PANORAMA_EVENT1( EdgeScrollerDown, int );

//-----------------------------------------------------------------------------
// Purpose: Control that shows buttons on the edges to scroll rather than a normal scroll bar
//-----------------------------------------------------------------------------
class CEdgeScroller : public CPanel2D
{
	DECLARE_PANEL2D( CEdgeScroller, CPanel2D );

public:
	CEdgeScroller( CPanel2D *pParent, const char *pchID );
	virtual ~CEdgeScroller();

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties ) OVERRIDE;


	// Callback to client panel to create a scrollbar
	virtual IUIScrollBar *CreateNewVerticalScrollBar( float flInitialScrollPos ) OVERRIDE;
	virtual IUIScrollBar *CreateNewHorizontalScrollBar( float flInitialScrollPos ) OVERRIDE;

protected:
	bool EventEdgeScrollerLeft( int cRepeat );
	bool EventEdgeScrollerRight( int cRepeat );
	bool EventEdgeScrollerUp( int cRepeat );
	bool EventEdgeScrollerDown( int cRepeat );

	bool EventInputFocusSet( const CPanelPtr< IUIPanel > &ptrPanel );

	bool m_bPageScroll;
	CPanelPtr< IUIPanel > m_pFocusedChild;
};


//-----------------------------------------------------------------------------
// Purpose: Scrollbar that just shows buttons on the edges of the panel rather than an actual Scrollbar
//-----------------------------------------------------------------------------
class CEdgeScrollBar : public CBaseScrollBar
{
	DECLARE_PANEL2D( CEdgeScrollBar, CBaseScrollBar );

public:
	CEdgeScrollBar( CPanel2D *pParent, const char *pchID, bool bHorizontal, bool bPageScroll );
	virtual ~CEdgeScrollBar();

	virtual void SetScrollWindowPosition( float flWindowPos, bool bImmediateMove = false ) OVERRIDE;
	virtual float GetInterpolatedScrollWindowPosition() OVERRIDE { return m_bHorizontal ? -GetParent()->GetInterpolatedXScrollOffset() : -GetParent()->GetInterpolatedYScrollOffset(); }


protected:
	virtual void UpdateLayout( bool bImmediateMove ) OVERRIDE;

	virtual void StopScroll() OVERRIDE;

	bool OnMouseScroll( const CPanelPtr< IUIPanel > &ptrPanel, int cRepeat );

private: 
	bool m_bHorizontal;

	CPanel2D *m_pMinButton;
	CPanel2D *m_pMaxButton;
};


} // namespace panorama

#endif // EDGE_SCROLLER_H
