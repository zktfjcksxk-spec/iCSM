//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "debugautocomplete.h"
#include "panorama/controls/textentry.h"
#include "panorama/controls/label.h"
#include "panorama/renderer/styleproperties.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D( CDebugAutoComplete, DebugAutoComplete );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDebugAutoComplete::CDebugAutoComplete( CTextEntry *pTarget, const char * pchPanelID ) : CPanel2D( pTarget->GetParentWindow(), pchPanelID )
{
	m_bClosing = false;
	SetAcceptsFocus( true );
	SetLayoutFile( pTarget->GetLayoutFile() );

	SetTarget( pTarget );
	
	RegisterEventHandler( Activated(), this, &CDebugAutoComplete::EventPanelActivated );
	RegisterForUnhandledEvent( InputFocusSet(), this, &CDebugAutoComplete::EventInputFocusSet );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDebugAutoComplete::~CDebugAutoComplete()
{
	UnregisterForUnhandledEvent( InputFocusSet(), this, &CDebugAutoComplete::EventInputFocusSet );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the target panel to position around
//-----------------------------------------------------------------------------
void CDebugAutoComplete::SetTarget( CTextEntry *pTarget )
{
	m_pTarget = pTarget;
	PositionNearParent();
}


//-----------------------------------------------------------------------------
// Purpose: override to change how this panel arranges its children
//-----------------------------------------------------------------------------
void CDebugAutoComplete::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );
	PositionNearParent();
}

//-----------------------------------------------------------------------------
// Purpose: Sets position of panel relative to target
//-----------------------------------------------------------------------------
void CDebugAutoComplete::PositionNearParent()
{
	CPanel2D *pTarget = m_pTarget.Get();
	if ( !pTarget )
		return;

	float xPos = 0.0f;
	float yPos = 0.0f;
	pTarget->GetPositionWithinAncestor( NULL, &xPos, &yPos );

	// calculate available space above and below target
	float flWindowWidth, flWindowHeight;
	GetParentWindow()->GetClientDimensions( flWindowWidth, flWindowHeight );

	float flAboveTarget = yPos;
	float flBelowTarget = flWindowHeight - yPos - pTarget->GetActualRenderHeight();

	// try to position below the target. If we can't fit top or bottom, go bottom
	float flOurHeight = GetActualRenderHeight();
	if ( flBelowTarget >= flOurHeight || flOurHeight > flAboveTarget )
		yPos += pTarget->GetActualRenderHeight();
	else
		yPos -= flOurHeight;

	// if we are wider than the remaining horizontal space from where the target starts, align with right side of window
	if ( flWindowWidth - xPos < GetActualRenderWidth() )
		xPos = flWindowWidth - GetActualRenderWidth();
	
	CUILength lenX( xPos, CUILength::k_EUILengthLength );
	CUILength lenY( yPos, CUILength::k_EUILengthLength );

	lenX.ScaleLengthValue( 1.0f / GetActualUIScaleX() );
	lenY.ScaleLengthValue( 1.0f / GetActualUIScaleY() );

	SetPosition( lenX, lenY, CUILength( 0.0f, CUILength::k_EUILengthLength ) );
}


