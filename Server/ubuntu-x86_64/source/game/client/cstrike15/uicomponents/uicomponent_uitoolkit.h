//========= Copyright (C) 1996-2016, Valve Corporation, All rights reserved. ============//
//
// Purpose: Helper to show popups, context menu from Panorama Javascript.
//
// $NoKeywords: $
//=============================================================================//

#ifndef UICOMPONENT_UITOOLKIT_H
#define UICOMPONENT_UITOOLKIT_H
#ifdef _WIN32
#pragma once
#endif

#include "uicomponent_common.h"
#include "panorama/popups/ui_popup_generic.h"

#include "tier1/utlstack.h"
#include "tier0/fasttimer.h"


namespace panorama
{
	class CPanel2D;
}


//
// Component
//
class CUiComponent_UiToolkit : public CUiComponentGlobalInstanceHelper< CUiComponent_UiToolkit >
{
	UI_COMPONENT_DECLARE_GLOBAL_INSTANCE_ONLY( CUiComponent_UiToolkit );

public:

	// Generic popup with up to 3 buttons
	// (using the default background style set in file://{resources}/layout/popups/popup_generic.xml

	panorama::IUIPanel *ShowGenericPopup( const char *pszTitle, const char *pszMessage, const char *pszStyle );
	panorama::IUIPanel *ShowGenericPopupOk( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSOkFunc );
	panorama::IUIPanel *ShowGenericPopupCancel( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSCancelFunc );
	panorama::IUIPanel *ShowGenericPopupYesNo( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSYesFunc, v8::Persistent<v8::Function> *pJSNoFunc );
	panorama::IUIPanel *ShowGenericPopupOkCancel( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSOKFunc, v8::Persistent<v8::Function> *pJSCancelFunc );
	panorama::IUIPanel *ShowGenericPopupYesNoCancel( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSYesFunc, v8::Persistent<v8::Function> *pJSNoFunc, v8::Persistent<v8::Function> *pJSCancelFunc );
	panorama::IUIPanel *ShowGenericPopupOneOption( const char *pszTitle, const char *pszMessage, const char *pszStyle, const char *pszOption1, v8::Persistent<v8::Function> *pJSFunc1 );
	panorama::IUIPanel *ShowGenericPopupTwoOptions( const char *pszTitle, const char *pszMessage, const char *pszStyle, const char *pszOption1, v8::Persistent<v8::Function> *pJSFunc1, const char *pszOption2, v8::Persistent<v8::Function> *pJSFunc2 );
	panorama::IUIPanel *ShowGenericPopupThreeOptions( const char *pszTitle, const char *pszMessage, const char *pszStyle, const char *pszOption1, v8::Persistent<v8::Function> *pJSFunc1, const char *pszOption2, v8::Persistent<v8::Function> *pJSFunc2, const char *pszOption3, v8::Persistent<v8::Function> *pJSFunc3 );
	
	// Generic popup letting you specify background style.
	// Valid background style are "none", "dim" or "blur"

