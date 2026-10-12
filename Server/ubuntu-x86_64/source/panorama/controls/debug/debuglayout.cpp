//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "debuglayout.h"
#include "panorama/controls/label.h"
#include "panorama/controls/button.h"
#include "../../controls/debug/debugger.h"

#ifdef WIN32
#include "winlite.h"
#endif

#if defined( SOURCE2_PANORAMA )
#include "pcre/pcrecpp.h"
#else
#include <pcrecpp.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CDebugLayout, DebugLayout )

const char *k_pchDebugLayoutElementText = "DebugLayoutElementText";
const char *k_pchDebugLayoutElementPropName = "DebugLayoutElementPropName";
const char *k_pchDebugLayoutElementPropValue = "DebugLayoutElementPropValue";
const char *k_pchDebugLayoutPanelRowSelected = "DebugLayoutPanelRowSelected";
const char *k_pchDebugLayoutPanelRowPainted = "DebugLayoutPanelRowPainted";
static const float k_flTopLevelIndent = 15.0f;

DECLARE_PANORAMA_EVENT0( UpdateDirtyPanelInfo );
DEFINE_PANORAMA_EVENT( UpdateDirtyPanelInfo );

DECLARE_PANORAMA_EVENT0( UpdatePanelPaintInfo );
DEFINE_PANORAMA_EVENT( UpdatePanelPaintInfo );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDebugLayout::CDebugLayout( CPanel2D *pParent, const char *pchName ) : CPanel2D( pParent, pchName )
{
	m_bScrollToDebugPanel = false;
	m_bShowDevInfo = false;

	RequireLoadLayout( "file://{resources}/layout/debuglayout.xml" );

	SetAcceptsInput( true );

	RegisterForUnhandledEvent( SetDebugTarget(), this, &CDebugLayout::EventSetDebugTarget );
	RegisterForUnhandledEvent( StyleClassesChanged(), this, &CDebugLayout::EventPanelStyleClassesChanged );
	RegisterForUnhandledEvent( StyleFlagsChanged(), this, &CDebugLayout::EventPanelStyleFlagsChanged );
	RegisterForUnhandledEvent( ShowDebugDevInfo(), this, &CDebugLayout::EventShowDebugDevInfo );
	RegisterEventHandler( BrowserGoToURL(), this, &CDebugLayout::EventBrowserGoToURL );

	RegisterEventHandler( Activated(), this, &CDebugLayout::EventPanelActivated );
	RegisterEventHandler( UpdateDirtyPanelInfo(), this, &CDebugLayout::EventUpdateDirtyPanelInfo );

	RegisterEventHandler( UpdatePanelPaintInfo(), this, &CDebugLayout::EventUpdatePanelPaintInfo );

	DispatchEventAsync( 0.01f, UpdatePanelPaintInfo(), this );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDebugLayout::~CDebugLayout()
{
	UnregisterForUnhandledEvent( SetDebugTarget(), this, &CDebugLayout::EventSetDebugTarget );
	UnregisterForUnhandledEvent( StyleClassesChanged(), this, &CDebugLayout::EventPanelStyleClassesChanged );
	UnregisterForUnhandledEvent( StyleFlagsChanged(), this, &CDebugLayout::EventPanelStyleFlagsChanged );
	UnregisterForUnhandledEvent( ShowDebugDevInfo(), this, &CDebugLayout::EventShowDebugDevInfo );
}


//-----------------------------------------------------------------------------
// Purpose: Clears layout (not other members like panel we are debugging)
//-----------------------------------------------------------------------------
void CDebugLayout::ClearLayout()
{	
	RemoveAndDeleteChildren();
	m_mapPanelDebugInfo.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Handle opening URL
//-----------------------------------------------------------------------------
bool CDebugLayout::EventBrowserGoToURL( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, const char *pchURL )
{
	CFileResource fileResource( pchURL );
#if defined( WIN32 ) && !defined( SOURCE2_PANORAMA )
	::ShellExecuteA( NULL, "open", fileResource.GetReferencePath(), NULL, NULL, SW_SHOWNORMAL);
#else
#if defined( PLATFORM_OSX ) || defined( OSX )
	const char *pchOpenCommand = "/usr/bin/open";
#elif defined( PLATFORM_LINUX ) || defined( LINUX )
	const char *pchOpenCommand = "/usr/bin/xdg-open";
#elif defined( PLATFORM_WINDOWS ) || defined( WIN32 )
	const char *pchOpenCommand = "start";
#else
#error
#endif
	ThreadShellExecute( pchOpenCommand, fileResource.GetReferencePath(), getenv( "HOME" ) );
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Update panel paint info
//-----------------------------------------------------------------------------
bool CDebugLayout::EventUpdatePanelPaintInfo()
{
	FOR_EACH_HASHMAP( m_mapPanelDebugInfo, i )
	{
		PanelDebugInfo_t &info = m_mapPanelDebugInfo[i];

		uint64 ulPanelPtrValue = info.m_pPanel.GetHandleAsUInt64();

		uint32 unMaxPanelPaintCount;
		uint32 unPaintCount;
		bool bCompositionLayer;
		double flLastFrameRepaint;
		UIEngine()->GetPanelPaintInfo( ulPanelPtrValue, unMaxPanelPaintCount, unPaintCount, bCompositionLayer, flLastFrameRepaint );

		if( flLastFrameRepaint >= UIEngine()->GetCurrentFrameTime() - 0.5f )
		{
			float r, g, b, a;

			// Highlight panels that are drawing more than others.
			// Many panels draw all of the time so this often
			// doesn't produce much of a range of colors.
			float flPaintIntensity = (float)unPaintCount / (float)unMaxPanelPaintCount;

			if( !bCompositionLayer )
			{
				r = Lerp( flPaintIntensity, 255, 169 );
				g = 252;
				b = Lerp( flPaintIntensity, 255, 198 );
				a = 255;
			}
			else
			{
				r = 255;
				g = 252;
				b = Lerp( flPaintIntensity, 255, 140 );
				a = 255;
			}

			float flSincePaint = UIEngine()->GetCurrentFrameTime() - flLastFrameRepaint;
			float flProgress = flSincePaint / 0.5;
			a = Lerp( flProgress, a, 0.0f );

			if( flProgress < 1.0f )
				info.m_pOpen->AccessStyle()->SetBackgroundColor( CFmtStr32( "#%02X%02X%02x%02X", (int)r, (int)g, (int)b, (int)a ).String() );
			else
				info.m_pOpen->AccessStyle()->ClearPropertySetFromElement( "background-color" );
		}
		else
		{
			info.m_pOpen->AccessStyle()->ClearPropertySetFromElement( "background-color" );
		}
	}

	DispatchEventAsync( 0.01f, UpdatePanelPaintInfo(), this );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the width of a tab using the styles of the specified label
//-----------------------------------------------------------------------------
float GetTabWidth( CLabel *pLabel )
{
	const char * pchFontFamily = NULL;
	float flFontSize;
	EFontWeight eWeight;
	EFontStyle eStyle;
	pLabel->AccessStyle()->GetFontStyle( &pchFontFamily, flFontSize, eStyle, eWeight );
	ETextAlign eAlign;
	pLabel->AccessStyle()->GetTextAlign( eAlign );
	int nLetterSpacing;
	pLabel->AccessStyle()->GetTextLetterSpacing( nLetterSpacing );

	float flLineHeight;
	pLabel->AccessStyle()->GetLineHeight( flLineHeight );

	IUITextLayout *pLayout = UIEngine()->CreateTextLayout( "W", pchFontFamily, flFontSize, flLineHeight, eWeight, eStyle, eAlign, false, false, nLetterSpacing, 1000.0f, 1000.0f );
	if ( !pLayout )
		return 0.0f;
	
	float flDesiredWidth, flDesiredHeight;
	pLayout->GetRequiredSize( flDesiredWidth, flDesiredHeight );
	flDesiredWidth = ceil( flDesiredWidth );
	UIEngine()->FreeTextLayout( pLayout );
	
	return flDesiredWidth;
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the panel depth (how many ancestors) for this panel
//-----------------------------------------------------------------------------
int GetPanelDepth( CPanel2D *pPanel )
{
	Assert( pPanel );

	int nDepth = 0;
	for ( CPanel2D *p = pPanel->GetParent(); p != NULL; p = p->GetParent() )
		nDepth++;

	return nDepth;	
}


//-----------------------------------------------------------------------------
// Purpose: Helper for AppendElementOpenTag()
//-----------------------------------------------------------------------------
struct OpenTagColors_t
{
	OpenTagColors_t( int iStart, int iEnd, CPanoramaSymbol symStyle )
	{
		m_iStart = iStart;
		m_iEnd = iEnd;
		m_symStyle = symStyle;
	}

	int m_iStart;
	int m_iEnd;
	CPanoramaSymbol m_symStyle;
};


//-----------------------------------------------------------------------------
// Purpose: Update dirty panel info
//-----------------------------------------------------------------------------
bool CDebugLayout::EventUpdateDirtyPanelInfo()
{
	FOR_EACH_RBTREE_FAST( m_treePanelsDirty, i )
	{
		CPanel2D *pPanel = ToPanel2D( m_treePanelsDirty.Element( i ).Get() );
		if( pPanel )
			UpdatePanelInfo( pPanel );
	}
	m_treePanelsDirty.Purge();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Appends panels for the element open tag
// Returns: < for measurement of indent width
//-----------------------------------------------------------------------------
void CDebugLayout::AppendElementOpenTag( CLabel *pLabel, CPanel2D *pPanel, bool bShowChildren )
{
	VPROF_BUDGET( "CDebugLayout::AppendElementOpenTag", VPROF_BUDGETGROUP_TENFOOT );

	static pcrecpp::RE_Options reOptions( PCRE_CASELESS );
	static pcrecpp::RE reWWWLong( "((https?|ftp|file)://|www\\.)([^<]*)", reOptions );

	// could build HTML using spans to set classes.. though requires reparsing by label. Hopefully this is a bit faster (we make lots of open tags)	
	static CUtlStringBuilder strBuilder;
	strBuilder.Clear();
	strBuilder.EnsureCapacity( 4096 );

	// get property info
	CUtlVector< DebugPropertyOutput_t * > vecProperties( 0, 12 );
	{

		VPROF_BUDGET( "CDebugLayout::AppendElementOpenTag - GetDebugPropertyInfo", VPROF_BUDGETGROUP_TENFOOT );
		pPanel->GetDebugPropertyInfo( &vecProperties );
	}
	
	// start (will use braces color as default style)
	strBuilder.Append( "&lt;" );

	char rgchEncoded[1024];
	V_BasicHtmlEntityEncode( rgchEncoded, V_ARRAYSIZE( rgchEncoded ), pPanel->GetPanelType().String(), V_strlen( pPanel->GetPanelType().String() ) );
	strBuilder.AppendFormat( "<span class=\"%s\">%s</span>", k_pchDebugLayoutElementText, rgchEncoded );

	// if showing dev info, add address in memory
	if ( m_bShowDevInfo )
	{
		strBuilder.AppendFormat( "<span class=\"%s\"> address=\"</span>", k_pchDebugLayoutElementPropName );
		strBuilder.AppendFormat( "<span class=\"%s\">%p</span>", k_pchDebugLayoutElementPropValue, pPanel);
		strBuilder.AppendFormat( "<span class=\"%s\">\"</span>", k_pchDebugLayoutElementPropName );

		strBuilder.AppendFormat( "<span class=\"%s\"> styleflags=\"</span>", k_pchDebugLayoutElementPropName );
		strBuilder.AppendFormat( "<span class=\"%s\">0x%x</span>", k_pchDebugLayoutElementPropValue, pPanel->GetStyleFlags());
		strBuilder.AppendFormat( "<span class=\"%s\">\"</span>", k_pchDebugLayoutElementPropName );
	}
	

	// add each property
	{
		VPROF_BUDGET( "CDebugLayout::AppendElementOpenTag - add properties", VPROF_BUDGETGROUP_TENFOOT );
		FOR_EACH_VEC( vecProperties, i )
		{
			DebugPropertyOutput_t *pProperty = vecProperties[i];

			V_BasicHtmlEntityEncode( rgchEncoded, V_ARRAYSIZE( rgchEncoded ), pProperty->m_strName.String(), pProperty->m_strName.Length() );
			strBuilder.AppendFormat( "<span class=\"%s\"> %s=\"</span>", k_pchDebugLayoutElementPropName, rgchEncoded );

			V_BasicHtmlEntityEncode( rgchEncoded, V_ARRAYSIZE( rgchEncoded ), pProperty->m_strValue.String(), pProperty->m_strValue.Length() );
			strBuilder.AppendFormat( "<span class=\"%s\">%s</span>", k_pchDebugLayoutElementPropValue, rgchEncoded );

			V_BasicHtmlEntityEncode( rgchEncoded, V_ARRAYSIZE( rgchEncoded ), pProperty->m_strName.String(), pProperty->m_strName.Length() );
			strBuilder.AppendFormat( "<span class=\"%s\">\"</span>", k_pchDebugLayoutElementPropName );
		}
		vecProperties.PurgeAndDeleteElements();
	}

	// close
	strBuilder.Append( bShowChildren ? "&gt;" : " /&gt;" );
	std::string strChat = strBuilder.String();

	reWWWLong.GlobalReplace( "<a href=\"\\1\\3\">\\1\\3</a>", &strChat );

	// make new label
	pLabel->SetText( strChat.c_str(), panorama::CLabel::k_ETextTypeHTML );
}


//-----------------------------------------------------------------------------
// Purpose: Appends panels for the element close tag
//-----------------------------------------------------------------------------
CPanel2D *CDebugLayout::AppendElementCloseRow( CPanel2D *pPanel, int nDepth )
{
	CPanel2D *pRowClose = new CPanel2D( this, NULL );
	pRowClose->RequireLoadLayoutSnippet( "PanelClose" );

	int cchPanel = V_strlen( pPanel->GetPanelType().String() );
	CLabel *pElement = assert_cast< CLabel * >( pRowClose->RequireChildInLayoutFile( "DebugLayoutPanelClose" ) );
	pElement->SetText( CFmtStr( "</%s>", pPanel->GetPanelType().String() ) );
	pElement->SetStyleForRange( 2, cchPanel + 1, k_pchDebugLayoutElementText );

	float flIndentWidth = pPanel->GetParent() ? GetTabWidth( pElement ) * 3 * nDepth : k_flTopLevelIndent;
	CUILength lenIndent( flIndentWidth, CUILength::k_EUILengthLength );
	CPanel2D *pRowIndent = pRowClose->RequireChildInLayoutFile( "Indent" );
	pRowIndent->AccessStyleDirty()->SetWidth( lenIndent );

	return pRowClose;
}


//-----------------------------------------------------------------------------
// Purpose: Adds panels for layout hierarchy to the specified panel
//-----------------------------------------------------------------------------
void CDebugLayout::AddLayoutForPanel( CPanel2D *pPanel )
{
	// build list of panel and its parents
	CUtlVector< CPanel2D * > vecPanels;
	for ( CPanel2D *p = pPanel; p != NULL; p = p->GetParent() )
		vecPanels.AddToTail( p );

	// start from top and add/expand to panel. Need to iterate backward as previous loop started at child
	FOR_EACH_VEC_BACK( vecPanels, i )
	{
		CPanel2D *pCurrent = vecPanels[i];
		PanelDebugInfo_t *pDebugInfo = FindDebugInfo( pCurrent );

		// when empty, will have to add top most panel
		if ( !pDebugInfo && i == vecPanels.Count() - 1 )
		{
			Assert( !pCurrent->GetParent() );
			pDebugInfo = AddCollapsedPanelToLayout( pCurrent, NULL, 0 );
		}

		if ( !pDebugInfo )
		{
			AssertMsg( false, "AddLayoutForPanelFailed to find debug info for a panel that should exist" );
			return;
		}

		// don't expand the target
		if ( i == 0 )
			continue;
		
		ExpandOrCollapse( pCurrent, true );

		// ExpandOrCollapse could have resized the debug info, so throw our pointer away
		pDebugInfo = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Appends panel layout info to provided string
//-----------------------------------------------------------------------------
CDebugLayout::PanelDebugInfo_t *CDebugLayout::AddCollapsedPanelToLayout( CPanel2D *pPanel, CPanel2D *pAfter, int nDepth )
{
	Assert( pPanel );
	Assert( !pAfter || nDepth > 0 );

	if ( FindDebugInfo( pPanel ) )
	{
		AssertMsg( false, "Trying to add duplicate panel to the debugger layout" );
		return NULL;
	}
	
	// create a new row and move it into place
	CPanel2D *pRowOpen = new CPanel2D( this, NULL );
	pRowOpen->RequireLoadLayoutSnippet( "PanelStart" );
	if ( pAfter )
		MoveChildAfter( pRowOpen, pAfter );

	// each row starts with indent
	CPanel2D *pRowIndent = pRowOpen->RequireChildInLayoutFile( "Indent" );

	// if ourselves (debugging the debugger), also need to return here. We are currently adding child panels, and will infinitely recurse
	bool bShowChildren = ( ( pPanel->GetChildCount() != 0 || pPanel->GetHiddenChildCount() != 0 ) && pPanel != this );
	CLabel *pOpenText = assert_cast< CLabel * >( pRowOpen->RequireChildInLayoutFile( "DebugLayoutPanelOpen" ) );
	AppendElementOpenTag( pOpenText, pPanel, bShowChildren );

	// set indent on button based on depth
	float flIndentWidth = pPanel->GetParent() ? GetTabWidth( pOpenText ) * 3 * nDepth : k_flTopLevelIndent;
	CUILength lenIndent( flIndentWidth, CUILength::k_EUILengthLength );
	pRowIndent->AccessStyleDirty()->SetWidth( lenIndent );

	// create a link to the style file
	CLabel *pStyleLink = assert_cast< CLabel * >( pRowOpen->RequireChildInLayoutFile( "StyleLink" ) );
	if ( pPanel->GetLayoutFileLoadedFrom().IsValid() )
	{
		const char *pchLayoutFilePath = pPanel->GetLayoutFileLoadedFrom().String();
		if ( !V_strncmp( pchLayoutFilePath, "http://", 7 ) )
		{
			// Give some context for http paths
			// Go up to the first slash
			const char *pchURI = pchLayoutFilePath + 7;
			const char *pchEnd = V_UnqualifiedFileName( pchLayoutFilePath );
			while ( pchEnd > pchURI && *pchURI != '/' )
				pchURI++;

			pStyleLink->SetDialogVariable( "layoutfile", pchURI );
		}
		else
		{
			pStyleLink->SetDialogVariable( "layoutfile", V_UnqualifiedFileName( pPanel->GetLayoutFileLoadedFrom().String() ) );
		}

		pStyleLink->SetText( "#Debugger_LayoutFileLink" );
		pStyleLink->SetTabIndex( k_flTabIndexAuto );
		pStyleLink->SetOnActivateEvent( OpenFileForEdit::MakeEvent( this, pPanel->GetLayoutFileLoadedFrom().String(), 0 ) );
	}
	else
	{
		pStyleLink->SetText( "#Debugger_LayoutFileLink_Code" );
	}
	
	pRowOpen->SetHasClass( "ShowChildren", bShowChildren );
	
	// need to keep track of these panels
	PanelDebugInfo_t &info = m_mapPanelDebugInfo[ m_mapPanelDebugInfo.Insert( pPanel ) ];
	info.m_pPanel = pPanel;
	info.m_pOpen = pRowOpen;
	info.m_pToggle = pRowOpen->RequireChildInLayoutFile( "DebugLabelToggle" );
	return &info;
}


//-----------------------------------------------------------------------------
// Purpose: Expands an XML node for an existing panel (adds immediate children)
//-----------------------------------------------------------------------------
void CDebugLayout::AddPanelChildrenToLayout( CPanel2D *pPanel )
{
	Assert( pPanel );
	if ( !pPanel )
		return;

	PanelDebugInfo_t *pDebugInfo = FindDebugInfo( pPanel );
	if ( !pDebugInfo )
	{
		AssertMsg( false, "CDebugLayout::ExpandPanelLayout: panel does not exist" );
		return;
	}

	// if doesn't have a toggle button, we can't expand
	CToggleButton *pToggleButton = pDebugInfo->m_pToggle.Get();
	if ( !pToggleButton )
	{
		AssertMsg( false, "CDebugLayout::ExpandPanelLayout: unable to expand this panel" );
		return;
	}

	// if already expanded (toggle button selected), we are done
	if ( !pToggleButton->IsSelected() )
		return;

	// error if panel already has a close panel
	if ( pDebugInfo->m_pClose.Get() )
	{
		AssertMsg( false, "CDebugLayout::ExpandPanelLayout: panel already has close tag" );
		return;
	}
	
	// going to call AddCollapsedPanelToLayout which could affect our pDebugInfo pointer..
	CPanel2D *pPrevRow = pDebugInfo->m_pOpen.Get();
	pDebugInfo = NULL;

	// add collapsed nodes for each panel child
	int nDepth = GetPanelDepth( pPanel );
	for ( int i = 0; i < pPanel->GetChildCount(); i++ )
	{
		CPanel2D *pChild = pPanel->GetChild( i );
		PanelDebugInfo_t *pDebugInfoAdded = AddCollapsedPanelToLayout( pChild, pPrevRow, nDepth + 1 );
		pPrevRow = pDebugInfoAdded->m_pOpen.Get();
	}

	// Also include special hidden panels like scrollbars and
	// mouse scroll regions.
	for ( int i = 0; i < pPanel->GetHiddenChildCount(); i++ )
	{
		CPanel2D *pChild = pPanel->GetHiddenChild( i );
		PanelDebugInfo_t *pDebugInfoAdded = AddCollapsedPanelToLayout( pChild, pPrevRow, nDepth + 1 );
		pPrevRow = pDebugInfoAdded->m_pOpen.Get();
	}

	// our debug info pointer could have moved due to calling AddCollapsedPanelToLayout(), so find it again
	pDebugInfo = FindDebugInfo( pPanel );
	Assert( pDebugInfo );

	// add our close element
	CPanel2D *pRowClose = AppendElementCloseRow( pPanel, nDepth );
	MoveChildAfter( pRowClose, pPrevRow );
	pDebugInfo->m_pClose = pRowClose;

	// set expanded
	pToggleButton->SetSelected( false );
}


//-----------------------------------------------------------------------------
// Purpose: Builds text
//-----------------------------------------------------------------------------
void CDebugLayout::Build( bool bForce )
{
	VPROF_BUDGET( "CDebugLayout::Build", VPROF_BUDGETGROUP_TENFOOT );

	CPanel2D *pDebugPanel = m_pDebugPanel.Get();
	if ( !pDebugPanel )
	{
		m_pTopLevel = NULL;
		ClearLayout();
		return;
	}

	// remember if panel was expanded
	bool bWasExpanded = false;
	PanelDebugInfo_t *pInfo = FindDebugInfo( pDebugPanel );
	if ( pInfo )
		bWasExpanded = (pInfo->m_pToggle.Get() && !pInfo->m_pToggle->IsSelected());

	// find the top level panel for this panel's hierarchy
	CPanel2D *pTopLevelParent = pDebugPanel;
	for ( CPanel2D *pParent = pDebugPanel->GetParent(); pParent != NULL; pParent = pParent->GetParent() )
		pTopLevelParent = pParent;	

	if ( bForce )
		ClearLayout();

	m_pTopLevel = pTopLevelParent;

	CUtlVector<panorama::IUIWindow *> vecWindows;
	// add the root for each top level window hierarchy
	vecWindows.AddToTail( pDebugPanel->GetParentWindow() );
	UIEngine()->GetWindowsForDebugger( vecWindows );

	FOR_EACH_VEC( vecWindows, iWin )
	{
		if ( GetParentWindow() != vecWindows[iWin] )
		{
			const CUtlLinkedList< IUIPanel* > &listVisiblePanels = vecWindows[iWin]->GetTopLevelVisiblePanels();
			FOR_EACH_LL( listVisiblePanels, i )
			{
				CPanel2D *pTopLevelPanel = ToPanel2D( listVisiblePanels[i] );

				// expand to the debug panel
				if ( pTopLevelParent == pTopLevelPanel )
					AddLayoutForPanel( pDebugPanel );
				else
					AddLayoutForPanel( pTopLevelPanel );
			}
		}
	}

	// update selected. We could have just cleared layout data so look up again
	pInfo = FindDebugInfo( pDebugPanel );
	if ( pInfo )
	{
		pInfo->m_pOpen->AddClass( k_pchDebugLayoutPanelRowSelected );
		if ( bWasExpanded && pInfo->m_pToggle.Get() )
			ExpandOrCollapse( pDebugPanel, true );
	}	
}


//-----------------------------------------------------------------------------
// Purpose: Checks if the panel or panel's parent is shown in the layout display
//-----------------------------------------------------------------------------
bool CDebugLayout::IsPanelOrParentInLayout( CPanel2D *pPanel )
{
	if ( !pPanel )
		return false;

	if ( FindDebugInfo( pPanel ) )
		return true;

	CPanel2D *pParent = pPanel->GetParent();
	if ( !pParent )
		return false;

	return (FindDebugInfo( pParent ) != NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Retrieves a pointer to the debug info for a specified panel
//-----------------------------------------------------------------------------
CDebugLayout::PanelDebugInfo_t *CDebugLayout::FindDebugInfo( CPanel2D *pTarget )
{
	int iMap = m_mapPanelDebugInfo.Find( pTarget );
	if ( iMap != m_mapPanelDebugInfo.InvalidIndex() )
		return &m_mapPanelDebugInfo.Element( iMap );

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Retrieves a pointer to the debug info for a specified panel
//-----------------------------------------------------------------------------
CDebugLayout::PanelDebugInfo_t *CDebugLayout::FindDebugInfoForRow( CPanel2D *pRow )
{
	FOR_EACH_HASHMAP( m_mapPanelDebugInfo, i )
	{
		PanelDebugInfo_t &info = m_mapPanelDebugInfo[i];
		if ( pRow == info.m_pOpen.Get() || pRow == info.m_pClose.Get() )
			return &m_mapPanelDebugInfo[i];
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: key handling for tree navigation
//-----------------------------------------------------------------------------
bool CDebugLayout::OnKeyDown( const KeyData_t &code )
{
	// none of these shortcuts can do anything without a selected panel
	if ( m_pDebugPanel.Get() == NULL )
	{
		return false;
	}

	PanelDebugInfo_t *pInfoSelected = FindDebugInfo( m_pDebugPanel.Get() );
	if ( pInfoSelected == NULL )
	{
		return false;
	}

	switch ( code.m_KeyCode )
	{
	default:
		break;
	case KEY_ENTER:
	case KEY_PAD_ENTER:
		{
			if ( pInfoSelected->m_pToggle.Get() )
			{
				// toggle collapsed state of selected element
				ExpandOrCollapse( pInfoSelected->m_pPanel.Get(), pInfoSelected->m_pToggle->IsSelected() );
			}
			return true;
		}
	case KEY_LEFT:
	case KEY_RIGHT:
		{
			// expand or collapse selected element
			// FUTURE if collapsing and selected item is already collapsed, then collapse the parent and select that
			if ( pInfoSelected->m_pToggle.Get() )
			{
				ExpandOrCollapse( pInfoSelected->m_pPanel.Get(), code.m_KeyCode == KEY_RIGHT );
			}
			return true;
		}
	case KEY_DOWN:
	case KEY_UP:
		{
			// this panel itself contains the debug elements; find the appropriate place to go
			// by looking for next/previous children until we find one that is not a close element
			// (we don't ever select the close elements)
			CPanel2D *ppanel = pInfoSelected->m_pOpen.Get();

			int iChildCur = GetChildIndex( ppanel );
			int iChildVictim = iChildCur;
			PanelDebugInfo_t *pinfoVictim = NULL;

			while ( pinfoVictim == NULL )
			{
				iChildVictim = iChildVictim + ( code.m_KeyCode == KEY_DOWN ? 1 : -1 );

				if ( iChildVictim < 0 || iChildVictim >= GetChildCount() )
				{
					// ran off the end; stop
					break;
				}

				CPanel2D *pvictim = GetChild( iChildVictim );
				pinfoVictim = FindDebugInfoForRow( pvictim );

				if ( !pvictim->BIsVisible() )
				{
					// skip invisible items
					pinfoVictim = NULL;
				}
				else if ( pvictim == pinfoVictim->m_pOpen.Get() )
				{
					// great
					break;
				}
				else if ( pvictim == pinfoVictim->m_pClose.Get() )
				{
					// skip the close tag; keep going to find the next valid item
					pinfoVictim = NULL;
				}
			}

			if ( pinfoVictim )
			{
				DispatchEvent( SetDebugTarget(), this, pinfoVictim->m_pPanel );
			}
			return true;
		}
	}
	return false;

}




//-----------------------------------------------------------------------------
// Purpose: Updates displayed information for a panel
//-----------------------------------------------------------------------------
void CDebugLayout::UpdatePanelInfo( CPanel2D *pPanel )
{
	VPROF_BUDGET( "UpdatePanelInfo", VPROF_BUDGETGROUP_TENFOOT );
	if ( !pPanel )
		return;

	PanelDebugInfo_t *pInfo = FindDebugInfo( pPanel );
	if ( !pInfo )
		return;
#ifndef PANORAMA_USE_S1WRAPPER
	VPROF_BUDGET( "UpdatePanelInfo - update", VPROF_BUDGETGROUP_TENFOOT );
#endif
	CLabel *pPanelInfo = assert_cast< CLabel*>( pInfo->m_pOpen->FindChild( "DebugLayoutPanelOpen" ) );
	Assert( pPanelInfo );

	// if ourselves, also need to return here. We are currently adding child panels, and will infinitely recurse
	bool bShowChildren = ( pPanel->GetChildCount() != 0 && pPanel != this );
	AppendElementOpenTag( pPanelInfo, pPanel, bShowChildren );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the panel we are debugging
//-----------------------------------------------------------------------------
bool CDebugLayout::EventSetDebugTarget( CPanelPtr< CPanel2D > pPanel )
{
	PanelDebugInfo_t *pInfo = FindDebugInfo( m_pDebugPanel.Get() );
	if ( pInfo )
		pInfo->m_pOpen->RemoveClass( k_pchDebugLayoutPanelRowSelected );

	m_pDebugPanel = pPanel;
	Build();
	
	// need at least 1 layout pass before we can scroll the panel into view
	m_bScrollToDebugPanel = true;

	// let others handle this message
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a panel's style classes changed
//-----------------------------------------------------------------------------
bool CDebugLayout::EventPanelStyleClassesChanged( const CPanelPtr< IUIPanel > &pPanel )
{
	if( m_treePanelsDirty.Count() == 0 )
		DispatchEventAsync( 0.05f, UpdateDirtyPanelInfo(), this );

	m_treePanelsDirty.Insert( pPanel );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a panel's style flags change
//-----------------------------------------------------------------------------
bool CDebugLayout::EventPanelStyleFlagsChanged( const CPanelPtr< IUIPanel > &pPanel )
{
	if( m_treePanelsDirty.Count() == 0 )
		DispatchEventAsync( 0.05f, UpdateDirtyPanelInfo(), this );

	m_treePanelsDirty.InsertIfNotFound( pPanel );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles event to show dev info
//-----------------------------------------------------------------------------
bool CDebugLayout::EventShowDebugDevInfo( bool bShow )
{
	if ( bShow == m_bShowDevInfo )
		return false;

	m_bShowDevInfo = bShow;
	Build( true );

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when this panel or a child has been selected
//-----------------------------------------------------------------------------
bool CDebugLayout::EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	CPanel2D *pSelected = ToPanel2D( pPanel.Get() );
	if ( pSelected->GetPanelType() == CLabel::GetPanelSymbol() )
		pSelected = pSelected->GetParent();

	FOR_EACH_HASHMAP( m_mapPanelDebugInfo, i )
	{
		PanelDebugInfo_t *pInfo = &m_mapPanelDebugInfo[i];
		if ( pSelected == pInfo->m_pOpen.Get() || pSelected == pInfo->m_pClose.Get() )
		{
			DispatchEvent( SetDebugTarget(), this, pInfo->m_pPanel );
			return true;
		}
		else if ( pSelected == pInfo->m_pToggle.Get() )
		{
			// the toggle button already flipped its state. Cheat and set it back
			CToggleButton *pToggle = pInfo->m_pToggle.Get();
			pToggle->SetSelected( !pToggle->IsSelected() );

			// selected = collapsed.. so reverse
			bool bExpand = pToggle->IsSelected();
			ExpandOrCollapse( pInfo->m_pPanel.Get(), bExpand );
			
			// ExpandOrCollapse could have caused the vector to reallocate.. so our pointer is invalid
			pInfo = NULL;
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Expands or collapses the provided debug info
//-----------------------------------------------------------------------------
void CDebugLayout::ExpandOrCollapse( CPanel2D *pPanel, bool bExpand )
{
	PanelDebugInfo_t *pDebugInfo = FindDebugInfo( pPanel );
	if ( !pDebugInfo )
	{
		return;
	}

	Assert( pDebugInfo->m_pOpen.Get() );

	CToggleButton *pToggle = pDebugInfo->m_pToggle.Get();
	if ( !pToggle )
	{
		AssertMsg( false, "CDebugLayout::ExpandOrCollapse: No toggle button" );
		return;
	}

	// check if already in the desired state
	if ( !pToggle->IsSelected() == bExpand )
		return;

	// if expanding, check if we have already loaded children. If not, do so and return
	CPanel2D *pClose = pDebugInfo->m_pClose.Get();
	if ( !pClose && bExpand )
	{
		AddPanelChildrenToLayout( pDebugInfo->m_pPanel.Get() );
		return;
	}

	// This panel contains a list of panel rows. Show all rows up to and including exit row
	int iStart = GetChildIndex( pDebugInfo->m_pOpen.Get() );
	int iEnd = GetChildIndex( pClose );

	if ( iStart < 0 || iEnd < 0 )
	{
		AssertMsg( false, "CDebugLayout::ExpandOrCollapse: Invalid indexes" );
		return;	
	}

	// show each child row until we hit the end
	for ( int i = iStart + 1; i <= iEnd; i++ )
	{
		CPanel2D *pChild = GetChild( i );
		pChild->SetVisible( bExpand );

		// pretty slow lookup. May need to improve when layout gets longer
		PanelDebugInfo_t *pInfo = FindDebugInfoForRow( pChild );
		Assert( pInfo );

		// if selected (which we are defining as collapsed), skip all panels until we find the closing panel. This prevents us from
		// expanding subsections which were previously collapsed.
		CToggleButton *pCurrentToggle = pInfo->m_pToggle.Get();
		if ( pCurrentToggle && pInfo->m_pOpen.Get() == pChild && pCurrentToggle->IsSelected() )
		{
			CPanel2D *pChildClose = pInfo->m_pClose.Get();
			// skip loop if we haven't loaded the panel's children yet
			if ( pChildClose )
			{
				while ( GetChild( i ) != pChildClose )
					i++;
			}
		}
	}

	// set expanded
	pToggle->SetSelected( !bExpand );
}


//-----------------------------------------------------------------------------
// Purpose: Layout traverse
//-----------------------------------------------------------------------------
void CDebugLayout::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );
	
	if ( m_bScrollToDebugPanel )
	{
		m_bScrollToDebugPanel = false;
		PanelDebugInfo_t *pInfo = FindDebugInfo( m_pDebugPanel.Get() );
		if ( pInfo )
			pInfo->m_pOpen->ScrollParentToMakePanelFit();
	}
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CDebugLayout::ValidateClientPanel( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	BaseClass::ValidateClientPanel( validator, pchName );

	ValidateObj( m_mapPanelDebugInfo );
	ValidateObj( m_treePanelsDirty );
}
#endif
