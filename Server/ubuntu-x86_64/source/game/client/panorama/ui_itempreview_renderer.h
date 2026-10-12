//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/source2/renderpanel.h"

#include "datacache/imdlcache.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "tier3/mdlutils.h"


//-----------------------------------------------------------------------------
// Forward decl
//-----------------------------------------------------------------------------

class CUI_ItemPreviewRenderer;
class CWorkshopWorkbenchDialog;

extern CWorkshopWorkbenchDialog *g_pWorkshopWorkbenchDialog;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

struct sMergedItemData
{
	CUtlString m_sEconItemName;

	CUtlString m_sMDLName;			// .mdl filename, only valid if econ item invalid

	MDLHandle_t m_hMDL;				// merged mdl handle
};

struct sCameraPreset
{
	Vector m_pos;			
	Vector m_pivot;		// orbit pivot pos
	QAngle m_ang;
	float  m_fov;
};

// separate class to support adding multiple characters for example and having a scene consist of multiple merged mdl's as opposed to one
// if desired, and no need to support that complexity, should revert to inheriting CMergedMDL in CUI_ItemPreviewRenderer
class CUI_SceneItem : public CMergedMDL
{
public:
	CUI_SceneItem();
	~CUI_SceneItem();

	void Init();
	
	void InitMDL( const char *szMDLName, const char *pEconItemName, CEconItemView *pEconItemView, IClientRenderable *pProxyData, CompositeTextureSize_t compTextureSize );
	int AddMergeMDL( const char *szMDLName, const char *pEconItemName, CEconItemView *pEconItemView = NULL, IClientRenderable *pProxyData = NULL, bool bRequestBonemergeTakeover = false, CompositeTextureSize_t compTextureSize = COMPOSITE_TEXTURE_SIZE_1024 );

	bool MergeEconItem( int &nMergeIndex, const char *szEconItemName, CEconItemView *pEconItem, char const *szReason = NULL );

	void SetMainMDL( int iMainMDLIndex );
	MDLHandle_t GetMainMDLHandle();
	CMDL *GetMainCMDL();
	const char *GetMainEconItemName();

	CMDL *GetMDLItemConfig();
	
	void SetMDLSkinIndex( int nNewSkinIndex );
	CMDL *GetRootMDL();
	bool AttemptToSyncWithItemFromOtherPanel( CUI_SceneItem *pSrc );

	CMDL *GetShadowFloorCMDL( const char *pMDLName );

	void UpdateTime( float flMdlTime, bool bPaused );
	float GetTime();
	void SetAnglesAndPosition( MDLHandle_t hMDL, const QAngle &ang, const Vector &pos );

	void SetParentRenderer( CUI_ItemPreviewRenderer *pRenderer );

	int FindSequenceFromActivity( CStudioHdr *pStudioHdr, const char *pszActivity );
	void SetAnim( const char *pszName, bool bUseSequencePlaybackFPS );
	void AddAnimFollowLoop( const char *pszName, bool bUseSequencePlaybackFPS );

	void ClearAnimFollowLoop();

	void SetRootMDLTransform( const matrix3x4_t &mat );
	const matrix3x4_t *GetRootMDLTransform();
	
	float *GetPoseParameters();
	int GetNumSequenceLayers();
	void RemoveAllSequenceLayers();
	void RemoveSequenceLayer( int nIndex );
	MDLSquenceLayer_t *GetSequenceLayer( int nIndex );
	MDLSquenceLayer_t *GetNextEmptySequenceLayer();
	float GetSequenceEndTime( CStudioHdr *pStudioHdr, int nIndex );

	CUtlVector< sMergedItemData > m_aMergedItemData; 

	QAngle m_Angle;

	matrix3x4_t m_ItemToWorld;	
	matrix3x4_t m_SceneAttach;

	// for spray material proxy refresh
	ITexture*	m_pSprayTex;
	int			m_nSprayRarity;
	int			m_nSprayTintID;
	float		m_flSprayCreationTime;

	virtual void OnPostSetUpBonesPreDraw() OVERRIDE;

protected:
	virtual void OnModelDrawPassStart( int iPass, CStudioHdr *pStudioHdr, int &nFlags ) OVERRIDE;
	virtual void OnModelDrawPassFinished( int iPass, CStudioHdr *pStudioHdr, int &nFlags ) OVERRIDE;

private:
	CUI_ItemPreviewRenderer *m_pParentRenderer;

	int m_iMainMDLIndex;

	// for recovering CEconItemView
	CUtlString m_sMainEconItemName;		// if no merged mdl's and main mdl is econitem

	MDLHandle_t m_hMainMDL;
};

