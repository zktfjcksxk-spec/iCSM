//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_popup_manager.h"
#include "panorama/ui_popup.h"
#include "popups/ui_popup_generic.h"
#include "popups/ui_popup_custom_layout.h"
#include "panorama/ui_root.h"
#include "panorama/controls/contextmenu.h"
#include "ui_symbols.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CUI_PopupManager, PopupManager )

DECLARE_PANORAMA_EVENT0( PopupBackgroundClicked );
DECLARE_PANORAMA_EVENT1( PopupShowAsync, const char * );

DEFINE_PANORAMA_EVENT( PopupBackgroundClicked );
DEFINE_PANORAMA_EVENT( SetPopupBackgroundBlur );
DEFINE_PANORAMA_EVENT( PopupShowAsync );
DEFINE_PANORAMA_EVENT( UIShowCustomLayoutPopup );
DEFINE_PANORAMA_EVENT( UIShowCustomLayoutPopupParameters );
DEFINE_PANORAMA_EVENT( UIPopupManagerVisibilityChanged );

using namespace panorama;

CUI_PopupManager::CUI_PopupManager( CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
	, m_bHandlingChildrenChanged( false )
	, m_pDimBackgroundPanel( nullptr )
	, m_pBlurBackgroundButton( nullptr )
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/popups/popup_manager.xml" ) );
	SetInputNamespace( "popup" );

	m_pDimBackgroundPanel = FindChildInLayoutFile( "DimBackground" );
	m_pBlurBackgroundButton = panel_cast< CButton * >( FindChildInLayoutFile( "BlurBackground" ) );

	m_pDimBackgroundPanel->AddClass( k_symHidden );
	m_pBlurBackgroundButton->AddClass( k_symHidden );

	RegisterEventHandler( PopupBackgroundClicked(), this, &CUI_PopupManager::EventBackgroundClicked );
	RegisterEventHandler( PopupShowAsync(), this, &CUI_PopupManager::EventShowPopupAsync );

	RegisterForUnhandledEvent( UIShowCustomLayoutPopup(), this, &CUI_PopupManager::EventShowCustomLayoutPopup );
	RegisterForUnhandledEvent( UIShowCustomLayoutPopupParameters(), this, &CUI_PopupManager::EventShowCustomLayoutPopupParameters );
}

CUI_PopupManager::~CUI_PopupManager()
{
}

/*static*/ CUI_PopupManager *CUI_PopupManager::GetWindowPopupManager( IUIWindow *pWindow )
{
	CUI_Root *pRoot = CUI_Root::GetRootForWindow( pWindow );
	if ( !pRoot )
		return nullptr;

	return pRoot->GetPopupManager();
}

/*static*/ CUI_PopupManager *CUI_PopupManager::GetPanelPopupManager( panorama::CPanel2D *pPanel )
{
	if ( !pPanel )
		return nullptr;

	return GetWindowPopupManager( pPanel->GetParentWindow() );
}


bool CUI_PopupManager::IsPopupVisible( CUI_Popup *pPopup )
{
	return pPopup->GetParent() == this && !pPopup->BHasClass( k_symHidden );
}

bool CUI_PopupManager::IsAnyPopupVisible()
{
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		if( !GetChild( i )->BHasClass( k_symHidden ) )
			return true;
	}
	return false;
}

void CUI_PopupManager::CloseIfVisible( CUI_Popup *pPopup, bool bImmediate /* = false */ )
{
    if ( IsPopupVisible( pPopup ) )
    {
        if ( bImmediate )
        {
            // We don't want to directly delete so that the manager can run its
            // normal close logic.
            pPopup->CloseAndDeleteAsync( 0.0f );
        }
        else
        {
            pPopup->HandlePopupButtonClicked( (char*)nullptr );
        }
    }
}

