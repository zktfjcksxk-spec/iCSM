//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_HTML_H
#define PANORAMA_HTML_H

#if defined(_WIN32) || defined(SOURCE2_PANORAMA)
#pragma once
#endif

#include "panel2d.h"
#include "scrollbar.h"
#include "tier1/utlmap.h"
#include "tier1/utlstring.h"
#include "../uievent.h"
#include "mathlib/beziercurve.h"
#include "../textinput/textinput.h"

#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
#include "html/ihtmlchrome.h"
#include "tier1/shared_memory.h"
#endif

class CTexturePanel;
namespace panorama
{
class CTextTooltip;
class CHTML;
class CFileOpenDialog;

struct HtmlFormHasFocus_t
{
	HtmlFormHasFocus_t() :
		m_bInput( false ),
		m_bInputHasMultiplePeers( false ),
		m_bUserInputThisPage( false ),
		m_bFocusedElementChanged( true )
	{
	}
	void Reset()
	{
		m_bInput = false;
		m_bUserInputThisPage = false;
		m_bInputHasMultiplePeers = false;
		m_bFocusedElementChanged = true;
	}

	bool operator==(const HtmlFormHasFocus_t &rhs) const
	{
		return rhs.m_bInput == m_bInput && 
			rhs.m_sName == m_sName &&
			rhs.m_sSearchLabel == m_sSearchLabel &&
			rhs.m_sInputType == m_sInputType;
	}
	bool			m_bInput;
	CUtlString		m_sName;
	CUtlString		m_sSearchLabel;
	bool			m_bInputHasMultiplePeers;
	bool			m_bUserInputThisPage;
	CUtlString		m_sInputType;
	bool			m_bFocusedElementChanged;
};

DECLARE_PANEL_EVENT2( HTMLURLChanged, const char *, const char * )
DECLARE_PANEL_EVENT1( HTMLLoadPage, const char * )
DECLARE_PANEL_EVENT2( HTMLFinishRequest, const char *, const char * )
DECLARE_PANEL_EVENT1( HTMLTitle, const char * )
DECLARE_PANEL_EVENT1( HTMLStatusText, const char * )
DECLARE_PANEL_EVENT1( HTMLJSAlert, const char * )
DECLARE_PANEL_EVENT1( HTMLJSConfirm, const char * )
DECLARE_PANEL_EVENT2( HMTLLinkAtPosition, const char *, bool )
DECLARE_PANEL_EVENT4( HMTLThumbNailImage, int, CUtlBuffer *, uint32, uint32  )
DECLARE_PANEL_EVENT1( HTMLOpenLinkInNewTab, const char * )
DECLARE_PANEL_EVENT2( HTMLOpenPopupTab,  CHTML *, const char * )
DECLARE_PANEL_EVENT2( HTMLBackForwardState, bool, bool )
DECLARE_PANEL_EVENT2( HTMLUpdatePageSize, int, int )
DECLARE_PANEL_EVENT5( HTMLSecurityStatus, const char *, bool, bool, bool, const char * )
DECLARE_PANEL_EVENT1( HTMLFullScreen, bool )
DECLARE_PANEL_EVENT2( HTMLStartMousePanning, int, int )
DECLARE_PANEL_EVENT0( HTMLStopMousePanning )
DECLARE_PANEL_EVENT0( HTMLCloseWindow )
DECLARE_PANEL_EVENT2( HTMLFormHasFocus, HtmlFormHasFocus_t, const char * /* URL */ )
DECLARE_PANEL_EVENT2( HTMLScreenShotTaken, const char *, const char * )
DECLARE_PANEL_EVENT1( HTMLFocusedNodeValue, const char * )
DECLARE_PANEL_EVENT2( HTMLStartRequest, const char *, bool * );

class CImagePanel;

enum CursorCode
{
	eCursorNone,
	eCursorArrow
};

class IUIDoubleBufferedTexture;
class CTransform3D;
extern const int k_nExtraScrollRoom; // max number of padding pixels to use if needed

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CHTML : public CPanel2D, public ITextInputControl
#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	, public IHTMLResponses
#endif
{
	DECLARE_PANEL2D( CHTML, CPanel2D );

public:
	CHTML( CPanel2D *parent, const char * pchPanelID,  bool bPopup = false );
	virtual ~CHTML();
	void Shutdown();

	// panel2d overrides
	virtual void Paint() OVERRIDE;
	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );
	virtual void OnStylesChanged() OVERRIDE;
	virtual bool BRequiresContentClipLayer() OVERRIDE { return true; } // BUGBUG Alfred - fix ::Paint to scale u/v offsets rather than requiring a clipping
	virtual void OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions ) OVERRIDE;

	// simple browser management
	void OpenURL(const char *);
	void PostURL( const char *pchURL, const char *pchPostData );
	void AddHeader( const char *pchHeader, const char *pchValue );
	void StopLoading();
	void Refresh();
	void GoBack();
	void GoForward();
	bool BCanGoBack();
	bool BCanGoForward();
	void SetBackgroundMode( bool bBackgroundMode );
	
	// kb/mouse management
	virtual bool OnKeyDown( const KeyData_t &code ) OVERRIDE;
	virtual bool OnKeyUp( const KeyData_t & code ) OVERRIDE;
	virtual bool OnKeyTyped( const KeyData_t &unichar ) OVERRIDE;
	virtual bool OnGamePadDown( const GamePadData_t &code ) OVERRIDE;
	virtual bool OnGamePadUp( const GamePadData_t &code ) OVERRIDE;
	virtual bool OnGamePadAnalog( const GamePadData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonDown( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonUp( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonDoubleClick( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseWheel( const MouseData_t &code ) OVERRIDE;
	virtual void OnMouseMove( float flMouseX, float flMouseY ) OVERRIDE;
	virtual bool OnVRTouchPad( const VRTouchEvent_t &code ) OVERRIDE;

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	virtual void GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties ) OVERRIDE;

	// run input event processing for something that may not be a real input event, so don't bubble to parents, etc.
	bool OnGamePadDownImpl( const GamePadData_t &code, bool *out_pbOptionalResult = nullptr );
	bool OnGamePadAnalogImpl( const GamePadData_t &code, bool *out_pbOptionalResult = nullptr );
	
	void ProcessAnalogScroll( float fValueX, float fValueY, float fDeadzoneValue, bool bAllowOverScroll );
	void ProcessAnalogZoom( float fValueX, float fValueY, float fDeadzoneValue );
	
	void ProcessSteamController();
	void InitSteamController();
	bool BHasSteamController() const { return m_hActiveControllerHandle != 0; }
	void SetControllerActionSet( const char *pszActionSet );

	// browser helpers
	void Copy();
	void Paste();
	void PressButton( panorama::KeyCode key, uchar32 unichar );
	void RequestLinkUnderCursor();
	void ZoomToElementUnderCursor();
	const char *PchLastLinkAtPosition() { return m_LinkAtPos.m_sURL; }
	bool BIsCursorOverLink() const { return m_LinkAtPos.m_bLiveLink && !m_LinkAtPos.m_sURL.IsEmpty(); }
	void RunJavascript( const char *pchScript );
	void ViewSource();
	void SetHorizontalScroll( int scroll );
	void SetVerticalScroll( int scroll );
	void OnHTMLCursorMove( float flMouseX, float flMouseY );

	// finding text on the page
	void Find( const char *pchSubStr );
	void StopFind();
	void FindNext();
	void FindPrevious();

	void SetFileDialogChoice( const char *pchFileName ); // callback if cef wanted us to pick a file

	bool BAcceptMouseInput(); // returns true if the control is listening to mouse input right now, false if gamepad input mode is on
	virtual bool BRequiresFocus() OVERRIDE { return true; }

	bool BIgnoreMouseBackForwardButtons() { return m_bIgnoreMouseBackForwardButtons; }
	void SetIgnoreMouseBackForwardButtons( bool bIgnore ) { m_bIgnoreMouseBackForwardButtons = bIgnore; }

	const char *PchCurrentURL() { return m_sCurrentURL; } // the current URL the browser has loaded
	const char *PchCurrentPageTitle() { return m_sHTMLTitle; } // the title of the currently loaded page

	void SaveCurrentPageToJPEG( const char *pchFileName, int nWide, int nTall ); // save this current page to a jpeg

	// results for JS alert popups
	void DismissJSDialog( bool bRetVal );

	static uint32 GetAndResetPaintCounter();

	void ReleaseTextureMemory( bool bSuppressTextureLoads = false );
	void RefreshTextureMemory();

	void CaptureThumbNailImage( CPanel2D *pEventTarget, int iUserData );
	void IncrementPageScale( float flScaleIncrement, bool bZoomFromOrigin = false );
	void ExitFullScreen();
	void ExecuteJavaScript( const char *pchScript );

	// SSL/security state for the loaded html page
	bool BIsSecure() const { return m_bIsSecure; }
	bool BIsCertError() const { return m_bIsCertError; }
	bool BIsEVCert() const { return m_bIsEVCert; }
	const char *PchCertName() const { return m_sCertName; }

	// if true don't allow the page to scroll beyond the page edges
	void SetEmbeddedMode( bool bState );
	void SetIgnoreCursor( bool bState ); // don't use or show a cursor

	void ZoomPageToFocusedElement();

	// ITextInputControl helpers

	virtual bool BSupportsImmediateTextReturn() { return false; }

	virtual int32 GetCursorOffset() const { return 0; }
	virtual  uint GetCharCount() const { return 0; }

	virtual const char *PchGetText() const { return ""; }
	virtual const uchar32 *Pch32GetText() const { return s_ch32EmptyStr; }

	virtual void InsertCharacterAtCursor( const uchar32 &unichar );
	virtual void InsertCharactersAtCursor( const uchar32 *pch32, size_t cch32 ) 
	{
		for ( uint i = 0; i < cch32; i++ )
			InsertCharacterAtCursor( pch32[i] ); 
	}

	virtual CPanel2D *GetAssociatedPanel() { return this; }
	virtual void OnTextInputHandlerOpened( CTextInputHandler *pHandler ) OVERRIDE { /* no custom implementation currently */ }

	virtual void RequestControlString() { RequestFocusedNodeValue(); }
	

	void PauseFlashVideoIfVisible();

	void ResetScrollbarsAndClearOverflow();

	void UpdateCursorBehaviourOnFocusChange( bool bGotFocus );

	void SetPopupChild(CHTML *pChild) { m_pPopupChild = pChild; }
	
	void EnableSteamControllerSupport();

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const char *pchName ) OVERRIDE;
	static void ValidateStatics( CValidator &validator, const char *pchName );
#endif

	class CHTMLVerticalScrollBar : public CScrollBar
	{
		DECLARE_PANEL2D( CHTMLVerticalScrollBar, CScrollBar );

	public:
		CHTMLVerticalScrollBar( CPanel2D *parent, const char * pchPanelID ) : CScrollBar( parent, pchPanelID )
		{
			m_pScrollThumb->AddClass( "VerticalScrollThumb" );
		}

		virtual float GetInterpolatedScrollWindowPosition() OVERRIDE { return -GetParent()->GetInterpolatedYScrollOffset(); }

		virtual ~CHTMLVerticalScrollBar() {}

	protected:
		virtual void UpdateLayout( bool bImmediateMove )
		{
			CUILength zero;
			zero.SetLength( 0.0f );

			if ( GetRangeSize() < 0.001f )
				return;

			CUILength length;

			float flXPosPercent = (GetScrollWindowPosition() - GetRangeMin()) / GetRangeSize();
			length.SetPercent( flXPosPercent * 100.0f );
			if ( bImmediateMove )
				m_pScrollThumb->SetPositionWithoutTransition( zero, length, zero );
			else
				m_pScrollThumb->SetPosition( zero, length, zero );

			float flWidthPercent = GetScrollWindowSize() / GetRangeSize();

			length.SetPercent( flWidthPercent*100.0f );
			m_pScrollThumb->AccessStyleDirty()->SetHeight( length );
			length.SetPercent( 100.0f );
			m_pScrollThumb->AccessStyleDirty()->SetWidth( length );

			m_bLastMoveImmediate = bImmediateMove;
		}

		virtual float GetActualLayoutLength() OVERRIDE { return GetActualLayoutHeight(); }
		virtual float GetContentLength() OVERRIDE { return GetRangeSize(); }
		virtual void SetMouseCoord( const Vector &vMousePosition ) OVERRIDE { m_flMouseCoord = vMousePosition.y; }

		virtual void StopScroll() OVERRIDE {}

	};

	class CHTMLHorizontalScrollBar : public CScrollBar
	{
		DECLARE_PANEL2D( CHTMLHorizontalScrollBar, CScrollBar );

	public:
		CHTMLHorizontalScrollBar( CPanel2D *parent, const char * pchPanelID ) : CScrollBar( parent, pchPanelID )
		{
			m_pScrollThumb->AddClass( "HorizontalScrollThumb" );
		}

		virtual float GetInterpolatedScrollWindowPosition() OVERRIDE { return -GetParent()->GetInterpolatedXScrollOffset(); }

		virtual ~CHTMLHorizontalScrollBar() {}

	protected:
		virtual void UpdateLayout( bool bImmediateMove )
		{
			CUILength zero;
			zero.SetLength( 0.0f );

			if ( GetRangeSize() < 0.001f )
				return;

			CUILength length;

			float flXPosPercent = (GetScrollWindowPosition() - GetRangeMin()) / GetRangeSize();
			length.SetPercent( flXPosPercent * 100.0f );
			if ( bImmediateMove )
				m_pScrollThumb->SetPositionWithoutTransition( length, zero, zero );
			else
				m_pScrollThumb->SetPosition( length, zero, zero );

			float flWidthPercent = GetScrollWindowSize() / GetRangeSize();

			length.SetPercent( flWidthPercent*100.0f );
			m_pScrollThumb->AccessStyleDirty()->SetWidth( length );
			length.SetPercent( 100.0f );
			m_pScrollThumb->AccessStyleDirty()->SetHeight( length );
			m_bLastMoveImmediate = bImmediateMove;
		}

		virtual float GetActualLayoutLength() OVERRIDE { return GetActualLayoutWidth(); }
		virtual float GetContentLength() OVERRIDE { return GetRangeSize(); }
		virtual void SetMouseCoord( const Vector &vMousePosition ) OVERRIDE { m_flMouseCoord = vMousePosition.x; }

		virtual void StopScroll() OVERRIDE {}

	};

	enum EHTMLScrollDirection
	{
		kHTMLScrollDirection_Up,
		kHTMLScrollDirection_Down,
		kHTMLScrollDirection_Left,
		kHTMLScrollDirection_Right
	};

	bool BCanScrollInDirection( EHTMLScrollDirection eDirection ) const;

	void ScrollPageUp( float flScrollValue );
	void ScrollPageDown( float flScrollValue );
protected:
	// functions you can override to specialize html behavior
	virtual void OnURLChanged( const char *url, const char *pchPostData, bool bIsRedirect );
	virtual void OnFinishRequest(const char *url, const char *pageTitle);
	virtual void OnPageLoaded( const char *url, const char *pageTitle, const CUtlMap < CUtlString, CUtlString > &headers );
	virtual bool OnStartRequestInternal( const char *url, const char *target, const char *pchPostData, bool bIsRedirect );
	virtual void ShowPopup();
	virtual void HidePopup();
	virtual bool OnOpenNewTab( const char *pchURL, bool bForeground );
	virtual bool OnPopupHTMLWindow( const char *pchURL, int x, int y, int wide, int tall );
	virtual void SetHTMLTitle( const char *pchTitle );
	virtual void OnLoadingResource( const char *pchURL );
	virtual void OnSetStatusText(const char *text);
	virtual void OnSetCursor( CursorCode cursor );
	virtual void OnFileLoadDialog( const char *pchTitle, const char *pchInitialFile );
	virtual void OnShowToolTip( const char *pchText );
	virtual void OnUpdateToolTip( const char *pchText );
	virtual void OnHideToolTip();
	virtual void OnSearchResults( int iActiveMatch, int nResults );

	friend class ::CTexturePanel;
	CRefPtr< IUIDoubleBufferedTexture > m_pDoubleBufferedTexture;
	CRefPtr< IUIDoubleBufferedTexture > m_pDoubleBufferedTexturePending;
	CRefPtr< IUIDoubleBufferedTexture > m_pDoubleBufferedTextureComboBox;
	int32 m_nTextureSerial; // serial number of the last texture we uploaded

	void RequestFocusedNodeValue();
	// if true then our html control overrides scrolling and scrollbars, if false we 
	// let the web control scroll itself
	void SetManualHTMLScroll( bool bControlScroll ) { m_bControlPageScrolling = bControlScroll;  }

	int HorizontalPageSize();
	int VerticalPageSize();

private:
	
	// getters/setters for html cef object
	void SetHTMLFocus();
	void KillHTMLFocus();
	int HorizontalScroll();
	int VerticalScroll();
	bool IsHorizontalScrollBarVisible();
	bool IsVeritcalScrollBarVisible();
	void GetCookiesForURL( const char *pchURL );
	void UpdatePanoramaScrollBars();
	bool BHandleKeyPressPageScroll() const;

#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	STEAM_CALLBACK( CHTML, OnLinkAtPositionResponse, HTML_LinkAtPosition_t, m_LinkAtPosRespose );

	STEAM_CALLBACK( CHTML, OnHTMLNeedsPaint, HTML_NeedsPaint_t, m_HTML_NeedsPaint );
	STEAM_CALLBACK( CHTML, OnHTMLStartRequest, HTML_StartRequest_t, m_HTML_StartRequest );
	STEAM_CALLBACK( CHTML, OnHTMLCloseBrowser, HTML_CloseBrowser_t, m_HTML_CloseBrowser );
	STEAM_CALLBACK( CHTML, OnHTMLURLChanged, HTML_URLChanged_t, m_HTML_URLChanged );
	STEAM_CALLBACK( CHTML, OnHTMLFinishedRequest, HTML_FinishedRequest_t, m_HTML_FinishedRequest );
	STEAM_CALLBACK( CHTML, OnHTMLOpenLinkInNewTab, HTML_OpenLinkInNewTab_t, m_HTML_OpenLinkInNewTab );
	STEAM_CALLBACK( CHTML, OnHTMLChangedTitle, HTML_ChangedTitle_t, m_HTML_ChangedTitle );
	STEAM_CALLBACK( CHTML, OnHTMLSearchResults, HTML_SearchResults_t, m_HTML_SearchResults );
	STEAM_CALLBACK( CHTML, OnHTMLCanGoBackAndForward, HTML_CanGoBackAndForward_t, m_HTML_CanGoBackAndForward );
	STEAM_CALLBACK( CHTML, OnHTMLHorizontalScroll, HTML_HorizontalScroll_t, m_HTML_HorizontalScroll );
	STEAM_CALLBACK( CHTML, OnHTMLVerticalScroll, HTML_VerticalScroll_t, m_HTML_VerticalScroll );
	STEAM_CALLBACK( CHTML, OnHTMLJSAlert, HTML_JSAlert_t, m_HTML_JSAlert );
	STEAM_CALLBACK( CHTML, OnHTMLJSConfirm, HTML_JSConfirm_t, m_HTML_JSConfirm );
	STEAM_CALLBACK( CHTML, OnHTMLFileOpenDialog, HTML_FileOpenDialog_t, m_HTML_FileOpenDialog );
	STEAM_CALLBACK( CHTML, OnHTMLNewWindow, HTML_NewWindow_t, m_HTML_NewWindow );
	STEAM_CALLBACK( CHTML, OnHTMLSetCursor, HTML_SetCursor_t, m_HTML_SetCursor );
	STEAM_CALLBACK( CHTML, OnHTMLStatusText, HTML_StatusText_t, m_HTML_StatusText );
	STEAM_CALLBACK( CHTML, OnHTMLShowToolTip, HTML_ShowToolTip_t, m_HTML_ShowToolTip );
	STEAM_CALLBACK( CHTML, OnHTMLUpdateToolTip, HTML_UpdateToolTip_t, m_HTML_UpdateToolTip );
	STEAM_CALLBACK( CHTML, OnHTMLHideToolTip, HTML_HideToolTip_t, m_HTML_HideToolTip );
#else
	// message handlers for ipc thread
	void BrowserSetIndex( int idx ) { m_iBrowser = idx; SendPendingHTMLMessages(); }
	int  BrowserGetIndex() { return m_iBrowser; }
	void BrowserReady( const CMsgBrowserReady *pCmd );
	void BrowserSetSharedPaintBuffers( const CMsgSetSharedPaintBuffers *pCmd );
	void BrowserNeedsPaint( const CMsgNeedsPaint *pCmd );
	void BrowserStartRequest( const CMsgStartRequest *pCmd );
	void BrowserURLChanged( const CMsgURLChanged *pCmd );
	void BrowserLoadedRequest( const CMsgLoadedRequest *pCmd );
	void BrowserFinishedRequest(const CMsgFinishedRequest *pCmd);
	void BrowserPageSecurity( const CMsgPageSecurity *pCmd );
	void BrowserShowPopup( const CMsgShowPopup *pCmd );
	void BrowserHidePopup( const CMsgHidePopup *pCmd );
	void BrowserOpenNewTab( const CMsgOpenNewTab *pCmd );
	IHTMLResponses *BrowserPopupHTMLWindow( const CMsgPopupHTMLWindow *pCmd );
	void BrowserSetHTMLTitle( const CMsgSetHTMLTitle *pCmd );
	void BrowserLoadingResource( const CMsgLoadingResource *pCmd );
	void BrowserStatusText( const CMsgStatusText *pCmd );
	void BrowserSetCursor( const CMsgSetCursor *pCmd );
	void BrowserFileLoadDialog( const CMsgFileLoadDialog *pCmd );
	void BrowserShowToolTip( const CMsgShowToolTip *pCmd );
	void BrowserUpdateToolTip( const CMsgUpdateToolTip *pCmd );
	void BrowserHideToolTip( const CMsgHideToolTip *pCmd );
	void BrowserSearchResults( const CMsgSearchResults *pCmd );
	void BrowserClose( const CMsgClose *pCmd );
	void BrowserHorizontalScrollBarSizeResponse( const CMsgHorizontalScrollBarSizeResponse *pCmd );
	void BrowserVerticalScrollBarSizeResponse( const CMsgVerticalScrollBarSizeResponse *pCmd );
	void BrowserGetZoomResponse( const CMsgGetZoomResponse *pCmd ) {}
	void BrowserLinkAtPositionResponse( const CMsgLinkAtPositionResponse *pCmd );
	void BrowserZoomToElementAtPositionResponse( const CMsgZoomToElementAtPositionResponse *pCmd );
	void BrowserJSAlert( const CMsgJSAlert *pCmd );
	void BrowserJSConfirm( const CMsgJSConfirm *pCmd );
	void BrowserCanGoBackandForward( const CMsgCanGoBackAndForward *pCmd );
	void BrowserOpenSteamURL( const CMsgOpenSteamURL *pCmd );
	void BrowserSizePopup( const CMsgSizePopup *pCmd );
	void BrowserScalePageToValueResponse( const CMsgScalePageToValueResponse *pCmd );
	void BrowserRequestFullScreen( const CMsgRequestFullScreen *pCmd );
	void BrowserExitFullScreen( const CMsgExitFullScreen *pCmd );
	void BrowserGetCookiesForURLResponse( const CMsgGetCookiesForURLResponse *pCmd );
	void BrowserNodeGotFocus( const CMsgNodeHasFocus *pCmd );
	void BrowserSavePageToJPEGResponse( const CMsgSavePageToJPEGResponse *pCmd );
	void BrowserFocusedNodeValueResponse( const CMsgFocusedNodeTextResponse *pCmd );
	void BrowserComboNeedsPaint(const CMsgComboNeedsPaint *pCmd);
	void BrowserJSMethodCall( const CMsgJSMethodCall *pCmd ) {}
	bool BSupportsOffMainThreadPaints();
	void ThreadNotifyPendingPaints();
#endif

	// helpers to control browser side, pos and textures
	void SetBrowserSize( int wide, int tall );
	void SendPendingHTMLMessages();

	// helpers to move the html page around inside the control
	void ScrollPageLeft( float flScrollValue );
	void ScrollPageRight( float flScrollValue );

	// Overrides for scroll bar to call back to us rather than normal panel2d call
	virtual void ScrollToXPercent( float flXPercent );
	virtual void ScrollToYPercent( float flXPercent );

	// event handlers
	bool OnGamepadInput( GamePadCode eGamePadCode );
	bool OnPropertyTransitionEnd( const CPanelPtr< IUIPanel > &pPanel, CStyleSymbol prop );
	bool OnSetBrowserSize( const CPanelPtr< IUIPanel > &pPanel, int nWide, int nTall );
	bool OnHTMLFormFocusPending( const CPanelPtr< IUIPanel > &pPanel );
	bool OnInputFocusSet( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel );
	bool OnInputFocusLost( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel );
	bool OnInputFocusTopLevelChanged( panorama::CPanelPtr< panorama::IUIPanel > ptrPanel );
	bool OnHTMLScreenShotCaptured( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, int nThumbNailWidth, int nThumbNailHeight  );
	bool OnHTMLCommitZoom( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, float flZoom );
	bool OnHTMLRequestRepaint( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel );

#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	bool OnFileOpenDialogFilesSelected( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, const char *pszFiles );
#endif

	// moving the html texture around
	void ResizeBrowserTextureIfNeeded();
	
	float GetHScrollOffset() 
	{ 
		return m_ScrollLeft.m_flOffsetTextureScroll;
	}

	float GetVScrollOffset()
	{ 
		return m_ScrollUp.m_flOffsetTextureScroll;
	}

	void SetPageDPI( float flDPI );

	bool m_bInitialized; // used to prevent double shutdown
	bool m_bReady; // When we are ready to load a url
	CTexturePanel *m_pTexurePanel;
	CLabel *m_pBrowserDisabledMessage;

	int m_nWindowWide, m_nWindowTall; // how big the html texture should be
	int m_nTextureWide, m_nTextureTall;

	// find in page state
	bool m_bInFind;
	CUtlString m_sLastSearchString;

	CUtlString m_sURLToLoad; // url to load once the browser is created
	CUtlString m_sURLPostData; // post data for url to load

	CUtlString m_sCurrentURL; // the current url we have loaded
	CUtlString m_sHTMLTitle; // the title of the page we are on

	int m_iBrowser; // our browser handle

	float m_flZoom; // what zoom level we are at
	bool m_bLastKeyFocus; // tracking for when key focus changes in style application

	struct ScrollControl_t
	{
		ScrollControl_t()
		{
			Reset();
		}

		void Reset()
		{
			m_flOffsetTextureScroll = 0.0f;
			m_bScrollingUp = false;
			m_flLastScrollTime = 0.0f;
			m_flScrollRemainder = 0.0f;
		}

		float	m_flOffsetTextureScroll; // amount the html texture is scrolled around the panel itself
		bool	m_bScrollingUp; // we are scrolling up (or left) on the page last?
		double	m_flLastScrollTime; // when did we scroll in this direction last, used for accel curve
		float	m_flScrollRemainder; // remainder from scolling added next time
	};

	ScrollControl_t m_ScrollUp;
	ScrollControl_t m_ScrollLeft;

	struct ScrollData_t
	{
		ScrollData_t() 
		{
			m_bVisible = false;
			m_nMaxScroll = m_nScroll = m_nPageSize = m_nWebScroll = 0;
		}

		bool operator==( const ScrollData_t &rhs ) const
		{
			return rhs.m_bVisible == m_bVisible &&
				rhs.m_nScroll == m_nScroll &&
				rhs.m_nPageSize == m_nPageSize &&
				rhs.m_nMaxScroll == m_nMaxScroll;
		}

		bool operator!=( const ScrollData_t &rhs ) const
		{
			return !operator==(rhs);
		}

		bool m_bVisible; // is the scroll bar visible
		int m_nMaxScroll; // most amount of pixels we can scroll
		int m_nPageSize; // the underlying size of the page itself in pixels
		int m_nScroll; // currently scrolled amount of pixels
		int m_nWebScroll; // last scrolled return value from cef, not updated locally
	};

	bool ScrollHelper( bool bHorizontal, float flScrollDelta, float flLeadInTime = 0.0f, bool bAllowOverScroll = false, bool * bHitEdge = NULL ); // shared code when scrolling around the page
	bool SetupScrollBar( bool bHorizontal, float flMaxSize );
	CScrollBar *MakeScrollBar( bool bHorizontal );

	ScrollData_t m_scrollHorizontal; // details of horizontal scroll bar
	ScrollData_t m_scrollVertical; // details of vertical scroll bar
	CCubicBezierCurve< Vector2D > m_ScrollBezier; // the curve to scale scroll accel by

	struct LinkAtPos_t
	{
		LinkAtPos_t() { m_bLiveLink = false; }
		CUtlString m_sURL;
		bool m_bLiveLink;
		bool m_bInput;
	};
	LinkAtPos_t m_LinkAtPos; // cache for link at pos requests, because the request is async

	// last position we saw the mouse at
	float m_flCursorX;
	float m_flCursorY;

	double m_flGamePadInputTime; // last time we saw input from the gamepad
	double m_flStartZoomTime;

	bool m_bPopupVisible; // true if a popup menu is visible on the client
	bool m_bCanGoBack;
	bool m_bCanGoForward;
	bool m_bIgnoreMouseBackForwardButtons;

	int m_nHTMLPageWide; // last size we told CEF the page should be 
	int m_nHTMLPageTall;

	CHTMLVerticalScrollBar *m_pVerticalScrollBar; // our own copy of the scroll bars that we control manually
	CHTMLHorizontalScrollBar *m_pHorizontalScrollBar;
	float m_flLastVeritcalScrollPos;
	float m_flLastHorizontalScrollPos;

	static uint32 sm_PaintCount;
	uint32 m_PageLoadCount; // the number of posturl calls we have made
#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	CUtlVector<HTMLCommandBuffer_t *> m_vecPendingMessages;
#endif
	bool m_bSuppressTextureLoads;

	bool m_bCaptureThumbNailThisFrame;
	CPanel2D *m_pCaptureEventTarget;
	int m_nCaptureUserData;

	bool m_bCommenceZoomOperationOnTextureUpload; // when the next texture upload is ready, should we apply scale/offset transforms we have saved above
	bool m_bFullScreen; // are we in fullscreen right now?
	bool m_bConfigureYouTubeHTML5OptIn; // are we doing the forcefully opt into youtube html5 beta path
	bool m_bMousePanningActive; // true if the mouse is in panning mode (middle mouse button)
	Vector2D m_vecMousePanningPos; // the x,y pos of the mouse over the panel when middle panning started
	CImagePanel *m_pMousePanningImage; // the image to show when panning
	CCubicBezierCurve< Vector2D > m_MousePanBezier; // the curve to scale panning accel by
	bool m_bIsSecure; // is this page ssl secure?
	bool m_bIsCertError; // did we have a cert error when loading? 
	bool m_bIsEVCert; // is it an EV cert?
	CUtlString m_sCertName; // who was the cert issued to?
	bool m_bEmbedded; // if true we are embedded instance, just show html pages and simple scrolling, not complex interactions
	bool m_bIgnoreCursor; // if true, the cursor is ignore for input
	float m_flMouseLastX;
	float m_flMouseLastY;
	
	panorama::HtmlFormHasFocus_t m_evtFocus;		// used for saving state of controls that have focus info dispatched

	bool m_bFocusEventSentForClick; // time we sent a focus event to the browser
	bool m_bDidMousePanWhileMouseDown; // did we do panning with the mouse held down
	bool m_bWaitingForZoomResponse;

	panorama::CTextTooltip *m_pTooltip;
	bool m_bGotKeyDown;
#if defined( SOURCE2_PANORAMA ) || defined( PANORAMA_PUBLIC_STEAM_SDK )
	HHTMLBrowser m_HTMLBrowser;
	void OnBrowserReady( HTML_BrowserReady_t *pBrowserReady, bool bIOFailure );
	CCallResult< CHTML, HTML_BrowserReady_t > m_SteamCallResultBrowserReady;

	CPanelPtr< CFileOpenDialog > m_pFileOpenDialog;
	
	CUtlVector<HHTMLBrowser> m_vecDenyNewBrowserWindows;
#else
	CChromePaintBufferClient m_SharedPaintBuffer;
#endif

	int m_nPopupX;
	int m_nPopupY;
	int m_nPopupWide;
	int m_nPopupTall;
	int m_nTextureSerialCombo;
	CUtlBuffer m_ComboTexture;
	CHTML *m_pPopupChild;

	CThreadMutex m_mutexHTMLTexture;
	CThreadMutex m_mutexScreenShot;
	CUtlBuffer m_bufScreenshotTexture;
	
	bool m_bControlPageScrolling;


	bool m_bInvertScrolling;
	bool m_bVerticalAxisSnap;
	bool m_bClickingLeftPad;
	bool m_bMarkZoomStart;
	float m_flInitialZoomLevel;
	float m_flZoomSwipeOriginPosition;
	
	// Steam controller state
	
#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	STEAM_CALLBACK( CHTML, OnControllerConnected, ControllerConnected_t );
#endif
	
	ControllerHandle_t m_hActiveControllerHandle;
	CUIScheduledDel	m_scheduledProcessController;
	ControllerAnalogActionHandle_t	m_hCursorAnalogAction;
	ControllerAnalogActionHandle_t	m_hScrollAnalogAction;
	ControllerAnalogActionHandle_t	m_hZoomAnalogAction;
	bool m_bHasControllerFocus;
	bool m_bControllerScrollThunked;
	
	bool m_bSteamControllerEnabled;

	// VR controller touchpad state
	CUtlLinkedList<VRTouchEvent_t> m_vecVRTouchEvents;
	bool m_bVRTouchPadFingerDown;
	float m_flVRTouchLinearMoveDistanceForHaptics;

	static uchar32 s_ch32EmptyStr[1];
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CHTMLSimpleNavigationWrapper : public CPanel2D
{
	DECLARE_PANEL2D( CHTMLSimpleNavigationWrapper, CPanel2D );

public:
	CHTMLSimpleNavigationWrapper( CPanel2D *pParent, const char *pchPanelID );

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

	virtual bool OnGamePadDown( const GamePadData_t &code ) OVERRIDE;
	virtual bool OnGamePadAnalog( const GamePadData_t &code ) OVERRIDE;
	virtual bool OnMouseWheel( const MouseData_t &code ) OVERRIDE;

private:
	void EnsureHTMLPanelReference();

private:
	CHTML *m_pHTML;
	CPanoramaSymbol m_symWrappedHTMLID;
	bool m_bInEventProcessing;
};

} // namespace panorama

#endif // PANORAMA_HTML_H
