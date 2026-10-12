//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"

using namespace panorama;


// general uievents
DEFINE_PANORAMA_EVENT_DOC( AddStyle, "class", "Add a CSS class to a panel." );
DEFINE_PANORAMA_EVENT_DOC( RemoveStyle, "class", "Remove a CSS class from a panel." );
DEFINE_PANORAMA_EVENT_DOC( ToggleStyle, "class", "Toggle whether a panel has the given CSS class." );
DEFINE_PANORAMA_EVENT_DOC( SwitchStyle, "slot, class", "Switch which class the panel has for a given attribute slot. Allows easily changing between multiple states." );
DEFINE_PANORAMA_EVENT_DOC( TriggerStyle, "class", "Remove then immediately add back a CSS class from a panel. Useful to re-trigger events like animations or sound effects." );
DEFINE_PANORAMA_EVENT_DOC( AddStyleToEachChild, "class", "Add a CSS class to all children of this panel." );
DEFINE_PANORAMA_EVENT_DOC( RemoveStyleFromEachChild, "class", "Remove a CSS class from all children of this panel." );
DEFINE_PANORAMA_EVENT( PanelLoaded );
DEFINE_PANORAMA_EVENT( CheckChildrenScrolledIntoView );
DEFINE_PANORAMA_EVENT( Scroll );
DEFINE_PANORAMA_EVENT( ScrollPanelIntoView );
DEFINE_PANORAMA_EVENT( ScrolledIntoView );
DEFINE_PANORAMA_EVENT( ScrolledOutOfView );
DEFINE_PANORAMA_EVENT( LoadLayoutFileAsync );
DEFINE_PANORAMA_EVENT( AppendChildrenFromLayoutFileAsync );
DEFINE_PANORAMA_EVENT( LoadLayoutFromXMLStringAsync );
DEFINE_PANORAMA_EVENT( LoadLayoutFromBase64XMLStringAsync );
DEFINE_PANORAMA_EVENT( Activated );
DEFINE_PANORAMA_EVENT( Cancelled );
DEFINE_PANORAMA_EVENT( ContextMenu );
DEFINE_PANORAMA_EVENT( LocalizationChanged );
DEFINE_PANORAMA_EVENT( InputFocusSet );
DEFINE_PANORAMA_EVENT( InputFocusLost );
DEFINE_PANORAMA_EVENT( InputFocusTopLevelChanged );
DEFINE_PANORAMA_EVENT_DOC( SetInputFocus, "", "Set focus to this panel." );
DEFINE_PANORAMA_EVENT_DOC( DropInputFocus, "", "Drop focus entirely from the window containing this panel." );
DEFINE_PANORAMA_EVENT( ShowTooltip );
DEFINE_PANORAMA_EVENT( StyleFlagsChanged );
DEFINE_PANORAMA_EVENT( StyleClassesChanged );
DEFINE_PANORAMA_EVENT( PanelStyleChanged );
DEFINE_PANORAMA_EVENT( AnimationStart );
DEFINE_PANORAMA_EVENT( AnimationEnd );
DEFINE_PANORAMA_EVENT( PropertyTransitionEnd );
DEFINE_PANORAMA_EVENT( CopyStringToClipboard );
DEFINE_PANORAMA_EVENT( SetAllChildrenActivationEnabled );
DEFINE_PANORAMA_EVENT_DOC( AsyncEvent, "delay, eventToFire", "Fire another event after a delay (in seconds)." );
DEFINE_PANORAMA_EVENT( SetPanelEvent );
DEFINE_PANORAMA_EVENT( ClearPanelEvent );
DEFINE_PANORAMA_EVENT( DispatchPanelEvent );
DEFINE_PANORAMA_EVENT_DOC( IfHasClassEvent, "class, eventToFire", "Fire another event if this panel has a given class." );
DEFINE_PANORAMA_EVENT_DOC( IfNotHasClassEvent, "class, eventToFire", "Fire another event if this panel does not have a given class." );
DEFINE_PANORAMA_EVENT_DOC( IfHoverOtherEvent, "otherPanelID, eventToFire", "Fire another event if currently hovering over a panel with the given ID." );
DEFINE_PANORAMA_EVENT_DOC( IfNotHoverOtherEvent, "otherPanelID, eventToFire", "Fire another event if not currently hovering over a panel with the given ID." );
DEFINE_PANORAMA_EVENT_DOC( ScrollToTop, "", "Scroll this panel to the top." );
DEFINE_PANORAMA_EVENT_DOC( ScrollToBottom, "", "Scroll this panel to the bottom." );
DEFINE_PANORAMA_EVENT( LoadAsyncComplete );
DEFINE_PANORAMA_EVENT_DOC( SetPanelSelected, "selected", "Set whether this panel is :selected." );
DEFINE_PANORAMA_EVENT( ResetToDefaultValue );
DEFINE_PANORAMA_EVENT_DOC( TogglePanelSelected, "", "Toggle whether this panel is :selected." );
DEFINE_PANORAMA_EVENT_DOC( SetChildPanelsSelected, "selected", "Set whether any child panels are :selected." );
DEFINE_PANORAMA_EVENT_DOC( ScrollPanelLeft, "", "Scroll the panel left by one line." );
DEFINE_PANORAMA_EVENT_DOC( ScrollPanelRight, "", "Scroll the panel right by one line." );
DEFINE_PANORAMA_EVENT_DOC( ScrollPanelUp, "", "Scroll the panel up by one line." );
DEFINE_PANORAMA_EVENT_DOC( ScrollPanelDown, "", "Scroll the panel down by one line." );
DEFINE_PANORAMA_EVENT_DOC( PagePanelLeft, "", "Scroll the panel left by one page." );
DEFINE_PANORAMA_EVENT_DOC( PagePanelRight, "", "Scroll the panel left by one page." );
DEFINE_PANORAMA_EVENT_DOC( PagePanelUp, "", "Scroll the panel up by one page." );
DEFINE_PANORAMA_EVENT_DOC( PagePanelDown, "", "Scroll the panel down by one page." );
DEFINE_PANORAMA_EVENT_DOC( MovePanelLeft, "repeatCount", "Move left from the panel. By default, this will change the focus position, but other panel types may implement this differently." );
DEFINE_PANORAMA_EVENT_DOC( MovePanelRight, "repeatCount", "Move right from the panel. By default, this will change the focus position, but other panel types may implement this differently." );
DEFINE_PANORAMA_EVENT_DOC( MovePanelUp, "repeatCount", "Move up from the panel. By default, this will change the focus position, but other panel types may implement this differently." );
DEFINE_PANORAMA_EVENT_DOC( MovePanelDown, "repeatCount", "Move down from the panel. By default, this will change the focus position, but other panel types may implement this differently." );
DEFINE_PANORAMA_EVENT_DOC( SetPanelEnabled, "enabled", "Sets whether the given panel is enabled" );
DEFINE_PANORAMA_EVENT( DropdownMenuFocusChanged );

