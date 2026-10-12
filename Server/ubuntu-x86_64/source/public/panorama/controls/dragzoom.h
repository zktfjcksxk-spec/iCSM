//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//


#ifndef DRAG_ZOOM_H
#define DRAG_ZOOM_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/controls/panel2d.h"

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: Drag Zoom - If you have a panel that doesn't fit into your parent panels viewport, use this panel for zoom
//-----------------------------------------------------------------------------
class CDragZoom : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CDragZoom, panorama::CPanel2D );

public:
	CDragZoom( panorama::CPanel2D *pParent, const char *pchID, bool bStartScaled = true, int nNumMouseWheelTicks = 8 );
	virtual ~CDragZoom( );

	virtual bool BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties ) OVERRIDE;

	virtual void OnMouseMove( float flMouseX, float flMouseY ) OVERRIDE;
	CPanel2D *GetContentPanel();

	bool BIsScaled( ) { return m_bIsScaled; }
	virtual EMouseCursors GetMouseCursor() OVERRIDE;
	void SetHorizontalPositionPercentage( float flHorizontalPercentage );
	void SetVerticalPositionPercentage( float flVerticalPercentage );
	void SetHorizontalPosition( float flRequestedPositionX );
	void SetVerticalPosition( float flRequestedPositionY );

	void ToggleFullZoom( float flNormalizedCenterX = 0.5f, float flNormalizedCenterY = 0.5f );

protected:
	virtual bool OnMouseButtonUp( const panorama::MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonDown( const panorama::MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonDoubleClick( const panorama::MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseWheel( const panorama::MouseData_t &code ) OVERRIDE;

	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );

	virtual void OnScaleChanged( float flScale ) {}

private:

	bool OnPanelLoaded( const CPanelPtr< IUIPanel > &panelPtr );
	void ToggleFullZoomInternal( float flMouseX, float flMouseY );

	float m_flLastMouseY;
	float m_flLastMouseX;
	float m_flLastPositionY;
	float m_flLastPositionX;
	bool m_bIsScaled;
	bool m_bIsMouseDown;
	float m_flLastScale;
	uint32 m_unNumMouseWheelTicks;
	uint32 m_unZoomPosition;
	CPanel2D *m_pSubPanel;
	int m_nMouseDelta;
	float m_flContentHeight;
	float m_flContentWidth;
	float m_flLayoutHeight;
	float m_flLayoutWidth;
	float m_flMinScale;
	float m_bFirstLoad;

	float m_flExtraZoomOut;

	void ClampTransformValues( float *pflTransformX, float *pflTransformY );
	bool CheckContentPanelSize();
	void ApplyUpdatedTransform( float flTransformX, float flTransformY );
	void EnableFastTransform( bool bIsFast );
	void UpdateContentLayoutValues();
	void SetScaleInternal( float flScale );
};

} // namespace panorama

#endif // DRAG_ZOOM_H