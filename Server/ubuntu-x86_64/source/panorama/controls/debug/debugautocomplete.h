//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CONTEXTMENU_H
#define CONTEXTMENU_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/controls/panel2d.h"

namespace panorama
{

class CTextEntry;
class CLabel;

//-----------------------------------------------------------------------------
// Purpose: Top level panel for a context menu
//-----------------------------------------------------------------------------
class CDebugAutoComplete : public CPanel2D
{
	DECLARE_PANEL2D( CDebugAutoComplete, CPanel2D );

public:
	CDebugAutoComplete( CTextEntry *pTarget, const char * pchPanelID );
	virtual ~CDebugAutoComplete();

	CTextEntry *GetTarget() { return m_pTarget.Get(); }
	void SetTarget( CTextEntry *pTarget );
	void PopulateNameSuggestions();
	void PopulateValueSuggestions( const char *pchStyleName );

	// kb/mouse management
	virtual bool OnKeyDown( const KeyData_t &code );
	virtual bool OnKeyUp( const KeyData_t & code );
	virtual bool OnKeyTyped( const KeyData_t &unichar );

	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );

private:
	void PositionNearParent();
	void DeleteSelf( bool bSetFocusToTarget = true );	
	void SuggestionSelected( CLabel *pLabel );

	// events
	bool EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool EventInputFocusSet( const CPanelPtr< IUIPanel > &pPanel );

	CPanelPtr< CTextEntry > m_pTarget;
	bool m_bClosing;
};

} // namespace panorama

#endif // CONTEXTMENU_H
