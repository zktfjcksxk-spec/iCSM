//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "debugpanel.h"
#include "panorama/controls/button.h"
#include "debuglayout.h"
#include "debugpanelparents.h"
#include "debugger.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CDebugPanel, DebugPanel );
REGISTER_PANEL2D_FACTORY( CDebugPanelComputed, DebugPanelComputed );

DEFINE_PANORAMA_EVENT( ShowDebugStyle );
DEFINE_PANORAMA_EVENT( ShowDebugComputed );

static const int k_nAutoReloadComputedDelay = 50 * k_nThousand;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDebugPanel::CDebugPanel( CPanel2D *pParent, const char *pchName ) : CPanel2D( pParent, pchName )
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/debugpanel.xml" ) );
	
	m_pPanelPages = FindChildInLayoutFile( "DebugPanelPages" );
	m_pStyle = FindChildInLayoutFile( "DebugPanelStyle" );
	m_pComputed = FindChildInLayoutFile( "DebugPanelComputed" );
	m_pPanelStyleInvalid = assert_cast< CLabel* >( FindChildInLayoutFile( "DebugPanelStyleInvalid" ) );

	RegisterEventHandler( ShowDebugStyle(), this, &CDebugPanel::OnShowDebugStyle );
	RegisterEventHandler( ShowDebugComputed(), this, &CDebugPanel::OnShowDebugComputed );
	RegisterEventHandler( DebugStyleStatus(), this, &CDebugPanel::OnDebugStyleStaus );

	OnShowDebugStyle();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDebugPanel::~CDebugPanel()
{

}


//-----------------------------------------------------------------------------
// Purpose: Hides all tabed pages
//-----------------------------------------------------------------------------
void CDebugPanel::HideAllPages()
{
	for( int i = 0; i < m_pPanelPages->GetChildCount(); i++ )
		m_pPanelPages->GetChild( i )->SetVisible( false );
}


