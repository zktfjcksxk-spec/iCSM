//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

class CUI_PageManager;
class CUI_Page;

// A PageNavigateID_t is a cookie you can use to figure out whether a page is
// still active in response to async input.
typedef uint32 PageNavigateID_t;

typedef void( *RegisterPageCallbackFunction )( CUI_PageManager * );

enum NavigationChangeReason_t
{
	// Switched to a different page
	NAV_CHANGE_SWITCHED_PAGE = 0,

	// Window became visible/invisible
	NAV_CHANGE_WINDOW_VISIBILITY = 1,
};

// Return values from the SetupPage functions on a page.
enum EPageSetupResult
{
	// Failure. Will not transition to this page.
	PAGE_SETUP_FAILED				= 0,

	// Success, but nothing changed on this page. Won't record history if you were already on the page.
	PAGE_SETUP_NO_CHANGE			= 1,

	// Success: record history of this event.
	PAGE_SETUP_SUCCESS				= 2,

	// Success: replace the most recent history event with this one.
	PAGE_SETUP_SUCCESS_REPLACE		= 3,

	// Success: don't record history, as the page will do something special manually.
	PAGE_SETUP_SUCCESS_NO_HISTORY	= 4,

	// Success: transition to this page normally, but mark it as a "modal" page. This means that
	// as soon as you leave this page, it will remove itself from the history stack, unless you go
	// to another modal page. Think of it as a hybrid "popup"-ish page. This should be uncommon, because
	// it always feels weird.
	PAGE_SETUP_SUCCESS_MODAL		= 5,
};
