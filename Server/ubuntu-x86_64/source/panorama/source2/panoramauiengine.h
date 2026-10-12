//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#if !defined( __PANORAMAUIENGINE_H__ )
#define __PANORAMAUIENGINE_H__
#pragma once

#include "panorama/source2/ipanoramaui.h" 
#include "panorama/iuiengine.h"
#include "appframework/iappsystem.h"
#include "tier3/tier3.h"

#include "refcount.h"
#include "shaderapi/ishaderapi.h"

#include "uienginesource2.h"

#include "renderer/source2surface.h"

#if ( defined( WIN32 ) && !defined( DX_TO_GL_ABSTRACTION ) ) || defined( _X360 )
#include <d3d9.h>
#endif

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

class CPanoramaUIEngine;

class DeviceCallbacks : public IShaderDeviceDependentObject
{
public:
	int m_iRefCount;
	CPanoramaUIEngine* m_pPanoramaUIEngine;

	DeviceCallbacks( void ) : m_iRefCount( 1 ), m_pPanoramaUIEngine( NULL )
	{
	}

	virtual void DeviceLost( void );

	virtual void DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd );

	virtual void ScreenSizeChanged( int width, int height );
};



//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
class CPanoramaUIEngine : public CTier3AppSystem < IPanoramaUIEngine >
{
	typedef CTier3AppSystem< IPanoramaUIEngine > BaseClass;

public:
	CPanoramaUIEngine();

	//-------------------------------------------------------------------------
	// IAppSystem implementation
	//-------------------------------------------------------------------------

	virtual const AppSystemInfo_t *GetDependencies() OVERRIDE;
	virtual bool Connect( CreateInterfaceFn factory ) OVERRIDE;
	virtual void Disconnect() OVERRIDE;
	virtual void *QueryInterface( const char *pInterfaceName ) OVERRIDE;
	virtual InitReturnVal_t Init() OVERRIDE;
	virtual void Shutdown() OVERRIDE;

	//-------------------------------------------------------------------------
	// IPanoramaUIEngine implementation
	//-------------------------------------------------------------------------

	virtual bool SetupUIEngine() OVERRIDE;
	virtual void ShutdownUIEngine() OVERRIDE;

	// Access UI engine, generally use global panorama::UIEngine() accessor instead as shorthand
	virtual panorama::IUIEngine * AccessUIEngine() OVERRIDE;

	virtual bool HandleInputEvent( const InputEvent_t &event, const CUtlVector<panorama::IUIWindow *> &vecWindowInputOrder, bool bOnlyIfFocused ) OVERRIDE;

	virtual void SetIMEAllowed( bool bAllowed ) OVERRIDE;

	virtual int DeleteClosedWindows() OVERRIDE;

	virtual void AddDeviceDependentObject( IShaderDeviceDependentObject *pObject ) { if ( m_pShaderDeviceMgr ) m_pShaderDeviceMgr->AddDeviceDependentObject( pObject ); }
	virtual void RemoveDeviceDependentObject( IShaderDeviceDependentObject *pObject ) { if ( m_pShaderDeviceMgr ) m_pShaderDeviceMgr->RemoveDeviceDependentObject( pObject ); }


	IDirect3DDevice9* GetD3Device() { return m_pD3DDevice; }
	void SetD3Device( IDirect3DDevice9* pDev ) { m_pD3DDevice = pDev; }

	panorama::IUIEngine *m_pUIEngine;

	IDirect3DDevice9* m_pD3DDevice;

	DeviceCallbacks* m_pDeviceCallbacks;
	IShaderDeviceMgr* m_pShaderDeviceMgr;

};

extern CPanoramaUIEngine *g_pPanoramaUIEngineImpl;

#if PANDX_DRAW
extern bool g_bPanDx;
extern void PanDxDeviceLost();
extern void PanDxDeviceReset();
extern void PanDxInit();
#endif

inline void DeviceCallbacks::DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd )
{
	m_pPanoramaUIEngine->SetD3Device( (IDirect3DDevice9*)pDevice );

	if ( m_pPanoramaUIEngine->m_pUIEngine )
		( (panorama::CUIEngineSource2*)(m_pPanoramaUIEngine->m_pUIEngine) )->OnDeviceRestored();
#if PANDX_DRAW
	PanDxDeviceReset();
#endif
}


inline void DeviceCallbacks::DeviceLost( void )
{
	if ( m_pPanoramaUIEngine->m_pUIEngine )
		( ( panorama::CUIEngineSource2* )( m_pPanoramaUIEngine->m_pUIEngine ) )->OnDeviceLost();

#if PANDX_DRAW
	PanDxDeviceLost();
#endif
}

inline void DeviceCallbacks::ScreenSizeChanged( int width, int height )
{
//	if ( m_pPanoramaUIEngine->m_pUIEngine )
//	{
//		( ( panorama::CUIEngineSource2* )( m_pPanoramaUIEngine->m_pUIEngine ) )->OnDeviceLost();
//		( ( panorama::CUIEngineSource2* )( m_pPanoramaUIEngine->m_pUIEngine ) )->OnDeviceRestored();
//	}

#if PANDX_DRAW
//	PanDxDeviceReset();
//	PanDxInit();
#endif
}


#endif
