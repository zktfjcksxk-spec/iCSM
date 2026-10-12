//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// UI PageManager
//
// A CUI_PageManager represents a collection of pages, of which only a single
// one is visible at any time. Each page is represented by a subclass of CUI_Page
// and is normally made visible by firing a "page event". See ui_page.h for
// details on how to make a new page.
// 
// A PageManager has a number of features:
//   1) Pages are allocated on-demand rather than all up front.
//   2) Maintains a history stack of pages that you can go forward/backward through.
//   3) Pages can be defined individually without having to touch a global list or the page manager.
//   4) Page events can be fired from anywhere and are automatically caught to switch the page.
//   5) Pages that aren't visible are automatically marked UnreadyForDisplay, which means they will unload their images/movies.
//=============================================================================//
#pragma once

#include "ui_page.h"

// Fired before/after a new page becomes active. Passes the old page and new page as parameters
DECLARE_PANEL_EVENT2( PageManagerActivatingPage, CUI_Page *, CUI_Page * );
DECLARE_PANEL_EVENT2( PageManagerActivatedPage, CUI_Page *, CUI_Page * );

// Fired whenever the primary tab of the current page changes.
DECLARE_PANEL_EVENT1( PageManagerSelectedPrimaryTabChanged, const char * );

// Fired whenever the history log is changed
DECLARE_PANEL_EVENT0( PageManagerHistoryChanged );

// Fired whenever the page manager clears a history event
DECLARE_PANEL_EVENT1( PageManagerDeletingHistoryEvent, panorama::IUIEvent * );

class CUI_PageManager : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CUI_PageManager, panorama::CPanel2D );

public:
	CUI_PageManager( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CUI_PageManager();

	// Get the singleton page manager
	static CUI_PageManager *GetInstance() { return s_pPageManager; }

	// Get the currently active page
	CUI_Page *GetActivePage() { return m_pActivePage; }

	// Get the panel type for the currently active page
	panorama::CPanoramaSymbol GetActivePagePanelType();

	// Insert an event into the navigation history. This is mostly done automatically whenever you switch pages,
	// but if you need to insert something special use this.
	void PushNavigateHistoryEvent( panorama::IUIEvent *pEvent );

	// Can you currently go forward/backwards through history?
	bool BCanNavigateBack();
	bool BCanNavigateForward();

	// Move forward/backwards through the history
	void NavigateBack();
	void NavigateForward();
	void NavigateBackConsumePages( CUtlVector< panorama::CPanoramaSymbol > &vecSymbolsToConsume, const char *pszFallbackEvent );

	// For debugging purposes, reload the current page
	void ReloadCurrentPage();

	// Show the page for when you click the given primary tab. Normally this is the last visited page
	// under that primary tab. If you're already viewing that tab though, it goes to the default page for that tab.
	void ShowPrimaryTabPage( const char *pszPrimaryTabID );

	// Set the event to fire as the default primary tab page
	void SetDefaultPrimaryTabEvent( const char *pszPrimaryTabID, panorama::IUIEvent *pEvent );

	// Get the current page's navigation id. This can be used as a cookie to determine whether you've switched pages
	// when responding to async input.
	PageNavigateID_t GetCurrentPageNavigateID() const;

	const char *GetActivePrimaryTabName() const { return m_strSelectedPrimaryTab; }

public:
	// For Internal Use - shouldn't need to call these directly
	template < class PageClass, class EventClass > bool EventNavigateToPage();
	template < class PageClass, class EventClass, typename T1 > bool EventNavigateToPage( T1 param1 );
	template < class PageClass, class EventClass, typename T1, typename T2 > bool EventNavigateToPage( T1 param1, T2 param2 );
	template < class PageClass, class EventClass, typename T1, typename T2, typename T3 > bool EventNavigateToPage( T1 param1, T2 param2, T3 param3 );
	template < class PageClass, class EventClass, typename T1, typename T2, typename T3, typename T4 > bool EventNavigateToPage( T1 param1, T2 param2, T3 param3, T4 param4 );
	template < class PageClass, class EventClass, typename T1, typename T2, typename T3, typename T4, typename T5 > bool EventNavigateToPage( T1 param1, T2 param2, T3 param3, T4 param4, T5 param5 );

	template < class PageClass > PageClass *GetPage();		// may return null if not yet created
	template < class PageClass > PageClass *EnsurePage();	// creates if necessary

	static void RegisterPageCallback( RegisterPageCallbackFunction pFunction );

private:
	void SwitchToPage( CUI_Page *pPage, panorama::IUIEvent *pEvent, EPageSetupResult eSetupResult );
	void RecordPageChange( CUI_Page *pPage, panorama::IUIEvent *pEvent, bool bRecordHistory );

	void UpdatePrimaryTabs( CUI_Page *pPage, panorama::IUIEvent *pEvent );

	void NavigateToHistoryIndex( int nIndex );

	void ClearModalPage( bool bClearAll );
	bool IsOnModalPage() const { return m_nModalPageDepth > 0; }

	bool EventWindowVisiblilityChanged( panorama::IUIWindow *pWindow );

	CUI_Page *m_pActivePage;

	struct SNavigateEvent
	{
		SNavigateEvent() : pEvent( nullptr ) {}
		SNavigateEvent( panorama::IUIEvent *pNewEvent, const CUtlString &strNewPrimaryTab ) : pEvent( pNewEvent ), strPrimaryTab( strNewPrimaryTab ) {}
		SNavigateEvent( const SNavigateEvent &other ) { pEvent = other.pEvent ? other.pEvent->Copy() : nullptr; strPrimaryTab = other.strPrimaryTab; }
		SNavigateEvent( SNavigateEvent &&other ) { pEvent = other.pEvent; other.pEvent = nullptr; strPrimaryTab = other.strPrimaryTab; }
		~SNavigateEvent() { delete pEvent; }

		panorama::IUIEvent *pEvent;
		CUtlString strPrimaryTab;
	};
	CUtlVector< SNavigateEvent > m_vecNavigateHistory;

	int m_nNavigateHistoryIndex;
	bool m_bNavigatingHistory;
	int m_nModalPageDepth;

	PageNavigateID_t m_unNextPageNavigateID;

	CUtlMap< CUtlString, panorama::IUIEvent * > m_mapDefaultPrimaryTabPageEvent;
	CUtlMap< CUtlString, panorama::IUIEvent * > m_mapLastPrimaryTabPageEvent;
	CUtlString m_strSelectedPrimaryTab;
	CUtlString m_strExpectedPrimaryTab;

	static CUtlVector< RegisterPageCallbackFunction > *s_pvecRegisterPageCallbacks;
	static CUI_PageManager *s_pPageManager;
};

