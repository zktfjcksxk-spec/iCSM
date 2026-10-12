//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uitoplevelwindowoverlay.h"
#ifdef WIN32
#include "uitoplevelwindowwin32.h"
#include "renderer/d3d10d2dsurface.h"
#elif defined(POSIX)
#include "renderer/sdlopenglsurface.h"
#endif

#define GAMEOVERLAYUI_EXPORTS

using namespace panorama;

static CCommandLineParam s_ClientTenfoot720p( "-720p", "Run tenfoot in 720p rather than 1080p" );

//-----------------------------------------------------------------------------
// Purpose: Class is a 'headless' top level window supporting render targets
//			that are just a surface.  More or less like CTopLevelWindowWin32
//			management without the actual hwnd final target.
//-----------------------------------------------------------------------------
CTopLevelWindowOverlay::CTopLevelWindowOverlay( CUIEngine *pUIEngineParent )
: CTopLevelWindow( pUIEngineParent )
{
	m_pOverlayInterface = new COverlayInterface( this );
	m_p3DSurface = NULL;
	m_pRenderEngine = NULL;
	m_bMouseOverWindow = false;
	m_unSurfaceWidth = 1920;
	m_unSurfaceHeight = 1080;
	m_unWindowWidth = 1920;
	m_unWindowHeight = 1080;
	m_nAppId = 0;
	m_bVisibleThisFrame = false;
	m_bVisibleLastFrame = false;
	m_bFullScreen = false;
	m_bFixedSurfaceSize = false;
	m_bUseCustomMouseCursor = true;
	m_bVisible = true;

	m_unInitialWidth = 0;
	m_unInitialHeight = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Cleanup
//-----------------------------------------------------------------------------
CTopLevelWindowOverlay::~CTopLevelWindowOverlay()
{
	CTopLevelWindowOverlay::Shutdown();
	SAFE_DELETE( m_pOverlayInterface );
}


//-----------------------------------------------------------------------------
// Purpose: Access overlay window interface for this window, NULL on non Steam Overlay windows
//-----------------------------------------------------------------------------
IUIOverlayWindow *CTopLevelWindowOverlay::GetOverlayInterface()
{
	return m_pOverlayInterface;
}

//-----------------------------------------------------------------------------
// Purpose: Post constructor startup
//-----------------------------------------------------------------------------
bool CTopLevelWindowOverlay::BInitializeSurface( const char *pchWindowTitle, int nWidth, int nHeight, IUIEngine::ERenderTarget eRenderTarget, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, const char *pchTargetMonitor )
{
	m_strTargetMonitor = pchTargetMonitor;
	m_unSurfaceWidth = nWidth;
	m_unSurfaceHeight = nHeight;
	m_unWindowWidth = nWidth;
	m_unWindowHeight = nHeight;
	m_eRenderTarget = eRenderTarget;
	m_bFixedSurfaceSize = bFixedSurfaceSize;
	m_bEnforceWindowAspectRatio = bEnforceWindowAspectRatio;
	m_bUseCustomMouseCursor = bUseCustomMouseCursor;
	m_bInputEnabled = false;

	// This is the size the overlay manager determined we should use for good performance
	// on this system/surface. Use it as an upper bound for any future resizing logic.
	m_unInitialWidth = nWidth;
	m_unInitialHeight = nHeight;

	// in overlay mode we don't use the hardware cursor for smoother rendering
	m_pCursorRender->UseHardwareCursorPositionForRendering( false );

#ifdef WIN32
	// Now initialize the 3d surface
	CD3D10D2DSurface *pSurface = new CD3D10D2DSurface();
	if ( !pSurface->BInitialize( NULL, m_unSurfaceWidth, m_unSurfaceHeight, m_unSurfaceWidth, m_unSurfaceHeight, eRenderTarget, true, false, bUseCustomMouseCursor ? m_pCursorRender : NULL ) )
	{
		delete pSurface;
		return false;
	}

#elif defined(POSIX)
	COpenGLSurface *pSurface = new COpenGLSurface();
	if ( !pSurface->BInitialize( NULL, 0, m_unSurfaceWidth, m_unSurfaceHeight, m_unSurfaceWidth, m_unSurfaceHeight, eRenderTarget, true, false, bUseCustomMouseCursor ? m_pCursorRender : NULL ) )
	{
		delete pSurface;
		return false;
	}
#else
#error "Surface impl please"
#endif
	m_p3DSurface = pSurface;
	m_pSurfaceInterface = pSurface;
	m_pSurfaceInterface->SetWindowScaleFactor( GetWindowScaleFactor() );

	m_pRenderEngine = new CUIRenderEngine( m_pUIEngineParent, m_p3DSurface, m_pInputEngine, this, nWidth, nHeight );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Set letter boxing color
//-----------------------------------------------------------------------------
void CTopLevelWindowOverlay::SetLetterboxColor( Color c )
{
	if ( m_p3DSurface )
		m_p3DSurface->SetOverlayLetterboxColor( c );
}

//-----------------------------------------------------------------------------
// Purpose: Push the set of commands needed to render this window to the output cmd stream
//-----------------------------------------------------------------------------
void CTopLevelWindowOverlay::PushOverlayRenderCmdStream( CSharedMemStream *pRenderStream, unsigned long dwPID, float flOpacity, EOverlayWindowAlignment alignment )
{ 
	if ( m_p3DSurface )
		m_p3DSurface->PushOverlayRenderCmdStream( pRenderStream, dwPID, flOpacity, m_unGameWidth, m_unGameHeight, alignment ); 
	
	m_bVisible = (flOpacity != 0.0f);
}


//-----------------------------------------------------------------------------
// Purpose: redo buffers/surfaces
//-----------------------------------------------------------------------------
void CTopLevelWindowOverlay::RunPlatformFrame()
{
	BaseClass::RunPlatformFrame();
}

//-----------------------------------------------------------------------------
// Purpose: Determine whether to allow an input message currently
//-----------------------------------------------------------------------------
bool CTopLevelWindowOverlay::BAllowInput( InputMessage_t &msg )
{
	// When the overlay is disabled we block input, but we always dispatch to our gamepad watcher
	if ( msg.m_eInputType == k_eGamePadUp || msg.m_eInputType == k_eGamePadDown )
	{
		// Fire off an event about this disabled input message, so any watchers/hooks that want to always see it can
		DispatchEvent( OverlayGamepadInputMsg(), (IUIPanel*)NULL, this, &msg );
	}

	return m_bInputEnabled;
}


//-----------------------------------------------------------------------------
// Purpose: Called when focus is set or lost
//-----------------------------------------------------------------------------
void CTopLevelWindowOverlay::SetFocus( bool bFocus )
{
	m_bFocus = bFocus;
}

//-----------------------------------------------------------------------------
// Purpose: directly set target game window size
//-----------------------------------------------------------------------------
void CTopLevelWindowOverlay::SetGameWindowSize( uint32 nWidth, uint32 nHeight )
{
	if ( nWidth == m_unGameWidth && nHeight == m_unGameHeight )
		return;

	m_unGameWidth = nWidth;
	m_unGameHeight = nHeight;
}


//-----------------------------------------------------------------------------
// Purpose: directly set fixed surface size bypassing normal resize/scaling logic
//-----------------------------------------------------------------------------
void CTopLevelWindowOverlay::SetFixedSurfaceSize( uint32 unSurfaceWidth, uint32 unSurfaceHeight )
{
	Assert( m_bFixedSurfaceSize );

	if ( m_unWindowWidth != unSurfaceWidth || m_unWindowHeight != unSurfaceHeight 
		|| m_unSurfaceWidth != unSurfaceWidth || m_unSurfaceHeight != unSurfaceHeight )
	{
		m_unWindowWidth = unSurfaceWidth;
		m_unWindowHeight = unSurfaceHeight;
		m_unSurfaceWidth = unSurfaceWidth;
		m_unSurfaceHeight = unSurfaceHeight;

		FOR_EACH_LL( m_listVisiblePanels, i )
		{
			m_listVisiblePanels[i]->InvalidateSizeAndPosition();
		}

		FOR_EACH_LL( m_listInvisiblePanels, i )
		{
			m_listInvisiblePanels[i]->InvalidateSizeAndPosition();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: redo buffers/surfaces
//-----------------------------------------------------------------------------
void CTopLevelWindowOverlay::OnWindowResize( uint32 nWidth, uint32 nHeight )
{
	if ( nWidth == m_unGameWidth && nHeight == m_unGameHeight )
		return;

	m_unGameWidth = nWidth;
	m_unGameHeight = nHeight;

	if ( !m_bFixedSurfaceSize )
	{
		if ( !m_bEnforceWindowAspectRatio )
		{
			m_unWindowWidth = nWidth;
			m_unWindowHeight = nHeight;
		}
		else
		{
			uint32 unSurfaceWidth = m_unInitialWidth;
			uint32 unSurfaceHeight = m_unInitialHeight;

			// Our max overlay res is 1080p, or 720p if specified on the command line. enforce this
			// by rounding down to the next size based on the game's window size and the preferred
			// size from the settings.

			if ( unSurfaceHeight >= 1080 && m_unGameWidth >= 1920 && m_unGameHeight >= 1080 )
			{
				unSurfaceWidth = 1920;
				unSurfaceHeight = 1080;
			}
			else if ( unSurfaceHeight >= 720 && m_unGameWidth >= 1280 && m_unGameHeight >= 720 )
			{
				unSurfaceWidth = 1280;
				unSurfaceHeight = 720;
			}
			else 
			{
				// small game window, or preferred size is very small
				unSurfaceWidth = MIN( m_unGameWidth, unSurfaceWidth );
				unSurfaceHeight = RoundFloatToInt( unSurfaceWidth * 0.5625f );

				// perhaps the game has a 2.35:1 window or something and we are now too tall
				if ( unSurfaceHeight > m_unGameHeight + 1 )
				{
					unSurfaceHeight = m_unGameHeight;
					unSurfaceWidth = RoundFloatToInt( unSurfaceHeight / 0.5625f );
				}
			}

			m_unWindowWidth = unSurfaceWidth;
			m_unWindowHeight = unSurfaceHeight;
			m_unSurfaceWidth = unSurfaceWidth;
			m_unSurfaceHeight = unSurfaceHeight;

			SetWindowScaleFactor( (float)m_unWindowHeight / 1080.0f );
		}

		FOR_EACH_LL( m_listVisiblePanels, i )
		{
			m_listVisiblePanels[i]->InvalidateSizeAndPosition();
		}

		FOR_EACH_LL( m_listInvisiblePanels, i )
		{
			m_listInvisiblePanels[i]->InvalidateSizeAndPosition();
		}
	}		
}


//-----------------------------------------------------------------------------
// Purpose: Should be a no-op
//-----------------------------------------------------------------------------
void CTopLevelWindowOverlay::SetWindowPosition( float x, float y )
{

}


//-----------------------------------------------------------------------------
// Purpose: always origin
//-----------------------------------------------------------------------------
void CTopLevelWindowOverlay::GetWindowPosition( float &x, float &y )
{
	x = y = 0.0;
}


//-----------------------------------------------------------------------------
// Purpose: always origin,size
//-----------------------------------------------------------------------------
void CTopLevelWindowOverlay::GetWindowBounds( float &left, float &top, float &right, float &bottom )
{
	left = top = 0.0;
	right = m_unSurfaceWidth;
	bottom = m_unSurfaceHeight;
}

//-----------------------------------------------------------------------------
// Purpose: same as texture/surface size
//-----------------------------------------------------------------------------
void CTopLevelWindowOverlay::GetClientDimensions( float &width, float &height )
{
	width = m_unSurfaceWidth;
	height = m_unSurfaceHeight;
}


//-----------------------------------------------------------------------------
// Purpose: no op
//-----------------------------------------------------------------------------
void CTopLevelWindowOverlay::Activate( bool bForceful )
{
	REFERENCE( bForceful );
}


//-----------------------------------------------------------------------------
// Purpose: no op
//-----------------------------------------------------------------------------
void CTopLevelWindowOverlay::Minimize()
{
}


//-----------------------------------------------------------------------------
// Purpose: store info about remote process
//-----------------------------------------------------------------------------
bool CTopLevelWindowOverlay::SetGameProcessInfo( AppId_t nAppId, bool bCanShareSurfaces, int32 eTextureFormat )
{
	m_nAppId = nAppId;
	m_bCanShareSurfaces = bCanShareSurfaces;
	if ( m_bCanShareSurfaces && m_eRenderTarget == IUIEngine::k_ERenderToOverlayTexture )
		m_eRenderTarget = IUIEngine::k_ERenderToOverlaySharedTexture;
	if ( !m_bCanShareSurfaces && m_eRenderTarget == IUIEngine::k_ERenderToOverlaySharedTexture )
		m_eRenderTarget =  IUIEngine::k_ERenderToOverlayTexture;
	m_p3DSurface->SetOverlayTextureFormat( eTextureFormat );
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: cleanup
//-----------------------------------------------------------------------------
void CTopLevelWindowOverlay::Shutdown()
{
	CTopLevelWindow::Shutdown();

	SAFE_DELETE( m_pRenderEngine );
	SAFE_DELETE( m_p3DSurface );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CTopLevelWindowOverlay::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

#ifdef WIN32
	ValidatePtr( (CD3D10D2DSurface *)m_p3DSurface );
#elif defined(POSIX)
	ValidatePtr( (COpenGLSurface *)m_p3DSurface );
#endif
	CTopLevelWindow::Validate( validator, pchName );
}
#endif
