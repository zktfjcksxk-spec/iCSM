//=========== Copyright Valve Corporation, All rights reserved. ===============//
// 
// Helper macros for creating / showing pages.
//=============================================================================//
#pragma once

#include "ui_page_types.h"
#include "ui_page.h"
#include "ui_page_manager.h"

// No Parameters
template < class PageClass, class EventClass >
class CUI_Page_RegisterHelper0
{
public:
	CUI_Page_RegisterHelper0() { CUI_PageManager::RegisterPageCallback( &RegisterShowPage ); }
	static void RegisterShowPage( CUI_PageManager *pPageManager ) { RegisterForUnhandledEvent( EventClass(), pPageManager, &CUI_PageManager::EventNavigateToPage< PageClass, EventClass > ); }
};

#define DECLARE_UI_PAGE_EVENT0( eventName ) \
	DECLARE_PANORAMA_EVENT0( eventName )

#define DEFINE_UI_PAGE_EVENT0( className, eventName ) \
	DEFINE_PANORAMA_EVENT( eventName ) \
	static CUI_Page_RegisterHelper0< className, eventName > g_Register_##eventName

#define DECLARE_UI_PAGE_EVENT_SETUP0( eventName, setupFunction ) \
	DECLARE_UI_PAGE_EVENT0( eventName ); \
	template < class pageClass > struct SetupPageEventHelper< pageClass, eventName > \
	{ \
		static EPageSetupResult CallSetupPage( pageClass *pPage ) \
			{ pPage->setupFunction(); } \
	} \


// 1 Parameter
template < class PageClass, class EventClass, typename ParamType1 >
class CUI_Page_RegisterHelper1
{
public:
	CUI_Page_RegisterHelper1() { CUI_PageManager::RegisterPageCallback( &RegisterShowPage ); }
	static void RegisterShowPage( CUI_PageManager *pPageManager ) { RegisterForUnhandledEvent( EventClass(), pPageManager, &CUI_PageManager::EventNavigateToPage< PageClass, EventClass, ParamType1 > ); }
};

#define DECLARE_UI_PAGE_EVENT1( eventName, paramType1 ) \
	DECLARE_PANORAMA_EVENT1( eventName, paramType1 )

#define DEFINE_UI_PAGE_EVENT1( className, eventName, paramType1 ) \
	DEFINE_PANORAMA_EVENT( eventName ) \
	static CUI_Page_RegisterHelper1< className, eventName, paramType1 > g_Register_##eventName

#define DECLARE_UI_PAGE_EVENT_SETUP1( eventName, setupFunction, paramType1 ) \
	DECLARE_UI_PAGE_EVENT1( eventName, paramType1 ); \
	template < class pageClass > struct SetupPageEventHelper< pageClass, eventName > \
	{ \
		static EPageSetupResult CallSetupPage( pageClass *pPage, paramType1 param1 ) \
			{ return pPage->setupFunction( param1 ); } \
	}


// 2 Parameters
template < class PageClass, class EventClass, typename ParamType1, typename ParamType2 >
class CUI_Page_RegisterHelper2
{
public:
	CUI_Page_RegisterHelper2() { CUI_PageManager::RegisterPageCallback( &RegisterShowPage ); }
	static void RegisterShowPage( CUI_PageManager *pPageManager ) { RegisterForUnhandledEvent( EventClass(), pPageManager, &CUI_PageManager::EventNavigateToPage< PageClass, EventClass, ParamType1, ParamType2 > ); }
};

#define DECLARE_UI_PAGE_EVENT2( eventName, paramType1, paramType2 ) \
	DECLARE_PANORAMA_EVENT2( eventName, paramType1, paramType2 )

#define DEFINE_UI_PAGE_EVENT2( className, eventName, paramType1, paramType2 ) \
	DEFINE_PANORAMA_EVENT( eventName ) \
	static CUI_Page_RegisterHelper2< className, eventName, paramType1, paramType2 > g_Register_##eventName

#define DECLARE_UI_PAGE_EVENT_SETUP2( eventName, setupFunction, paramType1, paramType2 ) \
	DECLARE_UI_PAGE_EVENT2( eventName, paramType1, paramType2 ); \
	template < class pageClass > struct SetupPageEventHelper< pageClass, eventName > \
	{ \
		static EPageSetupResult CallSetupPage( pageClass *pPage, paramType1 param1, paramType2 param2 ) \
			{ pPage->setupFunction( param1, param2 ); } \
	}


// 3 Parameters
template < class PageClass, class EventClass, typename ParamType1, typename ParamType2, typename ParamType3 >
class CUI_Page_RegisterHelper3
{
public:
	CUI_Page_RegisterHelper3() { CUI_PageManager::RegisterPageCallback( &RegisterShowPage ); }
	static void RegisterShowPage( CUI_PageManager *pPageManager ) { RegisterForUnhandledEvent( EventClass(), pPageManager, &CUI_PageManager::EventNavigateToPage< PageClass, EventClass, ParamType1, ParamType2, ParamType3 > ); }
};

