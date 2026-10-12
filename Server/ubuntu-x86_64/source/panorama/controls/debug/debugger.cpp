//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define OEMRESOURCE
#include <shellapi.h>
#include <shlwapi.h>
#endif

#include "debugger.h"
#include "panorama/controls/button.h"
#include "panorama/controls/textentry.h"
#include "panorama/iuilayoutmanager.h"
#include "../../controls/debug/debuglayout.h"
#include "../../controls/debug/debugpanelstyle.h"
#include "../../controls/debug/debugpanelparents.h"
#if defined( SOURCE2_PANORAMA )
#include "toolframework2/itoolframework2.h"
#include "assetsystem/iassetsystem.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D( CDebugger, Debugger )
REGISTER_PANEL2D_FACTORY( CVerticalSplitter, VerticalSplitter )
REGISTER_PANEL2D_FACTORY( CHorizontalSplitter, HorizontalSplitter )

DECLARE_PANORAMA_EVENT0( RebuildDebugLayout );
DEFINE_PANORAMA_EVENT( RebuildDebugLayout );

namespace panorama
{
DEFINE_PANORAMA_EVENT( SetDebugTarget );
DEFINE_PANORAMA_EVENT( OpenFileForEdit );
DEFINE_PANORAMA_EVENT( ShowDebugDevInfo );
DEFINE_PANORAMA_EVENT( UpdateJSConsolePriorHistory );
DEFINE_PANORAMA_EVENT( UpdateJSConsoleNextHistory );
DEFINE_PANORAMA_EVENT( PanoramaDebuggerOpened );
DEFINE_PANORAMA_EVENT( PanoramaDebuggerClosed );
}
//-----------------------------------------------------------------------------
// Purpose: Appends style flags (EStyleFlags) to a format string
//-----------------------------------------------------------------------------
extern void AppendStyleFlagsToString( CFmtStr1024 *pfmt, uint unStyleFlags );


