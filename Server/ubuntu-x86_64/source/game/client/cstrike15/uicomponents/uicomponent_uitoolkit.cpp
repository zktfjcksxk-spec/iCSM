//========= Copyright (C) 1996-2016, Valve Corporation, All rights reserved. ============//
//
// Purpose: Helper to show popups, context menu from Panorama Javascript.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "uicomponent_uitoolkit.h"

#include "panorama/ui_context_menu_manager.h"
#include "panorama/ui_popup_manager.h"
#include "panorama/ui_tooltip_manager.h"
#include "panorama/popups/ui_popup_custom_layout.h"
#include "panorama/controls/contextmenu.h"
#include "panorama/context_menus/ui_context_menu_custom_layout.h"
#include "panorama/ui_js_panel.h"
#include "panorama/csgo_globalpopups.h"

#include "IGameUIFuncs.h"
#include "gameui_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//////////////////////////////////////////////////////////////////////////
//
// Helper functions
//

static EPopupBackgroundStyle GetBackgroundStyleFromString( const char *pszBackgroundStyle )
{
	EPopupBackgroundStyle eBackgroundStyle = k_EPopupBackgroundStyle_None;

	Assert( pszBackgroundStyle );
	if ( V_stricmp( pszBackgroundStyle, "none" ) == 0 )
	{
		eBackgroundStyle = k_EPopupBackgroundStyle_None;
	}
	else if ( V_stricmp( pszBackgroundStyle, "dim" ) == 0 )
	{
		eBackgroundStyle = k_EPopupBackgroundStyle_Dim;
	}
	else if ( V_stricmp( pszBackgroundStyle, "dim_dismiss" ) == 0 )
	{
		eBackgroundStyle = k_EPopupBackgroundStyle_Dim_Dismiss;
	}
	else if ( V_stricmp( pszBackgroundStyle, "blur" ) == 0 )
	{
		eBackgroundStyle = k_EPopupBackgroundStyle_Blur;
	}
	else if ( V_stricmp( pszBackgroundStyle, "blur_dismiss" ) == 0 )
	{
		eBackgroundStyle = k_EPopupBackgroundStyle_Blur_Dismiss;
	}
	else
	{
		Warning( "CUiComponent_Popups - Unknown background style \"%s\". Valid value are \"none\", \"dim\" or \"blur\". Setting it to \"none\".\n", pszBackgroundStyle );
	}

	return eBackgroundStyle;
}

// Get the panel that contains the javascript context for the current callback
static panorama::IUIPanel *GetPanelForJavaScriptContext()
{
	v8::Isolate *pIsolate = v8::Isolate::GetCurrent();
	if ( !pIsolate )
		return nullptr;

	return panorama::UIEngine()->GetPanelForJavaScriptContext( *( pIsolate->GetCurrentContext() ) );
}


//////////////////////////////////////////////////////////////////////////
//
// Component instance
//

SF_COMPONENT_API_DEF_BEGIN( CUiComponent_UiToolkit )
SF_COMPONENT_API_DEF_END( CUiComponent_UiToolkit )

PANORAMA_COMPONENT_API_DEF_BEGIN( CUiComponent_UiToolkit )
#define UI_COMPONENT_FUNCTIONLIST_ELEMENT( returntype, fnname, argNames, description ) PANORAMA_COMPONENT_FUNCTION_API_DEF_DOC( returntype, fnname, CUiComponent_UiToolkit, argNames, description )
#define UI_COMPONENT_FUNCTIONLIST_ELEMENT_RAW( returntype, fnname, description ) PANORAMA_COMPONENT_FUNCTION_RAW_API_DEF_DOC( returntype, fnname, CUiComponent_UiToolkit, description )
#include "uicomponent_uitoolkit.functions.inc"
PANORAMA_COMPONENT_API_DEF_END( CUiComponent_UiToolkit )

UI_COMPONENT_API_DEF_COMMON_DOC( CUiComponent_UiToolkit, UiToolkit, "Helper to show popups, tooltips, context menus and to register/invoke js callbacks." )

PANORAMA_COMPONENT_DECLARE_EVENT2( UiToolkit, RunJSFunction, panorama::CPanelPtr<panorama::IUIPanel>, v8::Persistent<v8::Function> * );
PANORAMA_COMPONENT_DEFINE_EVENT( UiToolkit, RunJSFunction );


//////////////////////////////////////////////////////////////////////////
//
// CUiComponent_UiToolkit Methods
//

//-----------------------------------------------------------------------------
CUiComponent_UiToolkit::CUiComponent_UiToolkit()
{
	RegisterForUnhandledEvent( PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )(), this, &CUiComponent_UiToolkit::OnRunJSFunction );

	// Create global object

	v8::Isolate *pIsolate = panorama::UIEngine()->GetV8Isolate();
	v8::Isolate::Scope isolate_scope( pIsolate );
	v8::HandleScope handle_scope( pIsolate );

	v8::Persistent<v8::Context> &perContext = panorama::UIEngine()->GetV8GlobalContext();
	v8::Handle<v8::Context> context = v8::Local<v8::Context>::New( pIsolate, perContext );
	v8::Context::Scope context_scope( context );

	m_GlobalObject.Reset(pIsolate, v8::Object::New( pIsolate ) );
}

