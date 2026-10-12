//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"

#include "panorama/csgo_panorama.h"
#include "panorama/uischeduleddel.h"
#include "panorama/ui_popup_manager.h"
#include "panorama/ui_root.h"

#include "panorama/ui_itempreview_panel.h"

#include "cstrike15/cstrike15_item_inventory.h"
//#include "cstrike15/uicomponents/uicomponent_loadout.h"
//#include "econ/econ_ui.h"
#include "animation.h"

#include "gameui_interface.h"

#include "materialsystem/ivisualsdataprocessor.h"
#include "tier1/keyvalues.h"
//#include "workshoppreviewdialog.h"

#include "bone_setup.h"
#include "mathlib/noise.h"

#include "particle_parse.h"
#include "cs_app_lifetime_gamestats.h"
#include "panorama/source2/ipanoramaui.h"

#include "tier1/callqueue.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CUI_ItemPreviewPanel, ItemPreviewPanel );
REGISTER_PANEL2D_FACTORY( CUI_ItemPreviewColorSlider, ItemPreviewColorSlider );
REGISTER_PANEL2D_FACTORY( CUI_ItemPreviewSlider, ItemPreviewSlider );
REGISTER_PANEL2D( CUI_ItemPreviewDebug, ItemPreviewDebug );

using namespace panorama;

DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSceneReload );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelCloseDebugCamera );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelToggleDebugMode );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelCopyToClipboard );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelCopyPresetToClipboard );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelToggleEnabledLights );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetScene );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetPlayerModel );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelEquipPlayerFromLoadout );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelEquipPlayerWithItem );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelResetAnimation );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelQueueSequence );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelLayerSequence );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetPanelLightingAmount );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetFlashlightAmount );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetDirectionalLightModify );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetDirectionalLightPulseFlicker );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetDirectionalLightRotation );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetDirectionalLightAmount );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetDirectionalLightColor );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetDirectionalLightDirection );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetFlashlightPulseFlicker );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetFlashlightRotation );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetFlashlightColor );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetFlashlightPosition );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetFlashlightFOV );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetFlashlightNearFarZ );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetFlashlightAngle );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetAmbientLightColor );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetSceneRotation );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetSceneAngles );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetCameraPosition );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetCameraAngles );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetCameraPreset );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetSceneIntroRotation );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetSceneIntroFOV );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetEconItemTextureSize );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetFloatingFloorAlpha );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetParticleSystemOffsetPosition );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetParticleSystemOffsetAngles );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelAddParticleSystem );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelDebugLightSelection );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelDebugToggleSection_LightColor );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelDebugToggleSection_LightAnim );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelDebugToggleSection_LightFlashlightShadow );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelDebugResetLightAnim_RotX );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelDebugResetLightAnim_RotY );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelDebugResetLightAnim_RotZ );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelTogglePause );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelPause );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelEnableRendering );
DEFINE_PANORAMA_EVENT( UIItemPreviewPanelSetAsActivePreviewPanel );



int CUI_ItemPreviewPanel::s_nSuspendRepaintCount = 0;
int CUI_ItemPreviewPanel::s_nOffscreenPanelCount = 0;

CompositeTextureSize_t CUI_ItemPreviewPanel::sm_defaultCompositeTextureSize = COMPOSITE_TEXTURE_SIZE_1024;

static CUtlVector<KeyValues *> g_kvItemPreviewManifests;

extern CWorkshopWorkbenchDialog *g_pWorkshopWorkbenchDialog;

CUI_ItemPreviewPanel *g_pActivePreviewPanel = NULL;

#define SHADOW_FLOOR_MDL "models/weapons/pedestal_ground_shadow_plane.mdl"
#define DEFAULT_CAM_FOV 45.0f

#define MAX_SEQUENCE_LAYERS 8 // copy from CMergedMDL::MAX_SEQUENCE_LAYERS
// debug info
//#define _DEBUG_ANIMATION_INFO 

#if 0
#define ItemPreviewMsg DevMsg
#define ItemPreviewKVMsg KeyValuesDumpAsDevMsg
#else
#define ItemPreviewMsg( ... ) (void)(0)
#define ItemPreviewKVMsg( ... ) (void)(0)
#endif

//#define TESTING_ADD_EXTRA_WEAPON
//#define TESTING_CHARACTER_ANIMS
//#define TESTING_CHARACTER_ANIM_LAYERS

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static int UI_HelperGetSequenceIdFromStudioHdr( CStudioHdr &studioHdr, char const *szName )
{
	if ( V_isempty( szName ) )
		return ACT_INVALID;

	int iSequence = ACT_INVALID;

	for ( int iSeq = 0; iSeq < studioHdr.GetNumSeq(); ++iSeq )
	{
		mstudioseqdesc_t &seqDesc = studioHdr.pSeqdesc( iSeq );
		if ( !V_stricmp( seqDesc.pszActivityName(), szName ) )
		{
			iSequence = iSeq;
			break;
		}
	}

	if ( iSequence == ACT_INVALID )
		iSequence = LookupSequence( &studioHdr, szName );

	return iSequence;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CUI_ItemPreviewPanel::CUI_ItemPreviewPanel( CPanel2D *pParent, const char *pchID )
	: CRenderPanel( pParent, pchID, ePanelFlags_DontFireOnLoad )
{
	// We must uniqueify the world name
	static int s_nIndex = 1;
	char pAutoGeneratedID[128];
	if ( !pchID || !pchID[0] )
	{
		pchID = "unknown_itempreview_panel";
	}

	V_sprintf_safe( pAutoGeneratedID, "%s_%d", pchID, s_nIndex );
	++s_nIndex;
	pchID = pAutoGeneratedID;

	m_pItemPreviewRenderer = NULL;
	m_kvConfiguration = NULL;

	m_bItemPreviewInitialised = false;
	m_pDebug = NULL;

 	// create debug panel
 	m_pDebug = new CUI_ItemPreviewDebug( this, NULL );

 	m_pDebug->SetAcceptsInput( true );
 	m_pDebug->SetAcceptsFocus( true );
 	m_pDebug->SetVisible( false );

	RegisterForReadyEvents( true );
	
	RegisterEventHandler( UIItemPreviewPanelSetScene(), this, &CUI_ItemPreviewPanel::SetScene );
	RegisterEventHandler( UIItemPreviewPanelSetPlayerModel(), this, &CUI_ItemPreviewPanel::SetPlayerModel );
 	RegisterEventHandler( UIItemPreviewPanelEquipPlayerFromLoadout(), this, &CUI_ItemPreviewPanel::EquipPlayerFromLoadout );
	RegisterEventHandler( UIItemPreviewPanelEquipPlayerWithItem(), this, &CUI_ItemPreviewPanel::EquipPlayerWithItem );
	RegisterEventHandler( UIItemPreviewPanelResetAnimation(), this, &CUI_ItemPreviewPanel::ResetAnimation );
	RegisterEventHandler( UIItemPreviewPanelQueueSequence(), this, &CUI_ItemPreviewPanel::QueueSequence );
	RegisterEventHandler( UIItemPreviewPanelLayerSequence(), this, &CUI_ItemPreviewPanel::LayerSequence );

	RegisterEventHandler( UIItemPreviewPanelSetPanelLightingAmount(), this, &CUI_ItemPreviewPanel::SetPanelLightingAmount );
	RegisterEventHandler( UIItemPreviewPanelSetFlashlightAmount(), this, &CUI_ItemPreviewPanel::SetFlashlightAmount );
	RegisterEventHandler( UIItemPreviewPanelSetDirectionalLightModify(), this, &CUI_ItemPreviewPanel::SetDirectionalLightModify );
	RegisterEventHandler( UIItemPreviewPanelSetDirectionalLightPulseFlicker(), this, &CUI_ItemPreviewPanel::SetDirectionalLightPulseFlicker );
	RegisterEventHandler( UIItemPreviewPanelSetDirectionalLightRotation(), this, &CUI_ItemPreviewPanel::SetDirectionalLightRotation );
	RegisterEventHandler( UIItemPreviewPanelSetDirectionalLightAmount(), this, &CUI_ItemPreviewPanel::SetDirectionalLightAmount );
	RegisterEventHandler( UIItemPreviewPanelSetDirectionalLightColor(), this, &CUI_ItemPreviewPanel::SetDirectionalLightColor );
	RegisterEventHandler( UIItemPreviewPanelSetDirectionalLightDirection(), this, &CUI_ItemPreviewPanel::SetDirectionalLightDirection );
	RegisterEventHandler( UIItemPreviewPanelSetFlashlightPulseFlicker(), this, &CUI_ItemPreviewPanel::SetFlashlightPulseFlicker );
	RegisterEventHandler( UIItemPreviewPanelSetFlashlightRotation(), this, &CUI_ItemPreviewPanel::SetFlashlightRotation );
	RegisterEventHandler( UIItemPreviewPanelSetFlashlightColor(), this, &CUI_ItemPreviewPanel::SetFlashlightColor );
	RegisterEventHandler( UIItemPreviewPanelSetFlashlightPosition(), this, &CUI_ItemPreviewPanel::SetFlashlightPosition );
	RegisterEventHandler( UIItemPreviewPanelSetFlashlightAngle(), this, &CUI_ItemPreviewPanel::SetFlashlightAngle );
	RegisterEventHandler( UIItemPreviewPanelSetFlashlightFOV(), this, &CUI_ItemPreviewPanel::SetFlashlightFOV );
	RegisterEventHandler( UIItemPreviewPanelSetFlashlightNearFarZ(), this, &CUI_ItemPreviewPanel::SetFlashlightNearFarZ );
	RegisterEventHandler( UIItemPreviewPanelSetAmbientLightColor(), this, &CUI_ItemPreviewPanel::SetAmbientLightColor );
	RegisterEventHandler( UIItemPreviewPanelSetSceneRotation(), this, &CUI_ItemPreviewPanel::SetSceneRotation );
	RegisterEventHandler( UIItemPreviewPanelSetSceneAngles(), this, &CUI_ItemPreviewPanel::SetSceneAngles );
	RegisterEventHandler( UIItemPreviewPanelSetCameraPosition(), this, &CUI_ItemPreviewPanel::SetCameraPosition );
	RegisterEventHandler( UIItemPreviewPanelSetCameraAngles(), this, &CUI_ItemPreviewPanel::SetCameraAngles );
	RegisterEventHandler( UIItemPreviewPanelSetCameraPreset(), this, &CUI_ItemPreviewPanel::SetCameraPreset );
	RegisterEventHandler( UIItemPreviewPanelSetSceneIntroRotation(), this, &CUI_ItemPreviewPanel::SetSceneIntroRotation );
	RegisterEventHandler( UIItemPreviewPanelSetSceneIntroFOV(), this, &CUI_ItemPreviewPanel::SetSceneIntroFOV );
	RegisterEventHandler( UIItemPreviewPanelSetEconItemTextureSize(), this, &CUI_ItemPreviewPanel::SetEconItemTextureSize );
	RegisterEventHandler( UIItemPreviewPanelSetFloatingFloorAlpha(), this, &CUI_ItemPreviewPanel::SetFloatingFloorAlpha );

	RegisterEventHandler( UIItemPreviewPanelSetParticleSystemOffsetPosition(), this, &CUI_ItemPreviewPanel::SetParticleSystemOffsetPosition );
	RegisterEventHandler( UIItemPreviewPanelSetParticleSystemOffsetAngles(), this, &CUI_ItemPreviewPanel::SetParticleSystemOffsetAngles );
	RegisterEventHandler( UIItemPreviewPanelAddParticleSystem(), this, &CUI_ItemPreviewPanel::AddParticleSystem );

	RegisterEventHandler( UIItemPreviewPanelTogglePause(), this, &CUI_ItemPreviewPanel::TogglePause );
	RegisterEventHandler( UIItemPreviewPanelPause(), this, &CUI_ItemPreviewPanel::Pause );
	RegisterEventHandler( UIItemPreviewPanelEnableRendering(), this, &CUI_ItemPreviewPanel::EnableRendering );

	RegisterEventHandler( UIItemPreviewPanelSetAsActivePreviewPanel(), this, &CUI_ItemPreviewPanel::SetAsActivePreviewPanel );

	if ( !UIEngine()->BHaveEventHandlersRegisteredForType( CUI_ItemPreviewPanel::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( ReadyForDisplay(), &CUI_ItemPreviewPanel::OnReadyForDisplay );
		RegisterEventHandlerOnPanelType( UnreadyForDisplay(), &CUI_ItemPreviewPanel::OnUnreadyForDisplay );
	}

	SetAcceptsFocus( true );
	SetAcceptsInput( true );

 	if ( g_pPanoramaUIEngine )
 	{
 		g_pPanoramaUIEngine->AddDeviceDependentObject( this );
 	}
}

int CUI_ItemPreviewPanel::s_nCurrentSpawnAnimIndex = 0;
int CUI_ItemPreviewPanel::s_nCurrentIdleAnimIndex = 0;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CUI_ItemPreviewPanel::~CUI_ItemPreviewPanel()
{
 	if ( g_pPanoramaUIEngine )
 	{
 		g_pPanoramaUIEngine->RemoveDeviceDependentObject( this );
 	}

	if ( this == g_pActivePreviewPanel )
		g_pActivePreviewPanel = NULL;

	CloseDebugCamera();

	Reset();
}

void CUI_ItemPreviewPanel::Init()
{
	if ( m_bItemPreviewInitialised )
	{
		m_bItemPreviewInitialised = false;
		Reset();
	}

	m_pItemPreviewRenderer = new CUI_ItemPreviewRenderer();
	SetRenderThreadCallback( m_pItemPreviewRenderer, k_ERenderCallbackFlagsRenderTargetMode );

	if ( m_pDebug )
	{
		m_pDebug->SetItemPreviewRenderer( m_pItemPreviewRenderer );
		m_pDebug->SetItemPreviewPanel( this );

		if ( m_pDebug->BIsVisible() )
		{
			m_pItemPreviewRenderer->SetCameraManipulateAllowed( true );
		}
	}

	SetRotationFlags( SCENE_PANEL_ROTATION_NONE );
	SetAcceptsInput( true );
	ResetItemRotation();
	ResetLightAnimation();

	m_kvConfiguration = NULL;

	m_bIsPlayerPanel = false;
	m_bRequiresHighResModel = true;

	m_eMouseDragContext = MOUSEDRAG_NONE;

	m_bMouseDragStart = false;
	m_bCtrlDown = false;
	m_bIsLiveView = true;
	m_bAllowSuspendRepaint = true;
	m_bSuppressAutoReload = false;

	m_flLastTime = 0.0f;

	m_flLastMouseX = 0.0f;
	m_flLastMouseY = 0.0f;

	m_nPanoramaSurfaceWidth = 0;
	m_nPanoramaSurfaceHeight = 0;

	m_econItemTextureSize = sm_defaultCompositeTextureSize;

	m_nCurrentIdleAnimIndex = -1;

	m_flCameraPresetStartBlendTime = 0.0f;

	m_bFMActive = false;
	m_bFZActive = false;
}

void CUI_ItemPreviewPanel::Reset()
{
	ResetAnimation( false );

	if ( m_pItemPreviewRenderer )
	{
		CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;

		if ( pScene->m_pSprayTex )
		{
			// release scene spray tex, (matching AddRef via QcGetDecalDataForModelPreviewPanel when spray scene was initialized)
			pScene->m_pSprayTex->DecrementReferenceCount();
		}
		m_pItemPreviewRenderer->Release();
	}
	m_pItemPreviewRenderer = NULL;

	if ( m_kvConfiguration )
		m_kvConfiguration->deleteThis();
	m_kvConfiguration = NULL;

	if ( m_shadowDummyColorBufferTexture.IsValid() )
		m_shadowDummyColorBufferTexture.Shutdown();

	if ( m_shadowDepthTexture.IsValid() )
		m_shadowDepthTexture.Shutdown();

	if ( m_shadowFlashlightCookie.IsValid() )
		m_shadowFlashlightCookie.Shutdown();

	m_aEquippedItems.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Setup JS object template
//-----------------------------------------------------------------------------
void CUI_ItemPreviewPanel::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSAccessor( "manifest", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::GetManifest ), PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::SetManifest ) );
	RegisterJSAccessor( "item", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::GetItem ), PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::SetItem ) );

	//RegisterJSMethod( "SetScene", PANORAMA_DELEGATE_RESOLVE( &CUI_ItemPreviewPanel::SetScene, const char* ) );
	RegisterJSMethod( "SetScene", panorama::MethodInfo<bool, CUI_ItemPreviewPanel, const char*, const char*, bool>::Ptr<&CUI_ItemPreviewPanel::SetScene>() );
 	RegisterJSMethod( "SetPlayerModel", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::SetPlayerModel ) );
 	RegisterJSMethod( "EquipPlayerFromLoadout", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::EquipPlayerFromLoadout ) );
	RegisterJSMethod( "EquipPlayerWithItem", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::EquipPlayerWithItem ) );
 
	RegisterJSMethod( "ResetAnimation", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::ResetAnimation ) );
	RegisterJSMethod( "QueueSequence", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::QueueSequence ) );
	RegisterJSMethod( "LayerSequence", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::LayerSequence ) );

	RegisterJSMethod( "SetPanelLightingAmount", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::SetPanelLightingAmount ) );
	RegisterJSMethod( "SetFlashlightAmount", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::SetFlashlightAmount ) );
	RegisterJSMethod( "SetFlashlightPulseFlicker", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::SetFlashlightPulseFlicker ) );
	RegisterJSMethod( "SetFlashlightRotation", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::SetFlashlightRotation ) );
	RegisterJSMethod( "SetFlashlightColor", PANORAMA_DELEGATE(&CUI_ItemPreviewPanel::SetFlashlightColor ) );
	RegisterJSMethod( "SetFlashlightPosition", PANORAMA_DELEGATE(&CUI_ItemPreviewPanel::SetFlashlightPosition ) );
	RegisterJSMethod( "SetFlashlightAngle", PANORAMA_DELEGATE(&CUI_ItemPreviewPanel::SetFlashlightAngle ) );
	RegisterJSMethod( "SetFlashlightFOV", PANORAMA_DELEGATE ( &CUI_ItemPreviewPanel::SetFlashlightFOV ) );
	RegisterJSMethod( "SetFlashlightNearFarZ", PANORAMA_DELEGATE ( &CUI_ItemPreviewPanel::SetFlashlightNearFarZ ) );
	RegisterJSMethod( "SetAmbientLightColor", PANORAMA_DELEGATE(&CUI_ItemPreviewPanel::SetAmbientLightColor ) );
	RegisterJSMethod( "SetDirectionalLightModify", PANORAMA_DELEGATE(&CUI_ItemPreviewPanel::SetDirectionalLightModify ) );
	RegisterJSMethod( "SetDirectionalLightPulseFlicker", PANORAMA_DELEGATE(&CUI_ItemPreviewPanel::SetDirectionalLightPulseFlicker ) );
	RegisterJSMethod( "SetDirectionalLightRotation", PANORAMA_DELEGATE(&CUI_ItemPreviewPanel::SetDirectionalLightRotation ) );
	RegisterJSMethod( "SetDirectionalLightAmount", PANORAMA_DELEGATE(&CUI_ItemPreviewPanel::SetDirectionalLightAmount ) );
	RegisterJSMethod( "SetDirectionalLightColor", PANORAMA_DELEGATE(&CUI_ItemPreviewPanel::SetDirectionalLightColor ) );
	RegisterJSMethod( "SetDirectionalLightDirection", PANORAMA_DELEGATE(&CUI_ItemPreviewPanel::SetDirectionalLightDirection ) );
	RegisterJSMethod( "SetSceneRotation", PANORAMA_DELEGATE(&CUI_ItemPreviewPanel::SetSceneRotation));
	RegisterJSMethod( "SetSceneAngles", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::SetSceneAngles ) );
	RegisterJSMethod( "SetCameraPosition", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::SetCameraPosition ) );
	RegisterJSMethod( "SetCameraAngles", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::SetCameraAngles ) );
	RegisterJSMethod( "SetCameraPreset", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::SetCameraPreset ) );
	RegisterJSMethod( "SetSceneIntroRotation", PANORAMA_DELEGATE ( &CUI_ItemPreviewPanel::SetSceneIntroRotation ) );
	RegisterJSMethod( "SetSceneIntroFOV", PANORAMA_DELEGATE ( &CUI_ItemPreviewPanel::SetSceneIntroFOV ) );
	RegisterJSMethod( "SetEconItemTextureSize", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::SetEconItemTextureSize ) );
	RegisterJSMethod( "SetFloatingFloorAlpha", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::SetFloatingFloorAlpha ) );
	
	RegisterJSMethod( "SetParticleSystemOffsetPosition", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::SetParticleSystemOffsetPosition ) );
	RegisterJSMethod( "SetParticleSystemOffsetAngles", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::SetParticleSystemOffsetAngles ) );
	RegisterJSMethod( "AddParticleSystem", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::AddParticleSystem ) );

	RegisterJSMethod( "TogglePause", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::TogglePause ) );
	RegisterJSMethod( "Pause", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::Pause ) );
	RegisterJSMethod( "EnableRendering", PANORAMA_DELEGATE( &CUI_ItemPreviewPanel::EnableRendering ) );

	RegisterJSMethod( "SetAsActivePreviewPanel", PANORAMA_DELEGATE ( &CUI_ItemPreviewPanel::SetAsActivePreviewPanel ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewPanel::BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties )
{
	static CPanoramaSymbol k_symManifest( "manifest" );
	static CPanoramaSymbol k_symItem( "item" );
	static CPanoramaSymbol k_symInventory( "inventory" );
	static CPanoramaSymbol k_symAllowRotation( "mouse_rotate" );
	static CPanoramaSymbol k_symEnableRendering( "enable_rendering" );
	static CPanoramaSymbol k_symAntialias( "antialias" );
	static CPanoramaSymbol k_symEnableFloorShadow( "enable_floorshadow" );
	static CPanoramaSymbol k_symDefaultCustomMaterialSize( "custom_material_size" );
	static CPanoramaSymbol k_symPanoramaSurfaceXML( "panoramasurfacexml" );
	static CPanoramaSymbol k_symPanoramaSurfaceWidth( "panoramasurfacewidth" );
	static CPanoramaSymbol k_symPanoramaSurfaceHeight( "panoramasurfaceheight" );
	static CPanoramaSymbol k_symSound( "sound" );

	UIItemInfo_t info;

	info.m_bInventory = false;
	info.m_bRotate = false;
	info.m_bAntiAlias = false;
 	info.m_bEnableRendering = true;
	info.m_bEnableFloorShadow = false;

	bool bSet = false;
	bool bSuccess = true;

	FOR_EACH_VEC( vecProperties, i )
	{
		const ParsedPanelProperty_t &prop = vecProperties[i];

		if ( prop.m_symName == k_symManifest )
		{
			info.m_manifestName = prop.m_pchValue;

			bSet = true;
		}
		if ( prop.m_symName == k_symItem )
		{
			info.m_itemName = prop.m_pchValue;

			// ignore playerName for now and just use local player inventory
			//info.m_pPlayerInventory = CSInventoryManager()->GetLocalCSInventory();

			bSet = true;
		}
		else if ( prop.m_symName == k_symInventory )
		{
			if ( CSSHelpers::BParseTrueFalse( prop.m_pchValue, &info.m_bInventory ) )
			{
				bSet = true;
			}
		}
		else if ( prop.m_symName == k_symAllowRotation )
		{
 			if ( CSSHelpers::BParseTrueFalse( prop.m_pchValue, &info.m_bRotate ) )
 			{
 				bSet = true;
 			}
		}
		else if ( prop.m_symName == k_symAntialias )
		{
			if ( CSSHelpers::BParseTrueFalse( prop.m_pchValue, &info.m_bAntiAlias ) )
			{
				bSet = true;
			}
		}
		else if ( prop.m_symName == k_symEnableRendering )
		{
			if ( CSSHelpers::BParseTrueFalse( prop.m_pchValue, &info.m_bEnableRendering ) )
			{
				bSet = true;
			}
		}
		else if ( prop.m_symName == k_symEnableFloorShadow )
		{
			if ( CSSHelpers::BParseTrueFalse ( prop.m_pchValue, &info.m_bEnableFloorShadow ) )
			{
				bSet = true;
			}
		}
		else if ( prop.m_symName == k_symDefaultCustomMaterialSize )
		{
			float flSize;
			if ( CSSHelpers::BParseNumber( &flSize, prop.m_pchValue, NULL ) )
			{
				SetEconItemTextureSize( (int)flSize );
				bSet = true;
			}
		}
		else if ( prop.m_symName == k_symPanoramaSurfaceXML )
		{
			m_sPanoramaSurfaceXML = prop.m_pchValue;
		}
		else if ( prop.m_symName == k_symPanoramaSurfaceWidth )
		{
			m_nPanoramaSurfaceWidth = V_atoi( prop.m_pchValue );
		}
		else if ( prop.m_symName == k_symPanoramaSurfaceHeight )
		{
			m_nPanoramaSurfaceHeight = V_atoi( prop.m_pchValue );
		}
		else if( prop.m_symName == k_symSound )
		{
			m_sSound = prop.m_pchValue;
		}
		else
		{
			if ( !BSetProperty( prop.m_symName, prop.m_pchValue ) )
			{
				bSuccess = false;
			}
		}
	}

	if ( bSuccess && bSet )
	{
		// Since this is happening in BSetProperties, it can cause problems if the scene panel is in another panel
		// because it can produce this error
		// Should never apply styles while loading layout... data isn't yet fully parsed for initial apply and we may transition things wrong.
		// Therefore we queue it to happen immediately after this phase
		SetScene( info );
	}
	return bSuccess;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
ConVar panorama_3dpanel_camera_preset_blend_time( "panorama_3dpanel_camera_preset_blend_time", "1.0", FCVAR_DEVELOPMENTONLY, "time to blend between camera presets" );
ConVar panorama_3dpanel_anim_fadeinout_time_scale( "panorama_3dpanel_anim_fadeinout_time_scale", "2.0", FCVAR_DEVELOPMENTONLY, "temp scale factor for animation fade in/out time" );
ConVar panorama_3dpanel_anims_bookend( "panorama_3dpanel_anims_bookend", "1", FCVAR_DEVELOPMENTONLY, "default true. If all anims designed to follow on from each other" ); // TODO - this should be a per-panel attribute really

void CUI_ItemPreviewPanel::UpdateSceneAnimation()
{
	if ( !m_pItemPreviewRenderer )
		return;

	if ( m_info.m_bInventory )
		return;

	// scene
	CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;

	if ( !pScene )
	{
		DevWarning( "CUI_ItemPreviewPanel::UpdateSceneAnimation - no scene item!!\n" );
		return;
	}

	const char *pMainEconItemName = pScene->GetMainEconItemName();

	// player anim updates
	if ( !pMainEconItemName || V_isempty( pMainEconItemName ) )
	{
		MDLCACHE_CRITICAL_SECTION();

		CMDL *pMdlItemConfig = pScene->GetRootMDL();

		if ( pMdlItemConfig->m_MDLHandle == MDLHANDLE_INVALID )
			return;

		CStudioHdr studioHdr( pMdlItemConfig->GetStudioHdr(), g_pMDLCache );

		int nNumSeq = studioHdr.GetNumSeq();

		MDLSquenceLayer_t *pPrevLoopSeq = NULL;

		// update layer sequences
		for ( int nIndex = 0; nIndex < pScene->GetNumSequenceLayers(); nIndex++ )
		{
			MDLSquenceLayer_t *pLayer = pScene->GetSequenceLayer( nIndex );
			int nSeqIndex = pLayer->m_nSequenceIndex;

			if ( ( nSeqIndex < 0 ) || ( nSeqIndex >= nNumSeq ) )
				continue;

			int nFrameCount = MAX( 1, Studio_MaxFrame( &studioHdr, nSeqIndex, pScene->GetPoseParameters() ) );
			float flAdjustedTime = pScene->GetRootMDL()->m_flTime - pLayer->m_flStartTime;
			float flPlaybackRate = Studio_FPS( &studioHdr, nSeqIndex, pScene->GetPoseParameters() );

			if ( flAdjustedTime >= 0.0f )
			{
				// fading in?
				mstudioseqdesc_t &seqdesc = studioHdr.pSeqdesc( nSeqIndex );
				float flFadeInTime = seqdesc.fadeintime * panorama_3dpanel_anim_fadeinout_time_scale.GetFloat();

				if ( ( flFadeInTime > flAdjustedTime ) && ( pLayer->m_flWeight < 1.0f ) )
				{
					// fade in
					float s = flAdjustedTime / flFadeInTime;
					pLayer->m_flWeight = 3.0f * s * s - 2.0f * s * s * s;
#if defined _DEBUG_ANIMATION_INFO
					DevMsg("Layer %d fade in: Seq %d, wt %1.1f\n", nIndex, nSeqIndex, pLayer->m_flWeight );
#endif
				}
				else if ( !pLayer->m_bLoop ) // don't perform fadeout for looping layer
				{
					pLayer->m_flWeight = 1.0f;
					float flFadeOutTime = seqdesc.fadeouttime * panorama_3dpanel_anim_fadeinout_time_scale.GetFloat();

					if ( flFadeOutTime > 0.0f )
					{
						float flEndTime = flPlaybackRate > 0.0f ? float( nFrameCount ) / flPlaybackRate : float( nFrameCount );
						float flFadeoutStartTime = flEndTime - flFadeOutTime;

						// fade out
						if ( flAdjustedTime > flFadeoutStartTime )
						{
							if ( flAdjustedTime < flEndTime )
							{
								float s = 1.0f - ( flAdjustedTime - flFadeoutStartTime ) / flFadeOutTime;
								pLayer->m_flWeight *= 3.0f * s * s - 2.0f * s * s * s;
#if defined _DEBUG_ANIMATION_INFO
								DevMsg( "Layer %d fade out: Seq %d, wt %1.1f\n", nIndex, nSeqIndex, pLayer->m_flWeight );
#endif
							}
							else
							{
								pLayer->m_flWeight = 0.0f;
							}
						}
						else
						{
							if ( pPrevLoopSeq && panorama_3dpanel_anims_bookend.GetBool() )
							{
								// keep restarting previous looping sequence until the non looping seq starts to fade out
								// since anims designed to follow on from each other, we might blend back in the middle
								// of the looping sequence causing an unwanted (large) spacial blend 
								pPrevLoopSeq->m_flStartTime = pScene->GetRootMDL()->m_flTime;

								// only do this for most recent previous
								pPrevLoopSeq = NULL;
							}
						}
					}
				}
				else
				{
					pPrevLoopSeq = pLayer;
					pLayer->m_flWeight = 1.0f;
#if defined _DEBUG_ANIMATION_INFO
					DevMsg( "Layer %d, Seq %d, wt %1.1f\n", nIndex, nSeqIndex, pLayer->m_flWeight );
#endif
				}

				// remove layer if not looping and finished
				if ( !pLayer->m_bLoop )
				{
					float flCycle = ( flAdjustedTime * flPlaybackRate ) / nFrameCount;

					if ( flCycle > 1.0f )
					{
						// remove this layer
						pScene->RemoveSequenceLayer( nIndex );

						// compensate for item being removed!
						nIndex--;

#if defined _DEBUG_ANIMATION_INFO
						DevMsg( "RemoveLayer %d, seq %d, totSeq %d\n", nIndex, nSeqIndex, pScene->GetNumSequenceLayers() );
#endif
					}
				}
			}
			else
			{
				// not started
				pLayer->m_flWeight = 0.0f;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
ConVar panorama_3dpanel_guided_intro_delay( "panorama_3dpanel_guided_intro_delay", "0.1", FCVAR_DEVELOPMENTONLY, "time to blend between camera presets" );

void CUI_ItemPreviewPanel::UpdateGuidedItemRotation( float flTime )
{
	if ( !m_bFMActive )
		return;

	if ( !m_pItemPreviewRenderer )
		return;

	// scene
	CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;

	if ( !pScene )
		return;

	if ( m_flFMStartT < 0.0f )
	{
		if ( flTime - m_flLastTime )
		{
			m_flFMStartT = flTime + panorama_3dpanel_guided_intro_delay.GetFloat();
		}
		return;
	}

	float flLifeT = flTime - m_flFMStartT;

	if ( m_bFMLoop && ( flLifeT > ( m_flFMT1 + m_flFMT2 ) ) )
	{
		m_flFMStartT = flTime;
		flLifeT = 0.0f;
	}

	if ( flLifeT < 0.0f )
		return;

	float flDeltaT = flTime - m_flLastTime;

	if ( flLifeT < m_flFMT1 )
	{
		float flLR1 = m_flFMLR1 * flDeltaT;

		m_itemRotateSpeed = vec3_origin;
		m_aItemRotate[ RotateAxis_X ][ 1 ] += m_aRotateAxisSign[ RotateAxis_X ] * flLR1;
	}
	else if ( flLifeT < ( m_flFMT1 + m_flFMT2 ) )
	{
		float flLR2 = m_flFMLR2 * flDeltaT;

		m_itemRotateSpeed = vec3_origin;
		m_aItemRotate[ RotateAxis_X ][ 1 ] += m_aRotateAxisSign[ RotateAxis_X ] * flLR2;
	}
	else if ( flLifeT < ( 2.0f * ( m_flFMT1 + m_flFMT2 ) ) )
	{
		float flLR1 = m_flFMLR1 * flDeltaT;

		m_itemRotateSpeed = vec3_origin;

		// return to 0 rotation
		if ( m_aItemRotate[ RotateAxis_X ][ 1 ] > 0.0f )
		{
			m_aItemRotate[ RotateAxis_X ][ 1 ] += flLR1;

			if ( m_aItemRotate[ RotateAxis_X ][ 1 ] < 0.0f )
			{
				m_aItemRotate[ RotateAxis_X ][ 1 ] = 0.0f;
				m_bFMActive = false;
			}
		}
		else
		{
			m_aItemRotate[ RotateAxis_X ][ 1 ] -= flLR1;

			if ( m_aItemRotate[ RotateAxis_X ][ 1 ] > 0.0f )
			{
				m_aItemRotate[ RotateAxis_X ][ 1 ] = 0.0f;
				m_bFMActive = false;
			}
		}
	}
	else
	{
		// stop
		m_bFMActive = false;
	}
}


void CUI_ItemPreviewPanel::UpdateItemZoomIn( float flTime )
{
	if ( !m_bFZActive )
		return;

	if ( !m_pItemPreviewRenderer )
		return;

	if ( m_flFZStartT < 0.0f )
	{
		if ( flTime - m_flLastTime )
		{
			m_flFZStartT = flTime + panorama_3dpanel_guided_intro_delay.GetFloat();
		}
		return;
	}
	
	float flLifeT = flTime - m_flFZStartT;

	if ( flLifeT < 0.0f )
		return;

	//float flDeltaT = flTime - m_flLastTime;

	if ( flLifeT > m_flFZT1 )
	{
		m_bFZActive = false;
		m_pItemPreviewRenderer->SetCameraFOV( m_flFZTargetFOV );
		return;
	}
	
	float dt = clamp( flLifeT / m_flFZT1, 0.0f, 1.0f );
	float dtSq = dt * dt;
	float s = ( 3.0f * dtSq ) - ( 2.0f * dtSq * dt );

	if ( s >= 1.0f )
	{
		m_bFZActive = false;
		m_pItemPreviewRenderer->SetCameraFOV( m_flFZTargetFOV );
		return;
	}

	float flFOV = ( 1.0f - s ) * m_flFZStartFOV + ( s * m_flFZTargetFOV );

	m_pItemPreviewRenderer->SetCameraFOV( flFOV );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CUI_ItemPreviewPanel::EnsureItemsEquipped()
{
	if ( m_aEquippedItems.Count() == 0 )
		return;

	for ( int i = 0; i < m_aEquippedItems.Count(); i++ )
	{
		sEquipItem *pItem = &m_aEquippedItems[ i ];

		if ( pItem->m_bEquipped == false )
		{
			CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;

			CEconItemView *pExtraEconItem = GetEconItem( pItem->m_sName, NULL, pItem->m_nTeam, pItem->m_nPos );

			int nMergeIndex;
			pItem->m_bEquipped = pScene->MergeEconItem( nMergeIndex, pItem->m_sName, pExtraEconItem );
		}
	}
}

//-----------------------------------------------------------------------------
// Tell the world renderer it will need to set up its world since it's being painted
//-----------------------------------------------------------------------------
void CUI_ItemPreviewPanel::Paint()
{
	VPROF_BUDGET_THREAD( "CUI_ItemPreviewPanel::Paint", VPROF_BUDGETGROUP_TENFOOT );

	if ( !m_bItemPreviewInitialised )
		return;

	CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;

	if ( !pScene )
		return;

	float flTime = pScene->GetRootMDL()->m_flTime;

	int nW, nH;
	nW = (int)UIPanel()->GetActualRenderWidth();
	nH = (int)UIPanel()->GetActualRenderHeight();
	
	if ( !m_pPanelRT || ( nW != m_pPanelRT->GetTextureWidth() ) || ( nH != m_pPanelRT->GetTextureHeight() ) )
	{
		m_pPanelRT.SafeRelease();
		AccessRenderDevice()->BCreateRenderTargetTexture( &m_pPanelRT, nW, nH );

		if ( m_pItemPreviewRenderer )
			m_pItemPreviewRenderer->SetNeedsRedraw( true );
	}

	BaseClass::Paint();

	if ( !m_pItemPreviewRenderer )
		return;

	if ( !m_pItemPreviewRenderer->NeedsRedraw() )
 		return;

	// update equipped items
	EnsureItemsEquipped();

	// update scene
	m_pItemPreviewRenderer->OnTick();

	// update scene animation
	UpdateSceneAnimation();

	// update fake mouse input (guided item rotation)
	UpdateGuidedItemRotation( flTime );

	// update model/scene orientation
	QAngle angleItem = vec3_angle;
	float dt = flTime - m_flLastTime;
	for ( int i = 0; i < m_nNumRotateAxes; i++ )
	{
		angleItem[ m_aRotateAxisOrder[ i ] ] = UpdateItemRotation( i, dt );
	}
	angleItem += m_itemInitAngles;
	pScene->SetAnglesAndPosition( pScene->GetMainMDLHandle(), angleItem, vec3_origin );

	// update scene lights
	UpdateLights( flTime );

	// update particles
	UpdateParticleSystems( flTime );

	// update camera
	m_pItemPreviewRenderer->SetCameraPositionAndAngles( *m_pItemPreviewRenderer->GetCameraPositionOverride(), *m_pItemPreviewRenderer->GetCameraOrientOverride() );
	m_pItemPreviewRenderer->UpdateCameraManipulateTransform( m_flAltitude, m_flAzimuth, m_flDistance, m_vLookAtDelta, m_info.m_bInventory );

	UpdatePresetCamera( flTime );
	UpdateItemZoomIn( flTime );

	// refresh spray material proxy
	if ( pScene->m_pSprayTex )
	{
		extern void QcRefreshDecalDataForModelPreviewPanel( ITexture* pSprayTex, int nRarity, int nTintID, float flCreateTime );
		
		QcRefreshDecalDataForModelPreviewPanel( pScene->m_pSprayTex, pScene->m_nSprayRarity, pScene->m_nSprayTintID, pScene->m_flSprayCreationTime );
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	ICallQueue* pCQ = pRenderContext->GetCallQueue();
	if ( pCQ )
	{
		// Queuing a functor to be executed on the render thread. The functor will hold
		// a CRefPtr< IUITexture > and therefore when the functor is destroyed it will 
		// call the CRefPtr destructor that will eventually decrement the ref count.
		// Note that copying a CRefPtr will not automatically increment the ref count
		// so we are incrementing it here
		m_pPanelRT->AddRef();
		pCQ->QueueCall( AccessRenderDevice(), &IUIRenderDevice::PushPanelRT, m_pPanelRT );
	}
	else
	{
		AccessRenderDevice()->PushPanelRT( m_pPanelRT );
	}

	Vector4D scissor( 0, 0, nW, nH );
	m_pItemPreviewRenderer->RenderThreadCallback( &scissor, 0, 0, nW, nH, true );

	if ( pCQ )
	{
		// cf comment above about ref count
		m_pPanelRT->AddRef();
		pCQ->QueueCall( AccessRenderDevice(), &IUIRenderDevice::PopPanelRT, m_pPanelRT );
	}
	else
	{
		AccessRenderDevice()->PopPanelRT( m_pPanelRT  );
	}

	m_flLastTime = flTime;
}


void CUI_ItemPreviewPanel::OnVisibilityChanged()
{
	if ( m_pItemPreviewRenderer )
		m_pItemPreviewRenderer->SetNeedsRedraw( true );

	BaseClass::OnVisibilityChanged();
}

//-----------------------------------------------------------------------------
// Not all scene panels are live
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewPanel::BShouldAlwaysRepaint() 
{
	return m_bIsLiveView && ( !m_bAllowSuspendRepaint || s_nSuspendRepaintCount <= 0 );
}

static int UI_HelperItemPreviewCoordinate( char const *val, int minValue, int valRange )
{
	int nBase = 0;
	int nFactor = 1;
	int nDenom = 1;
	bool bProportional = true;

	switch ( val[0] )
	{
	case 'c':
		nBase = (minValue + valRange / 2);
		++val;
		break;
	case 'l':
		nBase = minValue;
		++val;
		break;
	case 'r':
		nBase = (minValue + valRange);
		++val;
		break;
	}

	switch ( val[0] )
	{
	case '%':
		nFactor = valRange;
		nDenom = 100;
		bProportional = false;
		++val;
		break;
	}

	int nActualVal = Q_atoi( val );

// 	if ( bProportional )
// 		nActualVal = vgui::scheme()->GetProportionalScaledValueEx( hScheme, nActualVal );

	return (nBase + (nActualVal * nFactor) / nDenom);
}

void UI_AccumulateItemPreviewSettingsStringValue( KeyValues *kvGlobal, KeyValues *kvExtra, char const *szName )
{
	if ( char const *szNewValue = kvExtra->GetString( szName, NULL ) )
	{
		if ( char const *szOldValue = kvGlobal->GetString( szName, NULL ) )
		{
			ItemPreviewMsg( "%s overrides '%s' with '%s'\n", szName, szOldValue, szNewValue );
		}
		else
		{
			ItemPreviewMsg( "%s = '%s'\n", szName, szNewValue );
		}
		kvGlobal->SetString( szName, szNewValue );
	}
}

void UI_AccumulateItemPreviewSettingsFloatValue( KeyValues *kvGlobal, KeyValues *kvExtra, char const *szName )
{
	KeyValues *kvKey = kvExtra->FindKey( szName );
	if ( !kvKey )
		return;

	KeyValues *kvOldKey = kvGlobal->FindKey( szName );
	if ( kvOldKey )
	{
		ItemPreviewMsg( "%s overrides '%.3f' with '%.3f'\n", szName, kvOldKey->GetFloat(), kvKey->GetFloat() );
	}
	else
	{
		ItemPreviewMsg( "%s = '%.3f'\n", szName, kvKey->GetFloat() );
	}
	kvGlobal->SetFloat( szName, kvKey->GetFloat() );
}

void UI_AccumulateItemPreviewSettingsConfig( KeyValues *kvGlobal, KeyValues *kvExtra )
{
	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "root_mdl" );
	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "root_anim" );
	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "root_anim_loop" );
	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "weapon_anim" );
	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "weapon_anim_loop" );
	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "root_camera" );
	UI_AccumulateItemPreviewSettingsFloatValue( kvGlobal, kvExtra, "root_camera_fov" );

	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "shadow_light" );
	UI_AccumulateItemPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_znear" );
	UI_AccumulateItemPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_zfar" );
	UI_AccumulateItemPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_hfov" );
	UI_AccumulateItemPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_vfov" );
	UI_AccumulateItemPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_atten_quadratic" );
	UI_AccumulateItemPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_atten_linear" );
	UI_AccumulateItemPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_atten_constant" );
	UI_AccumulateItemPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_atten_farz" );
	UI_AccumulateItemPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_brightness" );
	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "shadow_light_color" );
	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "shadow_light_texture" );

	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "light_ambient" );

	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "camera_offset" );
	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "camera_orient" );
	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "orbit_pivot" );

	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "shadow_light_offset" );
	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "shadow_light_orient" );
	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "shadow_light_rotation" );
	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "shadow_light_flicker" );

	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "item_rotate" );
	UI_AccumulateItemPreviewSettingsStringValue( kvGlobal, kvExtra, "item_orient" );

	if ( kvExtra->GetBool( "light_directional_clearall" ) )
	{
		if ( KeyValues *kvLights = kvGlobal->FindKey( "light_directional" ) )
		{
			ItemPreviewMsg( "light_directional_clearall clears the following directional lights:\n" );
			ItemPreviewKVMsg( kvGlobal->FindKey( "light_directional" ), 1, 1 );
			kvGlobal->RemoveSubKey( kvLights );
			kvLights->deleteThis();
		}
	}

	if ( kvExtra->GetBool( "mergemdls_clearall" ) )
	{
		if ( KeyValues *kvMergeMdls = kvGlobal->FindKey( "mergemdls" ) )
		{
			ItemPreviewMsg( "mergemdls_clearall clears the following mergemdls:\n" );
			ItemPreviewKVMsg( kvGlobal->FindKey( "mergemdls" ), 1, 1 );
			kvGlobal->RemoveSubKey( kvMergeMdls );
			kvMergeMdls->deleteThis();
		}
	}

	if ( kvExtra->GetBool( "weaponmergemdls_clearall" ) )
	{
		if ( KeyValues *kvMergeMdls = kvGlobal->FindKey( "weaponmergemdls" ) )
		{
			ItemPreviewMsg( "weaponmergemdls_clearall clears the following weaponmergemdls:\n" );
			ItemPreviewKVMsg( kvGlobal->FindKey( "weaponmergemdls" ), 1, 1 );
			kvGlobal->RemoveSubKey( kvMergeMdls );
			kvMergeMdls->deleteThis();
		}
	}

	for ( KeyValues *kvSub = kvExtra->GetFirstSubKey(); kvSub; kvSub = kvSub->GetNextKey() )
	{
		if ( !Q_stricmp( kvSub->GetName(), "light_directional_add" ) )
		{
			ItemPreviewMsg( "light_directional_add '%s'\n", kvSub->GetString() );
			kvGlobal->FindKey( "light_directional", true )->AddSubKey( kvSub->MakeCopy() );
		}
		else if ( !Q_stricmp( kvSub->GetName(), "mergemdl_add" ) )
		{
			ItemPreviewMsg( "mergemdl_add '%s'\n", kvSub->GetString() );
			kvGlobal->FindKey( "mergemdls", true )->AddSubKey( kvSub->MakeCopy() );
		}
		else if ( !Q_stricmp( kvSub->GetName(), "weaponmergemdl_add" ) )
		{
			ItemPreviewMsg( "weaponmergemdl_add '%s'\n", kvSub->GetString() );
			kvGlobal->FindKey( "weaponmergemdls", true )->AddSubKey( kvSub->MakeCopy() );
		}
		else if ( !Q_stricmp( kvSub->GetName(), "camera_preset_add" ) )
		{
			ItemPreviewMsg( "camera_preset_add '%s'\n", kvSub->GetString() );
			kvGlobal->FindKey( "camera_preset_add", true )->AddSubKey( kvSub->MakeCopy() );
		}
		else if ( !Q_stricmp( kvSub->GetName(), "mergemdl_clear" ) )
		{
			char const *szVal = kvSub->GetString();
			if ( KeyValues *kvMergeMdls = kvGlobal->FindKey( "mergemdls" ) )
			{
				for ( KeyValues *kvMergeMdl = kvMergeMdls->GetFirstSubKey(); kvMergeMdl; kvMergeMdl = kvMergeMdl->GetNextKey() )
				{
					if ( !Q_stricmp( kvMergeMdl->GetString(), szVal ) )
					{
						ItemPreviewMsg( "mergemdl_clear '%s'\n", szVal );
						kvMergeMdls->RemoveSubKey( kvMergeMdl );
						break;
					}
				}
			}
		}
		else if ( !Q_stricmp( kvSub->GetName(), "weaponmergemdl_clear" ) )
		{
			char const *szVal = kvSub->GetString();
			if ( KeyValues *kvMergeMdls = kvGlobal->FindKey( "weaponmergemdls" ) )
			{
				for ( KeyValues *kvMergeMdl = kvMergeMdls->GetFirstSubKey(); kvMergeMdl; kvMergeMdl = kvMergeMdl->GetNextKey() )
				{
					if ( !Q_stricmp( kvMergeMdl->GetString(), szVal ) )
					{
						ItemPreviewMsg( "weaponmergemdl_clear '%s'\n", szVal );
						kvMergeMdls->RemoveSubKey( kvMergeMdl );
						break;
					}
				}
			}
		}
	}
}