//-----------------------------------------------------------------------------
// Purpose: Shows the style tab
//-----------------------------------------------------------------------------
bool CDebugPanel::OnShowDebugStyle()
{
	HideAllPages();
	m_pStyle->SetVisible( true );

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Shows the computed tab
//-----------------------------------------------------------------------------
bool CDebugPanel::OnShowDebugComputed()
{
	HideAllPages();
	m_pComputed->SetVisible( true );

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Received event for debug style status
//-----------------------------------------------------------------------------
bool CDebugPanel::OnDebugStyleStaus( bool bUpdated )
{
	m_pPanelStyleInvalid->SetVisible( !bUpdated );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDebugPanelComputed::CDebugPanelComputed( CPanel2D *pParent, const char *pchName ) : CPanel2D( pParent, pchName )
, m_scheduledReload( MAKE_SCHEDULED_FUNC( CDebugPanelComputed::AutoReload ) )
{
	m_bCreatedControls = false;
	RegisterForUnhandledEvent( SetDebugTarget(), this, &CDebugPanelComputed::OnSetDebugTarget );

	m_scheduledReload.Schedule( k_nAutoReloadComputedDelay );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDebugPanelComputed::~CDebugPanelComputed()
{
	UnregisterForUnhandledEvent( SetDebugTarget(), this, &CDebugPanelComputed::OnSetDebugTarget );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a row to the computed output
// Returns: The created value label
//-----------------------------------------------------------------------------
CLabel *CDebugPanelComputed::AddRow( const char *pchName )
{
	// add a panel for each row. Style class should flow to the right
	CPanel2D *pRow = new CPanel2D( this, NULL );
	pRow->AddClass( "DebugComputedRow" );

	CLabel *pName = new CLabel ( pRow, NULL );
	pName->AddClass( "DebugComputedName" );
	pName->SetText( pchName, CLabel::k_ETextTypeUnlocalized );

	CLabel *pValue = new CLabel( pRow, NULL );
	pValue->AddClass( "DebugComputedValue" );

	return pValue;
}


//-----------------------------------------------------------------------------
// Purpose: Used to sort symbols by string
//-----------------------------------------------------------------------------
bool BSymbolStringLessThan( const CStyleSymbol &lhs, const CStyleSymbol &rhs )
{
	return (V_strcmp( lhs.String(), rhs.String() ) < 0);
}

//-----------------------------------------------------------------------------
// Purpose: Used to sort symbols by string
//-----------------------------------------------------------------------------
int SymbolStringCompare( const CStyleSymbol *lhs, const CStyleSymbol *rhs )
{
	return V_strcmp( lhs->String(), rhs->String() );
}


//-----------------------------------------------------------------------------
// Purpose: Creates all label controls for this panel
//-----------------------------------------------------------------------------
void CDebugPanelComputed::CreateControls()
{
	if ( m_bCreatedControls )
		return;

	m_bCreatedControls = true;

	// add property values
	CLabel *pLabel = new CLabel( this, "ComputedPropertiesHeader" );
	pLabel->SetText( "#Debugger_Properties" );

	// add properties sorted
	const CUtlVector< CStyleSymbol > &vecProperties = CStylePropertyFactory::GetAllProperties();
	CUtlVector< CStyleSymbol > vecSortedProperties;
	vecSortedProperties.EnsureCapacity( vecProperties.Count() );
	vecSortedProperties.CopyArray( vecProperties.Base(), vecProperties.Count() );
#if !defined( SOURCE2_PANORAMA )
	vecSortedProperties.Sort( BSymbolStringLessThan );
#else
	vecSortedProperties.Sort( SymbolStringCompare );
#endif

	FOR_EACH_VEC( vecSortedProperties, i )
	{
		CStyleSymbol symProperty = vecSortedProperties[i];
		CLabel *pRowLabel = AddRow( symProperty.String() );
		m_mapPropertyLabels.Insert( symProperty, pRowLabel );
	}


	// add measurements
	pLabel = new CLabel( this, "ComputedMeasurementsHeader" );
	pLabel->SetText( "#Debugger_Measurements" );

	m_pDesiredLayoutWidth = AddRow( "Desired Layout Width" );
	m_pDesiredLayoutHeight = AddRow( "Desired Layout Height" );
	m_pActualLayoutWidth = AddRow( "Actual Layout Width" );
	m_pActualLayoutHeight = AddRow( "Actual Layout Height" );
	m_pContentWidth = AddRow( "Content Width" );
	m_pContentHeight = AddRow( "Content Height" );
	m_pActualRenderWidth = AddRow( "Actual Render Width" );
	m_pActualRenderHeight = AddRow( "Actual Render Height" );
	m_pActualXOffset = AddRow( "Actual Panel Offset X" );
	m_pActualYOffset = AddRow( "Actual Panel Offset Y" );
	m_pContentsXScrollOffset = AddRow( "Content Panel Scroll X" );
	m_pContentsYScrollOffset = AddRow( "Content Panel Scroll Y" );
	m_pActualUIScaleX = AddRow( "Actual UI Scale X" );
	m_pActualUIScaleY = AddRow( "Actual UI Scale Y" );
	m_pActualUIScaleZ = AddRow( "Actual UI Scale Z" );

	// misc
	pLabel = new CLabel( this, "ComputedMiscHeader" );
	pLabel->SetText( "#Debugger_Misc" );

	m_pMiscLayoutFile = AddRow( "Layout File" );
	m_pMiscTabIndex = AddRow( "Tab Index" );
	m_pMiscSelectionPos = AddRow( "Selection Pos" );
	m_pHitTest = AddRow( "Hit Test" );
	m_pHitTestChildren = AddRow( "Hit Test Children" );

	// paint perf
	pLabel = new CLabel( this, "ComputedPaintPerfHeader" );
	pLabel->SetText( "Paint Performance", CLabel::k_ETextTypeUnlocalized );

	m_pCachedCommandList = AddRow( "Has Cached Command List" );
	m_pCommandListBytesSize = AddRow( "Command List Bytes Size" );
	m_pRepaintRate = AddRow( "Repaint Rate" );
}


//-----------------------------------------------------------------------------
// Purpose: Encapsulate how to render human-readable versions of tab indexes
// or selection pos axes
//-----------------------------------------------------------------------------
static void FormatSelPos( float selpos, CFmtStr32 &s )
{
	AssertOnce( k_flTabIndexInvalid == k_flSelectionPosInvalid );
	AssertOnce( k_flTabIndexAuto == k_flSelectionPosAuto );

	if ( selpos == k_flTabIndexInvalid )
	{
		s = "invalid";
	}
	else if ( selpos == k_flTabIndexAuto )
	{
		s = "auto";
	}
	else
	{
		s.Format( "%.0f", selpos );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Builds text
//-----------------------------------------------------------------------------
void CDebugPanelComputed::Build()
{
	VPROF_BUDGET( "CDebugPanelComputed::Build", VPROF_BUDGETGROUP_TENFOOT );

	CPanel2D *pPanel = m_pDebugPanel.Get();
	if ( !pPanel )
		return;

	CreateControls();
	IUIPanelStyle *pStyle = pPanel->AccessStyle();

	// properties
	const CUtlVector< CStyleSymbol > &vecProperties = CStylePropertyFactory::GetAllProperties();
	FOR_EACH_VEC( vecProperties, i )
	{
		// check if panel has property set. If not, need to create to get default value
		bool bCreatedProperty = false;
		CStyleProperty *pProperty = NULL;
		pStyle->FindPropertyInfo( vecProperties[i], &pProperty, NULL, NULL );
		if ( !pProperty )
		{
			pProperty = CStylePropertyFactory::CreateStyleProperty( vecProperties[i] );
			pProperty->ResolveDefaultValues();
			pProperty->ApplyUIScaleFactor( pPanel->GetActualUIScale(), pPanel->UIPanel()->GetParentActualUIScale() );

			bCreatedProperty = true;
		}

		CFmtStr1024 fmtProperty;
		pProperty->ToString( &fmtProperty );

		int iMap = m_mapPropertyLabels.Find( pProperty->GetPropertySymbol() );
		if ( iMap == m_mapPropertyLabels.InvalidIndex() )
		{
			AssertMsg1( false, "Couldn't find label for property: %s", pProperty->GetPropertySymbol().String() );
			continue;
		}

		CLabel *pLabel = m_mapPropertyLabels.Element( iMap );
		pLabel->SetText( fmtProperty, CLabel::k_ETextTypeUnlocalized );
		
		if ( bCreatedProperty && pProperty )
			CStylePropertyFactory::FreeStyleProperty( pProperty );
	}

	// measurement controls
	m_pDesiredLayoutWidth->SetText( CFmtStr( "%fpx", pPanel->GetDesiredLayoutWidth() ), CLabel::k_ETextTypeUnlocalized );
	m_pDesiredLayoutHeight->SetText( CFmtStr( "%fpx", pPanel->GetDesiredLayoutHeight() ), CLabel::k_ETextTypeUnlocalized );
	m_pActualLayoutWidth->SetText( CFmtStr( "%fpx", pPanel->GetActualLayoutWidth() ), CLabel::k_ETextTypeUnlocalized );
	m_pActualLayoutHeight->SetText( CFmtStr( "%fpx", pPanel->GetActualLayoutHeight() ), CLabel::k_ETextTypeUnlocalized );
	m_pContentWidth->SetText( CFmtStr( "%fpx", pPanel->GetContentWidth() ), CLabel::k_ETextTypeUnlocalized );
	m_pContentHeight->SetText( CFmtStr( "%fpx", pPanel->GetContentHeight() ), CLabel::k_ETextTypeUnlocalized );
	m_pActualRenderWidth->SetText( CFmtStr( "%fpx", pPanel->GetActualRenderWidth() ), CLabel::k_ETextTypeUnlocalized );
	m_pActualRenderHeight->SetText( CFmtStr( "%fpx", pPanel->GetActualRenderHeight() ), CLabel::k_ETextTypeUnlocalized );
	m_pActualXOffset->SetText( CFmtStr( "%fpx", pPanel->GetActualXOffset() ), CLabel::k_ETextTypeUnlocalized );
	m_pActualYOffset->SetText( CFmtStr( "%fpx", pPanel->GetActualYOffset() ), CLabel::k_ETextTypeUnlocalized );
	m_pContentsXScrollOffset->SetText( CFmtStr( "%fpx", pPanel->GetContentsXScrollOffset() ), CLabel::k_ETextTypeUnlocalized );
	m_pContentsYScrollOffset->SetText( CFmtStr( "%fpx", pPanel->GetContentsYScrollOffset() ), CLabel::k_ETextTypeUnlocalized );
	m_pActualUIScaleX->SetText( CFmtStr( "%f", pPanel->GetActualUIScaleX() ), CLabel::k_ETextTypeUnlocalized );
	m_pActualUIScaleY->SetText( CFmtStr( "%f", pPanel->GetActualUIScaleY() ), CLabel::k_ETextTypeUnlocalized );
	m_pActualUIScaleZ->SetText( CFmtStr( "%f", pPanel->GetActualUIScaleZ() ), CLabel::k_ETextTypeUnlocalized );

	// misc
	m_pMiscLayoutFile->SetText( pPanel->GetLayoutFile().String(), CLabel::k_ETextTypeUnlocalized );

	// tab index and selection pos have some special values to handle
	CFmtStr32 sTabIndex;
	FormatSelPos( pPanel->GetTabIndex(), sTabIndex );
	m_pMiscTabIndex->SetText( sTabIndex, CLabel::k_ETextTypeUnlocalized );

	CFmtStr32 sSelectionPosX, sSelectionPosY;
	FormatSelPos( pPanel->GetSelectionPositionX(), sSelectionPosX );
	FormatSelPos( pPanel->GetSelectionPositionY(), sSelectionPosY );
	CFmtStrN<64> sSelectionPos( "(%s,%s)", sSelectionPosX.String(), sSelectionPosY.String() );
	m_pMiscSelectionPos->SetText( sSelectionPos, CLabel::k_ETextTypeUnlocalized );

	// update calculated hit test field
	const char *pchHitTest = "true";
	if ( !pPanel->BHitTestEnabled() )
	{
		pchHitTest = "false";
	}
	else
	{
		// check if parent disables hit test for us
		for ( CPanel2D *pParent = pPanel->GetParent(); pParent != NULL; pParent = pParent->GetParent() )
		{
			if ( !pParent->BHitTestChildrenEnabled() )
			{
				pchHitTest = "disabled by parent";
				break;
			}
		}
	}
	m_pHitTest->SetText( pchHitTest, CLabel::k_ETextTypeUnlocalized );

	// check disable children
	m_pHitTestChildren->SetText( pPanel->BHitTestChildrenEnabled() ? "true" : "false", CLabel::k_ETextTypeUnlocalized );

	// Paint perf
	m_pCachedCommandList->SetText( pPanel->UIPanel()->BHasCachedCommandList() ? "true" : "false", CLabel::k_ETextTypeUnlocalized );
	m_pCommandListBytesSize->SetText( CFmtStr( "%u", pPanel->UIPanel()->GetCommandListBytesSize() ), CLabel::k_ETextTypeUnlocalized );
	m_pRepaintRate->SetText( CFmtStr( "%.2f", pPanel->UIPanel()->GetRepaintRate() ), CLabel::k_ETextTypeUnlocalized );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the panel we are debugging
//-----------------------------------------------------------------------------
bool CDebugPanelComputed::OnSetDebugTarget( CPanelPtr< CPanel2D > pPanel )
{
	m_pDebugPanel = pPanel;	
	Build();

	// let others handle this message
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called on timer to reload settings
//-----------------------------------------------------------------------------
void CDebugPanelComputed::AutoReload()
{
	// can make this event driven instead of polling when we add invalidate layout
	if ( BIsVisible() )
		Build();

	m_scheduledReload.Schedule( k_nAutoReloadComputedDelay );
}

#ifdef DBGFLAG_VALIDATE
void CDebugPanelComputed::ValidateClientPanel( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_mapPropertyLabels );
	ValidateObj( m_scheduledReload );
}
#endif