template < class PageClass >
PageClass *CUI_PageManager::GetPage()
{
	const char *pszID = PageClass::m_symbol.String();
	return panel_cast< PageClass * >( FindChild( pszID ) );
}

template < class PageClass >
PageClass *CUI_PageManager::EnsurePage()
{
	const char *pszID = PageClass::m_symbol.String();

	PageClass *pPage = panel_cast< PageClass * >( FindChild( pszID ), true );
	if ( !pPage )
	{
		pPage = assert_cast< PageClass * >( panorama::UIEngine()->CreatePanel( PageClass::m_symbol, pszID, UIPanel() ) );
	}

	return pPage;
}

/* Ugly templated functions to call the default SetupPage function for a PAGE_EVENT */
template < class PageClass, class EventClass >
struct SetupPageEventHelper
{
	static EPageSetupResult CallSetupPage( PageClass *pPage ) { return pPage->SetupPage(); }
	template < typename T1 > static EPageSetupResult CallSetupPage( PageClass *pPage, T1 param1 ) { return pPage->SetupPage( param1 ); }
	template < typename T1, typename T2 > static EPageSetupResult CallSetupPage( PageClass *pPage, T1 param1, T2 param2 ) { return pPage->SetupPage( param1, param2 ); }
	template < typename T1, typename T2, typename T3 > static EPageSetupResult CallSetupPage( PageClass *pPage, T1 param1, T2 param2, T3 param3 ) { return pPage->SetupPage( param1, param2, param3 ); }
	template < typename T1, typename T2, typename T3, typename T4 > static EPageSetupResult CallSetupPage( PageClass *pPage, T1 param1, T2 param2, T3 param3, T4 param4 ) { return pPage->SetupPage( param1, param2, param3, param4 ); }
	template < typename T1, typename T2, typename T3, typename T4, typename T5 > static EPageSetupResult CallSetupPage( PageClass *pPage, T1 param1, T2 param2, T3 param3, T4 param4, T5 param5 ) { return pPage->SetupPage( param1, param2, param3, param4, param5 ); }
};