// Only dispatched to panels that have SetUnloadWhenInvisible() set
DEFINE_PANORAMA_EVENT( ReadyForDisplay );
DEFINE_PANORAMA_EVENT( UnreadyForDisplay );

// window events
DEFINE_PANORAMA_EVENT( WindowGotFocus );
DEFINE_PANORAMA_EVENT( WindowLostFocus );
DEFINE_PANORAMA_EVENT( WindowCursorShown );
DEFINE_PANORAMA_EVENT( WindowCursorHidden );
DEFINE_PANORAMA_EVENT( WindowShown );
DEFINE_PANORAMA_EVENT( WindowHidden );
DEFINE_PANORAMA_EVENT( WindowOffScreen );	// fired when window cannot be seen or interacted with (eg, virtual desktop switch)
DEFINE_PANORAMA_EVENT( WindowOnScreen );		// fired when window can be seen again

// global events
DEFINE_PANORAMA_EVENT( MinimizeApp );
DEFINE_PANORAMA_EVENT( QuitApp );
DEFINE_PANORAMA_EVENT( ExitSteam );
DEFINE_PANORAMA_EVENT( ShutdownMachine );
DEFINE_PANORAMA_EVENT( RestartMachine );
DEFINE_PANORAMA_EVENT( SuspendMachine );
DEFINE_PANORAMA_EVENT( TurnOffActiveController );
DEFINE_PANORAMA_EVENT( GoOffline );
DEFINE_PANORAMA_EVENT( GoOnline );
DEFINE_PANORAMA_EVENT( ShowQuitDialog );
DEFINE_PANORAMA_EVENT( ChangeUser );