void CUI_PopupManager::CloseAllVisiblePopups()
{
	for ( int i = GetChildCount() - 1; i >= 0; --i )
	{
		CUI_Popup *pPopup = dynamic_cast<CUI_Popup *>( GetChild( i ) );
		if ( !pPopup )
			continue;

		CloseIfVisible( pPopup, true );
	}
}

// Utility helper
CUI_Popup_Generic *CUI_PopupManager::CreateGeneric( const char *pszTitle, const char *pszMessage, const char *pszOption1 /* = nullptr */, const char *pszEvent1 /* = nullptr */ )
{
	CUI_Popup_Generic *pPopup = new CUI_Popup_Generic( this, nullptr, nullptr );
    if ( pszOption1 )
    {
        pPopup->SetDisplayOneOption( pszTitle, pszMessage, pszOption1, pszEvent1 );
    }
    else
    {
        pPopup->SetDisplayOk( pszTitle, pszMessage, nullptr );
    }
    return pPopup;
}

// Utility helper
void CUI_PopupManager::ShowGeneric( const char *pszTitle, const char *pszMessage )
{
	ShowPopup( CreateGeneric( pszTitle, pszMessage ) );
}

void CUI_PopupManager::ShowPopup( CUI_Popup *pPopup )
{
	bool bAnyPopupWasVisible = IsAnyPopupVisible();

	pPopup->SetManager( this );
	pPopup->SetParent( this );
	CPanel2D *pLastChild = GetChild( GetChildCount() - 1 );
	MoveChildAfter( pPopup, pLastChild );

	pPopup->RemoveClass( k_symHidden );
	pPopup->SetFocus();
	pPopup->OnShow();

	UpdatePopupBackgrounds();

	DispatchEvent( DismissAllContextMenus(), nullptr );

	if ( !bAnyPopupWasVisible )
	{
		DispatchEvent( UIPopupManagerVisibilityChanged(), this, true );
	}
}

void CUI_PopupManager::ShowPopupAsync( CUI_Popup *pPopup, float flDelay )
{
	Assert( pPopup->GetParent() == this );
	Assert( pPopup->GetID() && pPopup->GetID()[ 0 ] != '\0' );

	DispatchEventAsync( flDelay, PopupShowAsync(), this, pPopup->GetID() );
}

panorama::IUIEvent* CUI_PopupManager::YldShowPopup( CUI_Popup *pPopup )
{
#ifdef NO_STEAM
	// Offline UI uses ShowPopup/ShowPopupAsync; no coordinator job can yield here.
	Warning( "Coordinator job popup requested in an offline build\n" );
	return nullptr;
#else
	pPopup->SetIsCreatedByJob( true );
	ShowPopup( pPopup );

	panorama::CPanelPtr< CUI_Popup > hPopup = pPopup;
	while ( true )
	{
		// Have to continually re-acquire the popup since it may have been deleted
		CUI_Popup *pCurPopup = hPopup.Get();
		if ( !pCurPopup )
			return nullptr;

		if ( pCurPopup->WasPopupButtonClicked() )
		{
			panorama::IUIEvent* pResult = pCurPopup->GetResult();
			pCurPopup->CleanupPopupInJob();
			return pResult;
		}

		GCSDK::GJobCur().BYieldingWaitOneFrame();
	}
	return nullptr;
#endif
}

GenericPopupResult_t CUI_PopupManager::YldShowGeneric( const char *pszTitle, const char *pszMessage )
{
	CUI_Popup_Generic *pGeneric = CreateGeneric( pszTitle, pszMessage );
	return pGeneric->YldShowGenericPopup();
}