#define DECLARE_UI_PAGE_EVENT3( eventName, paramType1, paramType2, paramType3 ) \
	DECLARE_PANORAMA_EVENT3( eventName, paramType1, paramType2, paramType3 )

#define DEFINE_UI_PAGE_EVENT3( className, eventName, paramType1, paramType2, paramType3 ) \
	DEFINE_PANORAMA_EVENT( eventName ) \
	static CUI_Page_RegisterHelper3< className, eventName, paramType1, paramType2, paramType3 > g_Register_##eventName

#define DECLARE_UI_PAGE_EVENT_SETUP3( eventName, setupFunction, paramType1, paramType2, paramType3 ) \
	DECLARE_UI_PAGE_EVENT3( eventName, paramType1, paramType2, paramType3 ); \
	template < class pageClass > struct SetupPageEventHelper< pageClass, eventName > \
	{ \
		static EPageSetupResult CallSetupPage( pageClass *pPage, paramType1 param1, paramType2 param2, paramType3 param3  ) \
			{ pPage->setupFunction( param1, param2, param3 ); } \
	}


// 4 Parameters
template < class PageClass, class EventClass, typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4 >
class CUI_Page_RegisterHelper4
{
public:
	CUI_Page_RegisterHelper4() { CUI_PageManager::RegisterPageCallback( &RegisterShowPage ); }
	static void RegisterShowPage( CUI_PageManager *pPageManager ) { RegisterForUnhandledEvent( EventClass(), pPageManager, &CUI_PageManager::EventNavigateToPage< PageClass, EventClass, ParamType1, ParamType2, ParamType3, ParamType4 > ); }
};

#define DECLARE_UI_PAGE_EVENT4( eventName, paramType1, paramType2, paramType3, paramType4 ) \
	DECLARE_PANORAMA_EVENT4( eventName, paramType1, paramType2, paramType3, paramType4 )

#define DEFINE_UI_PAGE_EVENT4( className, eventName, paramType1, paramType2, paramType3, paramType4 ) \
	DEFINE_PANORAMA_EVENT( eventName ) \
	static CUI_Page_RegisterHelper4< className, eventName, paramType1, paramType2, paramType3, paramType4 > g_Register_##eventName

#define DECLARE_UI_PAGE_EVENT_SETUP4( eventName, setupFunction, paramType1, paramType2, paramType3, paramType4 ) \
	DECLARE_UI_PAGE_EVENT4( eventName, paramType1, paramType2, paramType3, paramType4 ); \
	template < class pageClass > struct SetupPageEventHelper< pageClass, eventName > \
	{ \
		static EPageSetupResult CallSetupPage( pageClass *pPage, paramType1 param1, paramType2 param2, paramType3 param3, paramType4 param4  ) \
			{ pPage->setupFunction( param1, param2, param3, param4 ); } \
	}


// 5 Parameters
template < class PageClass, class EventClass, typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4, typename ParamType5 >
class CUI_Page_RegisterHelper5
{
public:
	CUI_Page_RegisterHelper5() { CUI_PageManager::RegisterPageCallback( &RegisterShowPage ); }
	static void RegisterShowPage( CUI_PageManager *pPageManager ) { RegisterForUnhandledEvent( EventClass(), pPageManager, &CUI_PageManager::EventNavigateToPage< PageClass, EventClass, ParamType1, ParamType2, ParamType3, ParamType4, ParamType5 > ); }
};

#define DECLARE_UI_PAGE_EVENT5( eventName, paramType1, paramType2, paramType3, paramType4, paramType5 ) \
	DECLARE_PANORAMA_EVENT5( eventName, paramType1, paramType2, paramType3, paramType4, paramType5 )

#define DEFINE_UI_PAGE_EVENT5( className, eventName, paramType1, paramType2, paramType3, paramType4, paramType5 ) \
	DEFINE_PANORAMA_EVENT( eventName ) \
	static CUI_Page_RegisterHelper5< className, eventName, paramType1, paramType2, paramType3, paramType4, paramType5 > g_Register_##eventName

#define DECLARE_UI_PAGE_EVENT_SETUP5( eventName, setupFunction, paramType1, paramType2, paramType3, paramType4, paramType5 ) \
	DECLARE_UI_PAGE_EVENT5( eventName, paramType1, paramType2, paramType3, paramType4, paramType5 ); \
	template < class pageClass > struct SetupPageEventHelper< pageClass, eventName > \
	{ \
		static EPageSetupResult CallSetupPage( pageClass *pPage, paramType1 param1, paramType2 param2, paramType3 param3, paramType4 param4, paramType5 param5 ) \
			{ pPage->setupFunction( param1, param2, param3, param4, param5 ); } \
	}