// only supports one particle system/collection for now
class CUI_SceneParticleSystem
{
public:
	CUI_SceneParticleSystem();
	~CUI_SceneParticleSystem();

	void Init( const char *szParticleSystem, const char *szAttachment, MDLHandle_t hAttachMDL, bool bRepeat, const Vector &vecSystemOffset, const QAngle &angSystemOffset );
	void Update( float flTime );
	void Draw();
	void Stop();

	void ResetParticleCollection();		// Purge particle collection for re-use
	void SetControlPoint( const Vector &vecCP, const QAngle &angCP );

	CParticleCollection *m_pParticleCollection;
	CUtlString m_Name;

	float  m_flLastTime;

	int	   m_nAttachIndex;		// attachment index in scene, or -1 if m_vecOffset is world space
	Vector m_vecOffset;			// offset in bone space, or ws position if m_nAttachIndex == -1
	QAngle m_angOffset;			// orientation of bone or ws if m_nAttachIndex == -1

	bool   m_bRepeat;

private:
	Vector m_vecCP;				// world space control point (1 for now)
	QAngle m_angCP;				// CP orientation
};

// renderer
class CUI_ItemPreviewRenderer : public panorama::CRenderThreadCallback
{
public:
	CUI_ItemPreviewRenderer();

	virtual void RenderThreadCallback( Vector4D *pScissorRect, float x0, float y0, float x1, float y1, bool bEnableAA );

	void OnTick();

	void SetSetupRenderStateDelayed( bool bDeferred ) { m_bSetupRenderStateDelayed = bDeferred; }
	void SetupRenderStateDelayed() { if ( m_bSetupRenderStateDelayed ) SetupRenderState( m_nRenderWidth, m_nRenderHeight ); }

	void SetRenderWithAA( bool bRenderWithAA ) { m_bRenderWithAA = bRenderWithAA; }

	// Sets the camera to look at the the thing we're spinning around
	void LookAt( const Vector &vecCenter, float flDistance );
	void LookAt( float flRadius );

	// Light probe
	void SetLightProbe( CDmxElement *pLightProbe );

	// 'Create'/'Set' Lights
	void InitDefaultLights();
	void ClearDirectionalLights();
	void AddDirectionalLight( const Vector& color, const Vector& direction );
	void SetDirectionalLight( int idx, const Vector& color, const Vector& direction );
	void SetDirectionalLightDir( int idx, const Vector& direction );
	void SetDirectionalLightCol( int idx, const Vector& color );
	void SetLightAmbient( const Vector& ambient );
	void SetInitialFlashlightState();
	void SetAnimatedFromInitialLightingState();

	// 'Get Lights
	void GetDirectionalLight_CurrentCol( int idx, Vector &col );
	void GetDirectionalLight_CurrentDir( int idx, Vector &dir );
	void GetDirectionalLight_InitialCol( int idx, Vector &col );
	void GetDirectionalLight_InitialDir( int idx, Vector &dir );
	void GetLightAmbient( Vector &ambient );
	void GetInitialFlashlightState( Vector &pos, QAngle &orient, Vector &col );
	int GetLocalLightCount();

	// Animate lights
	void AnimateAmbientLightAmount( float flAmount );
	void AnimateDirectionalLightAmount( int idx, float flAmount );
	void AnimateAllDirectionalLightAmounts( float flAmount );
	void RotateDirectionalLight( int idx, const QAngle angles );
	void RotateFlashlight( const QAngle angles );

	// Camera.
	float GetCameraFOV( void );
	void SetCameraFOV( float flFOV );
	float GetCameraAspect( void );
	void SetCameraAspect( float flFOV );
	void SetCameraPositionAndAngles( const Vector &vecPos, const QAngle &angDir );
	void SetCameraPosition( const Vector &vecPos );
	void SetCameraAngles( const QAngle &angDir );
	void GetCameraPositionAndAngles( Vector &vecPos, QAngle &angDir );
	void SetCameraOffset( const Vector &vecOffset );
	void GetCameraOffset( Vector &vecOffset );
	void ResetCameraPivot( void );
	void ResetFlashlightPivot ( void );
	void GetCameraPivotPositionAndAngles( Vector &vecPos, QAngle &angDir );
	void GetCameraPivotPosition( Vector &vecPos );
	void SetCameraPivotPosition( Vector &vecPos );
	void GetFlashlightPivotPosition ( Vector &vecPos );
	void SetFlashlightPivotPosition ( Vector &vecPos );
	void ComputeCameraTransform( matrix3x4_t *pWorldToCamera );
	void UpdateCameraTransform();
	void ResetView();

