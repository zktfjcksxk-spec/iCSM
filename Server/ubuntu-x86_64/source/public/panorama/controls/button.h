//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_BUTTON_H
#define PANORAMA_BUTTON_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"
#include "label.h"

namespace panorama
{

class CLabel;

//-----------------------------------------------------------------------------
// Purpose: Button
//-----------------------------------------------------------------------------
class CButton : public CPanel2D
{
	DECLARE_PANEL2D( CButton, CPanel2D );

public:
	CButton( CPanel2D *parent, const char * pchPanelID );
	virtual ~CButton();

	// clone
	virtual bool IsClonable() OVERRIDE { return AreChildrenClonable(); }
	virtual CPanel2D *Clone() OVERRIDE;

	// events
	bool EventActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );

protected:
	virtual void InitClonedPanel( CPanel2D *pPanel ) OVERRIDE;
};

//-----------------------------------------------------------------------------
// Purpose: Simple Button with text on it
//-----------------------------------------------------------------------------
class CTextButton : public CButton
{
	DECLARE_PANEL2D( CTextButton, CButton );

public:
	CTextButton( CPanel2D *parent, const char *pchPanelID );
	virtual ~CTextButton();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	virtual const char *PchGetText() const { return m_pLabel->PchGetText(); }
	virtual void SetText( const char *pchValue, CLabel::ETextType eTextType = CLabel::k_ETextTypeNone );
	void SetAllowRawText( bool bAllow );

	CLabel *GetLabel() { return m_pLabel; }

protected:
	virtual bool BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties );
	void InternalSetText( const char *pchValue ) { SetText( pchValue ); }

	CLabel *m_pLabel;
	CLabel::ETextType m_eTextType;
};


//-----------------------------------------------------------------------------
// Purpose: ToggleButton
//-----------------------------------------------------------------------------
class CToggleButton : public CButton
{
	DECLARE_PANEL2D( CToggleButton, CButton );

public:
	CToggleButton( CPanel2D *parent, const char * pchPanelID );
	virtual ~CToggleButton();

	void SetSelected( bool bSelected );
	void SetText( const char *pchText, CLabel::ETextType eTextType = CLabel::k_ETextTypeNone );
	void SetAllowRawText( bool bAllow );
	virtual bool OnKeyTyped( const KeyData_t &unichar ) OVERRIDE;

	// events
	virtual bool EventActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	virtual const char *PchGetText() const { return m_pLabel ? m_pLabel->PchGetText() : ""; }	

	CLabel *GetLabel() { return m_pLabel; }

protected:
	virtual bool BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties ) OVERRIDE;
	void InternalSetText( const char *pchValue ) { SetText( pchValue ); }

	CPanel2D *m_pButton;
	CLabel *m_pLabel;
	bool m_bAllowRawText;
	CLabel::ETextType m_eTextType;
};


//-----------------------------------------------------------------------------
// Purpose: RadioButton
//-----------------------------------------------------------------------------
class CRadioButton : public CButton
{
	DECLARE_PANEL2D( CRadioButton, CButton );

public:
	virtual void SetSelected( bool bSelected ) OVERRIDE;
	void SetText( const char *pchText, CLabel::ETextType eTextType = CLabel::k_ETextTypeNone );
	void SetAllowRawText( bool bAllow );
	const char *GetGroup() { return m_sGroup.String(); }
	void SetGroup( const char *pchGroup ) { m_sGroup = pchGroup; }
	CPanel2D *GetSelectedButton() const { return m_pPanelSelected.Get(); }

	virtual bool BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties ) OVERRIDE;

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	CLabel *GetLabel() { return m_pLabel; }

public:
	CRadioButton( CPanel2D *parent, const char * pchPanelID );
	virtual ~CRadioButton();

	// events:

	// i have been activated
	bool EventActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );

	// the specified radio, member of the specified group, has been activated (broadcast)
	bool EventOtherActivated( const CPanelPtr< IUIPanel > &pPanel, const char *szGroup );

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE
	{
		BaseClass::ValidateClientPanel( validator, pchName );
		VALIDATE_SCOPE();
		ValidateObj( m_sGroup );
	}
#endif
protected:
	void FireSelectionEvent();

	CPanel2D *m_pButton;
	CPanelPtr< CPanel2D > m_pPanelSelected;
	CLabel *m_pLabel;
	CLabel::ETextType m_eTextType;
	bool m_bAllowRawText;
	CUtlString m_sGroup;
};

} // namespace panorama

#endif // PANORAMA_BUTTON_H