//-----------------------------------------------------------------------------
// Purpose: Returns a string identifying the panel
//-----------------------------------------------------------------------------
bool panorama::GetDebugPanelName( char *pchBuffer, uint cubBuffer, IUIPanel *pPanel )
{
	CFmtStr1024 fmt( "%s", pPanel->ClientPtr()->GetPanelType().String() );
	if( pPanel->GetID()[0] != '\0' )
		fmt.AppendFormat( "#%s", pPanel->GetID() );

	FOR_EACH_VEC( pPanel->GetClasses(), iVec )
	{
		fmt.AppendFormat( ".%s", pPanel->GetClasses()[iVec].String() );
	}

	// append style flags but skip inspect
	uint unStyleFlags = pPanel->GetStyleFlags();
	unStyleFlags &= ~k_EStyleFlagInspect;
	AppendStyleFlagsToString( &fmt, unStyleFlags );

	// always try and copy something
	V_strncpy( pchBuffer, fmt.Access(), cubBuffer );
	return (cubBuffer < (uint)fmt.Length() + 1);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDebugger::CDebugger( IUIWindow *pWindow, const char *pchName ) : CPanel2D( pWindow, pchName )
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/debugger.xml" ) );
	m_pInspectButton = (CButton*)FindChildInLayoutFile( "InspectButton" );
	m_pPaintInfoButton = (CButton*)FindChildInLayoutFile( "PaintInfo" );
	m_pSaveButton = (CButton *)FindChildInLayoutFile( "SaveButton" );
	m_pSaveButton->SetEnabled( UIEngine()->UILayoutManager()->BHasFilesInMemory() );
	m_pRevertButton = assert_cast< CButton* >( FindChildInLayoutFile( "RevertButton" ) );
	m_pRevertButton->SetEnabled( UIEngine()->UILayoutManager()->BHasFilesInMemory() );
	m_pDevInfoButton = assert_cast< CToggleButton* >( FindChildInLayoutFile( "DevInfo" ) );
	m_pDebugLayout = assert_cast< CDebugLayout* >( FindPanelInLayoutFile( "DebugLayout" ) );

	m_bAsyncRebuildLayout = false;
	m_bHasInputCapture = false;
	SetAcceptsFocus( true );
	SetFocus();

	m_iLastJSConsoleListIndex = m_listJSConsoleHistory.InvalidIndex();

	RegisterForUnhandledEvent( InMemoryFileUpdate(), this, &CDebugger::OnInMemoryFileUpdate );
	RegisterForUnhandledEvent( InMemoryFilesSaved(), this, &CDebugger::OnInMemoryFilesSaved );
	RegisterForUnhandledEvent( ReloadStyleFile(), this, &CDebugger::OnStyleFileReloaded );
	RegisterForUnhandledEvent( JSConsoleOutput(), this, &CDebugger::OnJSConsoleOutput );
	RegisterEventHandler( SetDebugTarget(), this, &CDebugger::OnBubbledSetDebugTarget );
	RegisterEventHandler( Activated(), this, &CDebugger::OnPanelActivated );
	RegisterEventHandler( OpenFileForEdit(), this, &CDebugger::OnOpenFileForEdit );
	RegisterEventHandler( TextEntrySubmit(), this, &CDebugger::OnTextEntrySubmit );
	RegisterEventHandler( UpdateJSConsolePriorHistory(), this, &CDebugger::OnUpdateJSConsolePriorHistory );
	RegisterEventHandler( UpdateJSConsoleNextHistory(), this, &CDebugger::OnUpdateJSConsoleNextHistory );
	RegisterEventHandler( RebuildDebugLayout(), this, &CDebugger::OnRebuildLayout );

	DispatchEvent( PanoramaDebuggerOpened(), nullptr );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDebugger::~CDebugger()
{
	if ( m_pDebugPanel.Get() )
		m_pDebugPanel->RemoveStyleFlag( k_EStyleFlagInspect );

	ReleaseInputCapture();
	UnregisterForUnhandledEvent( InMemoryFileUpdate(), this, &CDebugger::OnInMemoryFileUpdate );
	UnregisterForUnhandledEvent( InMemoryFilesSaved(), this, &CDebugger::OnInMemoryFilesSaved );
	UnregisterForUnhandledEvent( ReloadStyleFile(), this, &CDebugger::OnStyleFileReloaded );
	UnregisterForUnhandledEvent( JSConsoleOutput(), this, &CDebugger::OnJSConsoleOutput );

	DispatchEvent( PanoramaDebuggerClosed(), nullptr );
}


//-----------------------------------------------------------------------------
// Purpose: Dispatches an event to rebuild layout next frame. Makes sure we only dispatch this once
//-----------------------------------------------------------------------------
void CDebugger::RebuildLayoutAsync()
{
	if ( m_bAsyncRebuildLayout )
		return;

	m_bAsyncRebuildLayout = true;
	DispatchEventAsync( 0.0f, RebuildDebugLayout(), this );
}


//-----------------------------------------------------------------------------
// Purpose: Rebuilds just layout panel
//-----------------------------------------------------------------------------
bool CDebugger::OnRebuildLayout()
{
	m_bAsyncRebuildLayout = false;
	m_pDebugLayout->Build( true );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a panel has been deleted
//-----------------------------------------------------------------------------
void CDebugger::OnPanelAdded( IUIPanel *pPanel )
{	
	if ( m_pDebugLayout->IsPanelOrParentInLayout( ToPanel2D(pPanel) ) )
		RebuildLayoutAsync();
}


//-----------------------------------------------------------------------------
// Purpose: Called when a panel has been deleted
//-----------------------------------------------------------------------------
void CDebugger::OnPanelDeleted( IUIPanel *pPanel, IUIPanel *pOldParent )
{
	bool bChangedDebugPanel = false;
	if ( m_pDebugPanel.Get() == pPanel )
	{
		SetDebugPanel( pOldParent );
		bChangedDebugPanel = true;
	}
	
	bool bInLayout = m_pDebugLayout->IsPanelOrParentInLayout( ToPanel2D(pPanel) ) || m_pDebugLayout->IsPanelOrParentInLayout( ToPanel2D(pOldParent) );
	if ( bChangedDebugPanel || bInLayout )
		RebuildLayoutAsync();
}


//-----------------------------------------------------------------------------
// Purpose: Called when a key is pressed
//-----------------------------------------------------------------------------
bool CDebugger::OnRefresh()
{
	IUIPanel *pOldPanel = m_pDebugPanel.Get();
	SetDebugPanel( NULL );
	SetDebugPanel( pOldPanel );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the panel we are debugging
//-----------------------------------------------------------------------------
void CDebugger::SetDebugPanel( IUIPanel *pPanel, bool bNotifyOtherPanels )
{
	// don't debug the debugger
	if ( pPanel && pPanel->GetParentWindow() == GetParentWindow() )
		pPanel = NULL;
	
	if ( (m_pDebugPanel.BPreviouslySet() && !pPanel) || m_pDebugPanel.Get() != pPanel )
	{
		// set inspect flags
		if ( m_pDebugPanel.Get() )
			m_pDebugPanel->RemoveStyleFlag( k_EStyleFlagInspect );

		m_pDebugPanel = pPanel;

		if ( pPanel )
			pPanel->AddStyleFlag( k_EStyleFlagInspect );
	}

	if ( !pPanel || !m_pDebugPanelLastNotify.BPreviouslySet() || (m_pDebugPanelLastNotify.Get() != pPanel && bNotifyOtherPanels) )
	{
		m_pDebugPanelLastNotify = pPanel;
		DispatchEvent( SetDebugTarget(), nullptr, ToPanel2D(m_pDebugPanel.Get()) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when a panel (this or child) is clicked
//-----------------------------------------------------------------------------
bool CDebugger::OnPanelActivated( const CPanelPtr< IUIPanel > &ptrPanel, EPanelEventSource_t eSource )
{
	CPanel2D *pPanel = ToPanel2D( ptrPanel.Get() );

	// inspect panel was clicked
	if ( pPanel == m_pInspectButton )
	{
		BeginInspect();
		return true;
	}

	// paint info button
	if( pPanel == m_pPaintInfoButton )
	{
		UIEngine()->SetPaintCountTrackingEnabled( !UIEngine()->GetPaintCountTrackingEnabled() );
	}

	if ( pPanel == m_pSaveButton )
		UIEngine()->UILayoutManager()->SaveInMemoryFiles();

	if ( pPanel == m_pRevertButton )
		UIEngine()->UILayoutManager()->RevertInMemoryFiles();

	if ( pPanel == m_pDevInfoButton )
		DispatchEvent( ShowDebugDevInfo(), this, m_pDevInfoButton->IsSelected() );

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when we are capturing input and the mouse moves
//-----------------------------------------------------------------------------
bool CDebugger::OnCapturedMouseHover( IUIPanel *pPanel )
{
	// update panel we are debugging.. don't tell others
	SetDebugPanel( pPanel, false );
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when we are capturing input and the mouse button is pressed
//-----------------------------------------------------------------------------
bool CDebugger::OnCapturedMouseButtonDown( IUIPanel *pPanel, const MouseData_t &code )
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when we are capturing input and the mouse button is released
//-----------------------------------------------------------------------------
bool CDebugger::OnCapturedMouseButtonUp( IUIPanel *pPanel, const MouseData_t &code )
{
	// pPanel will point to the panel which was clicked. However, the user could have clicked on a child panel that doesn't accept input.
	// if we are currently debugging the hovered panel & it is a child of the clicked panel, keep debugging that panel
	if ( pPanel )
	{
		IUIPanel *pHoverPanel = pPanel->GetParentWindow()->UIWindowInput()->GetMouseHover();
		if ( pHoverPanel && m_pDebugPanel.Get() == pHoverPanel )
			pPanel = pHoverPanel;
	}

	// pPanel could be null. If so, just leave the debugger on whatever we were previously debugging
	if ( !pPanel )
		pPanel = m_pDebugPanel.Get();

	// stop capturing input
	SetDebugPanel( pPanel );
	ReleaseInputCapture();
	GetParentWindow()->Activate( false );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when we are capturing input and the mouse button is double clicked
//-----------------------------------------------------------------------------
bool CDebugger::OnCapturedMouseButtonDoubleClick( IUIPanel *pPanel, const MouseData_t &code )
{
	// stop capturing input
	SetDebugPanel( pPanel );
	ReleaseInputCapture();
	GetParentWindow()->Activate( false );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when we are capturing input and the mouse button is double clicked
//-----------------------------------------------------------------------------
bool CDebugger::OnCapturedMouseButtonTripleClick( IUIPanel *pPanel, const MouseData_t &code )
{
	// stop capturing input
	SetDebugPanel( pPanel );
	ReleaseInputCapture();
	GetParentWindow()->Activate( false );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when we are capturing input and the mouse wheel is used
//-----------------------------------------------------------------------------
bool CDebugger::OnCapturedMouseWheel( IUIPanel *pPanel, const MouseData_t &code )
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Captures input
//-----------------------------------------------------------------------------
void CDebugger::BeginInspect()
{
	if ( !m_bHasInputCapture )
	{
		SetDebugPanel( NULL );
		UIEngine()->UIInputEngine()->SetInputCapture( this );
		UIEngine()->UIInputEngine()->SetDebugHitTesting( true );
		m_bHasInputCapture = true;
		DispatchEvent( ActivateMainWindow(), nullptr, false );

		// now that the main window should have focus.. set the hovered panel to focused
		CUtlVector< panorama::IUIWindow* > vecWindows;
		UIEngine()->GetWindowsForDebugger( vecWindows );
		for ( IUIWindow *pWindow : vecWindows )
		{
			if ( pWindow->BIsVisible() )
			{
				IUIPanel *pHover = pWindow->UIWindowInput()->GetMouseHover();
				if ( pHover )
				{
					SetDebugPanel( pHover );
				}
				else if ( pWindow->GetNumVisibleTopLevelPanels() )
				{
					SetDebugPanel( pWindow->GetTopLevelVisiblePanels().Element( 0 ) );
				}
				break;
			}
		}
	}
}


void CDebugger::ForceEndInspect()
{
	ReleaseInputCapture();
}


//-----------------------------------------------------------------------------
// Purpose: Releases input capture
//-----------------------------------------------------------------------------
void CDebugger::ReleaseInputCapture()
{
	if ( m_bHasInputCapture )
	{
		UIEngine()->UIInputEngine()->ReleaseInputCapture( this );
		UIEngine()->UIInputEngine()->SetDebugHitTesting( false );
		m_bHasInputCapture = false;
	}	
}


//-----------------------------------------------------------------------------
// Purpose: Called when one of our children requested a new debug target
//-----------------------------------------------------------------------------
bool CDebugger::OnBubbledSetDebugTarget( CPanelPtr< CPanel2D > pPanel )
{
	SetDebugPanel( pPanel.Get() ? pPanel->UIPanel() : NULL );

	// kill bubbling. We will re-raise this event to unhandled listeners
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles OpenFileForEdit event
//-----------------------------------------------------------------------------
bool CDebugger::OnOpenFileForEdit( const char *pchFile, uint unLine )
{
#if defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_USE_S1WRAPPER)
	if ( g_pAssetSystem )
	{
		IAsset *pFileAsset = g_pAssetSystem->FindAssetByFilename( pchFile );
		if ( pFileAsset )
		{
			CUtlVector<IAsset *> vecAssets;
			vecAssets.AddToTail( pFileAsset );
			g_pAssetSystem->OpenForEdit( vecAssets );
			g_pToolFramework2->OpenInPrimaryTool( vecAssets );
		}
	}
#else
	CUtlString strConverted;
	if ( V_strnicmp( pchFile, "http://", 7) == 0 || V_strnicmp( pchFile, "https://", 8 ) == 0 )
	{
		if ( UIEngine()->UILayoutManager()->BConvertHTTPPathToLocalP4Path( pchFile, strConverted ) )
		{
			pchFile = strConverted.String();
		}
	}


#ifdef WIN32
	
	// open in associated editor
	char szFullFileName[ MAX_PATH ];
	char szAppName[ MAX_PATH ];
	g_pFullFileSystem->RelativePathToFullPath( pchFile, "GAME", szFullFileName, sizeof( szFullFileName ) );
	DWORD nBufSize = sizeof( szAppName );
	HRESULT hr = ::AssocQueryStringA( 0, ASSOCSTR_EXECUTABLE, szFullFileName, NULL, szAppName, &nBufSize );
	if ( !FAILED( hr ) )
	{
		::ShellExecuteA( NULL, NULL, szAppName, szFullFileName, ".", SW_SHOWNORMAL );
	}

#elif defined(POSIX)
	uint procID = ThreadShellExecute( "p4", CFmtStr( "edit %s", pchFile ), "." );
	int nTries = 100;
	while ( procID && ThreadIsProcessIdActive( procID ) && --nTries > 0 )
		ThreadSleep( 10 );

	// open in an editor
	ThreadShellExecute( IsLinux() ? "xdg-open" : "open", pchFile, "." );
#else
#warning "Implement p4 + open logic"
#endif
#endif // SOURCE2_PANORAMA
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a style or layout file is updated in memory
//-----------------------------------------------------------------------------
bool CDebugger::OnInMemoryFileUpdate( CPanoramaSymbol symFile, uint location, uint unOldSize, uint unNewSize )
{
	m_pSaveButton->SetEnabled( true );
	m_pRevertButton->SetEnabled( true );
	
	// let bubble
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a style or layout file is updated in memory
//-----------------------------------------------------------------------------
bool CDebugger::OnInMemoryFilesSaved()
{
	// check if any files are still open for edit, in case only some of the file were saved
	m_pSaveButton->SetEnabled( UIEngine()->UILayoutManager()->BHasFilesInMemory() );

	// let bubble
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a style file has been reloaded from disk
//-----------------------------------------------------------------------------
bool CDebugger::OnStyleFileReloaded( CPanoramaSymbol symFile )
{
	m_pSaveButton->SetEnabled( UIEngine()->UILayoutManager()->BHasFilesInMemory() );
	m_pRevertButton->SetEnabled( UIEngine()->UILayoutManager()->BHasFilesInMemory() );

	// let bubble
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Gets the splitter position. If an error occured, returns 0
//-----------------------------------------------------------------------------
float CDebugger::GetSplitterPosition()
{
	// we assume the width was parent flow
	CUILength width;
	m_pDebugLayout->AccessStyle()->GetWidth( width );
	if ( width.GetType() == CUILength::k_EUILengthFillParentFlow )
		return width.GetValue();

	return 0.0f;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the splitter position. Value will be used as a ui length fill parent flow
//-----------------------------------------------------------------------------
void CDebugger::SetSplitterPosition( float flParentFlowValue )
{
	CPanel2D *pDebugPanel = FindPanelInLayoutFile( "DebugPanel" );
	if ( !pDebugPanel )
		return;

	if ( flParentFlowValue < 0.1f )
		return;

	float flValue = clamp( flParentFlowValue, 0.1f, 0.9f );
	m_pDebugLayout->AccessStyle()->SetWidth( CUILength( flValue, CUILength::k_EUILengthFillParentFlow ) );
	pDebugPanel->AccessStyle()->SetWidth( CUILength( 1 - flValue, CUILength::k_EUILengthFillParentFlow ) );
}


//-----------------------------------------------------------------------------
// Purpose: Output text to JS console
//-----------------------------------------------------------------------------
bool CDebugger::OnJSConsoleOutput( CPanelPtr< IUIPanel > pPanel, const char *pchText )
{
	// bugbug jmccaskey - check context
	CLabel *pConsole = assert_cast<CLabel*>(FindChildTraverse( "JSConsoleText" ));
	pConsole->SetMaxChars( panorama::k_eStringTruncationStyle_Front, 30000 );
	
	IUIPanel *pPanelActual = pPanel.Get();
	if( m_pLastJSContext.Get() != pPanelActual )
	{
		if( pPanelActual )
			pConsole->AppendText( CFmtStr1024( ":context->%p->%s:\n", pPanel.Get(), pPanel.Get() ? pPanel->GetLayoutFile().String() : "-" ).String() );
		else
			pConsole->AppendText( ":context->null->UIEngine Global:\n" );

		m_pLastJSContext = pPanel.Get();
	}

	pConsole->AppendText( pchText );
	pConsole->AppendText( "\n" );
	pConsole->ScrollToBottom();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: JS console text entry submit handler
//-----------------------------------------------------------------------------
bool CDebugger::OnTextEntrySubmit( const CPanelPtr< IUIPanel > &pPanel, const char *pchText )
{
	if( pPanel->GetID() && V_stricmp( pPanel->GetID(), "JSConsoleInput" ) == 0 )
	{
		IUIPanel *pContextPanel = m_pDebugPanel.Get();
		if( pContextPanel )
			pContextPanel = pContextPanel->GetJavaScriptContextParent();

		// Can't run, no context...
		if( !pContextPanel )
		{
			OnJSConsoleOutput( (IUIPanel*)NULL, "!! No context to execute within." );
			return true;
		}

		OnJSConsoleOutput( pContextPanel, pchText );

		UIEngine()->RunScript( pContextPanel, pchText, "JSConsoleInput", 0, 0, true, false );

		CTextEntry *pTextEntry = assert_cast<CTextEntry *>(ToPanel2D(pPanel.Get()));
		pTextEntry->SetText( "" );

		m_listJSConsoleHistory.AddToTail( pchText );
		while( m_listJSConsoleHistory.Count() > 100 )
			m_listJSConsoleHistory.Remove( m_listJSConsoleHistory.Head() );

		m_iLastJSConsoleListIndex = m_listJSConsoleHistory.InvalidIndex();

		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Command history
//-----------------------------------------------------------------------------
bool CDebugger::OnUpdateJSConsolePriorHistory()
{
	int iNew = m_listJSConsoleHistory.InvalidIndex();
	if( m_iLastJSConsoleListIndex == m_listJSConsoleHistory.InvalidIndex() )
		iNew = m_listJSConsoleHistory.Tail();
	else
		iNew = m_listJSConsoleHistory.Previous( m_iLastJSConsoleListIndex );

	if( iNew != m_listJSConsoleHistory.InvalidIndex() )
		m_iLastJSConsoleListIndex = iNew;

	CTextEntry *pTextEntry = assert_cast<CTextEntry *>(FindChildTraverse("JSConsoleInput"));

	if( m_iLastJSConsoleListIndex == m_listJSConsoleHistory.InvalidIndex() )
	{
		pTextEntry->SetText( "" );
	}
	else
	{
		pTextEntry->SetText( m_listJSConsoleHistory.Element( m_iLastJSConsoleListIndex ).String() );
		pTextEntry->SetCursorOffset( pTextEntry->GetCharCount() );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Command history
//-----------------------------------------------------------------------------
bool CDebugger::OnUpdateJSConsoleNextHistory()
{
	if( m_iLastJSConsoleListIndex == m_listJSConsoleHistory.InvalidIndex() )
		m_iLastJSConsoleListIndex = m_listJSConsoleHistory.InvalidIndex();
	else
		m_iLastJSConsoleListIndex = m_listJSConsoleHistory.Next( m_iLastJSConsoleListIndex );

	CTextEntry *pTextEntry = assert_cast<CTextEntry *>(FindChildTraverse( "JSConsoleInput" ));
	if( m_iLastJSConsoleListIndex == m_listJSConsoleHistory.InvalidIndex() )
	{
		pTextEntry->SetText( "" );
	}
	else
	{
		pTextEntry->SetText( m_listJSConsoleHistory.Element( m_iLastJSConsoleListIndex ).String() );
		pTextEntry->SetCursorOffset( pTextEntry->GetCharCount()  );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVerticalSplitter::CVerticalSplitter( CPanel2D *pParent, const char *pchName ) : BaseClass( pParent, pchName )
{
	SetAcceptsInput( true );
	SetMouseTracking( true );
	m_bMoving = false;
	m_flLastUpdate = 0.0f;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CVerticalSplitter::~CVerticalSplitter()
{

}


//-----------------------------------------------------------------------------
// Purpose: Called when mouse button goes down
//-----------------------------------------------------------------------------
bool CVerticalSplitter::OnMouseButtonDown( const MouseData_t &code )
{
	if ( code.m_MouseCode == MOUSE_LEFT )
		m_bMoving = true;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when mouse button is released
//-----------------------------------------------------------------------------
bool CVerticalSplitter::OnMouseButtonUp( const MouseData_t &code )
{
	if ( code.m_MouseCode == MOUSE_LEFT )
		m_bMoving = false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when mouse moves
//-----------------------------------------------------------------------------
void CVerticalSplitter::OnMouseMove( float flMouseX, float flMouseY )
{
	if ( !m_bMoving )
		return;

	// need a parent
	CPanel2D *pParent = GetParent();
	if ( !pParent )
	{
		AssertMsg( false, "Splitter needs a parent panel" );
		return;
	}

	if( flMouseX > -3.0f && flMouseX < GetActualLayoutWidth() + 3.0f )
		return;

	if( UIEngine()->GetCurrentFrameTime() - m_flLastUpdate < 0.05f )
		return;

	// get left and right panels
	int iSplitter = pParent->GetChildIndex( this );
	Assert( iSplitter == 1 && pParent->GetChildCount() == 3 );
	CPanel2D *pLHS = pParent->GetChild( iSplitter - 1 );
	CPanel2D *pRHS = pParent->GetChild( iSplitter + 1 );
	
	// get parent dimensions
	float flParentWidth = pParent->GetActualLayoutWidth();
	float flParentHeight = pParent->GetActualLayoutHeight();

	float flLeft, flTop, flRight, flBottom;
	pParent->AccessStyle()->GetContentInset( flParentWidth, flParentHeight, false, flLeft, flTop, flRight, flBottom );
	flParentWidth -= flLeft - flRight;
	flParentHeight -= flTop - flBottom;

	// get our current position
	float xPos = GetActualXOffset() + GetContentsXScrollOffset();

	// bugbug - include margins
	float flWidthLHS = xPos + flMouseX - (GetActualRenderWidth() / 2.0f);

	float flParentWidthMinusSplitter = flParentWidth - GetActualLayoutWidth();
	float flPercentLHS = flWidthLHS / flParentWidthMinusSplitter;
	flPercentLHS = clamp( flPercentLHS, 0.1f, 0.9f );

	pLHS->AccessStyle()->SetWidth( CUILength( flPercentLHS, CUILength::k_EUILengthFillParentFlow ) );
	pRHS->AccessStyle()->SetWidth( CUILength( 1 - flPercentLHS, CUILength::k_EUILengthFillParentFlow ) );

	m_flLastUpdate = UIEngine()->GetCurrentFrameTime();
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHorizontalSplitter::CHorizontalSplitter( CPanel2D *pParent, const char *pchName ) : BaseClass( pParent, pchName )
{
	SetAcceptsInput( true );
	SetMouseTracking( true );
	m_bMoving = false;
	m_flLastUpdate = 0.0f;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CHorizontalSplitter::~CHorizontalSplitter()
{

}


//-----------------------------------------------------------------------------
// Purpose: Called when mouse button goes down
//-----------------------------------------------------------------------------
bool CHorizontalSplitter::OnMouseButtonDown( const MouseData_t &code )
{
	if( code.m_MouseCode == MOUSE_LEFT )
		m_bMoving = true;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when mouse button is released
//-----------------------------------------------------------------------------
bool CHorizontalSplitter::OnMouseButtonUp( const MouseData_t &code )
{
	if( code.m_MouseCode == MOUSE_LEFT )
		m_bMoving = false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when mouse moves
//-----------------------------------------------------------------------------
void CHorizontalSplitter::OnMouseMove( float flMouseX, float flMouseY )
{
	if( !m_bMoving )
		return;

	// need a parent
	CPanel2D *pParent = GetParent();
	if( !pParent )
	{
		AssertMsg( false, "Splitter needs a parent panel" );
		return;
	}

	if( flMouseY > -3.0f && flMouseY < GetActualLayoutHeight() + 3.0f )
		return;

	if( UIEngine()->GetCurrentFrameTime() - m_flLastUpdate < 0.05f )
		return;

	// get left and right panels
	int iSplitter = pParent->GetChildIndex( this );
	Assert( iSplitter == 1 && pParent->GetChildCount() == 3 );
	CPanel2D *pLHS = pParent->GetChild( iSplitter - 1 );
	CPanel2D *pRHS = pParent->GetChild( iSplitter + 1 );

	// get parent dimensions
	float flParentWidth = pParent->GetActualLayoutWidth();
	float flParentHeight = pParent->GetActualLayoutHeight();

	float flLeft, flTop, flRight, flBottom;
	pParent->AccessStyle()->GetContentInset( flParentWidth, flParentHeight, false, flLeft, flTop, flRight, flBottom );
	flParentWidth -= flLeft - flRight;
	flParentHeight -= flTop - flBottom;

	// get our current position
	float yPos = GetActualYOffset() + GetContentsYScrollOffset();

	// bugbug - include margins
	float flHeightLHS = yPos + flMouseY - (GetActualRenderHeight() / 2.0f);

	float flParentHeightMinusSplitter = flParentHeight - GetActualLayoutHeight();
	float flPercentLHS = flHeightLHS / flParentHeightMinusSplitter;
	flPercentLHS = clamp( flPercentLHS, 0.1f, 0.9f );

	pLHS->AccessStyle()->SetHeight( CUILength( flPercentLHS, CUILength::k_EUILengthFillParentFlow ) );
	pRHS->AccessStyle()->SetHeight( CUILength( 1 - flPercentLHS, CUILength::k_EUILengthFillParentFlow ) );

	m_flLastUpdate = UIEngine()->GetCurrentFrameTime();
}

#ifdef DBGFLAG_VALIDATE
void CDebugger::Validate( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();

	ValidateObj( m_listJSConsoleHistory );
}
#endif