//-----------------------------------------------------------------------------
CUiComponent_UiToolkit::~CUiComponent_UiToolkit()
{

}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopup( const char *pszTitle, const char *pszMessage, const char *pszStyle )
{
	return ShowGenericPopupBgStyle( pszTitle, pszMessage, pszStyle, nullptr );
}


//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, const char *pszBackgroundStyle )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	CUI_PopupManager *pPopupManager = CUI_PopupManager::GetPanelPopupManager( ToPanel2D( pContextPanel ) );

	CUI_Popup_Generic *pPopup = NULL;
	if ( pPopupManager )
	{
		pPopup = new CUI_Popup_Generic( pPopupManager, nullptr, nullptr );
		pPopup->SetDisplayOk( pszTitle, pszMessage, nullptr );
		if ( pszBackgroundStyle )
		{
			pPopup->SetPopupBackgroundStyle( GetBackgroundStyleFromString( pszBackgroundStyle ) );
		}

		if ( pszStyle )
		{
			pPopup->AddClass( pszStyle );
		}

		pPopupManager->ShowPopup( pPopup );
	}
	return pPopup ? pPopup->UIPanel() : NULL;
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupOk( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSOkFunc )
{
	return ShowGenericPopupOkBgStyle( pszTitle, pszMessage, pszStyle, pJSOkFunc, nullptr );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupOkBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSOkFunc, const char *pszBackgroundStyle )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	panorama::IUIEvent *pOkEvent = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSOkFunc );
	
	CUtlVector< PopupChoiceParamEvent_t > vecOptions;
	vecOptions.AddToTail( { "#OK", pOkEvent } );
	return ShowGenericPopupInternal( pszTitle, pszMessage, pszStyle, vecOptions, pszBackgroundStyle, pOkEvent );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupCancel( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSCancelFunc )
{
	return ShowGenericPopupCancelBgStyle( pszTitle, pszMessage, pszStyle, pJSCancelFunc, nullptr );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupCancelBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSCancelFunc, const char *pszBackgroundStyle )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	panorama::IUIEvent *pCancelEvent = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSCancelFunc );

	CUtlVector< PopupChoiceParamEvent_t > vecOptions;
	vecOptions.AddToTail( { "#Cancel", pCancelEvent } );
	return ShowGenericPopupInternal( pszTitle, pszMessage, pszStyle, vecOptions, pszBackgroundStyle, pCancelEvent );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupYesNo( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSYesFunc, v8::Persistent<v8::Function> *pJSNoFunc )
{
	return ShowGenericPopupYesNoBgStyle( pszTitle, pszMessage, pszStyle, pJSYesFunc, pJSNoFunc, nullptr );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupYesNoBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSYesFunc, v8::Persistent<v8::Function> *pJSNoFunc, const char *pszBackgroundStyle )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	panorama::IUIEvent *pYesEvent = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSYesFunc );
	panorama::IUIEvent *pNoEvent = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSNoFunc );

	CUtlVector< PopupChoiceParamEvent_t > vecOptions;
	vecOptions.AddToTail( { "#UI_Yes", pYesEvent } );
	vecOptions.AddToTail( { "#UI_No", pNoEvent } );
	return ShowGenericPopupInternal( pszTitle, pszMessage, pszStyle, vecOptions, pszBackgroundStyle, nullptr );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupOkCancel( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSOkFunc, v8::Persistent<v8::Function> *pJSCancelFunc )
{
	return ShowGenericPopupOkCancelBgStyle( pszTitle, pszMessage, pszStyle, pJSOkFunc, pJSCancelFunc, nullptr );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupOkCancelBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSOkFunc, v8::Persistent<v8::Function> *pJSCancelFunc, const char *pszBackgroundStyle )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	panorama::IUIEvent *pOkEvent = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSOkFunc );
	panorama::IUIEvent *pCancelEvent = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSCancelFunc );

	CUtlVector< PopupChoiceParamEvent_t > vecOptions;
	vecOptions.AddToTail( { "#OK", pOkEvent } );
	vecOptions.AddToTail( { "#Cancel", pCancelEvent } );
	return ShowGenericPopupInternal( pszTitle, pszMessage, pszStyle, vecOptions, pszBackgroundStyle, pCancelEvent );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupYesNoCancel( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSYesFunc, v8::Persistent<v8::Function> *pJSNoFunc, v8::Persistent<v8::Function> *pJSCancelFunc )
{
	return ShowGenericPopupYesNoCancelBgStyle( pszTitle, pszMessage, pszStyle, pJSYesFunc, pJSNoFunc, pJSCancelFunc, nullptr );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupYesNoCancelBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSYesFunc, v8::Persistent<v8::Function> *pJSNoFunc, v8::Persistent<v8::Function> *pJSCancelFunc, const char *pszBackgroundStyle )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	panorama::IUIEvent *pYesEvent = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSYesFunc );
	panorama::IUIEvent *pNoEvent = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSNoFunc );
	panorama::IUIEvent *pCancelEvent = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSCancelFunc );

	CUtlVector< PopupChoiceParamEvent_t > vecOptions;
	vecOptions.AddToTail( { "#UI_Yes", pYesEvent } );
	vecOptions.AddToTail( { "#UI_No", pNoEvent } );
	vecOptions.AddToTail( { "#Cancel", pCancelEvent } );
	return ShowGenericPopupInternal( pszTitle, pszMessage, pszStyle, vecOptions, pszBackgroundStyle, nullptr );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupOneOption( const char *pszTitle, const char *pszMessage, const char *pszStyle, const char *pszOption1, v8::Persistent<v8::Function> *pJSFunc1 )
{
	return ShowGenericPopupOneOptionBgStyle( pszTitle, pszMessage, pszStyle, pszOption1, pJSFunc1, nullptr );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupOneOptionBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, const char *pszOption1, v8::Persistent<v8::Function> *pJSFunc1, const char *pszBackgroundStyle )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	panorama::IUIEvent *pEvent1 = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSFunc1 );

	CUtlVector< PopupChoiceParamEvent_t > vecOptions;
	vecOptions.AddToTail( { pszOption1, pEvent1 } );
	return ShowGenericPopupInternal( pszTitle, pszMessage, pszStyle, vecOptions, pszBackgroundStyle, pEvent1 );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupTwoOptions( const char *pszTitle, const char *pszMessage, const char *pszStyle, const char *pszOption1, v8::Persistent<v8::Function> *pJSFunc1, const char *pszOption2, v8::Persistent<v8::Function> *pJSFunc2 )
{
	return ShowGenericPopupTwoOptionsBgStyle( pszTitle, pszMessage, pszStyle, pszOption1, pJSFunc1, pszOption2, pJSFunc2, nullptr );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupTwoOptionsBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, const char *pszOption1, v8::Persistent<v8::Function> *pJSFunc1, const char *pszOption2, v8::Persistent<v8::Function> *pJSFunc2, const char *pszBackgroundStyle )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	panorama::IUIEvent *pEvent1 = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSFunc1 );
	panorama::IUIEvent *pEvent2 = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSFunc2 );

	CUtlVector< PopupChoiceParamEvent_t > vecOptions;
	vecOptions.AddToTail( { pszOption1, pEvent1 } );
	vecOptions.AddToTail( { pszOption2, pEvent2 } );
	return ShowGenericPopupInternal( pszTitle, pszMessage, pszStyle, vecOptions, pszBackgroundStyle, nullptr );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupThreeOptions( const char *pszTitle, const char *pszMessage, const char *pszStyle, const char *pszOption1, v8::Persistent<v8::Function> *pJSFunc1, const char *pszOption2, v8::Persistent<v8::Function> *pJSFunc2, const char *pszOption3, v8::Persistent<v8::Function> *pJSFunc3 )
{
	return ShowGenericPopupThreeOptionsBgStyle( pszTitle, pszMessage, pszStyle, pszOption1, pJSFunc1, pszOption2, pJSFunc2, pszOption3, pJSFunc3, nullptr );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupThreeOptionsBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, const char *pszOption1, v8::Persistent<v8::Function> *pJSFunc1, const char *pszOption2, v8::Persistent<v8::Function> *pJSFunc2, const char *pszOption3, v8::Persistent<v8::Function> *pJSFunc3, const char *pszBackgroundStyle )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	panorama::IUIEvent *pEvent1 = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSFunc1 );
	panorama::IUIEvent *pEvent2 = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSFunc2 );
	panorama::IUIEvent *pEvent3 = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSFunc3 );

	CUtlVector< PopupChoiceParamEvent_t > vecOptions;
	vecOptions.AddToTail( { pszOption1, pEvent1 } );
	vecOptions.AddToTail( { pszOption2, pEvent2 } );
	vecOptions.AddToTail( { pszOption3, pEvent3 } );
	return ShowGenericPopupInternal( pszTitle, pszMessage, pszStyle, vecOptions, pszBackgroundStyle, nullptr );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowCustomLayoutPopup( const char *pszPopupID, const char *pszLayoutFile )
{
	return ShowCustomLayoutPopupParameters( pszPopupID, pszLayoutFile, nullptr );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowCustomLayoutPopupParameters( const char *pszPopupID, const char *pszLayoutFile, const char *pszParameters )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();

	CUI_PopupManager *pPopupManager = CUI_PopupManager::GetPanelPopupManager( ToPanel2D( pContextPanel ) );

	CUI_Popup_CustomLayout *pPopup = new CUI_Popup_CustomLayout( pPopupManager, pszPopupID, nullptr );
	pPopup->Init( pszLayoutFile, pszParameters );

	pPopupManager->ShowPopup( pPopup );

	return pPopup->UIPanel();
}

