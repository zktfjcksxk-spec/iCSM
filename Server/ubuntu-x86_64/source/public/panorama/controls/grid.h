//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef GRID_H
#define GRID_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"
#include "panorama/controls/label.h"
#include "panorama/controls/mousescroll.h"

namespace panorama
{

DECLARE_PANEL_EVENT0( ReadyPanelForDisplay )
DECLARE_PANEL_EVENT0( PanelDoneWithDisplay )
DECLARE_PANEL_EVENT0( GridMotionTimeout );
DECLARE_PANEL_EVENT0( GridInFastMotion );
DECLARE_PANEL_EVENT0( GridStoppingFastMotion );
DECLARE_PANEL_EVENT0( GridPageLeft );
DECLARE_PANEL_EVENT0( GridPageRight );
DECLARE_PANEL_EVENT0( GridDirectionalMove );
DECLARE_PANEL_EVENT1( ChildIndexSelected, int );
DECLARE_PANEL_EVENT0( GridFlickTimeout );

//-----------------------------------------------------------------------------
// Purpose: Button
//-----------------------------------------------------------------------------
class CGrid : public CPanel2D
{
	DECLARE_PANEL2D( CGrid, CPanel2D );

public:
	CGrid( CPanel2D *parent, const char * pchPanelID );
	virtual ~CGrid();

	CPanel2D * AccessSelectedPanel() { return m_pFocusedChild.Get(); }

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// Scroll the grid so the focused panel is in the top left corner
	void MoveFocusToTopLeft();

	// Scroll the grid all the way to the left regardless of what's
	// focused.
	void ScrollPanelToLeftEdge();

	// give focus to this child in the grid without actually getting key focus from the ui framework
	void SetFocusedChild( CPanel2D *pPanel );

	// Trigger fast motion style temporarily, do this if you are directly setting focus ahead a bunch
	void TriggerFastMotion();
	void BumpFastMotionTimeout();

	void SetHorizontalCount( int nCount ) { SetHorizontalAndVerticalCount( nCount, m_nVerticalCount ); }
	void SetVerticalCount( int nCount ) { SetHorizontalAndVerticalCount( m_nHorizontalCount, nCount ); }
	int GetHorizontalCount() const { return m_nHorizontalCount; }
	int GetVerticalCount() const { return m_nVerticalCount; }

	virtual void GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties ) OVERRIDE;

	enum EScrollDirection
	{
		eScrollDirectionHorizontal,
		eScrollDirectionVertical
	};

	enum EMoveDirection
	{
		eMoveUp,
		eMoveDown,
		eMoveLeft,
		eMoveRight,
		eMoveNone,

	};

	void SetScrollDirection( EScrollDirection eScrollDirection );
	void SetMovementWrap( EMoveDirection  eMoveWrap ) { m_eMovementWrapDirection = eMoveWrap; }

	void SetFocusMargin( int nCount ) { m_nOverrideFocusMargin = nCount; InvalidateSizeAndPosition(); }
	int GetFocusMargin() const { return m_nOverrideFocusMargin; }

	float GetScrollProgress() const { return m_flScrollProgress; }

	virtual bool OnMoveUp( int nRepeats );
	virtual bool OnMoveDown( int nRepeats );
	virtual bool OnMoveRight( int nRepeats );
	virtual bool OnMoveLeft( int nRepeats );
	virtual bool OnTabForward( int nRepeats );
	virtual bool OnTabBackward( int nRepeats );
	virtual bool OnMouseButtonDown( const MouseData_t &code );
	virtual bool OnMouseButtonUp( const MouseData_t &code );
	virtual bool OnMouseWheel( const panorama::MouseData_t &code );
	virtual bool OnGamePadDown( const panorama::GamePadData_t &code );
	virtual bool OnKeyDown( const KeyData_t &code );

	virtual bool BRequiresContentClipLayer() OVERRIDE { return true; }

	virtual bool OnSetFocusToNextPanel( int nRepeats, EFocusMoveDirection moveType, bool bAllowWrap, float flTabIndexCurrent, float flXPosCurrent, float flYPosCurrent, float flXStart, float fYStart ) OVERRIDE
	{
		switch( moveType )
		{
		case k_ENextInTabOrder:
			if ( OnTabForward( nRepeats ) )
				return true;
			break;
		case k_ENextByXPosition:
			if ( OnMoveRight( nRepeats ) )
				return true;
			break;
		case k_EPrevInTabOrder:
			if ( OnTabBackward( nRepeats ) )
				return true;
			break;
		case k_EPrevByXPosition:
			if ( OnMoveLeft( nRepeats ) )
				return true;
			break;
		case k_ENextByYPosition:
			if ( OnMoveDown( nRepeats ) )
				return true;
			break;
		case k_EPrevByYPosition:
			if ( OnMoveUp( nRepeats ) )
				return true;
			break;
		default:
			break;
		}

		return false;
	}

	void SetHorizontalAndVerticalCount( int nHorizontalCount, int nVerticalCount );