template < class PageClass, class EventClass >
bool CUI_PageManager::EventNavigateToPage()
{
	PageClass *pPage = EnsurePage< PageClass >();

	EPageSetupResult eSetupResult = SetupPageEventHelper< PageClass, EventClass >::CallSetupPage( pPage );
	if ( eSetupResult == PAGE_SETUP_FAILED )
		return true;

	panorama::IUIEvent *pEvent = EventClass::MakeEvent( this );
	SwitchToPage( pPage, pEvent, eSetupResult );

	return true;
}

template < class PageClass, class EventClass, typename T1 >
bool CUI_PageManager::EventNavigateToPage( T1 param1 )
{
	PageClass *pPage = EnsurePage< PageClass >();

	EPageSetupResult eSetupResult = SetupPageEventHelper< PageClass, EventClass >::CallSetupPage( pPage, param1 );
	if ( eSetupResult == PAGE_SETUP_FAILED )
		return true;

	panorama::IUIEvent *pEvent = EventClass::MakeEvent( this, param1 );
	SwitchToPage( pPage, pEvent, eSetupResult );

	return true;
}

template < class PageClass, class EventClass, typename T1, typename T2 >
bool CUI_PageManager::EventNavigateToPage( T1 param1, T2 param2 )
{
	PageClass *pPage = EnsurePage< PageClass >();

	EPageSetupResult eSetupResult = SetupPageEventHelper< PageClass, EventClass >::CallSetupPage( pPage, param1, param2 );
	if ( eSetupResult == PAGE_SETUP_FAILED )
		return true;

	panorama::IUIEvent *pEvent = EventClass::MakeEvent( this, param1, param2 );
	SwitchToPage( pPage, pEvent, eSetupResult );

	return true;
}

template < class PageClass, class EventClass, typename T1, typename T2, typename T3 >
bool CUI_PageManager::EventNavigateToPage( T1 param1, T2 param2, T3 param3 )
{
	PageClass *pPage = EnsurePage< PageClass >();

	EPageSetupResult eSetupResult = SetupPageEventHelper< PageClass, EventClass >::CallSetupPage( pPage, param1, param2, param3 );
	if ( eSetupResult == PAGE_SETUP_FAILED )
		return true;

	panorama::IUIEvent *pEvent = EventClass::MakeEvent( this, param1, param2, param3 );
	SwitchToPage( pPage, pEvent, eSetupResult );

	return true;
}

template < class PageClass, class EventClass, typename T1, typename T2, typename T3, typename T4 >
bool CUI_PageManager::EventNavigateToPage( T1 param1, T2 param2, T3 param3, T4 param4 )
{
	PageClass *pPage = EnsurePage< PageClass >();

	EPageSetupResult eSetupResult = SetupPageEventHelper< PageClass, EventClass >::CallSetupPage( pPage, param1, param2, param3, param4 );
	if ( eSetupResult == PAGE_SETUP_FAILED )
		return true;

	panorama::IUIEvent *pEvent = EventClass::MakeEvent( this, param1, param2, param3, param4 );
	SwitchToPage( pPage, pEvent, eSetupResult );

	return true;
}

template < class PageClass, class EventClass, typename T1, typename T2, typename T3, typename T4, typename T5 >
bool CUI_PageManager::EventNavigateToPage( T1 param1, T2 param2, T3 param3, T4 param4, T5 param5 )
{
	PageClass *pPage = EnsurePage< PageClass >();

	EPageSetupResult eSetupResult = SetupPageEventHelper< PageClass, EventClass >::CallSetupPage( pPage, param1, param2, param3, param4, param5 );
	if ( eSetupResult == PAGE_SETUP_FAILED )
		return true;

	panorama::IUIEvent *pEvent = EventClass::MakeEvent( this, param1, param2, param3, param4, param5 );
	SwitchToPage( pPage, pEvent, eSetupResult );

	return true;
}

