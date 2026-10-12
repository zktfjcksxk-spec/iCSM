//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef IGAMEUIFUNCS_H
#define IGAMEUIFUNCS_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui/KeyCode.h"
#include "inputsystem/iinputstacksystem.h"

namespace panorama
{
	class IUIPanel;

	enum EGameInputFlags {
		k_EGameInputFlagsNone = 0,

		k_EGameInputUIEnableMouseCursor = 0x1,
		k_EGameInputUIEnableControllerInput = 0x4,
		k_EGameInputUIEnableKeyInput = 0x8,

		k_EGameInputDenyGameMouseMovement = 0x10,
		k_EGameInputDenyGameMouseClicks = 0x20,
		k_EGameInputDenyGameControllerInput = 0x40,
		k_EGameInputDenyGameKeys = 0x80,

		// UI consumes all user input
		k_EGameInputCaptureAll
			= k_EGameInputUIEnableMouseCursor
			| k_EGameInputUIEnableControllerInput
			| k_EGameInputUIEnableKeyInput
			| k_EGameInputDenyGameMouseMovement
			| k_EGameInputDenyGameMouseClicks
			| k_EGameInputDenyGameControllerInput
			| k_EGameInputDenyGameKeys,

		// UI has mouse control, but un-handled clicks
		// and other input are passed through to the game
		k_EGameInputShareMouse
			= k_EGameInputUIEnableMouseCursor
			| k_EGameInputDenyGameMouseMovement,

		// UI has mouse control, other input is passed through to the game
		// This means mouse bindings will not reach the game, you probably never want this for an in-game panel
		k_EGameInputCaptureMouse
			= k_EGameInputShareMouse
			| k_EGameInputDenyGameMouseClicks,

		// UI consumes all keyboard events
		// but mouse control remains in-game (e.g. text chat)
		k_EGameInputCaptureKeyboard
			= k_EGameInputUIEnableKeyInput
			| k_EGameInputDenyGameKeys,
	};

	DEFINE_ENUM_BITWISE_OPERATORS( EGameInputFlags );
}

abstract_class IGameUIFuncs
{
public:
	virtual bool		IsKeyDown( const char *keyname, bool& isdown ) = 0;
	virtual const char	*GetBindingForButtonCode( ButtonCode_t code ) = 0;
	virtual ButtonCode_t GetButtonCodeForBind( const char *pBind, int userId = -1 ) = 0;
	virtual void		GetVideoModes( struct vmode_s **liststart, int *count ) = 0;
	virtual void		SetFriendsID( uint friendsID, const char *friendsName ) = 0;
	virtual void		GetDesktopResolution( int &width, int &height ) = 0;
	virtual bool		IsConnectedToVACSecureServer() = 0;

// #ifdef PANORAMA_ENABLE_
	virtual void *AddPanoramaView( const char *pchViewName, void *pWindow ) = 0;
	virtual void RemovePanoramaView( void *pWindow ) = 0;
	virtual void PanoramaRunFrame(int nSlot) = 0;
	virtual void PanoramaRenderFrame(int nSlot) = 0;
	virtual InputContextHandle_t GetPanoramaInputContext() = 0;
	virtual uint64 PanoramaAddDenyAllInputToGame( panorama::IUIPanel *pPanel, const char *pchDebugContextName ) = 0;
	virtual void PanoramaReleaseDenyAllInputToGame( uint64 handle ) = 0;
	virtual bool PanoramaDeniesInputToGame() = 0;
	virtual uint64 PanoramaAddGameInputHandler( panorama::IUIPanel *pPanel, panorama::EGameInputFlags inputFlags, const char *pchDebugContextName ) = 0;
	virtual void PanoramaReleaseGameInputHandler( uint64 handle ) = 0;
	virtual bool PanoramaDeniesInputToGame( panorama::EGameInputFlags eFlags ) = 0;
	virtual uint64 PanoramaAddDenyMouseInputToGame( panorama::IUIPanel *pPanel, const char *pchDebugContextName ) = 0;
	virtual void PanoramaReleaseDenyMouseInputToGame( uint64 handle ) = 0;
	virtual bool IsPanoramaInECOMode() = 0;
	// #endif
};

#define VENGINE_GAMEUIFUNCS_VERSION "VENGINE_GAMEUIFUNCS_VERSION005"

#endif // IGAMEUIFUNCS_H