#if defined( CSTRIKE15 )
bool CUI_PopupManager::BYldPromptOKCancel( const char* pszHeader, const char* pszBody, style_index_t nStyleIndex )
#else
bool CUI_PopupManager::BYldPromptOKCancel( const char* pszHeader, const char* pszBody, const EconItemIDs_t *pIcon, style_index_t nStyleIndex )
#endif
{
	CUI_Popup_Generic *pPopup = new CUI_Popup_Generic( this, "ConfirmUseTool", nullptr );
	pPopup->SetDisplayOkCancel( pszHeader, pszBody, nullptr, nullptr );
#if !defined( CSTRIKE15 )
	if ( pIcon )
	{
		pPopup->SetEconItemIconVisible( *pIcon, nStyleIndex );
	}
#endif
	GenericPopupResult_t result = pPopup->YldShowGenericPopup();
	return ( result == GENERIC_POPUP_RESULT_BUTTON_0 );
}

#if defined( CSTRIKE15 )
void CUI_PopupManager::YldPromptOK( const char* pszHeader, const char* pszBody, style_index_t nStyleIndex )
#else
void CUI_PopupManager::YldPromptOK( const char* pszHeader, const char* pszBody, const EconItemIDs_t *pIcon, style_index_t nStyleIndex )
#endif
{
	CUI_Popup_Generic *pPopup = new CUI_Popup_Generic( this, "ConfirmUseTool", nullptr );
	pPopup->SetDisplayOk( pszHeader, pszBody, nullptr );
#if !defined( CSTRIKE15 )
	if ( pIcon )
	{
		pPopup->SetEconItemIconVisible( *pIcon, nStyleIndex );
	}
#endif
	pPopup->YldShowGenericPopup();
}

#if defined( CSTRIKE15 )
bool CUI_PopupManager::BYldPromptDangerousOKCancel( const char* pszHeader, const char* pszBody, const char *pConfirmationCode, style_index_t nStyleIndex )
#else
bool CUI_PopupManager::BYldPromptDangerousOKCancel( const char* pszHeader, const char* pszBody, const char *pConfirmationCode, const EconItemIDs_t *pIcon, style_index_t nStyleIndex )
#endif
{
	CUI_Popup_Generic *pPopup = new CUI_Popup_Generic( this, "ConfirmUseDangerousTool", nullptr );
	pPopup->SetDisplayOkCancel( pszHeader, pszBody, nullptr, nullptr );
	pPopup->SetConfirmationCodeVisible( pConfirmationCode );
#if !defined( CSTRIKE15 )
	if ( pIcon )
	{
		pPopup->SetEconItemIconVisible( *pIcon, nStyleIndex );
	}
#endif
	GenericPopupResult_t result = pPopup->YldShowGenericPopup();
	return ( result == GENERIC_POPUP_RESULT_BUTTON_0 );
}

bool CUI_PopupManager::BYldPromptOKCancelWithHeroIcon( const char* pszHeader, const char* pszBody, int nHeroID )
{
	CUI_Popup_Generic *pPopup = new CUI_Popup_Generic( this, "ConfirmUseTool", nullptr );
	pPopup->SetDisplayOkCancel( pszHeader, pszBody, nullptr, nullptr );
	pPopup->SetHeroIconVisible( nHeroID );
	GenericPopupResult_t result = pPopup->YldShowGenericPopup();
	return ( result == GENERIC_POPUP_RESULT_BUTTON_0 );
}

void CUI_PopupManager::YldPromptOKWithHeroIcon( const char* pszHeader, const char* pszBody, int nHeroID )
{
	CUI_Popup_Generic *pPopup = new CUI_Popup_Generic( this, "ConfirmUseTool", nullptr );
	pPopup->SetDisplayOk( pszHeader, pszBody, nullptr );
	pPopup->SetHeroIconVisible( nHeroID );
	pPopup->YldShowGenericPopup();
}

