//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/source2/renderpanel.h"
#include "panorama/ui_itempreview_renderer.h"
#include "econ/econ_item_system.h"
#include "game/client/irendercaptureconfiguration.h"
#include "mathlib/mathlib.h"
#include "mathlib/camera.h"
#include "cstrike15/cstrike15_item_inventory.h"
#include "particles/particles.h"
#include "cstrike15/gameui/modelpanel_weaponpreview.h"

#include "shaderapi/IShaderDevice.h"



DECLARE_PANORAMA_EVENT4( GlobalSceneFireEntityInput, const char *, const char *, const char *, const char * );

enum ScenePanelRotationFlags : uint8
{
	SCENE_PANEL_ROTATION_NONE = 0,
	SCENE_PANEL_ROTATION_MOUSE = 1 << 0,
	SCENE_PANEL_ROTATION_MOUSEWHEEL = 1 << 1,
	SCENE_PANEL_ROTATION_EVENT_DRIVEN = 1 << 2,

	SCENE_PANEL_ROTATION_ALL = 0xFF
};

struct UIItemInfo_t
{
	// FIXME: Just guessing at the right defaults for these, revisit.
	UIItemInfo_t():
		m_bInventory(false)
		, m_bRotate( false )
		, m_bAntiAlias( false )
		, m_bEnableRendering( true )
		, m_bEnableFloorShadow( false )
	{}
	CUtlString m_manifestName;		// 
	CUtlString m_itemName;			// 

	bool m_bInventory;

	bool m_bRotate;
	bool m_bAntiAlias;
	bool m_bEnableRendering;
	bool m_bEnableFloorShadow;
};


DECLARE_PANORAMA_EVENT0( UIItemPreviewPanelSceneReload );
DECLARE_PANORAMA_EVENT0( UIItemPreviewPanelCloseDebugCamera );
DECLARE_PANORAMA_EVENT0( UIItemPreviewPanelToggleDebugMode );
DECLARE_PANORAMA_EVENT0( UIItemPreviewPanelCopyToClipboard );
DECLARE_PANORAMA_EVENT0( UIItemPreviewPanelCopyPresetToClipboard ); 
DECLARE_PANORAMA_EVENT0( UIItemPreviewPanelToggleEnabledLights );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelSetScene, const char *, const char *, bool );
DECLARE_PANORAMA_EVENT1( UIItemPreviewPanelSetPlayerModel, const char * );
DECLARE_PANORAMA_EVENT2( UIItemPreviewPanelEquipPlayerFromLoadout, const char *, const char * );
DECLARE_PANORAMA_EVENT1( UIItemPreviewPanelEquipPlayerWithItem, const char * );
DECLARE_PANORAMA_EVENT1( UIItemPreviewPanelResetAnimation, bool );
DECLARE_PANORAMA_EVENT2( UIItemPreviewPanelQueueSequence, const char *, bool );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelLayerSequence, const char *, bool, bool );
DECLARE_PANORAMA_EVENT1( UIItemPreviewPanelSetPanelLightingAmount, float );
DECLARE_PANORAMA_EVENT1( UIItemPreviewPanelSetFlashlightAmount, float );
DECLARE_PANORAMA_EVENT1( UIItemPreviewPanelSetDirectionalLightModify, int );
DECLARE_PANORAMA_EVENT4( UIItemPreviewPanelSetDirectionalLightPulseFlicker, float, float, float, float );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelSetDirectionalLightRotation, float, float, float );
DECLARE_PANORAMA_EVENT1( UIItemPreviewPanelSetDirectionalLightAmount, float );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelSetDirectionalLightColor, float, float, float );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelSetDirectionalLightDirection, float, float, float );
DECLARE_PANORAMA_EVENT4( UIItemPreviewPanelSetFlashlightPulseFlicker, float, float, float, float );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelSetFlashlightRotation, float, float, float );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelSetFlashlightColor, float, float, float );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelSetFlashlightPosition, float, float, float );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelSetFlashlightAngle, float, float, float );
DECLARE_PANORAMA_EVENT1( UIItemPreviewPanelSetFlashlightFOV, float );
DECLARE_PANORAMA_EVENT2( UIItemPreviewPanelSetFlashlightNearFarZ, float, float );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelSetAmbientLightColor, float, float, float );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelSetSceneRotation, float, float, float );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelSetSceneAngles, float, float, float );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelSetCameraPosition, float, float, float );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelSetCameraAngles, float, float, float );
DECLARE_PANORAMA_EVENT2( UIItemPreviewPanelSetCameraPreset, int, bool );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelSetSceneIntroRotation, float, float, bool );
DECLARE_PANORAMA_EVENT2( UIItemPreviewPanelSetSceneIntroFOV, float, float );
DECLARE_PANORAMA_EVENT1( UIItemPreviewPanelSetEconItemTextureSize, int );
DECLARE_PANORAMA_EVENT1( UIItemPreviewPanelSetFloatingFloorAlpha, float );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelSetParticleSystemOffsetPosition, float, float, float );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelSetParticleSystemOffsetAngles, float, float, float );
DECLARE_PANORAMA_EVENT3( UIItemPreviewPanelAddParticleSystem, const char *, const char *, bool );
DECLARE_PANORAMA_EVENT0( UIItemPreviewPanelDebugLightSelection );
DECLARE_PANORAMA_EVENT0( UIItemPreviewPanelDebugToggleSection_LightColor );
DECLARE_PANORAMA_EVENT0( UIItemPreviewPanelDebugToggleSection_LightAnim );
DECLARE_PANORAMA_EVENT0( UIItemPreviewPanelDebugToggleSection_LightFlashlightShadow );
DECLARE_PANORAMA_EVENT0( UIItemPreviewPanelDebugResetLightAnim_RotX );
DECLARE_PANORAMA_EVENT0( UIItemPreviewPanelDebugResetLightAnim_RotY );
DECLARE_PANORAMA_EVENT0( UIItemPreviewPanelDebugResetLightAnim_RotZ );
DECLARE_PANORAMA_EVENT0( UIItemPreviewPanelTogglePause );
DECLARE_PANORAMA_EVENT1( UIItemPreviewPanelPause, bool );
DECLARE_PANORAMA_EVENT1( UIItemPreviewPanelEnableRendering, bool );
DECLARE_PANORAMA_EVENT0( UIItemPreviewPanelSetAsActivePreviewPanel );


