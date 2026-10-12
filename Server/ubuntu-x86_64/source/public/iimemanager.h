//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef IIMEMANAGER_H
#define IIMEMANAGER_H
#pragma once

#include "appframework/iappsystem.h"
#include "tier0/platwindow.h"
#include "imesystem/imeuiinterface.h"

abstract_class IIMEManager : public IAppSystem
{
public:
	virtual bool Setup( PlatWindow_t hWindow ) = 0;
	virtual bool IsValid() = 0;

	virtual void SetIMEEnabled( bool bEnabled ) = 0;
	virtual bool IsIMEEnabled() = 0;

	// Input Events
	virtual bool HandleIMEEvent( PlatWindow_t hWindow, uint32 uMsg, uintp wParam, uintp lParam ) = 0;
	virtual bool HandlePreProcessKeyboardEvent( PlatWindow_t hWindow, uint32 uMsg, uintp wParam, uintp lParam ) = 0;
	virtual void HandleMouseDownEvent( IIMEUIView *pUIView, IIMEUIObject *pObjectUnderMouse ) = 0;
	virtual void HandleFocusChange( IIMEUIObject *pObject, bool bFocusSet ) = 0;

	// Track focus gain/loss events
	virtual void SetActiveUIView( IIMEUIView *pUIView, bool bActive ) = 0;
	virtual IIMEUIView *GetActiveUIView() = 0;

	virtual LoggingChannelID_t GetLoggingChannel() = 0;
};

#endif