KeyValues * UI_BuildItemPreviewSettingsForEconItemView( const CEconItemView *pItem, KeyValues *kvManifest, PreviewMode previewMode, const char *pszMDL )
{
	extern bool Helper_IsSpray( const CEconItemDefinition * pEconItemDefinition );
	extern bool Helper_IsFanShield( const CEconItemDefinition * pEconItemDefinition );

	//
	// Determine rule processing params
	//
	char szItemModelBaseName[MAX_PATH] = {};

	int nItemTeam = TEAM_UNASSIGNED;
	bool bWorkshop = false;
	bool bWorkshopArms = false;
	bool bWorkshopGreenScreen = false;
	int nItemQuality = 0;
	int nItemRarity = 0;
	bool bStatTrak = false;
	bool bUidNameTag = false;
	bool bPreviewingStickers = false;

	char const *szItemType = "char_other";
	char const *szItemTeam = "Any";
	
	if ( pItem )
	{
		if ( char const *szModel = (previewMode == PreviewMode_Hold) ? pItem->GetPlayerDisplayModel() : pItem->GetPedestalDisplayModel() )
		{
			V_FileBase( szModel, szItemModelBaseName, Q_ARRAYSIZE( szItemModelBaseName ) );
		}
		ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView: model = '%s'\n", szItemModelBaseName );

		szItemType = pItem->GetStaticData() ? pItem->GetStaticData()->GetWeaponTypeString() : "other";
		if ( !szItemType )
		{
			if ( pItem->IsStickerTool() )
			{
				szItemType = "sticker_tool";
			}
			else if ( Helper_IsSpray( pItem->GetItemDefinition() ) || Helper_IsFanShield( pItem->GetItemDefinition() ) )
			{
				szItemType = "spray_tool";
			}
			else if ( pItem->GetStaticData() && pItem->GetStaticData()->IsTool() )
			{
				szItemType = "tool";
			}
			else if ( pItem->GetStaticData() && !V_strcmp( pItem->GetStaticData()->GetItemClass(), "wearable_item" ) )
			{
				szItemType = "wearable";
			}
			else
			{
				szItemType = "other";
			}
		}
		ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView: type = '%s'\n", szItemType );

		nItemTeam = pItem->GetStaticData() ? pItem->GetStaticData()->GetUsedByTeam() : TEAM_UNASSIGNED;
		szItemTeam = (nItemTeam == TEAM_UNASSIGNED) ? "Any" : (nItemTeam == TEAM_CT) ? "CT" : "T";
		ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView: type = '%s'\n", szItemTeam );

		bWorkshop = (pItem->GetCustomPaintKit() && (pItem->GetCustomPaintKit()->nID & 0xFFFFFFF0) == PREVIEW_PAINTKIT_ID_BASE) ? true : false;
		bWorkshopArms = (previewMode == PreviewMode_Hold);
		bWorkshopGreenScreen = (previewMode == PreviewMode_GreenScreen);
		ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView: workshop = '%s'\n", !bWorkshop ? "None" : (bWorkshopArms ? "Arms" : (bWorkshopGreenScreen ? "Green Screen" : "Workbench")) );

		nItemQuality = pItem->GetQuality();
		ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView: quality = %d\n", nItemQuality );

		nItemRarity = pItem->GetRarity();
		ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView: rarity = %d\n", nItemRarity );

		static CSchemaAttributeDefHandle pAttrDef_KillEater( "kill eater" );
		bStatTrak = pItem->FindAttribute( pAttrDef_KillEater );
		ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView: stattrak = %s\n", bStatTrak ? "yes" : "no" );

		bUidNameTag = (pItem->GetCustomName() != NULL);
		ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView: nametag = %s\n", bUidNameTag ? "yes" : "no" );

		bPreviewingStickers = (previewMode == PreviewMode_Stickers);
		ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView: previewing stickers = %s\n", bPreviewingStickers ? "yes" : "no" );
	}
	else
	{
		V_strcpy( szItemModelBaseName, pszMDL );
		ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView: mdl test = '%s'\n", szItemModelBaseName );
	}

	//
	// Build up the config
	//
	KeyValues *kvConfig = new KeyValues( "config" );

	// Walk all the keys
	for ( KeyValues *kvSection = kvManifest->GetFirstTrueSubKey(); kvSection; kvSection = kvSection->GetNextTrueSubKey() )
	{
		KeyValues *kvRuleRequirement = kvSection->FindKey( "rule" );
		if ( !kvRuleRequirement )
			continue; // this is not a rule section

		// Run the rules and early out if the rules don't match
		if ( char const *szRuleModel = kvRuleRequirement->GetString( "model", NULL ) )
		{
			if ( Q_stricmp( szRuleModel, szItemModelBaseName ) )
			{
				ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView section '%s' skipped due to model rule '%s'\n", kvSection->GetName(), szRuleModel );
				continue;
			}
		}
		if ( char const *szRuleModelPartial = kvRuleRequirement->GetString( "model_partial", NULL ) )
		{
			if ( !Q_stristr( szItemModelBaseName, szRuleModelPartial ) )
			{
				ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView section '%s' skipped due to model_partial rule '%s'\n", kvSection->GetName(), szRuleModelPartial );
				continue;
			}
		}
		if ( char const *szRuleType = kvRuleRequirement->GetString( "type", NULL ) )
		{
			if ( Q_stricmp( szRuleType, szItemType ) )
			{
				ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView section '%s' skipped due to type rule '%s'\n", kvSection->GetName(), szRuleType );
				continue;
			}
		}

		if ( char const *szRuleTeam = kvRuleRequirement->GetString( "team", NULL ) )
		{
			if ( Q_stricmp( szRuleTeam, szItemTeam ) )
			{
				ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView section '%s' skipped due to team rule '%s'\n", kvSection->GetName(), szRuleTeam );
				continue;
			}
		}

		int nRuleQuality = kvRuleRequirement->GetInt( "quality", -1 );
		if ( (nRuleQuality >= 0) && (nItemQuality != nRuleQuality) )
		{
			ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView section '%s' skipped due to quality rule '%d'\n", kvSection->GetName(), nRuleQuality );
			continue;
		}

		int nRuleRarity = kvRuleRequirement->GetInt( "rarity", -1 );
		if ( (nRuleRarity >= 0) && (nItemRarity < nRuleRarity) )
		{
			ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView section '%s' skipped due to rarity rule '%d'\n", kvSection->GetName(), nRuleRarity );
			continue;
		}

		int nRuleStatTrak = kvRuleRequirement->GetInt( "stattrak", -1 );
		if ( (nRuleStatTrak >= 0) && (bStatTrak != !!nRuleStatTrak) )
		{
			ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView section '%s' skipped due to stattrak rule '%d'\n", kvSection->GetName(), nRuleStatTrak );
			continue;
		}

		int nRuleUidNameTag = kvRuleRequirement->GetInt( "nametag", -1 );
		if ( (nRuleUidNameTag >= 0) && (bUidNameTag != !!nRuleUidNameTag) )
		{
			ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView section '%s' skipped due to nametag rule '%d'\n", kvSection->GetName(), nRuleUidNameTag );
			continue;
		}

		int nRuleWorkshop = kvRuleRequirement->GetInt( "workshop", -1 );
		if ( (nRuleWorkshop >= 0) && (bWorkshop != !!nRuleWorkshop) )
		{
			ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView section '%s' skipped due to workshop rule '%d'\n", kvSection->GetName(), nRuleWorkshop );
			continue;
		}

		int nRuleWorkshopArms = kvRuleRequirement->GetInt( "workshop_arms", -1 );
		if ( (nRuleWorkshopArms >= 0) && (bWorkshopArms != !!nRuleWorkshopArms) )
		{
			ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView section '%s' skipped due to workshop_arms rule '%d'\n", kvSection->GetName(), nRuleWorkshopArms );
			continue;
		}
		else if ( nRuleWorkshopArms >= 0 && !bWorkshop )
		{
			ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView section '%s' skipped due to workshop mode off\n", kvSection->GetName() );
			continue;
		}

		int nRuleWorkshopGreenScreen = kvRuleRequirement->GetInt( "workshop_greenscreen", -1 );
		if ( (nRuleWorkshopGreenScreen >= 0) && (bWorkshopGreenScreen != !!nRuleWorkshopGreenScreen) )
		{
			ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView section '%s' skipped due to workshop_greeenscreen rule '%d'\n", kvSection->GetName(), nRuleWorkshopGreenScreen );
			continue;
		}
		else if ( nRuleWorkshopGreenScreen >= 0 && !bWorkshop )
		{
			ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView section '%s' skipped due to workshop mode off\n", kvSection->GetName() );
			continue;
		}

		int nRuleStickerPreview = kvRuleRequirement->GetInt( "sticker_preview", -1 );
		if ( (nRuleStickerPreview >= 0) && (bPreviewingStickers != !!nRuleStickerPreview) )
		{
			ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView section '%s' skipped due to sticker preview rule '%d'\n", kvSection->GetName(), nRuleStickerPreview );
			continue;
		}

		// Here we know that all the rules passed!
		ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView section '%s' rules matched\n", kvSection->GetName() );
		UI_AccumulateItemPreviewSettingsConfig( kvConfig, kvSection->FindKey( "config" ) );
	}

	ItemPreviewMsg( "UI_BuildItemPreviewSettingsForEconItemView configuration:\n" );
	ItemPreviewKVMsg( kvConfig, 1, 1 );

	return kvConfig;
}

Vector UI_ParseSettingsVectorFromString( char const *szString )
{
	Vector vec = vec3_origin;
	if ( (szString[0] == '[') || (szString[0] == '{') )
		++szString;
	while ( *szString && V_isspace( *szString ) )
		++szString;

	vec.x = Q_atof( szString );
	while ( *szString && !V_isspace( *szString ) )
		++szString;
	while ( *szString && V_isspace( *szString ) )
		++szString;

	vec.y = Q_atof( szString );
	while ( *szString && !V_isspace( *szString ) )
		++szString;
	while ( *szString && V_isspace( *szString ) )
		++szString;

	vec.z = Q_atof( szString );

	return vec;
}

Vector2D UI_ParseSettingsVector2DFromString( char const *szString )
{
	Vector2D vec( 0.0f, 0.0f );

	if ( szString[0] == '[' )
		++szString;
	while ( *szString && V_isspace( *szString ) )
		++szString;

	vec.x = Q_atof( szString );
	while ( *szString && !V_isspace( *szString ) )
		++szString;
	while ( *szString && V_isspace( *szString ) )
		++szString;

	vec.y = Q_atof( szString );

	return vec;
}

Vector4D UI_ParseSettingsVector4DFromString( char const *szString )
{
	Vector4D vec = vec4_origin;
	if ( (szString[0] == '[') || (szString[0] == '{') )
		++szString;
	while ( *szString && V_isspace( *szString ) )
		++szString;

	vec.x = Q_atof( szString );
	while ( *szString && !V_isspace( *szString ) )
		++szString;
	while ( *szString && V_isspace( *szString ) )
		++szString;

	vec.y = Q_atof( szString );
	while ( *szString && !V_isspace( *szString ) )
		++szString;
	while ( *szString && V_isspace( *szString ) )
		++szString;

	vec.z = Q_atof( szString );
	while ( *szString && !V_isspace( *szString ) )
		++szString;
	while ( *szString && V_isspace( *szString ) )
		++szString;

	vec.w = Q_atof( szString );

	return vec;
}

float UI_ParseSettingsFloatFromString( char const *szString )
{
	float flTmp = 0.0f;
	if ( ( szString[ 0 ] == '[' ) || ( szString[ 0 ] == '{' ) )
		++szString;
	while ( *szString && V_isspace( *szString ) )
		++szString;

	flTmp = Q_atof ( szString );

	return flTmp;
}

static const CEconItemView * HelperLaunchWeaponPreviewItemResolve( uint64 iItemID )
{
	// Use the same inventory resolver as the original weapon preview panel.
	return CEconItemView::FindOrCreateEconItemViewForItemID( iItemID );
}


CEconItemView *CUI_ItemPreviewPanel::GetEconItem( const char *szItemName, bool *pPreviewingSingleSticker, int iClass, int iSlot )
{
	const CEconItemView *pEquippedItem = NULL;

	if ( !szItemName || ( Q_strlen( szItemName ) == 0 ) || !Q_stricmp( szItemName, "slot" ) )
	{
		CCSPlayerInventory *pLocalInv = CSInventoryManager()->GetLocalCSInventory();
		if ( pLocalInv )
		{
			pEquippedItem = pLocalInv->GetMutableItemInLoadout( iClass, iSlot );
		}
	}
	else if ( char const *szInventoryItemId = StringAfterPrefix( szItemName, "img://inventory_" ) )
	{
		uint64 itemid = Q_atoui64( szInventoryItemId );

		pEquippedItem = HelperLaunchWeaponPreviewItemResolve( itemid );
	}
	else if ( StringHasPrefix( szItemName, "img://itemdata_" ) )
	{
		CUtlVector< char* > urlFragments;

		V_SplitString( szItemName, "_", urlFragments );

		uint16 iDefIndex = (uint16)atoi( urlFragments[1] );
		uint16 iPaintIndex = (uint16)atoi( urlFragments[2] );
		uint64 ullItemId = CombinedItemIdMakeFromDefIndexAndPaint( iDefIndex, iPaintIndex );
		pEquippedItem = HelperLaunchWeaponPreviewItemResolve( ullItemId );
		urlFragments.PurgeAndDeleteElements();
	}
	else if ( char const *szStickerPreviewItemId = StringAfterPrefix( szItemName, "vmt://stickerpreview_" ) )
	{
		uint64 itemid = Q_atoui64( szStickerPreviewItemId );
		pEquippedItem = HelperLaunchWeaponPreviewItemResolve( itemid );

		if ( pEquippedItem->GetStaticData()->IsTool() && pPreviewingSingleSticker )
		{
			*pPreviewingSingleSticker = true;
		}
	}
	else if ( char const *szSprayInventoryItemId = StringAfterPrefix( szItemName, "vmt://spraypreview_" ) )
	{
		uint64 itemid = Q_atoui64( szSprayInventoryItemId );
		pEquippedItem = HelperLaunchWeaponPreviewItemResolve( itemid );

		if ( pEquippedItem->GetStaticData()->IsTool() && pPreviewingSingleSticker )
		{
			*pPreviewingSingleSticker = true;
		}
	}
	else if ( V_strstr( szItemName, ".mdl" ) )
	{
		// .mdl name
	}
	else
	{
		uint64 itemid = Q_atoui64( szItemName );

		pEquippedItem = HelperLaunchWeaponPreviewItemResolve( itemid );
	}

	// $$$REI For some reason we need a mutable econ item during preview to generate materials.
	return const_cast<CEconItemView*>( pEquippedItem ) ;
}

bool CUI_ItemPreviewPanel::LaunchPanelHelper_Inventory( CEconItemView *pEquippedItem )
{
	if ( pEquippedItem )
	{
		// still inventory - check if we have a valid mdl
		if ( !pEquippedItem->GetIconDisplayModel() )
		{
			DevWarning( "CUI_ItemPreviewPanel::LaunchPanelHelper_Inventory - no valid icon display model for item %s!\n", pEquippedItem->GetItemName() );
			return false;
		}

		//[OGS] UIEvent, Inspect. Categorized by player state, coarse item type.
// 		if ( !engine->IsConnected() )
// 		{
// 			//Main Menu
// 			CSAppLifetimeGameStats()->RecordUIEvent( pEquippedItem->GetItemID() ? "InventoryItemPainted" : "InventoryItemDefault" );
// 		}
// 		else
// 		{
// 			//Connected
// 			CSAppLifetimeGameStats()->RecordUIEvent( pEquippedItem->GetItemID() ? "InventoryItemConnectedPainted" : "InventoryItemConnectedDefault" );
// 		}
	}

	return true;
}

bool CUI_ItemPreviewPanel::LaunchPanelHelper_Manifest( CEconItemView *pEquippedItem, const bool bPreviewingSingleSticker, const UIItemInfo_t &info )
{
	const char *pManifestName = info.m_manifestName;
	const char *pItemName = info.m_itemName;

	// use cached manifest if available...
	KeyValues *kvManifest = NULL;
	for ( int nPanelManifest = 0; nPanelManifest < g_kvItemPreviewManifests.Count(); nPanelManifest++ )
	{
		KeyValues *kvPanelManifest = g_kvItemPreviewManifests[ nPanelManifest ];

		const char *pPanelManifestName = kvPanelManifest->GetName();

		if ( !V_stricmp( pPanelManifestName, pManifestName ) )
		{
			kvManifest = kvPanelManifest;
			break;
		}
	}

	// ...otherwise load new manifest
	if ( !kvManifest )
	{
		kvManifest = new KeyValues( "manifest" );

		if ( !kvManifest->LoadFromFile( g_pFullFileSystem, pManifestName ) )
		{
			kvManifest->deleteThis();
			return false;
		}

		g_kvItemPreviewManifests.AddToTail( kvManifest );
	}

	// record UI events - TODO - do we still need this, and if so, should this be modified or updated here (taken from SF WeaponPreviewPanel
	if ( pEquippedItem )
	{
		//[OGS] UIEvent, Inspect. Categorized by player state, coarse item type.
// 		if ( !engine->IsConnected() )
// 		{
// 			//Main Menu
// 			CSAppLifetimeGameStats()->RecordUIEvent( pEquippedItem->GetItemID() ? "ItemInspectMainMenuPainted" : "ItemInspectMainMenuDefault" );
// 		}
// 		else
// 		{
// 			//Connected
// 			CSAppLifetimeGameStats()->RecordUIEvent( pEquippedItem->GetItemID() ? "ItemInspectConnectedPainted" : "ItemInspectConnectedDefault" );
// 		}
	}
	else
	{
		// check model validity

//		CSAppLifetimeGameStats()->RecordUIEvent( "ItemInspectMainMenu_CharacterTest" );

		if ( V_strstr( pItemName, "models/player" ) != NULL )
		{
			m_bIsPlayerPanel = true;
		}
	}

	PreviewMode PanelPreviewMode = PreviewMode_Default;
	if ( Q_stristr( pItemName, "?stickers" ) )
	{
		PanelPreviewMode = PreviewMode_Stickers;
	}

	KeyValues *kvPreviewSettings = UI_BuildItemPreviewSettingsForEconItemView( pEquippedItem, kvManifest, PanelPreviewMode, pItemName );
	KeyValues::AutoDelete autodelete_kvPreviewSettings( kvPreviewSettings );

	if ( kvPreviewSettings )
	{
		m_kvConfiguration = kvPreviewSettings->MakeCopy();
	}

	return true;
}

ConVar panorama_3dpanel_pedestalmodel( "panorama_3dpanel_pedestalmodel", "1", FCVAR_DEVELOPMENTONLY, "" );

ConVar panorama_3dpanel_shadowslopescaledepthbias( "panorama_3dpanel_shadowslopescaledepthbias", "5.0", FCVAR_DEVELOPMENTONLY, "slopescaledepthbias to use when rendering flashlight depth" );
ConVar panorama_3dpanel_shadowdepthbias( "panorama_3dpanel_shadowdepthbias", "0.0", FCVAR_DEVELOPMENTONLY, "depthbias to use when rendering flashlight depth" );


// Helper class to work around a MDLCache threading issue caused by
// unloading / loading the same model in the same frame
// ( cf comments in CMDLCache::ClearAsync for a full explanation)
// This helper class ensures that mdls are unloaded after loading
// the new mdls and therefore avoiding cases where a mdl ref count
// would go from 1 to 0 and back to 1 in the same frame
class CDelayUnloadMDLsHelper
{
public:

	CDelayUnloadMDLsHelper( const CUtlVector< sMergedItemData > &aMergedItemData )
	{
		m_vecMDLsToUnload.EnsureCapacity( aMergedItemData.Count() );
		FOR_EACH_VEC( aMergedItemData, i )
		{
			const sMergedItemData &item = aMergedItemData[i];

			if ( item.m_hMDL != MDLHANDLE_INVALID )
			{
				g_pMDLCache->AddRef( item.m_hMDL );
				m_vecMDLsToUnload.AddToTail( item.m_hMDL );
			}
		}
	}

	~CDelayUnloadMDLsHelper()
	{
		FOR_EACH_VEC( m_vecMDLsToUnload, i )
		{
			g_pMDLCache->Release( m_vecMDLsToUnload[i] );
		}
	}

private:

	CUtlVector<MDLHandle_t> m_vecMDLsToUnload;
};