#define PREVIEW_PAINTKIT_ID_BASE	0x42420

#define MAX_PANEL_LIGHTS 4

//-----------------------------------------------------------------------------
// Helper functions for the Workshop Workbench Dialog
//-----------------------------------------------------------------------------

enum RotateAxis
{
	RotateAxis_X,
	RotateAxis_Y,
	RotateAxis_Z,

	NUM_ROTATEAXIS,
	RotateAxis_Invalid
};


//-----------------------------------------------------------------------------
// Purpose: Displays debug info
//-----------------------------------------------------------------------------
class CUI_ItemPreviewPanel;
class CUI_ItemPreviewDebug;

extern CUI_ItemPreviewPanel *g_pActivePreviewPanel;

//-----------------------------------------------------------------------------
// Custom panel types
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// HSV Sliders
//-----------------------------------------------------------------------------
class CUI_ItemPreviewColorSlider : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CUI_ItemPreviewColorSlider, panorama::CPanel2D );

public:
	CUI_ItemPreviewColorSlider( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CUI_ItemPreviewColorSlider();

//	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue );
	virtual void OnShow();

	void SetValue( float flValue ) { m_pSlider->SetValue( flValue ); }
	float GetValue() const { return m_pSlider->GetValue(); }

	void SetMin( float flMin ) { m_pSlider->SetMin( flMin ); m_flMinVal = flMin; }
	void SetMax( float flMax ) { m_pSlider->SetMax( flMax ); m_flMaxVal = flMax;  }
	float GetMin() const { return m_pSlider->GetMin(); }
	float GetMax() const { return m_pSlider->GetMax(); }

	//void SetTitle( const char *szTitle ) { m_pSliderTitle->SetText( szTitle ); };

	//virtual bool BIsClientPanelEvent( panorama::CPanoramaSymbol symProperty ) OVERRIDE;

	void SetItemPreviewDebugPanel( CUI_ItemPreviewDebug *pItemPreviewDebug ) { m_pItemPreviewDebug = pItemPreviewDebug; }

private:
	bool EventSliderValueChanged ( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, float flValue );

	CUI_ItemPreviewDebug *m_pItemPreviewDebug;

	float				m_flMinVal;
	float				m_flMaxVal;

	panorama::CSlider	*m_pSlider;
};

