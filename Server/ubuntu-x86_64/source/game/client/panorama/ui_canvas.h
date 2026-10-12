//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/source2/renderpanel.h"
#include "game/client/irendercaptureconfiguration.h"
#include "mathlib/mathlib.h"
#include "mathlib/camera.h"

#include "shaderapi/IShaderDevice.h"


class CUI_CanvasRenderer;

//-----------------------------------------------------------------------------
// Purpose: Drawing Canvas
//-----------------------------------------------------------------------------
class CUI_Canvas : public panorama::CRenderPanel, public IShaderDeviceDependentObject
{
	DECLARE_PANEL2D( CUI_Canvas, panorama::CRenderPanel );

public:
	CUI_Canvas( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CUI_Canvas();

	void Clear( const Color &clearColor );
	void DrawLine( const Vector2D& v0, const Vector2D& v1, const Color& drawColor );
 	void DrawThickLine( const Vector2D &v0, const Vector2D &v1, float flThickness, const Color &drawColor );
 	void DrawRect( const Vector2D& v0, const Vector2D& v1, const Color& drawColor, IMaterial *pMaterial );
 	void DrawRect( const Vector2D& v0, const Vector2D& v1, const Vector2D& uv0, const Vector2D& uv1, const Color& drawColor, IMaterial *pMaterial );
 	void DrawLineRect( const Vector2D& v0, const Vector2D& v1, const Color& drawColor );
 	void DrawFilledCircle( const Vector2D& vPos, float flRadius, const Color& drawColor );
 	void DrawFilledWedge( const Vector2D& vPos, float flRadius, float flStartAngle, float flAngleDelta, const Color& drawColor );
 	void DrawLineCircle( const Vector2D& vPos, float flRadius, const Color& drawColor );
 	void DrawPoly( int nPointCount, const Vector2D *pPoints, const Color *pColors );
 	void DrawPoly( int nPointCount, const Vector2D *pPoints, const Color &color );
 	void DrawLinePoints( int nPointCount, const Vector2D *pPoints, const Color &color );
	void DrawThickLinePoints( int nPointCount, const Vector2D *pPoints, float flThickness, const Color &color );
	void DrawRects( int nPointCount, const Vector2D *pPoints, const Vector2D *pUVs, const Color* pColors, IMaterial *pMaterial );

	void EnableDrawing( bool bEnable ) { m_bAllowDrawing = bEnable; }
	void SetDrawSize( const float flBrushSize ) { m_flDrawSize = flBrushSize; }
	void SetDrawColor( const Color &drawColor ) { m_drawColor = drawColor; }

	// JS
	bool ClearJS( const char *szColor );
	bool SetDrawColorJS( const char *szColor );
	bool SetDrawSizeJS( const float flDrawSize );
	bool DrawLinePointsJS( int nPointCount, v8::Local<v8::Array> points, float flThickness, const char *szColor );
	bool DrawShadedPolyJS( int nPointCount, v8::Local<v8::Array> points, v8::Local<v8::Array> colors );
	bool DrawPolyJS( int nPointCount, v8::Local<v8::Array> points, const char *szColor );
	bool DrawLineCircleJS( float flcX, float flcY, float flRadius, const char *szColor );
	bool DrawFilledCircleJS( float flcX, float flcY, float flRadius, const char *szColor );
	bool DrawFilledWedgeJS( float flcX, float flcY, float flRadius, float flStartAngle, float flAngleDelta, const char *szColor );

	// Inherited from CRenderPanel
 	virtual bool BSetProperties( const CUtlVector< panorama::ParsedPanelProperty_t > &vecProperties ) OVERRIDE;
 	virtual bool OnGamePadDown( const panorama::GamePadData_t &code ) OVERRIDE;
 	virtual bool OnKeyTyped( const panorama::KeyData_t &unichar ) OVERRIDE;
 	virtual bool OnKeyDown( const panorama::KeyData_t &unichar ) OVERRIDE;
 	virtual bool OnKeyUp( const panorama::KeyData_t &unichar ) OVERRIDE;
 	virtual bool OnMouseButtonDown( const panorama::MouseData_t &code ) OVERRIDE;
 	virtual bool OnMouseButtonUp( const panorama::MouseData_t &code ) OVERRIDE;
 	virtual bool OnMouseWheel( const panorama::MouseData_t &code ) OVERRIDE;
 	virtual void OnMouseMove( float flMouseX, float flMouseY ) OVERRIDE;
	virtual bool BHasLayoutTraversed() { return m_bHasLayoutTraversed; }

	void SetOnCanvasFirstLayoutEvent( panorama::IUIEvent *pEvent );
	void ClearOnCanvasFirstLayoutEvent();
	void SetOnCanvasLayoutEvent( panorama::IUIEvent *pEvent );
	void ClearOnCanvasLayoutEvent();

	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE;
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// TODO: handle device lost/reset 
	// IShaderDeviceDependentObject methods
 	virtual void DeviceLost( void );
 	virtual void DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd );
 	virtual void ScreenSizeChanged( int width, int height );