	void SetRenderCaptureCameraFOV( float flFOV );
	void SetRenderCaptureCameraPositionAndAngles( const Vector &vecPos, const QAngle &angDir );
	void GetRenderCaptureCameraPositionAndAngles( Vector &vecPos, QAngle &angDir );

	void UpdateCameraPreset0();
	void UpdateCameraPreset( int idx, const Vector &vecPos, const Vector &vecPivot, const QAngle &angDir, const float &fov );
	sCameraPreset *SetCameraPreset( int nPreset, bool bBlend );
	sCameraPreset *NextCameraPreset();
	sCameraPreset *PrevCameraPreset();
	sCameraPreset *GetCameraPreset();
	void BlendCameraPreset( Vector &blendedPos, Vector &blendedPivot, QAngle &blendedAng, float &blendedFOV, float flAlpha );
	void ResetCameraPreset();

	void SetBackgroundColor( int r, int g, int b );
	void SetBackgroundColor( const Color& c );
	const Color& GetBackgroundColor() const;
	void SetGridColor( int r, int g, int b );

	void EnableRenderingWithFlashlight( void *pvConfiguration );
	void * GetRenderingWithFlashlightConfiguration() const { return m_pvRenderingWithFlashlightConfiguration; }

	void RenderCapture();

	int GetAttachmentIndex( CUI_SceneItem *pItem, const char *pszAttachment );
	void SetDirectionalLightAttachment( CUI_SceneItem *pItem, int idx, const char *pszAttachment );
	void SetCurrentFromInitialLightingState();
	void SetLookAtCamera( CUI_SceneItem *pItem );
	void SetItemAttachment( const char *pMDLName, const char *pRootMDLName, const char *pszAttachment );
	void SetCameraAttachment( CUI_SceneItem *pItem, const char *pszAttachment );
	void SetRenderCaptureCameraAttachment( CUI_SceneItem *pItem, const char *pszAttachment );

	int GetCameraAttachment() { return m_iCameraAttachment; }
	int GetRenderCaptureAttachment() { return m_iRenderCaptureCameraAttachment; }
	int GetDirectionalLightAttachment( int i ) { return m_iDirectionalLightAttachments[i]; }
	MaterialLightingState_t *GetInitialLightingState() { return &m_InitialLightingState; }
	MaterialLightingState_t *GetCurrentLightingState() { return &m_CurrentLightingState; }
	int *GetDirectionalLightAttachments() { return m_iDirectionalLightAttachments; }

	void SetCameraOrientOverride( QAngle angNew ) { m_vecCameraOrientOverride = angNew; }
	void SetCameraOrientOverrideEnabled( bool bEnabled ) { m_bCameraOrientOverrideEnabled = bEnabled; }
	bool IsCameraOrientOverrideEnabled( void ) { return m_bCameraOrientOverrideEnabled; }

	void SetCameraPositionOverride( Vector vecNew ) { m_vecCameraPositionOverride = vecNew; }
	void SetCameraPositionOverrideEnabled( bool bEnabled ) { m_bCameraPositionOverrideEnabled = bEnabled; }
	bool IsCameraPositionOverrideEnabled( void ) { return m_bCameraPositionOverrideEnabled; }

	void SetRenderCaptureCameraOrientOverride( QAngle angNew ) { m_vecRenderCaptureCameraOrientOverride = angNew; }
	void SetRenderCaptureCameraOrientOverrideEnabled( bool bEnabled ) { m_bRenderCaptureCameraOrientOverrideEnabled = bEnabled; }
	bool IsRenderCaptureCameraOrientOverrideEnabled( void ) { return m_bRenderCaptureCameraOrientOverrideEnabled; }

	void SetRenderCaptureCameraPositionOverride( Vector vecNew ) { m_vecRenderCaptureCameraPositionOverride = vecNew; }
	void SetRenderCaptureCameraPositionOverrideEnabled( bool bEnabled ) { m_bRenderCaptureCameraPositionOverrideEnabled = bEnabled; }
	bool IsRenderCaptureCameraPositionOverrideEnabled( void ) { return m_bRenderCaptureCameraPositionOverrideEnabled; }

 	void SetInventoryCameraOrient( QAngle angNew ) { m_vecInventoryCameraOrient = angNew; }
 	void SetInventoryCameraPosition( Vector vecNew ) { m_vecInventoryCameraPosition = vecNew; }

	Vector *GetCameraPositionOverride() { return &m_vecCameraPositionOverride; }
	QAngle *GetCameraOrientOverride() { return &m_vecCameraOrientOverride; }