//-----------------------------------------------------------------------------
// Animated light/regular slider
//-----------------------------------------------------------------------------
class CUI_ItemPreviewSlider : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CUI_ItemPreviewSlider, panorama::CPanel2D );

public:
	CUI_ItemPreviewSlider( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CUI_ItemPreviewSlider();

	//	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue );
	virtual void OnShow();

	void SetDefaultValue( float flValue ) { m_pSlider->SetDefaultValue( flValue ); }
	void SetShowDefaultValue( bool bShow ) { m_pSlider->SetShowDefaultValue( bShow ); }
	void SetValue( float flValue ) { m_pSlider->SetValue( flValue ); }
	float GetValue() const { return m_pSlider->GetValue(); }

	void SetMin( float flMin ) { m_pSlider->SetMin( flMin ); m_flMinVal = flMin; }
	void SetMax( float flMax ) { m_pSlider->SetMax( flMax ); m_flMaxVal = flMax; }
	float GetMin() const { return m_pSlider->GetMin(); }
	float GetMax() const { return m_pSlider->GetMax(); }

	void SetItemPreviewDebugPanel( CUI_ItemPreviewDebug *pItemPreviewDebug ) { m_pItemPreviewDebug = pItemPreviewDebug; }

	enum eItemPreviewSliderType
	{
		ITEMPREVIEWSLIDER_ANIMATEDLIGHT,
		ITEMPREVIEWSLIDER_FLASHLIGHTSHADOW,
		ITEMPREVIEWSLIDER_CAMERA
	};

	void SetType( eItemPreviewSliderType eType ) { m_type = eType; }
	void SetIncrement ( float flInc ) { m_pSlider->SetIncrement( flInc ); }

private:
	bool EventSliderValueChanged ( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, float flValue );

	CUI_ItemPreviewDebug *m_pItemPreviewDebug;

	float				m_flMinVal;
	float				m_flMaxVal;

	panorama::CSlider	*m_pSlider;

	eItemPreviewSliderType	m_type;

};

//-----------------------------------------------------------------------------
// Debug panel
//-----------------------------------------------------------------------------
class CUI_ItemPreviewDebug : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CUI_ItemPreviewDebug, panorama::CPanel2D );