void CUI_PopupManager::CloseAndDeletePopup( CUI_Popup *pPopup, float flDelay )
{
	pPopup->AddClass( k_symHidden );
	pPopup->SetEnabled( false );
	pPopup->OnClose();
	pPopup->SetManager( NULL );

	UpdatePopupBackgrounds();

	// If this popup had focus, throw it somewhere useful
	bool bChangeFocus = pPopup->BHasKeyFocus() || pPopup->BHasDescendantKeyFocus();
	bool bFoundOtherPopup = false;

	for ( int i = GetChildCount() - 1; i >= 0; --i )
	{
		CUI_Popup *pOtherPopup = dynamic_cast< CUI_Popup * >( GetChild( i ) );
		if ( !pOtherPopup )
			continue;

		if ( pOtherPopup->BHasClass( k_symHidden ) )
			continue;

		if ( bChangeFocus )
			pOtherPopup->SetFocus();

		bFoundOtherPopup = true;
		break;
	}

	if ( !bFoundOtherPopup )
	{
		DispatchPanelEvent( "onpopupsdismissed" );
		DispatchEvent( UIPopupManagerVisibilityChanged(), this, false );
	}

	pPopup->DeleteAsync( flDelay );
}

void CUI_PopupManager::UpdatePopupBackgrounds()
{
	// Find the highest level popup that wants each style of background, and position the background before that
	bool bFoundDim = false;
	bool bFoundBlur = false;

	for ( int i = GetChildCount() - 1; i >= 0; --i )
	{
		CUI_Popup *pPopup = dynamic_cast< CUI_Popup * >( GetChild( i ) );
		if ( !pPopup )
			continue;

		if ( pPopup->BHasClass( k_symHidden ) )
			continue;

		EPopupBackgroundStyle eBackgroundStyle = pPopup->GetPopupBackgroundStyle();
		if ( !bFoundDim && ( eBackgroundStyle == k_EPopupBackgroundStyle_Dim || eBackgroundStyle == k_EPopupBackgroundStyle_Dim_Dismiss ) )
		{
			MoveChildBefore( m_pDimBackgroundPanel, pPopup );
			m_pDimBackgroundPanel->RemoveClass( k_symHidden );
			m_pDimBackgroundPanel->SetHitTestEnabled( true );
			bFoundDim = true;
		}
		else if ( !bFoundBlur && ( eBackgroundStyle == k_EPopupBackgroundStyle_Blur || eBackgroundStyle == k_EPopupBackgroundStyle_Blur_Dismiss ) )
		{
			MoveChildBefore( m_pBlurBackgroundButton, pPopup );
			m_pBlurBackgroundButton->RemoveClass( k_symHidden );
			m_pBlurBackgroundButton->SetHitTestEnabled( true );
			m_pBlurBackgroundButton->SetEnabled( true );
			DispatchEvent( SetPopupBackgroundBlur(), this, true );
			bFoundBlur = true;
		}

		if ( bFoundDim && bFoundBlur )
			break;
	}

	if ( !bFoundDim )
	{
		m_pDimBackgroundPanel->AddClass( k_symHidden );
		m_pDimBackgroundPanel->SetHitTestEnabled( false );
	}

	if ( !bFoundBlur )
	{
		m_pBlurBackgroundButton->AddClass( k_symHidden );
		m_pBlurBackgroundButton->SetHitTestEnabled( false );
		m_pBlurBackgroundButton->SetEnabled( false );
		DispatchEvent( SetPopupBackgroundBlur(), this, false );
	}
}

void CUI_PopupManager::OnAfterChildrenChanged()
{
	BaseClass::OnAfterChildrenChanged();

	// This can be called really early during panel construction
	if ( !m_pBlurBackgroundButton || !m_pDimBackgroundPanel )
		return;

	if ( m_bHandlingChildrenChanged )
		return;

	// If a child is deleted out from underneath the popup manager,
	// make sure we update the background accordingly.
	m_bHandlingChildrenChanged = true;
	UpdatePopupBackgrounds();
	m_bHandlingChildrenChanged = false;
}

