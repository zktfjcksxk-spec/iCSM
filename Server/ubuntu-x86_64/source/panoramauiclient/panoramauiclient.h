//====== Copyright © 2014-2015, Valve Corporation, All rights reserved. =======
//
// Purpose: IPanoramaUIClient app system implementation
//
//=============================================================================

#ifndef PANORAMAUICLIENT_H
#define PANORAMAUICLIENT_H
#pragma once

#include "appframework/iappsystem.h"
#include "tier3/tier3.h"
#include "panorama/panorama.h"
#include "panorama/iuiengine.h"
#include "panorama/source2/ipanoramaui.h"

class CPanoramaUIClient : public CTier3AppSystem<IPanoramaUIClient>
{
	typedef CTier3AppSystem<IPanoramaUIClient> BaseClass;

public:
    CPanoramaUIClient();
    
	//-------------------------------------------------------------------------
	// IAppSystem implementation
	//-------------------------------------------------------------------------

	// 7ls src1
	virtual bool Connect( CreateInterfaceFn factory ) OVERRIDE;

	virtual const AppSystemInfo_t *GetDependencies() OVERRIDE;
	virtual void Disconnect() OVERRIDE;
	virtual void *QueryInterface( const char *pInterfaceName ) OVERRIDE;
	virtual void Shutdown() OVERRIDE;

	//-------------------------------------------------------------------------
	// IPanoramaUIClient implementation
	//-------------------------------------------------------------------------

    virtual panorama::IUIEngine *SetupUIEngine( const char *pszLanguage, PlatWindow_t hWindow ) OVERRIDE;
	virtual void ShutdownUIEngine() OVERRIDE;

	virtual bool HandleInputEvent( const InputEvent_t &event, const CUtlVector<panorama::IUIWindow *> &vecWindowInputOrder, bool bOnlyIfFocused ) OVERRIDE;

    virtual panorama::IUIPanelClient *CreatePanel2D( panorama::IUIWindow *pParent, const char *pID ) OVERRIDE;
    virtual IPanoramaClientDebugger *CreateDebugger( panorama::IUIWindow *pParent, const char *pID ) OVERRIDE;

private:
	bool SetupNamedPaths();
};

#endif // PANORAMAUICLIENT_H