public:
	CUI_ItemPreviewDebug( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CUI_ItemPreviewDebug() {}

	virtual bool OnKeyDown( const panorama::KeyData_t &unichar ) OVERRIDE;
	virtual bool OnKeyUp( const panorama::KeyData_t &unichar ) OVERRIDE;
	virtual bool OnMouseButtonDown( const panorama::MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonUp( const panorama::MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseWheel( const panorama::MouseData_t &code ) OVERRIDE;
	virtual void OnMouseMove( float flMouseX, float flMouseY ) OVERRIDE;

	void SetItemPreviewRenderer( CUI_ItemPreviewRenderer *pItemPreviewRenderer ) { m_pItemPreviewRenderer = pItemPreviewRenderer; }
	void SetItemPreviewPanel( CUI_ItemPreviewPanel *pItemPreviewPanel ) { m_pItemPreviewPanel = pItemPreviewPanel; }
	void Show();

	bool ToggleDebugMode();
	void SetupDebugMode();

	void UpdateHSVPanels( Vector colRGB, bool bUpdateSliderValues = false );
	void UpdateLightAnimPanels( QAngle rot, Vector4D pulseFlicker, bool bUpdateSliderValues = false );
	void UpdateFlashlightShadowPanels ( float flFOV, float flNearZ, float flFarZ, bool bUpdateSliderValues = false );

	void SetDebugLightFromHSV();
	void SetHSVFromDebugLight();

	void SetAnimatedLightsFromUI();
	void SetFlashlightShadowFromUI();

	bool SelectDebugLight();
	bool ToggleEnabledLights();

	bool ToggleSection_LightColor();
	bool ToggleSection_LightAnim();
	bool ToggleSection_LightFlashlightShadow();

	bool ResetLightAnim_RotX();
	bool ResetLightAnim_RotY();
	bool ResetLightAnim_RotZ();

	void UpdateCameraFOVPanels( float flFOV, bool bUpdateSliderValues );
	void SetCameraFOVFromUI();


#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE
	{
		VALIDATE_SCOPE();
		ValidateObj( m_scheduledUpdate );
	}
#endif

	enum eDebugMode
	{
		DEBUGMODE_CAMERA,
		DEBUGMODE_LIGHTS
	};

	enum eDebugLight
	{
		DEBUGMODE_LIGHT_NONE,

		DEBUGMODE_LIGHT_FLASHLIGHT,
		DEBUGMODE_LIGHT_DIR0,
		DEBUGMODE_LIGHT_DIR1,
		DEBUGMODE_LIGHT_DIR2,
		DEBUGMODE_LIGHT_DIR3,
		DEBUGMODE_LIGHT_AMBIENT,
	};

	eDebugMode GetMode() { return m_debugMode; }	
	eDebugLight GetActiveDebugLight();
	bool BEnabledAllLights() { return m_bEnableAllLights; }

private:
	void Update();

	bool SceneReload();
	bool CloseDebugCamera();
	bool CopyToClipboard();
	bool CopyCameraAsPresetToClipboard();
	bool CopyCameraAsFlashlightToClipboard ();

	CUI_ItemPreviewRenderer *m_pItemPreviewRenderer;
	CUI_ItemPreviewPanel *m_pItemPreviewPanel;

	panorama::CLabel *m_pCameraPos;
	panorama::CLabel *m_pCameraAng;
	panorama::CLabel *m_pOrbitPivot;

	panorama::CLabel *m_pDebugModeTooltipLabelCamera;
	panorama::CLabel *m_pDebugModeTooltipLabelLights;

	panorama::CPanel2D *m_pDebugModeLightPanel;

	panorama::CLabel *m_pDebugModeLightSelectionLabel;
	panorama::CDropDown *m_pDebugLightSelectionDropDown;

	panorama::CPanel2D *m_pDebugModeLightColorSettings;
	panorama::CPanel2D *m_pLightColorDropdown;
	panorama::CImagePanel *m_pLightColorDropdownIcon;

	panorama::CPanel2D *m_pDebugModeLightAnimSettings;
	panorama::CPanel2D *m_pLightAnimDropdown;
	panorama::CImagePanel *m_pLightAnimDropdownIcon;

	panorama::CPanel2D *m_pDebugModeLightFlashlightShadowSettings;
	panorama::CPanel2D *m_pLightFlashlightShadowDropdown;
	panorama::CImagePanel *m_pLightFlashlightShadowDropdownIcon;

	panorama::CPanel2D *m_pDebugModeCameraFOVSettings;

	panorama::CPanel2D *m_pDebugModeEnabledLightsToggleButton;
	panorama::CLabel *m_pDebugModeLabelEnabledAllLights;
	panorama::CLabel *m_pDebugModeLabelEnabledSelectedLight;

	CUI_ItemPreviewColorSlider *m_pLightColorSlider_H;
	CUI_ItemPreviewColorSlider *m_pLightColorSlider_S;
	CUI_ItemPreviewColorSlider *m_pLightColorSlider_V;
	CUI_ItemPreviewColorSlider *m_pLightColorSlider_HDR;

	panorama::CPanel2D *m_pLightColorRGBBox;

	panorama::CPanel2D *m_pDebugModeLightRot;
	panorama::CPanel2D *m_pDebugModeLightPulse;
	panorama::CPanel2D *m_pDebugModeLightFlicker;

	panorama::CLabel *m_pLightRotLabel_X;
	panorama::CLabel *m_pLightRotLabel_Y;
	panorama::CLabel *m_pLightRotLabel_Z;

	CUI_ItemPreviewSlider *m_pLightRotSlider_X;
	CUI_ItemPreviewSlider *m_pLightRotSlider_Y;
	CUI_ItemPreviewSlider *m_pLightRotSlider_Z;

	CUI_ItemPreviewSlider *m_pLightPulseSlider_A;
	CUI_ItemPreviewSlider *m_pLightPulseSlider_F;

	CUI_ItemPreviewSlider *m_pLightFlickerSlider_A;
	CUI_ItemPreviewSlider *m_pLightFlickerSlider_F;

	CUI_ItemPreviewSlider *m_pFlashlightShadowFOVSlider;
	panorama::CLabel *m_pFlashlightShadowFOVLabel;

	CUI_ItemPreviewSlider *m_pFlashlightShadowNearZSlider;
	panorama::CLabel *m_pFlashlightShadowNearZLabel;
	CUI_ItemPreviewSlider *m_pFlashlightShadowFarZSlider;
	panorama::CLabel *m_pFlashlightShadowFarZLabel;

	CUI_ItemPreviewSlider *m_pCameraFOVSlider;
	panorama::CLabel *m_pCameraFOVLabel;

	panorama::CButton *m_pCameraAsPresetButton;

	float m_flhdrScale;

	panorama::CLabel *m_pLightColorLabel_HDR;

	panorama::CUIScheduledDel m_scheduledUpdate;

	eDebugMode m_debugMode;

	bool m_bEnableAllLights;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
struct sEquipItem
{
	CUtlString m_sName;
	int m_nTeam;
	int m_nPos;
	bool m_bEquipped;
};

//-----------------------------------------------------------------------------
// Purpose: Map & Model rendering class
//-----------------------------------------------------------------------------
class CUI_ItemPreviewPanel : public panorama::CRenderPanel, public IShaderDeviceDependentObject
{
	DECLARE_PANEL2D( CUI_ItemPreviewPanel, panorama::CRenderPanel );

	// Inherited from CRenderPanel
public:
	virtual bool BSetProperties( const CUtlVector< panorama::ParsedPanelProperty_t > &vecProperties ) OVERRIDE;
	virtual bool OnGamePadDown( const panorama::GamePadData_t &code ) OVERRIDE;
	virtual bool OnKeyTyped( const panorama::KeyData_t &unichar ) OVERRIDE;
	virtual bool OnKeyDown( const panorama::KeyData_t &unichar ) OVERRIDE;
	virtual bool OnKeyUp( const panorama::KeyData_t &unichar ) OVERRIDE;
	virtual bool OnMouseButtonDown( const panorama::MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonUp( const panorama::MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseWheel( const panorama::MouseData_t &code ) OVERRIDE;
	virtual void OnMouseMove( float flMouseX, float flMouseY ) OVERRIDE;
	virtual bool BShouldAlwaysRepaint() OVERRIDE;

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// TODO: handle device lost/reset 
	// IShaderDeviceDependentObject methods
 	virtual void DeviceLost( void );
 	virtual void DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd );
 	virtual void ScreenSizeChanged( int width, int height );

	// for debug panel
	bool SceneReload();
	bool CloseDebugCamera();
	bool ToggleDebugMode();

	bool CopyDebugCameraSettingsToClipboard();
	bool CopyPresetCameraSettingsToClipboard();
	bool CopyFlashlightSettingsToClipboard();
	bool CopyAllLightSettingsToClipboard();

	// Other public methods
	CUI_ItemPreviewPanel( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CUI_ItemPreviewPanel();

	bool SetScene( const UIItemInfo_t &info, bool bReInit = true );
	void ClearScene();

	uint8 GetRotationFlags() const { return m_unRotationFlags; }
	void SetRotationFlags( uint8 unRotationFlags );

	void SetAllowSuspendRepaint( bool bAllow );
	void SetAntialias( bool bAntialias );

	void SetIsLiveView( bool bEnable );

	static void StartSuspendingRepaint() { s_nSuspendRepaintCount++; }
	static void StopSuspendingRepaint() { s_nSuspendRepaintCount--; if ( s_nSuspendRepaintCount < 0 ) { Assert( false ); s_nSuspendRepaintCount = 0; } }

	virtual void GetDebugPropertyInfo( CUtlVector< panorama::DebugPropertyOutput_t *> *pvecProperties ) OVERRIDE;

	bool SetEconItemTextureSize( int nSize );

	float GetItemRotation( int axis ) { return m_aItemRotate[ axis ][ 0 ]; }
	void SetItemRotation( int axis, float flRotation ) { m_aItemRotate[ axis ][ 0 ] = m_aItemRotate[ axis ][ 1 ] = flRotation; m_itemRotateSpeed[ axis ] = 0.0f; }
	void SetItemRotationSpeedTarget( int axis, float flRotationSpeed, bool bInstantly = false ) { m_itemRotateSpeedTarget[ axis ] = flRotationSpeed; if ( bInstantly ) { m_itemRotateSpeed[ axis ] = m_itemRotateSpeedTarget[ axis ]; } }
	void SetItemRotationAcceleration( int axis, float flAcceleration ) { m_itemRotateAcceleration[ axis ] = flAcceleration; }

	void ToggleAnimatingLights();
	void ReCentreFlashlightOnCamera();

	bool SetDirectionalLightModify( int nLight );
	bool SetDirectionalLightPulseFlicker( float flPulseFrequency, float flPulseAmount, float flFlickerRate, float flFlickerAmount );
	bool SetDirectionalLightRotation( float flRotX, float flRotY, float flRotZ );
	bool SetDirectionalLightAmount( float flAmount );
	bool SetDirectionalLightColor( float flColR, float flColG, float flColB );
	bool SetDirectionalLightDirection( float flDirX, float flDirY, float flDirZ );
	bool SetFlashlightPulseFlicker( float flPulseFrequency, float flPulseAmount, float flFlickerRate, float flFlickerAmount );
	bool SetFlashlightRotation( float flRotX, float flRotY, float flRotZ );
	bool SetFlashlightColor( float flColR, float flColG, float flColB );
	bool SetFlashlightPosition( float flPosX, float flPosY, float flPosZ );
	bool SetFlashlightAngle( float flAngX, float flAngY, float flAngZ );
	bool SetFlashlightFOV( float flFOV );
	bool SetFlashlightNearFarZ( float flNearZ, float flFarZ );
	bool SetFlashlightNearFarZFromSceneBounds();
	bool SetAmbientLightColor( float flColR, float flColG, float flColB );

	void GetDirectionalLightPulseFlicker( int nLight, Vector4D &vPulseFlicker );
	void GetDirectionalLightRotation( int nLight, QAngle &rot );
	void GetDirectionalLightColor( int nLight, Vector &vCol );
	void GetDirectionalLightDirection( int nLight, Vector &vDir );
	void GetFlashlightPulseFlicker( Vector4D &vPulseFlicker );
	void GetFlashlightRotation( QAngle &rot );
	void GetFlashlightColor( Vector &vCol );
	void GetFlashlightPosition( Vector &vDir );
	void GetFlashlightAngle( QAngle &ang );
	void GetAmbientLightColor( Vector &vCol );

	bool SetSceneIntroRotation ( float flLR, float flTime, bool bLoop );
	bool SetSceneIntroFOV ( float flScale, float flTime );

	bool SetAsActivePreviewPanel();
	void ApplySticker( int nSlot, int nStickerId );
	static CUI_ItemPreviewPanel *GetActivePreviewPanel() { return g_pActivePreviewPanel; }

	CEconItemView *GetWeaponPreviewItemData();

private:
	// Override to know that we're actually going to render
	virtual void Paint() OVERRIDE;
	virtual void OnVisibilityChanged() OVERRIDE;

	void Init();
	void Reset();

	CEconItemView *GetEconItem( const char *szItemName, bool *pPreviewingSingleSticker, int iClass, int iSlot );

	bool LaunchPanelHelper_Inventory( CEconItemView *pEquippedItem );
	bool LaunchPanelHelper_Manifest( CEconItemView *pEquippedItem, const bool bPreviewingSingleSticker, const UIItemInfo_t &info );
	bool InitSceneModels( CEconItemView *pEquippedItem, const char *szMDLName );
	bool ApplyInitialConfiguration_Manifest( CEconItemView *pEquippedItem, const char *szEconItemOrMDLName, bool bInventoryLighting );
	bool ApplyInitialConfiguration_Inventory( CEconItemView *pEquippedItem );

	void MergeStickersToItem( CEconItemView *pEquippedItem, CUI_SceneItem *pScene );
	void ApplyStickersToItemWorldModel( CUI_SceneItem *pScene, CEconItemView *pEquippedItem, MDLHandle_t hMDL );

	void UpdateSceneAnimation();
	void UpdateGuidedItemRotation( float flTime );
	void UpdateItemZoomIn( float flTime );

	void EnsureItemsEquipped();

	bool OnSetCameraEntity( const char *pCameraName, float flDuration );
	bool OnGlobalSetCameraEntity( const char *pSceneName, const char *pCameraName, float flDuration );

	bool OnReadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );
	bool OnUnreadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );
	bool OnSetRotationSpeed( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, float flSpeed );

	bool SelectDebugLight();

	float UpdateItemRotation( int axis, float dt );
	void ResetItemRotation();

	void UpdateLights( float flTime );
	void ResetLightAnimation();

	void UpdateParticleSystems( float flTime );
	void StopParticleSystems();
	void CreateParticleSystem( const char *szParticleSystem, bool bRepeat );

	void InitCameraManipulateValues_Manifest();
	void InitCameraManipulateValues_Inventory();

	bool SetScene( const char *szManifest, const char *szItem, bool bInventory );
	bool SetPlayerModel( const char *szPlayer );
	bool EquipPlayer( const char *szItem, const char *szTeam, const char *szPos );
	bool EquipPlayerFromLoadout( const char *szTeam, const char *szPos );
	bool EquipPlayerWithItem( const char *szItem );

	bool ResetAnimation( bool bResetUsingManifest );
	bool LayerSequence( const char *szAnim, bool bLoop, bool bWaitForPreviousLayerSequenceToFinish );
	bool QueueSequence( const char *szAnim, bool bImmediate );

	bool SetPanelLightingAmount( float flAmount );
	bool SetFlashlightAmount( float flAmount );

	bool SetSceneRotation( float flRot0, float flRot1, float flRot2 );
	bool SetSceneAngles( float flRot0, float flRot1, float flRot2 );
	bool SetCameraPosition( float flX, float flY, float flZ );
	bool SetCameraAngles( float flX, float flY, float flZ );
	bool SetCameraPreset( int nPreset, bool bBlend );

	bool SetFloatingFloorAlpha( float flAlpha );

	bool ParticleTest( const char *szParticleSystemName, bool bRepeat );
	bool SetParticleSystemOffsetPosition( float flX, float flY, float flZ );
	bool SetParticleSystemOffsetAngles( float flX, float flY, float flZ );
	bool AddParticleSystem( const char *szParticleSystemName, const char *szAttachToBoneName, bool bRepeat );

	bool TogglePause();
	bool Pause( bool bPause );
	bool EnableRendering( bool bEnable );

	const char *GetItem() const { return m_info.m_itemName.Get(); }
	const char *GetManifest() const { return m_info.m_manifestName.Get(); }
	bool GetMouseRotate() const { return m_info.m_bRotate; }

	void SetItem( const char *szItem ) { m_info.m_itemName = szItem; }
	void SetManifest( const char *szManifest ) { m_info.m_manifestName = szManifest; }
	void SetMouseRotate( const bool bMouseRotate ) { m_info.m_bRotate = bMouseRotate; }

	void UpdatePresetCamera( float flTime );

public:
	static CompositeTextureSize_t sm_defaultCompositeTextureSize;

protected:

	KeyValues *m_kvConfiguration;
	CompositeTextureSize_t m_econItemTextureSize;

	CTextureReference m_shadowDummyColorBufferTexture;
	CTextureReference m_shadowDepthTexture;
	CTextureReference m_shadowFlashlightCookie;
	CRenderCaptureConfigurationState m_cfgRenderCapture;

private:
	CUI_ItemPreviewRenderer *m_pItemPreviewRenderer;

	CUI_ItemPreviewDebug *m_pDebug;

	UIItemInfo_t m_info;

	bool m_bItemPreviewInitialised;
	bool m_bIsPlayerPanel;
	bool m_bRequiresHighResModel;

	bool m_bMouseDragStart;

	enum eMouseDragContext
	{
		MOUSEDRAG_NONE,
		MOUSEDRAG_ITEM,
		MOUSEDRAG_CAMERA_ORIENT,
		MOUSEDRAG_CAMERA_DISTANCE,
		MOUSEDRAG_CAMERA_PIVOT,
		MOUSEDRAG_LIGHT_ORIENT,
		MOUSEDRAG_LIGHT_DISTANCE,
		MOUSEDRAG_LIGHT_PIVOT,
		MOUSEDRAG_LIGHT_COLOR_R,
		MOUSEDRAG_LIGHT_COLOR_G,
		MOUSEDRAG_LIGHT_COLOR_B,
		MOUSEDRAG_LIGHT_STRENGTH,
		MOUSEDRAG_LIGHT_PULSE_F,
		MOUSEDRAG_LIGHT_PULSE_A,
		MOUSEDRAG_LIGHT_FLICKER_R,
		MOUSEDRAG_LIGHT_FLICKER_A,
		MOUSEDRAG_LIGHT_ROT,
	};

	eMouseDragContext m_eMouseDragContext;

	bool m_bCtrlDown;

	bool m_bIsLiveView;
	float m_flLastMouseX;
	float m_flLastMouseY;

	float m_flLastTime;

	//	float m_flItemChangeTimeout;

	// item manipulation
	Vector		m_aItemRotate[ 2 ];
	Vector		m_itemRotateSpeed;
	Vector		m_itemRotateSpeedTarget;
	Vector		m_itemRotateAcceleration;
	Vector		m_itemRotateDelta;
	QAngle		m_itemInitAngles;
	int			m_nNumRotateAxes;
	RotateAxis	m_aRotateAxisOrder[ NUM_ROTATEAXIS ];
	Vector2D	m_aRotateAxisBounds[ NUM_ROTATEAXIS ];
	float		m_aRotateAxisSign[ NUM_ROTATEAXIS ];
	uint8		m_unRotationFlags;

	// fake mouse move/item intro rotate
	bool		m_bFMActive;
	float		m_flFMStartT;
	float		m_flFMT1;
	float		m_flFMT2;
	float		m_flFMLR1;
	float		m_flFMLR2;
	bool		m_bFMLoop;

	// camera manipulation
	float		m_flAzimuth;
	float		m_flAltitude;
	float		m_flDistance;
	float		m_flLookAtDeltaX;
	float		m_flLookAtDeltaY;
	float		m_flLookAtDeltaZ;
	Vector		m_vLookAtDelta;
	float		m_flCameraPresetStartBlendTime;

	// fake camera zoom
	bool		m_bFZActive;
	float		m_flFZStartT;
	float		m_flFZT1;
	float		m_flFZStartFOV;
	float		m_flFZTargetFOV;


	// light manipulation
	struct sLightInfo
	{
		QAngle	 m_rotateSpeed;
		Vector4D m_pulseFlicker;			// Vector x - pulse frequency, y - pulse amount, z - noise frequency, w - noise amount
		Vector	 m_color;
		Vector	 m_direction;
	};

	struct sFlashlightInfo
	{
		QAngle	 m_rotateSpeed;
		Vector4D m_pulseFlicker;			// Vector x - pulse frequency, y - pulse amount, z - noise frequency, w - noise amount
		Vector	 m_color;
		Vector	 m_position;
		QAngle   m_angle;
	};

	bool		m_bEnableAnimatingLights;
	float		m_flAnimateLightTotalPauseTime;
	float		m_flAnimateLightStartPauseTime;
	int			m_nDirectionalLightModify;

	sLightInfo m_aDirLightInfo[ MAX_PANEL_LIGHTS ];
	sFlashlightInfo m_flashlightInfo;
	Vector m_vAmbientLightColor;

	bool m_bAllowSuspendRepaint;
	bool m_bSuppressAutoReload;

	CUtlString m_sPanoramaSurfaceXML;
	int m_nPanoramaSurfaceWidth;
	int m_nPanoramaSurfaceHeight;

	static int s_nOffscreenPanelCount;
	CUtlString m_sOffscreenPanelName;

	static int s_nSuspendRepaintCount;

	int	m_nCurrentIdleAnimIndex;

	static int s_nCurrentSpawnAnimIndex;
	static int s_nCurrentIdleAnimIndex;

	CUtlVector< sEquipItem > m_aEquippedItems;

	CUtlString m_sSound;
};