panorama::IUIPanel * CUiComponent_UiToolkit::ShowGlobalCustomLayoutPopup( const char *pszPopupID, const char *pszLayoutFile )
{
	return ShowGlobalCustomLayoutPopupParameters( pszPopupID, pszLayoutFile, nullptr );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGlobalCustomLayoutPopupParameters( const char *pszPopupID, const char *pszLayoutFile, const char *pszParameters )
{
	CUI_PopupManager *pPopupManager = CCSGO_GlobalPopups::GetInstance()->GetPopupManager();

	CUI_Popup_CustomLayout *pPopup = new CUI_Popup_CustomLayout( pPopupManager, pszPopupID, nullptr );
	pPopup->Init( pszLayoutFile, pszParameters );

	pPopupManager->ShowPopup( pPopup );

	return pPopup->UIPanel();
}

//-----------------------------------------------------------------------------
panorama::IUIPanel * CUiComponent_UiToolkit::ShowGenericPopupInternal( const char *pszTitle, const char *pszMessage, const char *pszStyle, const CUtlVector< PopupChoiceParamEvent_t > &vecOptions, const char *pszBackgroundStyle, panorama::IUIEvent *pCancelEvent /*= nullptr*/ )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	CUI_PopupManager *pPopupManager = CUI_PopupManager::GetPanelPopupManager( ToPanel2D( pContextPanel ) );

	CUI_Popup_Generic *pPopup = NULL;
	if ( pPopupManager )
	{
		pPopup = new CUI_Popup_Generic( pPopupManager, nullptr, nullptr );
		pPopup->SetDisplayParameters( pszTitle, pszMessage, vecOptions );
		if ( pszBackgroundStyle )
		{
			pPopup->SetPopupBackgroundStyle( GetBackgroundStyleFromString( pszBackgroundStyle ) );
		}
		if ( pCancelEvent )
		{
			pPopup->SetDismissAndCancelEvent( pCancelEvent );
		}

		if ( pszStyle )
		{
			pPopup->AddClass( pszStyle );
		}

		pPopupManager->ShowPopup( pPopup );
	}
	return pPopup ? pPopup->UIPanel() : NULL;
}

