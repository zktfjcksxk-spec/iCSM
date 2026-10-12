//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
//=============================================================================

#ifndef PANORAMAENGINEHANDLER_H
#define PANORAMAENGINEHANDLER_H
#pragma once

#include "tier0/platwindow.h"
#include "inputsystem/InputEnums.h"
#include "panorama/panorama.h"
#include "panorama/iuiengine.h"
#include "panorama/source2/ipanoramaui.h"
#include "panorama/iuiwindow.h"
#include "tier1/utlpriorityqueue.h"
#include "igame.h"
#include "IGameUIFuncs.h"

//-----------------------------------------------------------------------------
// Panorama Engine code
//-----------------------------------------------------------------------------
class CPanoramaEngineHandler
{
	void RunFrame();
public:

	CPanoramaEngineHandler();

	bool IsPanoramaEnabled() const { return m_bValid; }

	virtual InitReturnVal_t Init();
	void Shutdown();

	bool ProcessUserInput( const InputEvent_t &ie );
	void ChangeResolution( const int nWindowWidth, const int nWindowHeight );

	panorama::IUIPanelClient *AddPanoramaView( const char *pchViewName, panorama::IUIWindow *pWindow );
	void RemovePanoramaView( panorama::IUIWindow *pWindow );

	void PanoramaRunFrame(int nSlot);
	void PanoramaRenderFrame(int nSlot);

	bool IsInECOMode() const;

	panorama::IUIEngine *UIEngine() { return m_pUIEngine; }

#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY ) 
	bool IsDebuggerShown() { return m_bShowDebugger; };
	void SetDebuggerShown( bool bShow ) { m_bShowDebugger = bShow; }
	void ToggleDebugger();

	bool OnCloseDebuggerWindow();

	void SaveDebuggerDimentions();

	void CopyDebuggerRtBits( uint32* pBitsOut, uint32 nWd, uint32 nHt )
	{
		m_pDebugWindow->CopyDebuggerRtBits( pBitsOut, m_nDebuggerW, m_nDebuggerH);
	}
	int m_nDebuggerX;
	int m_nDebuggerY;
	int m_nDebuggerW;
	int m_nDebuggerH;
	void OnDebuggerResize( uint32 nNewWidth, uint32 nNewHeight );
#else

	bool IsDebuggerShown() { return false; }

#endif

	// Denies input to the game by filtering input events
	// Note that only mouse and key events will be filtered. It is important not to 
	// filter events such as IE_AppActivated and let the game handle such events
	// (eg. to correctly show/hide cursor)
	// AddGameInputHandler will return a non zero handle. AddGameInputHandler takes a
	// panel as a parameters. If the panel is valid, panorama will only filter input events
	// if the panel and its top level window are visible (checked every frame in RunFrame)
	// (Note that we are only checking the visibility of the panel and its top level window 
	// but not all ancestors of the  given panel, therefore it is still possible for the 
	// panel to not be visible on the screen but still deny input to the game). 
	// If pPanel is NULL, Panorama will always filter input events until the corresponding 
	// ReleaseGameInputHandler is being called.
	// ReleaseGameInputHandler takes a handle previously returned by AddGameInputHandler.
	uint64 AddGameInputHandler( panorama::IUIPanel *pPanel, panorama::EGameInputFlags eFlags, const char *pchDebugContextName );
	void ReleaseGameInputHandler( uint64 handle );
	bool DeniesInputToGame( panorama::EGameInputFlags eFlags ) const { return ( m_eGameInputFlags & eFlags ) == eFlags; }
	void DumpDenyAllInputToGame() const;
	
	void SetIMEAllowed( bool bAllowed );

	InputContextHandle_t GetInputContext() { return m_hPanoramaInputContext; }

#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY )
	IPanoramaClientDebugger *m_pDebugger;
#endif

private:

	bool OnActivateMainWindow();
	bool OnWindowShutdown( panorama::IUIWindow *pWindow );
	void RecalculateInputOrder();

#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY )
	bool OnCreateDebuggerWindow();
	bool OnBeginDebuggerInspect();
	PlatWindow_t CreateAppWindow( const char *pTitle, int nPlatWindowFlags, int x, int y, int w, int h );
#endif

	void OnProfileOnEvent();
	void OnProfileOffEvent();


private:

	bool m_bValid;

#if ( PLATFORM_WINDOWS && DEVELOPMENT_ONLY )
	bool m_bShowDebugger;
	panorama::IUIWindow *m_pDebugWindow;
	PlatWindow_t m_hDebuggerWindow;
	uint64 m_hDebuggerDenyInputToGame = 0;
#endif

    panorama::IUIEngine *m_pUIEngine;
	
	struct ViewEntry_t
	{
		ViewEntry_t() {}
		CUtlString m_sViewName;
		panorama::IUIWindow *m_pWindow;
	};
	static bool ViewPriorityOrder( ViewEntry_t const &lhs, ViewEntry_t const &rhs, void *pCtx );
	CUtlVector<ViewEntry_t> m_Views; // kept sorted in ascending priority order
	CUtlVector<CUtlString> m_vecViewsToRemove;
	CUtlVector<panorama::IUIWindow *> m_vecWindowInputOrder;
	CUtlVector<panorama::IUIWindow*> m_pWindows;

	int m_nMainWindowWidth;
	int m_nMainWindowHeight;

	InputContextHandle_t m_hPanoramaInputContext;
	uint64 m_nNextInputHandle;
	struct DenyInputEntry_t
	{
		uint64 m_handle;
		panorama::PanelHandle_t m_panelHandle;
		CUtlString m_debugName;
		panorama::EGameInputFlags m_eGameInputFlags;
	};
	CUtlVector< DenyInputEntry_t > m_denyAllInputEntries;
	// Flag used in CPanoramaEngineHandler::ProcessUserInput to filter input events
	// This flag is being recomputed every frame in CPanoramaEngineHandler::RunFrame
	// Flag set to true if a panel from m_denyAllInputEntries is visible
	panorama::EGameInputFlags m_eGameInputFlags;

	bool m_bInECOMode;
};

CPanoramaEngineHandler &PanoramaEngineHandler();

#endif // PANORAMAENGINEHANDLER_H
