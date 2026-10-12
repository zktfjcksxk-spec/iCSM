//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#pragma once

#include "panorama/controls/panel2d.h"

namespace panorama
{

//////////////////////////////////////////////////////////////////////////
//
// progress bar, just sizes two panels to represent progress
//
class CCircularProgressBar: public CPanel2D
{
	DECLARE_PANEL2D( CCircularProgressBar, CPanel2D );
public:
	CCircularProgressBar( CPanel2D *pParent, const char *pchID );
	virtual ~CCircularProgressBar();

	void SetMin( float flMin ) { m_flMin = flMin; UpdateRadialClip(); }
	void SetMax( float flMax ) { m_flMax = flMax; UpdateRadialClip(); }
	void SetValue( float flValue ) { m_flCur = flValue; UpdateRadialClip(); }

	float GetMin() const { return m_flMin; }
	float GetMax() const { return m_flMax; }
	float GetValue() const { return m_flCur; }
	

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

protected:
	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	void UpdateRadialClip();

private:
	float m_flMin;
	float m_flMax;
	float m_flCur;

	CPanel2D *m_ppanelBG;
	CPanel2D *m_ppanelFG;
};

} // namespace panorama


