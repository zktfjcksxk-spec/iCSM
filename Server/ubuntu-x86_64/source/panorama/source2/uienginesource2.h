//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UIENGINESOURCE2_H
#define UIENGINESOURCE2_H
#pragma once

#include "iuiengine.h"
#include "uiengine.h"
#include "renderer/uirenderengine.h"
#include "renderer/iui3dsurface.h"
#include "utlstring.h"
#include "utlmap.h"
#include "utlsortvector.h"
#include "inputsystem/InputEnums.h"
#include "resourcesystem/iresourcesystem.h"
#include "rendersystem/irenderdevice.h"

class CPanoramaTypeManager;
struct ResourceMonitor_t;
struct DirectoryWatch_t;
struct FileWatch_t;

namespace panorama
{
class CTopLevelWindow;
class CUIRenderDeviceSource2;

//
// Win32 instance of UI engine.
//
class CUIEngineSource2 : public CUIEngine, public IToolsResourceListener, public IRenderDeviceEventListener
{
public:
	CUIEngineSource2();
	~CUIEngineSource2();

	// Do one time initialization of the engine
	virtual bool BInitialize();

	// IUIEngine implementation
	virtual void Shutdown();
	virtual bool StartupSubsystems( IUISettings *pSettings, PlatWindow_t hWindow );
	virtual void RunPlatformFrame();
	
	virtual IUIWindow *CreateNewUILayerWindow( uint32 xPos, uint32 yPos, uint32 width, uint32 height, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, bool bAcceptKBandMouse, const char *pName, InputContextHandle_t hInputContext ) OVERRIDE;
	virtual IUIWindow *CreateNewOffscreenUIWindow( uint32 width, uint32 height, const char *pName, InputContextHandle_t hInputContext, bool bDrawToBackBuffer ) OVERRIDE;
	virtual bool DestroyWindow( IUIWindow *pWindow ) OVERRIDE;
	virtual void OnResolutionChange( float fRelativeScalefactor );
	virtual void OnGPUMemLevelChanged();

	virtual bool ShowNativeTopMostMessageBox( const char *pchMsg, const char *pchTitle, ENativeMessageBoxType_t eType );

	// Input locale support, stubbed out for now
	virtual ELanguage GetCurrentInputLocale();
	virtual bool BHaveInputLocale( ELanguage language );
	virtual void SetInputLocale( ELanguage language );

	// GPU Information
	virtual bool BGetGPUInformation( char *rgchGPUDesc, uint32 unGPUDescBytes, uint64 *pulDedicatedGPUMem, uint64 *pulDedicatedSystemMem, uint64 *pulSharedMem );

	virtual IUISoundSystem *CreateSoundSystem() OVERRIDE;
	virtual void GetWindowsForDebugger( CUtlVector<IUIWindow *> &vecWindows ) OVERRIDE;

	virtual void OnFileCacheRemoved( CPanoramaSymbol fileSymbol ) OVERRIDE;

	virtual void ReloadChangedFile( const char *pchFile ) OVERRIDE;
	
#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

	CUIRenderDeviceSource2 *GetRenderDevice() { return m_pRenderDevice; }
	CImageResourceManager *GetImageResourceManager() { return m_pImageResourceManager; }
	virtual IUIImageManager *UIImageManager() OVERRIDE { return GetImageResourceManager(); }

	// Methods of IToolsResourceListener
public:
	virtual void NotifyResourceStatusChange( ResourceId_t nResourceId, ResourceHandle_t hResource, ResourceType_t nResourceType, ResourceLoadType_t nLoadType ) OVERRIDE;

	// Methods of IRenderDeviceEventListener
public:
	virtual void OnDeviceLost();
	virtual void OnDeviceRestored();
	virtual void OnDeviceCreated();
	virtual void OnModeChanged( const RenderDeviceInfo_t &mode );

public:
	bool MonitorResourceForChanges( ResourceHandle_t hResource );
	void StopMonitoringResourceForChanges( ResourceHandle_t hResource );
	void ReloadImageResourceFile( const char *pResourceFile );

	bool MonitorFileForChanges( const char *pFilename, FileChangeCallback_t fileChangeCallback );

	bool GetOriginalImageDimensions( const char *pResourceFile, uint32 *pOriginalWidth, uint32 *pOriginalHeight );

	// 7ls src1 CON_COMMAND_MEMBER_F( CUIEngineSource2, "panorama_spew_texture_info", SpewTextureInfo_f, "Spew Texture info.", FCVAR_DONTRECORD );

protected:
	// Internal interace for clipboard access
	virtual void CopyToClipboardImpl( const char *pchTextUTF8 ) OVERRIDE;
	virtual void GetClipboardTextImpl( CUtlString &strUTF8 ) const OVERRIDE;

private:
	void UpdateFileMonitoring();
	void ServiceQueuedReloads();

	bool m_bInShutdown;
	bool m_bDeviceRestore;
	
	CPanoramaTypeManager	*m_pPanoramaLayoutTypeManager;
	CPanoramaTypeManager	*m_pPanoramaStyleTypeManager;
	CPanoramaTypeManager	*m_pPanoramaDynamicImagesTypeManager;
	CPanoramaTypeManager	*m_pPanoramaScriptTypeManager;

	CUIRenderDeviceSource2 *m_pRenderDevice;
	CImageResourceManager *m_pImageResourceManager;

	CUtlMap< ResourceHandle_t, ResourceMonitor_t*, int, CDefLess< ResourceHandle_t > > m_MonitoredResources;

	CUtlVector< DirectoryWatch_t* > m_MonitoredDirectories;
	CUtlDict< FileWatch_t*, int > m_MonitoredFiles;

	HResourceManifest m_hRequiredManifest;

	CUtlVector< ResourceMonitor_t * > m_QueuedResourceReloads;
};

} // namespace panorama

#endif // UIENGINESOURCE2_H