bool CUI_PopupManager::EventBackgroundClicked()
{
	static const CPanoramaSymbol k_symPropertyOnCancel( "oncancel" );

	// Find the popup responsible for the current blur and trigger its oncancel
	for ( int i = GetChildCount() - 1; i >= 0; --i )
	{
		CUI_Popup *pPopup = dynamic_cast< CUI_Popup * >( GetChild( i ) );
		if ( !pPopup )
			continue;

		if ( pPopup->BHasClass( k_symHidden ) )
			continue;

		EPopupBackgroundStyle eBackgroundStyle = pPopup->GetPopupBackgroundStyle();
		if ( eBackgroundStyle == k_EPopupBackgroundStyle_None )
			continue;

		if ( eBackgroundStyle == k_EPopupBackgroundStyle_Blur_Dismiss || eBackgroundStyle == k_EPopupBackgroundStyle_Dim_Dismiss )
		{
			// If they have a custom oncancel event, call that. Otherwise just dismiss the popup
			if ( pPopup->BIsPanelEventSet( k_symPropertyOnCancel ) )
			{
				pPopup->DispatchPanelEvent( k_symPropertyOnCancel );
			}
			else
			{
				pPopup->HandlePopupButtonClicked( (IUIEvent*)NULL );
			}
		}
		break;
	}

	return true;
}

bool CUI_PopupManager::EventShowPopupAsync( const char *pszPopupID )
{
	CUI_Popup *pPopup = assert_cast< CUI_Popup * >( FindChild( pszPopupID ) );
	if ( !pPopup )
	{
		Assert( false );
		return true;
	}

	ShowPopup( pPopup );
	return true;
}

void CUI_PopupManager::ForceClosePopups( void )
{
	for ( int i = GetChildCount() - 1; i >= 0; --i )
	{
		CUI_Popup *pPopup = dynamic_cast< CUI_Popup * >( GetChild( i ) );
		if ( !pPopup || !pPopup->IsPopupVisible() )
			continue;

		delete pPopup;
	}
}

void CUI_PopupManager::OnPopupLayoutReloaded( CUI_Popup *pPopup )
{
	// When a popup's layout is reloaded, make sure it stays visible
	pPopup->RemoveClass( "Hidden" );
}

bool CUI_PopupManager::BIsClientPanelEvent( CPanoramaSymbol symProperty )
{
	static const CPanoramaSymbol k_symOnPopupsDismissed( "onpopupsdismissed" );
	if ( symProperty == k_symOnPopupsDismissed )
		return true;

	return BaseClass::BIsClientPanelEvent( symProperty );
}

bool CUI_PopupManager::EventShowCustomLayoutPopup( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr, const char *pszPopupID, const char *pszLayoutFile )
{
	return EventShowCustomLayoutPopupParameters( panelPtr, pszPopupID, pszLayoutFile, nullptr );
}

bool CUI_PopupManager::EventShowCustomLayoutPopupParameters( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr, const char *pszPopupID, const char *pszLayoutFile, const char *pszParameters )
{
	CPanel2D *pEventParent = ToPanel2D( panelPtr.Get() );
	CUI_PopupManager *pPopupManager = GetPanelPopupManager( pEventParent );
	if ( !pPopupManager || pPopupManager != this )
		return false;

	CUI_Popup_CustomLayout *pPopup = new CUI_Popup_CustomLayout( this, pszPopupID, pEventParent );
	pPopup->Init( pszLayoutFile, pszParameters );
	pPopup->ShowPopup();
	return true;
}

// Localization parent
IUIPanel *CUI_PopupManager::GetLocalizationParent() const
{
	CPanel2D *pPanel = m_hLocalizationParent.Get();
	if ( pPanel )
		return pPanel->UIPanel();

	return BaseClass::GetLocalizationParent();
}

void CUI_PopupManager::SetLocalizationParent( panorama::CPanel2D *pPanel )
{
	if ( pPanel != GetParent() )
	{
		m_hLocalizationParent = pPanel;
	}
	else
	{
		m_hLocalizationParent = nullptr;
	}
}


