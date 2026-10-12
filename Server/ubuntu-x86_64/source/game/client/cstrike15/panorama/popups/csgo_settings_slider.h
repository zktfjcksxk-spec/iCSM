//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"
#include "panorama/panoramasymbol.h"
#include "csgo_iconvar_panorama_setter.h"

namespace panorama
{
	class CLabel;
	class CSlider;
}

class CSGO_SettingsSlider : public panorama::CPanel2D, public CCSGO_iConvarPanoramaSetter
{
	DECLARE_PANEL2D( CSGO_SettingsSlider, panorama::CPanel2D );

public:
	CSGO_SettingsSlider( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CSGO_SettingsSlider( );
	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue );
	virtual void OnShow();
	virtual ConVarRef& GetConVarRef() OVERRIDE { return m_ConVar; }

	void SetValue( float flValue ) { m_pSlider->SetValue( flValue ); }
	float GetValue() const { return m_pSlider->GetValue(); }
	void SetMin( float flMin ) { m_pSlider->SetMin( flMin ); }
	void SetMax( float flMax ) { m_pSlider->SetMax( flMax ); }
	float GetMin() const { return m_pSlider->GetMin(); }
	float GetMax() const { return m_pSlider->GetMax(); }
	float ActualValue() const { return Lerp( m_pSlider->GetValue(), m_flMinVal, m_flMaxVal ); }

	void AddClass( const char *pchClassName );
	void RemoveClass( const char *pchClassName );

	void RestoreCVarDefault();

	virtual bool BIsClientPanelEvent( panorama::CPanoramaSymbol symProperty ) OVERRIDE;
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

private:
	bool EventSliderValueChanged( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, float flValue );
	bool EventTextEntrySubmit( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, const char *pchText );
	void UpdateLabel( float flValue );
	void UpdateOutOfBounds( float flValue );
	float GetVarValue();

	panorama::CLabel		*m_pSliderTitle;
	panorama::CTextEntry	*m_pSliderValue;
	panorama::CSlider		*m_pSlider;
	ConVarRef				m_ConVar;
	float					m_flMinVal;
	float					m_flMaxVal;
	float					m_flMinDisplayPercentage;
	float					m_fl100PercentValue;
	bool					m_bDisplayTextAsPercent;
	bool					m_bInvertConvar;
	bool					m_bConstrainToRange;
	int						m_nDisplayPrecision;
};
