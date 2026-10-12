//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uienginesdl.h"
#include "uitoplevelwindowsdl.h"
#include "uitoplevelwindowoverlay.h"
#include "text/uitextlayoutposix.h"
#include "text/uitextservicespango.h"
#ifdef OSX
#include <Carbon/Carbon.h>
#include "../common/osxhelpers.h"
#endif
#ifdef WIN32
#include "uienginewin32.h" // for BGetGPUInformationShared impl
#endif

#ifdef LINUX
// GPU defines to sniff video ram
#ifndef GL_NVX_gpu_memory_info
#define GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX 0x9047
#define GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX 0x9048
#define GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX 0x9049
#define GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX 0x904A
#define GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX 0x904B
#endif
#ifndef GL_ATI_meminfo
#define GL_ATI_meminfo
#define GL_VBO_FREE_MEMORY_ATI                            0x87FB
#define GL_TEXTURE_FREE_MEMORY_ATI                        0x87FC
#define GL_RENDERBUFFER_FREE_MEMORY_ATI                   0x87FD
/* Undocumented in the spec */
#define GL_TOTAL_PHYSICAL_MEMORY_ATI                      0x87FE
#endif
#endif


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

namespace panorama
{


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUIEngineSDL::CUIEngineSDL() : 
	m_bInShutdown( false ),
		m_hSDLWindow( NULL )
{
	m_eCurrentUILanguage = k_Lang_English;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUIEngineSDL::~CUIEngineSDL()
{
	while( m_vecWindows.Count() )
	{
		// Deleting will end up calling back into us to remove from vector.
		delete m_vecWindows[m_vecWindows.Count()-1];
	}

	// May or may not have already shutdown
	Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: Do one time initialization of the engine
//-----------------------------------------------------------------------------
bool CUIEngineSDL::BInitialize()
{
#if defined(OGL) || defined(POSIX)
    g_IUITextServices = new CUITextServicesPango();
#endif
    
    // forestw: on Linux we seem to be doing our X11 init in StartupSubsystems instead
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Init the subsystems we use
//-----------------------------------------------------------------------------
#if defined( SOURCE2_PANORAMA )
bool CUIEngineSDL::StartupSubsystems( IUISettings *pSettings )
#else
bool CUIEngineSDL::StartupSubsystems( IUISettings *pSettings, IHTMLChromeController *pHTMLController )
#endif
{
	// This is cached at SDL_Init time, so we need to set it before
	// initializing with SDL_INIT_JOYSTICK.
	SDL_SetHint( SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1" );

	//SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
	//SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );

	// SDL needs to be initialized
	SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);

#if 0
	m_mapFontTextureCache.Insert( 1, new CFontTextureCache( this ) );
	m_flLastChromeFrameTime = 0.0;
	m_bPendingReconfigure = false;
#endif
	
	if ( pSettings )
		m_eCurrentUILanguage = PchLanguageToELanguage( pSettings->GetUILanguage(), k_Lang_English );

#if defined( SOURCE2_PANORAMA )
	return CUIEngine::StartupSubsystems( pSettings );
#else
	return CUIEngine::StartupSubsystems( pSettings, pHTMLController );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Create a new top level window
//-----------------------------------------------------------------------------
CTopLevelWindow *CUIEngineSDL::CreateNewWindow( const char *pchWindowTitle, uint32 width, uint32 height, ERenderTarget eRenderType, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, const char *pchTargetWindow )
{
	CTopLevelWindow *pWindow;
	
	if ( IUIEngine::BIsOverlayTarget( eRenderType ) )
	{
#if !defined( SOURCE2_PANORAMA  )
		CTopLevelWindowOverlay *pWindowImpl = new CTopLevelWindowOverlay( this );
		if ( !pWindowImpl->BInitializeSurface( pchWindowTitle, width, height, eRenderType, bFixedSurfaceSize, bEnforceWindowAspectRatio, bUseCustomMouseCursor, pchTargetWindow ) )
		{
			delete pWindowImpl;
			return NULL;
		}
		pWindow = pWindowImpl;
#else
		AssertMsg( false, "No Overlay windows allowed in source2" );
		return NULL;
#endif
	}
	else
	{
#if !defined( SOURCE2_PANORAMA )
		CTopLevelWindowSDL * pWindowImpl = new CTopLevelWindowSDL( this );
		if ( !pWindowImpl->BInitializeSurface( pchWindowTitle, width, height, eRenderType, bFixedSurfaceSize, bEnforceWindowAspectRatio, bUseCustomMouseCursor, pchTargetWindow ) )
		{
			delete pWindowImpl;
			return NULL;
		}
#else
		CTopLevelWindowSource2 * pWindowImpl = new CTopLevelWindowSource2( this );
		if ( !pWindowImpl->BInitializeSurface( pchWindowTitle, width, height, eRenderType, bFixedSurfaceSize, bEnforceWindowAspectRatio, bUseCustomMouseCursor, pchTargetWindow ) )
		{
			delete pWindowImpl;
			return NULL;
		}
#endif

		pWindow = pWindowImpl;
	}

	if( !pWindow )
		return NULL;

	if ( !pWindow->FinishInitialization() )
	{
		delete pWindow;
		return NULL;
	}

	m_vecWindows.AddToTail( pWindow );
	return pWindow;
}

//-----------------------------------------------------------------------------
// Purpose: Run a frame, pumps input, paints, whatever.
//-----------------------------------------------------------------------------
void CUIEngineSDL::RunPlatformFrame()
{
	// Run Win32 input/message loop
	FOR_EACH_VEC( m_vecWindows, i )
	{
		m_vecWindows[i]->RunPlatformFrame();
	}

	CTopLevelWindowSDL::PumpMessageLoop();

	CUIEngine::RunPlatformFrame();
}


//-----------------------------------------------------------------------------
// Purpose: Shutdown the UI engine including the surface and window
//-----------------------------------------------------------------------------
void CUIEngineSDL::Shutdown()
{
	if ( m_bInShutdown )
		return;
	m_bInShutdown = true;

	SDL_Event ev;
	SDL_PumpEvents();
	while ( SDL_PollEvent( &ev ) > 0 ) { }

	CUIEngine::Shutdown();
	m_bInShutdown = false;
}


//-----------------------------------------------------------------------------
// Purpose: Shows a native message box.. usually for development
//-----------------------------------------------------------------------------
bool CUIEngineSDL::ShowNativeTopMostMessageBox( const char *pchMsg, const char *pchTitle, ENativeMessageBoxType_t eType )
{
#if defined(LINUX) || defined(WIN32)
	if ( m_pSettings && !m_pSettings->BAllowOSModalDialog() )
	{
		// never show an xwin dialog on steamos boxes as we have no way to present them using our custom compositor, just log to stdout and keep moving
		printf( "%s : %s \n", pchTitle, pchMsg );
		return true;
	}

	if ( eType == k_ENativeMessageOk )
	{
		return SDL_ShowSimpleMessageBox( 0, pchTitle, pchMsg, m_hSDLWindow ) == 0;
	}

	if ( eType == k_ENativeMessageYesNo )
	{
		SDL_MessageBoxButtonData buttons[ 2 ];
		buttons[ 0 ].buttonid = 0;
		buttons[ 0 ].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
		buttons[ 0 ].text = "Yes";

		buttons[ 1 ].buttonid = 1;
		buttons[ 1 ].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		buttons[ 1 ].text = "No";

		SDL_MessageBoxData data;
		memset( &data, 0, sizeof( data ) );
		data.flags = SDL_MESSAGEBOX_WARNING;
		data.window = m_hSDLWindow;
		data.title = pchTitle;
		data.message = pchMsg;
		data.numbuttons = V_ARRAYSIZE( buttons );
		data.buttons = buttons;

		int buttonid;
		if ( SDL_ShowMessageBox( &data, &buttonid ) != 0 )
			return false;
		return buttonid == 0;
	}

	AssertMsg( false, "Implement new ENativeMessageBoxType_t!" );
#elif defined(OSX)
	CFOptionFlags responseFlags;
	CFStringRef message, title, accept, cancel;
	message = CFStringCreateWithCString(NULL, pchMsg, CFStringGetSystemEncoding() ) ;
	title = CFStringCreateWithCString(NULL, pchTitle, CFStringGetSystemEncoding() ) ;

	if ( eType == k_ENativeMessageOk )
		CFUserNotificationDisplayAlert( 0, kCFUserNotificationCautionAlertLevel, 0, 0, 0, title, message, NULL, NULL, NULL, &responseFlags );
	else if ( eType == k_ENativeMessageYesNo )
	{
		accept = CFStringCreateWithCString(NULL, "Yes", CFStringGetSystemEncoding() ) ;
		cancel = CFStringCreateWithCString(NULL, "No", CFStringGetSystemEncoding() ) ;
		CFUserNotificationDisplayAlert( 0, kCFUserNotificationCautionAlertLevel, 0, 0, 0, title, message, accept, cancel, NULL, &responseFlags );
		CFRelease(accept);
		CFRelease(cancel);
		
	}
	
	CFRelease(message);
	CFRelease(title);
	
	return ( responseFlags == kCFUserNotificationDefaultResponse );
#else
#error
#endif
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Copies text to the system clipboard
//-----------------------------------------------------------------------------
void CUIEngineSDL::CopyToClipboardImpl( const char *pchTextUTF8 )
{
	SDL_SetClipboardText( pchTextUTF8 );
}


//-----------------------------------------------------------------------------
// Purpose: Gets clipboard text as UTF8
//-----------------------------------------------------------------------------
void CUIEngineSDL::GetClipboardTextImpl( CUtlString &strUTF8 ) const
{
	char *clip = SDL_GetClipboardText();
	if (clip)
	{
		strUTF8 = clip;
		SDL_free(clip);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get information about the users GPU
//-----------------------------------------------------------------------------
bool CUIEngineSDL::BGetGPUInformation( char *rgchGPUDesc, uint32 unGPUDescBytes, uint64 *pulDedicatedGPUMem, uint64 *pulDedicatedSystemMem, uint64 *pulSharedMem )
{
	if ( rgchGPUDesc && unGPUDescBytes > 0 )
		rgchGPUDesc[0] = 0;

	if ( pulDedicatedGPUMem )
		*pulDedicatedGPUMem = 0;

	if ( pulDedicatedSystemMem )
		*pulDedicatedSystemMem = 0;

	if ( pulSharedMem )
		*pulSharedMem = 0;

#ifdef LINUX
	bool bRet = false;
	if ( pulDedicatedGPUMem )
	{
		// If we have a GLX context already we can just use it
		Display *oldhGLXDisplay = glXGetCurrentDisplay();
		GLXDrawable oldhGLXDrawable = glXGetCurrentDrawable();
		GLXContext oldhGLXContext = glXGetCurrentContext();
		Display *dpy = NULL;
		Window win = 0;
		GLXContext ctx = NULL;
		if ( !oldhGLXContext )
		{
			dpy = XOpenDisplay( NULL );
			int scrnum = DefaultScreen( dpy );

			int attribSingle[] = { GLX_RGBA, GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1,  GLX_BLUE_SIZE, 1, None };
			int attribDouble[] = {   GLX_RGBA, GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1, GLX_DOUBLEBUFFER, None };
		
			XSetWindowAttributes attr;
			unsigned long mask;
			Window root;
			XVisualInfo *visinfo;
			int width = 100, height = 100;
		
			root = RootWindow(dpy, scrnum);
			visinfo = glXChooseVisual(dpy, scrnum, attribSingle);
			if (!visinfo) 
			{
				visinfo = glXChooseVisual(dpy, scrnum, attribDouble);
				if (!visinfo) 
				{
					Msg( "Error: couldn't find RGB GLX visual\n" );
					return false;
				}
			}
		
			attr.background_pixel = 0;
			attr.border_pixel = 0;
			attr.colormap = XCreateColormap(dpy, root, visinfo->visual, AllocNone);
			attr.event_mask = StructureNotifyMask | ExposureMask;
			mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
			win = XCreateWindow(dpy, root, 0, 0, width, height, 0, visinfo->depth, InputOutput, visinfo->visual, mask, &attr);
		
			ctx = glXCreateContext( dpy, visinfo, NULL, true );
			if (!ctx) 
			{
				Msg( "Error: glXCreateContext failed\n" );
				XDestroyWindow(dpy, win);
				return false;
			}

			Assert( dpy != NULL );
			Assert( win != 0 );
			Assert( ctx != NULL );
			glXMakeCurrent(dpy, win, ctx);
		}
		if ( glXGetCurrentContext() )
		{
			const char *pchVendor = (char *)glGetString(GL_VENDOR);
			V_strncpy( rgchGPUDesc, pchVendor, unGPUDescBytes );
			
			const char *pchExt = (char *)glGetString(GL_EXTENSIONS);

			if ( pchExt && V_strstr( pchExt, "GL_NVX_gpu_memory_info" ) ) 
			{
				GLint kbytes = 0;
				glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &kbytes);
				*pulDedicatedGPUMem = 1024*(uint64)kbytes;
				bRet = true;
			}

			if ( pchExt && V_strstr( pchExt, "GL_ATI_meminfo" ) ) 
			{
				//GLint kbytes = 0;
				// This API call doesn't actually work on any AMD machines I can get my hands on
				// so just make a number up....
				//glGetIntegerv( GL_TOTAL_PHYSICAL_MEMORY_ATI, &kbytes);
				*pulDedicatedGPUMem = 256*1024*1024;
				bRet = true;
			}

			// return to the old context
			if ( oldhGLXDisplay )
				glXMakeCurrent( oldhGLXDisplay, oldhGLXDrawable, oldhGLXContext );
		}

		if ( ctx )
			glXDestroyContext( dpy, ctx );
		if ( win )
			XDestroyWindow( dpy, win );
		if ( dpy )
			XCloseDisplay( dpy );
	}
	
	//AssertMsg( false, "Implement BGetGPUInformation" );
	if ( !bRet && pulDedicatedGPUMem )
		*pulDedicatedGPUMem = 256 * 1024 * 1024; // just assume 256mb
	return bRet;

#elif defined(OSX)
	return OSXHelpers::BGetGPUInformation( rgchGPUDesc, unGPUDescBytes, pulDedicatedGPUMem, pulDedicatedSystemMem, pulSharedMem );
#elif defined(WIN32)
	return CUIEngineWin32::BGetGPUInformationShared( rgchGPUDesc, unGPUDescBytes, pulDedicatedGPUMem, pulDedicatedSystemMem, pulSharedMem );
#else
#error
return false;
#endif
}
	

//-----------------------------------------------------------------------------
// Purpose: return the locale we are currently on
//-----------------------------------------------------------------------------
ELanguage CUIEngineSDL::GetCurrentInputLocale() 
{ 
	return m_eCurrentUILanguage;
}
	

//-----------------------------------------------------------------------------
// Purpose: Return true if input in the given language is supported
//-----------------------------------------------------------------------------	
bool CUIEngineSDL::BHaveInputLocale( ELanguage language ) 
{ 
	// Win32 restricts input locales to those matching available keyboard layouts.
	// Linux and OSX don't do this.
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: change to this locale
//-----------------------------------------------------------------------------
void CUIEngineSDL::SetInputLocale( ELanguage language )
{
	// Win32 sets keyboard layout to match the input locale as well.
	// Linux and OSX don't do this.
	m_eCurrentUILanguage = language;
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CUIEngineSDL::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	CUIEngine::Validate( validator, pchName );
	CTopLevelWindowSDL::ValidateStatics( validator, "CTopLevelWindowSDL::ValidateStatics" );
}
#endif

} // namespace panorama