bool CUI_ItemPreviewPanel::InitSceneModels( CEconItemView *pEquippedItem, const char *szEconItemOrMDLName )
{
	extern bool Helper_IsSpray( const CEconItemDefinition * pEconItemDefinition );
	extern bool Helper_IsFanShield( const CEconItemDefinition * pEconItemDefinition );

	if ( !m_pItemPreviewRenderer )
		return false;

	CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;

	if ( !pScene )
	{
		DevWarning( "CUI_ItemPreviewPanel::ApplyInitialConfiguration - no renderer Scene Item!\n" );
		return false;
	}

	// clear current models
	// Using CDelayUnloadMDLsHelper to make sure we load the new mdls first
	// before unloading the existing one (cf CDelayUnloadMDLsHelper comments)
	CDelayUnloadMDLsHelper delayUnloadHelper( pScene->m_aMergedItemData );
	pScene->ClearMergeMDLs();
	pScene->m_aMergedItemData.Purge();

	if ( pEquippedItem )
	{
		// Setup the root model and merged models
		const char *pszRootMdl = m_kvConfiguration->GetString( "root_mdl" );

		// are we in first person arms mode?
		bool bUsePedestalModel = true;
		if ( V_stristr( pszRootMdl, "firstperson" ) != NULL )
		{
			bUsePedestalModel = false;
		}

		bool bNoRootMDL = false;

		if ( V_isempty( pszRootMdl ) )
		{
			bNoRootMDL = true;
		}

		if ( pEquippedItem->IsValid() )
		{
			if ( pEquippedItem->IsStickerTool() )
			{
				int nRarityIndex = pEquippedItem->GetRarity();
				if ( nRarityIndex >= 3 && nRarityIndex <= 7 )
					pScene->SetMDLSkinIndex( nRarityIndex - 3 );
			
				if ( !bNoRootMDL )
				{
					// valid root mdl
					pScene->InitMDL( pszRootMdl, NULL, NULL, NULL, m_econItemTextureSize );

					// merge 'scene/world' models
					for ( KeyValues *kvMergeMdls = m_kvConfiguration->FindKey( "mergemdls" )->GetFirstSubKey(); kvMergeMdls; kvMergeMdls = kvMergeMdls->GetNextKey() )
					{
						pScene->AddMergeMDL( kvMergeMdls->GetString(), NULL, NULL, NULL, false, m_econItemTextureSize );
					}

					pScene->SetMainMDL( pScene->AddMergeMDL( "models/inventory_items/sticker_inspect.mdl", NULL, NULL, NULL, false, m_econItemTextureSize ) );
				}
				else
				{
					// currently untested
					pScene->InitMDL( "models/inventory_items/sticker_inspect.mdl", NULL, NULL, NULL, m_econItemTextureSize );
				}

				CMDL *pStickerCMDL = pScene->GetMainCMDL();

				if ( pStickerCMDL )
				{
					IMaterial *pMatStickerOverride = pEquippedItem->GetToolStickerMaterial();
					if ( pMatStickerOverride && !pMatStickerOverride->IsErrorMaterial() )
					{
						pStickerCMDL->SetSimpleMaterialOverride( pMatStickerOverride );
					}
				}
			}
			else if ( Helper_IsSpray( pEquippedItem->GetItemDefinition() ) || Helper_IsFanShield( pEquippedItem->GetItemDefinition() ) )
			{
				if ( !bNoRootMDL )
				{
					// valid root mdl
					pScene->InitMDL( pszRootMdl, NULL, NULL, NULL, m_econItemTextureSize );

					// merge 'scene/world' models
					for ( KeyValues *kvMergeMdls = m_kvConfiguration->FindKey( "mergemdls" )->GetFirstSubKey(); kvMergeMdls; kvMergeMdls = kvMergeMdls->GetNextKey() )
					{
						pScene->AddMergeMDL( kvMergeMdls->GetString(), NULL, NULL, NULL, false, m_econItemTextureSize );
					}

					pScene->SetMainMDL( pScene->AddMergeMDL( "models/sprays/spray_plane.mdl", NULL, NULL, NULL, false, m_econItemTextureSize ) );
				}
				else
				{
					// currently untested
					pScene->InitMDL( "models/sprays/spray_plane.mdl", NULL, NULL, NULL, m_econItemTextureSize );
				}

				CMDL *pSprayCMDL = pScene->GetMainCMDL();

				if ( pSprayCMDL )
				{
					extern IMaterial * QcCreateDecalDataForModelPreviewPanel( int nStickerKitDefinition, int nTintID );
					uint32 nStickerId = pEquippedItem->GetStickerAttributeBySlotIndexInt( 0, k_EStickerAttribute_ID, 0 );

					static CSchemaAttributeDefHandle hAttrSprayTintID( "spray tint id" );
					uint32 unTintID = 0;
					if ( !hAttrSprayTintID || !pEquippedItem->FindAttribute( hAttrSprayTintID, &unTintID ) )
						unTintID = 0;

					IMaterial *pMatStickerOverride = QcCreateDecalDataForModelPreviewPanel( nStickerId, unTintID );
					if ( pMatStickerOverride && !pMatStickerOverride->IsErrorMaterial() )
					{
						pSprayCMDL->SetSimpleMaterialOverride( pMatStickerOverride );

						// filled in later (if mat_queue_mode = -1, or below if 0)
						pScene->m_pSprayTex = nullptr;

						extern void QcGetDecalDataForModelPreviewPanel( ITexture** ppTex, int *pRarity, int *pTintID, float *pCreateTime );

						// note this call incs ref count of spray tex, released in ::Reset
						QcGetDecalDataForModelPreviewPanel( &pScene->m_pSprayTex, &pScene->m_nSprayRarity, &pScene->m_nSprayTintID, &pScene->m_flSprayCreationTime );
					}
				}
			}
			else
			{
				const char *pszItem;

				if ( !bNoRootMDL )
				{
					// valid root mdl
					pScene->InitMDL( pszRootMdl, NULL, NULL, NULL, m_econItemTextureSize );

					// Add the item merged to the root model, request bonemerge takeover from that point and add models merged to the item after it
					pszItem = bUsePedestalModel ? pEquippedItem->GetPedestalDisplayModel() : pEquippedItem->GetPlayerDisplayModel();
					int iModelIndex = pScene->AddMergeMDL( pszItem, pEquippedItem ? szEconItemOrMDLName : NULL, pEquippedItem, NULL, true, m_econItemTextureSize );

					if ( iModelIndex >= 0 )
					{
						pScene->SetMainMDL( iModelIndex );
					}
				}
				else
				{
					// no valid root mdl

					// TODO - must be a smarter way of getting the appropriate mdl?
					if ( !m_bRequiresHighResModel && pEquippedItem->GetBuyMenuDisplayModel() )
					{
						pszItem = pEquippedItem->GetBuyMenuDisplayModel();
					}
					else 
					{
						pszItem = pEquippedItem->GetPedestalDisplayModel();

						if( !pszItem )
							pszItem = pEquippedItem->GetPlayerDisplayModel();
					}

					if ( !pszItem && pEquippedItem->GetStaticData() && !V_strcmp( pEquippedItem->GetStaticData()->GetItemClass(), "wearable_item" ) )
					{
						pszItem = pEquippedItem->GetExtraWearableModel();
					}

					if ( !pszItem )
					{
						pszItem = pEquippedItem->GetWorldDisplayModel();
					}

					if ( !pszItem )
					{
						pszItem = pEquippedItem->GetWorldDroppedModel();
					}

					if ( !pszItem )
					{
						DevWarning( "CUI_ItemPreviewPanel::InitSceneModels - no valid item and no valid root mdl!\n" );
						return false;
					}

					pScene->InitMDL( pszItem, pEquippedItem ? szEconItemOrMDLName : NULL, pEquippedItem, NULL, m_econItemTextureSize );
				}

				// merge 'scene/world' models
				KeyValues* kvMergeMdls = m_kvConfiguration->FindKey( "mergemdls" );
				if ( kvMergeMdls )
				{
					FOR_EACH_SUBKEY( kvMergeMdls, kvMergeMdl )
					{
						pScene->AddMergeMDL( kvMergeMdl->GetString(), NULL, NULL, NULL, false, m_econItemTextureSize );
					}
				}
			}

			KeyValues* kvWeaponMergeMdls = m_kvConfiguration->FindKey( "weaponmergemdls" );
			if ( kvWeaponMergeMdls )
			{
				FOR_EACH_SUBKEY( kvWeaponMergeMdls, kvWeaponMergeMdl )
				{
					pScene->AddMergeMDL( kvWeaponMergeMdl->GetString(), NULL, NULL, NULL, false, m_econItemTextureSize );
				}
			}

			// apply stickers to equipped item
			MergeStickersToItem( pEquippedItem, pScene );
		}
	}
	else
	{
		// Setup the root model and merged models
		const char *pszRootMdl = m_kvConfiguration->GetString( "root_mdl" );

		// are we in first person arms mode?
		bool bUsePedestalModel = true;
		if ( V_stristr( pszRootMdl, "firstperson" ) != NULL )
		{
			bUsePedestalModel = false;
		}

		if ( !V_isempty( pszRootMdl ) )
		{
			pScene->InitMDL( pszRootMdl, NULL, NULL, NULL, m_econItemTextureSize );
			pScene->AddMergeMDL( pEquippedItem ? NULL : szEconItemOrMDLName, NULL, NULL, NULL, false, m_econItemTextureSize );
		}
		else
		{
			const char *pMDLName = pEquippedItem ? NULL : szEconItemOrMDLName;
			if ( !V_isempty( pMDLName ) )
			{
				pScene->InitMDL( pMDLName, NULL, NULL, NULL, m_econItemTextureSize );
			}
		}

		// merge 'scene/world' models
		KeyValues *pMergeMdlsKV = m_kvConfiguration->FindKey( "mergemdls" );
		if ( pMergeMdlsKV )
		{
			FOR_EACH_SUBKEY(pMergeMdlsKV, kvMergeMdl)
			{
				pScene->AddMergeMDL( kvMergeMdl->GetString(), NULL, NULL, NULL, false, m_econItemTextureSize );
			}
		}

		// transparent floor for flashlight shadow
		if ( m_info.m_bEnableFloorShadow )
			pScene->AddMergeMDL( SHADOW_FLOOR_MDL, NULL, NULL, NULL, false );
	}

	return true;
}

bool CUI_ItemPreviewPanel::ApplyInitialConfiguration_Manifest( CEconItemView *pEquippedItem, const char *szEconItemOrMDLName, bool bInventoryLighting )
{
	if ( !m_pItemPreviewRenderer )
		return false;

	// scene
	InitSceneModels ( pEquippedItem, szEconItemOrMDLName );

	CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;

	MDLHandle_t hMDLMain = pScene->GetMainMDLHandle();

	// reset camera
	m_pItemPreviewRenderer->ResetCameraPivot();
	m_pItemPreviewRenderer->SetCameraOffset( vec3_origin );
	m_pItemPreviewRenderer->SetCameraPositionAndAngles( vec3_origin, vec3_angle );
	m_pItemPreviewRenderer->SetCameraFOV( DEFAULT_CAM_FOV );
	m_pItemPreviewRenderer->UpdateCameraPreset( 0, vec3_origin, vec3_origin, vec3_angle, DEFAULT_CAM_FOV );
	pScene->SetAnglesAndPosition( hMDLMain, vec3_angle, vec3_origin );

	// reset time
	m_pItemPreviewRenderer->OnTick();

	//
	// Apply all the configuration settings
	//

	// Directional Lights
	Vector vAmbientColor = UI_ParseSettingsVectorFromString(m_kvConfiguration->GetString("light_ambient", "[0.11 0.11 0.12]"));
	SetAmbientLightColor( vAmbientColor.x, vAmbientColor.y, vAmbientColor.z );
	int nLight = 0;
	if ( KeyValues* kvDirectionalLights = m_kvConfiguration->FindKey( "light_directional" ) )
	{
		FOR_EACH_SUBKEY( kvDirectionalLights, kvDirectionalLight )
		{
			char const *sz = kvDirectionalLight->GetString();
			Vector rgb = vec3_origin, dir = vec3_origin, rot = vec3_origin;
			Vector4D flicker = vec4_origin;
			bool bflRGB = false;

			if ( char const *szRGB = V_strstr( sz, "rgb[" ) )
			{
				rgb = UI_ParseSettingsVectorFromString( szRGB + 3 );
				bflRGB = false;
			}

			if ( char const *szRGB = V_strstr( sz, "rgb{" ) )
			{
				rgb = UI_ParseSettingsVectorFromString( szRGB + 3 );
				bflRGB = true;
			}

			if ( char const *szDIR = V_strstr( sz, "dir[" ) )
			{
				dir = UI_ParseSettingsVectorFromString( szDIR + 3 );
			}

			if ( char const *szROT = V_strstr( sz, "rot[" ) )
			{
				rot = UI_ParseSettingsVectorFromString( szROT + 3 );
			}

			if ( char const *szFLICKER = V_strstr( sz, "flicker[" ) )
			{
				flicker = UI_ParseSettingsVector4DFromString( szFLICKER + 7 );
			}

			char chLightAttachmentPoint[128] = {};
			if ( char const *szLightAttachmentPoint = V_strstr( sz, "attach[" ) )
			{
				char const *pszEnd = strchr( szLightAttachmentPoint + 7, ']' );
				if ( pszEnd > szLightAttachmentPoint + 7 )
				{
					V_sprintf_safe( chLightAttachmentPoint, "%.*s", pszEnd - szLightAttachmentPoint - 7, szLightAttachmentPoint + 7 );
					if ( dir == vec3_origin )	// set a default direction when having attachment light
						dir.Init( 0.0f, 0.0f, -1.0f );
				}
			}

			if ( rgb != vec3_origin && dir != vec3_origin )
			{
				if ( !bflRGB )
				{
					rgb /= 255.0f;
				}

				SetDirectionalLightModify( nLight );

				SetDirectionalLightColor( rgb.x, rgb.y, rgb.z );
				SetDirectionalLightDirection( dir.x, dir.y, dir.z );

				if ( flicker != vec4_origin )
				{
					SetDirectionalLightPulseFlicker( flicker.x, flicker.y, flicker.z, flicker.w );
				}

				if ( chLightAttachmentPoint[0] )
				{
					m_pItemPreviewRenderer->SetDirectionalLightAttachment( pScene, nLight, chLightAttachmentPoint );
				}
				else if ( rot != vec3_origin )
				{
					SetDirectionalLightRotation( rot.x, rot.y, rot.z );
				}

				// this will add the light to the renderer initial state
				m_pItemPreviewRenderer->SetDirectionalLight(nLight, rgb, dir);

				nLight++;
			}
		}
	}

	// Item rotation bounds and axis order
	if ( KeyValues *kvItemRotate = m_kvConfiguration->FindKey( "item_rotate" ) )
	{
		char const *sz = kvItemRotate->GetString();
		Vector2D rot = vec2_origin;
		RotateAxis rotAxis = RotateAxis_Invalid;
		int rotOrder = 0;

		while ( *sz )
		{
			bool bFound = false;

			if ( sz[ 0 ] == 'x' )
			{
				rot = UI_ParseSettingsVector2DFromString( sz + 1 );
				rotAxis = RotateAxis_X;
				bFound = true;
			}
			else if ( sz[ 0 ] == 'y' )
			{
				rot = UI_ParseSettingsVector2DFromString( sz + 1 );
				rotAxis = RotateAxis_Y;
				bFound = true;
			}
			else if ( sz[ 0 ] == 'z' )
			{
				rot = UI_ParseSettingsVector2DFromString( sz + 1 );
				rotAxis = RotateAxis_Z;
				bFound = true;
			}

			if ( bFound )
			{
				m_aRotateAxisSign[ rotOrder ] = 1.0f;

				rot.x = clamp( rot.x, -360.0f, 360.f );
				rot.y = clamp( rot.y, -360.0f, 360.f );
				if ( rot.x > rot.y )
				{
					float flTmp = rot.x;
					rot.x = rot.y;
					rot.y = flTmp;

					m_aRotateAxisSign[ rotOrder ] = -1.0f;
				}

				m_aRotateAxisOrder[ rotOrder ] = rotAxis;
				m_aRotateAxisBounds[ rotOrder ] = rot;

				rotOrder++;
				++sz;
			}

			++sz;
		}
		m_nNumRotateAxes = rotOrder;
	}

	// Initial item orientation
	sscanf( m_kvConfiguration->GetString( "item_orient", "0 0 0" ), "%f %f %f", &m_itemInitAngles.x, &m_itemInitAngles.y, &m_itemInitAngles.z );

	m_pItemPreviewRenderer->SetSetupRenderStateDelayed( true );

	// Animation
	pScene->SetAnim( m_kvConfiguration->GetString( "root_anim", "ACT_IDLE_INSPECT" ), true );
	pScene->AddAnimFollowLoop( m_kvConfiguration->GetString( "root_anim_loop", "ACT_IDLE_INSPECT" ), true );

	//Disabling support for default item pedestal animation
	{
		MDLCACHE_CRITICAL_SECTION();

		CMDL *pMdlItemConfig = pScene->GetMDLItemConfig();

		if ( pMdlItemConfig->m_MDLHandle != MDLHANDLE_INVALID )
		{
			CStudioHdr studioHdr( pMdlItemConfig->GetStudioHdr(), g_pMDLCache );

			char const *szItemAnim = m_kvConfiguration->GetString( "weapon_anim", "ACT_IDLE_INSPECT" );

			int iSequence = UI_HelperGetSequenceIdFromStudioHdr( studioHdr, szItemAnim );
			if ( iSequence != ACT_INVALID )
			{
				pMdlItemConfig->m_nSequence = iSequence;
				pMdlItemConfig->m_bUseSequencePlaybackFPS = true;

				char const *szItemAnimLoop = m_kvConfiguration->GetString( "weapon_anim_loop", "ACT_IDLE_INSPECT" );
				iSequence = UI_HelperGetSequenceIdFromStudioHdr( studioHdr, szItemAnimLoop );

				if ( iSequence != ACT_INVALID )
				{
					pMdlItemConfig->m_arrSequenceFollowLoop.AddToTail( iSequence );
				}
			}
		}
	}

	// flashlight shadow (render capture) and associated light and view settings
	m_pItemPreviewRenderer->EnableRenderingWithFlashlight( reinterpret_cast<void *>( &m_cfgRenderCapture ) );

	// get render camera and capture (flashlight shadow) camera attach bones, if present...otherwise use offsets
	m_pItemPreviewRenderer->SetCameraAttachment( pScene, m_kvConfiguration->GetString( "root_camera", "attach_camera_inspect" ) );
	m_pItemPreviewRenderer->SetRenderCaptureCameraAttachment( pScene, m_kvConfiguration->GetString( "shadow_light", "attach_camera_inspect_light" ) );

	m_pItemPreviewRenderer->SetCameraFOV( m_kvConfiguration->GetFloat( "root_camera_fov", 54.0f ) );
	
	if ( m_pDebug )
		m_pDebug->UpdateCameraFOVPanels( m_pItemPreviewRenderer->GetCameraFOV (), true );

	QAngle angCameraOffset;
	sscanf( m_kvConfiguration->GetString( "camera_orient", "0 0 0" ), "%f %f %f", &angCameraOffset.x, &angCameraOffset.y, &angCameraOffset.z );
	m_pItemPreviewRenderer->SetCameraOrientOverride( angCameraOffset );
	m_pItemPreviewRenderer->SetCameraOrientOverrideEnabled( true );
	sscanf( m_kvConfiguration->GetString( "shadow_light_orient", "0 0 0" ), "%f %f %f", &angCameraOffset.x, &angCameraOffset.y, &angCameraOffset.z );
	SetFlashlightAngle( angCameraOffset.x, angCameraOffset.y, angCameraOffset.z );

	Vector vecCameraOffset;
	sscanf( m_kvConfiguration->GetString( "camera_offset", "0 0 0" ), "%f %f %f", &vecCameraOffset.x, &vecCameraOffset.y, &vecCameraOffset.z );
	m_pItemPreviewRenderer->SetCameraPositionOverride( vecCameraOffset );
	m_pItemPreviewRenderer->SetCameraPositionOverrideEnabled( true );
	sscanf(m_kvConfiguration->GetString( "shadow_light_offset", "0 0 0"), "%f %f %f", &vecCameraOffset.x, &vecCameraOffset.y, &vecCameraOffset.z );
	SetFlashlightPosition( vecCameraOffset.x, vecCameraOffset.y, vecCameraOffset.z );
	sscanf( m_kvConfiguration->GetString( "orbit_pivot", "0 0 0" ), "%f %f %f", &vecCameraOffset.x, &vecCameraOffset.y, &vecCameraOffset.z );
	m_pItemPreviewRenderer->SetCameraPivotPosition( vecCameraOffset );

	// camera presets
	int iCameraPreset = 1;	// start at 1, since 0 is the default manifest camera
	if ( KeyValues* kvCameraPresets = m_kvConfiguration->FindKey ( "camera_preset_add" ) )
	{
		FOR_EACH_SUBKEY ( kvCameraPresets, kvCameraPreset )
		{
			char const *sz = kvCameraPreset->GetString ();

			Vector pos = vec3_origin, pivot = vec3_origin, orient = vec3_origin;

			if ( char const *szPos = V_strstr ( sz, "pos[" ) )
			{
				pos = UI_ParseSettingsVectorFromString ( szPos + 3 );
			}

			if ( char const *szPivot = V_strstr ( sz, "pivot[" ) )
			{
				pivot = UI_ParseSettingsVectorFromString ( szPivot + 5 );
			}

			if ( char const *szOrient = V_strstr ( sz, "orient[" ) )
			{
				orient = UI_ParseSettingsVectorFromString ( szOrient + 6 );
			}

			float fov;
			if ( char const *szFOV = V_strstr ( sz, "fov[" ) )
			{
				fov = UI_ParseSettingsFloatFromString ( szFOV + 3 );
			}
			else
			{
				fov = m_pItemPreviewRenderer->GetCameraFOV ();
			}


			m_pItemPreviewRenderer->UpdateCameraPreset ( iCameraPreset, pos, pivot, *( (QAngle *)&orient ), fov );

			iCameraPreset++;
		}
	}
	m_pItemPreviewRenderer->UpdateCameraPreset0();

	extern bool ClientShadowMgrAcquireShadowDepthTexture( CTextureReference *pDummyColorTexture, CTextureReference *pShadowDepthTexture );
	ClientShadowMgrAcquireShadowDepthTexture( &m_shadowDummyColorBufferTexture, &m_shadowDepthTexture );
	m_cfgRenderCapture.m_pIVRenderView = render;
	m_cfgRenderCapture.m_pFlashlightDepthTexture = m_shadowDepthTexture;
	m_cfgRenderCapture.m_pDummyColorBufferTexture = m_shadowDummyColorBufferTexture;

	m_cfgRenderCapture.m_renderFlashlightState.m_NearZ = m_kvConfiguration->GetFloat( "shadow_light_znear", 4.0f );
	m_cfgRenderCapture.m_renderFlashlightState.m_FarZ = m_kvConfiguration->GetFloat( "shadow_light_zfar", 512.0f );
	m_cfgRenderCapture.m_renderFlashlightState.m_fHorizontalFOVDegrees = m_kvConfiguration->GetFloat( "shadow_light_hfov", 54.0f );
	m_cfgRenderCapture.m_renderFlashlightState.m_fVerticalFOVDegrees = m_kvConfiguration->GetFloat( "shadow_light_vfov", 54.0f );
	m_pItemPreviewRenderer->SetRenderCaptureCameraFOV( m_cfgRenderCapture.m_renderFlashlightState.m_fHorizontalFOVDegrees );

	m_cfgRenderCapture.m_renderFlashlightState.m_fQuadraticAtten = m_kvConfiguration->GetFloat( "shadow_light_atten_quadratic", 0.0f );
	m_cfgRenderCapture.m_renderFlashlightState.m_fLinearAtten = m_kvConfiguration->GetFloat( "shadow_light_atten_linear", 1024.0f );
	m_cfgRenderCapture.m_renderFlashlightState.m_fConstantAtten = m_kvConfiguration->GetFloat( "shadow_light_atten_constant", 0.0f );
	m_cfgRenderCapture.m_renderFlashlightState.m_FarZAtten = m_kvConfiguration->GetFloat( "shadow_light_atten_farz", 1024.0f );

	Vector vecShadowLightColor = UI_ParseSettingsVectorFromString( m_kvConfiguration->GetString( "shadow_light_color" ) );
	vecShadowLightColor *= m_kvConfiguration->GetFloat ( "shadow_light_brightness", 1.0f );
	SetFlashlightAmount( 1.0f ); // scale/amount already incorporated into color above
	SetFlashlightColor( vecShadowLightColor.x, vecShadowLightColor.y, vecShadowLightColor.z );

	Vector vecShadowLightRot = UI_ParseSettingsVectorFromString( m_kvConfiguration->GetString( "shadow_light_rotation" ) );
	SetFlashlightRotation( vecShadowLightRot.x, vecShadowLightRot.y, vecShadowLightRot.z );

	Vector4D vecShadowLightFlicker = UI_ParseSettingsVector4DFromString( m_kvConfiguration->GetString( "shadow_light_flicker" ) );
	SetFlashlightPulseFlicker( vecShadowLightFlicker.x, vecShadowLightFlicker.y, vecShadowLightFlicker.z, vecShadowLightFlicker.w );

	m_cfgRenderCapture.m_renderFlashlightState.m_nSpotlightTextureFrame = 0;
	m_cfgRenderCapture.m_renderFlashlightState.m_pProjectedMaterial = NULL;
	if ( !m_shadowFlashlightCookie.IsValid() )
	{
		m_shadowFlashlightCookie.Init( m_kvConfiguration->GetString( "shadow_light_texture", "effects/flashlight001" ), TEXTURE_GROUP_OTHER, true );
	}
	m_cfgRenderCapture.m_renderFlashlightState.m_pSpotlightTexture = m_shadowFlashlightCookie;

	m_cfgRenderCapture.m_renderFlashlightState.m_bEnableShadows = true;
	m_cfgRenderCapture.m_renderFlashlightState.m_bShadowHighRes = true;
	m_cfgRenderCapture.m_renderFlashlightState.m_bDrawShadowFrustum = false;
	m_cfgRenderCapture.m_renderFlashlightState.m_flShadowSlopeScaleDepthBias = panorama_3dpanel_shadowslopescaledepthbias.GetFloat();
	m_cfgRenderCapture.m_renderFlashlightState.m_flShadowDepthBias = panorama_3dpanel_shadowdepthbias.GetFloat();

	if ( m_cfgRenderCapture.m_renderFlashlightState.m_NearZ >= m_cfgRenderCapture.m_renderFlashlightState.m_FarZ )
	{
		SetFlashlightNearFarZFromSceneBounds();
	}

	// 3d panels in Panorama assumed to be HDR enabled
	// a map that is not HDR enabled causes issues with the legacy flashlight Non-HDR path (2.0 multiplier), causing bright flashlight
	// we can't reset HDR mode for panorama 3d panel rendering since that is effectively the same cost as a device restore (invalidate/reload lightmaps, etc).
	m_cfgRenderCapture.m_renderFlashlightState.m_bIgnoreNonHDRPath = true;

	// lights
	if ( bInventoryLighting && pEquippedItem )
	{
		const InventoryImageData_t *pInventoryImageData = pEquippedItem->GetStaticData()->GetInventoryImageData();
		if ( pInventoryImageData )
		{
			MaterialLightingState_t *pLightingState = m_pItemPreviewRenderer->GetInitialLightingState();
			memset( pLightingState, 0, sizeof( MaterialLightingState_t ) );

			// default for inventory icon
			m_pItemPreviewRenderer->SetLightAmbient( Vector( 0.15f, 0.15f, 0.15f ) );

			//the default light is a blueish rim
			pLightingState->m_pLocalLightDesc[ 0 ].InitDirectional( Vector( -1.0f, 0.3f, 1.0f ), Vector( 1.5f, 1.8f, 2.0f ) );
			pLightingState->m_nLocalLightCount = 1;

			//pLightingState->m_nLocalLightCount = 0;
			//int nLightDescIndex = ( pInventoryImageData->m_bOverrideDefaultLight ) ? 0 : m_pRenderToRTData->m_LightingState.m_nLocalLightCount;
			int inventoryImageDataLightCount = 0;
			for ( int i = 0; i < MATERIAL_MAX_LIGHT_COUNT; i++ )
			{
				LightDesc_t *pLightDescSrc = pInventoryImageData->m_pLightDesc[ i ];

				if ( ( !pLightDescSrc ) || ( pLightDescSrc->m_Type == MATERIAL_LIGHT_DISABLE ) )
					continue;

				LightDesc_t *pLightDescDst = &pLightingState->m_pLocalLightDesc[ i ];

				*pLightDescDst = *pLightDescSrc;

				inventoryImageDataLightCount++;
			}

			if ( inventoryImageDataLightCount > 0 )
			{
				pLightingState->m_nLocalLightCount = inventoryImageDataLightCount;
			}

			// disable flashlight for now
			m_pItemPreviewRenderer->EnableRenderingWithFlashlight( NULL );
		}
	}

	// finished adding all lights
	m_pItemPreviewRenderer->SetInitialFlashlightState();

	m_pItemPreviewRenderer->SetCurrentFromInitialLightingState();

	m_flAnimateLightTotalPauseTime = pScene->GetRootMDL()->m_flTime;

	return true;
}

