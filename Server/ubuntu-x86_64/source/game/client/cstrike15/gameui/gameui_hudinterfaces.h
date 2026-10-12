//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Interfaces to common HUD elements shared between SF and Panorama
//
//=============================================================================//

#ifndef GAMEUI_HUDINTERFACES_H
#define GAMEUI_HUDINTERFACES_H

class CHudElement;

class ISharedHudElement
{
public:
	virtual CHudElement* AsHudElement() = 0;
};

class IHudChat : public ISharedHudElement
{
public:
	virtual void ClearHistory() = 0;
	virtual bool ChatRaised() = 0;
	virtual void StartMessageMode( int iMessageMode ) = 0;
};
IHudChat* GetHudChat();

#endif // GAMEUI_HUDINTERFACES_H