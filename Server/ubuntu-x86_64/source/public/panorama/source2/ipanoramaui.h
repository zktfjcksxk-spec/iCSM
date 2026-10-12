//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#if !defined( __IPANORAMAUI_H__ )
#define __IPANORAMAUI_H__
#pragma once

#include "appframework/iappsystem.h"
#include "tier3/tier3.h"
#include "inputsystem/InputEnums.h"
#include "inputsystem/ButtonCode.h"
#include "tier1/refcount.h"
//#include "../game/client/iwordfilter.h"
#include "vscript/ivscript.h"
#include "rendersystem/irenderdevice.h"
#include "panorama/input/mousecodes.h"
#include "shaderapi/IShaderDevice.h"


class IDirect3DDevice9;

namespace panorama
{
class IUIEngine;
class IUIWindow;
class IUIPanelClient;
}

// Manages the communication between panorama_client-level code and
// the Panorama core in panorama.dll.
class IPanoramaUIEngine : public IAppSystem
{
public:
	virtual bool SetupUIEngine() = 0;
	virtual void ShutdownUIEngine() = 0;

	// Access UI engine, generally use global panorama::UIEngine() accessor instead as shorthand
	virtual panorama::IUIEngine * AccessUIEngine() = 0;

	virtual bool HandleInputEvent( const InputEvent_t &event, const CUtlVector<panorama::IUIWindow *> &vecWindowInputOrder, bool bOnlyIfFocused ) = 0;

	virtual void SetIMEAllowed( bool bAllowed ) = 0;

	virtual int DeleteClosedWindows() = 0;
	
	virtual IDirect3DDevice9* GetD3Device() = 0;
	virtual void SetD3Device( IDirect3DDevice9* pDev ) = 0;

	virtual void AddDeviceDependentObject( IShaderDeviceDependentObject *pObject ) = 0;
	virtual void RemoveDeviceDependentObject( IShaderDeviceDependentObject *pObject ) = 0;
};


//
// Panorama class wrappers to make methods virtual for cross-DLL calling.
//

class IPanoramaClientDebugger
{
public:
    virtual ~IPanoramaClientDebugger() {}

	virtual void BeginInspect() = 0;
	virtual void ForceEndInspect() = 0;
	virtual float GetSplitterPosition() = 0;
	virtual void SetSplitterPosition( float flParentFlowValue ) = 0;
};
    
// Manages the communication between non-Panorama code and
// a component using panorama_client.
class IPanoramaUIClient : public IAppSystem
{
public:
    virtual panorama::IUIEngine *SetupUIEngine( const char *pszLanguage, PlatWindow_t hWindow ) = 0;
	virtual void ShutdownUIEngine() = 0;

	virtual bool HandleInputEvent( const InputEvent_t &event, const CUtlVector<panorama::IUIWindow *> &vecWindowInputOrder, bool bOnlyIfFocused ) = 0;

    virtual panorama::IUIPanelClient *CreatePanel2D( panorama::IUIWindow *pParent, const char *pID ) = 0;
    virtual IPanoramaClientDebugger *CreateDebugger( panorama::IUIWindow *pParent, const char *pID ) = 0;
};

inline ButtonCode_t PanoramaMouseToSource2ButtonCode( panorama::MouseCode eMouseCode )
{
	switch ( eMouseCode )
	{
		case panorama::MOUSE_LEFT:		return ::MOUSE_LEFT;
		case panorama::MOUSE_MIDDLE:	return ::MOUSE_MIDDLE;
		case panorama::MOUSE_RIGHT:		return ::MOUSE_RIGHT;
		case panorama::MOUSE_4:			return ::MOUSE_4;
		case panorama::MOUSE_5:			return ::MOUSE_5;

		default:
			Assert( !"Invalid panorama mouse code" );
			return ::BUTTON_CODE_INVALID;
	}
}

#endif