	Vector *GetRenderCaptureCameraPositionOverride() { return &m_vecRenderCaptureCameraPositionOverride; }
	QAngle *GetRenderCaptureCameraOrientOverride() { return &m_vecRenderCaptureCameraOrientOverride; }

	Vector *GetInventoryCameraPosition() { return &m_vecInventoryCameraPosition; }
	QAngle *GetInventoryCameraOrient() { return &m_vecInventoryCameraOrient; }

	void SetCameraManipulateAllowed( bool bEnabled );
	void SetCameraIsManipulating( bool bEnabled );
	bool IsCameraManipulateEnabled() { return m_bCameraManipulateEnabled; }
	bool IsCameraManipulateAllowed() { return m_bCameraManipulateAllowed; }

	void SetRenderingInventoryItem( bool bInventoryItem ) { m_bInventoryItem = bInventoryItem; }
	bool IsRenderingInventoryItem() { return m_bInventoryItem; }

	void UpdateCameraManipulateTransform( float flAltitude, float flAzimuth, float flDistance, float flLookAtOffsetX, float flLookAtOffsetY, bool bForce );
	void UpdateCameraManipulateTransform( float flAltitude, float flAzimuth, float flDistance, Vector vLookAtOffset, bool bForce );

	bool IsPaint3dForRenderCapture() const { return m_bInRender3dForRenderCapture; }

	void ClearModelAnimFollowLoop();

	Camera_t & GetCameraSettings() { return m_Camera; }
	Camera_t & GetRenderCaptureCameraSettings() { return m_RenderCaptureCamera; }

	void GetSceneBounds( Vector &bbMin, Vector &bbMax );
	void GetSceneBounds( Vector &center, float &radius );

	// particles
	void InitParticleSystems();
	void UpdateParticleSystems( float flTime );
	void StopParticleSystems();
	void SetParticleSystemOffsetPosition( float flX, float flY, float flZ );
	void SetParticleSystemOffsetAngles( float flX, float flY, float flZ );
	void AddParticleSystem( const char *szParticlesystem, const char *szAttachToBone, bool bRepeat );
	void DrawParticleSystems();

	// animation update
	void ToggleAnimationPaused() { m_bAnimationPaused = !m_bAnimationPaused; m_bNeedsRedraw = true; }
	void SetAnimationPaused( bool bPause ) { m_bAnimationPaused = bPause; m_bNeedsRedraw = true; }
	bool IsAnimationPaused() { return m_bAnimationPaused; }

	void EnableRendering( bool bEnable ) { m_bEnableRendering = bEnable; m_bNeedsRedraw = true; }
	bool IsRenderingEnabled() { return ( m_bEnableRendering /*&& !g_pWorkshopWorkbenchDialog*/ ); }

	// specify if we can use the current version of the panel RT or if it needs re-drawing
	void SetNeedsRedraw( bool bNeedsRedraw ) { m_bNeedsRedraw = bNeedsRedraw; }
	bool NeedsRedraw() { return m_bNeedsRedraw; }

	// debug shadow floor
	void EnableDrawShadowFloor( bool bEnable ) { m_bDrawShadowFloor = bEnable; }
	void EnableDrawWorldPivot( bool bEnable ) { m_bDrawWorldPivot = bEnable; }
	void EnableDrawFlashlightPivot ( bool bEnable ) { m_bDrawFlashlightPivot = bEnable; }
	void EnableDrawPresetCameras( bool bEnable ) { m_bDrawPresetCameras = bEnable; }
	void EnableDrawDirectionaLight( int idx ) { m_nDrawDebugDirectionalLightIdx = idx; }

	void ToggleDebugHelperRendering() { m_bDrawDebugHelpers = !m_bDrawDebugHelpers; }

	// scene
	CUI_SceneItem m_SceneMergedMDL;

	// particles
	CUtlVector< CUI_SceneParticleSystem > m_aSceneParticleSystems;

protected:

	MaterialLightingState_t m_InitialLightingState;		// initial state
	MaterialLightingState_t m_CurrentLightingState;	    // render using this state

	Vector m_InitialFlashlightPositionOverride;
	QAngle m_InitialFlashlightOrientOverride;

	//matrix3x4_t m_LightToWorld[ MATERIAL_MAX_LIGHT_COUNT ];

	bool HasLightProbe() const;
	ITexture *GetLightProbeCubemap( bool bHDR );
	void DrawGrid();

	IMaterial *GetWireframeMaterial();

	CUtlVector< sCameraPreset > m_aCameraPresets;
	int m_nCurrCameraPreset;
	int m_nLastCameraPreset;

private:
	~CUI_ItemPreviewRenderer();

