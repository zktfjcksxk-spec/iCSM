//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Control to present buttons using a steam controller's touchpad
//=============================================================================//

#ifndef PANORAMA_TOUCHPAD_H
#define PANORAMA_TOUCHPAD_H

#include "panorama/controls/panel2d.h"
#include "panorama/controls/panelptr.h"
#include "panorama/input/iuiinput.h"
#include "panorama/uievent.h"

namespace panorama
{
	class CTouchPad;

	DECLARE_PANORAMA_EVENT3( TouchKeyStyleChanged, CPanelPtr<CPanel2D>, const char *, bool );
	DECLARE_PANORAMA_EVENT2( TouchKeyClicked, CPanel2D*, CTouchPad* );
	
	enum EKeyOverlapTestBehavior
	{
		kOverlapTest_ExtendLeftRightQuadrantsToInfinity,
		kOverlapTest_OverlapActualPositionIgnoringDeadzone,
		kOverlapTest_OnlyTestNeighbors,
	};

	class CTouchPad
	{
	public:
		bool Initialize( CPanel2D *pParent, const char *pointerID, const char *padID, const char *pszTouchPadActiveClass,
							IUIEngine::EHapticFeedbackPosition eHapticsPosition, bool bFingerOnPad );
		void UpdatePointerState( bool bSteamPadHardwarePointersEnabled );
		void OnTouch( void );
		void OnRelease( void );
		bool OnMove( float touchX, float touchY );
		void OnButtonDown( void );
		
		bool OverlapsTouchKey( float pointerX, float pointerY, float fParentPadX, float fParentPadY, IUIPanel *pTouchKey, EKeyOverlapTestBehavior eOverlapTest );
		
		SteamPadPointer_t m_renderPointerState;
		
		const char *m_pszTouchPadActiveClass;
		CPanel2D *m_pPointerPanel;
		CPanel2D *m_pPadPanel;
		CPanelPtr<CPanel2D> m_pHoverKey;				// what key are we currently hovering over?
		CPanelPtr<CPanel2D> m_pLastHoverKey;			// what was the last key we were hovering over? this will either match m_pHoverKey or have the last value m_pHoverKey had if its currently nullptr
		CUtlVector< IUIPanel * > m_vecTouchKeys;
		bool m_bFingerOnPad;
		
		float m_hoverX;
		float m_hoverY;
		
		CPanel2D *m_pParent;
		
		IUIEngine::EHapticFeedbackPosition m_eHapticsPosition;
		
		float m_flScaleFactor;
	};
}

#endif // PANORAMA_TOUCHPAD_H