// TODO - get working for sprays and stickers (i.e. simple, non-manifest mdl view matching icon)
bool CUI_ItemPreviewPanel::ApplyInitialConfiguration_Inventory( CEconItemView *pEquippedItem )
{
	extern bool Helper_IsSpray( const CEconItemDefinition * pEconItemDefinition );
	extern bool Helper_IsFanShield( const CEconItemDefinition * pEconItemDefinition );

	if ( !m_pItemPreviewRenderer )
		return false;

	if ( !pEquippedItem )
		return false;

	const InventoryImageData_t *pInventoryImageData = pEquippedItem->GetStaticData()->GetInventoryImageData();
	//if ( !pInventoryImageData )
	//	return false;

	CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;

	if ( !pScene )
	{
		DevWarning( "CUI_ItemPreviewPanel::ApplyInitialConfiguration_Inventory - no scene item!!\n" );
		return false;
	}

	m_pItemPreviewRenderer->SetRenderingInventoryItem( true );

	m_pItemPreviewRenderer->ResetCameraPivot();
	m_pItemPreviewRenderer->SetCameraOffset( vec3_origin );
	m_pItemPreviewRenderer->SetCameraPositionAndAngles( vec3_origin, vec3_angle );

	// clear current models
	pScene->ClearMergeMDLs();

	// set up inventory mdl - used to match inventory icon rendering
	const char *pszInventoryItem = pEquippedItem->GetIconDisplayModel();

	//if ( pszInventoryItem )
	{
		bool bSticker = false;
		bool bSpray = false;
		bool bGlove = false;

		if ( pEquippedItem->IsValid() )
		{
			if ( pEquippedItem->IsStickerTool() )
			{
				// scene
				pScene->InitMDL( "models/inventory_items/sticker_inspect.mdl", NULL, NULL, NULL, m_econItemTextureSize );

				int nRarityIndex = pEquippedItem->GetRarity();
				if ( nRarityIndex >= 3 && nRarityIndex <= 7 )
					pScene->SetMDLSkinIndex( nRarityIndex - 3 );

				// swap over ptrs - not for stickers?
				//pEquippedItem = pScene->GetMainEconItemView();

				CMDL *pStickerMdlCMDL = pScene->GetRootMDL();

				if ( pStickerMdlCMDL )
				{
					IMaterial *pMatStickerOverride = pEquippedItem->GetToolStickerMaterial();
					if ( pMatStickerOverride && !pMatStickerOverride->IsErrorMaterial() )
					{
						pStickerMdlCMDL->SetSimpleMaterialOverride( pMatStickerOverride );
					}
				}
				bSticker = true;
			}
			else if ( Helper_IsSpray( pEquippedItem->GetItemDefinition() ) || Helper_IsFanShield( pEquippedItem->GetItemDefinition() ) )
			{
				// scene
				pScene->InitMDL( "models/sprays/spray_plane.mdl", NULL, NULL, NULL, m_econItemTextureSize );

				// swap over ptrs - not for sprays?
				//pEquippedItem = pScene->GetMainEconItemView();

				CMDL *pSprayMdlCMDL = pScene->GetRootMDL();

				if ( pSprayMdlCMDL )
				{
					extern IMaterial * QcCreateDecalDataForModelPreviewPanel( int nStickerKitDefinition, int nTintID );
					uint32 nStickerId = pEquippedItem->GetStickerAttributeBySlotIndexInt( 0, k_EStickerAttribute_ID, 0 );

					static CSchemaAttributeDefHandle hAttrSprayTintID( "spray tint id" );
					uint32 unTintID = 0;
					if ( !hAttrSprayTintID || !pEquippedItem->FindAttribute( hAttrSprayTintID, &unTintID ) )
						unTintID = 0;

					IMaterial *pMatStickerOverride = QcCreateDecalDataForModelPreviewPanel( nStickerId, unTintID );
					if ( pMatStickerOverride && !pMatStickerOverride->IsErrorMaterial() )
					{
						pSprayMdlCMDL->SetSimpleMaterialOverride( pMatStickerOverride );
					}
				}
				bSpray = true;
			}
			else
			{
				if ( !pszInventoryItem )
					return false;

				if ( V_strstr( pszInventoryItem, "glove" ) != NULL )
					bGlove = true;

				// scene
				pScene->InitMDL( pszInventoryItem, NULL, pEquippedItem, NULL, m_econItemTextureSize );

				// stickers
#if 0
				// no sticker attachments on inventory icon models!

				pEquippedItem->GenerateStickerMaterials();
				if ( pEquippedItem->ItemHasAnyStickersApplied() )
				{
					for ( int i = 0; i < pEquippedItem->GetNumSupportedStickerSlots(); i++ )
					{
						if ( pEquippedItem->GetStickerIMaterialBySlotIndex( i ) != NULL )
						{
							int iStickerMDLIndex = pScene->AddMergeMDL( pEquippedItem->GetStickerSlotModelBySlotIndex( i ) );
							MDLHandle_t hStickerMDL = pScene->m_aMergedItemData[ iStickerMDLIndex ].m_hMDL;

							if ( CMDL *pStickerMdlCMDL = ( hStickerMDL != MDLHANDLE_INVALID ) ? pScene->GetMergeMDL( hStickerMDL ) : NULL )
							{
								pStickerMdlCMDL->SetSimpleMaterialOverride( pEquippedItem->GetStickerIMaterialBySlotIndex( i ) );
							}
						}
					}
				}
#endif

			}
		}

		// camera
		matrix3x4_t matCamera;
		pScene->SetupBonesForAttachmentQueries();

		if ( pInventoryImageData && ( pInventoryImageData->m_cameraFOV != -1.0f ) )
		{
			m_pItemPreviewRenderer->SetCameraFOV( pInventoryImageData->m_cameraFOV );
		}
		else
		{
			m_pItemPreviewRenderer->SetCameraFOV( 20.0f );
		}
		
		if ( m_pDebug )
			m_pDebug->UpdateCameraFOVPanels ( m_pItemPreviewRenderer->GetCameraFOV (), true );

		{
			m_pItemPreviewRenderer->SetCameraAttachment( pScene, "camera_inventory" );

			if ( m_pItemPreviewRenderer->GetCameraAttachment() == -1 )
			{
				Vector vecCameraOffset = vec3_origin;
				QAngle angCameraOffset = vec3_angle;

				if ( pInventoryImageData )
				{
					if ( pInventoryImageData->m_pCameraAngles )
					{
						m_pItemPreviewRenderer->SetCameraOrientOverride( *( pInventoryImageData->m_pCameraAngles ) );
						m_pItemPreviewRenderer->SetCameraOrientOverrideEnabled( true );
						m_pItemPreviewRenderer->SetInventoryCameraOrient( *( pInventoryImageData->m_pCameraAngles ) );
					}
					if ( pInventoryImageData->m_pCameraOffset )
					{
						m_pItemPreviewRenderer->SetCameraPositionOverride( *( pInventoryImageData->m_pCameraOffset ) );
						m_pItemPreviewRenderer->SetCameraPositionOverrideEnabled( true );
						m_pItemPreviewRenderer->SetInventoryCameraPosition( *( pInventoryImageData->m_pCameraOffset ) );
					}

					vecCameraOffset = *( pInventoryImageData->m_pCameraOffset );
					angCameraOffset = *( pInventoryImageData->m_pCameraAngles );
				}

				if ( bSpray )
				{
					// TODO? - how do we want to render sprays/stickers, etc in 3D on mouseover in the inventory page?
// 					m_pItemPreviewRenderer->SetInventoryCameraPosition( sVecSprayOffset );
// 					m_pItemPreviewRenderer->SetInventoryCameraOrient( sAngSprayOffset );
// 					m_pItemPreviewRenderer->SetCameraFOV( 30.0f );
				}
				else
				{

					float flRadius;
					Vector vecCenter;

					pScene->GetBoundingSphere( vecCenter, flRadius );

					// clamp to a reasonable range
					flRadius = clamp( flRadius, 6.0f, 20.0f );

					// since tan( fov/2 ) = f/d
					// cos( fov/2 ) = r / r' where r = sphere radius, r' = perp distance from sphere center to max extent of camera
					// d/f = r'/d' where d' is distance of camera to sphere
					// d' = r' / tan( fov/2 ) * r' = r / ( cos (fov/2) * tan( fov/2 ) ) = r / sin( fov/2 )
					float flFOVx = m_pItemPreviewRenderer->GetCameraFOV() * M_PI / 360.0f;
					//Vector vecCameraOffset = *( pInventoryImageData->m_pCameraOffset );
					vecCameraOffset.x -= ( flRadius / sin( flFOVx ) );

					// now setup the camera's origin and angles
					matrix3x4_t matCameraPivot;
					matrix3x4_t offset;
					matrix3x4_t worldToCamera;

					//AngleMatrix( *( pInventoryImageData->m_pCameraAngles ), vecCenter, matCameraPivot );
					AngleMatrix( angCameraOffset, vecCenter, matCameraPivot );
					SetIdentityMatrix( offset );
					MatrixSetColumn( vecCameraOffset, 3, offset );
					ConcatTransforms( matCameraPivot, offset, worldToCamera );

					Vector vecPosition;
					QAngle angles;

					MatrixAngles( worldToCamera, angles, vecPosition );
					m_pItemPreviewRenderer->SetInventoryCameraPosition( vecPosition );
					m_pItemPreviewRenderer->SetInventoryCameraOrient( angles );
				}
			}

			matrix3x4_t camera;
			if ( pScene->GetAttachment( m_pItemPreviewRenderer->GetCameraAttachment() + 1, camera ) )
			{
				Vector vecPosition;
				QAngle angles;

				MatrixPosition( camera, vecPosition );
				MatrixAngles( camera, angles );

				m_pItemPreviewRenderer->SetInventoryCameraPosition( vecPosition );
				m_pItemPreviewRenderer->SetInventoryCameraOrient( angles );
			}

			m_pItemPreviewRenderer->SetSetupRenderStateDelayed( true );

			// lights

			MaterialLightingState_t *pLightingState = m_pItemPreviewRenderer->GetInitialLightingState();
			memset( pLightingState, 0, sizeof( MaterialLightingState_t ) );

			// default for inventory icon
			m_pItemPreviewRenderer->SetLightAmbient( Vector( 0.15f, 0.15f, 0.15f ) );

			//the default light is a blueish rim
			pLightingState->m_pLocalLightDesc[ 0 ].InitDirectional( Vector( -1.0f, 0.3f, 1.0f ), Vector( 1.5f, 1.8f, 2.0f ) );
			pLightingState->m_nLocalLightCount = 1;

			if ( pInventoryImageData )
			{
				//pLightingState->m_nLocalLightCount = 0;
				//int nLightDescIndex = ( pInventoryImageData->m_bOverrideDefaultLight ) ? 0 : m_pRenderToRTData->m_LightingState.m_nLocalLightCount;
				int inventoryImageDataLightCount = 0;
				for ( int i = 0; i < MATERIAL_MAX_LIGHT_COUNT; i++ )
				{
					LightDesc_t *pLightDescSrc = pInventoryImageData->m_pLightDesc[ i ];

					if ( ( !pLightDescSrc ) || ( pLightDescSrc->m_Type == MATERIAL_LIGHT_DISABLE ) )
						continue;

					LightDesc_t *pLightDescDst = &pLightingState->m_pLocalLightDesc[ i ];

					*pLightDescDst = *pLightDescSrc;

					inventoryImageDataLightCount++;
				}

				if ( inventoryImageDataLightCount > 0 )
				{
					pLightingState->m_nLocalLightCount = inventoryImageDataLightCount;
				}
			}

			// disable flashlight for now
			m_pItemPreviewRenderer->EnableRenderingWithFlashlight( NULL );

			// finished adding all lights
			m_pItemPreviewRenderer->SetCurrentFromInitialLightingState();

			// Item rotation bounds and axis order
			// TODO - temp solution
			{
				char const *szWeapon = "y[-60 30] x[-20 20]";
				char const *szGlove = "y[-15 8] x[-5 5]";
				char const *szSpray = "z[-180 180] y[-20 20]";
				Vector2D rot = vec2_origin;
				RotateAxis rotAxis = RotateAxis_Invalid;
				int rotOrder = 0;

				char const *sz;
				if ( bSpray )
				{
					sz = szSpray;
				}
				else if ( bGlove )
				{
					sz = szGlove;
				}
				else
				{
					sz = szWeapon;
				}

				while ( *sz )
				{
					bool bFound = false;

					if ( sz[ 0 ] == 'x' )
					{
						rot = UI_ParseSettingsVector2DFromString( sz + 1 );
						rotAxis = RotateAxis_X;
						bFound = true;
					}
					else if ( sz[ 0 ] == 'y' )
					{
						rot = UI_ParseSettingsVector2DFromString( sz + 1 );
						rotAxis = RotateAxis_Y;
						bFound = true;
					}
					else if ( sz[ 0 ] == 'z' )
					{
						rot = UI_ParseSettingsVector2DFromString( sz + 1 );
						rotAxis = RotateAxis_Z;
						bFound = true;
					}

					if ( bFound )
					{
						m_aRotateAxisSign[ rotOrder ] = 1.0f;

						rot.x = clamp( rot.x, -360.0f, 360.f );
						rot.y = clamp( rot.y, -360.0f, 360.f );
						if ( rot.x > rot.y )
						{
							float flTmp = rot.x;
							rot.x = rot.y;
							rot.y = flTmp;
							m_aRotateAxisSign[ rotOrder ] = -1.0f;
						}

						float fRotScale = 1.0f;

						m_aRotateAxisOrder[ rotOrder ] = rotAxis;
						m_aRotateAxisBounds[ rotOrder ] = rot * fRotScale;

						rotOrder++;
						++sz;
					}

					++sz;
				}
				m_nNumRotateAxes = rotOrder;
			}
		}
	}

	m_pItemPreviewRenderer->SetInitialFlashlightState();

	m_pItemPreviewRenderer->SetCurrentFromInitialLightingState();

	m_flAnimateLightTotalPauseTime = pScene->GetRootMDL()->m_flTime;

	return true;
}

void CUI_ItemPreviewPanel::InitCameraManipulateValues_Manifest()
{
	Vector pos;
	QAngle ang;

	m_pItemPreviewRenderer->GetCameraPivotPositionAndAngles( m_vLookAtDelta, ang );

	pos = *m_pItemPreviewRenderer->GetCameraPositionOverride();
	ang = *m_pItemPreviewRenderer->GetCameraOrientOverride();

	m_flAzimuth = DEG2RAD( ang.y );
	m_flAltitude = DEG2RAD( ang.x );

	pos -= m_vLookAtDelta;
	m_flDistance = pos.Length();

	m_pItemPreviewRenderer->UpdateCameraManipulateTransform( m_flAltitude, m_flAzimuth, m_flDistance, m_vLookAtDelta, true );
}


void CUI_ItemPreviewPanel::InitCameraManipulateValues_Inventory()
{
	float flRadius;
	Vector vecCenter;

	CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;

	if ( !pScene )
		return;

	// set initial values from current camera
 	QAngle ang;
 
 	ang = *m_pItemPreviewRenderer->GetCameraOrientOverride();

	m_flAzimuth = DEG2RAD( ang.y );
	m_flAltitude = DEG2RAD( ang.x );

	pScene->GetBoundingSphere( vecCenter, flRadius );

	m_flDistance = flRadius;
	m_flLookAtDeltaX = 0.0f;
	m_flLookAtDeltaY = 0.0f;
	m_flLookAtDeltaZ = 0.0f;
	m_vLookAtDelta = vec3_origin;
}

bool CUI_ItemPreviewPanel::SetScene( const UIItemInfo_t &info, bool bReInit )
{
	VPROF_BUDGET_THREAD( "CUI_ItemPreviewPanel::SetScene", VPROF_BUDGETGROUP_TENFOOT );

	*static_cast<UIItemInfo_t*>(&m_info) = *static_cast<const UIItemInfo_t*>(&info);

	extern float g_flEconInspectPreviewTime;
	g_flEconInspectPreviewTime = gpGlobals->curtime;

	// re-init if necessary
	if ( bReInit )
	{
		Init();
	}

	SetRotationFlags( info.m_bRotate ? SCENE_PANEL_ROTATION_MOUSE : SCENE_PANEL_ROTATION_NONE );
	SetAntialias( info.m_bAntiAlias );

	bool bInventory = info.m_bInventory;

	bool bPreviewingSingleSticker = false;
	const char *pItemName = info.m_itemName;

	CEconItemView *pEquippedItem = NULL;
	if ( !V_isempty ( pItemName ) )
	{
		pEquippedItem = GetEconItem( pItemName, &bPreviewingSingleSticker, TEAM_TERRORIST, LOADOUT_POSITION_RIFLE1 );
	}

	if ( pEquippedItem && !pEquippedItem->IsValid() )
		return false;

	// decide whether to render 3D to match inventory icon, or just render the pedestal itempreview scene.
	// until we have inventory rendering working for all model classes 
	// currently spray, sticker, music revert to using pedestal scene setup
	if ( pEquippedItem && bInventory )
	{
		// temp lazy class check

		// rendered small, don't force resizing?
		m_econItemTextureSize = COMPOSITE_TEXTURE_SIZE_512;

		const char *pszItem = pEquippedItem->GetPedestalDisplayModel(); // use pedestal model to get model that better describes class for now
																			
		// TEMP - for inventory, use manifest/inspect setup for these items
		if ( pszItem )
		{
			if ( /*V_strstr( pszItem, "glove" ) ||*/
					V_strstr( pszItem, "music" ) ||
					V_strstr( pszItem, "spray" ) ||
					V_strstr( pszItem, "sticker" ) )
			{
				bInventory = false;
			}
		}
		else
		{
			if ( V_strstr( pItemName, "spray" ) ||
					V_strstr( pItemName, "sticker" ) )
			{
				bInventory = false;
			}
		}
	}

	m_bItemPreviewInitialised = false;

	if ( bInventory )
	{
		// settings from econ_item_inventory
		if ( LaunchPanelHelper_Inventory( pEquippedItem ) )
		{
			if ( !ApplyInitialConfiguration_Inventory( pEquippedItem ) )
			{
				return false;
			}
			InitCameraManipulateValues_Inventory();
			m_bItemPreviewInitialised = true;
		}
	}
	else
	{
		// settings from manifest file
		if ( LaunchPanelHelper_Manifest( pEquippedItem, bPreviewingSingleSticker, m_info ) )
		{
			if( !ApplyInitialConfiguration_Manifest ( pEquippedItem, info.m_itemName, false ) )
			{
				return false;
			}
			InitCameraManipulateValues_Manifest();
			m_bItemPreviewInitialised = true;
		}
	}

	EnableRendering( m_info.m_bEnableRendering );

	// by default, last set up panel becomes the active/global one
	SetAsActivePreviewPanel();

	return m_bItemPreviewInitialised;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CUI_ItemPreviewPanel::SetRotationFlags( uint8 unRotationFlags )
{
	if ( m_unRotationFlags == unRotationFlags )
		return;

	m_unRotationFlags = unRotationFlags;

	if ( m_unRotationFlags == 0 )
	{
		// Leave it wherever it happend to be at the time.
		m_itemRotateSpeed = vec3_origin;
		m_itemRotateSpeedTarget = vec3_origin;
	}

	//SetAcceptsInput( (m_unRotationFlags & ~SCENE_PANEL_ROTATION_EVENT_DRIVEN) != 0 );
	SetAcceptsInput( true );
}

void CUI_ItemPreviewPanel::SetAllowSuspendRepaint( bool bAllow )
{
	if ( m_bAllowSuspendRepaint != bAllow )
	{
		m_bAllowSuspendRepaint = bAllow;
		if ( bAllow )
		{
			SetRepaint( k_EPanelRepaintFull );
		}
 	}
}

void CUI_ItemPreviewPanel::SetAntialias( bool bAntialias )
{
	if ( !m_pItemPreviewRenderer )
		return;

	UIPanel()->SetNeedsIntermediateTexture( bAntialias );
}

void CUI_ItemPreviewPanel::SetIsLiveView( bool bEnable )
{
 	if ( m_bIsLiveView != bEnable )
 	{
 		m_bIsLiveView = bEnable;
 		SetRepaint( k_EPanelRepaintFull );
 	}
}

bool CUI_ItemPreviewPanel::SceneReload()
{
	if ( !m_info.m_manifestName.IsEmpty() )
	{
		// if already loaded force reload later by ensuring manifest deleted here
		for ( int nPanelManifest = 0; nPanelManifest < g_kvItemPreviewManifests.Count(); nPanelManifest++ )
		{
			KeyValues *kvPanelManifest = g_kvItemPreviewManifests[ nPanelManifest ];

			const char *pPanelManifestName = kvPanelManifest->GetName();

			if ( !V_stricmp( pPanelManifestName, m_info.m_manifestName ) )
			{
				// delete
				kvPanelManifest->deleteThis();
				g_kvItemPreviewManifests.Remove( nPanelManifest );
				break;
			}
		}
	}

	// un-equip any equipped items (will get re-equipped at next Paint)
	for ( int i = 0; i < m_aEquippedItems.Count(); i++ )
	{
		m_aEquippedItems[ i ].m_bEquipped = false;
	}

	SetScene( m_info, false );

	// ensure light debug panels (sliders, etc) refreshed after reload
	if ( m_pDebug && ( m_pDebug->GetMode () == CUI_ItemPreviewDebug::DEBUGMODE_LIGHTS ) )
	{
		m_pDebug->SelectDebugLight();
	}

	return true;
}

bool CUI_ItemPreviewPanel::CloseDebugCamera()
{
	if ( !m_pItemPreviewRenderer )
		return false;

	m_pItemPreviewRenderer->SetCameraManipulateAllowed( false );

	if ( !m_pDebug )
		return false;

	if ( m_pDebug->BIsVisible() )
	{
		Vector vecPos;
		QAngle angRot;

		if ( m_pItemPreviewRenderer->GetCameraAttachment() >= 0 )
		{
			m_pItemPreviewRenderer->SetCameraPositionOverride( vec3_origin );
			m_pItemPreviewRenderer->SetCameraOrientOverride( vec3_angle );
		}
		else
		{
			m_pItemPreviewRenderer->GetCameraPositionAndAngles( vecPos, angRot );

			m_pItemPreviewRenderer->SetCameraPositionOverride( vecPos );
			m_pItemPreviewRenderer->SetCameraOrientOverride( angRot );
		}

		m_pDebug->SetVisible( false );

		m_pItemPreviewRenderer->EnableDrawShadowFloor( false );
		m_pItemPreviewRenderer->EnableDrawWorldPivot( false );
		m_pItemPreviewRenderer->EnableDrawFlashlightPivot ( false );
		m_pItemPreviewRenderer->EnableDrawPresetCameras( false );
		m_pItemPreviewRenderer->EnableDrawDirectionaLight( -1 );
	}
	return true;
}

bool CUI_ItemPreviewPanel::SelectDebugLight()
{
	if ( !m_pDebug )
		return false;

	if ( !m_pDebug->BIsVisible() )
		return false;

	m_pDebug->SelectDebugLight();

	return true;
}

bool CUI_ItemPreviewPanel::OnSetRotationSpeed( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, float flSpeed )
{
	return true;
}

ConVar panorama_loadout_rotate_scale( "panorama_loadout_rotate_scale", "2.0" );
ConVar panorama_loadout_rotate_drag( "panorama_loadout_rotate_drag", "0.19" );
ConVar panorama_loadout_rotate_grab_scale( "panorama_loadout_rotate_grab_scale", "0.5" );
ConVar panorama_loadout_rotate_frametime_multiplier( "panorama_loadout_rotate_frametime_multiplier", "4.0" );

ConVar panorama_camera_rotate_azimuth_scale( "panorama_camera_rotate_azimuth_scale", "0.004" );
ConVar panorama_camera_rotate_altitude_scale( "panorama_camera_rotate_altitude_scale", "0.004" );

ConVar panorama_light_rotate_azimuth_scale( "panorama_light_rotate_azimuth_scale", "0.004" );
ConVar panorama_light_rotate_altitude_scale( "panorama_light_rotate_altitude_scale", "0.004" );
ConVar panorama_light_move_scale( "panorama_light_move_scale", "0.1" );

ConVar panorama_camera_inout_scale_kb( "panorama_camera_inout_scale_kb", "1" );
ConVar panorama_camera_inout_scale( "panorama_camera_inout_scale", "0.1" );
ConVar panorama_camera_lookat_scale( "panorama_camera_lookat_scale", "0.1" );

ConVar panorama_light_inout_scale( "panorama_light_inout_scale", "0.5" );

float CUI_ItemPreviewPanel::UpdateItemRotation( int axis, float dt )
{
	if ( m_unRotationFlags != 0 || m_itemRotateSpeedTarget[ axis ] != 0.0f )
	{
		float flAcceleration = (m_itemRotateAcceleration[ axis ] == 0.0f) ? panorama_loadout_rotate_drag.GetFloat() : m_itemRotateAcceleration[ axis ];

		m_itemRotateSpeed[ axis ] = Approach( m_itemRotateSpeedTarget[ axis ], m_itemRotateSpeed[ axis ], flAcceleration );
		
		m_aItemRotate[ axis ][ 1 ] += m_itemRotateSpeed[ axis ] * m_aRotateAxisSign[ axis ];

		if ( !(m_unRotationFlags & SCENE_PANEL_ROTATION_EVENT_DRIVEN) )
		{
			m_aItemRotate[ axis ][ 1 ] = clamp( m_aItemRotate[axis][1], m_aRotateAxisBounds[ axis ].x, m_aRotateAxisBounds[ axis ].y );
		}

		m_aItemRotate[ axis ][ 0 ] = Lerp(
			clamp( dt * panorama_loadout_rotate_frametime_multiplier.GetFloat(), 0.0f, 1.0f ),
			m_aItemRotate[ axis ][ 0 ],
			m_aItemRotate[ axis ][ 1 ]
			);

		if ( !(m_unRotationFlags & SCENE_PANEL_ROTATION_EVENT_DRIVEN) )
		{
			m_aItemRotate[ axis ][ 0 ] = clamp( m_aItemRotate[ axis ][ 0 ], m_aRotateAxisBounds[ axis ].x, m_aRotateAxisBounds[ axis ].y );
		}
	}

	return m_aItemRotate[ axis ][ 0 ];
}

void CUI_ItemPreviewPanel::ResetItemRotation()
{
	m_nNumRotateAxes = 0;
	m_itemRotateSpeed = vec3_origin;
	m_aRotateAxisBounds[ RotateAxis_X ] = vec2_origin;
	m_aRotateAxisBounds[ RotateAxis_Y ] = vec2_origin;
	m_aRotateAxisBounds[ RotateAxis_Z ] = vec2_origin;
	m_aRotateAxisOrder[ RotateAxis_X ] = RotateAxis_Invalid;
	m_aRotateAxisOrder[ RotateAxis_Y ] = RotateAxis_Invalid;
	m_aRotateAxisOrder[ RotateAxis_Z ] = RotateAxis_Invalid;
	m_aRotateAxisSign[ RotateAxis_X ] = 1.0f;
	m_aRotateAxisSign[ RotateAxis_Y ] = 1.0f;
	m_aRotateAxisSign[ RotateAxis_Z ] = 1.0f;
	m_itemInitAngles = vec3_angle;

	m_aItemRotate[ 0 ] = m_aItemRotate[ 1 ] = vec3_origin;
	if ( (m_unRotationFlags & SCENE_PANEL_ROTATION_EVENT_DRIVEN) == 0 )
	{
		m_itemRotateSpeed = m_itemRotateSpeedTarget = vec3_origin;
	}
	m_itemRotateAcceleration = vec3_origin;
}

//-----------------------------------------------------------------------------
// vecPF.x - pulse frequecy * 2 PI
// vecPF.y - pulse amount
// vecPF.z - noise frequency * 2 PI
// vecPF.w - noise amount
//-----------------------------------------------------------------------------
float LightPulseFlickerAmount( Vector4D vecPF, float flTime )
{
	// pulse
	float flPulse = vecPF.y * cosf( flTime * vecPF.x );

	// flicker
	QAngle ang = vec3_angle;
	matrix3x4_t mat;

	ang.x = AngleNormalizePositive( flTime * vecPF.z );
	ang.y = AngleNormalizePositive( flTime * vecPF.z * 0.3f ); 

	mat.InitFromQAngles( ang );

//	float flNoise = vecPF.w * ( Turbulence( mat.GetForward(), (int)( vecPF.w * 3.0f ) + 1 ) * 2.0f - 1.0f );
	float flNoise = vecPF.w * ImprovedPerlinNoise( mat.GetForward() );

	return 1.0f + flPulse + flNoise;
}

void CUI_ItemPreviewPanel::UpdateLights( float flTime )
{
	float flAmount;
	QAngle ang;

	flTime -= m_flAnimateLightTotalPauseTime;

	CUI_ItemPreviewDebug::eDebugLight eSelectedLight = CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_NONE;

	if ( m_pDebug && 
		 ( m_pDebug->GetMode() == CUI_ItemPreviewDebug::DEBUGMODE_LIGHTS ) && 
		 ( !m_pDebug->BEnabledAllLights() ) )
	{
		// disable unselected lights

		// selected light
		eSelectedLight = m_pDebug->GetActiveDebugLight();
	}

	// initialise render state for lights to initial state before (potentially) animating
	m_pItemPreviewRenderer->SetCurrentFromInitialLightingState();

	// flashlight - rotate pos/dir
	if ( m_bEnableAnimatingLights && ( m_flashlightInfo.m_rotateSpeed != vec3_angle ) )
	{
		ang = flTime * m_flashlightInfo.m_rotateSpeed;
		ang.NormalizePositive();
		m_pItemPreviewRenderer->RotateFlashlight( ang );
	}

	// flashlight - intensity, pulse and flicker
	flAmount = 1.0f;
	if ( m_bEnableAnimatingLights && ( m_flashlightInfo.m_pulseFlicker != vec4_origin ) )
	{
		flAmount = LightPulseFlickerAmount( m_flashlightInfo.m_pulseFlicker, flTime );
	}
	if ( ( eSelectedLight != CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_NONE ) &&
		( eSelectedLight != CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_FLASHLIGHT ) )
	{
		flAmount = 0.0f;
	}
	SetFlashlightAmount( flAmount );

	// directional lights
	for ( int idx = 0; idx < m_pItemPreviewRenderer->GetLocalLightCount(); idx++ )
	{
		sLightInfo *pLightInfo = &m_aDirLightInfo[ idx ];

		// rotate dir
		if ( m_bEnableAnimatingLights && ( pLightInfo->m_rotateSpeed != vec3_angle ) )
		{
			ang = flTime * pLightInfo->m_rotateSpeed;
			ang.NormalizePositive();
			m_pItemPreviewRenderer->RotateDirectionalLight( idx, ang );
		}

		// intensity
		flAmount = 1.0f;
		if ( m_bEnableAnimatingLights && ( pLightInfo->m_pulseFlicker != vec4_origin ) )
		{
			flAmount = LightPulseFlickerAmount( pLightInfo->m_pulseFlicker, flTime );
		}

		if ( ( eSelectedLight != CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_NONE ) &&
			 ( eSelectedLight != ( CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_DIR0 + idx ) ) )
		{
			flAmount = 0.0f;
		}
		m_pItemPreviewRenderer->AnimateDirectionalLightAmount( idx, flAmount );
	}

	// leave ambient light amount alone for now
	flAmount = 1.0f;
	if ( ( eSelectedLight != CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_NONE ) &&
		( eSelectedLight != ( CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_AMBIENT ) ) )
	{
		flAmount = 0.0f;
	}
	m_pItemPreviewRenderer->AnimateAmbientLightAmount( flAmount );

}

void CUI_ItemPreviewPanel::ResetLightAnimation()
{
	m_flashlightInfo.m_rotateSpeed = vec3_angle;
	m_flashlightInfo.m_pulseFlicker = vec4_origin;

	for ( int idx = 0; idx < MAX_PANEL_LIGHTS; idx++ )
	{
		sLightInfo *pLightInfo = &m_aDirLightInfo[idx];

		pLightInfo->m_rotateSpeed = vec3_angle;
		pLightInfo->m_pulseFlicker = vec4_origin;
	}

	m_nDirectionalLightModify = 0;
	m_bEnableAnimatingLights = true;

	m_flAnimateLightTotalPauseTime = 0;
	m_flAnimateLightStartPauseTime = 0;
}

void CUI_ItemPreviewPanel::ToggleAnimatingLights()
{
	if ( !m_pItemPreviewRenderer )
		return;

	m_bEnableAnimatingLights = !m_bEnableAnimatingLights;

	CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;
	if ( !m_bEnableAnimatingLights )
	{
		m_flAnimateLightStartPauseTime = pScene->GetRootMDL()->m_flTime;
	}
	else
	{
		m_flAnimateLightTotalPauseTime += pScene->GetRootMDL()->m_flTime - m_flAnimateLightStartPauseTime;
	}
}

void CUI_ItemPreviewPanel::ReCentreFlashlightOnCamera()
{
	if ( !m_pItemPreviewRenderer )
		return;

	Vector pos;
	QAngle ang;

	m_pItemPreviewRenderer->GetCameraPositionAndAngles( pos, ang );

	m_pItemPreviewRenderer->SetRenderCaptureCameraPositionOverride( pos );
	m_pItemPreviewRenderer->SetRenderCaptureCameraOrientOverride( ang );
}


bool CUI_ItemPreviewPanel::OnGamePadDown( const panorama::GamePadData_t &code )
{
	return false;
}

bool CUI_ItemPreviewPanel::OnKeyTyped( const panorama::KeyData_t &unichar )
{
	if ( unichar.m_UniChar == L' ' )
	{
 	}

	return false;
}

bool CUI_ItemPreviewPanel::OnKeyDown( const panorama::KeyData_t &unichar )
{
	if ( !m_pDebug )
		return false; // not possible?

	if ( !m_pItemPreviewRenderer )
		return false;

	if ( !m_pDebug->BIsVisible() )
	{
		if ( unichar.m_bFirstDown )
		{
			if ( unichar.m_KeyCode == panorama::KEY_SPACE )
			{
				m_pDebug->SetVisible( false );
			}
		}

		if ( unichar.m_KeyCode == panorama::KEY_PAGEDOWN )
		{
			m_pItemPreviewRenderer->PrevCameraPreset();
			CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;
			if ( pScene )
				m_flCameraPresetStartBlendTime = pScene->GetRootMDL()->m_flTime;
		}
		else if ( unichar.m_KeyCode == panorama::KEY_PAGEUP )
		{
			m_pItemPreviewRenderer->NextCameraPreset();
			CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;
			if ( pScene )
				m_flCameraPresetStartBlendTime = pScene->GetRootMDL()->m_flTime;
		}
		else if ( unichar.m_KeyCode == panorama::KEY_HOME )
		{
			m_pItemPreviewRenderer->ResetCameraPreset();
		}
	}
	else
	{
		if ( unichar.m_KeyCode == panorama::KEY_SPACE )
		{
			m_pDebug->SetVisible( false );
		}
	}

	if ( unichar.m_bFirstDown )
	{
		if ( ( unichar.m_KeyCode == panorama::KEY_LCONTROL ) || ( unichar.m_KeyCode == panorama::KEY_RCONTROL ) )
		{
			m_bCtrlDown = true;
		}
		else if ( m_bCtrlDown && ( unichar.m_KeyCode == panorama::KEY_C ) )
		{
			if ( m_pDebug->GetMode() == CUI_ItemPreviewDebug::DEBUGMODE_CAMERA )
			{
				// copy camera debug settings to clipboard to allow pasting into manifest
				CopyDebugCameraSettingsToClipboard();
			}
			else
			{
				// copy all light settings to clipboard
				CopyAllLightSettingsToClipboard();
			}
		}
		else if ( unichar.m_KeyCode == panorama::KEY_P )
		{
			// copy camera as manifest 'preset' 
			CopyPresetCameraSettingsToClipboard();
		}
		else if ( ( unichar.m_KeyCode == panorama::KEY_A ) && ( m_pDebug->GetMode() == CUI_ItemPreviewDebug::DEBUGMODE_LIGHTS ) )
		{
			m_pDebug->ToggleEnabledLights();
		}
		else if ( ( unichar.m_KeyCode == panorama::KEY_C ) && ( m_pDebug->GetMode() == CUI_ItemPreviewDebug::DEBUGMODE_LIGHTS ) )
		{
			ReCentreFlashlightOnCamera();
		}
		else if ( ( unichar.m_KeyCode == panorama::KEY_X ) && ( m_pDebug->GetMode() == CUI_ItemPreviewDebug::DEBUGMODE_LIGHTS ) )
		{
			m_pItemPreviewRenderer->ToggleDebugHelperRendering();
		}
		else if ( ( unichar.m_KeyCode == panorama::KEY_S ) && ( m_pDebug->GetMode() == CUI_ItemPreviewDebug::DEBUGMODE_LIGHTS ) )
		{
			ToggleAnimatingLights();
		}
	}

	return false;
}

bool CUI_ItemPreviewPanel::OnKeyUp( const panorama::KeyData_t &unichar )
{
	if ( ( unichar.m_KeyCode == panorama::KEY_LCONTROL ) || ( unichar.m_KeyCode == panorama::KEY_RCONTROL ) )
	{
		m_bCtrlDown = false;
	}

	return false;
}

bool CUI_ItemPreviewPanel::OnMouseButtonDown( const panorama::MouseData_t &code )
{
 	if ( !( m_unRotationFlags & SCENE_PANEL_ROTATION_MOUSE ) || 
		( (code.m_MouseCode != panorama::MOUSE_LEFT) && (code.m_MouseCode != panorama::MOUSE_RIGHT) && (code.m_MouseCode != panorama::MOUSE_MIDDLE) ) )
 		return BaseClass::OnMouseButtonDown( code );

	if ( !m_pDebug )
		return false; // not possible

	if ( m_pDebug->BIsVisible() )
	{
		if ( ( !m_bCtrlDown ) || ( m_pDebug->GetMode() == CUI_ItemPreviewDebug::DEBUGMODE_CAMERA ) )
		{
			if ( code.m_MouseCode == panorama::MOUSE_LEFT )
			{
				m_eMouseDragContext = MOUSEDRAG_CAMERA_ORIENT;
			}
			else if ( code.m_MouseCode == panorama::MOUSE_RIGHT )
			{
				m_eMouseDragContext = MOUSEDRAG_CAMERA_DISTANCE;
			}
			else if ( code.m_MouseCode == panorama::MOUSE_MIDDLE )
			{
				m_eMouseDragContext = MOUSEDRAG_CAMERA_PIVOT;
			}
		}
		else
		{
			if ( code.m_MouseCode == panorama::MOUSE_LEFT )
			{
				m_eMouseDragContext = MOUSEDRAG_LIGHT_ORIENT;
			}
			else if ( code.m_MouseCode == panorama::MOUSE_RIGHT )
			{
				m_eMouseDragContext = MOUSEDRAG_LIGHT_DISTANCE;
			}
			else if ( code.m_MouseCode == panorama::MOUSE_MIDDLE )
			{
				m_eMouseDragContext = MOUSEDRAG_LIGHT_PIVOT;
			}
		}
	}
	else if ( code.m_MouseCode == panorama::MOUSE_LEFT )
	{
		if ( m_sSound.IsEmpty() == false && (this->m_unRotationFlags&SCENE_PANEL_ROTATION_MOUSE) )
		{

			CUtlString sound;

			if ( g_pActivePreviewPanel != nullptr )
			{
				CEconItemView const *pItemView = g_pActivePreviewPanel->GetWeaponPreviewItemData();
				if ( pItemView != nullptr )
				{	
					sound = pItemView->GetItemDefinition()->GetRawDefinition()->GetString( "item_slot" );
				}
			}

			GameUI().PlayUISoundScript( ( sound + "_startRotate" ).String() );
		}

		m_bFMActive = false;

		m_eMouseDragContext = MOUSEDRAG_ITEM;
	}

	m_bMouseDragStart = true;

	return BaseClass::OnMouseButtonDown( code );
}

bool CUI_ItemPreviewPanel::OnMouseButtonUp( const panorama::MouseData_t &code )
{
	if ( !( m_unRotationFlags & SCENE_PANEL_ROTATION_MOUSE ) ||
		( ( code.m_MouseCode != panorama::MOUSE_LEFT ) && ( code.m_MouseCode != panorama::MOUSE_RIGHT ) && ( code.m_MouseCode != panorama::MOUSE_MIDDLE ) ) )
 		return BaseClass::OnMouseButtonUp( code );

	if ( !m_pDebug )
		return false;

	m_eMouseDragContext = MOUSEDRAG_NONE;

	if ( m_pDebug->BIsVisible() )
	{
		return false;
	}

	return BaseClass::OnMouseButtonUp( code );
}

bool CUI_ItemPreviewPanel::OnMouseWheel( const panorama::MouseData_t &code )
{
 	if ( !( m_unRotationFlags & SCENE_PANEL_ROTATION_MOUSEWHEEL ) )
 		return BaseClass::OnMouseWheel( code );

	return true;
}

void CUI_ItemPreviewPanel::OnMouseMove( float flMouseX, float flMouseY )
{
	if ( m_bMouseDragStart )
	{
		m_flLastMouseX = flMouseX;
		m_flLastMouseY = flMouseY;
		m_bMouseDragStart = false;
		return;
	}

	float flDeltaX = flMouseX - m_flLastMouseX;
	float flDeltaY = flMouseY - m_flLastMouseY;
	m_flLastMouseX = flMouseX;
	m_flLastMouseY = flMouseY;

	switch ( m_eMouseDragContext )
	{
	case MOUSEDRAG_ITEM:
		m_itemRotateSpeed = vec3_origin;
		m_aItemRotate[ RotateAxis_X ][ 1 ] += m_aRotateAxisSign[ RotateAxis_X ] * flDeltaX * panorama_loadout_rotate_grab_scale.GetFloat();
		m_aItemRotate[ RotateAxis_Y ][ 1 ] += m_aRotateAxisSign[ RotateAxis_Y ] * flDeltaY * panorama_loadout_rotate_grab_scale.GetFloat();
		break;
	case MOUSEDRAG_CAMERA_ORIENT:
		m_flAzimuth += panorama_camera_rotate_azimuth_scale.GetFloat() * flDeltaX;
		m_flAltitude -= panorama_camera_rotate_altitude_scale.GetFloat() * flDeltaY;
		m_flAltitude = MAX( -M_PI / 2, MIN( M_PI / 2, m_flAltitude ) );
		break;
	case MOUSEDRAG_CAMERA_DISTANCE:
		m_flDistance += panorama_camera_inout_scale.GetFloat() * flDeltaY;
		break;
	case MOUSEDRAG_CAMERA_PIVOT:
		{
			matrix3x4_t cameraPivot;
			QAngle angles( RAD2DEG( m_flAltitude ), RAD2DEG( m_flAzimuth ), 0.0f );
			AngleMatrix( angles, vec3_origin, cameraPivot );

			Vector vF, vL, vU;
			cameraPivot.GetBasisVectorsFLU( &vF, &vL, &vU );

			m_vLookAtDelta += panorama_camera_lookat_scale.GetFloat() * flDeltaX * vL;
			m_vLookAtDelta += panorama_camera_lookat_scale.GetFloat() * flDeltaY * vU;

			m_flLookAtDeltaX += panorama_camera_lookat_scale.GetFloat() * flDeltaX;
			m_flLookAtDeltaY += panorama_camera_lookat_scale.GetFloat() * flDeltaY;
		}
		break;
	case MOUSEDRAG_LIGHT_ORIENT:
		if ( m_pDebug )
		{
			// only directional lights currently supported

			switch ( m_pDebug->GetActiveDebugLight() )
			{
			case CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_FLASHLIGHT:
				{
					// get pos, angles
					QAngle ang;
					Vector pos;
					GetFlashlightAngle( ang );
					GetFlashlightPosition( pos );

					matrix3x4_t rotMat, origMat, dstMat;
					AngleMatrix( ang, pos, origMat );

					// fake flashlight pivot in order to pick a sensible point to pivot around
					// choose point closest to scene pivot along direction and from position of flashlight
 					Vector pivot, camPivot;
 					m_pItemPreviewRenderer->GetCameraPivotPosition( camPivot );
					m_pItemPreviewRenderer->GetFlashlightPivotPosition( pivot );
					float flDist = origMat.GetForward().Dot( pivot - pos );

					// update angles
					float azimuth = DEG2RAD( ang.y );
					azimuth += panorama_light_rotate_azimuth_scale.GetFloat() * flDeltaX;
					float altitude = DEG2RAD( ang.x );
					altitude -= panorama_light_rotate_altitude_scale.GetFloat() * flDeltaY;
					altitude = MAX( -M_PI / 2, MIN( M_PI / 2, altitude ) );

					ang.x = RAD2DEG( altitude );
					ang.y = RAD2DEG( azimuth );
					ang.z = 0.0f;

					AngleMatrix( ang, pivot, rotMat );

					Vector vOffset = vec3_origin;
					vOffset.x = -flDist;

					matrix3x4_t matOffset;
					SetIdentityMatrix( matOffset );
					MatrixSetColumn( vOffset, 3, matOffset );

					ConcatTransforms( rotMat, matOffset, dstMat );

					MatrixAngles( dstMat, ang, pos );
					SetFlashlightAngle( ang.x, ang.y, ang.z );
					SetFlashlightPosition( pos.x, pos.y, pos.z );

					// disabling this on moving flashlight for now - do it implicitly by setting z dist sliders to znear >= zfar
					//SetFlashlightNearFarZFromSceneBounds();

					m_pItemPreviewRenderer->SetInitialFlashlightState();
				}
				break;
			case CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_DIR0:
			case CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_DIR1:
			case CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_DIR2:
			case CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_DIR3:
				{
					Vector dir;

					int idx = (int)( m_pDebug->GetActiveDebugLight() - CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_DIR0 );

					GetDirectionalLightDirection( idx, dir );

					matrix3x4_t dstMat;
					QAngle ang;

					VectorAngles( dir, ang );

					// update angles
					float azimuth = DEG2RAD( ang.y );
					azimuth += panorama_light_rotate_altitude_scale.GetFloat() * flDeltaX;
					float altitude = DEG2RAD( ang.x );
					altitude -= panorama_light_rotate_altitude_scale.GetFloat() * flDeltaY;

					// new ang
					ang.x = RAD2DEG( altitude );
					ang.y = RAD2DEG( azimuth );
					ang.z = 0.0f;

					AngleMatrix( ang, dstMat );

					dir = dstMat.GetForward();
					SetDirectionalLightModify( idx );
					SetDirectionalLightDirection( dir.x, dir.y, dir.z );
				}
				break;
			case CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_AMBIENT:
			default:
				break;

			}
		}
		break;
	case MOUSEDRAG_LIGHT_PIVOT:
		if ( m_pDebug && ( m_pDebug->GetActiveDebugLight() == CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_FLASHLIGHT ) )
		{
			matrix3x4_t cameraPivot;
			QAngle angles ( RAD2DEG ( m_flAltitude ), RAD2DEG ( m_flAzimuth ), 0.0f );
			AngleMatrix ( angles, vec3_origin, cameraPivot );

			Vector vF, vL, vU;
			cameraPivot.GetBasisVectorsFLU ( &vF, &vL, &vU );

			Vector vDelta = -panorama_light_move_scale.GetFloat() * flDeltaX * vL;
			vDelta -= panorama_light_move_scale.GetFloat() * flDeltaY * vU;

			m_flashlightInfo.m_position += vDelta;

			SetFlashlightPosition( m_flashlightInfo.m_position.x, m_flashlightInfo.m_position.y, m_flashlightInfo.m_position.z );

			Vector pivot;
			m_pItemPreviewRenderer->GetFlashlightPivotPosition( pivot );
			pivot += vDelta;
			m_pItemPreviewRenderer->SetFlashlightPivotPosition( pivot );

			m_pItemPreviewRenderer->SetInitialFlashlightState();
		}
		break;
	case MOUSEDRAG_LIGHT_DISTANCE:
		if ( m_pDebug && ( m_pDebug->GetActiveDebugLight() == CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_FLASHLIGHT ) )
		{
			// only valid for flashlight
			// get pos, angles
			QAngle ang;
			Vector pos;
			GetFlashlightAngle( ang );
			GetFlashlightPosition( pos );

			matrix3x4_t rotMat, origMat, dstMat;
			AngleMatrix( ang, pos, origMat );

			// update pos
			Vector dir = origMat.GetForward();

			pos += ( panorama_light_inout_scale.GetFloat() * flDeltaY ) * dir;

			SetFlashlightPosition( pos.x, pos.y, pos.z );
			m_pItemPreviewRenderer->SetInitialFlashlightState();
		}
		break;
	default:
		break;
	}
}

void CUI_ItemPreviewPanel::GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties )
{
	BaseClass::GetDebugPropertyInfo( pvecProperties );

	if ( !m_info.m_manifestName.IsEmpty() )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t();
		pProperty->m_strName = "manifest";
		pProperty->m_strValue = m_info.m_manifestName;
		pvecProperties->AddToTail( pProperty );
	}

	if ( !m_info.m_itemName.IsEmpty() )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t();
		pProperty->m_strName = "item";
		pProperty->m_strValue = m_info.m_itemName;
		pvecProperties->AddToTail( pProperty );
	}
}