//-----------------------------------------------------------------------------
// Purpose: Updates value suggestions in context menu
//-----------------------------------------------------------------------------
void CDebugAutoComplete::PopulateValueSuggestions( const char *pchStyleName )
{
	RemoveAndDeleteChildren();
	
	const char *pchText = m_pTarget->PchGetText();
	if ( !pchText )
	{
		DeleteSelf();
		return;
	}

	CStyleSymbol symStyle = UIEngine()->UIStyleFactory()->GetPropertyNameForAlias( pchStyleName );
	if ( !symStyle.IsValid() )
	{
		DeleteSelf();
		return;
	}

	CStyleProperty *pProperty = NULL;
	pProperty = UIEngine()->UIStyleFactory()->CreateStyleProperty( symStyle );
	if ( !pProperty )
	{
		DeleteSelf();
		return;
	}

	CUtlVector< CUtlString > vecSuggestions;
	pProperty->GetSuggestedValues( pchStyleName, pchText, vecSuggestions );

	FOR_EACH_VEC( vecSuggestions, i )
	{			
		CLabel *pLabel = new CLabel( this, NULL );
		pLabel->SetText( vecSuggestions[i] );
		pLabel->SetSelectionPosition( k_flSelectionPosInvalid, k_flSelectionPosAuto );
		pLabel->SetAcceptsFocus( true );
		
		// No point adding more than 100 panels, the user will have to type somethign to filter more specifically
		if ( i >= 100 )
			break;
	}

	UIEngine()->UIStyleFactory()->FreeStyleProperty( pProperty );

	// update focus
	if ( GetChildCount() > 0 )
	{
		GetChild( 0 )->SetFocus();

		// we want the text entry to believe it has focus. We will be feeding it all key input
		m_pTarget->AddStyleFlag( k_EStyleFlagFocus );
	}
	else
	{
		DeleteSelf();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Updates name suggestions in context menu
//-----------------------------------------------------------------------------
void CDebugAutoComplete::PopulateNameSuggestions()
{
	RemoveAndDeleteChildren();

	const char *pchText = m_pTarget->PchGetText();
	size_t cchText = V_strlen( pchText );
	if ( cchText == 0 )
	{
		DeleteSelf();
		return;
	}

	// add all properties that begin with target's text
	const CUtlVector< CUtlString > &vecProperties = UIEngine()->UIStyleFactory()->GetSortedPropertyAndAliasNames();
	int iVec = vecProperties.SortedFindLessOrEqual( pchText, StylePropertyNameLessThan, NULL );
	if ( iVec == vecProperties.InvalidIndex() )
		iVec = 0;

	while ( vecProperties.IsValidIndex( iVec ) )
	{
		const char *pchValue = vecProperties[iVec].String();
		int nCmp = V_strncmp( pchValue, pchText, cchText );
		if ( nCmp > 0 )
			break;

		// if less than, keep going
		if ( nCmp < 0 )
		{
			iVec++;
			continue;
		}

		// matches			
		CLabel *pLabel = new CLabel( this, NULL );
		pLabel->SetText( vecProperties[iVec] );
		pLabel->SetSelectionPosition( k_flSelectionPosInvalid, k_flSelectionPosAuto );
		pLabel->SetAcceptsFocus( true );
		iVec++;
	}

	// update focus
	if ( GetChildCount() > 0 )
	{
		GetChild( 0 )->SetFocus();

		// we want the text entry to believe it has focus. We will be feeding it all key input
		m_pTarget->AddStyleFlag( k_EStyleFlagFocus );
	}
	else
	{
		DeleteSelf();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when a keyboard key is pressed
//-----------------------------------------------------------------------------
bool CDebugAutoComplete::OnKeyDown( const KeyData_t &code )
{
	bool bHandled = BaseClass::OnKeyUp( code );
	CTextEntry *pTarget = m_pTarget.Get();

	// handle keys which make our navigation work, otherwise forward to text entry
	if ( code.m_KeyCode == KEY_ESCAPE )
	{
		DeleteSelf();
		return true;
	}
	else if ( code.m_KeyCode == KEY_UP || code.m_KeyCode == KEY_DOWN )
	{
		return bHandled;
	}
	else if ( code.m_KeyCode == KEY_TAB )
	{
		CPanel2D *pInputFocus = ToPanel2D( GetParentWindow()->UIWindowInput()->GetInputFocus() );
		Assert ( pInputFocus->GetPanelType() == CLabel::GetPanelSymbol() && pInputFocus->GetParent() == this );
		CLabel *pLabel = (CLabel*)pInputFocus;

		SuggestionSelected( pLabel );
		return true;
	}
	else if ( code.m_KeyCode == KEY_ENTER )
	{
		// dont forward enter, however return false so default activation code is fired for selected label
		return false;
	}
	
	if ( pTarget )
		return pTarget->OnKeyDown( code );

	return bHandled;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a keyboard key is released
//-----------------------------------------------------------------------------
bool CDebugAutoComplete::OnKeyUp( const KeyData_t & code )
{
	bool bHandled = BaseClass::OnKeyUp( code );

	CTextEntry *pTarget = m_pTarget.Get();
	if ( pTarget )
		return pTarget->OnKeyUp( code );

	return bHandled;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a character has been entered
//-----------------------------------------------------------------------------
bool CDebugAutoComplete::OnKeyTyped( const KeyData_t &unichar )
{
	bool bHandled = BaseClass::OnKeyTyped( unichar );

	CTextEntry *pTarget = m_pTarget.Get();
	if ( pTarget )
		return pTarget->OnKeyTyped( unichar );

	return bHandled;
}


//-----------------------------------------------------------------------------
// Purpose: Sets target to selected suggestion and closes
//-----------------------------------------------------------------------------
void CDebugAutoComplete::SuggestionSelected( CLabel *pLabel )
{
	CTextEntry *pTarget = m_pTarget.Get();

	m_bClosing = true;
	pTarget->SetText( pLabel->PchGetText() );
	DispatchEvent( TabForward(), pTarget, 0 );

	// we need to simulate the user submitting text, so fire the event
	DispatchEvent( TextEntrySubmit(), pTarget, pTarget->PchGetText() );

	DeleteSelf( false );
}


//-----------------------------------------------------------------------------
// Purpose: Deletes our panel and handles fixing focus
//-----------------------------------------------------------------------------
void CDebugAutoComplete::DeleteSelf( bool bSetFocusToTarget )
{
	m_bClosing = true;

	CPanel2D *pTarget = m_pTarget.Get();
	if ( pTarget )
	{
		if ( bSetFocusToTarget )
			pTarget->SetFocus();
		else if ( ToPanel2D( GetParentWindow()->UIWindowInput()->GetInputFocus() ) != pTarget )
			pTarget->RemoveStyleFlag( k_EStyleFlagFocus );
	}
	
	delete this;
}


//-----------------------------------------------------------------------------
// Purpose: Event fired when a child panel (or ourselves) is activated
//-----------------------------------------------------------------------------
bool CDebugAutoComplete::EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	CPanel2D *pHit = ToPanel2D( pPanel.Get() );
	if ( !pHit || pHit->GetPanelType() != CLabel::GetPanelSymbol() )
		return false;

	SuggestionSelected( (CLabel*)pHit );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event fired when input focus changes
//-----------------------------------------------------------------------------
bool CDebugAutoComplete::EventInputFocusSet( const CPanelPtr< IUIPanel > &pPanel )
{
	// focus could have changed because we are closing
	if ( m_bClosing )
		return false;

	CPanel2D *pHasFocus = ToPanel2D( pPanel.Get() );

	// if panel isn't the target or in our tree, close
	if ( !pHasFocus || (pHasFocus != m_pTarget.Get() && pHasFocus != this && !pHasFocus->IsDescendantOf( this )) )
		DeleteSelf( false );

	return false;
}