void CUiComponent_UiToolkit::CloseAllVisiblePopups()
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	CUI_PopupManager *pPopupManager = CUI_PopupManager::GetPanelPopupManager( ToPanel2D( pContextPanel ) );
	pPopupManager->CloseAllVisiblePopups();
}

//-----------------------------------------------------------------------------
bool CUiComponent_UiToolkit::OnRunJSFunction( panorama::CPanelPtr<panorama::IUIPanel> panelContext, v8::Persistent<v8::Function> *pJSFunc )
{
	// If panel context is gone, don't try to call
	panorama::IUIPanel *pPanel = panelContext.Get();
	if ( pPanel == NULL )
	{
		Warning( "CUiComponent_UiToolkit: Context panel for function no longer exists, failing to dispatch." );
		return true;
	}

	panorama::UIEngine()->RunFunction( pPanel, pJSFunc, 0, nullptr, false );

	return true;
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::ShowTextTooltip( const char *pszTargetPanelID, const char *pszText )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	if ( !pContextPanel )
		return;

	panorama::IUIPanel *pTargetPanel = pContextPanel->FindChildTraverse( pszTargetPanelID );
	if ( !pTargetPanel )
	{
		Warning( "UiToolkit.ShowTextTooltip - Unable to find target panel %s.\n", pszTargetPanelID );
		return;
	}
	
	panorama::UIEngine()->DispatchEvent( UIShowTextTooltip::MakeEvent( pTargetPanel->ClientPtr(), pszText ) );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::ShowTextTooltipOnPanel( panorama::CPanel2D *pTargetPanel, const char *pszText )
{
	panorama::UIEngine()->DispatchEvent( UIShowTextTooltip::MakeEvent( pTargetPanel, pszText ) );
}


//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::ShowTextTooltipStyled( const char *pszTargetPanelID, const char *pszText, const char *pszClass )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	if ( !pContextPanel )
		return;

	panorama::IUIPanel *pTargetPanel = pContextPanel->FindChildTraverse( pszTargetPanelID );
	if ( !pTargetPanel )
	{
		Warning( "UiToolkit.ShowTextTooltipStyled - Unable to find target panel %s.\n", pszTargetPanelID );
		return;
	}

	panorama::UIEngine()->DispatchEvent( UIShowTextTooltipStyled::MakeEvent( pTargetPanel->ClientPtr(), pszText, pszClass ) );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::ShowTextTooltipOnPanelStyled( panorama::CPanel2D *pTargetPanel, const char *pszText, const char *pszClass )
{
	panorama::UIEngine()->DispatchEvent( UIShowTextTooltipStyled::MakeEvent( pTargetPanel, pszText, pszClass ) );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::HideTextTooltip()
{
	panorama::UIEngine()->DispatchEvent( UIHideTextTooltip::MakeEvent( nullptr ) );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::ShowTitleTextTooltip( const char *pszTargetPanelID, const char *pszTitle, const char *pszText )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	if ( !pContextPanel )
		return;

	panorama::IUIPanel *pTargetPanel = pContextPanel->FindChildTraverse( pszTargetPanelID );
	if ( !pTargetPanel )
	{
		Warning( "UiToolkit.ShowTitleTextTooltip - Unable to find target panel %s.\n", pszTargetPanelID );
		return;
	}

	panorama::UIEngine()->DispatchEvent( UIShowTitleTextTooltip::MakeEvent( pTargetPanel->ClientPtr(), pszTitle, pszText ) );
}

void CUiComponent_UiToolkit::ShowTitleTextTooltipStyled( const char *pszTargetPanelID, const char *pszTitle, const char *pszText, const char *pszClass )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	if ( !pContextPanel )
		return;

	panorama::IUIPanel *pTargetPanel = pContextPanel->FindChildTraverse( pszTargetPanelID );
	if ( !pTargetPanel )
	{
		Warning( "UiToolkit.ShowTitleTextTooltipStyled - Unable to find target panel %s.\n", pszTargetPanelID );
		return;
	}

	panorama::UIEngine()->DispatchEvent( UIShowTitleTextTooltipStyled::MakeEvent( pTargetPanel->ClientPtr(), pszTitle, pszText, pszClass ) );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::HideTitleTextTooltip()
{
	panorama::UIEngine()->DispatchEvent( UIHideTitleTextTooltip::MakeEvent( nullptr ) );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::ShowTitleImageTextTooltip( const char *pszTargetPanelID, const char *pszTitle, const char *pszImagePath, const char *pszText )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	if ( !pContextPanel )
		return;

	panorama::IUIPanel *pTargetPanel = pContextPanel->FindChildTraverse( pszTargetPanelID );
	if ( !pTargetPanel )
	{
		Warning( "UiToolkit.ShowTitleImageTextTooltip - Unable to find target panel %s.\n", pszTargetPanelID );
		return;
	}

	panorama::UIEngine()->DispatchEvent( UIShowTitleImageTextTooltip::MakeEvent( pTargetPanel->ClientPtr(), pszTitle, pszImagePath, pszText ) );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::ShowTitleImageTextTooltipStyled( const char *pszTargetPanelID, const char *pszTitle, const char *pszImagePath, const char *pszText, const char *pszClass )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	if ( !pContextPanel )
		return;

	panorama::IUIPanel *pTargetPanel = pContextPanel->FindChildTraverse( pszTargetPanelID );
	if ( !pTargetPanel )
	{
		Warning( "UiToolkit.ShowTitleImageTextTooltipStyled - Unable to find target panel %s.\n", pszTargetPanelID );
		return;
	}

	panorama::UIEngine()->DispatchEvent( UIShowTitleImageTextTooltipStyled::MakeEvent( pTargetPanel->ClientPtr(), pszTitle, pszImagePath, pszText, pszClass ) );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::HideTitleImageTextTooltip()
{
	panorama::UIEngine()->DispatchEvent( UIHideTitleImageTextTooltip::MakeEvent( nullptr ) );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::ShowCustomLayoutParametersTooltip( const char *pszTargetPanelID, const char *pszTooltipID, const char *pszLayoutXml, const char *pszParameters )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	if ( !pContextPanel )
		return;

	panorama::IUIPanel *pTargetPanel = pContextPanel->FindChildTraverse( pszTargetPanelID );
	if ( !pTargetPanel )
	{
		Warning( "UiToolkit.ShowCustomLayoutParametersTooltip - Unable to find target panel %s.\n", pszTargetPanelID );
		return;
	}

	panorama::UIEngine()->DispatchEvent( UIShowCustomLayoutParametersTooltip::MakeEvent( pTargetPanel->ClientPtr(), pszTooltipID, pszLayoutXml, pszParameters ) );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::ShowCustomLayoutParametersTooltipStyled( const char *pszTargetPanelID, const char *pszTooltipID, const char *pszLayoutXml, const char *pszParameters, const char *pszClass )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	if ( !pContextPanel )
		return;

	panorama::IUIPanel *pTargetPanel = pContextPanel->FindChildTraverse( pszTargetPanelID );
	if ( !pTargetPanel )
	{
		Warning( "UiToolkit.ShowCustomLayoutParametersTooltip - Unable to find target panel %s.\n", pszTargetPanelID );
		return;
	}

	panorama::UIEngine()->DispatchEvent( UIShowCustomLayoutParametersTooltipStyled::MakeEvent( pTargetPanel->ClientPtr(), pszTooltipID, pszLayoutXml, pszParameters, pszClass ) );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::ShowCustomLayoutTooltip( const char *pszTargetPanelID, const char *pszTooltipID, const char *pszLayoutXml )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	if ( !pContextPanel )
		return;

	panorama::IUIPanel *pTargetPanel = pContextPanel->FindChildTraverse( pszTargetPanelID );
	if ( !pTargetPanel )
	{
		Warning( "UiToolkit.ShowCustomLayoutTooltip - Unable to find target panel %s.\n", pszTargetPanelID );
		return;
	}

	panorama::UIEngine()->DispatchEvent( UIShowCustomLayoutTooltip::MakeEvent( pTargetPanel->ClientPtr(), pszTooltipID, pszLayoutXml ) );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::ShowCustomLayoutTooltipStyled( const char *pszTargetPanelID, const char *pszTooltipID, const char *pszLayoutXml, const char *pszClass )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	if ( !pContextPanel )
		return;

	panorama::IUIPanel *pTargetPanel = pContextPanel->FindChildTraverse( pszTargetPanelID );
	if ( !pTargetPanel )
	{
		Warning( "UiToolkit.ShowCustomLayoutTooltip - Unable to find target panel %s.\n", pszTargetPanelID );
		return;
	}

	panorama::UIEngine()->DispatchEvent( UIShowCustomLayoutTooltipStyled::MakeEvent( pTargetPanel->ClientPtr(), pszTooltipID, pszLayoutXml, pszClass ) );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::HideCustomLayoutTooltip( const char *pszTooltipID )
{
	panorama::UIEngine()->DispatchEvent( UIHideCustomLayoutTooltip::MakeEvent( nullptr, pszTooltipID ) );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel *CUiComponent_UiToolkit::ShowSimpleContextMenu( const char *pszTargetPanelID, const char *pszContextMenuId, v8::Local<v8::Array> items )
{
	return ShowSimpleContextMenuWithDismissEvent( pszTargetPanelID, pszContextMenuId, items, nullptr );
}

panorama::IUIPanel *CUiComponent_UiToolkit::ShowSimpleContextMenuWithDismissEvent( const char *pszTargetPanelID, const char *pszContextMenuId, v8::Local<v8::Array> items, v8::Persistent<v8::Function> *pJSDismissFunc)
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	CUI_ContextMenuManager *pContextMenuManager = CUI_ContextMenuManager::GetPanelContextMenuManager( ToPanel2D( pContextPanel ) );
	if ( !pContextMenuManager )
		return NULL;

	panorama::CSimpleContextMenu *pContextMenu = new panorama::CSimpleContextMenu( pContextMenuManager, pszContextMenuId, nullptr );
	v8::Isolate *pIsolate = v8::Isolate::GetCurrent();
	if ( !pIsolate )
		return NULL;
	v8::HandleScope handle_scope( pIsolate );

	// Iterate over the items array and add menu items to the context menu
	for ( uint32_t i = 0; i < items->Length(); ++i )
	{
		v8::Local< v8::Value > itemValue;
		if ( !items->Get( pIsolate->GetCurrentContext(), i ).ToLocal( &itemValue ) )
			break;
		if ( itemValue->IsObject() )
		{
			v8::Local< v8::Object > item = itemValue.As<v8::Object>();
			
			v8::Local< v8::Value > jsLabel = item->Get( pIsolate->GetCurrentContext(), v8::String::NewFromUtf8( pIsolate, "label" ).ToLocalChecked() ).FromMaybe( v8::Local<v8::Value>() );
			if ( jsLabel.IsEmpty() || !jsLabel->IsString() )
			{
				Warning( "UiToolkit.ShowSimpleContextMenu - item %d: expected String for label\n", i );
				continue;
			}

			v8::Local< v8::Value > jsCallback = item->Get( pIsolate->GetCurrentContext(), v8::String::NewFromUtf8( pIsolate, "jsCallback" ).ToLocalChecked() ).FromMaybe( v8::Local<v8::Value>() );
			if ( jsCallback.IsEmpty() || !jsCallback->IsFunction() )
			{
				Warning( "UiToolkit.ShowSimpleContextMenu - item %d: expected Function for jsCallback\n", i );
				continue;
			}

			v8::String::Utf8Value labelStr(v8::Isolate::GetCurrent(),  jsLabel );
			v8::Persistent< v8::Function > *pJSFunc = new v8::Persistent<v8::Function>();
			pJSFunc->Reset( pIsolate, v8::Handle<v8::Function>::Cast( jsCallback ) );

			// Create the panorama event responsible for running the javascript callback
			panorama::IUIEvent *pCallbackEvent = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSFunc );

			panorama::CPanel2D* pMenuItemPanel = pContextMenu->AddMenuItemEvent( *labelStr, pCallbackEvent );
			
			// Optional icon for this button
			v8::Local< v8::Value > jsIcon = item->Get( pIsolate->GetCurrentContext(), v8::String::NewFromUtf8( pIsolate, "icon" ).ToLocalChecked() ).FromMaybe( v8::Local<v8::Value>() );
			if ( !jsIcon.IsEmpty() && jsIcon->IsString() )
			{
				panorama::CImagePanel *pIconImage = new panorama::CImagePanel( pMenuItemPanel, NULL );
				v8::String::Utf8Value iconImageStr(v8::Isolate::GetCurrent(),  jsIcon );
				pIconImage->SetImage( *iconImageStr );
			}
			
			// Optional css style to add to this button
			v8::Local< v8::Value > jsCustomStyle = item->Get( pIsolate->GetCurrentContext(), v8::String::NewFromUtf8( pIsolate, "style" ).ToLocalChecked() ).FromMaybe( v8::Local<v8::Value>() );
			if ( !jsCustomStyle.IsEmpty() && jsCustomStyle->IsString() )
			{
				v8::String::Utf8Value customStyle(v8::Isolate::GetCurrent(),  jsCustomStyle );
				pMenuItemPanel->AddClass( *customStyle );
			}

			// Free v8::Function
			pJSFunc->Reset();
			delete pJSFunc;
		}
		else
		{
			Warning( "UiToolkit.ShowSimpleContextMenu - item %d must be a javascript object of the form obj.label / obj.jsCallback\n", i );
		}
	}

	// Set Dismiss event
	if ( pJSDismissFunc )
	{
		panorama::IUIEvent *pDismissEvent = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSDismissFunc );
		pContextMenu->SetDismissEvent( pDismissEvent );
	}

	panorama::IUIPanel *pTargetPanel = pContextPanel->FindChildTraverse( pszTargetPanelID );
	if ( pTargetPanel )
	{
		pContextMenu->SetMenuTarget( pTargetPanel );
	}

	pContextMenu->SetVisible( true );
	pContextMenu->SetFocus();

	return pContextMenu->UIPanel();
}

//-----------------------------------------------------------------------------
panorama::IUIPanel *CUiComponent_UiToolkit::ShowCustomLayoutContextMenu( const char *pszTargetPanelID, const char *pszContextMenuID, const char *pszLayoutFile )
{
	return ShowCustomLayoutContextMenuParametersDismissEvent( pszTargetPanelID, pszContextMenuID, pszLayoutFile, nullptr, nullptr );
}

//-----------------------------------------------------------------------------
panorama::IUIPanel *CUiComponent_UiToolkit::ShowCustomLayoutContextMenuParameters( const char *pszTargetPanelID, const char *pszContextMenuID, const char *pszLayoutFile, const char *pszParameters )
{
	return ShowCustomLayoutContextMenuParametersDismissEvent( pszTargetPanelID, pszContextMenuID, pszLayoutFile, pszParameters, nullptr );
}

panorama::IUIPanel *CUiComponent_UiToolkit::ShowCustomLayoutContextMenuParametersDismissEvent( const char *pszTargetPanelID, const char *pszContextMenuID, const char *pszLayoutFile, const char *pszParameters, v8::Persistent<v8::Function> *pJSDismissFunc )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	CUI_ContextMenuManager *pContextMenuManager = CUI_ContextMenuManager::GetPanelContextMenuManager( ToPanel2D( pContextPanel ) );
	if ( !pContextMenuManager )
		return NULL;

	CUI_ContextMenu_CustomLayout *pContextMenu = new CUI_ContextMenu_CustomLayout( pContextMenuManager, pszContextMenuID, ToPanel2D( pContextPanel ) );
	pContextMenu->Init( pszLayoutFile, pszParameters );
	pContextMenu->SetVisible( true );
	pContextMenu->SetFocus();

	// Set Dismiss event
	if ( pJSDismissFunc )
	{
		panorama::IUIEvent *pDismissEvent = PANORAMA_COMPONENT_EVENT_NAME( UiToolkit, RunJSFunction )::MakeEvent( nullptr, pContextPanel, pJSDismissFunc );
		pContextMenu->SetDismissEvent( pDismissEvent );
	}

	panorama::IUIPanel *pTargetPanel = pContextPanel->FindChildTraverse( pszTargetPanelID );
	if ( pTargetPanel )
	{
		pContextMenu->SetMenuTarget( pTargetPanel );
	}

	return pContextMenu->UIPanel();
}

//-----------------------------------------------------------------------------
panorama::JSGenericCallbackHandle_t CUiComponent_UiToolkit::RegisterJSCallback( v8::Persistent<v8::Function> *pJSCallbackFunc )
{
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	v8::Local<v8::Function> fnLocal = v8::Local<v8::Function>::New( panorama::UIEngine()->GetV8Isolate(), *pJSCallbackFunc );

	return panorama::UIEngine()->RegisterJSGenericCallback( pContextPanel, fnLocal );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::InvokeJSCallback( const v8::FunctionCallbackInfo< v8::Value > &callbackInfo )
{
	if ( callbackInfo.Length() < 1 )
	{
		callbackInfo.GetIsolate()->ThrowException( v8::String::NewFromUtf8( callbackInfo.GetIsolate(), "UiToolkit.InvokeJSCallback takes at least 1 argument: callback handle returned by UiToolkit.RegisterJSCallback." ).ToLocalChecked() );
		return;
	}

	// Get the callback handle
	if ( !callbackInfo[0]->IsNumber() )
	{
		callbackInfo.GetIsolate()->ThrowException( v8::String::NewFromUtf8( callbackInfo.GetIsolate(), "UiToolkit.InvokeJSCallback - The first argument must be a callback handle returned by UiToolkit.RegisterJSCallback." ).ToLocalChecked() );
		return;
	}
	panorama::JSGenericCallbackHandle_t nHandle = (int)callbackInfo[0].As<v8::Number>()->Value();

	// Arguments to use when invoking callback
	v8::HandleScope handle_scope( callbackInfo.GetIsolate() );
	CUtlVector< v8::Local< v8::Value > > args;
	for ( int i = 1; i < callbackInfo.Length(); ++i )
	{
		args.AddToTail( callbackInfo[i] );
	}

	// Invoke the callback
	v8::Local< v8::Value > retVal;
	panorama::UIEngine()->InvokeJSGenericCallback( nHandle, args.Count(), args.Base(), &retVal );
	if ( !retVal.IsEmpty() )
	{
		callbackInfo.GetReturnValue().Set( retVal );
	}
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::UnregisterJSCallback( panorama::JSGenericCallbackHandle_t nHandle )
{
	panorama::UIEngine()->UnregisterJSGenericCallback( nHandle );
}


//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::GetGlobalObject( const v8::FunctionCallbackInfo< v8::Value > &callbackInfo )
{
	callbackInfo.GetReturnValue().Set( v8::Local<v8::Object>::New( callbackInfo.GetIsolate(), m_GlobalObject ) );
}

//-----------------------------------------------------------------------------
// Callback handler responsible for creating a panel (panel type registered in javascript)

struct CreateJsPanelUserData_t
{
	CUtlString m_panelTypeName;
	CUtlString m_layoutXml;
};

static panorama::IUIPanelClient *CreateJsPanel( const char *pchID, panorama::IUIPanel *pParent, void *pUserData )
{
	if ( !pUserData )
	{
		return NULL;
	}

	CreateJsPanelUserData_t *pPanelData = (CreateJsPanelUserData_t *)pUserData;

	return new CUI_JS_Panel( ToPanel2D( pParent ), pchID, pPanelData->m_panelTypeName.Get(), pPanelData->m_layoutXml.Get() );
}

//-----------------------------------------------------------------------------
void  CUiComponent_UiToolkit::RegisterPanel2d( const char *pszPanelTypeName, const char *pszLayoutXml )
{
	DevMsg( "\tRegistering '%s' panel ('%s') with panorama.\n", pszPanelTypeName, pszLayoutXml );

	if ( panorama::UIEngine()->BRegisteredPanelType( pszPanelTypeName ) )
	{
		Warning( "UiToolkit.RegisterPanel2d - Panel type '%s' has already been registered, ignoring new registration (%s).\n", pszPanelTypeName, pszLayoutXml );
		return;
	}

	// Create factory for the new panel type

	CreateJsPanelUserData_t *pUserData = new CreateJsPanelUserData_t;
	pUserData->m_panelTypeName = pszPanelTypeName;
	pUserData->m_layoutXml = pszLayoutXml;

	panorama::CPanoramaSymbol s_dummySmbol;
	panorama::CPanel2DFactory *pFactory = new panorama::CPanel2DFactory( 
		&s_dummySmbol, 
		pszPanelTypeName,
		CreateJsPanel,
		NULL,
		&CUI_JS_Panel::GetPanelSymbol,
		pUserData );

	// Register factory with panorama

	panorama::UIEngine()->RegisterPanelFactoryWithEngine( pszPanelTypeName, pFactory );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::ProfilingScopeBegin( const char *pszTag )
{
	if ( !ThreadInMainThread() )
		return;

#ifdef RAD_TELEMETRY_ENABLED
	panorama::IUIPanel *pContextPanel = GetPanelForJavaScriptContext();
	CFmtStr strZoneName( "javascript (%s) - %s", pContextPanel->GetLayoutFile().String(), pszTag );

	TM_ENTER( TELEMETRY_LEVEL2, TMZF_NONE, "%s", TM_DYNAMIC_STRING( TELEMETRY_LEVEL2, strZoneName.Get() ) );
#endif

	m_perfTimersStack.Push();
	m_perfTimersStack.Top().Start();
}

//-----------------------------------------------------------------------------
double CUiComponent_UiToolkit::ProfilingScopeEnd()
{
	if ( !( ThreadInMainThread() && ( m_perfTimersStack.Count() > 0 ) ) )
		return 0.0;

	m_perfTimersStack.Top().End();
	double durationMS = m_perfTimersStack.Top().GetDuration().GetMillisecondsF();
	m_perfTimersStack.Pop();

#ifdef RAD_TELEMETRY_ENABLED
	TM_LEAVE( TELEMETRY_LEVEL2 );
#endif

	return durationMS;
}

//-----------------------------------------------------------------------------
uint64 CUiComponent_UiToolkit::AddDenyAllInputToGame( panorama::IUIPanel *pPanel, const char *pchDebugContextName )
{
	return gameuifuncs->PanoramaAddDenyAllInputToGame( pPanel, pchDebugContextName );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::ReleaseDenyAllInputToGame( uint64 handle )
{
	gameuifuncs->PanoramaReleaseDenyAllInputToGame( handle );
}

//-----------------------------------------------------------------------------
uint64 CUiComponent_UiToolkit::AddDenyMouseInputToGame( panorama::IUIPanel *pPanel, const char *pchDebugContextName )
{
	return gameuifuncs->PanoramaAddDenyMouseInputToGame( pPanel, pchDebugContextName );
}

//-----------------------------------------------------------------------------
void CUiComponent_UiToolkit::ReleaseDenyMouseInputToGame( uint64 handle )
{
	gameuifuncs->PanoramaReleaseDenyMouseInputToGame( handle );
}

//-----------------------------------------------------------------------------
const char *CUiComponent_UiToolkit::MakeStringSafe( const char *szString )
{
	static char szSafeStringUTF8[1024] = {0};
	szSafeStringUTF8[0] = 0;
	if ( szString )
	{
		static wchar_t wszUnicodeString[ 1024 ] = { 0 };
		wszUnicodeString[0] = 0;
		V_UTF8ToUnicode( szString, wszUnicodeString, sizeof( wszUnicodeString ) );

		static wchar_t wszSafeUnicodeString[ 1024 ] = { 0 };
		wszSafeUnicodeString[0] = 0;
		GameUI_MakeStringSafe( wszUnicodeString, wszSafeUnicodeString, sizeof( wszSafeUnicodeString ) );

		V_UnicodeToUTF8( wszSafeUnicodeString, szSafeStringUTF8, sizeof( szSafeStringUTF8 ) );
	}

	return szSafeStringUTF8;
}

//-----------------------------------------------------------------------------
bool CUiComponent_UiToolkit::IsPanoramaInECOMode()
{
	return gameuifuncs->IsPanoramaInECOMode();
}