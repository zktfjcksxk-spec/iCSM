//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef NUMBER_ENTRY_H
#define NUMBER_ENTRY_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"

DECLARE_PANORAMA_EVENT0( NumberEntryIncrementValue );
DECLARE_PANORAMA_EVENT0( NumberEntryDecrementValue );
DECLARE_PANEL_EVENT0( NumberEntryChanged );

namespace panorama
{

class CTextEntry;
class CButton;

//-----------------------------------------------------------------------------
// Purpose: Control to pick a number, made of a text entry plus buttons to
// increment and decrement.
//-----------------------------------------------------------------------------
class CNumberEntry : public CPanel2D
{
	DECLARE_PANEL2D( CNumberEntry, CPanel2D );

public:
	CNumberEntry( CPanel2D *parent, const char * pchPanelID );
	virtual ~CNumberEntry();

	void Clear();

	int GetValue() const { return m_nValue; }
	void SetValue( int nValue );

	void SetMin( int nMin );
	int GetMin() const { return m_nMin; }

	void SetMax( int nMax );
	int GetMax() const { return m_nMax; }

	void SetIncrement( int nIncrement );
	int GetIncrement() const { return m_nIncrement; }

	void IncrementValue();
	void DecrementValue();
	void AdjustValue( bool bIncrement );

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;
	virtual bool BIsClientPanelEvent( CPanoramaSymbol symProperty ) OVERRIDE;
	virtual bool OnMouseWheel( const MouseData_t &code ) OVERRIDE;

private:
	bool EventIncrementValue();
	bool EventDecrementValue();
	bool EventTextEntryChanged( const CPanelPtr< IUIPanel > &panelPtr );

	void SetValueInternal( int nValue, bool bUpdateText );
	void UpdateText( const char *pszText );

	CTextEntry *m_pTextEntry;
	CPanel2D *m_pIncrementButton;
	CPanel2D *m_pDecrementButton;

	int m_nMin;
	int m_nMax;
	int m_nIncrement;
	int m_nValue;

	bool m_bHandlingTextChanged;
};


} // namespace panorama

#endif // NUMBERENTRY_H