	// Disable the automatic UI factor scaling (when you want to use device pixels
	// instead of 1080p scaled pixels for your positions.)
	void SetScalingDisabled( bool bDisabled );

	void SetAntialias( bool bAntialias );

private:
	// Override to know that we're actually going to render
	virtual void Paint() OVERRIDE;
	virtual void OnVisibilityChanged() OVERRIDE;

	void Init();

	bool OnReadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );
	bool OnUnreadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );

	bool EnableRendering( bool bEnable );

private:
	void ClearCommandQueue( void );
	void EnqueueCommand( class CUI_Canvas_DrawCommand *pCmd );

	CUI_CanvasRenderer *m_pRenderer;
	class CUI_Canvas_DrawCommand *m_pCommandQueue;
	class CUI_Canvas_DrawCommand **m_pCommandQueueTail;

	bool m_bClearQueue;
	bool m_bScalingDisabled;
	bool m_bHasLayoutTraversed;

	bool m_bAllowDrawing;

	bool m_bAllowSuspendRepaint;
	static int s_nSuspendRepaintCount;

	bool m_bDrawing;
//	bool m_bIsLiveView;

	float m_flLastMouseX;
	float m_flLastMouseY;
//	float m_flLastTime;
	float m_flDrawSize;
	Color m_drawColor;


	CUtlString m_sPanoramaSurfaceXML;
	int m_nPanoramaSurfaceWidth;
	int m_nPanoramaSurfaceHeight;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CUI_CanvasRenderer : public panorama::CRenderThreadCallback
{
public:
	CUI_CanvasRenderer();

	virtual void RenderThreadCallback( Vector4D *pScissorRect, float x0, float y0, float x1, float y1, bool bEnableAA );

	float GetUIScaleX() const { return m_flUIScaleX; }
	float GetUIScaleY() const { return m_flUIScaleY; }

	void OnTick();
	void SetRenderWithAA( bool bRenderWithAA ) { m_bRenderWithAA = bRenderWithAA; }

	CMaterialReference  m_WhiteMaterial;

private:
	~CUI_CanvasRenderer();

	void EnableRendering( bool bEnable ) { m_bEnableRendering = bEnable; m_bNeedsRedraw = true; }
	bool IsRenderingEnabled() { return ( m_bEnableRendering ); }

	// specify if we can use the current version of the panel RT or if it needs re-drawing
	void SetNeedsRedraw( bool bNeedsRedraw ) { m_bNeedsRedraw = bNeedsRedraw; }
	bool NeedsRedraw() { return m_bNeedsRedraw; }

	void BeginAARendering();
	void EndAARendering();

	void BeginPaint( int iLeft, int iTop, int iRight, int iBottom, Vector4D *pScissorAttribute );
	void EndPaint( bool bIgnoreAlphaWhenCompositing );
	void OnPaint();

	void TempLines();

	Camera_t m_RenderCaptureCamera;

	CMaterialReference  m_FullScreenBufferMaterial;
	CMaterialReference  m_FXAAMaterial;
	CTextureReference   m_FullScreenBuffer;

	int	m_nRenderWidth, m_nRenderHeight;
	bool m_bInAARendering;
	Vector4D m_vecScissorAttribute;

	int m_n3DLeft, m_n3DRight, m_n3DTop, m_n3DBottom;
	float m_flClipLeft, m_flClipRight, m_flClipTop, m_flClipBottom;
//	float m_flAspect;

	float m_flUIScaleX;
	float m_flUIScaleY;

	float m_flAutoPlayTimeBase;

//	bool m_bMSAAEnabled;
	class CUI_Canvas_DrawCommand *m_pDrawCommands;
	class CUI_Canvas_DrawCommand **m_pDrawCommandsTail;

	void ClearDrawCommands();
//	void SetMSAAEnabled( bool bEnabled ) { m_bMSAAEnabled = bEnabled; }

	bool m_bRenderWithAA;
	bool m_bForceFXAA;

	bool m_bNeedsRedraw;
	bool m_bEnableRendering;

	bool m_bInPaintMode;

	friend class CUI_Canvas;
};