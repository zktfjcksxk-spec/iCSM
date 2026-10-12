//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UIENGINESDL_H
#define UIENGINESDL_H

#include "iuiengine.h"
#include "uiengine.h"
#include "renderer/uirenderengine.h"
#include "renderer/iui3dsurface.h"
#include "utlstring.h"
#include "utlmap.h"
#include "utlsortvector.h"

#include <SDL.h>
#include <SDL_opengl.h>

namespace panorama
{
class CTopLevelWindow;

//
// Win32 instance of UI engine.
//
class CUIEngineSDL : public CUIEngine
{
public:

	CUIEngineSDL();
	~CUIEngineSDL();

	// Do one time initialization of the engine
	virtual bool BInitialize();

	// IUIEngine implementation
	virtual void Shutdown();
#if defined( SOURCE2_PANORAMA )
	virtual bool StartupSubsystems( IUISettings *pSettings );
#else
	virtual bool StartupSubsystems( IUISettings *pSettings, IHTMLChromeController *pHTMLController );
#endif
	virtual void RunPlatformFrame();
	virtual CTopLevelWindow *CreateNewWindow( const char *pchWindowTitle, uint32 width, uint32 height, IUIEngine::ERenderTarget eRenderType, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomCursors, const char *pchTargetWindow );
	
	virtual bool ShowNativeTopMostMessageBox( const char *pchMsg, const char *pchTitle, ENativeMessageBoxType_t eType );

	// Input locale support, stubbed out for now
	virtual ELanguage GetCurrentInputLocale();
	virtual bool BHaveInputLocale( ELanguage language );
	virtual void SetInputLocale( ELanguage language );

	// GPU Information
	virtual bool BGetGPUInformation( char *rgchGPUDesc, uint32 unGPUDescBytes, uint64 *pulDedicatedGPUMem, uint64 *pulDedicatedSystemMem, uint64 *pulSharedMem );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

protected:
	// Internal interace for clipboard access
	virtual void CopyToClipboardImpl( const char *pchTextUTF8 ) OVERRIDE;
	virtual void GetClipboardTextImpl( CUtlString &strUTF8 ) const OVERRIDE;

private:
	bool m_bInShutdown;

	SDL_Window *m_hSDLWindow;

	ELanguage m_eCurrentUILanguage;
};

} // namespace panorama

#endif // UIENGINESDL_H
