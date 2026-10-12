//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/dropdown.h"
#include "panorama/controls/label.h"
#include "panorama/uijsregistration.h"
#include "panorama/iuisoundsystem.h"
#include "contextui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CDropDown, DropDown );
REGISTER_PANEL2D( CDropDownMenu, DropDownMenu );
REGISTER_PANEL2D( CDropDownMenuBackground, DropDownMenuBackground );

DEFINE_PANORAMA_EVENT( DropDownMenuClosed );
DEFINE_PANORAMA_EVENT( DropDownSelectionChanged );
DEFINE_PANORAMA_EVENT( DropDownSelectNext );
DEFINE_PANORAMA_EVENT( DropDownSelectPrevious );

static const char * k_pchMenuDisplayed( "DropDownMenuVisible" );

// "oninputsubmit" and "onuserinputsubmit"
// the former can be triggered by both user interaction, or setting selection from code
// the latter is only triggered by user interaction that leads to a change
static const char * k_pchOnInputSubmit( "oninputsubmit" );
static const char * k_pchOnUserInputSubmit( "onuserinputsubmit" );	


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDropDown::CDropDown( CPanel2D *parent, const char *pchPanelID ) : CPanel2D( parent, pchPanelID )
{
	m_bSuppressClick = false;
	m_pSelected = NULL;
	SetAcceptsFocus( true );

	// create the dropdown menu
	bool bHasID = (pchPanelID && pchPanelID[0] != '\0');
	m_pMenu = new CDropDownMenu( this, bHasID ? CFmtStr( "%sDropDownMenu", pchPanelID ).String() : NULL );
	m_pMenu->SetChildFocusOnHover( true );

	// create the dropdown menu background
	m_pMenuBackground = new CDropDownMenuBackground( m_pMenu.Get(), bHasID ? CFmtStr( "%sDropDownMenuBg", pchPanelID ).String() : NULL );
	m_pMenu->SetMenuBackground( m_pMenuBackground.Get() );

	// register for events
	if( !UIEngine()->BHaveEventHandlersRegisteredForType( CDropDown::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( Activated(), &CDropDown::EventPanelActivated );
		RegisterEventHandlerOnPanelType( DropDownMenuClosed(), &CDropDown::EventDropDownMenuClosed );
		RegisterEventHandlerOnPanelType( ResetToDefaultValue(), &CDropDown::EventResetToDefault );
		RegisterEventHandlerOnPanelType( DropDownSelectNext(), &CDropDown::EventDropDownSelectNext );
		RegisterEventHandlerOnPanelType( DropDownSelectPrevious(), &CDropDown::EventDropDownSelectPrevious );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDropDown::~CDropDown()
{
	CPanel2D *pMenu = m_pMenu.Get();
	if ( pMenu )
	{
		delete pMenu;
		m_pMenu = NULL;
	}
	CPanel2D *pBackground = m_pMenuBackground.Get();
	if ( pBackground )
	{
		delete pBackground;
		m_pMenuBackground = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Expose methods to JavaScript
//-----------------------------------------------------------------------------
void CDropDown::SetupJavascriptObjectTemplate()
{
	CPanel2D::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "AddOption", PANORAMA_DELEGATE( &CDropDown::AddOption ) );
	RegisterJSMethod( "HasOption", PANORAMA_DELEGATE( &CDropDown::HasOption ) );
	RegisterJSMethod( "RemoveOption", PANORAMA_DELEGATE( &CDropDown::RemoveOption ) );
	RegisterJSMethod( "RemoveAllOptions", PANORAMA_DELEGATE( &CDropDown::RemoveAllOptions ) );
	RegisterJSMethod( "GetSelected", PANORAMA_DELEGATE( &CDropDown::GetSelected ) );
	RegisterJSMethod( "SetSelected", PANORAMA_DELEGATE_EXPLICIT( &CDropDown::SetSelected, void, CDropDown, const char* ) );
	RegisterJSMethod( "SetSelectedIndex", PANORAMA_DELEGATE_EXPLICIT( &CDropDown::SetSelected, void, CDropDown, int ) );
	RegisterJSMethod( "FindDropDownMenuChild", PANORAMA_DELEGATE( &CDropDown::FindDropDownMenuChild ) );
	RegisterJSMethod( "AccessDropDownMenu", PANORAMA_DELEGATE( &CDropDown::AccessDropDownMenu ) );
}


//-----------------------------------------------------------------------------
// Purpose: Called for setting properties from the layout file
//-----------------------------------------------------------------------------
bool CDropDown::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static const CPanoramaSymbol k_pchInitialSelection( "initialselection" );
	if ( symName == k_pchInitialSelection )
	{
		m_strInitialSelection = pchValue;
		return true;
	}

	static const CPanoramaSymbol k_pchMenuClass( "menuclass" );
	if ( symName == k_pchMenuClass )
	{
		m_pMenu->AddClasses( pchValue );
		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}


//-----------------------------------------------------------------------------
// Purpose: Called after our panel has been created (and children added from layout file)
//-----------------------------------------------------------------------------
void CDropDown::OnInitializedFromLayout()
{
	// Move all children added by the layout file to our drop down menu	
	while ( GetChildCount() > 0 )
	{
		CPanel2D *pChild = GetChild( 0 );
		pChild->SetParent( NULL );
		m_pMenu->AddOption( pChild );
	}

	// now that the children are moved, set the selected child in the dropdown menu
	if ( !m_strInitialSelection.IsEmpty() )
	{
		m_pMenu->SetSelected( m_strInitialSelection.String() );
		m_strDefaultSelection = m_strInitialSelection;
		m_strInitialSelection = NULL;
	}

	// update selected to clone
	UpdateSelectedChild( true );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a new option to the dropdown
//-----------------------------------------------------------------------------
void CDropDown::AddOption( CPanel2D *pPanel )
{
	// if we dont have a menu, not loaded yet. Add as a child
	if ( !m_pMenu.Get() )
		pPanel->SetParent( this );
	else
		m_pMenu->AddOption( pPanel );
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if option exists
//-----------------------------------------------------------------------------
bool CDropDown::HasOption( const char *pchID )
{
	return m_pMenu->HasOption( pchID );
}


//-----------------------------------------------------------------------------
// Purpose: Removes panel from menu
//-----------------------------------------------------------------------------
void CDropDown::RemoveOption( const char *pchID )
{
	m_pMenu->RemoveOption( pchID );
	UpdateSelectedChild( false );
}


//-----------------------------------------------------------------------------
// Purpose: Removes all options from this menu
//-----------------------------------------------------------------------------
void CDropDown::RemoveAllOptions()
{
	m_pMenu->RemoveAll();
	UpdateSelectedChild( false );
}


//-----------------------------------------------------------------------------
// Purpose: Finds children matching the class and returns them by id
//-----------------------------------------------------------------------------
void CDropDown::FindOptionIDsByClass( const char *pchClassName, CUtlVector< CUtlString > &vecIDsOut )
{
	for( int i=0; i < m_pMenu->GetChildCount(); ++i )
	{
		CPanel2D *pPanel = m_pMenu->GetChild( i );
		if ( pPanel->BHasClass( pchClassName ) )
			vecIDsOut.AddToTail( pPanel->GetID() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sorts options
//-----------------------------------------------------------------------------
void CDropDown::SortOptions( std::function< int( const IUIPanelClient *, const IUIPanelClient * ) > fnCompare )
{
	if ( m_pMenu.Get() )
		m_pMenu->SortChildren( fnCompare );
}


//-----------------------------------------------------------------------------
// Purpose: Call if you know you've changed the contents of one of the option panels
//-----------------------------------------------------------------------------
void CDropDown::InvalidateOptions( bool bForceReload )
{
	UpdateSelectedChild( true, bForceReload );
}


//-----------------------------------------------------------------------------
// Purpose: Updates the selected child
//-----------------------------------------------------------------------------
void CDropDown::UpdateSelectedChild( bool bSuppressChangedEvent, bool bInvalidateAlways )
{
	Assert( m_pMenu.Get() );
	CPanel2D *pSelected = m_pMenu->GetSelectedChild();

	// skip if dropdown selection has not changed
	if ( !m_pSelected.Get() && !pSelected )
		return;

	if ( m_pSelected.Get() && pSelected && !bInvalidateAlways )
	{
		if ( V_strcmp( m_pSelected->GetID(), pSelected->GetID() ) == 0 )
			return;
	}

	// changed
	if( m_pSelected.Get() )
	{
		delete m_pSelected.Get();
		m_pSelected = NULL;
	}

	if ( pSelected )
	{
		m_pSelected = pSelected->Clone();
		m_pSelected->SetAcceptsFocus( false );
	}

	if ( m_pSelected.Get() )
	{
		m_pSelected->SetParent( this );
	}
	
	if ( !bSuppressChangedEvent )
	{
		DispatchPanelEvent( k_pchOnInputSubmit );
		DispatchEvent( DropDownSelectionChanged(), this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Find a child by id of the dropdown itself
//-----------------------------------------------------------------------------
CPanel2D *CDropDown::FindDropDownMenuChild( const char *pchID )
{
	return m_pMenu->FindChild( pchID );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the selected child. If called during construction, selection will be delayed till panel loaded event is fired
//-----------------------------------------------------------------------------
void CDropDown::SetSelected( const char *pchID, bool bNotify )
{
	m_pMenu->SetSelected( pchID );
	UpdateSelectedChild( !bNotify );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the selected child. If called during construction, selection will be delayed till panel loaded event is fired
//-----------------------------------------------------------------------------
void CDropDown::SetSelected( int nIndex, bool bNotify )
{
	m_pMenu->SetSelected( nIndex );
	UpdateSelectedChild( !bNotify );
}

//-----------------------------------------------------------------------------
// Purpose: Clears the currently selection option
//-----------------------------------------------------------------------------
void CDropDown::ClearSelection()
{
	m_pMenu->ClearSelection();
	UpdateSelectedChild( true );
}

//-----------------------------------------------------------------------------
// Purpose: Resets the selected child to default. If called during construction, selection will be delayed till panel loaded event is fired
//-----------------------------------------------------------------------------
void CDropDown::ResetToDefault( bool bNotify )
{
	Assert( m_strDefaultSelection.IsValid() );
	Assert( !m_strDefaultSelection.IsEmpty() );
	if ( !m_strDefaultSelection.IsEmpty() )
	{
		m_pMenu->SetSelected( m_strDefaultSelection.String() );
		UpdateSelectedChild( !bNotify );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set our value to the default value
//-----------------------------------------------------------------------------
void CDropDown::OnResetToDefaultValue( )
{
	ResetToDefault( true );
}

//-----------------------------------------------------------------------------
// Purpose: Handles the Reset to Default event
//-----------------------------------------------------------------------------
bool CDropDown::EventResetToDefault( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	if ( ToPanel2D( pPanel.Get() ) == this )
		OnResetToDefaultValue();

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDropDown::EventDropDownSelectNext( void )
{
	CPanel2D *pSelectedChild = m_pMenu->GetSelectedChild();

	int nIndex = m_pMenu->GetChildIndex( pSelectedChild );

	if ( nIndex + 1 < m_pMenu->GetChildCount() )
	{
		CPanel2D *pChild = m_pMenu->GetChild( nIndex + 1 );
		m_pMenu->SelectOption( pChild->GetID() );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDropDown::EventDropDownSelectPrevious( void )
{
	CPanel2D *pSelectedChild = m_pMenu->GetSelectedChild();

	int nIndex = m_pMenu->GetChildIndex( pSelectedChild );

	if ( nIndex >= 1 )
	{
		CPanel2D *pChild = m_pMenu->GetChild( nIndex - 1 );
		m_pMenu->SelectOption( pChild->GetID() );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the panel activated event
//-----------------------------------------------------------------------------
bool CDropDown::EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	if ( ToPanel2D(pPanel.Get()) != this )
		return false;

	ShowMenu();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Mouse was clicked on us or a descendant
//-----------------------------------------------------------------------------
bool CDropDown::OnMouseButtonDown( const MouseData_t &mouseData )
{
	// if the menu is visible, close
	if ( BHasClass( k_pchMenuDisplayed ) )
	{
		m_pMenu->Close();
		SetFocus();

		// suppress the next mouse click so we dont get an activate event from mouse
		m_bSuppressClick = true;
		return true;
	}

	m_bSuppressClick = false;
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Mouse was clicked on this panel
//-----------------------------------------------------------------------------
bool CDropDown::OnClick( IUIPanel *pPanel, const MouseData_t &code )
{
	if ( pPanel != UIPanel() && !pPanel->IsDescendantOf( UIPanel() ) )
		return false;

	bool bRet = m_bSuppressClick;
	m_bSuppressClick = false;
	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Shows the drop down context menu
//-----------------------------------------------------------------------------
void CDropDown::ShowMenu()
{
	Assert( m_pMenu.Get() );
	AddClass( k_pchMenuDisplayed );
	m_pMenu->Show();
}


//-----------------------------------------------------------------------------
// Purpose: Handles the attached drop down menu closing
//-----------------------------------------------------------------------------
bool CDropDown::EventDropDownMenuClosed( bool bSelectionChanged, CPanelPtr< CPanel2D > pPanel )
{
	RemoveClass( k_pchMenuDisplayed );
	if ( bSelectionChanged )
		UpdateSelectedChild( false );

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: override to change how this panel arranges its children
//-----------------------------------------------------------------------------
void CDropDown::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );
	
	// our layout changed.. invalidate the menu if up
	if ( m_pMenu.Get() )
		m_pMenu->InvalidatePosition();
}


//-----------------------------------------------------------------------------
// Purpose: Checks if symbol is a panel event
//-----------------------------------------------------------------------------
bool CDropDown::BIsClientPanelEvent( CPanoramaSymbol symProperty )
{
	if ( symProperty == k_pchOnInputSubmit )
		return true;

	if( symProperty == k_pchOnUserInputSubmit )
		return true;

	return BaseClass::BIsClientPanelEvent( symProperty );
}

//-----------------------------------------------------------------------------
// Purpose
//-----------------------------------------------------------------------------
int CDropDown::GetNumOptions()
{
	return m_pMenu->GetChildCount();
}

//-----------------------------------------------------------------------------
// Purpose
//-----------------------------------------------------------------------------
CPanel2D *CDropDown::GetOptionByIndex( int nIndex )
{
	return m_pMenu->GetChild( nIndex );
}


//-----------------------------------------------------------------------------
// Purpose: Called when styles change
//-----------------------------------------------------------------------------
void CDropDown::OnStylesChanged()
{
	BaseClass::OnStylesChanged();

	// if the menu is visible and we became disabled, close the menu
	if ( !IsEnabled() && BHasClass( k_pchMenuDisplayed ) )
	{
		m_pMenu->Close();
	}
}

#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Validation
//-----------------------------------------------------------------------------
void CDropDown::ValidateClientPanel( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();	
	BaseClass::ValidateClientPanel( validator, pchName );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDropDownMenu::CDropDownMenu( CDropDown *pDropDown, const char * pchPanelID ) : CPanel2D( pDropDown->GetParentWindow(), pchPanelID )
{
	Assert( pDropDown );
	m_pDropDown = pDropDown;
	m_pSelectedChild = NULL;
	m_bFirstLayoutAfterShow = false;

	SetLayoutFile( pDropDown->GetLayoutFile() );
	SetLayoutLoadedFromParent( pDropDown );

	RegisterEventHandler( Activated(), this, &CDropDownMenu::EventPanelActivated );
	RegisterEventHandler( Cancelled(), this, &CDropDownMenu::EventCancelled );
	RegisterEventHandler( ResetToDefaultValue(), this, &CDropDownMenu::EventResetToDefault );
	RegisterEventHandler( InputFocusSet(), this, &CDropDownMenu::EventInputFocusSet );
	RegisterEventHandler( InputFocusLost( ), this, &CDropDownMenu::EventInputFocusLost );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDropDownMenu::~CDropDownMenu()
{
	
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDropDownMenu::SetMenuBackground( CDropDownMenuBackground *pBackground )
{
	m_pMenuBackground = pBackground;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the selected child
//-----------------------------------------------------------------------------
CPanel2D *CDropDownMenu::GetSelectedChild()
{
	return m_pSelectedChild;
}


//-----------------------------------------------------------------------------
// Purpose: Shows the drop down menu
//-----------------------------------------------------------------------------
void CDropDownMenu::Show()
{
	// always invalidate layout when becoming visible. Makes sure we are positioned properly
	InvalidateSizeAndPosition();
	m_bFirstLayoutAfterShow = true;

	if ( m_pDropDown )
		SetInputNamespace( m_pDropDown->GetInputNamespace() );

	AddClass( k_pchMenuDisplayed );
	if ( m_pMenuBackground )
	{
		m_pMenuBackground->SetVisible( true );
	}

	if ( m_pSelectedChild && m_pSelectedChild->BIsVisible() )
		m_pSelectedChild->SetFocus();
	else
		SetFocus();
}

//-----------------------------------------------------------------------------
// Purpose: Hides the drop down menu
//-----------------------------------------------------------------------------
void CDropDownMenu::Hide( bool bSelectionChanged )
{
	RemoveClass( k_pchMenuDisplayed );
	if ( m_pMenuBackground )
	{
		m_pMenuBackground->SetVisible( false );
	}

	if ( m_pDropDown->IsEnabled() )
	{
		m_pDropDown->SetFocus();
	}
	
	DispatchEvent( DropDownMenuClosed(), m_pDropDown, bSelectionChanged, m_pDropDown );
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the first selected panel
//-----------------------------------------------------------------------------
void CDropDownMenu::AddOption( CPanel2D *pPanel )
{
	pPanel->SetParent( this );
	pPanel->SetSelectionPosition( k_flSelectionPosAuto, k_flSelectionPosAuto );
	pPanel->SetTabIndex( k_flTabIndexAuto );
	pPanel->AddClass( "DropDownChild" );

	// Labels magically accept focus, not true for other types... hacky.
	if ( pPanel->GetPanelType() == CLabel::GetPanelSymbol() )
	{
		CLabel *pLabel = assert_cast< CLabel * >( pPanel );
		pLabel->SetAcceptsFocus( true );
		pLabel->SetAllowTextSelection( false );
	}

	if ( !m_pSelectedChild )
		m_pSelectedChild = pPanel;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the selected child
//-----------------------------------------------------------------------------
void CDropDownMenu::SetSelected( const char *pchID )
{
	CPanel2D *pPanel = FindChild( pchID );
	if ( pPanel )
		m_pSelectedChild = pPanel;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the selected child
//-----------------------------------------------------------------------------
void CDropDownMenu::SetSelected( int nIndex )
{
	CPanel2D *pPanel = GetChild( nIndex );
	if ( pPanel )
		m_pSelectedChild = pPanel;
}

void CDropDownMenu::ClearSelection()
{
	m_pSelectedChild = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDropDownMenu::SelectOption( const char *pchID )
{
	SetSelected( pchID );
	DispatchEvent( DropDownMenuClosed(), m_pDropDown, true, m_pDropDown );
}

void CDropDownMenu::SelectOption( int nIndex )
{
	SetSelected( nIndex );
	DispatchEvent( DropDownMenuClosed(), m_pDropDown, true, m_pDropDown );
}

//-----------------------------------------------------------------------------
// Purpose: Handles the Reset to Default event
//-----------------------------------------------------------------------------
bool CDropDownMenu::EventResetToDefault( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	if ( ToPanel2D( pPanel.Get() ) == this || pPanel->IsDescendantOf( this->UIPanel() ) )
	{
		OnResetToDefaultValue();
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Set our value to the default value
//-----------------------------------------------------------------------------
void CDropDownMenu::OnResetToDefaultValue( )
{
	m_pDropDown->ResetToDefault( true );
	Show();
}

//-----------------------------------------------------------------------------
// Purpose: Handles the panel activated event
//-----------------------------------------------------------------------------
bool CDropDownMenu::EventPanelActivated( const CPanelPtr< IUIPanel > &ptrPanel, EPanelEventSource_t eSource )
{
	CPanel2D *pSelected = ToPanel2D(ptrPanel.Get());
	if ( pSelected == this )
		return true;

	bool bSelectionChanged = (pSelected != m_pSelectedChild);
	m_pSelectedChild = pSelected;
	Assert( m_pSelectedChild );

	UIEngine()->UISoundSystem()->PlaySound("UIPanorama.submenu_select", nullptr, panorama::k_ESoundType_Effects, 1.0f, 0.5f, 0.0f);


	Hide( bSelectionChanged );

	if ( bSelectionChanged )
	{
		m_pDropDown->DispatchPanelEvent( k_pchOnUserInputSubmit );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the canceled event
//-----------------------------------------------------------------------------
bool CDropDownMenu::EventCancelled( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	Hide( false );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: override to change how this panel arranges its children
//-----------------------------------------------------------------------------
void CDropDownMenu::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{	
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );	

	CContextUI::SLayoutTarget layoutTarget;
	CContextUI::SetupTargetForPanel( layoutTarget, m_pDropDown, GetParentWindow() );

	// Dropdown position and arrow positions are normally hardcoded
	CContextUI::SLayoutPosition layoutPosition;
	layoutPosition.ePositions[ 0 ] = k_EContextUIPositionBottom;
	layoutPosition.ePositions[ 1 ] = k_EContextUIPositionTop;
	layoutPosition.ePositions[ 2 ] = k_EContextUIPositionRight;
	layoutPosition.ePositions[ 3 ] = k_EContextUIPositionLeft;
	layoutPosition.lenHorizontalBodyPosition.SetPercent( 0.0f );
	layoutPosition.lenVerticalBodyPosition.SetPercent( 0.0f );
	layoutPosition.lenHorizontalArrowPosition.SetPercent( 50.0f );
	layoutPosition.lenVerticalArrowPosition.SetPercent( 50.0f );

	// Allow overriding the body position via an XML attribute
	if ( m_pDropDown )
	{
		const char *pszBodyPosition = m_pDropDown->GetAttribute( "dropdownbodyposition", "" );
		if ( pszBodyPosition )
		{
			CSSHelpers::BParseIntoTwoUILengths( &layoutPosition.lenHorizontalBodyPosition, &layoutPosition.lenVerticalBodyPosition, pszBodyPosition, &pszBodyPosition );
		}
	}

	CContextUI::LayoutContextUI( this, layoutTarget, layoutPosition );

	// size just changed; need to make sure focused panel is scrolled into view
	if ( m_bFirstLayoutAfterShow )
	{
		m_bFirstLayoutAfterShow = false;
		if ( m_pSelectedChild )
			m_pSelectedChild->ScrollParentToMakePanelFit( SCROLL_BEHAVIOR_DEFAULT, true );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if option exists
//-----------------------------------------------------------------------------
bool CDropDownMenu::HasOption( const char *pchID )
{
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		CPanel2D *pPanel = GetChild( i );
		if ( V_strcmp( pPanel->GetID(), pchID ) == 0 )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Removes panel from menu
//-----------------------------------------------------------------------------
void CDropDownMenu::RemoveOption( const char *pchID )
{
	CPanel2D *pRemove = NULL;
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		CPanel2D *pPanel = GetChild( i );
		if ( V_strcmp( pPanel->GetID(), pchID ) == 0 )
		{
			pRemove = pPanel;
			break;
		}
	}
	
	if ( !pRemove )
		return;

	// if the child to delete is the selected child, move focus to first existing child
	// CDropDown will take care of dispatching selection changed (only code that can call methods on the dropdown)
	bool bRemovedFocusChild = (pRemove == m_pSelectedChild);
	SAFE_DELETE( pRemove );
	if ( bRemovedFocusChild )
		m_pSelectedChild = GetChildCount() > 0 ? GetChild( 0 ) : NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Removes all panels from the menu
//-----------------------------------------------------------------------------
void CDropDownMenu::RemoveAll()
{
	RemoveAndDeleteChildren();
	m_pSelectedChild = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Fires whenever a member in a dropdown menu is selected
//-----------------------------------------------------------------------------
bool CDropDownMenu::EventInputFocusSet( const CPanelPtr< IUIPanel > &ptrPanel )
{
	DispatchEvent( DropdownMenuFocusChanged(), m_pDropDown, ptrPanel );
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Fires whenever a member in a dropdown menu is selected
//-----------------------------------------------------------------------------
bool CDropDownMenu::EventInputFocusLost( const CPanelPtr< IUIPanel > &ptrPanel )
{
	IUIPanel *pPanel = ptrPanel.Get( );
	if( !pPanel )
		return false;

	// If we clicked anywhere other than the tab options panel or the current tab, close the dropdownmenu
	if( !BHasKeyFocus( ) && !BHasDescendantKeyFocus( ) )
	{
		Close();
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Validation
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CDropDownMenu::ValidateClientPanel( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();	
	BaseClass::ValidateClientPanel( validator, pchName );
}
#endif



//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDropDownMenuBackground::CDropDownMenuBackground( CDropDownMenu *pDropDownMenu, const char * pchPanelID ) : CPanel2D( pDropDownMenu->GetParentWindow(), pchPanelID )
{
	Assert( pDropDownMenu );
	m_pMenu = pDropDownMenu;

	SetLayoutFile( pDropDownMenu->GetLayoutFile() );
	SetLayoutLoadedFromParent( pDropDownMenu );

	pDropDownMenu->SetParent( this );

	SetVisible( false );
	SetAcceptsInput( true );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDropDownMenuBackground::~CDropDownMenuBackground()
{

}


//-----------------------------------------------------------------------------
// Purpose: Dismiss the dropdownmenu when clicked outside of it
//-----------------------------------------------------------------------------
bool CDropDownMenuBackground::OnClick( IUIPanel *pPanel, const panorama::MouseData_t &code )
{
	if ( pPanel->ClientPtr() == this )
	{
		if ( m_pMenu )
		{
			m_pMenu->Close();
		}
		return true;
	}
	return BaseClass::OnClick( pPanel, code );
}