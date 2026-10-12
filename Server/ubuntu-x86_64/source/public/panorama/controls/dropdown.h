//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_DROPDOWN_H
#define PANORAMA_DROPDOWN_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"

namespace panorama
{

DECLARE_PANEL_EVENT0( DropDownSelectionChanged );
DECLARE_PANORAMA_EVENT2( DropDownMenuClosed, bool, CPanelPtr< CPanel2D > );
DECLARE_PANORAMA_EVENT0( DropDownSelectNext );
DECLARE_PANORAMA_EVENT0( DropDownSelectPrevious );

class CDropDownMenu;
class CDropDownMenuBackground;

//-----------------------------------------------------------------------------
// Purpose: Drop Down Control
//-----------------------------------------------------------------------------
class CDropDown : public CPanel2D
{
	DECLARE_PANEL2D( CDropDown, CPanel2D );

public:
	CDropDown( CPanel2D *parent, const char * pchPanelID );
	virtual ~CDropDown();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE;
	void AddOption( CPanel2D *pPanel );
	bool HasOption( const char *pchID );
	void RemoveOption( const char *pchID );
	void RemoveAllOptions();

	void FindOptionIDsByClass( const char *pchClassName, CUtlVector< CUtlString > &vecIDsOut );
	void SortOptions( std::function< int( const IUIPanelClient *, const IUIPanelClient * ) > fnCompare );

	CPanel2D *GetSelected() { return m_pSelected.Get(); }
	void SetSelected( const char *pchID, bool bNotify );
	void SetSelected( int nIndex, bool bNotify );
	void SetSelected( const char *pchID ) { return SetSelected( pchID, true ); }
	void SetSelected( int nIndex) { return SetSelected( nIndex, true ); }
	void SetDefault( const char *pchID ) { m_strDefaultSelection.Set( pchID ); }
	void ResetToDefault( bool bNotify );
	void ClearSelection();

	CPanel2D *AccessDropDownMenu() { return (CPanel2D*)m_pMenu.Get(); }

	virtual bool OnMouseButtonDown( const MouseData_t &mouseData ) OVERRIDE;
	virtual bool OnClick( IUIPanel *pPanel, const MouseData_t &code ) OVERRIDE;
	virtual void OnResetToDefaultValue();
	
	// Call if you know you've changed the contents of one of the option panels
	void InvalidateOptions( bool bForceReload );

	CPanel2D *FindDropDownMenuChild( const char *pchID );
	virtual bool BIsClientPanelEvent( CPanoramaSymbol symProperty ) OVERRIDE;

	int GetNumOptions();
	CPanel2D *GetOptionByIndex( int nIndex );

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif

protected:
	bool EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool EventDropDownMenuClosed( bool bSelectionChanged, CPanelPtr< CPanel2D > pPanel );
	bool EventResetToDefault( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );
	bool EventDropDownSelectNext( void );
	bool EventDropDownSelectPrevious( void );

	virtual void OnInitializedFromLayout() OVERRIDE;
	void ShowMenu();
	void UpdateSelectedChild( bool bSuppressChangedEvent, bool bInvalidateAlways = false );

	virtual void OnStylesChanged() OVERRIDE;

	CPanelPtr< CDropDownMenu > GetMenu() { return m_pMenu; }

private:
	CPanelPtr< CDropDownMenuBackground >m_pMenuBackground;
	CPanelPtr< CDropDownMenu >m_pMenu;
	CPanelPtr<CPanel2D> m_pSelected;
	bool m_bSuppressClick;

	CUtlString m_strInitialSelection;
	CUtlString m_strDefaultSelection;
};


//-----------------------------------------------------------------------------
// Purpose: Drop Down Menu (shown when activated)
//-----------------------------------------------------------------------------
class CDropDownMenu : public CPanel2D
{
	DECLARE_PANEL2D( CDropDownMenu, CPanel2D );

public:
	CDropDownMenu( CDropDown *pDropDown, const char * pchPanelID );
	virtual ~CDropDownMenu();

	void Show();
	void Close() { Hide( false ); }

	void SetMenuBackground( CDropDownMenuBackground *pBackground );

	CPanel2D *GetSelectedChild();
	void SetSelected( const char *pchID );
	void SetSelected( int nIndex );
	void SelectOption( const char *pchID );
	void SelectOption( int nIndex );
	void AddOption( CPanel2D *pPanel );
	bool HasOption( const char *pchID );
	void RemoveOption( const char *pchID );
	void RemoveAll();
	void ClearSelection();

	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE;
	virtual void OnResetToDefaultValue();
	virtual IUIPanel *GetLocalizationParent() const OVERRIDE { return m_pDropDown->UIPanel(); }

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif

protected:
	bool EventPanelActivated( const CPanelPtr< IUIPanel > &ptrPanel, EPanelEventSource_t eSource );
	bool EventCancelled( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool EventResetToDefault( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );
	bool EventInputFocusSet( const panorama::CPanelPtr< IUIPanel > &ptrPanel );
	bool EventInputFocusLost( const panorama::CPanelPtr< IUIPanel > &ptrPanel );

private:
	void Hide( bool bSelectionChanged );

	CDropDown *m_pDropDown;
	CPanel2D *m_pSelectedChild;
	CPanelPtr< CDropDownMenuBackground >m_pMenuBackground;
	bool m_bFirstLayoutAfterShow;
};

//-----------------------------------------------------------------------------
// Purpose: Drop Down Menu Background panel (shown when dropdown menu activated)
//			responsible for filtering input and closing dropdown menu if clicked on
//-----------------------------------------------------------------------------
class CDropDownMenuBackground : public CPanel2D
{
	DECLARE_PANEL2D( CDropDownMenuBackground, CPanel2D );

public:
	CDropDownMenuBackground( CDropDownMenu *pDropDownMenu, const char * pchPanelID );
	virtual ~CDropDownMenuBackground();

	virtual bool OnClick( IUIPanel *pPanel, const panorama::MouseData_t &code );

private:

	CPanelPtr< CDropDownMenu > m_pMenu;
};


} // namespace panorama

#endif // PANORAMA_DROPDOWN_H
