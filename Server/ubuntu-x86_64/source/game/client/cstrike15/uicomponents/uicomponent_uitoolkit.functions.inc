//
// UI component functions for popups
//
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopup,				
	"title, message, style ", "Show a popup with the given title add message and optional style. Button present: \"OK\"." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupOk,			
	"title, message, style, okJSFunc", "Show a popup with the given title add message and optional style. Button present: \"OK\"." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupCancel,		
	"title, message, style, cancelJSFunc", "Show a popup with the given title add message and optional style. Button present: \"Cancel\"." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupYesNo,			
	"title, message, style, yesJSFunc, noJSFunc", "Show a popup with the given title add message and optional style. Button present: \"Yes\"/\"No\"." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupOkCancel,		
	"title, message, style, okJSFunc, cancelJSFunc", "Show a popup with the given title add message and optional style. Button present: \"Ok\"/\"Cancel\"." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupYesNoCancel,	
	"title, message, style, yesJSFunc, noJSFunc, cancelJSFunc", "Show a popup with the given title add message and optional style. Button present: \"Yes\"/\"No\"/\"Cancel\"." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupOneOption,		
	"title, message, style, optionName, optionJSFunc", "Show a popup with the given title add message and optional style and let you specify the name of one button." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupTwoOptions,	
	"title, message, style, option1Name, option1JSFunc, option2Name, option2JSFunc", "Show a popup with the given title add message and optional style and let you specify the name of two button." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupThreeOptions,	
	"title, message, style, option1Name, option1JSFunc, option2Name, option2JSFunc, option3Name, option3JSFunc", "Show a popup with the given title add message and optional style and let you specify the name of two button." )
										 
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupBgStyle, 
	"title, message, style, bgStyle", "Show a popup with the given title add message and optional style. You can specify the background style (\"none\", \"dim\" or \"blur\"). Button present: \"OK\"." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupOkBgStyle, 
	"title, message, style, okJSFunc, bgStyle", "Show a popup with the given title add message and optional style. You can specify the background style (\"none\", \"dim\" or \"blur\"). Button present: \"OK\"." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupCancelBgStyle, 
	"title, message, style, cancelJSFunc, bgStyle", "Show a popup with the given title add message and optional style. You can specify the background style (\"none\", \"dim\" or \"blur\"). Button present: \"Cancel\"." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupYesNoBgStyle, 
	"title, message, style, yesJSFunc, noJSFunc, bgStyle", "Show a popup with the given title add message and optional style. You can specify the background style (\"none\", \"dim\" or \"blur\"). Button present: \"Yes\"/\"No\"." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupOkCancelBgStyle, 
	"title, message, style, okJSFunc, cancelJSFunc, bgStyle", "Show a popup with the given title add message and optional style. You can specify the background style (\"none\", \"dim\" or \"blur\"). Button present: \"Ok\"/\"Cancel\"." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupYesNoCancelBgStyle, 
	"title, message, style, yesJSFunc, noJSFunc, cancelJSFunc, bgStyle", "Show a popup with the given title add message and optional style. You can specify the background style (\"none\", \"dim\" or \"blur\"). Button present: \"Yes\"/\"No\"/\"Cancel\"." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupOneOptionBgStyle, 
	"title, message, style, optionName, optionJSFunc, bgStyle", "Show a popup with the given title add message and optional style and let you specify the name of one button. You can specify the background style (\"none\", \"dim\" or \"blur\"). " )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupTwoOptionsBgStyle, 
	"title, message, style, option1Name, option1JSFunc, option2Name, option2JSFunc, bgStyle", "Show a popup with the given title add message and optional style and let you specify the name of two button. You can specify the background style (\"none\", \"dim\" or \"blur\"). " )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGenericPopupThreeOptionsBgStyle, 
	"title, message, style, option1Name, option1JSFunc, option2Name, option2JSFunc, option3Name, option3JSFunc, bgStyle", "Show a popup with the given title add message and optional style and let you specify the name of two button. You can specify the background style (\"none\", \"dim\" or \"blur\"). " )
										 
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowCustomLayoutPopup, 
	"popupID, layoutFile", "Show a popup that lets you specify a layout." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowCustomLayoutPopupParameters, 
	"popupID, layoutFile, parameters", "Show a popup that lets you specify a layout and parameters." )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, CloseAllVisiblePopups, 
	"", "Force closing all visible popups" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGlobalCustomLayoutPopup, 
	"popupID, layoutFile", "Show a popup on the 'global popups top level window' that lets you specify a layout." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowGlobalCustomLayoutPopupParameters, 
	"popupID, layoutFile, parameters", "Show a popup on 'global popups top level window' that lets you specify a layout and parameters." )
										 
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, ShowTextTooltip, 
	"targetPanelID, text", "Show a tooltip with the given text" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, ShowTextTooltipOnPanel, 
	"targetPanel, text", "Show a tooltip with the given text on given panel" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, ShowTextTooltipStyled, 
	"targetPanelID, text, style", "Show a tooltip with the given text. Also apply a CSS class named \"style\" to allow custom styling." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, ShowTextTooltipOnPanelStyled, 
	"targetPanel, text, style", "Show a tooltip with the given text on given panel. Also apply a CSS class named \"style\" to allow custom styling." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, HideTextTooltip, 
	"", "Hide the text tooltip" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, ShowTitleTextTooltip, 
	"targetPanelID, title, text", "Show a tooltip with the given title and text." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, ShowTitleTextTooltipStyled, 
	"targetPanelID, title, text, style", "Show a tooltip with the given title and text. Also apply a CSS class named \"style\" to allow custom styling." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, HideTitleTextTooltip, 
	"", "Hide the title text tooltip" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, ShowTitleImageTextTooltip, 
	"targetPanelID, title, image, text", "Show a tooltip with the given title, image and text." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, ShowTitleImageTextTooltipStyled, 
	"targetPanelID, title, image, text, style", "Show a tooltip with the giben title, image and text. Also apply a CSS class named \"style\" to allow custom styling." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, HideTitleImageTextTooltip, 
	"", "Hide the title image text tooltip" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, ShowCustomLayoutParametersTooltip, 
	"targetPanelID, tooltipID, layoutFile, parameters", "Show a tooltip with a specifix id and using the given layout and parameters." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, ShowCustomLayoutParametersTooltipStyled, 
	"targetPanelID, tooltipID, layoutFile, parameters, style", "Show a tooltip with a specifix id and using the given layout and parameters. Also apply a CSS class named \"style\" (to the tooltip root panel) in order to allow custom styling (eg. \"Tooltip_NoArrow\" to remove tooltip's arrow)." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, ShowCustomLayoutTooltip, 
	"targetPanelID, tooltipID, layoutFile", "Show a tooltip with a specifix id and using the given layout." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, ShowCustomLayoutTooltipStyled, 
	"targetPanelID, tooltipID, layoutFile", "Show a tooltip with a specifix id and using the given layout. Also apply a CSS class named \"style\" (to the tooltip root panel) in order to allow custom styling (eg. \"Tooltip_NoArrow\" to remove tooltip's arrow)." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, HideCustomLayoutTooltip, 
	"tooltipID", "Hide the tooltip with the given id." )
										 
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowSimpleContextMenu, 
	"targetPanelID, contentmenuID, items", "Show a context menu with a specific id and populate the context menu item list using the given \"items\" array. Each elements of the items array is a javascript object of the form {label, jsCallback}. targetPanelID  can be the empty string in which case the cursor position is used to position the context menu. Returns context menu panel." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowSimpleContextMenuWithDismissEvent, 
	"targetPanelID, contentmenuID, items, dismissJsFunc", "Show a context menu with a specific id and populate the context menu item list using the given \"items\" array. Each elements of the items array is a javascript object of the form {label, jsCallback}. targetPanelID  can be the empty string in which case the cursor position is used to position the context menu. Returns context menu panel." )
										 
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowCustomLayoutContextMenu, 
	"targetPanelID, contentmenuID, layoutFile", "Show a context menu with a specific id and using the given layout. targetPanelID  can be the empty string in which case the cursor position is used to position the context menu. Returns context menu panel." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowCustomLayoutContextMenuParameters, 
	"targetPanelID, contentmenuID, layoutFile, parameters", "Show a context menu with a specific id and using the given layout and parameters. targetPanelID  can be the empty string in which case the cursor position is used to position the context menu. Returns context menu panel." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::IUIPanel *, ShowCustomLayoutContextMenuParametersDismissEvent, 
	"targetPanelID, contentmenuID, layoutFile, parameters, function", "Show a context menu with a specific id and using the given layout and parameters and call a function when dismissed. targetPanelID  can be the empty string in which case the cursor position is used to position the context menu. Returns context menu panel." )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( panorama::JSGenericCallbackHandle_t,	RegisterJSCallback, 
	"jsCallbackFunc", "Register a javascript callback that can be invoke at a later stage using InvokeJSCallback. Returns a callback handle." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT_RAW( void,	InvokeJSCallback, 
	"Invoke a javascript callback using a handle previously registered with RegisterJSCallback. First argument must be the callback handle followed by the callback's arguments." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void,	UnregisterJSCallback, 
	"jsCallbackHandle", "Unregister a javascript calloback previously registered with RegisterJSCallback." )

UI_COMPONENT_FUNCTIONLIST_ELEMENT_RAW( void,	GetGlobalObject, 
	"Returns a global object that can be used to store global variables you would like to share across js files." )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( void,	RegisterPanel2d, 
	"panelTypeName, layoutFile", "Register a panel type name with the corresponding layout file" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( void,	ProfilingScopeBegin, 
	"tagName", "Notify telemetry that a zone is been entered" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( double,	ProfilingScopeEnd, 
	"", "Notify telemetry that a zone is been left. Returns duration in milliseconds." )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( uint64,	AddDenyAllInputToGame, 
	"panelPtr, strDebugContextName", "Denies input to the game by filtering input events. Returns a handle used by ReleaseDenyAllInputToGame." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void,	ReleaseDenyAllInputToGame, 
	"handle", "ReleaseDenyAllInputToGame takes a handle as parameters previously returned by AddDenyAllInputToGame" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( uint64,	AddDenyMouseInputToGame, 
	"panelPtr, strDebugContextName", "Denies mouse input to the game by filtering mouse input events. Returns a handle used by ReleaseDenyMouseInputToGame." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void,	ReleaseDenyMouseInputToGame, 
	"handle", "ReleaseDenyMouseInputToGame takes a handle as parameters previously returned by AddDenyMouseInputToGame" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( string,	MakeStringSafe, 
	"str", "" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool,	IsPanoramaInECOMode, 
	"", "Is Panorama in ECO (perf) mode" )
