//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_page_manager.h"
#include "ui_page.h"
#include "panorama/uievents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CUI_PageManager, PageManager )

using namespace panorama;

DEFINE_PANORAMA_EVENT( PageManagerActivatingPage );
DEFINE_PANORAMA_EVENT( PageManagerActivatedPage );
DEFINE_PANORAMA_EVENT( PageManagerSelectedPrimaryTabChanged );
DEFINE_PANORAMA_EVENT( PageManagerHistoryChanged );
DEFINE_PANORAMA_EVENT( PageManagerDeletingHistoryEvent );


/*static*/ CUI_PageManager *CUI_PageManager::s_pPageManager = NULL;
/*static*/ CUtlVector< RegisterPageCallbackFunction > *CUI_PageManager::s_pvecRegisterPageCallbacks = NULL;

CUI_PageManager::CUI_PageManager( CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
	, m_pActivePage( NULL )
	, m_nNavigateHistoryIndex( -1 )
	, m_bNavigatingHistory( false )
	, m_nModalPageDepth( 0 )
	, m_unNextPageNavigateID( 1 )
{
	Assert( s_pPageManager == NULL );
	s_pPageManager = this;

	m_mapDefaultPrimaryTabPageEvent.SetLessFunc( UtlStringLessFunc );
	m_mapLastPrimaryTabPageEvent.SetLessFunc( UtlStringLessFunc );

	RegisterForUnhandledEvent( TopLevelWindowVisibilityChanged(), this, &CUI_PageManager::EventWindowVisiblilityChanged );

	// Register handlers for each dashboard page
	if ( s_pvecRegisterPageCallbacks )
	{
		for ( RegisterPageCallbackFunction pFunction : *s_pvecRegisterPageCallbacks )
		{
			pFunction( this );
		}

		// The main dashboard is a singleton, so we can free up the memory from the register functions now.
		delete s_pvecRegisterPageCallbacks;
		s_pvecRegisterPageCallbacks = NULL;
	}
	else
	{
		AssertMsg( false, "page system has no page callback functions? Did you accidentally create two of them?" );
	}
}

CUI_PageManager::~CUI_PageManager()
{
	m_mapLastPrimaryTabPageEvent.PurgeAndDeleteElements();
	m_mapDefaultPrimaryTabPageEvent.PurgeAndDeleteElements();

	Assert( s_pPageManager == this );
	s_pPageManager = NULL;
}

void CUI_PageManager::ClearModalPage( bool bClearAll )
{
	while ( m_nModalPageDepth > 0 )
	{
		if ( !m_vecNavigateHistory.IsEmpty() )
		{
			// Remove/delete the current event we're on
			Assert( m_nNavigateHistoryIndex == ( m_vecNavigateHistory.Count() - 1 ) );
			m_vecNavigateHistory.Remove( m_nNavigateHistoryIndex );
			--m_nNavigateHistoryIndex;
		}
		--m_nModalPageDepth;
		if ( !bClearAll )
			break;
	}
}

void CUI_PageManager::SwitchToPage( CUI_Page *pPage, IUIEvent *pEvent, EPageSetupResult eSetupResult )
{
	if ( !pPage )
	{
		delete pEvent;
		return;
	}

	CUI_Page *pOldPage = m_pActivePage;

	DispatchEvent( PageManagerActivatingPage(), this, pOldPage, pPage );

	Assert( !IsOnModalPage() || ( pOldPage != pPage ) || ( eSetupResult != PAGE_SETUP_SUCCESS_REPLACE ) );

	// Don't clear in the reload case. In the back case, EventNavigateBack has already cleared it.
	if ( !m_bNavigatingHistory )
	{
		if ( eSetupResult != PAGE_SETUP_SUCCESS_MODAL )
		{
			ClearModalPage( true );
		}
		else
		{
			m_nModalPageDepth++;
		}
	}

	if ( pOldPage != pPage )
	{
		if ( pOldPage )
		{
			pOldPage->RemoveClass( "PageVisible" );
		}

		// Force a reload of any resources since we're about to display the page
		pPage->SetVisible( true );
		pPage->SetReadyForDisplay( true );

		if ( pOldPage )
		{
			pOldPage->OnNavigatingAway( NAV_CHANGE_SWITCHED_PAGE );
		}
		pPage->OnNavigatingTo( NAV_CHANGE_SWITCHED_PAGE );

		pPage->AddClass( "PageVisible" );
		pPage->SetFocus();

		m_pActivePage = pPage;

		RecordPageChange( pPage, pEvent, eSetupResult != PAGE_SETUP_SUCCESS_NO_HISTORY );

		if ( pOldPage )
		{
			pOldPage->OnNavigatedAway( NAV_CHANGE_SWITCHED_PAGE );
		}
		pPage->OnNavigatedTo( NAV_CHANGE_SWITCHED_PAGE );

		// Unload resources for any page that is not currently visible
		// or might be transitioning from being visible.
		for ( CPanel2D *pPagePanel : Children() )
		{
			CUI_Page *pExistingPage = panel_cast< CUI_Page * >( pPagePanel, true );
			if ( pExistingPage == m_pActivePage || pExistingPage == pOldPage )
				continue;

			pExistingPage->SetVisible( false );
			pExistingPage->SetReadyForDisplay( false );
		}
	}
	else if ( eSetupResult == PAGE_SETUP_SUCCESS_REPLACE )
	{
		pPage->SetFocus();

		// Clear the stack ahead of where we are currently
		for ( int i = m_vecNavigateHistory.Count() - 1; i > m_nNavigateHistoryIndex; --i )
		{
			m_vecNavigateHistory.Remove( i );
		}

		// We should never be empty here, because we're replacing
		Assert( !m_vecNavigateHistory.IsEmpty() );
		Assert( m_nNavigateHistoryIndex == ( m_vecNavigateHistory.Count() - 1 ) );
		if ( !m_vecNavigateHistory.IsEmpty() )
		{
			// Remove/delete the current event we're on
			m_vecNavigateHistory.Remove( m_nNavigateHistoryIndex );
		}

		// Add the new event to our history stack
		m_vecNavigateHistory.AddToTail( SNavigateEvent( pEvent, m_strSelectedPrimaryTab ) );
		DispatchEvent( PageManagerHistoryChanged(), this );
	}
	else if ( eSetupResult == PAGE_SETUP_SUCCESS || eSetupResult == PAGE_SETUP_SUCCESS_NO_HISTORY || eSetupResult == PAGE_SETUP_SUCCESS_MODAL )
	{
		// Same page, but displaying different info (e.g. the hero page switching which hero it's showing).
		pPage->SetFocus();
		RecordPageChange( pPage, pEvent, ( eSetupResult == PAGE_SETUP_SUCCESS ) || ( eSetupResult == PAGE_SETUP_SUCCESS_MODAL ) );
	}
	else
	{
		Assert( eSetupResult == PAGE_SETUP_NO_CHANGE );
		// No change - just delete the event
		delete pEvent;
	}

	DispatchEvent( PageManagerActivatedPage(), this, pOldPage, pPage );

	// Fire off a panel event when the page changes so it can listen and do
	// things in response (e.g. Javascript)
	if ( eSetupResult == PAGE_SETUP_SUCCESS || eSetupResult == PAGE_SETUP_SUCCESS_REPLACE || eSetupResult == PAGE_SETUP_SUCCESS_NO_HISTORY )
	{
		pPage->DispatchPanelEvent( "onpagesetupsuccess" );
	}
}

CPanoramaSymbol CUI_PageManager::GetActivePagePanelType()
{
	if ( m_pActivePage )
	{
		return m_pActivePage->GetPanelType();
	}

	static CPanoramaSymbol emptyPanelType( "" );
	return emptyPanelType;
}

void CUI_PageManager::RecordPageChange( CUI_Page *pPage, IUIEvent *pEvent, bool bRecordHistory )
{
	pPage->SetPageNavigateID( m_unNextPageNavigateID++ );

	UpdatePrimaryTabs( pPage, pEvent );

	if ( bRecordHistory )
	{
		PushNavigateHistoryEvent( pEvent ); // will delete or take ownership of pEvent
	}
	else
	{
		delete pEvent;
	}
}

void CUI_PageManager::UpdatePrimaryTabs( CUI_Page *pPage, panorama::IUIEvent *pEvent )
{
	const char *pszSelectedTab = pPage ? pPage->GetPrimaryTab() : nullptr;

	// If the page is not associated with any tab (e.g. Web Browser), either use the
	// expected primary tab or the current primary tab instead.
	if ( !pszSelectedTab && !m_strExpectedPrimaryTab.IsEmpty() )
	{
		pszSelectedTab = m_strExpectedPrimaryTab.Get();
	}
	else if ( !pszSelectedTab && !m_strSelectedPrimaryTab.IsEmpty() )
	{
		pszSelectedTab = m_strSelectedPrimaryTab.Get();
	}

	// Record that this is the last page event for that tab, but only if we're not modal
	if ( pEvent && pszSelectedTab && !IsOnModalPage() )
	{
		int i = m_mapLastPrimaryTabPageEvent.Find( pszSelectedTab );
		if ( i != m_mapLastPrimaryTabPageEvent.InvalidIndex() )
		{
			delete m_mapLastPrimaryTabPageEvent.Element( i );
		}

		m_mapLastPrimaryTabPageEvent.InsertOrReplace( pszSelectedTab, pEvent->Copy() );
	}

	// Update the selected tab
	bool bPrimaryTabChanged = m_strSelectedPrimaryTab != pszSelectedTab;
	if ( bPrimaryTabChanged )
	{
		m_strSelectedPrimaryTab = pszSelectedTab;
		DispatchEvent( PageManagerSelectedPrimaryTabChanged(), this, m_strSelectedPrimaryTab.Get() );
	}
}

void CUI_PageManager::SetDefaultPrimaryTabEvent( const char *pszPrimaryTabID, panorama::IUIEvent *pEvent )
{
	m_mapDefaultPrimaryTabPageEvent.InsertOrReplace( pszPrimaryTabID, pEvent );
}

void CUI_PageManager::ShowPrimaryTabPage( const char *pszPrimaryTabID )
{
	IUIEvent *pEvent = nullptr;

	// If they're already on the primary tab and are trying to click it again,
	// just fall through to the default page for that tab
	if ( m_strSelectedPrimaryTab != pszPrimaryTabID )
	{
		int iMap = m_mapLastPrimaryTabPageEvent.Find( pszPrimaryTabID );
		if ( iMap != m_mapLastPrimaryTabPageEvent.InvalidIndex() )
		{
			pEvent = m_mapLastPrimaryTabPageEvent.Element( iMap )->Copy();
		}
	}

	if ( !pEvent )
	{
		int iMap = m_mapDefaultPrimaryTabPageEvent.Find( pszPrimaryTabID );
		if ( iMap == m_mapDefaultPrimaryTabPageEvent.InvalidIndex() )
		{
			AssertMsg( false, "Unable to show primary tab page for %s", pszPrimaryTabID );
			return;
		}

		pEvent = m_mapDefaultPrimaryTabPageEvent.Element( iMap )->Copy();
	}

	// Make sure that the page believes it's the correct primary tab.
	m_strExpectedPrimaryTab = pszPrimaryTabID;
	UIEngine()->DispatchEvent( pEvent );
	m_strExpectedPrimaryTab = nullptr;
}

bool CUI_PageManager::BCanNavigateBack()
{
	if ( m_pActivePage && m_pActivePage->BOverrideNavigateBack() )
		return m_pActivePage->BCanNavigateBack();

	return m_nNavigateHistoryIndex > 0;
}

bool CUI_PageManager::BCanNavigateForward()
{
	if ( m_pActivePage && m_pActivePage->BOverrideNavigateForward() )
		return m_pActivePage->BCanNavigateForward();

	return m_nNavigateHistoryIndex < m_vecNavigateHistory.Count() - 1;
}

void CUI_PageManager::NavigateBack()
{
	if ( !BCanNavigateBack() )
		return;

	if ( m_pActivePage && m_pActivePage->BSuppressNavigation() )
		return;

	if ( m_pActivePage && m_pActivePage->BOverrideNavigateBack() )
	{
		m_pActivePage->NavigateBack();
		return;
	}

	if ( IsOnModalPage() )
	{
		ClearModalPage( false );
		NavigateToHistoryIndex( m_nNavigateHistoryIndex );
	}
	else
	{
		NavigateToHistoryIndex( m_nNavigateHistoryIndex - 1 );
	}

	return;
}

void CUI_PageManager::NavigateForward()
{
	if ( !BCanNavigateForward() )
		return;

	if ( m_pActivePage && m_pActivePage->BSuppressNavigation() )
		return;

	if ( m_pActivePage && m_pActivePage->BOverrideNavigateForward() )
	{
		m_pActivePage->NavigateForward();
		return;
	}

	Assert( !IsOnModalPage() );	// Illegal
	if ( IsOnModalPage() )
		return;

	NavigateToHistoryIndex( m_nNavigateHistoryIndex + 1 );
	return;
}

void CUI_PageManager::NavigateToHistoryIndex( int nIndex )
{
	if ( nIndex < 0 || nIndex >= m_vecNavigateHistory.Count() )
		return;

	m_bNavigatingHistory = true;

	m_nNavigateHistoryIndex = nIndex;

	// Make sure the selected primary tab is whatever it was when this navigate event was set
	m_strExpectedPrimaryTab = m_vecNavigateHistory[ nIndex ].strPrimaryTab;

	IUIEvent *pCopy = m_vecNavigateHistory[ nIndex ].pEvent->Copy();
	UIEngine()->DispatchEvent( pCopy );

	m_strExpectedPrimaryTab = nullptr;

	m_bNavigatingHistory = false;

	DispatchEvent( PageManagerHistoryChanged(), this );
}

void CUI_PageManager::PushNavigateHistoryEvent( panorama::IUIEvent *pEvent )
{
	// If we're in the middle of navigating via back/forward/etc, then don't record this again
	if ( m_bNavigatingHistory )
	{
		delete pEvent;
		return;
	}

	// Clear the stack ahead of where we are currently
	for ( int i = m_vecNavigateHistory.Count() - 1; i > m_nNavigateHistoryIndex; --i )
	{
		panorama::UIEngine()->DispatchEvent( PageManagerDeletingHistoryEvent::MakeEvent( this, m_vecNavigateHistory[ i ].pEvent->Copy() ) );
		m_vecNavigateHistory.Remove( i );
	}

	// Only store a maximum number of entries
	const int k_nMaxNavigateSize = 50;
	while ( m_vecNavigateHistory.Count() >= k_nMaxNavigateSize )
	{
		panorama::UIEngine()->DispatchEvent( PageManagerDeletingHistoryEvent::MakeEvent( this, m_vecNavigateHistory[ 0 ].pEvent->Copy() ) );
		m_vecNavigateHistory.Remove( 0 );
	}

	// Record the event
	m_vecNavigateHistory.AddToTail( SNavigateEvent( pEvent, m_strSelectedPrimaryTab ) );
	m_nNavigateHistoryIndex = m_vecNavigateHistory.Count() - 1;

	DispatchEvent( PageManagerHistoryChanged(), this );
}


void CUI_PageManager::ReloadCurrentPage()
{
	CUI_Page *pActivePage = m_pActivePage;
	m_pActivePage = NULL;
	delete pActivePage;

	NavigateToHistoryIndex( m_nNavigateHistoryIndex );
}

PageNavigateID_t CUI_PageManager::GetCurrentPageNavigateID() const
{
	return m_pActivePage ? m_pActivePage->GetPageNavigateID() : 0;
}

bool CUI_PageManager::EventWindowVisiblilityChanged( IUIWindow *pWindow )
{
	if ( pWindow != GetParentWindow() )
		return false;

	if ( m_pActivePage )
	{
		bool bWindowVisible = pWindow->BIsVisible();
		if ( bWindowVisible )
		{
			m_pActivePage->OnNavigatingTo( NAV_CHANGE_WINDOW_VISIBILITY );
			m_pActivePage->OnNavigatedTo( NAV_CHANGE_WINDOW_VISIBILITY );
		}
		else
		{
			m_pActivePage->OnNavigatingAway( NAV_CHANGE_WINDOW_VISIBILITY );
			m_pActivePage->OnNavigatedAway( NAV_CHANGE_WINDOW_VISIBILITY );
		}
	}

	return false;
}


/*static*/ void CUI_PageManager::RegisterPageCallback( RegisterPageCallbackFunction pFunction )
{
	if ( !s_pvecRegisterPageCallbacks )
	{
		s_pvecRegisterPageCallbacks = new CUtlVector< RegisterPageCallbackFunction >();
	}

	s_pvecRegisterPageCallbacks->AddToTail( pFunction );
}

void CUI_PageManager::NavigateBackConsumePages( CUtlVector< CPanoramaSymbol > &vecSymbolsToConsume, const char *pszFallbackEvent )
{
	int nNavigateHistoryIndex = m_nNavigateHistoryIndex;
	do
	{
		IUIEvent *pEvent = m_vecNavigateHistory[nNavigateHistoryIndex].pEvent;
		if ( !pEvent )
			break;

		bool bMatches = false;
		FOR_EACH_VEC( vecSymbolsToConsume, i )
		{
			if ( pEvent->GetEventType() == vecSymbolsToConsume[i] )
			{
				bMatches = true;
				break;
			}
		}

		if ( bMatches )
		{
			nNavigateHistoryIndex--;
		}
		else
			break;
		
	} while ( nNavigateHistoryIndex >= 0 );

	// Clear the stack ahead of where we are currently
	for ( int i = m_vecNavigateHistory.Count() - 1; i > nNavigateHistoryIndex; --i )
	{
		panorama::UIEngine()->DispatchEvent( PageManagerDeletingHistoryEvent::MakeEvent( this, m_vecNavigateHistory[i].pEvent->Copy() ) );
		m_vecNavigateHistory.Remove( i );
	}

	if ( nNavigateHistoryIndex < 0 )
	{
		Assert( m_vecNavigateHistory.Count() == 0 );

		m_nNavigateHistoryIndex = -1;

		if ( pszFallbackEvent && *pszFallbackEvent )
		{
			IUIEvent *pFallbackEvent = UIEngine()->CreateEventFromString( UIPanel(), pszFallbackEvent, &pszFallbackEvent );
			if ( pFallbackEvent )
			{
				UIEngine()->DispatchEvent( pFallbackEvent );		
			}
		}
		return;
	}
	else
	{
		// go back and find the right index we should be on and nav to that
		NavigateToHistoryIndex( nNavigateHistoryIndex );
	}	

	DispatchEvent( PageManagerHistoryChanged(), this );
}