#if DEVELOPMENT_ONLY
DEFINE_PANORAMA_EVENT( ToggleDebugger );
DEFINE_PANORAMA_EVENT( ShowPanelZoo );
DEFINE_PANORAMA_EVENT( DumpMemory );
DEFINE_PANORAMA_EVENT( ProfileOn );
DEFINE_PANORAMA_EVENT( ProfileOff );
DEFINE_PANORAMA_EVENT( Refresh );
#endif

DEFINE_PANORAMA_EVENT( ToggleConsole );


DEFINE_PANORAMA_EVENT( MoveUp );
DEFINE_PANORAMA_EVENT( MoveDown );
DEFINE_PANORAMA_EVENT( MoveLeft );
DEFINE_PANORAMA_EVENT( MoveRight );
DEFINE_PANORAMA_EVENT_DOC( ScrollUp, "", "Scroll the panel up by one line." );
DEFINE_PANORAMA_EVENT_DOC( ScrollDown, "", "Scroll the panel down by one line." );
DEFINE_PANORAMA_EVENT_DOC( ScrollLeft, "", "Scroll the panel left by one line." );
DEFINE_PANORAMA_EVENT_DOC( ScrollRight, "", "Scroll the panel right by one line." );
DEFINE_PANORAMA_EVENT_DOC( PageUp, "", "Scroll the panel up by one page." );
DEFINE_PANORAMA_EVENT_DOC( PageDown, "", "Scroll the panel down by one page." );
DEFINE_PANORAMA_EVENT_DOC( PageLeft, "", "Scroll the panel left by one page." );
DEFINE_PANORAMA_EVENT_DOC( PageRight, "", "Scroll the panel right by one page." );
DEFINE_PANORAMA_EVENT( TabForward );
DEFINE_PANORAMA_EVENT( TabBackward );
DEFINE_PANORAMA_EVENT( GamepadInserted );
DEFINE_PANORAMA_EVENT( GamepadRemoved );
DEFINE_PANORAMA_EVENT( ReloadStyleFile );
DEFINE_PANORAMA_EVENT( TopLevelWindowClose );		// fired when top level window is destructing while all children are still valid
DEFINE_PANORAMA_EVENT( TopLevelWindowVisibilityChanged );
DEFINE_PANORAMA_EVENT( GamepadInput );
DEFINE_PANORAMA_EVENT( ImageLoaded );
DEFINE_PANORAMA_EVENT( ImageFailedLoad );
DEFINE_PANORAMA_EVENT( DeletePanel );
DEFINE_PANORAMA_EVENT( ActivateMainWindow );
DEFINE_PANORAMA_EVENT( ToggleFullscreen );
DEFINE_PANORAMA_EVENT( GuideButton );
DEFINE_PANORAMA_EVENT( GuideButtonUp );
DEFINE_PANORAMA_EVENT( PadBackButton );
DEFINE_PANORAMA_EVENT( PadBackButtonUp );
DEFINE_PANORAMA_EVENT( SteampadGuideButton );
DEFINE_PANORAMA_EVENT( SteampadGuideButtonUp );
DEFINE_PANORAMA_EVENT( StartButton );
DEFINE_PANORAMA_EVENT( StartButtonUp );
DEFINE_PANORAMA_EVENT( OverlayGamepadInputMsg );
DEFINE_PANORAMA_EVENT( None );						// short circuited in bind handling not to fire an event
DEFINE_PANORAMA_EVENT( ExecuteSteamURL );
DEFINE_PANORAMA_EVENT( UserInputActive ); 
DEFINE_PANORAMA_EVENT( AsyncPanoramaQuitWithError );
DEFINE_PANORAMA_EVENT( BrowserGoToURL );
DEFINE_PANORAMA_EVENT( GameControllerMappingChanged );
DEFINE_PANORAMA_EVENT( StopStreaming );
DEFINE_PANORAMA_EVENT( StopStreamingAndSuspendClient );
DEFINE_PANORAMA_EVENT( CloseModalDialog );
DEFINE_PANORAMA_EVENT( AsyncPanoramaSurfaceLost );
DEFINE_PANORAMA_EVENT( AsyncPanoramaSurfaceReturned );
DEFINE_PANORAMA_EVENT( SoundVolumeChanged );
DEFINE_PANORAMA_EVENT( SoundMuteChanged );
DEFINE_PANORAMA_EVENT( ActiveControllerTypeChanged );
DEFINE_PANORAMA_EVENT( SystemInputLanguageChanged );