//-----------------------------------------------------------------------------
// Helper used to set dialog variables on all popups created within the scope of CPopupDialogVariableScope
//-----------------------------------------------------------------------------
CPopupDialogVariableScope::CPopupDialogVariableScope( CUI_PopupManager *pManager )
{
	m_hPopupManager = pManager;
	if ( !pManager )
		return;

	m_hPrevLocalizationParent = pManager->GetLocalizationParent();
	CPanel2D *pTempPanel = new CPanel2D( m_hPrevLocalizationParent.Get(), "" );
	pManager->SetLocalizationParent( pTempPanel );
	m_hTempPanel = pTempPanel;
}

void CPopupDialogVariableScope::SetDialogVariable( const char *pchKey, const char *pchValue )
{
	CPanel2D *pLocalizationParent = m_hTempPanel.Get();
	if ( pLocalizationParent )
	{
		pLocalizationParent->SetDialogVariable( pchKey, pchValue );
	}
}

void CPopupDialogVariableScope::SetDialogVariable( const char *pchKey, int iVal )
{
	CPanel2D *pLocalizationParent = m_hTempPanel.Get();
	if ( pLocalizationParent )
	{
		pLocalizationParent->SetDialogVariable( pchKey, iVal );
	}
}

void CPopupDialogVariableScope::SetDialogVariable( const char *pchKey, uint64 uVal )
{
	CPanel2D *pLocalizationParent = m_hTempPanel.Get();
	if ( pLocalizationParent )
	{
		pLocalizationParent->SetDialogVariable( pchKey, uVal );
	}
}

void CPopupDialogVariableScope::SetDialogVariable( const char *pchKey, time_t timeVal )
{
	CPanel2D *pLocalizationParent = m_hTempPanel.Get();
	if ( pLocalizationParent )
	{
		pLocalizationParent->SetDialogVariable( pchKey, timeVal );
	}
}

void CPopupDialogVariableScope::SetDialogVariable( const char *pchKey, CCurrencyAmount amount )
{
	CPanel2D *pLocalizationParent = m_hTempPanel.Get();
	if ( pLocalizationParent )
	{
		pLocalizationParent->SetDialogVariable( pchKey, amount );
	}
}

void CPopupDialogVariableScope::SetDialogVariable( const char *pchKey, const CUtlString &value )
{
	CPanel2D *pLocalizationParent = m_hTempPanel.Get();
	if ( pLocalizationParent )
	{
		pLocalizationParent->SetDialogVariable( pchKey, value );
	}
}

void CPopupDialogVariableScope::SetDialogVariableLocString( const char *pchKey, const char *pchValue )
{
	CPanel2D *pLocalizationParent = m_hTempPanel.Get();
	if ( pLocalizationParent )
	{
		pLocalizationParent->SetDialogVariableLocString( pchKey, pchValue );
	}
}


CPopupDialogVariableScope::~CPopupDialogVariableScope()
{
	CUI_PopupManager *pManager = m_hPopupManager.Get();
	if ( pManager )
	{
		pManager->SetLocalizationParent( m_hPrevLocalizationParent.Get() );
		m_hPrevLocalizationParent = nullptr;
		m_hPopupManager = nullptr;
	}

	CPanel2D *pTempPanel = m_hTempPanel.Get();
	if ( pTempPanel )
	{
		delete pTempPanel;
		m_hTempPanel = nullptr;
	}
}


CUI_PopupManager *GetTopmostPopupManager()
{
	int nRootCount = CUI_Root::GetRootCount();
	for ( int i = nRootCount - 1; i >= 0; --i )
	{
		CUI_Root *pRoot = CUI_Root::GetRootByIndex( i );
		if ( !pRoot->GetParentWindow()->BIsVisible() )
			continue;

		if ( !pRoot->GetPopupManager() )
			continue;

		return pRoot->GetPopupManager();
	}

	return nullptr;
}