bool CUI_ItemPreviewPanel::OnReadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	if ( m_pItemPreviewRenderer )
		m_pItemPreviewRenderer->SetNeedsRedraw( true );

	return true;
}

bool CUI_ItemPreviewPanel::OnUnreadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	if ( m_pItemPreviewRenderer )
		m_pItemPreviewRenderer->SetNeedsRedraw( true );

	// Not displaying the panel anymore, release the panel render target. It will
	// be recreated at the next Paint.
	m_pPanelRT.SafeRelease();

	return true;
}


bool Panorama_HelperWeaponPreviewGetStatTrakScore( void *, int *puiScore )
{
	if ( !g_pActivePreviewPanel )
		return false;

	CEconItemView const *pItemView = g_pActivePreviewPanel->GetWeaponPreviewItemData();
	if ( !pItemView )
		return false;

	int nKillEater = 0;
	for ( int i = 0; i < GetKillEaterAttrPairCount(); i++ )
	{
		const CEconItemAttributeDefinition *pKillEaterAltAttrDef = GetKillEaterAttrPair_Score( i );
		if ( pKillEaterAltAttrDef )
		{
			// To get live attribute changes we need to get them from the SOCache because the instance of EconItemView won't have them until we spawn a new copy.
			attrib_value_t nKillEaterAltScore = 0;
			if ( pItemView->FindAttribute( pKillEaterAltAttrDef, &nKillEaterAltScore ) )
			{
				// The fallback might be larger
				nKillEater = MAX( nKillEater, static_cast<int>( nKillEaterAltScore ) );
			}
			break;
		}
	}

	if ( puiScore )
		*puiScore = nKillEater;
	return true;
}

bool Panorama_HelperWeaponPreviewGetLabel ( void *, const char **p_szLabel )
{
	if ( !g_pActivePreviewPanel )
		return false;

	CEconItemView const *pItemView = g_pActivePreviewPanel->GetWeaponPreviewItemData();
	if ( !pItemView )
		return false;

	if ( p_szLabel && pItemView->GetCustomName() )
		*p_szLabel = pItemView->GetCustomName();

	return true;
}


// TODO
// CEconItemView * Helper_CreateReferenceEconItemWithPreviewDataBlock( CEconItemPreviewDataBlock const &protoData )
// {
// }

//-----------------------------------------------------------------------------
//
// IShaderDeviceDependentObject methods
//
//-----------------------------------------------------------------------------
void CUI_ItemPreviewPanel::DeviceLost( void )
{
	if ( m_pItemPreviewRenderer )
		m_pItemPreviewRenderer->SetNeedsRedraw( true );
}

void CUI_ItemPreviewPanel::DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd )
{
	if ( m_pItemPreviewRenderer )
		m_pItemPreviewRenderer->SetNeedsRedraw( true );
}