	void SetIgnoreFastMotion( bool bValue ) { m_bIgnoreFastMotion = bValue; m_ulMotionSinceStart = 0; }


#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif

protected:
	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );
	virtual void OnBeforeChildrenChanged() { m_bForceRelayout = true; ++m_nChildChangeInProgress; EndFlick(); }

	virtual void OnChildStylesChanged() OVERRIDE { if ( !m_bComputingPositions) m_bVecVisibleDirty = true; }
	virtual void OnAfterChildrenChanged() OVERRIDE{ --m_nChildChangeInProgress; if ( !m_bComputingPositions ) m_bVecVisibleDirty = true; }

	// Allow overriding the status of scrolling for this panel
	virtual bool BCanCustomScrollUp() const OVERRIDE;
	virtual bool BCanCustomScrollDown() const OVERRIDE;
	virtual bool BCanCustomScrollLeft() const OVERRIDE;
	virtual bool BCanCustomScrollRight() const OVERRIDE;

	// Does the panel implement drag scroll?
	virtual bool BCustomCanDragScroll() const OVERRIDE;
	virtual bool BCustomScrollInProgress() OVERRIDE;


private:

	bool OnPageUp();
	bool OnPageDown();

	bool OnScrollToTop( const CPanelPtr< IUIPanel > &pPanel );
	bool OnScrollToBottom( const CPanelPtr< IUIPanel > &pPanel );

	void SetScrollDirection( const char *pchDirection ) { BSetScrollDirection( pchDirection ); }
	const char *GetScrollDirection() const { return (m_eScrollDirection == eScrollDirectionVertical) ? "vertical" : "horizontal"; }
	bool BSetScrollDirection( const char* pchValue );

	void UpdateVecVisible();
	int GetVisibleChildCount() const;
	CPanel2D *GetVisibleChild( int iVisibleIndex );

	void DoMouseDrag( float flStart, float flOffset );
	void StartFlick();
	void EndFlick();
	void MoveFocusIntoView();
	void StopScroll();

	// event handlers
	bool EventInputFocusSet( const CPanelPtr< IUIPanel > &ptrPanel );
	bool EventInputFocusLost( const CPanelPtr< IUIPanel > &ptrPanel );
	bool MotionTimeout( const CPanelPtr< IUIPanel > &ptrPanel );
	bool OnMouseScroll( const CPanelPtr< IUIPanel > &ptrPanel, int cRepeat );
	bool EventWindowCursorShown( IUIWindow *pWindow );
	bool EventWindowCursorHidden( IUIWindow *pWindow );

	bool EventDragScrollStart( const CPanelPtr< IUIPanel > &pPanel );
	bool EventDragScrollMouseMove( const CPanelPtr< IUIPanel > &pPanel, int nLastX, int nLastY, int nX, int nY );
	bool EventDragScrollEnd( const CPanelPtr< IUIPanel > &pPanel, int nLastX, int nLastY, float flVelocityX, float flVelocityY );
	bool EventGridFlickTimeout( const CPanelPtr< IUIPanel > &ptrPanel );

	void RegisterForCursorChanges();
	void UnregisterForCursorChanges();

	int GetFocusedChildVisibleIndex() const;
	void UpdateChildPositions( bool bForceTopLeft = false );

	bool MoveSelection( EMoveDirection eMove, int nRepeats );

	float ScrollAndDragOffsetsToPixelOffset( int nScrollOffset, float flDragScrollOffset ) const;
	void PixelOffsetToScrollAndDragOffsets( float flPixelOffset, int &nScrollOffset, float &flDragScrollOffset ) const;

	float GetCurrentScrollPosition();
	float GetFinalScrollPosition() const;


	bool m_bHadFocus;

	EScrollDirection m_eScrollDirection;
	EMoveDirection m_eMovementWrapDirection;
	EMoveDirection m_eLastMoveDirection;

	CPanelPtr< CPanel2D > m_pFocusedChild;
	CUtlVector< CPanelPtr<CPanel2D> > m_vecPanelsReadyForDisplay;

	int m_nScrollOffset;

	float m_flChildWidth;
	float m_flChildHeight;
	float m_flScaleOffset;

	float m_flScrollProgress;

	int m_nHorizontalCount;
	int m_nVerticalCount;
	
	// override how far from the edge we let the focused child move (unless it is on the edge of the grid itself)
	int m_nOverrideFocusMargin;

	bool m_bForceRelayout;

	bool m_bIgnoreFastMotion;
	double m_flStartedMotion;
	double m_flLastMotion;
	uint64 m_ulMotionSinceStart;
	bool m_bFastMotionStarted;
	bool m_bMotionLoopRunning;
	bool m_bVecVisibleDirty;
	bool m_bComputingPositions;
	double m_flLastFocusChangeTime;
	int m_nChildChangeInProgress;

	bool m_bMouseDown;
	bool m_bDragScrolling;
	float m_flDragScrollOffset;
	bool m_bFocusDueToDragScroll;
	bool m_bNextPositionImmediate;
	float m_flFlickFastMotionEnd;

	CUtlVector< CPanel2D * > m_vecVisibleChildren;
	
	panorama::CMouseScrollRegion *m_pLeftMouseScrollRegion;
	panorama::CMouseScrollRegion *m_pRightMouseScrollRegion;

};


} // namespace panorama

#endif // GRID_H