namespace panorama
{

void OnActiveControllerTypeChangedDefaultHandler( IUIPanel *pPanel, EActiveControllerType eActiveControllerType )
{
	static const CPanoramaSymbol k_symXInputPad( "XInputPad" );
	static const CPanoramaSymbol k_symSteamPad( "SteamPad" );
	static const CPanoramaSymbol k_symSteamVR( "SteamVRController" );

	pPanel->RemoveClasses( "XInputPad SteamPad SteamVRController" );
	switch ( eActiveControllerType )
	{
	case k_EActiveControllerType_XInput:
		pPanel->AddClass( k_symXInputPad );
		break;

	case k_EActiveControllerType_Steam:
		pPanel->AddClass( k_symSteamPad );
		break;

	case k_EActiveControllerType_VR:
		pPanel->AddClass( k_symSteamVR );
		break;

	default:
		break;
	}
}

}

DEFINE_PANORAMA_EVENT( MediaVolumeMute );
DEFINE_PANORAMA_EVENT( MediaVolumeDown );
DEFINE_PANORAMA_EVENT( MediaVolumeUp );
DEFINE_PANORAMA_EVENT( MediaNextTrack );
DEFINE_PANORAMA_EVENT( MediaPrevTrack );
DEFINE_PANORAMA_EVENT( MediaStop );
DEFINE_PANORAMA_EVENT( MediaPlayPause );

DEFINE_PANORAMA_EVENT( JSConsoleOutput );

DEFINE_PANORAMA_EVENT( InMemoryFileUpdate );
DEFINE_PANORAMA_EVENT( InMemoryFilesSaved );

DEFINE_PANORAMA_EVENT( VideoPlayerInitalized );
DEFINE_PANORAMA_EVENT( VideoPlayerRepeated );
DEFINE_PANORAMA_EVENT( VideoPlayerEnded );
DEFINE_PANORAMA_EVENT( VideoPlayerPlaybackStateChange );
DEFINE_PANORAMA_EVENT( VideoPlayerChangedRepresentation );

DEFINE_PANORAMA_EVENT( PlaySoundEffect );
DEFINE_PANORAMA_EVENT( PlayMainMenuMusic );
DEFINE_PANORAMA_EVENT( SoundFinished );

DEFINE_PANORAMA_EVENT( PollingForSteamClientUpdate );
DEFINE_PANORAMA_EVENT( SettingsPanelShown );
DEFINE_PANORAMA_EVENT( DummyWizardOpen );

// debugger events
DEFINE_PANORAMA_EVENT( CreateDebuggerWindow );
DEFINE_PANORAMA_EVENT( CloseDebuggerWindow );
DEFINE_PANORAMA_EVENT( BeginDebuggerInspect );

DEFINE_PANORAMA_EVENT( JSONWebAPIResponse );

// panel drag events
DEFINE_PANORAMA_EVENT( DragStart );
DEFINE_PANORAMA_EVENT( DragEnter );
DEFINE_PANORAMA_EVENT( DragDrop );
DEFINE_PANORAMA_EVENT( DragLeave );
DEFINE_PANORAMA_EVENT( DragEnd );

// drag scroll events
DEFINE_PANORAMA_EVENT( DragScrollStart );
DEFINE_PANORAMA_EVENT( DragScrollMouseMove );
DEFINE_PANORAMA_EVENT( DragScrollEnd );

DEFINE_PANORAMA_EVENT( TextInputHandlerStateChange );
DEFINE_PANORAMA_EVENT( TextInputFinished );