void CUI_ItemPreviewPanel::ScreenSizeChanged( int width, int height )
{
	if ( m_pItemPreviewRenderer )
		m_pItemPreviewRenderer->SetNeedsRedraw( true );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

bool CUI_ItemPreviewPanel::SetScene( const char *szManifest, const char *szItem, bool bInventory )
{
	UIItemInfo_t info;

	info = m_info;

	info.m_manifestName = szManifest;
	info.m_itemName = szItem;

	info.m_bInventory = bInventory;

	info.m_bAntiAlias = true;
	info.m_bRotate = true;

	return ( SetScene( info ) );
}

bool CUI_ItemPreviewPanel::SetPlayerModel( const char *szPlayer )
{
	if ( !m_bIsPlayerPanel )
		return true;

	m_info.m_itemName = szPlayer;

	InitSceneModels( NULL, szPlayer );
	return true;
}

//-----------------------------------------------------------------------------
//
// Either szItem is valid (equipping with a specific item), or szTeam and szPos are valid (equipping from loadout)
//
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewPanel::EquipPlayer( const char *szItem, const char *szTeam, const char *szPos )
{
	if ( !m_pItemPreviewRenderer )
		return true;

	if ( !m_bIsPlayerPanel )
		return true;

	CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;

	if ( !pScene )
	{
		DevWarning( "CUI_ItemPreviewPanel::EquipPlayer - no scene item!!\n" );
		return true;
	}

	CEconItemView *pExtraEconItem = NULL;

	int nIndex = m_aEquippedItems.AddToTail();
	if ( !m_aEquippedItems.IsValidIndex( nIndex ) )
	{
		return true;
	}

	if ( szItem )
	{
		// equipping player with item
		m_aEquippedItems[ nIndex ].m_sName = szItem;
		m_aEquippedItems[ nIndex ].m_nTeam = -1;
		m_aEquippedItems[ nIndex ].m_nPos = -1;
	}
// 	else
// 	{
// 		Assert( szTeam && szPos );
// 
// 		// equipping player from loadout
// 		m_aEquippedItems[ nIndex ].m_sName.Clear();
// 		m_aEquippedItems[ nIndex ].m_nTeam = GetTeamFromString( szTeam );
// 		m_aEquippedItems[ nIndex ].m_nPos = GetLoadoutSubPositionAsInt( szPos );
// 	}

	pExtraEconItem = GetEconItem( szItem, NULL, m_aEquippedItems[ nIndex ].m_nTeam, m_aEquippedItems[ nIndex ].m_nPos );

	int nMergeIndex = -1;
	m_aEquippedItems[ nIndex ].m_bEquipped = pScene->MergeEconItem( nMergeIndex, szItem, pExtraEconItem, "equip_player" );

	// TODO
	if ( pScene->m_aMergedItemData.IsValidIndex( nMergeIndex ) )
	{
		ApplyStickersToItemWorldModel( pScene, pExtraEconItem, pScene->m_aMergedItemData[ nMergeIndex ].m_hMDL );
	}

	return true;
}

bool CUI_ItemPreviewPanel::EquipPlayerFromLoadout( const char *szTeam, const char *szPos )
{
	return EquipPlayer( NULL, szTeam, szPos );
}

bool CUI_ItemPreviewPanel::EquipPlayerWithItem( const char *szItem )
{
	return EquipPlayer( szItem, NULL, NULL );
}


//-----------------------------------------------------------------------------
// merge stickers
//-----------------------------------------------------------------------------
void CUI_ItemPreviewPanel::MergeStickersToItem( CEconItemView *pEquippedItem, CUI_SceneItem *pScene )
{
	if ( !( pEquippedItem && pScene ) )
		return;

	pEquippedItem->GenerateStickerMaterials();

	if ( pEquippedItem->ItemHasAnyStickersApplied() )
	{
		for ( int i = 0; i < pEquippedItem->GetNumSupportedStickerSlots(); i++ )
		{
			if ( pEquippedItem->GetStickerIMaterialBySlotIndex( i ) != NULL )
			{
				int iStickerMDLIndex = pScene->AddMergeMDL( pEquippedItem->GetStickerSlotModelBySlotIndex( i ), NULL, NULL, NULL, false, m_econItemTextureSize );
				MDLHandle_t hStickerMDL = pScene->m_aMergedItemData[ iStickerMDLIndex ].m_hMDL;

				if ( CMDL *pStickerMdlCMDL = ( hStickerMDL != MDLHANDLE_INVALID ) ? pScene->GetMergeMDL( hStickerMDL ) : NULL )
				{
					pStickerMdlCMDL->SetSimpleMaterialOverride( pEquippedItem->GetStickerIMaterialBySlotIndex( i ) );
				}
			}
		}
	}
}

const char* const szStickerAttachmentLookupTable[] = {
	"sticker_a",
	"sticker_b",
	"sticker_c",
	"sticker_d",
	"sticker_e"
};

void CUI_ItemPreviewPanel::ApplyStickersToItemWorldModel( CUI_SceneItem *pScene, CEconItemView *pEquippedItem, MDLHandle_t hMDL )
{
	if ( !( pEquippedItem && pScene ) )
		return;

	if ( hMDL == MDLHANDLE_INVALID )
		return;

	pEquippedItem->GenerateStickerMaterials();

	if ( pEquippedItem->ItemHasAnyStickersApplied() )
	{
		//
		// cf C_BaseCombatWeapon::ApplyThirdPersonStickers
		//

		MDLCACHE_CRITICAL_SECTION();

		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

		CMDL *pCMDL = pScene->GetMergeMDL( hMDL );

		studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( hMDL );
		CStudioHdr studioHdr( pStudioHdr, g_pMDLCache );
		studiohwdata_t *pStudioHWData = g_pMDLCache->GetHardwareData( hMDL );

		// init/assign decals to CMDL
		StudioDecalHandle_t decalHandle = g_pStudioRender->CreateDecalList( pStudioHWData );
		pCMDL->m_DecalHandle = decalHandle;

		int nBoneCount = pStudioHdr->numbones;
		CMatRenderData< matrix3x4a_t > rdBoneToWorld( pRenderContext, nBoneCount );

		matrix3x4_t mat;
		mat.SetToIdentity();
		pCMDL->SetUpBones( mat, nBoneCount, rdBoneToWorld.Base() );

		matrix3x4_t weaponBoneTransform;
		Vector vecWorldRayOrigin, vecWorldRayDirection;

		bool bWeaponProvidedStickerAttachments = false;

		for ( int i = 0; i < pEquippedItem->GetNumSupportedStickerSlots(); i++ )
		{
			IMaterial *pStickerMaterialThirdPerson = pEquippedItem->GetStickerIMaterialBySlotIndex( i, true );

			if ( pStickerMaterialThirdPerson != NULL )
			{
				int nStickerAttachmentBoneIndex = Studio_BoneIndexByName( &studioHdr, szStickerAttachmentLookupTable[ i ] );

				if ( nStickerAttachmentBoneIndex != -1 )
				{
					MatrixCopy( rdBoneToWorld[ nStickerAttachmentBoneIndex ], weaponBoneTransform );
					MatrixPosition( weaponBoneTransform, vecWorldRayOrigin );
					Vector a, b;
					MatrixVectors( weaponBoneTransform, &vecWorldRayDirection, &a, &b );
					bWeaponProvidedStickerAttachments = true;
				}
				else
				{
					int nBIndex = Studio_BoneIndexByName( &studioHdr, pEquippedItem->GetStickerWorldModelBoneParentNameBySlotIndex( i ) );
					if ( nBIndex == -1 )
						continue; //couldn't find the parent bone this sticker slot wanted

					MatrixCopy( rdBoneToWorld[ nBIndex ], weaponBoneTransform );

					Ray_t stickerRayLocal;
					stickerRayLocal.Init( pEquippedItem->GetStickerSlotWorldProjectionStartBySlotIndex( i ),
										  pEquippedItem->GetStickerSlotWorldProjectionEndBySlotIndex( i ) );

					VectorTransform( stickerRayLocal.m_Start, weaponBoneTransform, vecWorldRayOrigin );
					VectorRotate( stickerRayLocal.m_Delta, weaponBoneTransform, vecWorldRayDirection );
				}

				Ray_t stickerRayWorld;
				stickerRayWorld.Init( vecWorldRayOrigin, vecWorldRayOrigin + ( 3 * vecWorldRayDirection ) );

				VMatrix vmatrix_weaponBoneTransform( weaponBoneTransform );

				Vector vecStickerUp = vmatrix_weaponBoneTransform.GetLeft();
				if ( bWeaponProvidedStickerAttachments )
				{
					//content defined sticker attachments are z-up
					vecStickerUp = /*-*/vmatrix_weaponBoneTransform.GetUp(); // NOTE: is -ve in c_basecombatweapon.cpp?
				}

				g_pStudioRender->AddDecal( decalHandle, g_pMDLCache->GetStudioHdr( hMDL ),
										   rdBoneToWorld.Base(), stickerRayWorld, vecStickerUp, pStickerMaterialThirdPerson, 1.2f, 0/*body*/, false/*noPokeThru*/, ADDDECAL_TO_ALL_LODS );
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

// CON_COMMAND_F ( modelpanel_set_sticker, "[Slot] [Id] Adds a sticker to the 3d weapon preview model", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT )
// {
// 	if ( args.ArgC () < 3 )
// 		return;
// 
// 	g_pModelPanelWeaponPreview->ApplySticker ( V_atoi ( args[ 1 ] ), V_atoi ( args[ 2 ] ) );
// }

bool CUI_ItemPreviewPanel::SetAsActivePreviewPanel()
{
	g_pActivePreviewPanel = this;

	return true;
}

void CUI_ItemPreviewPanel::ApplySticker( int nSlot, int nStickerId )
{
	if ( !m_pItemPreviewRenderer )
		return;

	CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;

	if ( !pScene )
		return;

	CEconItemView *pItem = GetEconItem( pScene->GetMainEconItemName(), NULL, -1, -1 );

	if ( pItem && pItem->ItemHasAnyFreeStickerSlots() )
	{
		nSlot = pItem->GetStickerSlotFirstFreeFromIndex( nSlot );

		if ( nSlot != -1 )
		{
			//setup the next free sticker
			pItem->SetOrAddAttributeValueByName( CFmtStr ( "sticker slot %i id", nSlot ), *( (float*)&nStickerId ) );

			// re-apply merged mdls including new sticker
			InitSceneModels ( pItem, pScene->GetMainEconItemName () );// NULL );

			//preemptively clear it out right away
			pItem->SetOrAddAttributeValueByName( CFmtStr ( "sticker slot %i id", nSlot ), 0 );
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CEconItemView *CUI_ItemPreviewPanel::GetWeaponPreviewItemData()
{
	if ( !m_pItemPreviewRenderer )
		return NULL;

	CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;
	
	if ( !pScene )
		return NULL;

	return GetEconItem( pScene->GetMainEconItemName(), NULL, -1, -1 );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewPanel::ResetAnimation( bool bResetUsingManifest )
{
	if ( !m_pItemPreviewRenderer )
		return true;

	// scene
	CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;

	if ( !pScene )
	{
		DevWarning( "CUI_ItemPreviewPanel::ResetPlayerAnims - no scene item!!\n" );
		return true;
	}

	pScene->ClearAnimFollowLoop();

	pScene->RemoveAllSequenceLayers();
#if defined _DEBUG_ANIMATION_INFO
	DevMsg("\n\n*** ResetAnimation ***\n\n");
#endif

	if ( bResetUsingManifest )
	{
		MDLCACHE_CRITICAL_SECTION();

		CMDL *pMdlItemConfig = pScene->GetMDLItemConfig();

		if ( pMdlItemConfig->m_MDLHandle == MDLHANDLE_INVALID )
			return true;

		CStudioHdr studioHdr( pMdlItemConfig->GetStudioHdr(), g_pMDLCache );

		char const *szItemAnim = m_kvConfiguration->GetString( "weapon_anim", "ACT_IDLE_INSPECT" );

		int iSequence = UI_HelperGetSequenceIdFromStudioHdr( studioHdr, szItemAnim );
		if ( iSequence != ACT_INVALID )
		{
			pMdlItemConfig->m_nSequence = iSequence;
			pMdlItemConfig->m_bUseSequencePlaybackFPS = true;

			char const *szItemAnimLoop = m_kvConfiguration->GetString( "weapon_anim_loop", "ACT_IDLE_INSPECT" );
			iSequence = UI_HelperGetSequenceIdFromStudioHdr( studioHdr, szItemAnimLoop );

			if ( iSequence != ACT_INVALID )
			{
				pMdlItemConfig->m_arrSequenceFollowLoop.AddToTail( iSequence );
			}
		}
	}

	// reset start time
	m_pItemPreviewRenderer->OnTick();

	return true;
}

//-----------------------------------------------------------------------------
// Queue a regular sequemce to the merged mdl
//
// Merged mdl behavior for queued sequences:
//
// Queued sequences: an array of sequences that play one after the other from start to finish
//					 the last sequence will repeat (loop). 
//  				 There is no blending, fading, etc with these sequences.
//					 Weight of each sequence is set explicitly to 1.0 in merged mdl setupbones
//							
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewPanel::QueueSequence( const char *szAnim, bool bImmediate )
{
	if ( !m_pItemPreviewRenderer )
		return false;

	// scene
	CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;

	if ( !pScene )
	{
		DevWarning( "CUI_ItemPreviewPanel::QueueSequence - no scene item!!\n" );
		return false;
	}

	if ( m_pItemPreviewRenderer->IsAnimationPaused() && !bImmediate )
		return false;

	{
		MDLCACHE_CRITICAL_SECTION();

		CMDL *pMdlItemConfig = pScene->GetMDLItemConfig();

		if ( pMdlItemConfig->m_MDLHandle == MDLHANDLE_INVALID )
			return false;

		CStudioHdr studioHdr( pMdlItemConfig->GetStudioHdr(), g_pMDLCache );
		int nSequence = UI_HelperGetSequenceIdFromStudioHdr( studioHdr, szAnim );

		if ( nSequence != ACT_INVALID )
		{
			if ( bImmediate )
			{
				// set to head of queue and start immediately
				pScene->SetSequence( nSequence, true );
#if defined _DEBUG_ANIMATION_INFO
				DevMsg( "QueueSeq(Imm): %s seqId %d\n", szAnim, nSequence );
#endif
			}
			else
			{
				// queue this sequence up next
				pScene->AddSequenceFollowLoop( nSequence, true );
#if defined _DEBUG_ANIMATION_INFO
				DevMsg( "QueueSeq(queue): %s seqId %d\n", szAnim, nSequence );
#endif
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Add a layer sequence
//
// Merged mdl behavior for layered sequences:
//
// Layered sequences: an array of sequences to accumulate on top of the queued sequence (via accumulatepose)
//					  weights can be sepcified, so can perform blending/fading at a higher level (as in this fn).
//					  Is the only way to currently perform any kind of blending without changing merged mdl.
//
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewPanel::LayerSequence( const char *szAnim, bool bLoop, bool bWaitForPreviousLayerSequenceToFinish )
{
	if ( !m_pItemPreviewRenderer )
		return false;

	// scene
	CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;

	if ( !pScene )
	{
		DevWarning( "CUI_ItemPreviewPanel::LayerSequence - no scene item!!\n" );
		return false;
	}

	if ( m_pItemPreviewRenderer->IsAnimationPaused() && ( pScene->GetNumSequenceLayers() > 1 ) )
		return false;

	if ( pScene->GetNumSequenceLayers() >= MAX_SEQUENCE_LAYERS )
	{
		DevWarning( "CUI_ItemPreviewPanel::LayerSequence - max sequence layers (8) reached!!\n" );
		return false;
	}

	{
		MDLCACHE_CRITICAL_SECTION();

		CMDL *pMdlItemConfig = pScene->GetRootMDL();

		if ( pMdlItemConfig->m_MDLHandle == MDLHANDLE_INVALID )
		{
			DevWarning ( "CUI_ItemPreviewPanel::LayerSequence - no valid mdl!!\n" );
			return false;
		}

		CStudioHdr studioHdr( pMdlItemConfig->GetStudioHdr (), g_pMDLCache );

		int nSequenceIndex = UI_HelperGetSequenceIdFromStudioHdr( studioHdr, szAnim );

		if ( nSequenceIndex != ACT_INVALID )
		{
			int nNumCurrSequenceLayers = pScene->GetNumSequenceLayers();

			MDLSquenceLayer_t *pLayer = pScene->GetNextEmptySequenceLayer();

			pLayer->m_nSequenceIndex = nSequenceIndex;
			pLayer->m_bLoop = bLoop;
			pLayer->m_flWeight = 0.0f;

			// delay start ?
			if ( bWaitForPreviousLayerSequenceToFinish && ( nNumCurrSequenceLayers > 0 ) )
			{
				mstudioseqdesc_t &seqdesc = studioHdr.pSeqdesc( nSequenceIndex );
				float flFadeInTime = seqdesc.fadeintime * panorama_3dpanel_anim_fadeinout_time_scale.GetFloat();
				pLayer->m_flStartTime = pScene->GetSequenceEndTime( &studioHdr, nNumCurrSequenceLayers - 1 ) - flFadeInTime;

#if defined _DEBUG_ANIMATION_INFO
				DevMsg( "LayerSeq(trans): %s, seqId %d, totSeq %d, lp %d\n", szAnim, nSequenceIndex, pScene->GetNumSequenceLayers(), bLoop );
#endif
			}
			else
			{
				// start right away
				pLayer->m_flStartTime = pScene->GetRootMDL()->m_flTime;
				
				if ( nNumCurrSequenceLayers == 0 )
				{
					// don't fade in
					pLayer->m_flWeight = 1.0f;
					// right now CMDL assumes a base sequence is playing, even if we set the weight of this layer to 1.0 if it has a blend in time
					// then it will blend with whatever queued sequence is already playing (or possible the default sequence since CMDL alwyas has at least one (base) seq playing - TODO - see if we can reconfigure this
					// For now this can be worked around two ways. 1. Ensure if you want to play a layer right away then just queue that same anim as the base sequence, it will blend into itself, or 
					// 2. ensure the anim has no blend in time.
					// (note - layers provide the only mechanism in CMDL for blending anims right now)
					QueueSequence( szAnim, true );
				}

#if defined _DEBUG_ANIMATION_INFO
				DevMsg( "LayerSeq(no-trans): %s, seqId %d, totSeq %d, lp %d\n", szAnim, nSequenceIndex, pScene->GetNumSequenceLayers(), bLoop );
#endif
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Light animation
//-----------------------------------------------------------------------------

// overall dimming of lights in panel
bool CUI_ItemPreviewPanel::SetPanelLightingAmount( float flAmount )
{
	if ( !m_pItemPreviewRenderer )
		return true;

	SetFlashlightAmount( flAmount );
	m_pItemPreviewRenderer->AnimateAmbientLightAmount( flAmount );
	m_pItemPreviewRenderer->AnimateAllDirectionalLightAmounts( flAmount );

	return true;
}

bool CUI_ItemPreviewPanel::SetFlashlightAmount( float flAmount ) 
{ 
	m_cfgRenderCapture.m_renderFlashlightState.m_fBrightnessScale = flAmount;

	return true; 
};

bool CUI_ItemPreviewPanel::SetDirectionalLightModify( int nLight )
{
	if ( ( nLight < 0 ) || ( nLight >= MAX_PANEL_LIGHTS ) )
		return true;

	m_nDirectionalLightModify = nLight;

	return true;
}

bool CUI_ItemPreviewPanel::SetDirectionalLightPulseFlicker( float flPulseFrequency, float flPulseAmount, float flFlickerRate, float flFlickerAmount )
{
	m_aDirLightInfo[ m_nDirectionalLightModify ].m_pulseFlicker.x = ( 2.0f * M_PI_F ) * fabs( flPulseFrequency ); // pulse frequency * 2 PI
	m_aDirLightInfo[ m_nDirectionalLightModify ].m_pulseFlicker.y = fabs( flPulseAmount );	// pulse amount
	m_aDirLightInfo[ m_nDirectionalLightModify ].m_pulseFlicker.z = 360.0f * fabs( flFlickerRate ); // flicker frequency * 2 PI
	m_aDirLightInfo[ m_nDirectionalLightModify ].m_pulseFlicker.w = fabs( flFlickerAmount );	// flicker amount

	return true;
}

bool CUI_ItemPreviewPanel::SetDirectionalLightRotation( float flRotX, float flRotY, float flRotZ )
{
	m_aDirLightInfo[ m_nDirectionalLightModify ].m_rotateSpeed.x = flRotX;
	m_aDirLightInfo[ m_nDirectionalLightModify ].m_rotateSpeed.y = flRotY;
	m_aDirLightInfo[ m_nDirectionalLightModify ].m_rotateSpeed.z = flRotZ;

	return true;
}

bool CUI_ItemPreviewPanel::SetDirectionalLightAmount(float flAmount)
{
	m_pItemPreviewRenderer->AnimateDirectionalLightAmount( m_nDirectionalLightModify, flAmount );

	return true;
}

bool CUI_ItemPreviewPanel::SetDirectionalLightColor( float flColR, float flColG, float flColB )
{
	m_aDirLightInfo[ m_nDirectionalLightModify ].m_color.x = flColR;
	m_aDirLightInfo[ m_nDirectionalLightModify ].m_color.y = flColG;
	m_aDirLightInfo[ m_nDirectionalLightModify ].m_color.z = flColB;

	if (m_pItemPreviewRenderer)
		m_pItemPreviewRenderer->SetDirectionalLightCol( m_nDirectionalLightModify, m_aDirLightInfo[ m_nDirectionalLightModify ].m_color );

	return true;
}

bool CUI_ItemPreviewPanel::SetDirectionalLightDirection( float flDirX, float flDirY, float flDirZ )
{
	m_aDirLightInfo[ m_nDirectionalLightModify ].m_direction.x = flDirX;
	m_aDirLightInfo[ m_nDirectionalLightModify ].m_direction.y = flDirY;
	m_aDirLightInfo[ m_nDirectionalLightModify ].m_direction.z = flDirZ;

	if ( m_pItemPreviewRenderer )
		m_pItemPreviewRenderer->SetDirectionalLightDir( m_nDirectionalLightModify, m_aDirLightInfo[ m_nDirectionalLightModify ].m_direction );

	return true;
}

bool CUI_ItemPreviewPanel::SetFlashlightPulseFlicker( float flPulseFrequency, float flPulseAmount, float flFlickerRate, float flFlickerAmount )
{
	m_flashlightInfo.m_pulseFlicker.x = ( 2.0f * M_PI_F ) * fabs( flPulseFrequency ); // pulse frequency * 2 PI
	m_flashlightInfo.m_pulseFlicker.y = fabs( flPulseAmount ); // pulse amount
	m_flashlightInfo.m_pulseFlicker.z = 360.0f * fabs( flFlickerRate ); // flicker frequency * 2 PI
	m_flashlightInfo.m_pulseFlicker.w = fabs( flFlickerAmount ); // flicker amount

	return true;
}

bool CUI_ItemPreviewPanel::SetFlashlightRotation( float flRotX, float flRotY, float flRotZ )
{
	m_flashlightInfo.m_rotateSpeed.x = flRotX;
	m_flashlightInfo.m_rotateSpeed.y = flRotY;
	m_flashlightInfo.m_rotateSpeed.z = flRotZ;

	return true;
}

bool CUI_ItemPreviewPanel::SetFlashlightColor( float flColR, float flColG, float flColB )
{
	m_flashlightInfo.m_color.x = flColR;
	m_flashlightInfo.m_color.y = flColG;
	m_flashlightInfo.m_color.z = flColB;

	m_cfgRenderCapture.m_renderFlashlightState.m_Color[ 0 ] = flColR;
	m_cfgRenderCapture.m_renderFlashlightState.m_Color[ 1 ] = flColG;
	m_cfgRenderCapture.m_renderFlashlightState.m_Color[ 2 ] = flColB;
	m_cfgRenderCapture.m_renderFlashlightState.m_Color[ 3 ] = 1.0f;

	return true;
}

bool CUI_ItemPreviewPanel::SetFlashlightPosition( float flPosX, float flPosY, float flPosZ )
{
	m_flashlightInfo.m_position.x = flPosX;
	m_flashlightInfo.m_position.y = flPosY;
	m_flashlightInfo.m_position.z = flPosZ;

	if ( m_pItemPreviewRenderer )
	{
		m_pItemPreviewRenderer->SetRenderCaptureCameraPositionOverride( m_flashlightInfo.m_position );
		m_pItemPreviewRenderer->SetRenderCaptureCameraPositionOverrideEnabled( true );
	}

	return true;
}

bool CUI_ItemPreviewPanel::SetFlashlightAngle( float flAngX, float flAngY, float flAngZ )
{
	m_flashlightInfo.m_angle.x = flAngX;
	m_flashlightInfo.m_angle.y = flAngY;
	m_flashlightInfo.m_angle.z = flAngZ;

	if ( m_pItemPreviewRenderer )
	{
		m_pItemPreviewRenderer->SetRenderCaptureCameraOrientOverride( m_flashlightInfo.m_angle );
		m_pItemPreviewRenderer->SetRenderCaptureCameraOrientOverrideEnabled(true);
	}

	return true;
}


bool CUI_ItemPreviewPanel::SetFlashlightFOV( float flFOV )
{
	if ( m_pItemPreviewRenderer )
	{
		m_pItemPreviewRenderer->SetRenderCaptureCameraFOV( flFOV );

		CRenderCaptureConfigurationState *pFlashlightInfo = reinterpret_cast<CRenderCaptureConfigurationState *>( m_pItemPreviewRenderer->GetRenderingWithFlashlightConfiguration () );

		if ( pFlashlightInfo )
		{
			pFlashlightInfo->m_renderFlashlightState.m_fVerticalFOVDegrees = flFOV;
			pFlashlightInfo->m_renderFlashlightState.m_fHorizontalFOVDegrees = flFOV;
		}
	}

	return true;
}

// get near/far z from scene bounds and flashlight position
bool CUI_ItemPreviewPanel::SetFlashlightNearFarZFromSceneBounds()
{
	if ( !m_pItemPreviewRenderer )
		return false;

	CRenderCaptureConfigurationState *pFlashlightInfo = reinterpret_cast<CRenderCaptureConfigurationState *>( m_pItemPreviewRenderer->GetRenderingWithFlashlightConfiguration() );

	if ( !pFlashlightInfo )
		return false;

	// get tightest depth bounds
	float flNearZ, flFarZ;

	Vector pos;
	QAngle ang;
	GetFlashlightAngle( ang );
	GetFlashlightPosition( pos );

	matrix3x4_t mat;
	AngleMatrix( ang, pos, mat );

	float flSceneRadius;
	Vector vSceneCenter;

	m_pItemPreviewRenderer->GetSceneBounds( vSceneCenter, flSceneRadius );

	float flDist = mat.GetForward().Dot( vSceneCenter - pos );

	flNearZ = flDist - flSceneRadius;
	flFarZ = flDist + flSceneRadius;

	pFlashlightInfo->m_renderFlashlightState.m_NearZ = flNearZ;
	pFlashlightInfo->m_renderFlashlightState.m_FarZ = flFarZ;
	pFlashlightInfo->m_renderFlashlightState.m_FarZAtten = flFarZ * 2.0f;

	return true;
}

// set specific flashlight near/far z
bool CUI_ItemPreviewPanel::SetFlashlightNearFarZ( float flNearZ, float flFarZ )
{
	CRenderCaptureConfigurationState *pFlashlightInfo = reinterpret_cast<CRenderCaptureConfigurationState *>( m_pItemPreviewRenderer->GetRenderingWithFlashlightConfiguration () );

	if ( pFlashlightInfo )
	{
		pFlashlightInfo->m_renderFlashlightState.m_NearZ = flNearZ;
		pFlashlightInfo->m_renderFlashlightState.m_FarZ = flFarZ;
		pFlashlightInfo->m_renderFlashlightState.m_FarZAtten = flFarZ;
	}

	return true;
}

bool CUI_ItemPreviewPanel::SetAmbientLightColor( float flColR, float flColG, float flColB )
{
	m_vAmbientLightColor.x = flColR;
	m_vAmbientLightColor.y = flColG;
	m_vAmbientLightColor.z = flColB;

	if (m_pItemPreviewRenderer)
		m_pItemPreviewRenderer->SetLightAmbient( m_vAmbientLightColor );

	return true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

void CUI_ItemPreviewPanel::GetDirectionalLightPulseFlicker( int nLight, Vector4D &vPulseFlicker )
{
	vPulseFlicker = m_aDirLightInfo[ nLight ].m_pulseFlicker;
}

void CUI_ItemPreviewPanel::GetDirectionalLightRotation( int nLight, QAngle &rot )
{
	rot = m_aDirLightInfo[ nLight ].m_rotateSpeed;
}

void CUI_ItemPreviewPanel::GetDirectionalLightColor( int nLight, Vector &vCol )
{
	vCol = m_aDirLightInfo[ nLight ].m_color;
}

void CUI_ItemPreviewPanel::GetDirectionalLightDirection( int nLight, Vector &vDir )
{
	vDir = m_aDirLightInfo[ nLight ].m_direction;
}

void CUI_ItemPreviewPanel::GetFlashlightPulseFlicker( Vector4D &vPulseFlicker )
{
	vPulseFlicker = m_flashlightInfo.m_pulseFlicker;
}

void CUI_ItemPreviewPanel::GetFlashlightRotation( QAngle &rot )
{
	rot = m_flashlightInfo.m_rotateSpeed;
}

void CUI_ItemPreviewPanel::GetFlashlightColor( Vector &vCol )
{
	vCol = m_flashlightInfo.m_color;
}

void CUI_ItemPreviewPanel::GetFlashlightPosition( Vector &vPos )
{
	vPos = m_flashlightInfo.m_position;
}

void CUI_ItemPreviewPanel::GetFlashlightAngle( QAngle &ang )
{
	ang = m_flashlightInfo.m_angle;
}

void CUI_ItemPreviewPanel::GetAmbientLightColor( Vector &vCol )
{
	vCol = m_vAmbientLightColor;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

bool CUI_ItemPreviewPanel::SetSceneRotation( float flRot0, float flRot1, float flRot2 )
{
	if ( !m_pItemPreviewRenderer )
		return true;

	float aRot[3] = { flRot0, flRot1, flRot2 };

	if ( (fabsf( flRot0 ) < 0.0001f) &&
		 (fabsf( flRot1 ) < 0.0001f) &&
		 (fabsf( flRot2 ) < 0.0001f) )
	{
		SetRotationFlags( m_info.m_bRotate ? SCENE_PANEL_ROTATION_MOUSE : SCENE_PANEL_ROTATION_NONE );
	}
	else
	{
		SetRotationFlags( SCENE_PANEL_ROTATION_ALL );
	}

	for ( int idx = 0; idx < NUM_ROTATEAXIS; idx++ )
	{
		if ( m_aRotateAxisOrder[ idx ] != RotateAxis_Invalid )
		{
			SetItemRotationSpeedTarget( idx, aRot[ m_aRotateAxisOrder[ idx ] ], true );
		}
	}

	return true;
}

bool CUI_ItemPreviewPanel::SetSceneAngles( float flRot0, float flRot1, float flRot2 )
{
	if ( !m_pItemPreviewRenderer )
		return true;
	
	m_aItemRotate[ 0 ] = m_aItemRotate[ 1 ] = Vector( flRot0, flRot1, flRot2 );
	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

bool CUI_ItemPreviewPanel::SetCameraPosition( float flX, float flY, float flZ )
{
	if ( !m_pItemPreviewRenderer )
		return true;

	m_pItemPreviewRenderer->SetCameraPosition( Vector( flX, flY, flZ ) );
	m_pItemPreviewRenderer->SetCameraPositionOverride( Vector ( flX, flY, flZ ) );
	m_pItemPreviewRenderer->UpdateCameraPreset0();

	return true;
}

bool CUI_ItemPreviewPanel::SetCameraAngles( float flX, float flY, float flZ )
{
	if ( !m_pItemPreviewRenderer )
		return true;

	m_pItemPreviewRenderer->SetCameraAngles( QAngle( flX, flY, flZ ) );
	m_pItemPreviewRenderer->SetCameraOrientOverride( QAngle( flX, flY, flZ ) );
	m_pItemPreviewRenderer->UpdateCameraPreset0();

	return true;
}

bool CUI_ItemPreviewPanel::SetCameraPreset( int nPreset, bool bBlend )
{
	if ( !m_pItemPreviewRenderer )
		return true;

	m_pItemPreviewRenderer->SetCameraPreset( nPreset, bBlend );

	if ( bBlend )
	{
		CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;
		if ( pScene )
			m_flCameraPresetStartBlendTime = pScene->GetRootMDL()->m_flTime;
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
ConVar panorama_loadout_rotate_intro_scale ( "panorama_loadout_rotate_intro_scale", "0.5" );

bool CUI_ItemPreviewPanel::SetSceneIntroRotation( float flLR, float flT, bool bLoop )
{
	if ( !m_bFMActive )
		m_flFMStartT = -1.0f;

	if ( flT <= 0.0f )
		return false;

	m_bFMActive = true;
	m_flFMT1 = flT * 0.5f;
	m_flFMLR1 = -flLR;
	m_flFMT2 = flT * 0.5f;
	m_flFMLR2 = flLR;
	m_bFMLoop = bLoop;

	return true;
}

bool CUI_ItemPreviewPanel::SetSceneIntroFOV ( float flScale, float flT )
{
	if ( !m_pItemPreviewRenderer )
		return true;

	m_bFZActive = true;
	m_flFZStartT = -1.0f;
	m_flFZT1 = flT;
	m_flFZStartFOV = MIN( 180.0f, flScale * m_pItemPreviewRenderer->GetCameraFOV() );
	m_flFZTargetFOV = m_pItemPreviewRenderer->GetCameraFOV();

	m_pItemPreviewRenderer->SetCameraFOV ( m_flFZStartFOV );

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// TODO - find alternative approach, changing the mdl.m_color doesn't seem to do anything
bool CUI_ItemPreviewPanel::SetFloatingFloorAlpha( float flAlpha )
{
	if ( !m_pItemPreviewRenderer )
		return true;

	// scene
	CUI_SceneItem *pScene = &m_pItemPreviewRenderer->m_SceneMergedMDL;

	if ( pScene )
	{
		MDLCACHE_CRITICAL_SECTION();

		CMDL *pMdl = pScene->GetShadowFloorCMDL( SHADOW_FLOOR_MDL );

		if ( pMdl )
		{
			flAlpha = clamp( flAlpha, 0.0f, 1.0f );
			pMdl->m_Color.SetColor( pMdl->m_Color.r(), pMdl->m_Color.g(), pMdl->m_Color.b(), flAlpha * 255.0f );
		}

		if ( pMdl->GetMDL() != MDLHANDLE_INVALID )
			g_pMDLCache->Release( pMdl->GetMDL() );
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewPanel::SetParticleSystemOffsetPosition( float flX, float flY, float flZ )
{
	if ( !m_pItemPreviewRenderer )
		return true;

	m_pItemPreviewRenderer->SetParticleSystemOffsetPosition( flX, flY, flZ );

	return true;
}

bool CUI_ItemPreviewPanel::SetParticleSystemOffsetAngles( float flX, float flY, float flZ )
{
	if ( !m_pItemPreviewRenderer )
		return true;

	m_pItemPreviewRenderer->SetParticleSystemOffsetAngles( flX, flY, flZ );

	return true;
}

bool CUI_ItemPreviewPanel::AddParticleSystem( const char *szParticleSystemName, const char *szAttachToBoneName, bool bRepeat )
{
	if ( !m_pItemPreviewRenderer )
		return true;

	m_pItemPreviewRenderer->AddParticleSystem( szParticleSystemName, szAttachToBoneName, bRepeat );

	return true;
}

void CUI_ItemPreviewPanel::UpdateParticleSystems( float flTime )
{
	if ( !m_pItemPreviewRenderer )
		return;

	m_pItemPreviewRenderer->UpdateParticleSystems( flTime );
}

void CUI_ItemPreviewPanel::StopParticleSystems()
{
	if ( !m_pItemPreviewRenderer )
		return;

	m_pItemPreviewRenderer->StopParticleSystems();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewPanel::TogglePause()
{
	if ( !m_pItemPreviewRenderer )
		return false;

	m_pItemPreviewRenderer->ToggleAnimationPaused();

	return true;
}

bool CUI_ItemPreviewPanel::Pause( bool bPause )
{
	if ( !m_pItemPreviewRenderer )
		return false;

	if ( bPause != m_pItemPreviewRenderer->IsAnimationPaused() )
	{
		m_pItemPreviewRenderer->SetAnimationPaused( bPause );
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewPanel::EnableRendering( bool bEnable )
{
	if ( !m_pItemPreviewRenderer )
		return false;

	if ( bEnable != m_pItemPreviewRenderer->IsRenderingEnabled() )
	{
		m_pItemPreviewRenderer->EnableRendering( bEnable );
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CUI_ItemPreviewPanel::UpdatePresetCamera( float flTime )
{
	if ( !m_pItemPreviewRenderer )
		return;

	if ( m_pDebug->BIsVisible() )
		return;

	if ( m_bFZActive )
		return;

	if ( sCameraPreset *pCameraPreset = m_pItemPreviewRenderer->GetCameraPreset() )
	{
		Vector blendedPos, blendedPivot;
		QAngle blendedAng;
		float blendedFOV;

		float dt = ( flTime - m_flCameraPresetStartBlendTime );
		if ( ( dt > 0.0f ) && ( dt < panorama_3dpanel_camera_preset_blend_time.GetFloat() ) )
		{
			dt = clamp( dt / panorama_3dpanel_camera_preset_blend_time.GetFloat(), 0.0f, 1.0f );
			float dtSq = dt * dt;
			float s = ( 3.0f * dtSq ) - ( 2.0f * dtSq * dt );

			m_pItemPreviewRenderer->BlendCameraPreset( blendedPos, blendedPivot, blendedAng, blendedFOV, s );

			m_pItemPreviewRenderer->SetCameraPivotPosition( blendedPivot );
			m_pItemPreviewRenderer->SetCameraPositionAndAngles( blendedPos, blendedAng );
			m_pItemPreviewRenderer->SetCameraFOV( blendedFOV );
		}
		else
		{
			m_pItemPreviewRenderer->SetCameraPivotPosition( pCameraPreset->m_pivot );
			m_pItemPreviewRenderer->SetCameraPositionAndAngles( pCameraPreset->m_pos, pCameraPreset->m_ang );
			m_pItemPreviewRenderer->SetCameraFOV( pCameraPreset->m_fov );
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewPanel::CopyDebugCameraSettingsToClipboard()
{
	if ( !m_pItemPreviewRenderer )
		return false;

	CUtlVector< char > textBuf;

	Vector pos, pivot;
	QAngle angles;
	float fov;

	m_pItemPreviewRenderer->GetCameraPositionAndAngles( pos, angles );
	m_pItemPreviewRenderer->GetCameraPivotPosition( pivot );
	fov = m_pItemPreviewRenderer->GetCameraFOV();

	char tempText[ 512 ];
	V_sprintf_safe( tempText, "\"camera_offset\"   \"%3.2f %3.2f %3.2f\"\n\"camera_orient\"   \"%3.2f %3.2f %3.2f\"\n\"orbit_pivot\"     \"%3.2f %3.2f %3.2f\"\n\"root_camera_fov\"     \"%2.1f\"",
					pos.x, pos.y, pos.z, angles.x, angles.y, angles.z, pivot.x, pivot.y, pivot.z, fov );

	textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
	textBuf.AddToTail( 0 );

//	vgui::system()->SetClipboardText( textBuf.Base(), textBuf.Count() );

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewPanel::CopyFlashlightSettingsToClipboard()
{
	if ( !m_pItemPreviewRenderer )
		return false;

	CUtlVector< char > textBuf;

	Vector pos, pivot;
	QAngle angles;

	m_pItemPreviewRenderer->GetCameraPositionAndAngles( pos, angles );
	m_pItemPreviewRenderer->GetCameraPivotPosition ( pivot );

	char tempText[ 512 ];
	V_sprintf_safe( tempText, "\"shadow_light_offset\"   \"%3.2f %3.2f %3.2f\"\n\"shadow_light_orient\"   \"%3.2f %3.2f %3.2f\"",
					pos.x, pos.y, pos.z, angles.x, angles.y, angles.z );

	textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
	textBuf.AddToTail( 0 );

//	vgui::system()->SetClipboardText( textBuf.Base(), textBuf.Count() );

	return true;
}

//-----------------------------------------------------------------------------
// TODO - animated and rotating light settings
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewPanel::CopyAllLightSettingsToClipboard()
{
	if (!m_pItemPreviewRenderer)
		return false;

	CUtlVector< char > textBuf;

	Vector pos, dir, col;
	Vector4D pulseFlicker;
	QAngle ang, rot;

	char tempText[512];

	CUI_ItemPreviewDebug::eDebugLight eSelectedLight = m_pDebug->GetActiveDebugLight();


	if ( ( eSelectedLight == CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_NONE ) ||
		 ( m_pDebug->BEnabledAllLights() ) )
	{
		// clear all directional lights
		V_sprintf_safe( tempText, "\"light_directional_clearall\" \"1\"" );
		textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
		textBuf.AddToTail( '\n' );

		// add directional lights
		for ( int idx = 0; idx < m_pItemPreviewRenderer->GetLocalLightCount(); idx++ )
		{
			GetDirectionalLightColor( idx, col );
			GetDirectionalLightDirection( idx, dir );
			GetDirectionalLightRotation( idx, rot );
			GetDirectionalLightPulseFlicker( idx, pulseFlicker );

			if ( ( col.x + col.y + col.z ) > 0.0f )
			{
				V_sprintf_safe( tempText, "\"light_directional_add\"      \"rgb{%3.2f %3.2f %3.2f} dir[%3.2f %3.2f %3.2f] rot[%3.1f %3.1f %3.1f] flicker[%3.2f %3.2f %3.2f %3.2f]\"",
					col.x, col.y, col.z, dir.x, dir.y, dir.z,
					rot.x, rot.y, rot.z, pulseFlicker.x, pulseFlicker.y, pulseFlicker.z, pulseFlicker.w );
				textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
				textBuf.AddToTail( '\n' );
			}
		}

		// add flashlight
		GetFlashlightPosition( pos );
		GetFlashlightAngle( ang );
		GetFlashlightColor( col );

		V_sprintf_safe( tempText, "\"shadow_light_offset\"        \"%3.2f %3.2f %3.2f\"\n\"shadow_light_orient\"        \"%3.2f %3.2f %3.2f\"",
			pos.x, pos.y, pos.z, ang.x, ang.y, ang.z );
		textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
		textBuf.AddToTail( '\n' );

		// set flashlight brightness to 1.0 and scale color?
		float hdrScale = 1.0f;
		Vector rgbN;
		if ( ( col.x > 1.0f ) || ( col.y > 1.0f ) || ( col.z > 1.0f ) )
		{
			hdrScale = MAX( col.x, MAX( col.y, col.z ) );
		}
		rgbN = col / hdrScale;

		V_sprintf_safe( tempText, "\"shadow_light_brightness\"    \"%3.2f\"", hdrScale );
		textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
		textBuf.AddToTail( '\n' );

		V_sprintf_safe( tempText, "\"shadow_light_color\"         \"[%3.2f %3.2f %3.2f]\"",
			rgbN.x, rgbN.y, rgbN.z );
		textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
		textBuf.AddToTail( '\n' );

		GetFlashlightRotation( rot );
		V_sprintf_safe( tempText, "\"shadow_light_rotation\"      \"[%3.2f %3.2f %3.2f]\"",
			rot.x, rot.y, rot.z );
		textBuf.AddMultipleToTail (V_strlen( tempText ), tempText );
		textBuf.AddToTail( '\n' );

		GetFlashlightPulseFlicker( pulseFlicker );
		V_sprintf_safe( tempText, "\"shadow_light_flicker\"       \"[%3.2f %3.2f %3.2f %3.2f]\"",
			pulseFlicker.x, pulseFlicker.y, pulseFlicker.z, pulseFlicker.w );
		textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
		textBuf.AddToTail( '\n' );

		CRenderCaptureConfigurationState *pFlashlightInfo = reinterpret_cast<CRenderCaptureConfigurationState *>( m_pItemPreviewRenderer->GetRenderingWithFlashlightConfiguration () );

		if ( pFlashlightInfo )
		{
			V_sprintf_safe( tempText, "\"shadow_light_hfov\"       \"%3.1f\"\n\"shadow_light_vfov\"       \"%3.1f\"",
				pFlashlightInfo->m_renderFlashlightState.m_fVerticalFOVDegrees, pFlashlightInfo->m_renderFlashlightState.m_fVerticalFOVDegrees );
			textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
			textBuf.AddToTail ( '\n' );

			V_sprintf_safe( tempText, "\"shadow_light_znear\"       \"%4.1f\"\n\"shadow_light_zfar\"       \"%4.1f\"\n\"shadow_light_atten_farz\"       \"%4.1f\"",
				pFlashlightInfo->m_renderFlashlightState.m_NearZ, pFlashlightInfo->m_renderFlashlightState.m_FarZ, pFlashlightInfo->m_renderFlashlightState.m_FarZAtten );
			textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
			textBuf.AddToTail ( '\n' );
		}

		// add ambient
		GetAmbientLightColor( col );
		V_sprintf_safe( tempText, "\"light_ambient\"              \"[%3.2f %3.2f %3.2f]\"",
			col.x, col.y, col.z );
		textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
	}
	else
	{
		if ( eSelectedLight == CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_FLASHLIGHT )
		{
			GetFlashlightPulseFlicker( pulseFlicker );
			V_sprintf_safe( tempText, "SetFlashlightPulseFlicker( %3.2f, %3.2f, %3.2f, %3.2f )",
				pulseFlicker.x, pulseFlicker.y, pulseFlicker.z, pulseFlicker.w );
			textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
			textBuf.AddToTail( '\n' );

			GetFlashlightRotation( rot );
			V_sprintf_safe( tempText, "SetFlashlightRotation( %3.2f, %3.2f, %3.2f )",
				rot.x, rot.y, rot.z );
			textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
			textBuf.AddToTail( '\n' );

			GetFlashlightPosition( pos );
			V_sprintf_safe( tempText, "SetFlashlightPosition( %3.2f, %3.2f, %3.2f )",
				pos.x, pos.y, pos.z );
			textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
			textBuf.AddToTail( '\n' );

			CRenderCaptureConfigurationState *pFlashlightInfo = reinterpret_cast<CRenderCaptureConfigurationState *>( m_pItemPreviewRenderer->GetRenderingWithFlashlightConfiguration () );

			if ( pFlashlightInfo )
			{
				V_sprintf_safe( tempText, "SetFlashlightFOV( %3.1f )",
					pFlashlightInfo->m_renderFlashlightState.m_fVerticalFOVDegrees );
				textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
				textBuf.AddToTail( '\n' );

				V_sprintf_safe( tempText, "SetFlashlightNearFarZ( %3.1f, %3.1f )",
					pFlashlightInfo->m_renderFlashlightState.m_NearZ, pFlashlightInfo->m_renderFlashlightState.m_FarZ );
				textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
				textBuf.AddToTail( '\n' );
			}

			GetFlashlightAngle( ang );
			V_sprintf_safe( tempText, "SetFlashlightAngles( %3.2f, %3.2f, %3.2f )",
				ang.x, ang.y, ang.z );
			textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
			textBuf.AddToTail ( '\n' );

			GetFlashlightColor( col );
			V_sprintf_safe( tempText, "SetFlashlightColor( %3.2f, %3.2f, %3.2f )",
				col.x, col.y, col.z );
			textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
		}
		else if ( eSelectedLight == CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_AMBIENT )
		{
			GetAmbientLightColor( col );
			V_sprintf_safe( tempText, "SetAmbientLightColor( %3.2f, %3.2f, %3.2f )",
				col.x, col.y, col.z );
			textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
		}
		else
		{
			int idx = eSelectedLight - CUI_ItemPreviewDebug::DEBUGMODE_LIGHT_DIR0;

			GetDirectionalLightColor( idx, col );
			V_sprintf_safe( tempText, "SetDirectionalLightColor( %3.2f, %3.2f, %3.2f )",
				col.x, col.y, col.z );
			textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );

			GetDirectionalLightDirection ( idx, dir );
			V_sprintf_safe ( tempText, "SetDirectionalLightDirection( %3.2f, %3.2f, %3.2f )",
				dir.x, dir.y, dir.z );
			textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );

			GetDirectionalLightRotation( idx, rot );
			V_sprintf_safe( tempText, "SetDirectionalLightRotation( %3.2f, %3.2f, %3.2f )",
				rot.x, rot.y, rot.z );
			textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );

			GetDirectionalLightPulseFlicker( idx, pulseFlicker );
			V_sprintf_safe( tempText, "SetDirectionalLightPulseFlicker( %3.2f, %3.2f, %3.2f, %3.2f )",
				pulseFlicker.x, pulseFlicker.y, pulseFlicker.z, pulseFlicker.w );
			textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
		}
	}

	textBuf.AddToTail( 0 );

//	vgui::system()->SetClipboardText( textBuf.Base(), textBuf.Count() );

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewPanel::CopyPresetCameraSettingsToClipboard()
{
	if ( !m_pItemPreviewRenderer )
		return false;

	CUtlVector< char > textBuf;

	Vector pos, pivot;
	QAngle angles;
	float fov;

	m_pItemPreviewRenderer->GetCameraPositionAndAngles( pos, angles );
	m_pItemPreviewRenderer->GetCameraPivotPosition( pivot );
	fov = m_pItemPreviewRenderer->GetCameraFOV();

	char tempText[ 512 ];
	V_sprintf_safe( tempText, "\"camera_preset_add\"   \"pos[%3.2f %3.2f %3.2f] pivot[%3.2f %3.2f %3.2f] orient[%3.2f %3.2f %3.2f] fov[%2.1f]\"\n",
					pos.x, pos.y, pos.z, pivot.x, pivot.y, pivot.z, angles.x, angles.y, angles.z, fov );

	textBuf.AddMultipleToTail( V_strlen( tempText ), tempText );
	textBuf.AddToTail( 0 );

//	vgui::system()->SetClipboardText( textBuf.Base(), textBuf.Count() );

	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewPanel::SetEconItemTextureSize( int nSize )
{
	if ( nSize > 1024 )
	{
		m_econItemTextureSize = COMPOSITE_TEXTURE_SIZE_2048;
	}
	else if ( nSize > 512 )
	{
		m_econItemTextureSize = COMPOSITE_TEXTURE_SIZE_1024;
	}
	else if ( nSize > 256 )
	{
		m_econItemTextureSize = COMPOSITE_TEXTURE_SIZE_512;
	}
	else
	{
		m_econItemTextureSize = COMPOSITE_TEXTURE_SIZE_256;
	}

	return true;
}

//-----------------------------------------------------------------------------
//
// Debug panel (for player camera and flashlight manipulation)
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// HSV sliders for debug panel
//-----------------------------------------------------------------------------
CUI_ItemPreviewColorSlider::CUI_ItemPreviewColorSlider( panorama::CPanel2D *pParent, const char *pchID )
	:
	CPanel2D( pParent, pchID )
{
	m_pSlider = new CSlider( this, "Slider" );
	
	m_flMinVal = 0.0f;
	m_flMaxVal = 0.0f;

	m_pSlider->SetDirection( CSlider::k_EDirectionHorizontal );
	m_pSlider->SetIncrement( 0.01f );

	SetValue( 0.0f );
	RegisterEventHandler( SliderValueChanged(), this, &CUI_ItemPreviewColorSlider::EventSliderValueChanged );
}

CUI_ItemPreviewColorSlider::~CUI_ItemPreviewColorSlider()
{
}

bool CUI_ItemPreviewColorSlider::EventSliderValueChanged( const CPanelPtr< IUIPanel > &pPanel, float flValue )
{
	// update light color from HSV slider values
	m_pItemPreviewDebug->SetDebugLightFromHSV();

	// update HSV slider colors
	m_pItemPreviewDebug->SetHSVFromDebugLight();

	return true;
}

void CUI_ItemPreviewColorSlider::OnShow()
{
	if ( m_flMinVal == m_flMaxVal )
	{
		return;	// Cannot use slider if min and max val are the same
	}
}

//-----------------------------------------------------------------------------
// HSV sliders for debug panel
//-----------------------------------------------------------------------------
CUI_ItemPreviewSlider::CUI_ItemPreviewSlider( panorama::CPanel2D *pParent, const char *pchID )
	:
	CPanel2D( pParent, pchID )
{
	m_pSlider = new CSlider( this, "Slider" );

	m_flMinVal = 0.0f;
	m_flMaxVal = 0.0f;

	m_pSlider->SetDirection( CSlider::k_EDirectionHorizontal );
	m_pSlider->SetIncrement( 0.01f );

	SetValue( 0.0f );
	RegisterEventHandler( SliderValueChanged(), this, &CUI_ItemPreviewSlider::EventSliderValueChanged );
}

CUI_ItemPreviewSlider::~CUI_ItemPreviewSlider()
{
}

bool CUI_ItemPreviewSlider::EventSliderValueChanged( const CPanelPtr< IUIPanel > &pPanel, float flValue )
{
	// update light animation values
	if ( m_type == ITEMPREVIEWSLIDER_ANIMATEDLIGHT )
		m_pItemPreviewDebug->SetAnimatedLightsFromUI();

	// update flashlight shadow values
	if ( m_type == ITEMPREVIEWSLIDER_FLASHLIGHTSHADOW )
		m_pItemPreviewDebug->SetFlashlightShadowFromUI();

	// update camera fov values
	if ( m_type == ITEMPREVIEWSLIDER_CAMERA )
		m_pItemPreviewDebug->SetCameraFOVFromUI();

	return true;
}

void CUI_ItemPreviewSlider::OnShow()
{
	if ( m_flMinVal == m_flMaxVal )
	{
		return;	// Cannot use slider if min and max val are the same
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUI_ItemPreviewDebug::CUI_ItemPreviewDebug( CPanel2D *pParent, const char *pchID ) : CPanel2D( pParent, pchID ), m_scheduledUpdate( MAKE_SCHEDULED_FUNC( CUI_ItemPreviewDebug::Update ) )
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/itempreviewdebug.xml", true ) );

	RegisterEventHandler( UIItemPreviewPanelSceneReload(), this, &CUI_ItemPreviewDebug::SceneReload );
	RegisterEventHandler( UIItemPreviewPanelCloseDebugCamera(), this, &CUI_ItemPreviewDebug::CloseDebugCamera );
	RegisterEventHandler( UIItemPreviewPanelToggleDebugMode(), this, &CUI_ItemPreviewDebug::ToggleDebugMode );
	RegisterEventHandler( UIItemPreviewPanelCopyToClipboard(), this, &CUI_ItemPreviewDebug::CopyToClipboard );
	RegisterEventHandler( UIItemPreviewPanelCopyPresetToClipboard (), this, &CUI_ItemPreviewDebug::CopyCameraAsPresetToClipboard );
	RegisterEventHandler( UIItemPreviewPanelDebugLightSelection(), this, &CUI_ItemPreviewDebug::SelectDebugLight );
	RegisterEventHandler( UIItemPreviewPanelDebugToggleSection_LightColor(), this, &CUI_ItemPreviewDebug::ToggleSection_LightColor );
	RegisterEventHandler( UIItemPreviewPanelDebugToggleSection_LightAnim(), this, &CUI_ItemPreviewDebug::ToggleSection_LightAnim );
	RegisterEventHandler( UIItemPreviewPanelDebugToggleSection_LightFlashlightShadow(), this, &CUI_ItemPreviewDebug::ToggleSection_LightFlashlightShadow );
	RegisterEventHandler( UIItemPreviewPanelDebugResetLightAnim_RotX(), this, &CUI_ItemPreviewDebug::ResetLightAnim_RotX );
	RegisterEventHandler( UIItemPreviewPanelDebugResetLightAnim_RotY(), this, &CUI_ItemPreviewDebug::ResetLightAnim_RotY );
	RegisterEventHandler( UIItemPreviewPanelDebugResetLightAnim_RotZ(), this, &CUI_ItemPreviewDebug::ResetLightAnim_RotZ );
	RegisterEventHandler( UIItemPreviewPanelToggleEnabledLights(), this, &CUI_ItemPreviewDebug::ToggleEnabledLights );

	m_pDebugModeLightPanel = RequirePanelInLayoutFile( "LightMode" );

	m_pDebugModeTooltipLabelCamera = panel_cast<CLabel*>( RequirePanelInLayoutFile( "ToggleDebugModeLabelCamera" ) );
	m_pDebugModeTooltipLabelLights = panel_cast<CLabel*>( RequirePanelInLayoutFile( "ToggleDebugModeLabelLights" ) );

	m_pDebugModeLightSelectionLabel = panel_cast<CLabel *>( RequirePanelInLayoutFile( "LightSelectionLabel" ) );
	m_pDebugLightSelectionDropDown = panel_cast<CDropDown*>( RequirePanelInLayoutFile( "LightSelectionEnum" ) );

	m_pDebugModeEnabledLightsToggleButton = RequirePanelInLayoutFile ( "EnabledLightsToggleButton" );
	m_pDebugModeLabelEnabledAllLights = panel_cast<CLabel*>( RequirePanelInLayoutFile ( "ToggleLightsLabelAll" ) );
	m_pDebugModeLabelEnabledSelectedLight = panel_cast<CLabel*>( RequirePanelInLayoutFile ( "ToggleLightsLabelSelected" ) );

	m_pDebugModeLightColorSettings = RequirePanelInLayoutFile( "LightColorSettings" );
	m_pDebugModeLightAnimSettings = RequirePanelInLayoutFile( "LightAnimSettings" );
	m_pDebugModeLightFlashlightShadowSettings = RequirePanelInLayoutFile ( "LightFlashlightShadowSettings" );
	m_pDebugModeCameraFOVSettings = RequirePanelInLayoutFile ( "CameraFOVSettings" );

	m_pCameraAsPresetButton = panel_cast<CButton *>( RequirePanelInLayoutFile( "ItemPreviewCopyAsPresetButton" ) );

	// HSV sliders
	m_pLightColorSlider_H = panel_cast<CUI_ItemPreviewColorSlider*>( RequirePanelInLayoutFile( "ColorPick_HueSlider" ) );
	m_pLightColorSlider_S = panel_cast<CUI_ItemPreviewColorSlider*>( RequirePanelInLayoutFile( "ColorPick_SatSlider" ) );
	m_pLightColorSlider_V = panel_cast<CUI_ItemPreviewColorSlider*>( RequirePanelInLayoutFile( "ColorPick_ValSlider" ) );

	// rgb box
	m_pLightColorRGBBox = RequirePanelInLayoutFile( "ColorPick_RGBBox" );

	// HDR
	m_pLightColorSlider_HDR = panel_cast<CUI_ItemPreviewColorSlider*>( RequirePanelInLayoutFile( "ColorPick_HDRSlider" ) );

	m_pLightColorSlider_H->SetItemPreviewDebugPanel( this );
	m_pLightColorSlider_S->SetItemPreviewDebugPanel( this );
	m_pLightColorSlider_V->SetItemPreviewDebugPanel( this );
	m_pLightColorSlider_HDR->SetItemPreviewDebugPanel( this );

	m_pLightColorSlider_HDR->SetMin( 1.0f );
	m_pLightColorSlider_HDR->SetMax( 16.0f );

	m_pLightColorLabel_HDR = panel_cast<CLabel *>( RequireChildInLayoutFile( "ColorPick_HDRLabel" ) );

	// dropdown sections
	m_pLightColorDropdown = RequireChildInLayoutFile( "LightColorSection" );
	m_pLightColorDropdownIcon = panel_cast<CImagePanel *>( RequireChildInLayoutFile( "LightColorSectionOpen" ) );
	m_pLightAnimDropdown = RequireChildInLayoutFile( "LightAnimSection" );
	m_pLightAnimDropdownIcon = panel_cast<CImagePanel *>( RequireChildInLayoutFile( "LightAnimSectionOpen" ) );
	m_pLightFlashlightShadowDropdown = RequireChildInLayoutFile ( "LightFlashlightShadowSection" );
	m_pLightFlashlightShadowDropdownIcon = panel_cast<CImagePanel *>( RequireChildInLayoutFile ( "LightFlashlightShadowSectionOpen" ) );

	m_pLightColorDropdown->SetHasClass( "LightSection--Closed", false );
	m_pLightColorDropdownIcon->SetImage( "file://{images}/control_icons/arrow_solid_down.vtf" );
	m_pDebugModeLightColorSettings->SetVisible( true );

	m_pLightAnimDropdown->SetHasClass( "LightSection--Closed", true );
	m_pLightAnimDropdownIcon->SetImage( "file://{images}/control_icons/arrow_solid_left.vtf" );
	m_pDebugModeLightAnimSettings->SetVisible( false );

	m_pLightFlashlightShadowDropdown->SetHasClass ( "LightSection--Closed", true );
	m_pLightFlashlightShadowDropdownIcon->SetImage ( "file://{images}/control_icons/arrow_solid_left.vtf" );
	m_pDebugModeLightFlashlightShadowSettings->SetVisible ( false );

	m_pDebugModeCameraFOVSettings->SetVisible( true );

	// anim sliders
	m_pDebugModeLightRot = RequirePanelInLayoutFile( "LightRot" );
	m_pDebugModeLightPulse = RequirePanelInLayoutFile( "LightPulse" );
	m_pDebugModeLightFlicker = RequirePanelInLayoutFile( "LightFlicker" );

	m_pLightRotLabel_X = panel_cast<CLabel *>( RequireChildInLayoutFile( "LightRotX_Label" ) );
	m_pLightRotLabel_Y = panel_cast<CLabel *>( RequireChildInLayoutFile( "LightRotY_Label" ) );
	m_pLightRotLabel_Z = panel_cast<CLabel *>( RequireChildInLayoutFile( "LightRotZ_Label" ) );

	m_pLightRotSlider_X = panel_cast<CUI_ItemPreviewSlider*>( RequirePanelInLayoutFile( "LightRotX_Slider" ) );
	m_pLightRotSlider_Y = panel_cast<CUI_ItemPreviewSlider*>( RequirePanelInLayoutFile( "LightRotY_Slider" ) );
	m_pLightRotSlider_Z = panel_cast<CUI_ItemPreviewSlider*>( RequirePanelInLayoutFile( "LightRotZ_Slider" ) );

	m_pLightPulseSlider_A = panel_cast<CUI_ItemPreviewSlider*>( RequirePanelInLayoutFile( "LightPulseA_Slider" ) );
	m_pLightPulseSlider_F = panel_cast<CUI_ItemPreviewSlider*>( RequirePanelInLayoutFile( "LightPulseF_Slider" ) );

	m_pLightFlickerSlider_A = panel_cast<CUI_ItemPreviewSlider*>( RequirePanelInLayoutFile( "LightFlickerA_Slider" ) );
	m_pLightFlickerSlider_F = panel_cast<CUI_ItemPreviewSlider*>( RequirePanelInLayoutFile( "LightFlickerF_Slider" ) );

	m_pLightRotSlider_X->SetType( CUI_ItemPreviewSlider::ITEMPREVIEWSLIDER_ANIMATEDLIGHT );
	m_pLightRotSlider_Y->SetType( CUI_ItemPreviewSlider::ITEMPREVIEWSLIDER_ANIMATEDLIGHT );
	m_pLightRotSlider_Z->SetType( CUI_ItemPreviewSlider::ITEMPREVIEWSLIDER_ANIMATEDLIGHT );
	m_pLightPulseSlider_A->SetType( CUI_ItemPreviewSlider::ITEMPREVIEWSLIDER_ANIMATEDLIGHT );
	m_pLightPulseSlider_F->SetType( CUI_ItemPreviewSlider::ITEMPREVIEWSLIDER_ANIMATEDLIGHT );
	m_pLightFlickerSlider_A->SetType( CUI_ItemPreviewSlider::ITEMPREVIEWSLIDER_ANIMATEDLIGHT );
	m_pLightFlickerSlider_F->SetType( CUI_ItemPreviewSlider::ITEMPREVIEWSLIDER_ANIMATEDLIGHT );

	m_pLightRotSlider_X->SetItemPreviewDebugPanel( this );
	m_pLightRotSlider_Y->SetItemPreviewDebugPanel( this );
	m_pLightRotSlider_Z->SetItemPreviewDebugPanel( this );
	m_pLightPulseSlider_A->SetItemPreviewDebugPanel( this );
	m_pLightPulseSlider_F->SetItemPreviewDebugPanel( this );
	m_pLightFlickerSlider_A->SetItemPreviewDebugPanel( this );
	m_pLightFlickerSlider_F->SetItemPreviewDebugPanel( this );

	m_pLightRotSlider_X->SetMin( -360.0f );
	m_pLightRotSlider_X->SetMax( 360.0f );
	m_pLightRotSlider_X->SetDefaultValue( 0.0f );
	m_pLightRotSlider_X->SetShowDefaultValue( true );
	m_pLightRotSlider_Y->SetMin( -360.0f );
	m_pLightRotSlider_Y->SetMax( 360.0f );
	m_pLightRotSlider_Y->SetDefaultValue( 0.0f );
	m_pLightRotSlider_Y->SetShowDefaultValue( true );
	m_pLightRotSlider_Z->SetMin( -360.0f );
	m_pLightRotSlider_Z->SetMax( 360.0f );
	m_pLightRotSlider_Z->SetDefaultValue( 0.0f );
	m_pLightRotSlider_Z->SetShowDefaultValue( true );

	m_pLightPulseSlider_A->SetMin( 0.0f );
	m_pLightPulseSlider_A->SetMax( 1.0f );
	m_pLightPulseSlider_F->SetMin( 0.0f );
	m_pLightPulseSlider_F->SetMax( 4.0f );

	m_pLightFlickerSlider_A->SetMin( 0.0f );
	m_pLightFlickerSlider_A->SetMax( 1.0f );
	m_pLightFlickerSlider_F->SetMin( 0.0f );
	m_pLightFlickerSlider_F->SetMax( 4.0f );

	// flashlight shadow sliders
	m_pFlashlightShadowFOVSlider = panel_cast<CUI_ItemPreviewSlider*>( RequirePanelInLayoutFile ( "FlashlightShadow_FOVSlider" ) );
	m_pFlashlightShadowFOVSlider->SetType ( CUI_ItemPreviewSlider::ITEMPREVIEWSLIDER_FLASHLIGHTSHADOW );
	m_pFlashlightShadowFOVLabel = panel_cast<CLabel *>( RequireChildInLayoutFile ( "FlashlightShadow_FOVLabel" ) );
	m_pFlashlightShadowFOVSlider->SetItemPreviewDebugPanel( this );

	m_pFlashlightShadowFOVSlider->SetMin( 1.0f );
	m_pFlashlightShadowFOVSlider->SetMax( 70.0f );
	m_pFlashlightShadowFOVSlider->SetDefaultValue( 54.0f );
	m_pFlashlightShadowFOVSlider->SetShowDefaultValue( true );
	m_pFlashlightShadowFOVSlider->SetIncrement( 1.0f );

	m_pFlashlightShadowNearZSlider = panel_cast<CUI_ItemPreviewSlider*>( RequirePanelInLayoutFile( "FlashlightShadow_NearZSlider" ) );
	m_pFlashlightShadowNearZSlider->SetType( CUI_ItemPreviewSlider::ITEMPREVIEWSLIDER_FLASHLIGHTSHADOW );
	m_pFlashlightShadowNearZLabel = panel_cast<CLabel *>( RequireChildInLayoutFile( "FlashlightShadow_NearZLabel" ) );
	m_pFlashlightShadowNearZSlider->SetItemPreviewDebugPanel( this );

	m_pFlashlightShadowNearZSlider->SetMin( 0.0f );
	m_pFlashlightShadowNearZSlider->SetMax( 512.0f );
	m_pFlashlightShadowNearZSlider->SetDefaultValue( 4.0f );
	m_pFlashlightShadowNearZSlider->SetShowDefaultValue( true );
	m_pFlashlightShadowNearZSlider->SetIncrement( 2.0f );

	m_pFlashlightShadowFarZSlider = panel_cast<CUI_ItemPreviewSlider*>( RequirePanelInLayoutFile( "FlashlightShadow_FarZSlider" ) );
	m_pFlashlightShadowFarZSlider->SetType( CUI_ItemPreviewSlider::ITEMPREVIEWSLIDER_FLASHLIGHTSHADOW );
	m_pFlashlightShadowFarZLabel = panel_cast<CLabel *>( RequireChildInLayoutFile( "FlashlightShadow_FarZLabel" ) );
	m_pFlashlightShadowFarZSlider->SetItemPreviewDebugPanel( this );

	m_pFlashlightShadowFarZSlider->SetMin( 0.0f );
	m_pFlashlightShadowFarZSlider->SetMax( 512.0f );
	m_pFlashlightShadowFarZSlider->SetDefaultValue( 512.0f );
	m_pFlashlightShadowFarZSlider->SetShowDefaultValue( true );
	m_pFlashlightShadowFarZSlider->SetIncrement( 2.0f );

	// camera fov slider
	m_pCameraFOVSlider = panel_cast<CUI_ItemPreviewSlider*>( RequirePanelInLayoutFile( "Camera_FOVSlider" ) );
	m_pCameraFOVSlider->SetType( CUI_ItemPreviewSlider::ITEMPREVIEWSLIDER_CAMERA );
	m_pCameraFOVLabel = panel_cast<CLabel *>( RequireChildInLayoutFile( "Camera_FOVLabel" ) );
	m_pCameraFOVSlider->SetItemPreviewDebugPanel( this );

	m_pCameraFOVSlider->SetMin( 20.0f );
	m_pCameraFOVSlider->SetMax( 70.0f );
	m_pCameraFOVSlider->SetDefaultValue( 45.0f );
	m_pCameraFOVSlider->SetShowDefaultValue( true );
	m_pCameraFOVSlider->SetIncrement( 1.0f );

	m_pItemPreviewRenderer = NULL;

	m_debugMode = DEBUGMODE_LIGHTS;
	m_bEnableAllLights = true;
	ToggleDebugMode();
}

//-----------------------------------------------------------------------------
// Purpose: Sets video to show
//-----------------------------------------------------------------------------
void CUI_ItemPreviewDebug::Show()
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return;

	Update();
	m_scheduledUpdate.Schedule( 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUI_ItemPreviewDebug::Update()
{
	// 	Vector pos, pivot;
	// 	QAngle angles;
	// 
	// 	m_pItemPreviewRenderer->GetCameraPositionAndAngles( pos, angles );
	// 	m_pItemPreviewRenderer->GetCameraPivotPosition( pivot );
	// 	m_pCameraPos->SetText( CFmtStr("%3.2f, %3.2f, %3.2f", pos.x, pos.y, pos.z) );
	// 	m_pCameraAng->SetText( CFmtStr("%3.2f, %3.2f, %3.2f", angles.x, angles.y, angles.z) );
	// 	m_pOrbitPivot->SetText( CFmtStr( "%3.2f, %3.2f, %3.2f", pivot.x, pivot.y, pivot.z ) );

	//	m_scheduledUpdate.Schedule( 0.1f );
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return;
}


bool CUI_ItemPreviewDebug::SceneReload()
{ 
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return false;
	
	return m_pItemPreviewPanel->SceneReload(); 
}

bool CUI_ItemPreviewDebug::CloseDebugCamera()
{ 
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return false;
	
	return m_pItemPreviewPanel->CloseDebugCamera();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewDebug::ToggleDebugMode()
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return false;

	if ( m_debugMode == DEBUGMODE_CAMERA )
	{
		m_debugMode = DEBUGMODE_LIGHTS;
	}
	else
	{
		m_debugMode = DEBUGMODE_CAMERA;
	}

	SetupDebugMode();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewDebug::CopyToClipboard()
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return false;

	if ( GetMode() == eDebugMode::DEBUGMODE_CAMERA )
	{
		return m_pItemPreviewPanel->CopyDebugCameraSettingsToClipboard();
	}
	else
	{
		return m_pItemPreviewPanel->CopyAllLightSettingsToClipboard();
	}
}

bool CUI_ItemPreviewDebug::CopyCameraAsPresetToClipboard()
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return false;

	if ( GetMode() == eDebugMode::DEBUGMODE_CAMERA )
	{
		return m_pItemPreviewPanel->CopyPresetCameraSettingsToClipboard();
	}

	return false;
}

bool CUI_ItemPreviewDebug::CopyCameraAsFlashlightToClipboard()
{
	if ( !m_pItemPreviewPanel || !BIsVisible () )
		return false;

	if ( GetMode () == eDebugMode::DEBUGMODE_CAMERA )
	{
		return m_pItemPreviewPanel->CopyFlashlightSettingsToClipboard();
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUI_ItemPreviewDebug::SetupDebugMode()
{
	if ( !m_pItemPreviewRenderer || !BIsVisible() )
		return;

	if ( m_debugMode == DEBUGMODE_LIGHTS )
	{
		if ( m_pItemPreviewRenderer )
		{
			m_pItemPreviewRenderer->EnableDrawShadowFloor( true );
			m_pItemPreviewRenderer->EnableDrawWorldPivot( false );
			m_pItemPreviewRenderer->EnableDrawPresetCameras( false );
			m_pItemPreviewRenderer->EnableDrawDirectionaLight( -1 );

			if ( GetActiveDebugLight() == DEBUGMODE_LIGHT_FLASHLIGHT )
			{
				m_pItemPreviewRenderer->EnableDrawFlashlightPivot( true );
			}
			else
			{
				m_pItemPreviewRenderer->EnableDrawFlashlightPivot( false );
			}
		}

		m_pDebugModeCameraFOVSettings->SetVisible( false );

		m_pDebugModeTooltipLabelLights->SetVisible( true );
		m_pDebugLightSelectionDropDown->SetVisible( true );

		m_pDebugModeLightPanel->SetVisible( true );

		SelectDebugLight();

		m_pDebugModeTooltipLabelCamera->SetVisible( false );
		m_pCameraAsPresetButton->SetVisible( false );

		m_pDebugModeEnabledLightsToggleButton->SetVisible( true );
		m_pDebugModeLabelEnabledAllLights->SetVisible( m_bEnableAllLights );
		m_pDebugModeLabelEnabledSelectedLight->SetVisible ( !m_bEnableAllLights );
	}
	else
	{
		m_pDebugModeTooltipLabelCamera->SetVisible( true );

		if ( m_pItemPreviewRenderer )
		{
			m_pItemPreviewRenderer->EnableDrawShadowFloor( false );
			m_pItemPreviewRenderer->EnableDrawWorldPivot( true );
			m_pItemPreviewRenderer->EnableDrawFlashlightPivot ( false );
			m_pItemPreviewRenderer->EnableDrawPresetCameras( true );
			m_pItemPreviewRenderer->EnableDrawDirectionaLight( -1 );
		}

		m_pDebugModeCameraFOVSettings->SetVisible( true );

		m_pDebugModeTooltipLabelLights->SetVisible( false );
		m_pDebugLightSelectionDropDown->SetVisible( false );

		m_pDebugModeLightPanel->SetVisible( false );

		m_pCameraAsPresetButton->SetVisible( true );

		m_pDebugModeEnabledLightsToggleButton->SetVisible( false );
	}
}
//-----------------------------------------------------------------------------
// Purpose: toggle enabled lights (all vs selected)
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewDebug::ToggleEnabledLights()
{
	if ( !m_pItemPreviewPanel || !BIsVisible () )
		return true;

	m_bEnableAllLights = !m_bEnableAllLights;

	m_pDebugModeLabelEnabledAllLights->SetVisible( m_bEnableAllLights );
	m_pDebugModeLabelEnabledSelectedLight->SetVisible( !m_bEnableAllLights );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewDebug::ToggleSection_LightColor()
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return false;

	if ( GetMode() != DEBUGMODE_LIGHTS )
		return false;

	if ( m_pDebugModeLightColorSettings->BIsVisible() )
	{
		// label color
		m_pLightColorDropdown->SetHasClass( "LightSection--Closed", true );

		// arrow 
		m_pLightColorDropdownIcon->SetImage( "file://{images}/control_icons/arrow_solid_left.vtf" );

		// block
		m_pDebugModeLightColorSettings->SetVisible( false );
	}
	else
	{
		// label color
		m_pLightColorDropdown->SetHasClass( "LightSection--Closed", false );

		// arrow 
		m_pLightColorDropdownIcon->SetImage( "file://{images}/control_icons/arrow_solid_down.vtf" );

		// block
		m_pDebugModeLightColorSettings->SetVisible( true );
	}

	return true;
}

bool CUI_ItemPreviewDebug::ToggleSection_LightAnim()
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return false;

	if ( ( GetMode() != DEBUGMODE_LIGHTS ) || ( GetActiveDebugLight() == DEBUGMODE_LIGHT_NONE ) )
		return false;

	if ( m_pDebugModeLightAnimSettings->BIsVisible() )
	{
		// label color
		m_pLightAnimDropdown->SetHasClass( "LightSection--Closed", true );

		// arrow 
		m_pLightAnimDropdownIcon->SetImage( "file://{images}/control_icons/arrow_solid_left.vtf" );

		// block
		m_pDebugModeLightAnimSettings->SetVisible( false );
	}
	else if ( GetActiveDebugLight() != DEBUGMODE_LIGHT_AMBIENT )
	{
		// label color
		m_pLightAnimDropdown->SetHasClass( "LightSection--Closed", false );

		// arrow 
		m_pLightAnimDropdownIcon->SetImage( "file://{images}/control_icons/arrow_solid_down.vtf" );

		// block
		m_pDebugModeLightAnimSettings->SetVisible( true );
	}

	return true;
}

bool CUI_ItemPreviewDebug::ToggleSection_LightFlashlightShadow()
{
	if ( !m_pItemPreviewPanel || !BIsVisible () )
		return false;

	if ( ( GetMode() != DEBUGMODE_LIGHTS ) || ( GetActiveDebugLight() == DEBUGMODE_LIGHT_NONE ) )
		return false;

	if ( m_pDebugModeLightFlashlightShadowSettings->BIsVisible() )
	{
		// label color
		m_pLightFlashlightShadowDropdown->SetHasClass( "LightSection--Closed", true );

		// arrow 
		m_pLightFlashlightShadowDropdownIcon->SetImage( "file://{images}/control_icons/arrow_solid_left.vtf" );

		// block
		m_pDebugModeLightFlashlightShadowSettings->SetVisible( false );
	}
	else if ( GetActiveDebugLight() != DEBUGMODE_LIGHT_AMBIENT )
	{
		// label color
		m_pLightFlashlightShadowDropdown->SetHasClass( "LightSection--Closed", false );

		// arrow 
		m_pLightFlashlightShadowDropdownIcon->SetImage( "file://{images}/control_icons/arrow_solid_down.vtf" );

		// block
		m_pDebugModeLightFlashlightShadowSettings->SetVisible( true );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewDebug::ResetLightAnim_RotX()
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return false;

	if ( GetMode() != DEBUGMODE_LIGHTS )
		return false;

	m_pLightRotSlider_X->SetValue( 0.0f );

	return true;
}

bool CUI_ItemPreviewDebug::ResetLightAnim_RotY()
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return false;

	if ( GetMode() != DEBUGMODE_LIGHTS )
		return false;

	m_pLightRotSlider_Y->SetValue( 0.0f );

	return true;
}

bool CUI_ItemPreviewDebug::ResetLightAnim_RotZ()
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return false;

	if ( GetMode() != DEBUGMODE_LIGHTS )
		return false;

	m_pLightRotSlider_Z->SetValue( 0.0f );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CUI_ItemPreviewDebug::eDebugLight CUI_ItemPreviewDebug::GetActiveDebugLight()
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return DEBUGMODE_LIGHT_NONE;

	CPanel2D *pSelectedPanel = m_pDebugLightSelectionDropDown->GetSelected();

	if ( !pSelectedPanel )
		return DEBUGMODE_LIGHT_NONE;

	eDebugLight nLight = ( eDebugLight )pSelectedPanel->GetAttribute( "value", 0 );

	if ( ( nLight < DEBUGMODE_LIGHT_FLASHLIGHT ) || ( nLight > DEBUGMODE_LIGHT_AMBIENT ) )
		nLight = DEBUGMODE_LIGHT_NONE;

	return nLight;
}

//-----------------------------------------------------------------------------
// Converts from RGB space to HSV
// Assumes RGB in floats, and might be HDR
// returns HSV in floats [0,1] range
//-----------------------------------------------------------------------------	
void RGBToHSV( const Vector &rgb, Vector &hsv, float &hdrScale )
{
	// take hdr into account (normalize rgb first)
	hdrScale = 1.0f;
	Vector rgbN = rgb;

	if ( ( rgb.x > 1.0f ) || ( rgb.y > 1.0f ) || ( rgb.z > 1.0f ) )
	{
		hdrScale = MAX( rgb.x, MAX( rgb.y, rgb.z ) );
		rgbN *= 1.0f / hdrScale;
	}

	float M = MAX( rgbN.x, MAX( rgbN.y, rgbN.z ) );
	float C = M - MIN( rgbN.x, MIN( rgbN.y, rgbN.z ) );

	float H = 0.0f, S = 0.0f, V = 0.0f;

	// V
	V = M;

	// S
	if ( C > 0.0f )
	{
		S = C / V;

		// H
		if ( M == rgbN.x )
		{
 			H = fmod( ( rgbN.y - rgbN.z ) / C, 6.0f );
 		}
		else if ( M == rgbN.y )
		{
			H = ( ( rgbN.z - rgbN.x ) / C ) + 2.0f;
		}
		else
		{
			H = ( ( rgbN.x - rgbN.y ) / C ) + 4.0f;
		}
		// to get H in [0,1)
		H /= 6.0f;

		if ( H < 0.0f )
			H += 1.0f;
	}

	hsv.x = H;
	hsv.y = S;
	hsv.z = V;
}

void HSVToRGB( const Vector &hsv, const float hdrScale, Vector &rgb )
{
	float C = hsv.z * hsv.y;

	float H = hsv.x * 6.0f;

	float X = C * ( 1.0f - fabs( fmod( H, 2.0f ) - 1.0f ) );

	float R, G, B;

	if ( H <= 1.0f )
	{
		R = C;
		G = X;
		B = 0.0f;
	}
	else if ( H <= 2.0f )
	{
		R = X;
		G = C;
		B = 0.0f;
	}
	else if ( H <= 3.0f )
	{
		R = 0.0f;
		G = C;
		B = X;
	}
	else if ( H <= 4.0f )
	{
		R = 0.0f;
		G = X;
		B = C;
	}
	else if ( H <= 5.0f )
	{
		R = X;
		G = 0.0f;
		B = C;
	}
	else
	{
		R = C;
		G = 0.0f;
		B = X;
	}

	float m = hsv.z - C;

	rgb.x = R + m;
	rgb.y = G + m;
	rgb.z = B + m;

	rgb *= hdrScale;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUI_ItemPreviewDebug::UpdateHSVPanels( Vector colRGB, bool bUpdateSliderValues )
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return;

	Vector hsv;
	float hdrScale;

	RGBToHSV( colRGB, hsv, hdrScale );

//	pPanelStyle->SetBackgroundColor( "gradient( linear, 0% 0%, 100% 0%, from(#0000ffff), to(#00ff00ff) )" );
//	pPanelStyle->SetBackgroundColor( CFmtStr( "gradient( linear, 0% 0%, 100% 0%, from(#0000ffff), to(#00ff00ff) )" ) );

	IUIPanelStyle *pPanelStyle;

	Vector hsvTmp, rgbTmp[6];

	// Hue
	pPanelStyle = m_pLightColorSlider_H->AccessStyle();
	// default - pPanelStyle->SetBackgroundColor( "gradient( linear, 0% 0%, 100% 0%, from(#ff0000ff), color-stop(0.17,#ffff00ff), color-stop(0.33,#00ff00ff), color-stop(0.5,#00ffffff), color-stop(0.67,#0000ffff), color-stop(0.83,#ff00ffff), to(#000000ff) )" );
	hsvTmp = Vector( 0.0f, hsv.y, hsv.z );
	HSVToRGB( hsvTmp, 1.0f, rgbTmp[ 0 ] );
	hsvTmp.x = 0.17f;
	HSVToRGB( hsvTmp, 1.0f, rgbTmp[ 1 ] );
	hsvTmp.x = 0.33f;
	HSVToRGB( hsvTmp, 1.0f, rgbTmp[ 2 ] );
	hsvTmp.x = 0.5f;
	HSVToRGB( hsvTmp, 1.0f, rgbTmp[ 3 ] );
	hsvTmp.x = 0.67f;
	HSVToRGB( hsvTmp, 1.0f, rgbTmp[ 4 ] );
	hsvTmp.x = 0.83f;
	HSVToRGB( hsvTmp, 1.0f, rgbTmp[ 5 ] );
	rgbTmp[ 0 ] *= 255.0f;
	rgbTmp[ 1 ] *= 255.0f;
	rgbTmp[ 2 ] *= 255.0f;
	rgbTmp[ 3 ] *= 255.0f;
	rgbTmp[ 4 ] *= 255.0f;
	rgbTmp[ 5 ] *= 255.0f;
	pPanelStyle->SetBackgroundColor( CFmtStr( "gradient( linear, 0%% 0%%, 100%% 0%%, from(rgb(%d,%d,%d)), color-stop(0.17,rgb(%d,%d,%d)), color-stop(0.33,rgb(%d,%d,%d)), color-stop(0.5,rgb(%d,%d,%d)), color-stop(0.67,rgb(%d,%d,%d)), color-stop(0.83,rgb(%d,%d,%d)), to(rgb(%d,%d,%d)) )", 
											  (int)rgbTmp[ 0 ].x, (int)rgbTmp[ 0 ].y, (int)rgbTmp[ 0 ].z,
											  (int)rgbTmp[ 1 ].x, (int)rgbTmp[ 1 ].y, (int)rgbTmp[ 1 ].z,
											  (int)rgbTmp[ 2 ].x, (int)rgbTmp[ 2 ].y, (int)rgbTmp[ 2 ].z,
											  (int)rgbTmp[ 3 ].x, (int)rgbTmp[ 3 ].y, (int)rgbTmp[ 3 ].z,
											  (int)rgbTmp[ 4 ].x, (int)rgbTmp[ 4 ].y, (int)rgbTmp[ 4 ].z,
											  (int)rgbTmp[ 5 ].x, (int)rgbTmp[ 5 ].y, (int)rgbTmp[ 5 ].z,
											  (int)rgbTmp[ 0 ].x, (int)rgbTmp[ 0 ].y, (int)rgbTmp[ 0 ].z ) );

	// Saturation
	pPanelStyle = m_pLightColorSlider_S->AccessStyle();
	hsvTmp = Vector( hsv.x, 0.5f, hsv.z );
	HSVToRGB( hsvTmp, 1.0f, rgbTmp[ 0 ] );
	hsvTmp.y = 1.0f;
	HSVToRGB( hsvTmp, 1.0f, rgbTmp[ 1 ] );
	int V = (int)( hsv.z * 255.0f );
	rgbTmp[ 0 ] *= 255.0f;
	rgbTmp[ 1 ] *= 255.0f;
	pPanelStyle->SetBackgroundColor( CFmtStr( "gradient(linear, 0%% 0%%, 100%% 0%%, from(rgb(%d,%d,%d)), color-stop(0.5,rgb(%d,%d,%d)), to(rgb(%d,%d,%d)))",
											  V, V, V, 
											  (int)rgbTmp[0].x, (int)rgbTmp[0].y, (int)rgbTmp[0].z,
											  (int)rgbTmp[1].x, (int)rgbTmp[1].y, (int)rgbTmp[1].z ) );

	// Value
	pPanelStyle = m_pLightColorSlider_V->AccessStyle();
	hsvTmp = Vector( hsv.x, hsv.y, 0.5f );
	HSVToRGB( hsvTmp, 1.0f, rgbTmp[ 0 ] );
	hsvTmp.z = 1.0f;
	HSVToRGB( hsvTmp, 1.0f, rgbTmp[ 1 ] );
	rgbTmp[ 0 ] *= 255.0f;
	rgbTmp[ 1 ] *= 255.0f;
	pPanelStyle->SetBackgroundColor( CFmtStr( "gradient(linear, 0%% 0%%, 100%% 0%%, from(rgb(0,0,0)), color-stop(0.5,rgb(%d,%d,%d)), to(rgb(%d,%d,%d)))",
											  (int)rgbTmp[ 0 ].x, (int)rgbTmp[ 0 ].y, (int)rgbTmp[ 0 ].z,
											  (int)rgbTmp[ 1 ].x, (int)rgbTmp[ 1 ].y, (int)rgbTmp[ 1 ].z ) );

	// RGB Box
	pPanelStyle = m_pLightColorRGBBox->AccessStyle();
	rgbTmp[ 0 ] = ( colRGB * 255.0f ) / hdrScale;
	Color c( rgbTmp[0].x, rgbTmp[0].y, rgbTmp[0].z, 255 );
	pPanelStyle->SetSimpleBackgroundColor( c );

	if ( bUpdateSliderValues )
	{
		m_pLightColorSlider_H->SetValue( hsv.x );
		m_pLightColorSlider_S->SetValue( hsv.y );
		m_pLightColorSlider_V->SetValue( hsv.z );
		m_pLightColorSlider_HDR->SetValue( hdrScale );
	}

	// HDR scale
	pPanelStyle = m_pLightColorSlider_HDR->AccessStyle();
	pPanelStyle->SetBackgroundColor( "gradient( linear, 0% 0%, 100% 0%, from(#000000ff), to(#7f7f7fff) )" );

	m_pLightColorLabel_HDR->SetText( CFmtStr( "HDR: %1.2f", m_pLightColorSlider_HDR->GetValue() ) );// hdrScale ) );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUI_ItemPreviewDebug::UpdateLightAnimPanels( QAngle rot, Vector4D pulseFlicker, bool bUpdateSliderValues )
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return;

	if ( bUpdateSliderValues )
	{
		m_pLightRotSlider_X->SetValue( rot.x );
		m_pLightRotSlider_Y->SetValue( rot.y );
		m_pLightRotSlider_Z->SetValue( rot.z );

		m_pLightPulseSlider_F->SetValue( pulseFlicker.x );
		m_pLightPulseSlider_A->SetValue( pulseFlicker.y );

		m_pLightFlickerSlider_F->SetValue( pulseFlicker.z );
		m_pLightFlickerSlider_A->SetValue( pulseFlicker.w );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUI_ItemPreviewDebug::UpdateFlashlightShadowPanels( float flFOV, float flNearZ, float flFarZ, bool bUpdateSliderValues )
{
	if ( !m_pItemPreviewPanel || !BIsVisible () )
		return;

	if ( bUpdateSliderValues )
	{
		m_pFlashlightShadowFOVSlider->SetValue( flFOV );
		m_pFlashlightShadowNearZSlider->SetValue( flNearZ );
		m_pFlashlightShadowFarZSlider->SetValue( flFarZ );
	}

	m_pFlashlightShadowFOVLabel->SetText( CFmtStr ( "fov: %2.0f", m_pFlashlightShadowFOVSlider->GetValue() ) );
	m_pFlashlightShadowNearZLabel->SetText( CFmtStr( "nrZ: %3.0f", m_pFlashlightShadowNearZSlider->GetValue() ) );
	m_pFlashlightShadowFarZLabel->SetText( CFmtStr( "frZ: %3.0f", m_pFlashlightShadowFarZSlider->GetValue() ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CUI_ItemPreviewDebug::SelectDebugLight()
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return false; 

	CUI_ItemPreviewDebug::eDebugLight nLight = GetActiveDebugLight();

	Vector col;
	QAngle rot;
	Vector4D pulseFlicker;

	switch ( nLight )
	{
	case DEBUGMODE_LIGHT_FLASHLIGHT:
		{
			float flFOV, flNearZ, flFarZ;

			CRenderCaptureConfigurationState *pFlashlightInfo = reinterpret_cast<CRenderCaptureConfigurationState *>( m_pItemPreviewRenderer->GetRenderingWithFlashlightConfiguration() );
	
			if ( pFlashlightInfo )
			{
				col.x = pFlashlightInfo->m_renderFlashlightState.m_Color[ 0 ];
				col.y = pFlashlightInfo->m_renderFlashlightState.m_Color[ 1 ];
				col.z = pFlashlightInfo->m_renderFlashlightState.m_Color[ 2 ];

				m_pItemPreviewPanel->GetFlashlightRotation( rot );
				m_pItemPreviewPanel->GetFlashlightPulseFlicker( pulseFlicker );

				flFOV = pFlashlightInfo->m_renderFlashlightState.m_fVerticalFOVDegrees;
				flNearZ = pFlashlightInfo->m_renderFlashlightState.m_NearZ;
				flFarZ = pFlashlightInfo->m_renderFlashlightState.m_FarZ;
			}
			m_pItemPreviewRenderer->EnableDrawDirectionaLight( -1 );
			m_pItemPreviewRenderer->EnableDrawShadowFloor( true );

			UpdateHSVPanels( col, true );
			UpdateLightAnimPanels( rot, pulseFlicker, true );
			UpdateFlashlightShadowPanels ( flFOV, flNearZ, flFarZ, true );

			m_pItemPreviewRenderer->EnableDrawFlashlightPivot ( true );
		}
		break;
	case DEBUGMODE_LIGHT_AMBIENT:
		m_pItemPreviewRenderer->GetLightAmbient( col );

		m_pItemPreviewRenderer->EnableDrawDirectionaLight( -1 );
		m_pItemPreviewRenderer->EnableDrawShadowFloor( false );

		// ensure light anim section is disabled, not yet supported
		m_pLightAnimDropdown->SetHasClass( "LightSection--Closed", true );
		m_pLightAnimDropdownIcon->SetImage( "file://{images}/control_icons/arrow_solid_left.vtf" );
		m_pDebugModeLightAnimSettings->SetVisible( false );

		// ensure light flashlight shadow section is disabled
		m_pLightFlashlightShadowDropdown->SetHasClass( "LightSection--Closed", true );
		m_pLightFlashlightShadowDropdownIcon->SetImage ( "file://{images}/control_icons/arrow_solid_left.vtf" );
		m_pDebugModeLightFlashlightShadowSettings->SetVisible ( false );

		m_pItemPreviewRenderer->EnableDrawFlashlightPivot ( false );

		UpdateHSVPanels( col, true );

		break;
	case DEBUGMODE_LIGHT_DIR0:
	case DEBUGMODE_LIGHT_DIR1:
	case DEBUGMODE_LIGHT_DIR2:
	case DEBUGMODE_LIGHT_DIR3:
		m_pItemPreviewRenderer->GetDirectionalLight_InitialCol( nLight - DEBUGMODE_LIGHT_DIR0, col );
		m_pItemPreviewRenderer->EnableDrawDirectionaLight( nLight - DEBUGMODE_LIGHT_DIR0 );
		m_pItemPreviewRenderer->EnableDrawShadowFloor( false );

		m_pItemPreviewPanel->GetDirectionalLightRotation( nLight - DEBUGMODE_LIGHT_DIR0, rot );
		m_pItemPreviewPanel->GetDirectionalLightPulseFlicker( nLight - DEBUGMODE_LIGHT_DIR0, pulseFlicker );

		// ensure light flashlight shadow section is disabled
		m_pLightFlashlightShadowDropdown->SetHasClass ( "LightSection--Closed", true );
		m_pLightFlashlightShadowDropdownIcon->SetImage ( "file://{images}/control_icons/arrow_solid_left.vtf" );
		m_pDebugModeLightFlashlightShadowSettings->SetVisible ( false );

		m_pItemPreviewRenderer->EnableDrawFlashlightPivot ( false );

		UpdateHSVPanels( col, true );
		UpdateLightAnimPanels( rot, pulseFlicker, true );

		break;
	default:
		break;
	}


	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUI_ItemPreviewDebug::SetDebugLightFromHSV()
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return;

	Vector hsv, rgb;

	hsv.x = m_pLightColorSlider_H->GetValue();
	hsv.y = m_pLightColorSlider_S->GetValue();
	hsv.z = m_pLightColorSlider_V->GetValue();
	float hdrScale = m_pLightColorSlider_HDR->GetValue();

	HSVToRGB( hsv, hdrScale, rgb );

	CUI_ItemPreviewDebug::eDebugLight nLight = GetActiveDebugLight();

	switch ( nLight )
	{
	case DEBUGMODE_LIGHT_FLASHLIGHT:
	{
		m_pItemPreviewPanel->SetFlashlightColor( rgb.x, rgb.y, rgb.z );
	}
	break;
	case DEBUGMODE_LIGHT_AMBIENT:
		m_pItemPreviewPanel->SetAmbientLightColor( rgb.x, rgb.y, rgb.z );
		break;
	case DEBUGMODE_LIGHT_DIR0:
	case DEBUGMODE_LIGHT_DIR1:
	case DEBUGMODE_LIGHT_DIR2:
	case DEBUGMODE_LIGHT_DIR3:
		m_pItemPreviewPanel->SetDirectionalLightModify( nLight - DEBUGMODE_LIGHT_DIR0 );
		m_pItemPreviewPanel->SetDirectionalLightColor( rgb.x, rgb.y, rgb.z );
		break;
	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUI_ItemPreviewDebug::SetHSVFromDebugLight()
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return;

	CUI_ItemPreviewDebug::eDebugLight nLight = GetActiveDebugLight();

	Vector col(0.0f, 0.0f, 0.0f);

	switch ( nLight )
	{
	case DEBUGMODE_LIGHT_FLASHLIGHT:
	{
		m_pItemPreviewPanel->GetFlashlightColor( col );
	}
	break;
	case DEBUGMODE_LIGHT_AMBIENT:
		m_pItemPreviewPanel->GetAmbientLightColor( col );
		break;
	case DEBUGMODE_LIGHT_DIR0:
	case DEBUGMODE_LIGHT_DIR1:
	case DEBUGMODE_LIGHT_DIR2:
	case DEBUGMODE_LIGHT_DIR3:
		m_pItemPreviewPanel->GetDirectionalLightColor( nLight - DEBUGMODE_LIGHT_DIR0, col );
		break;
	default:
		break;
	}

	UpdateHSVPanels( col );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUI_ItemPreviewDebug::SetAnimatedLightsFromUI()
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return;

	if ( !m_pItemPreviewPanel )
		return;

	float rotX = m_pLightRotSlider_X->GetValue();
	float rotY = m_pLightRotSlider_Y->GetValue();
	float rotZ = m_pLightRotSlider_Z->GetValue();

	float pulseA = m_pLightPulseSlider_A->GetValue();
	float pulseF = m_pLightPulseSlider_F->GetValue();

	float flickerA = m_pLightFlickerSlider_A->GetValue();
	float flickerF = m_pLightFlickerSlider_F->GetValue();

	CUI_ItemPreviewDebug::eDebugLight nLight = GetActiveDebugLight();

	switch ( nLight )
	{
	case DEBUGMODE_LIGHT_FLASHLIGHT:
		m_pItemPreviewPanel->SetFlashlightRotation( rotX, rotY, rotZ );
		m_pItemPreviewPanel->SetFlashlightPulseFlicker( pulseF, pulseA, flickerF, flickerA );
		break;
	case DEBUGMODE_LIGHT_AMBIENT:
		// not supported yet
		break;
	case DEBUGMODE_LIGHT_DIR0:
	case DEBUGMODE_LIGHT_DIR1:
	case DEBUGMODE_LIGHT_DIR2:
	case DEBUGMODE_LIGHT_DIR3:
		m_pItemPreviewPanel->SetDirectionalLightModify( nLight - DEBUGMODE_LIGHT_DIR0 );

		m_pItemPreviewPanel->SetDirectionalLightRotation( rotX, rotY, rotZ );
		m_pItemPreviewPanel->SetDirectionalLightPulseFlicker( pulseF, pulseA, flickerF, flickerA );
		break;
	default:
		break;
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUI_ItemPreviewDebug::SetFlashlightShadowFromUI()
{
	if ( !m_pItemPreviewPanel || !BIsVisible () )
		return;

	if ( !m_pItemPreviewRenderer )
		return;

	CRenderCaptureConfigurationState *pFlashlightInfo = reinterpret_cast<CRenderCaptureConfigurationState *>( m_pItemPreviewRenderer->GetRenderingWithFlashlightConfiguration () );

	if ( pFlashlightInfo )
	{
		float flNearZ = m_pFlashlightShadowNearZSlider->GetValue();
		float flFarZ = m_pFlashlightShadowFarZSlider->GetValue();

		// get tightest depth bounds if slider min >= slider max, otherwise keep slider values 
		if ( flNearZ >= flFarZ )
		{
			Vector pos;
			QAngle ang;
			m_pItemPreviewPanel->GetFlashlightAngle( ang );
			m_pItemPreviewPanel->GetFlashlightPosition( pos );

			matrix3x4_t mat;
			AngleMatrix( ang, pos, mat );

			float flSceneRadius;
			Vector vSceneCenter;

			m_pItemPreviewRenderer->GetSceneBounds( vSceneCenter, flSceneRadius );

			float flDist = mat.GetForward().Dot( vSceneCenter - pos );

			flNearZ = flDist - flSceneRadius;
			flFarZ = flDist + flSceneRadius;
		}

 		pFlashlightInfo->m_renderFlashlightState.m_NearZ = flNearZ;
 		pFlashlightInfo->m_renderFlashlightState.m_FarZ = flFarZ;
 		pFlashlightInfo->m_renderFlashlightState.m_FarZAtten = flFarZ * 2.0f;

		// FOV
		float flFOV = m_pFlashlightShadowFOVSlider->GetValue();

		pFlashlightInfo->m_renderFlashlightState.m_fVerticalFOVDegrees = flFOV;
		pFlashlightInfo->m_renderFlashlightState.m_fHorizontalFOVDegrees = flFOV;

		UpdateFlashlightShadowPanels( flFOV, flNearZ, flFarZ, false );
		m_pItemPreviewPanel->SetFlashlightFOV( flFOV );
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUI_ItemPreviewDebug::UpdateCameraFOVPanels( float flFOV, bool bUpdateSliderValues )
{
	if ( !m_pItemPreviewPanel )
		return;

	if ( bUpdateSliderValues )
	{
		m_pCameraFOVSlider->SetValue( flFOV );
	}

	m_pCameraFOVLabel->SetText( CFmtStr ( "fov: %1.0f", m_pCameraFOVSlider->GetValue() ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUI_ItemPreviewDebug::SetCameraFOVFromUI()
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return;

	if ( !m_pItemPreviewRenderer )
		return;


	float flFOV = m_pCameraFOVSlider->GetValue();

	UpdateCameraFOVPanels( flFOV, false );
	m_pItemPreviewRenderer->SetCameraFOV ( flFOV );
}

//-----------------------------------------------------------------------------
// Purpose: DebugPanel input
//-----------------------------------------------------------------------------

bool CUI_ItemPreviewDebug::OnKeyDown( const panorama::KeyData_t &unichar )
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return BaseClass::OnKeyDown( unichar );

	//return BaseClass::OnKeyDown( unichar );
	return m_pItemPreviewPanel->OnKeyDown( unichar );
}

bool CUI_ItemPreviewDebug::OnKeyUp( const panorama::KeyData_t &unichar )
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return BaseClass::OnKeyUp( unichar );

	return m_pItemPreviewPanel->OnKeyUp( unichar );
}

bool CUI_ItemPreviewDebug::OnMouseButtonDown( const panorama::MouseData_t &code )
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return BaseClass::OnMouseButtonDown( code );

	return m_pItemPreviewPanel->OnMouseButtonDown( code );
}

bool CUI_ItemPreviewDebug::OnMouseButtonUp( const panorama::MouseData_t &code )
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return BaseClass::OnMouseButtonUp( code );

	return m_pItemPreviewPanel->OnMouseButtonUp( code );
}

bool CUI_ItemPreviewDebug::OnMouseWheel( const panorama::MouseData_t &code )
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
		return BaseClass::OnMouseWheel( code );

	return m_pItemPreviewPanel->OnMouseWheel( code );
}

void CUI_ItemPreviewDebug::OnMouseMove( float flMouseX, float flMouseY )
{
	if ( !m_pItemPreviewPanel || !BIsVisible() )
	{
		BaseClass::OnMouseMove( flMouseX, flMouseY );
		return;
	}

	m_pItemPreviewPanel->OnMouseMove( flMouseX, flMouseY );
}

// TODO - from modelpanel_weaponpreview.cpp - sticker, workshop, preview helpers
#if 0

CEconItemView * CModelPanelWeaponPreview::sm_pExternalEconItemView1 = NULL;
CEconItemView * CModelPanelWeaponPreview::sm_pExternalEconItemView2 = NULL;
itemid_t CModelPanelWeaponPreview::sm_ullExternalEconItemIdLast = ITEMID_WEAPONPREVIEW_EXTERNAL2;

CEconItemView * Helper_CreateReferenceEconItemWithPreviewDataBlock( CEconItemPreviewDataBlock const &protoData )
{
	CEconItemView *pNewItemView = NULL;

	static CSchemaItemDefHandle hSticker( "sticker" );
	static CSchemaItemDefHandle hSpray( "spray" );
	static CSchemaItemDefHandle hSprayPaint( "spraypaint" );
	bool bStickerGraphic = ( hSticker && protoData.defindex() == hSticker->GetDefinitionIndex() ) ||
		( hSpray && protoData.defindex() == hSpray->GetDefinitionIndex() ) ||
		( hSprayPaint && protoData.defindex() == hSprayPaint->GetDefinitionIndex() );

	static CSchemaItemDefHandle hMusicKit( "musickit" );
	bool bMusicKit = ( hMusicKit && protoData.defindex() == hMusicKit->GetDefinitionIndex() );

	// Make a new econ item view
	if ( bStickerGraphic )
		pNewItemView = InventoryManager()->CreateReferenceEconItem( protoData.defindex(),
																	protoData.stickers().size() ? protoData.stickers( 0 ).sticker_id() : 0,
																	protoData.stickers().size() ? protoData.stickers( 0 ).tint_id() : 0 );
	else if ( bMusicKit )
		pNewItemView = InventoryManager()->CreateReferenceEconItem( protoData.defindex(), protoData.musicindex() );
	else
		pNewItemView = InventoryManager()->CreateReferenceEconItem( protoData.defindex(), protoData.paintindex() );

	pNewItemView->SetItemQuality( protoData.quality() );
	pNewItemView->SetItemQualityOverride( protoData.quality() );
	pNewItemView->SetItemRarityOverride( protoData.rarity() );
	uint32 uiPaintWear = protoData.paintwear();
	pNewItemView->SetOrAddAttributeValueByName( "set item texture wear", *reinterpret_cast<float *>( &uiPaintWear ) );
	pNewItemView->SetOrAddAttributeValueByName( "set item texture seed", protoData.paintseed() );
	if ( protoData.has_killeatervalue() )
	{
		pNewItemView->SetOrAddAttributeValueByName( "kill eater score type", protoData.killeaterscoretype() );
		uint32 nStatTrak = protoData.killeatervalue();
		float flStatTrak = *reinterpret_cast<float*>( (uint32 *)&nStatTrak );
		pNewItemView->SetOrAddAttributeValueByName( "kill eater", flStatTrak );
	}

	if ( protoData.has_customname() )
	{
		pNewItemView->SetCustomNameOverride( protoData.customname().c_str() );
	}

	for ( int iSticker = 0; iSticker < protoData.stickers().size(); ++iSticker )
	{
		// Support up to 6 slots
		CEconItemPreviewDataBlock::Sticker const &infoSticker = protoData.stickers( iSticker );
		if ( !infoSticker.has_slot() ) continue;
		if ( ( int( infoSticker.slot() ) < 0 ) || ( int( infoSticker.slot() ) >= 6 ) ) continue;
		if ( !infoSticker.sticker_id() ) continue;

		uint32 uiStickerID = infoSticker.sticker_id();
		pNewItemView->SetOrAddAttributeValueByName( CFmtStr( "sticker slot %d id", infoSticker.slot() ), *reinterpret_cast<float *>( &uiStickerID ) );
		pNewItemView->SetOrAddAttributeValueByName( CFmtStr( "sticker slot %d wear", infoSticker.slot() ), infoSticker.wear() );
		pNewItemView->SetOrAddAttributeValueByName( CFmtStr( "sticker slot %d scale", infoSticker.slot() ), infoSticker.has_scale() ? infoSticker.scale() : 1.0f );
		pNewItemView->SetOrAddAttributeValueByName( CFmtStr( "sticker slot %d rotation", infoSticker.slot() ), infoSticker.rotation() );
	}

	if ( protoData.stickers().size() && protoData.stickers( 0 ).tint_id() )
	{
		uint32 uiTintID = protoData.stickers( 0 ).tint_id();
		pNewItemView->SetOrAddAttributeValueByName( "spray tint id", *reinterpret_cast<float *>( &uiTintID ) );
	}

	return pNewItemView;
}

static void Helper_LaunchPreviewWithPreviewDataBlock( CEconItemPreviewDataBlock const &protoData )
{
	// Figure out which econ item view we will be overriding
	CEconItemView **ppItemView = &CModelPanelWeaponPreview::sm_pExternalEconItemView1;
	if ( CModelPanelWeaponPreview::sm_ullExternalEconItemIdLast == ITEMID_WEAPONPREVIEW_EXTERNAL1 )
	{
		CModelPanelWeaponPreview::sm_ullExternalEconItemIdLast = ITEMID_WEAPONPREVIEW_EXTERNAL2;
		ppItemView = &CModelPanelWeaponPreview::sm_pExternalEconItemView2;
	}
	else
	{
		CModelPanelWeaponPreview::sm_ullExternalEconItemIdLast = ITEMID_WEAPONPREVIEW_EXTERNAL1;
		ppItemView = &CModelPanelWeaponPreview::sm_pExternalEconItemView1;
	}

	// This item pointer will be set to newly allocated item
	*ppItemView = NULL;

	// Make a new econ item view
	CEconItemView *pNewItemView = Helper_CreateReferenceEconItemWithPreviewDataBlock( protoData );

	// We are ready with the item!
	*ppItemView = pNewItemView;

	// Log UI event
//	CSAppLifetimeGameStats()->RecordUIEvent( "WeaponInspectExternalItem" );

	//SCALEFORM_COMPONENT_BROADCAST_EVENT_WITH_PARAMS_BEGIN( Inventory, WeaponPreviewRequest, pEventParams );
	//pEventParams->SetString( "itemid", CFmtStr( "%llu", CModelPanelWeaponPreview::sm_ullExternalEconItemIdLast ) );
	//SCALEFORM_COMPONENT_BROADCAST_EVENT_WITH_PARAMS_END( Inventory, WeaponPreviewRequest, pEventParams );
	UI_COMPONENT_BROADCAST_EVENT( Inventory, WeaponPreviewRequest, CFmtStr( "%llu", CModelPanelWeaponPreview::sm_ullExternalEconItemIdLast ).Get() );
}

static void Helper_RequestEconActionPreview( uint64 paramS, uint64 paramA, uint64 paramD, uint64 paramM )
{
	if ( !paramA || !paramD )
		return;
	if ( !paramS && !paramM )
		return;

	static double s_flTime = 0.0f;
	double flNow = Plat_FloatTime();
	if ( s_flTime && ( flNow - s_flTime <= 2.5 ) )
		return;

	s_flTime = flNow;
	GCSDK::CProtoBufMsg< CMsgGCCStrike15_v2_Client2GCEconPreviewDataBlockRequest > msg( k_EMsgGCCStrike15_v2_Client2GCEconPreviewDataBlockRequest );
	msg.Body().set_param_s( paramS );
	msg.Body().set_param_a( paramA );
	msg.Body().set_param_d( paramD );
	msg.Body().set_param_m( paramM );
	GCClientSystem()->GetGCClient()->BSendMessage( msg );
}

class ClientJob_EMsgGCCStrike15_v2_Client2GCEconPreviewDataBlockResponse : public GCSDK::CGCClientJob
{
public:
	ClientJob_EMsgGCCStrike15_v2_Client2GCEconPreviewDataBlockResponse( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient )
	{
	}

	virtual bool BYieldingRunJobFromMsg( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgGCCStrike15_v2_Client2GCEconPreviewDataBlockResponse> msg( pNetPacket );
		Helper_LaunchPreviewWithPreviewDataBlock( msg.Body().iteminfo() );
		return true;
	}
};
GC_REG_CLIENT_JOB( ClientJob_EMsgGCCStrike15_v2_Client2GCEconPreviewDataBlockResponse, k_EMsgGCCStrike15_v2_Client2GCEconPreviewDataBlockResponse );

CON_COMMAND_F( csgo_econ_action_preview, "Preview an economy item", FCVAR_DONTRECORD | FCVAR_HIDDEN | FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( args.ArgC() < 2 )
		return;

	// kill the workshop preview dialog if it's up
// 	if ( g_pWorkshopWorkbenchDialog )
// 	{
// 		delete g_pWorkshopWorkbenchDialog;
// 		g_pWorkshopWorkbenchDialog = NULL;
// 	}

	extern float g_flReadyToCheckForPCBootInvite;
	if ( !g_flReadyToCheckForPCBootInvite || !gpGlobals->curtime || !gpGlobals->framecount )
	{
		ConMsg( "Deferring csgo_econ_action_preview command!\n" );
		return;
	}

	// Encoded parameter, validate basic length
	char const *pchEncodedAscii = args.Arg( 1 );
	int nLen = Q_strlen( pchEncodedAscii );
	if ( nLen <= 16 ) { Assert( 0 ); return; }

	// If we are launched with new format requesting steam_ownerid and assetid then do async query
	if ( *pchEncodedAscii == 'S' )
	{
		uint64 uiParamS = Q_atoui64( pchEncodedAscii + 1 );
		uint64 uiParamA = 0;
		uint64 uiParamD = 0;
		if ( char const *pchParamA = strchr( pchEncodedAscii, 'A' ) )
		{
			uiParamA = Q_atoui64( pchParamA + 1 );
			if ( char const *pchParamD = strchr( pchEncodedAscii, 'D' ) )
			{
				uiParamD = Q_atoui64( pchParamD + 1 );
				Helper_RequestEconActionPreview( uiParamS, uiParamA, uiParamD, 0ull );
			}
		}
		return;
	}

	// Else if we are launched with new format requesting market listing id and assetid then do async query
	if ( *pchEncodedAscii == 'M' )
	{
		uint64 uiParamM = Q_atoui64( pchEncodedAscii + 1 );
		uint64 uiParamA = 0;
		uint64 uiParamD = 0;
		if ( char const *pchParamA = strchr( pchEncodedAscii, 'A' ) )
		{
			uiParamA = Q_atoui64( pchParamA + 1 );
			if ( char const *pchParamD = strchr( pchEncodedAscii, 'D' ) )
			{
				uiParamD = Q_atoui64( pchParamD + 1 );
				Helper_RequestEconActionPreview( 0ull, uiParamA, uiParamD, uiParamM );
			}
		}
		return;
	}

	// Parsed parameters
	CEconItemPreviewDataBlock protoData;
	if ( nLen % 2 ) { Assert( 0 ); return; }
	uint32 numBytes = nLen / 2;
	uint8 *uiDecoded = (uint8 *)stackalloc( numBytes );
	for ( uint32 j = 0; j < numBytes; ++j )
	{
		uint32 uHi = ( pchEncodedAscii[ 2 * j + 0 ] >= 'A' ) ? ( 10 + pchEncodedAscii[ 2 * j + 0 ] - 'A' ) : ( pchEncodedAscii[ 2 * j + 0 ] - '0' );
		uint32 uLo = ( pchEncodedAscii[ 2 * j + 1 ] >= 'A' ) ? ( 10 + pchEncodedAscii[ 2 * j + 1 ] - 'A' ) : ( pchEncodedAscii[ 2 * j + 1 ] - '0' );
		if ( uHi >= 16 || uLo >= 16 ) { Assert( 0 ); return; }
		uint32 uVal = ( uHi * 16 ) + uLo;
		if ( uVal >= 256 ) { Assert( 0 ); return; }
		uiDecoded[ j ] = uint8( uVal );
	}
	for ( uint32 j = 1; j < numBytes; ++j )
	{
		uiDecoded[ j ] = ( uiDecoded[ j ] ^ uiDecoded[ 0 ] );
	}
	// First byte is the size of the message
	uint32 numPrefixBytes = sizeof( uint8 );
	uint32 uiByteSize = numBytes - numPrefixBytes - sizeof( uint32 );
	// Last uint32 is the signature of the message
	uint32 uiSignature;
	Q_memcpy( &uiSignature, uiDecoded + numBytes - sizeof( uint32 ), sizeof( uint32 ) );
	uiSignature = BigLong( uiSignature );
	{
		CRC32_t crcVal = CRC32_ProcessSingleBuffer( uiDecoded, uiByteSize + numPrefixBytes );
		crcVal = ( crcVal * uiByteSize ) ^ ( crcVal % 0x10000 );
		if ( crcVal != uiSignature ) { Assert( 0 ); return; }
	}
	if ( !protoData.ParseFromArray( uiDecoded + numPrefixBytes, uiByteSize ) ) { Assert( 0 ); return; }

	//
	// Launch the preview
	//
	Helper_LaunchPreviewWithPreviewDataBlock( protoData );
}

//
// Workshop Preview Dialog Helpers
//
static int s_nCurrentPaintKitId = PREVIEW_PAINTKIT_ID_BASE;

void UpdateChildWeaponPreviewPanel( KeyValues* pPaintKitKV, const char* pWeapon, int nWear, bool bShowStatTrak, bool bShowNameTag, PreviewMode previewMode, int nSeed, vgui::Panel *parent, bool bCreate, bool bForceAnimReset )
{
	if ( engine->IsConnected() )
		return;

	if ( !g_pModelPanelWeaponPreview && !bCreate )
		return;

	GetItemSchema()->RemovePaintKitDefinition( s_nCurrentPaintKitId );

	if ( s_nCurrentPaintKitId < ( PREVIEW_PAINTKIT_ID_BASE + 0xf ) )
	{
		s_nCurrentPaintKitId++;
	}
	else
	{
		s_nCurrentPaintKitId = PREVIEW_PAINTKIT_ID_BASE;
	}

	// Figure out which econ item view we will be overriding
	CEconItemView **ppItemView = &CModelPanelWeaponPreview::sm_pExternalEconItemView1;
	if ( CModelPanelWeaponPreview::sm_ullExternalEconItemIdLast == ITEMID_WEAPONPREVIEW_EXTERNAL1 )
	{
		CModelPanelWeaponPreview::sm_ullExternalEconItemIdLast = ITEMID_WEAPONPREVIEW_EXTERNAL2;
		ppItemView = &CModelPanelWeaponPreview::sm_pExternalEconItemView2;
	}
	else
	{
		CModelPanelWeaponPreview::sm_ullExternalEconItemIdLast = ITEMID_WEAPONPREVIEW_EXTERNAL1;
		ppItemView = &CModelPanelWeaponPreview::sm_pExternalEconItemView1;
	}

	// This item pointer will be set to newly allocated item
	*ppItemView = NULL;

	// 
	CEconItemDefinition *pItem = GetItemSchema()->GetItemDefinitionByName( pWeapon );
	if ( !pItem )
	{
		return;
	}
	int	nDefinitionIndex = pItem->GetDefinitionIndex();

	CPaintKit *pPaintKit = new CPaintKit;
	pPaintKit->nID = s_nCurrentPaintKitId;
	pPaintKit->InitFromKeyValues( pPaintKitKV, GetItemSchema()->GetPaintKitDefinitionByName( "workshop_default" ), true );
	pPaintKit->sName.Set( "workshop workbench" );

	GetItemSchema()->AddPaintKitDefinition( s_nCurrentPaintKitId, pPaintKit );

	// Make a new econ item view
	CEconItemView *pNewItemView = InventoryManager()->CreateReferenceEconItem( nDefinitionIndex, pPaintKit->nID );
	pNewItemView->SetItemQuality( 0 );
	pNewItemView->SetItemQualityOverride( 0 );
	pNewItemView->SetItemRarityOverride( 0 );
	float flWear = (float)nWear / 100.0f;
	flWear = RemapValClamped( flWear, 0.0f, 1.0f, pPaintKit->flWearRemapMin, pPaintKit->flWearRemapMax );
	pNewItemView->SetOrAddAttributeValueByName( "set item texture wear", flWear );

	// changing the random seed each time causes the textures to regen properly
	pNewItemView->SetOrAddAttributeValueByName( "set item texture seed", nSeed );

	if ( bShowStatTrak )
	{
		pNewItemView->SetOrAddAttributeValueByName( "kill eater score type", 1 );
		pNewItemView->SetOrAddAttributeValueByName( "kill eater", 0 );
	}

	if ( bShowNameTag )
	{
		pNewItemView->SetCustomNameOverride( "Workshop Preview" );
	}

	// We are ready with the item!
	*ppItemView = pNewItemView;

	char const *szEquippedPedestalDisplayModel = ( previewMode == PreviewMode_Hold ) ? pNewItemView->GetPlayerDisplayModel() : pNewItemView->GetPedestalDisplayModel();
	if ( !szEquippedPedestalDisplayModel || !*szEquippedPedestalDisplayModel )
	{
		DevWarning( "UpdateChildWeaponPreviewPanel cannot find model for item %llu/%p!\n", pNewItemView->GetItemID(), pNewItemView->GetItemDefinition() );
		return;
	}

	KeyValues *kvManifest = new KeyValues( "manifest" );
	KeyValues::AutoDelete autodelete_kvManifest( kvManifest );
	if ( !kvManifest->LoadFromFile( g_pFullFileSystem, "resource/ui/econ/ItemModelPanelWorkshopWeaponPreview.res" ) )
		return;

	//[OGS] UIEvent, Inspect. Categorized by player state, coarse item type.
//	CSAppLifetimeGameStats()->RecordUIEvent( "WorkshopWeaponPreview" );

	KeyValues *kvPreviewSettings = BuildWeaponPreviewSettingsForEconItemView( pNewItemView, kvManifest, previewMode );
	KeyValues::AutoDelete autodelete_kvPreviewSettings( kvPreviewSettings );

	if ( bCreate )
	{
		int x, y, wide, tall;
		parent->GetBounds( x, y, wide, tall );

		g_pModelPanelWeaponPreview = new CModelPanelWeaponPreview( parent, "WorkshopWeaponPreviewPanel", kvPreviewSettings, pNewItemView );
		g_pModelPanelWeaponPreview->SetWeaponTextureSize( COMPOSITE_TEXTURE_SIZE_2048 );

		g_pModelPanelWeaponPreview->SetParent( parent );

		vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/ClientScheme.res", "ClientScheme" );
		g_pModelPanelWeaponPreview->SetScheme( scheme );

		g_pModelPanelWeaponPreview->SetBounds( 0, 0, wide, tall );

		g_pModelPanelWeaponPreview->SetMouseInputEnabled( false );
		g_pModelPanelWeaponPreview->SetKeyBoardInputEnabled( false );
		g_pModelPanelWeaponPreview->SetVisible( true );
		g_pModelPanelWeaponPreview->MoveToFront();
		g_pModelPanelWeaponPreview->PerformLayout();
	}
	else
	{
		g_pModelPanelWeaponPreview->UpdateItem( kvPreviewSettings, pNewItemView, bForceAnimReset );
	}

	//
	// NOTE: <vitaliy Aug 2016> -- looks like this code might have a bug: it allocates a new reference econ item at the tail of internal
	// econ items arrays in inventory manager, but removes searching from head, so chances are it will remove the previously allocated
	// reference econ item, and not the custom configured one that was newly added at the tail.
	// On top of that reference econ items seem to never actually deallocate their memory and all chained resources, so this code
	// if executed multiple times will leak a bunch of econ items.
	// At the time of writing this, it's unclear if reference econ items can actually be deallocated, and since this code path is
	// only for workshop contributors it is a low traffic path that is probably not worth fixing.
	//
	InventoryManager()->RemoveReferenceEconItem( nDefinitionIndex, pPaintKit->nID, 0 );
}

KeyValues *GetWorkshopWorkbenchKeyValuesFromFile( const char *pFilename )
{
	KeyValues* pPaintKitKV = new KeyValues( "workshop workbench" );
	if ( pFilename && pFilename[ 0 ] != 0 )
	{
		// load specified paintkit kv file
		if ( !pPaintKitKV->LoadFromFile( g_pFullFileSystem, pFilename ) )
		{
			pPaintKitKV->deleteThis();
			DevMsg( "Unable to load %s\n", pFilename );
			pPaintKitKV = NULL;
		}
	}
	else if ( GetItemSchema()->GetPaintKitDefinitionByName( "workshop_default" ) )
	{
		// no file specified, so just use default PaintKit def to make some KVs
		GetItemSchema()->GetPaintKitDefinitionByName( "workshop_default" )->FillKeyValuesForWorkshop( pPaintKitKV );
	}
	else
	{
		// no file specified, and default PaintKit def not available
		DevMsg( "Unable to find workshop_default definition!\n" );
		pPaintKitKV->deleteThis();
		pPaintKitKV = NULL;
	}

	return pPaintKitKV;
}

void CreateChildWeaponPreviewPanel( KeyValues *pPaintKitKV, const char* pWeapon, int nWear, bool bShowStatTrak, bool bShowNameTag, PreviewMode previewMode, int nSeed, vgui::Panel *parent )
{
	delete g_pModelPanelWeaponPreview;
	g_pModelPanelWeaponPreview = NULL;

	UpdateChildWeaponPreviewPanel( pPaintKitKV, pWeapon, nWear, bShowStatTrak, bShowNameTag, previewMode, nSeed, parent, true, true );
}

void ChildWeaponPreviewPanelPlayAnimation( const char *pAnimName )
{
	if ( g_pModelPanelWeaponPreview )
	{
		g_pModelPanelWeaponPreview->PlayAnimOnWeapon( pAnimName );
	}
}

void CleaupChildWorkshopPreviewPanel()
{
	// we don't delete g_pModelPanelWeaponPreview here, because it was attached to the dialog and deleted by it
	// the dialog calls this when it's destructing
	g_pModelPanelWeaponPreview = NULL;
	GetItemSchema()->RemovePaintKitDefinition( s_nCurrentPaintKitId );
}
#endif