	void InitFullScreenBuffer( const char *pszRenderTargetName );
	void CreateNewTextureID( bool procedural = false );
	void DestroyTextureID( int id );

	sCameraPreset *GetLastCameraPreset();

	void Begin3DPaint( int iLeft, int iTop, int iRight, int iBottom, Vector4D *pScissorAttribute );
	void End3DPaint( bool bIgnoreAlphaWhenCompositing );
	void OnPaint3D();

	void UpdateStudioRenderConfig();

	void SetupRenderState( int nDisplayWidth, int nDisplayHeight );
	void DestroyLights();

	void DrawMainScene();
	void DrawShadowFloor();
	void DrawPivot();
	void DrawPresetCameras();
	void DrawDirectionalLight();

	void BeginAARendering();
	void EndAARendering();

	void InitDebugMDLs();

	CMDL m_DebugMDL_Flashlight;
	CMDL m_DebugMDL_DirLight;
	CMDL m_DebugMDL_Axis;
	CMDL m_DebugMDL_Pivot;
	CMDL m_DebugMDL_Floor;

	IMaterial *m_pShadowFloorMaterial;

	int m_iRotateMDLIndex;

	int	m_nRenderWidth, m_nRenderHeight;
	bool m_bInAARendering;
	Vector4D m_vecScissorAttribute;

	int m_n3DLeft, m_n3DRight, m_n3DTop, m_n3DBottom;
	float m_flClipLeft, m_flClipRight, m_flClipTop, m_flClipBottom;
	float m_flAspect;

	Color m_ClearColor;
	Color m_GridColor;

	Camera_t m_Camera;
	matrix3x4_t m_CameraPivot;
	Vector m_FlashlightPivotPos;

	Camera_t m_RenderCaptureCamera;

	Vector m_vecCameraOffset;

	CMaterialReference	m_Wireframe;
	CMaterialReference  m_LightProbeBackground;
	CMaterialReference  m_LightProbeHDRBackground;
	CMaterialReference  m_FullScreenBufferMaterial;
	CMaterialReference  m_FXAAMaterial;
	
	CTextureReference   m_LightProbeCubemap;
	CTextureReference   m_LightProbeHDRCubemap;
	CTextureReference   m_FullScreenBuffer;

	int		m_iItemAttachment;
	Quaternion m_baseItemAttachmentQuaternion;
	QAngle m_baseItemAttachmentQAngle;

	CUI_SceneItem *m_pCameraLookAtItem;

	int		m_iCameraAttachment;
	int		m_iRenderCaptureCameraAttachment;
	int		m_iDirectionalLightAttachments[MATERIAL_MAX_LIGHT_COUNT];
	float	m_flAutoPlayTimeBase;

	bool	m_bDrawDebugHelpers;
	bool	m_bDrawShadowFloor;
	bool	m_bDrawWorldPivot;
	bool	m_bDrawFlashlightPivot;
	bool	m_bDrawPresetCameras;
	int		m_nDrawDebugDirectionalLightIdx;

	bool	m_bCameraManipulateEnabled;
	bool	m_bCameraManipulateAllowed;
	Vector  m_cameraManipulateInitPos;
	QAngle  m_cameraManipulateInitAngle;

	bool	m_bCameraOrientOverrideEnabled;
	QAngle	m_vecCameraOrientOverride;

	bool	m_bCameraPositionOverrideEnabled;
	Vector	m_vecCameraPositionOverride;

	bool	m_bRenderCaptureCameraOrientOverrideEnabled;
	QAngle	m_vecRenderCaptureCameraOrientOverride;

	bool	m_bRenderCaptureCameraPositionOverrideEnabled;
	Vector	m_vecRenderCaptureCameraPositionOverride;

	Vector  m_vecInventoryCameraPosition;
	QAngle  m_vecInventoryCameraOrient;

	Vector  m_vecParticleSystemOffset;
	QAngle  m_angParticleSystemOffset;

	QAngle  m_CameraPivotAngles;

	bool	m_bEnableRendering;

	bool	m_bIn3DPaintMode;

	bool	m_bLockView;
	bool	m_bWireFrame;

	bool	m_bTimeStarted;
	bool	m_bAnimationPaused;
	bool	m_bNeedsRedraw;

	bool	m_bHasLightProbe;
	bool	m_bSetupRenderStateDelayed;
	bool	m_bInRender3dForRenderCapture;

	bool	m_bInventoryItem;	// to work around current rendering of inventory icon images and matching 3D view

	bool	m_bRenderWithAA;
	bool	m_bForceFXAA;

	void *	m_pvRenderingWithFlashlightConfiguration;
};
