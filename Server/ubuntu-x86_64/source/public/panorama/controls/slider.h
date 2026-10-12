//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_SLIDER_H
#define PANORAMA_SLIDER_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/controls/panel2d.h"
#include "panorama/controls/button.h"

namespace panorama
{

DECLARE_PANEL_EVENT1( SliderValueChanged, float );
DECLARE_PANEL_EVENT1( SlottedSliderValueChanged, int );
DECLARE_PANEL_EVENT1( SliderFocusChanged, bool );
DECLARE_PANEL_EVENT1( SpinnerValueChanged, float );

//-----------------------------------------------------------------------------
// Purpose: Slider control which includes track, progress & thumb
//-----------------------------------------------------------------------------
class CSlider: public CPanel2D
{
	DECLARE_PANEL2D( CSlider, CPanel2D );

public:
	CSlider( CPanel2D *pParent, const char *pchID );
	virtual ~CSlider();

	enum ESliderDirection
	{
		k_EDirectionVertical,
		k_EDirectionHorizontal
	};

	void SetMin( float flMin ) { m_flMin = flMin; InvalidateSizeAndPosition(); }
	void SetMax( float flMax ) { m_flMax = flMax; InvalidateSizeAndPosition(); }
	float GetMin() const { return m_flMin; }
	float GetMax() const { return m_flMax; }
	void SetIncrement( float flValue ) { m_flIncrement = flValue; }
	float GetIncrement() const { return m_flIncrement; }
	virtual void SetValue( float flValue );
	virtual void SetValueNoEvents( float flValue );
	float GetValue() const { return m_flCur; }
	float GetDefaultValue() const { return m_flDefault; }
	void SetDefaultValue ( float flValue ) { m_flDefault = flValue; }
	void SetShowDefaultValue( bool bShow ) { m_bShowDefault = bShow; }
	virtual void SetDirection( ESliderDirection eValue );
	bool IsDragging() const { return m_bDraggingThumb; }
	bool IsMouseDown() const { return m_bMouseDown; }

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

	virtual bool OnMouseButtonDown( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonUp( const MouseData_t &code ) OVERRIDE;
	virtual void OnMouseMove( float flMouseX, float flMouseY ) OVERRIDE;
	virtual bool OnMoveUp( int nRepeats ) OVERRIDE;
	virtual bool OnMoveRight( int nRepeats ) OVERRIDE;
	virtual bool OnMoveDown( int nRepeats ) OVERRIDE;
	virtual bool OnMoveLeft( int nRepeats ) OVERRIDE;

	virtual bool OnActivate(panorama::EPanelEventSource_t eSource);
	virtual bool OnCancel(panorama::EPanelEventSource_t eSource);
	virtual void OnStyleFlagsChanged();
	virtual void OnResetToDefaultValue();

	virtual bool BIsClientPanelEvent( CPanoramaSymbol symProperty ) OVERRIDE;

	void SetRequiresSelection( bool bRequireSelection ) { m_bRequiresSelection = bRequireSelection; }

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

protected:
	bool EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE;
	void SetValueFromMouse( float x, float y );
	float CalculateValueFromMousePosition( float x, float y );
	ESliderDirection GetDirection() { return m_eDirection; }

	CPanel2D *m_pThumb;
	CPanel2D *m_pTrack;
	CPanel2D *m_pProgress;
	CPanel2D *m_pDefaultTick;
	bool EventActivated( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource );
	bool EventCancelled( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource );
	bool EventStyleFlagsChanged( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );
	bool EventResetToDefault( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );

private:
	bool AllowInteraction( void );
	bool ShouldShowDefault( void ) { return m_bShowDefault; }

	float m_flMin;
	float m_flMax;
	float m_flDefault;
	float m_flCur;
	float m_flLast;
	float m_flIncrement;
	bool m_bRequiresSelection;
	bool m_bDraggingThumb;
	bool m_bShowDefault;
	bool m_bMouseDown;
	float m_flMouseDownTime;
	ESliderDirection m_eDirection;

	float m_flLastMouseX;
	float m_flLastMouseY;
	float m_flMouseDownValueOffset;
};

class CSlottedSlider : public CSlider
{
	DECLARE_PANEL2D( CSlottedSlider, CSlider );
public:
	CSlottedSlider( CPanel2D *pParent, const char *pchID );
	virtual ~CSlottedSlider();

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void SetValue( float flValue ) OVERRIDE;
	void SetValue( int nValue );
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE;
	int GetCurrentNotch() { return m_nCurNotch; }

private:
	int m_nNumNotches;
	int m_nCurNotch;
	CUtlVector< CPanel2D* > m_pNotches;
};


class CSpinner : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CSpinner, panorama::CPanel2D );
public:
	CSpinner( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CSpinner();

	virtual bool OnMouseButtonDown( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonUp( const MouseData_t &code ) OVERRIDE;
	virtual void OnMouseMove( float flMouseX, float flMouseY ) OVERRIDE;

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	bool EventActivated( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource );

	virtual bool OnActivate( panorama::EPanelEventSource_t eSource );

	void SetValueFromMouse( float x, float y );
	void SetValue( float flValue );
	float GetValue() const { return m_flCur; }

	void SetSpinlock( bool bVal ) { m_bSpinlock = bVal; }
	bool GetSpinlock() { return m_bSpinlock; }

	float CalculateValueFromMousePosition( float x, float y );



private:

	float m_flMin;
	float m_flMax;
	int m_nThrow;
	float m_flDefault;
	float m_flCur;
	float m_flLast;

	bool m_bMouseDown;
	float m_flMouseDownTime;

	float m_flLastMouseX;
	float m_flLastMouseY;

	float m_flMouseDownY;

	bool m_bMouseIsSpinning; // used to differentiate clicks from drags

	bool m_bSpinlock; // used to ignore drags when button is locked

};

} // namespace panorama

#endif // PANORAMA_SLIDER_H
