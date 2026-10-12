//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "debugpanelparents.h"
#include "panorama/controls/label.h"	
#include "panorama/controls/button.h"
#include "../../controls/debug/debugger.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CDebugPanelParents, DebugPanelParents )



//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDebugPanelParents::CDebugPanelParents( CPanel2D *pParent, const char *pchName ) : CPanel2D( pParent, pchName )
{
	m_bRebuilding = false;
	SetVisible( false );

	RegisterForUnhandledEvent( SetDebugTarget(), this, &CDebugPanelParents::EventSetDebugTarget );	
	RegisterForUnhandledEvent( StyleClassesChanged(), this, &CDebugPanelParents::EventPanelStyleClassesChanged );
	RegisterForUnhandledEvent( StyleFlagsChanged(), this, &CDebugPanelParents::EventPanelStyleFlagsChanged );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDebugPanelParents::~CDebugPanelParents()
{
	UnregisterForUnhandledEvent( SetDebugTarget(), this, &CDebugPanelParents::EventSetDebugTarget );	
	UnregisterForUnhandledEvent( StyleClassesChanged(), this, &CDebugPanelParents::EventPanelStyleClassesChanged );
	UnregisterForUnhandledEvent( StyleFlagsChanged(), this, &CDebugPanelParents::EventPanelStyleFlagsChanged );
}


//-----------------------------------------------------------------------------
// Purpose: Builds text
//-----------------------------------------------------------------------------
void CDebugPanelParents::Build()
{
	if ( m_bRebuilding )
		return;

	m_bRebuilding = true;
	RemoveAndDeleteChildren();
	
	CPanel2D *pPanel = m_pDebugPanel.Get();
	SetVisible( pPanel != NULL );
	if ( !pPanel )
	{
		m_bRebuilding = false;
		return;	
	}

	// add a specially named label for the selected panel
	char buffer[1024];
	GetDebugPanelName( buffer, V_ARRAYSIZE( buffer ), pPanel->UIPanel() );
	CLabel *pSelected = new CLabel( this, "SelectedPanelName" );
	pSelected->SetAllowRawText( true );
	pSelected->SetText( buffer );

	// foreach parent, add a separator and button for the parent
	for ( pPanel = pPanel->GetParent(); pPanel != NULL; pPanel = pPanel->GetParent() )
	{
		CLabel *pSeperator = new CLabel( this, NULL );
		pSeperator->SetAllowRawText( true );
		pSeperator->SetText( "<" );

		CButton *pButton = new CButton( this, NULL );

		CPanelPtr< CPanel2D > ptrPanel( pPanel );
		pButton->SetTabIndex( k_flTabIndexAuto );
		pButton->SetOnActivateEvent( SetDebugTarget::MakeEvent( this, ptrPanel ) );

		CLabel *pButtonText = new CLabel( pButton, NULL );
		GetDebugPanelName( buffer, V_ARRAYSIZE( buffer ), pPanel->UIPanel() );
		pButtonText->SetAllowRawText( true );
		pButtonText->SetText( buffer );
	}

	m_bRebuilding = false;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the panel we are debugging
//-----------------------------------------------------------------------------
bool CDebugPanelParents::EventSetDebugTarget( CPanelPtr< CPanel2D > pPanel )
{
	//m_pDebugPanel.SetFromUInt64( hPanel );
	m_pDebugPanel = pPanel;
	Build();

	// let others handle this message
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a panel's style flags change
//-----------------------------------------------------------------------------
bool CDebugPanelParents::EventPanelStyleFlagsChanged( const CPanelPtr< IUIPanel > &pPanel )
{
	CPanel2D *pDebugPanel = m_pDebugPanel.Get();
	CPanel2D *pChanged = ToPanel2D(pPanel.Get());
	if ( pDebugPanel && pChanged && (pDebugPanel == pChanged || pDebugPanel->IsDescendantOf( pChanged )) )
		Build();

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a panel's style classes changed
//-----------------------------------------------------------------------------
bool CDebugPanelParents::EventPanelStyleClassesChanged( const CPanelPtr< IUIPanel > &pPanel )
{
	CPanel2D *pDebugPanel = m_pDebugPanel.Get();
	CPanel2D *pChanged = ToPanel2D(pPanel.Get());
	if ( pDebugPanel && pChanged && (pDebugPanel == pChanged || pDebugPanel->IsDescendantOf( pChanged )) )
		Build();

	return false;
}