	panorama::IUIPanel *ShowGenericPopupBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, const char *pszBackgroundStyle );
	panorama::IUIPanel *ShowGenericPopupOkBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSOkFunc, const char *pszBackgroundStyle );
	panorama::IUIPanel *ShowGenericPopupCancelBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSCancelFunc, const char *pszBackgroundStyle );
	panorama::IUIPanel *ShowGenericPopupYesNoBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSYesFunc, v8::Persistent<v8::Function> *pJSNoFunc, const char *pszBackgroundStyle );
	panorama::IUIPanel *ShowGenericPopupOkCancelBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSOKFunc, v8::Persistent<v8::Function> *pJSCancelFunc, const char *pszBackgroundStyle );
	panorama::IUIPanel *ShowGenericPopupYesNoCancelBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, v8::Persistent<v8::Function> *pJSYesFunc, v8::Persistent<v8::Function> *pJSNoFunc, v8::Persistent<v8::Function> *pJSCancelFunc, const char *pszBackgroundStyle );
	panorama::IUIPanel *ShowGenericPopupOneOptionBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, const char *pszOption1, v8::Persistent<v8::Function> *pJSFunc1, const char *pszBackgroundStyle );
	panorama::IUIPanel *ShowGenericPopupTwoOptionsBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, const char *pszOption1, v8::Persistent<v8::Function> *pJSFunc1, const char *pszOption2, v8::Persistent<v8::Function> *pJSFunc2, const char *pszBackgroundStyle );
	panorama::IUIPanel *ShowGenericPopupThreeOptionsBgStyle( const char *pszTitle, const char *pszMessage, const char *pszStyle, const char *pszOption1, v8::Persistent<v8::Function> *pJSFunc1, const char *pszOption2, v8::Persistent<v8::Function> *pJSFunc2, const char *pszOption3, v8::Persistent<v8::Function> *pJSFunc3, const char *pszBackgroundStyle );


	// Custom layout popups

	panorama::IUIPanel *ShowCustomLayoutPopup( const char *pszPopupID, const char *pszLayoutFile );
	panorama::IUIPanel *ShowCustomLayoutPopupParameters( const char *pszPopupID, const char *pszLayoutFile, const char *pszParameters );

	panorama::IUIPanel *ShowGlobalCustomLayoutPopup( const char *pszPopupID, const char *pszLayoutFile );
	panorama::IUIPanel *ShowGlobalCustomLayoutPopupParameters( const char *pszPopupID, const char *pszLayoutFile, const char *pszParameters );

	void CloseAllVisiblePopups();


	// Tooltips
	// Convenience functions that will just dispatched the corresponding panorama event
	// such as UIShowTextTooltip, UIShowTitleTextTooltip ...
	// That way popups, tooltips and context menus are all exposed to javascript via the UiToolkit component

	// A tooltip with just text
	void ShowTextTooltip( const char *pszTargetPanelID, const char *pszText );
	void ShowTextTooltipOnPanel( panorama::CPanel2D *pTargetPanel, const char *pszText );
	void ShowTextTooltipStyled( const char *pszTargetPanelID, const char *pszText, const char *pszClass );
	void ShowTextTooltipOnPanelStyled( panorama::CPanel2D *pTargetPanel, const char *pszText, const char *pszClass );
	void HideTextTooltip();

	// A tooltip with a title and text
	void ShowTitleTextTooltip( const char *pszTargetPanelID, const char *pszTitle, const char *pszText );
	void ShowTitleTextTooltipStyled( const char *pszTargetPanelID, const char *pszTitle, const char *pszText, const char *pszClass );
	void HideTitleTextTooltip();

	// A tooltip with a title, image, and text
	void ShowTitleImageTextTooltip( const char *pszTargetPanelID, const char *pszTitle, const char *pszImagePath, const char *pszText );
	void ShowTitleImageTextTooltipStyled( const char *pszTargetPanelID, const char *pszTitle, const char *pszImagePath, const char *pszText, const char *pszClass );
	void HideTitleImageTextTooltip();

	// A tooltip with a custom XML layout
	void ShowCustomLayoutParametersTooltip( const char *pszTargetPanelID, const char *pszTooltipID, const char *pszLayoutXml, const char *pszParameters );
	void ShowCustomLayoutParametersTooltipStyled( const char *pszTargetPanelID, const char *pszTooltipID, const char *pszLayoutXml, const char *pszParameters, const char *pszClass );
	void ShowCustomLayoutTooltip( const char *pszTargetPanelID, const char *pszTooltipID, const char *pszLayoutXml );
	void ShowCustomLayoutTooltipStyled( const char *pszTargetPanelID, const char *pszTooltipID, const char *pszLayoutXml, const char *pszClass );
	void HideCustomLayoutTooltip( const char *pszTooltipID );


	// Simple context menu

	// each element of the items array is an javascript object obj where:
	//	obj.label		: const char*
	//	obj.jsCallback	: v8::function
	panorama::IUIPanel *ShowSimpleContextMenu( const char *pszTargetPanelID, const char *pszContextMenuId, v8::Local<v8::Array> items);
	panorama::IUIPanel *ShowSimpleContextMenuWithDismissEvent( const char *pszTargetPanelID, const char *pszContextMenuId, v8::Local<v8::Array> items, v8::Persistent<v8::Function> *pJSDismissFunc);


	// Custom layout context menu

	panorama::IUIPanel *ShowCustomLayoutContextMenu( const char *pszTargetPanelID, const char *pszContextMenuID, const char *pszLayoutFile );
	panorama::IUIPanel *ShowCustomLayoutContextMenuParameters( const char *pszTargetPanelID, const char *pszContextMenuID, const char *pszLayoutFile, const char *pszParameters );
	panorama::IUIPanel *ShowCustomLayoutContextMenuParametersDismissEvent( const char *pszTargetPanelID, const char *pszContextMenuID, const char *pszLayoutFile, const char *pszParameters, v8::Persistent<v8::Function> *pJSDismissFunc );


	// Callback registration

	// Register a javascript callback, returning a callback handle
	panorama::JSGenericCallbackHandle_t RegisterJSCallback( v8::Persistent<v8::Function> *pJSCallbackFunc );
	// Invoke a javascript callback by handle previously registered with the system
	// First argument must be the callback handle, followed by the callback's arguments
	void InvokeJSCallback( const v8::FunctionCallbackInfo< v8::Value > &callbackInfo );
	// Unregister a callback from the system (future invoke on the handle will do nothing
	void UnregisterJSCallback( panorama::JSGenericCallbackHandle_t nHandle );


	// Global object used to store global variables that can be shared across js files

	void GetGlobalObject(const v8::FunctionCallbackInfo< v8::Value > &callbackInfo);

	// Panel registration
	// Register a panel with panorama from javascript

	void RegisterPanel2d( const char *pszPanelTypeName, const char *pszLayoutXml );

	// Profiling - telemetry

	void ProfilingScopeBegin( const char *pszTag );
	double ProfilingScopeEnd();

	// Denies input to game by filtering input events

	uint64 AddDenyAllInputToGame( panorama::IUIPanel *pPanel, const char *pchDebugContextName );
	void ReleaseDenyAllInputToGame( uint64 handle );
	uint64 AddDenyMouseInputToGame( panorama::IUIPanel *pPanel, const char *pchDebugContextName );
	void ReleaseDenyMouseInputToGame( uint64 handle );

	// Utility functions

	const char *MakeStringSafe( const char *szString );

	bool IsPanoramaInECOMode();

private:

	panorama::IUIPanel * ShowGenericPopupInternal( const char *pszTitle, const char *pszMessage, const char *pszStyle, const CUtlVector< PopupChoiceParamEvent_t > &vecOptions, const char *pszBackgroundStyle, panorama::IUIEvent *pCancelEvent = nullptr );

	bool OnRunJSFunction( panorama::CPanelPtr<panorama::IUIPanel> panelContext, v8::Persistent<v8::Function> *pJSFunc );


	v8::Persistent< v8::Object > m_GlobalObject;

	CUtlStack< CFastTimer > m_perfTimersStack;
};

#endif	// UICOMPONENT_UITOOLKIT_H