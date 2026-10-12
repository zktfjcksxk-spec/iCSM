//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UITOPLEVELWINDOWSOURCE2_H
#define UITOPLEVELWINDOWSOURCE2_H
#pragma once

#include "panorama/uitoplevelwindow.h"
#include "renderer/iui3dsurface.h"
#include "input/mousecursors.h"
#include "inputsystem/ButtonCode.h"
#include "inputsystem/InputEnums.h"
#include "iimemanager.h"

class CIMEUITextField;

namespace panorama
{

//
// Top level window class
//
class CTopLevelWindowSource2 : public CTopLevelWindow, public IIMEUIView
{
public:
	CTopLevelWindowSource2( CUIEngine *pUIEngineParent, const char *pName, InputContextHandle_t hInputContext );
	virtual ~CTopLevelWindowSource2();

	// Initialize backing surface for window
	virtual bool BInitializeSurface( int xPos, int yPos, int nWidth, int nHeight, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, bool bAcceptKBandMouse, bool bDrawToBackBuffer );

	// Resize the window to specified dimensions
	virtual void OnWindowResize( uint32 width, uint32 height );

	// Render the window into render context
	virtual void RenderWindow( PlatWindow_t hwnd, bool bDebugger ) ;
	virtual void SetAnimationDisabled( bool bAnimate );
#ifdef PLATFORM_WINDOWS
	virtual void CopyDebuggerRtBits( uint32* pBitsOut, uint32 nWd, uint32 nHt );
#endif

	virtual bool BUseAutoMouseUpBehavior() { return m_bUseAutoMouseUpBehavior; }

	virtual void SetForceConsumeKBAndMouseInputEvents( bool bForce );
	virtual bool BForceConsumeKBAndMouseInputEvents();

	// Access image manager for window
	virtual CImageResourceManager* AccessImageManager() OVERRIDE;

	// Window position management
	virtual void SetWindowPosition( float x, float y ) OVERRIDE;
	virtual void GetWindowPosition( float &x, float &y ) OVERRIDE;
	virtual void GetWindowBounds( float &left, float &top, float &right, float &bottom ) OVERRIDE;
	virtual void GetClientDimensions( float &width, float &height ) OVERRIDE;
	virtual void Activate( bool bForceful ) OVERRIDE;
	virtual void Minimize() OVERRIDE {}
	virtual void SetTopMost( bool bTopMost ) OVERRIDE {}
	virtual bool BHasFocus() OVERRIDE;
	virtual bool BIsVisible() OVERRIDE;
	virtual void* GetNativeWindowHandle() OVERRIDE;
	virtual void ForceHideWindow() OVERRIDE {};
	bool BUseSystemCursor() { return !m_bUseCustomMouseCursor; }

	// Necessary for generating mouse enter & leave events on windows
	bool IsMouseOver() { return m_bMouseOverWindow; }
	void OnMouseEnter();
	void OnMouseLeave() { m_bMouseOverWindow = false; }

	void SetMouseCursor( EMouseCursors eCursor );
	void ClearRepeats( KeyCode code );
	int IncrementRepeats( KeyCode code );

	virtual void SetPreventForceWindowOnTop( bool bPreventForceTopLevel );
	
	void OnLostFocus();
	void OnGotFocus();

	// Input routing from source2
	bool HandleInputEvent( const InputEvent_t &event );

	PlatWindow_t GetPlatWindow() const { return m_hPlatWindow; }

	virtual void SetVisible( bool bVisible );

	void SetUseAutoMouseUpBehavior( bool bAutoMouseUpBehavior ) { m_bUseAutoMouseUpBehavior = bAutoMouseUpBehavior; }

	virtual void TextEntryFocusChange( IUIPanel *pPanel ) OVERRIDE;
	virtual void TextEntryInvalid( IUIPanel *pPanel ) OVERRIDE;

	// Interface for IIMEUIView
	virtual IIMEUIObject *GetFocusedObject();
	virtual PlatWindow_t GetAssociatedPlatWindow();

	bool IsClosed() const
	{
		return m_bIsClosed;
	}
	
#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
	static void ValidateStatics( CValidator &validator, const tchar *pchName );
#endif

protected:
	virtual void Shutdown();

private:
	bool HandleKeyboardInputMessage( InputMessage_t &input );
#ifdef PANORAMA_USE_S1WRAPPER
	bool ShouldSkipKeyCodeTypedEvent() { return m_bSkipKeyCodeTypedEvent; }
	void SkipNextKeyCodeTypedEvent( bool bShouldSkip ) { m_bSkipKeyCodeTypedEvent = bShouldSkip; }
	bool m_bSkipKeyCodeTypedEvent;
#endif

	CUtlString m_Name;

	InputContextHandle_t m_hInputContext;

	int m_unXPos;
	int m_unYPos;

	IUI3DSurface *m_p3DSurface;
	bool m_bMouseOverWindow;
	bool m_bHidden;
	CUtlMap< KeyCode, int, int, CDefLess< KeyCode > > m_mapKeyRepeats;
	PlatWindow_t m_hPlatWindow;

	int m_nMouseX;
	int m_nMouseY;
	bool m_bAcceptKBandMouse;

	CIMEUITextField *m_pIMEUITextField;

	bool m_bUseAutoMouseUpBehavior;

	bool m_bIsClosed;
	bool m_bIsAnimationDisabled;

	bool m_bForceConsumeKBAndMouseInputEvents;
};

} // namespace panorama

#endif // UITOPLEVELWINDOWSDL_H
