
#include "cbase.h"

#include "ienginevgui.h"
#include "gameui_interface.h"
#include "basepanel.h"

#include "cstrike15/cstrike15_item_inventory.h"
#include "econ/econ_ui.h"
#include "animation.h"

#include "matchmaking/imatchframework.h"
#include "modelpanel_weaponpreview.h"
#include "renderparm.h"

#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterialvar.h"
#include "imaterialproxydict.h"

#include "cstrike15_gcmessages.pb.h"
#include "workshoppreviewdialog.h"
#include "materialsystem/ivisualsdataprocessor.h"
#include "materialsystem/icompositetexture.h"

#include "gc_clientsystem.h"
#include "cstrike15_gcmessages.pb.h"
#include "cstrike15_gcconstants.h"
#include "cs_app_lifetime_gamestats.h"

#include "uicomponents/uicomponent_inventory.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CModelPanelWeaponPreview *g_pModelPanelWeaponPreview = NULL;
extern CWorkshopWorkbenchDialog *g_pWorkshopWorkbenchDialog;

CModelPanelWeaponPreview::CModelPanelWeaponPreview( vgui::Panel *parent, const char *name, KeyValues *kvConfiguration, const CEconItemView *pItem ) : vgui::EditablePanel( parent, name )
{
	m_kvConfiguration = kvConfiguration ? kvConfiguration->MakeCopy() : NULL;

	m_hMdlWeapon = MDLHANDLE_INVALID;

	m_ItemData = *pItem;
	m_pModelPanel = new CBaseModelPanel( this, "modelpanel" );
	m_weaponTextureSize = COMPOSITE_TEXTURE_SIZE_1024;
}

CModelPanelWeaponPreview::~CModelPanelWeaponPreview()
{
	delete m_pModelPanel.Get();

	if ( m_kvConfiguration )
		m_kvConfiguration->deleteThis();
	m_kvConfiguration = NULL;

	if ( m_shadowFlashlightCookie.IsValid() )
		m_shadowFlashlightCookie.Shutdown();
}

void CModelPanelWeaponPreview::PaintBackground()
{
	BaseClass::PaintBackground();

	if ( m_ItemData.GetCustomMaterialCount() > 0 )
	{
		GetModelPanel()->UpdateModelCustomMaterials( m_hMdlWeapon, &m_ItemData );
		m_ItemData.ClearCustomMaterials(); 
	}
}

Vector ParseSettingsVectorFromString( char const *szString )
{
	Vector vec = vec3_origin;
	if ( szString[0] == '[' )
		++szString;
	while ( *szString && V_isspace( *szString ) )
		++ szString;
	
	vec.x = Q_atof( szString );
	while ( *szString && !V_isspace( *szString ) )
		++ szString;
	while ( *szString && V_isspace( *szString ) )
		++ szString;
		
	vec.y = Q_atof( szString );
	while ( *szString && !V_isspace( *szString ) )
		++ szString;
	while ( *szString && V_isspace( *szString ) )
		++ szString;
		
	vec.z = Q_atof( szString );

	return vec;
}

void CModelPanelWeaponPreview::PerformLayout()
{
	int w,h;
	GetSize( w, h );
	m_pModelPanel->SetBounds( 0, 0, w, h );
	BaseClass::PerformLayout();
}

static int HelperGetSequenceIdFromStudioHdr( CStudioHdr &studioHdr, char const *szName )
{
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

CON_COMMAND_F( modelpanel_set_sticker, "[Slot] [Id] Adds a sticker to the 3d weapon preview model", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT )
{
	if ( args.ArgC() < 3 )
		return;
	g_pModelPanelWeaponPreview->ApplySticker( V_atoi( args[1] ), V_atoi( args[2] ) );
}

void CModelPanelWeaponPreview::ApplySticker( int nSlot, int nStickerId )
{
	CEconItemView *pItem = &m_ItemData;
	if ( pItem && pItem->ItemHasAnyFreeStickerSlots() )
	{
		nSlot = pItem->GetStickerSlotFirstFreeFromIndex( nSlot );
		if ( nSlot != -1 )
		{
			//setup the next free sticker
			pItem->SetOrAddAttributeValueByName( CFmtStr( "sticker slot %i id", nSlot ), *((float*)&nStickerId) );
			
			UpdateItem( m_kvConfiguration, pItem, false, true );

			//preemptively clear it out right away
			pItem->SetOrAddAttributeValueByName( CFmtStr( "sticker slot %i id", nSlot ), 0 );
		}	
	}
}

void CModelPanelWeaponPreview::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	extern bool Helper_IsSpray( const CEconItemDefinition * pEconItemDefinition );
	BaseClass::ApplySchemeSettings( pScheme );

	SetBorder( NULL );
	if ( GetModelPanel() )
	{
		GetModelPanel()->SetBackgroundColor( Color( 0, 0, 0, 0 ) );
		GetModelPanel()->SetBorder( NULL );
		GetModelPanel()->SetVisible( true );
		GetModelPanel()->SetEnabled( true );
		GetModelPanel()->SetStartFramed( false );

		// Setup the root model and merged models
		const char *pszRootMdl = m_kvConfiguration->GetString( "root_mdl" );

		// are we in first person arms mode?
		bool bUsePedestalModel = true;
		if ( V_stristr( pszRootMdl, "firstperson" ) != NULL )
		{
			bUsePedestalModel = false;
		}

		GetModelPanel()->SetMDL( pszRootMdl );
		if ( KeyValues* pkvMergeMdlsRoot = m_kvConfiguration->FindKey( "mergemdls" ) )
		{
			for ( KeyValues *kvMergeMdls = pkvMergeMdlsRoot->GetFirstSubKey(); kvMergeMdls; kvMergeMdls = kvMergeMdls->GetNextKey() )
			{
				GetModelPanel()->SetMergeMDL( kvMergeMdls->GetString() );
			}
		}

		if ( m_ItemData.IsStickerTool() )
		{

			int nRarityIndex = m_ItemData.GetRarity();
			if ( nRarityIndex >= 3 && nRarityIndex <= 7 )
				GetModelPanel()->SetMdlSkinIndex( nRarityIndex - 3 );

			MDLHandle_t pStickerMdl = GetModelPanel()->SetMergeMDL( "models/inventory_items/sticker_inspect.mdl" );
			if ( CMDL *pStickerMdlCMDL = ( pStickerMdl != MDLHANDLE_INVALID ) ? GetModelPanel()->GetMergeMDL( pStickerMdl ) : NULL )
			{
				IMaterial *pMatStickerOverride = m_ItemData.GetToolStickerMaterial();
				if ( pMatStickerOverride && !pMatStickerOverride->IsErrorMaterial() )
				{
					pStickerMdlCMDL->SetSimpleMaterialOverride( pMatStickerOverride );
				}
			}
		}
		else if ( Helper_IsSpray( m_ItemData.GetItemDefinition() ) )
		{
			MDLHandle_t pStickerMdl = GetModelPanel()->SetMergeMDL( "models/sprays/spray_plane.mdl" );
			if ( CMDL *pStickerMdlCMDL = ( pStickerMdl != MDLHANDLE_INVALID ) ? GetModelPanel()->GetMergeMDL( pStickerMdl ) : NULL )
			{
				extern IMaterial * QcCreateDecalDataForModelPreviewPanel( int nStickerKitDefinition, int nTintID );
				uint32 nStickerId = m_ItemData.GetStickerAttributeBySlotIndexInt( 0, k_EStickerAttribute_ID, 0 );

				static CSchemaAttributeDefHandle hAttrSprayTintID( "spray tint id" );
				uint32 unTintID = 0;
				if ( !hAttrSprayTintID || !m_ItemData.FindAttribute( hAttrSprayTintID, &unTintID ) )
					unTintID = 0;

				IMaterial *pMatStickerOverride = QcCreateDecalDataForModelPreviewPanel( nStickerId, unTintID );
				if ( pMatStickerOverride && !pMatStickerOverride->IsErrorMaterial() )
				{
					pStickerMdlCMDL->SetSimpleMaterialOverride( pMatStickerOverride );
				}
			}
		}
		else
		{
			// Add the weapon merged to the root model, request bonemerge takeover from that point and add models merged to the weapon after it
			//FIXME: Reverse this, MergeMDL copies custom materials from the econ item, but the line after it does the upgrade to 1024! 
			m_hMdlWeapon = GetModelPanel()->SetMergeMDL( bUsePedestalModel ? m_ItemData.GetPedestalDisplayModel() : m_ItemData.GetPlayerDisplayModel(), &m_ItemData, static_cast< IClientRenderable * >( &m_ItemData ), true );
			m_ItemData.UpdateGeneratedMaterial( false, MDLHANDLE_INVALID, m_weaponTextureSize );
			if ( KeyValues* pkvMergeWeaponMdlsRoot = m_kvConfiguration->FindKey( "weaponmergemdls" ) )
			{
				for ( KeyValues *kvMergeMdls = pkvMergeWeaponMdlsRoot->GetFirstSubKey(); kvMergeMdls; kvMergeMdls = kvMergeMdls->GetNextKey() )
				{
					GetModelPanel()->SetMergeMDL( kvMergeMdls->GetString() );
				}
			}

			m_ItemData.GenerateStickerMaterials();
			if ( m_ItemData.ItemHasAnyStickersApplied() )
			{
				for ( int i=0; i<m_ItemData.GetNumSupportedStickerSlots(); i++ )
				{
					if ( m_ItemData.GetStickerIMaterialBySlotIndex(i) != NULL )
					{
						MDLHandle_t pStickerMdl = GetModelPanel()->SetMergeMDL( m_ItemData.GetStickerSlotModelBySlotIndex(i) );
						if ( CMDL *pStickerMdlCMDL = ( pStickerMdl != MDLHANDLE_INVALID ) ? GetModelPanel()->GetMergeMDL( pStickerMdl ) : NULL )
						{
							pStickerMdlCMDL->SetSimpleMaterialOverride( m_ItemData.GetStickerIMaterialBySlotIndex(i) );
						}
					}
				}
			}
		}

		GetModelPanel()->ResetCameraPivot();
		GetModelPanel()->SetCameraOffset( vec3_origin );
		GetModelPanel()->SetCameraPositionAndAngles( vec3_origin, vec3_angle );
		GetModelPanel()->SetModelAnglesAndPosition( vec3_angle, vec3_origin );

		//
		// Apply all the configuration settings
		//
		GetModelPanel()->ClearDirectionalLights();
		GetModelPanel()->SetLightAmbient( ParseSettingsVectorFromString( m_kvConfiguration->GetString( "light_ambient", "[0.11 0.11 0.12]" ) ) );
		int iDirectionalLight = 0;
		for ( KeyValues *kvDirectionalLights = m_kvConfiguration->FindKey( "light_directional" )->GetFirstSubKey(); kvDirectionalLights; kvDirectionalLights = kvDirectionalLights->GetNextKey() )
		{
			char const *sz = kvDirectionalLights->GetString();
			Vector rgb = vec3_origin, dir = vec3_origin;
			if ( char const *szRGB = strstr( sz, "rgb[" ) )
				rgb = ParseSettingsVectorFromString( szRGB + 3 );
			if ( char const *szDIR = strstr( sz, "dir[" ) )
				dir = ParseSettingsVectorFromString( szDIR + 3 );
			char chLightAttachmentPoint[128] = {};
			if ( char const *szLightAttachmentPoint = strstr( sz, "attach[" ) )
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
				GetModelPanel()->UpdateDirectionalLight( iDirectionalLight, Color( rgb.x, rgb.y, rgb.z ), dir );
				if ( chLightAttachmentPoint[0] )
					GetModelPanel()->SetDirectionalLightAttachment( iDirectionalLight, chLightAttachmentPoint );
				iDirectionalLight ++;
			}
		}

		GetModelPanel()->SetSetupRenderStateDelayed( true );
		GetModelPanel()->SetRender3DSupersampled( true );

		GetModelPanel()->SetModelAnim( m_kvConfiguration->GetString( "root_anim", "ACT_IDLE_INSPECT" ), true );
		GetModelPanel()->AddModelAnimFollowLoop( m_kvConfiguration->GetString( "root_anim_loop", "ACT_IDLE_INSPECT" ), true );

		//Disabling support for default weapon pedestal animation
		if ( CMDL *pMdlWeaponConfig = ( m_hMdlWeapon != MDLHANDLE_INVALID ) ? GetModelPanel()->GetMergeMDL( m_hMdlWeapon ) : NULL )
		{
			MDLCACHE_CRITICAL_SECTION();
		
			CStudioHdr studioHdr( pMdlWeaponConfig->GetStudioHdr(), g_pMDLCache );
			char const *szWeaponAnim = m_kvConfiguration->GetString( "weapon_anim", "ACT_IDLE_INSPECT" );
		
			int iSequence = HelperGetSequenceIdFromStudioHdr( studioHdr, szWeaponAnim );
			if ( iSequence != ACT_INVALID )
			{
				pMdlWeaponConfig->m_nSequence = iSequence;
				pMdlWeaponConfig->m_bUseSequencePlaybackFPS = true;
		
				char const *szWeaponAnimLoop = m_kvConfiguration->GetString( "weapon_anim_loop", "ACT_IDLE_INSPECT" );
				iSequence = HelperGetSequenceIdFromStudioHdr( studioHdr, szWeaponAnimLoop );
				if ( iSequence != ACT_INVALID )
				{
					pMdlWeaponConfig->m_arrSequenceFollowLoop.AddToTail( iSequence );
				}
			}
		}

		GetModelPanel()->SetCameraAttachment( m_kvConfiguration->GetString( "root_camera", "attach_camera_inspect" ) );
		GetModelPanel()->SetRenderCaptureCameraAttachment( m_kvConfiguration->GetString( "shadow_light", "attach_camera_inspect_light" ) );
		GetModelPanel()->SetCameraFOV( m_kvConfiguration->GetFloat( "root_camera_fov", 54.0f ) );

		Vector vecCameraOffset;
		sscanf(m_kvConfiguration->GetString("camera_offset", "0 0 0"), "%f %f %f", &vecCameraOffset.x, &vecCameraOffset.y, &vecCameraOffset.z);
		GetModelPanel()->SetCameraPositionOverride(vecCameraOffset);
		GetModelPanel()->SetCameraPositionOverrideEnabled(true);

		Vector vecCameraOffsetAngle;
		sscanf(m_kvConfiguration->GetString("camera_orient", "0 0 0"), "%f %f %f", &vecCameraOffsetAngle.x, &vecCameraOffsetAngle.y, &vecCameraOffsetAngle.z);
		GetModelPanel()->SetCameraOrientOverride(vecCameraOffsetAngle);
		GetModelPanel()->SetCameraOrientOverrideEnabled(true);

		extern bool ClientShadowMgrAcquireShadowDepthTexture( CTextureReference *pDummyColorTexture, CTextureReference *pShadowDepthTexture );
		ClientShadowMgrAcquireShadowDepthTexture( &m_shadowDummyColorBufferTexture, &m_shadowDepthTexture );
		m_cfgRenderCapture.m_pIVRenderView = render;
		m_cfgRenderCapture.m_pFlashlightDepthTexture = m_shadowDepthTexture;
		m_cfgRenderCapture.m_pDummyColorBufferTexture = m_shadowDummyColorBufferTexture;
		
		m_cfgRenderCapture.m_renderFlashlightState.m_NearZ = m_kvConfiguration->GetFloat( "shadow_light_znear", 4.0f );
		m_cfgRenderCapture.m_renderFlashlightState.m_FarZ = m_kvConfiguration->GetFloat( "shadow_light_zfar", 512.0f );
		m_cfgRenderCapture.m_renderFlashlightState.m_fHorizontalFOVDegrees = m_kvConfiguration->GetFloat( "shadow_light_hfov", 54.0f );
		m_cfgRenderCapture.m_renderFlashlightState.m_fVerticalFOVDegrees = m_kvConfiguration->GetFloat( "shadow_light_vfov", 54.0f );
		
		m_cfgRenderCapture.m_renderFlashlightState.m_fQuadraticAtten = m_kvConfiguration->GetFloat( "shadow_light_atten_quadratic", 0.0f );
		m_cfgRenderCapture.m_renderFlashlightState.m_fLinearAtten = m_kvConfiguration->GetFloat( "shadow_light_atten_linear", 512.0f );
		m_cfgRenderCapture.m_renderFlashlightState.m_fConstantAtten = m_kvConfiguration->GetFloat( "shadow_light_atten_constant", 0.0f );
		m_cfgRenderCapture.m_renderFlashlightState.m_FarZAtten = m_kvConfiguration->GetFloat( "shadow_light_atten_farz", 512.0f );
		
		m_cfgRenderCapture.m_renderFlashlightState.m_fBrightnessScale = m_kvConfiguration->GetFloat( "shadow_light_brightness", 1.0f );
		
		Vector vecShadowLightColor = ParseSettingsVectorFromString( m_kvConfiguration->GetString( "shadow_light_color" ) );
		m_cfgRenderCapture.m_renderFlashlightState.m_Color[0] = vecShadowLightColor.x;
		m_cfgRenderCapture.m_renderFlashlightState.m_Color[1] = vecShadowLightColor.y;
		m_cfgRenderCapture.m_renderFlashlightState.m_Color[2] = vecShadowLightColor.z;
		m_cfgRenderCapture.m_renderFlashlightState.m_Color[3] = 1.0f;

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
		m_cfgRenderCapture.m_renderFlashlightState.m_flShadowSlopeScaleDepthBias = g_pMaterialSystemHardwareConfig->GetShadowSlopeScaleDepthBias();
		m_cfgRenderCapture.m_renderFlashlightState.m_flShadowDepthBias = g_pMaterialSystemHardwareConfig->GetShadowDepthBias();

		GetModelPanel()->EnableRenderingWithFlashlight( reinterpret_cast< void * >( &m_cfgRenderCapture ) );
	}

	UI_COMPONENT_BROADCAST_EVENT( Inventory, ModelPanelReady );
}

void CModelPanelWeaponPreview::UpdateItem( KeyValues *kvConfiguration, CEconItemView *pItem, bool bForceAnimReset, bool bUpdatingStickers /*= false*/ )
{
	Assert( bUpdatingStickers || kvConfiguration ); // if not updating stickers must pass valid configuration
	Assert( bUpdatingStickers || pItem ); // if not updating stickers must pass valid item

	if ( GetModelPanel() )
	{
		float flWeaponTime = 0.0f;
		float flWeaponTimeBasisAdjustment = 0.0f;
		int nWeaponSequence = ACT_INVALID;

		// clean up existing merge models, saving weapon animation info if not reseting
		if ( GetModelPanel()->GetMergeMDL(m_hMdlWeapon) )
		{
			GetModelPanel()->GetMergeMDL(m_hMdlWeapon)->ClearCustomMaterials( true );

			if ( !bForceAnimReset )
			{
				flWeaponTime = GetModelPanel()->GetMergeMDL(m_hMdlWeapon)->m_flTime;
				flWeaponTimeBasisAdjustment = GetModelPanel()->GetMergeMDL(m_hMdlWeapon)->m_flTimeBasisAdjustment;
				nWeaponSequence = GetModelPanel()->GetMergeMDL(m_hMdlWeapon)->m_nSequence;
			}
		}
		GetModelPanel()->ClearMergeMDLs();

		// save the root model animation info if not reseting
		float flRootTime = 0.0f;
		float flRootTimeBasisAdjustment = 0.0f;
		int nRootSequence = ACT_INVALID;
		if ( !bForceAnimReset )
		{
			flRootTime = GetModelPanel()->GetMDL()->m_flTime;
			flRootTimeBasisAdjustment = GetModelPanel()->GetMDL()->m_flTimeBasisAdjustment;
			nRootSequence = GetModelPanel()->GetMDL()->m_nSequence;
		}

		if ( !bUpdatingStickers && ( m_kvConfiguration != kvConfiguration ) )
		{
			if ( m_kvConfiguration )
				m_kvConfiguration->deleteThis();

			m_kvConfiguration = kvConfiguration ? kvConfiguration->MakeCopy() : NULL;
		}
				
		m_hMdlWeapon = MDLHANDLE_INVALID;

		if ( !bUpdatingStickers )
		{
			m_ItemData.ClearCustomMaterials( true );
			m_ItemData = *pItem;
		}

		// Setup the root model and merged models
		const char *pszRootMdl = m_kvConfiguration->GetString( "root_mdl" );

		// are we in first person arms mode?
		bool bUsePedestalModel = true;
		if ( V_stristr( pszRootMdl, "firstperson" ) != NULL )
		{
			bUsePedestalModel = false;
		}

		GetModelPanel()->SetMDL( pszRootMdl );
		for ( KeyValues *kvMergeMdls = m_kvConfiguration->FindKey( "mergemdls" )->GetFirstSubKey(); kvMergeMdls; kvMergeMdls = kvMergeMdls->GetNextKey() )
		{
			GetModelPanel()->SetMergeMDL( kvMergeMdls->GetString() );
		}

		// Add the weapon merged to the root model, request bonemerge takeover from that point and add models merged to the weapon after it
		m_hMdlWeapon = GetModelPanel()->SetMergeMDL( bUsePedestalModel ? m_ItemData.GetPedestalDisplayModel() : m_ItemData.GetPlayerDisplayModel(), &m_ItemData, static_cast< IClientRenderable * >( &m_ItemData ), true );
		m_ItemData.UpdateGeneratedMaterial( false, MDLHANDLE_INVALID, m_weaponTextureSize );
		for ( KeyValues *kvMergeMdls = m_kvConfiguration->FindKey( "weaponmergemdls" )->GetFirstSubKey(); kvMergeMdls; kvMergeMdls = kvMergeMdls->GetNextKey() )
		{
			GetModelPanel()->SetMergeMDL( kvMergeMdls->GetString() );
		}

		m_ItemData.GenerateStickerMaterials();
		if ( m_ItemData.ItemHasAnyStickersApplied() )
		{
			for ( int i=0; i<m_ItemData.GetNumSupportedStickerSlots(); i++ )
			{
				if ( m_ItemData.GetStickerIMaterialBySlotIndex(i) != NULL )
				{
					MDLHandle_t pStickerMdl = GetModelPanel()->SetMergeMDL( m_ItemData.GetStickerSlotModelBySlotIndex(i) );
					if ( CMDL *pStickerMdlCMDL = ( pStickerMdl != MDLHANDLE_INVALID ) ? GetModelPanel()->GetMergeMDL( pStickerMdl ) : NULL )
					{
						pStickerMdlCMDL->SetSimpleMaterialOverride( m_ItemData.GetStickerIMaterialBySlotIndex(i) );
					}
				}
			}
		}

		//
		// Apply all the configuration light settings
		//
		GetModelPanel()->SetLightAmbient( ParseSettingsVectorFromString( m_kvConfiguration->GetString( "light_ambient", "[0.11 0.11 0.12]" ) ) );
		GetModelPanel()->ClearDirectionalLights();
		int iDirectionalLight = 0;
		for ( KeyValues *kvDirectionalLights = m_kvConfiguration->FindKey( "light_directional" )->GetFirstSubKey(); kvDirectionalLights; kvDirectionalLights = kvDirectionalLights->GetNextKey() )
		{
			char const *sz = kvDirectionalLights->GetString();
			Vector rgb = vec3_origin, dir = vec3_origin;
			if ( char const *szRGB = strstr( sz, "rgb[" ) )
				rgb = ParseSettingsVectorFromString( szRGB + 3 );
			if ( char const *szDIR = strstr( sz, "dir[" ) )
				dir = ParseSettingsVectorFromString( szDIR + 3 );
			char chLightAttachmentPoint[128] = {};
			if ( char const *szLightAttachmentPoint = strstr( sz, "attach[" ) )
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
				GetModelPanel()->UpdateDirectionalLight( iDirectionalLight, Color( rgb.x, rgb.y, rgb.z ), dir );
				if ( chLightAttachmentPoint[0] )
					GetModelPanel()->SetDirectionalLightAttachment( iDirectionalLight, chLightAttachmentPoint );
				iDirectionalLight ++;
			}
		}

		// restore the root animation or reset it
		if ( nRootSequence != ACT_INVALID )
		{
			GetModelPanel()->GetMDL()->m_flTime = flRootTime;
			GetModelPanel()->GetMDL()->m_flTimeBasisAdjustment = flRootTimeBasisAdjustment;
			GetModelPanel()->GetMDL()->m_nSequence = nRootSequence;
		}
		else
		{
			GetModelPanel()->SetModelAnim( m_kvConfiguration->GetString( "root_anim", "ACT_IDLE_INSPECT" ), true );
		}
		GetModelPanel()->ClearModelAnimFollowLoop();
		GetModelPanel()->AddModelAnimFollowLoop( m_kvConfiguration->GetString( "root_anim_loop", "ACT_IDLE_INSPECT" ), true );

		// restore the weapon animation or reset it
		if ( CMDL *pMdlWeaponConfig = ( m_hMdlWeapon != MDLHANDLE_INVALID ) ? GetModelPanel()->GetMergeMDL( m_hMdlWeapon ) : NULL )
		{
			MDLCACHE_CRITICAL_SECTION();

			CStudioHdr studioHdr( pMdlWeaponConfig->GetStudioHdr(), g_pMDLCache );

			if ( nWeaponSequence != ACT_INVALID )
			{
				pMdlWeaponConfig->m_nSequence = nWeaponSequence;
				pMdlWeaponConfig->m_bUseSequencePlaybackFPS = true;
				pMdlWeaponConfig->m_flTime = flWeaponTime;
				pMdlWeaponConfig->m_flTimeBasisAdjustment = flWeaponTimeBasisAdjustment;
			}
			else
			{
				char const *szWeaponAnim = m_kvConfiguration->GetString( "weapon_anim", "ACT_IDLE_INSPECT" );
				
				nWeaponSequence = HelperGetSequenceIdFromStudioHdr( studioHdr, szWeaponAnim );
				if ( nWeaponSequence != ACT_INVALID )
				{
					pMdlWeaponConfig->m_nSequence = nWeaponSequence;
					pMdlWeaponConfig->m_bUseSequencePlaybackFPS = true;
					pMdlWeaponConfig->m_flTimeBasisAdjustment = pMdlWeaponConfig->m_flTime;
				}
			}

			pMdlWeaponConfig->m_arrSequenceFollowLoop.RemoveAll();

			char const *szWeaponAnimLoop = m_kvConfiguration->GetString( "weapon_anim_loop", "ACT_IDLE_INSPECT" );
			int nLoopSequence = HelperGetSequenceIdFromStudioHdr( studioHdr, szWeaponAnimLoop );
			if ( nLoopSequence != ACT_INVALID && nLoopSequence != nWeaponSequence )
			{
				pMdlWeaponConfig->m_arrSequenceFollowLoop.AddToTail( nLoopSequence );
			}
		}

		GetModelPanel()->SetCameraAttachment( m_kvConfiguration->GetString( "root_camera", "attach_camera_inspect" ) );
		GetModelPanel()->SetRenderCaptureCameraAttachment( m_kvConfiguration->GetString( "shadow_light", "attach_camera_inspect_light" ) );
		
		GetModelPanel()->SetCameraFOV( m_kvConfiguration->GetFloat( "root_camera_fov", 54.0f ) );

		Vector vecCameraOffset;
		sscanf( m_kvConfiguration->GetString( "camera_offset", "0 0 0" ), "%f %f %f", &vecCameraOffset.x, &vecCameraOffset.y, &vecCameraOffset.z);
		GetModelPanel()->SetCameraPositionOverride(vecCameraOffset);
		GetModelPanel()->SetCameraPositionOverrideEnabled(true);

		Vector vecCameraOffsetAngle;
		sscanf(m_kvConfiguration->GetString("camera_orient", "0 0 0"), "%f %f %f", &vecCameraOffsetAngle.x, &vecCameraOffsetAngle.y, &vecCameraOffsetAngle.z);
		GetModelPanel()->SetCameraOrientOverride(vecCameraOffsetAngle);
		GetModelPanel()->SetCameraOrientOverrideEnabled(true);

		// update shadow config stuff (doesn't change the shadow_light_texture)
		m_cfgRenderCapture.m_renderFlashlightState.m_NearZ = m_kvConfiguration->GetFloat( "shadow_light_znear", 4.0f );
		m_cfgRenderCapture.m_renderFlashlightState.m_FarZ = m_kvConfiguration->GetFloat( "shadow_light_zfar", 512.0f );
		
		m_cfgRenderCapture.m_renderFlashlightState.m_fHorizontalFOVDegrees = m_kvConfiguration->GetFloat( "shadow_light_hfov", 54.0f );
		m_cfgRenderCapture.m_renderFlashlightState.m_fVerticalFOVDegrees = m_kvConfiguration->GetFloat( "shadow_light_vfov", 54.0f );
		
		m_cfgRenderCapture.m_renderFlashlightState.m_fQuadraticAtten = m_kvConfiguration->GetFloat( "shadow_light_atten_quadratic", 0.0f );
		m_cfgRenderCapture.m_renderFlashlightState.m_fLinearAtten = m_kvConfiguration->GetFloat( "shadow_light_atten_linear", 512.0f );
		m_cfgRenderCapture.m_renderFlashlightState.m_fConstantAtten = m_kvConfiguration->GetFloat( "shadow_light_atten_constant", 0.0f );
		m_cfgRenderCapture.m_renderFlashlightState.m_FarZAtten = m_kvConfiguration->GetFloat( "shadow_light_atten_farz", 512.0f );
		
		m_cfgRenderCapture.m_renderFlashlightState.m_fBrightnessScale = m_kvConfiguration->GetFloat( "shadow_light_brightness", 1.0f );
		
		Vector vecShadowLightColor = ParseSettingsVectorFromString( m_kvConfiguration->GetString( "shadow_light_color" ) );
		m_cfgRenderCapture.m_renderFlashlightState.m_Color[0] = vecShadowLightColor.x;
		m_cfgRenderCapture.m_renderFlashlightState.m_Color[1] = vecShadowLightColor.y;
		m_cfgRenderCapture.m_renderFlashlightState.m_Color[2] = vecShadowLightColor.z;
		m_cfgRenderCapture.m_renderFlashlightState.m_Color[3] = 1.0f;

		m_cfgRenderCapture.m_renderFlashlightState.m_nSpotlightTextureFrame = 0;
		m_cfgRenderCapture.m_renderFlashlightState.m_pProjectedMaterial = NULL;

		PerformLayout();
	}
}

void CModelPanelWeaponPreview::PlayAnimOnWeapon( const char* pszAnimName )
{
	if ( !GetModelPanel() )
		return;

	if ( CMDL *pMdlWeaponConfig = ( m_hMdlWeapon != MDLHANDLE_INVALID ) ? GetModelPanel()->GetMergeMDL( m_hMdlWeapon ) : NULL )
	{
		MDLCACHE_CRITICAL_SECTION();

		CStudioHdr studioHdr( pMdlWeaponConfig->GetStudioHdr(), g_pMDLCache );

		int nSequence = HelperGetSequenceIdFromStudioHdr( studioHdr, pszAnimName );
		if ( nSequence != ACT_INVALID )
		{
			pMdlWeaponConfig->m_nSequence = nSequence;
			pMdlWeaponConfig->m_flTimeBasisAdjustment = pMdlWeaponConfig->m_flTime;

			// do this to go back to the loop anim again afterwards, but only if it doesn't already have a loop anim queued
			char const *szWeaponAnimLoop = m_kvConfiguration->GetString( "weapon_anim_loop", "ACT_IDLE_INSPECT" );
			int nLoopSequence = HelperGetSequenceIdFromStudioHdr( studioHdr, szWeaponAnimLoop );
			if ( nLoopSequence != ACT_INVALID )
			{
				if ( pMdlWeaponConfig->m_arrSequenceFollowLoop.Count() == 0 || pMdlWeaponConfig->m_arrSequenceFollowLoop.Tail() != nLoopSequence )
				{
					pMdlWeaponConfig->m_arrSequenceFollowLoop.AddToTail( nLoopSequence );
				}
			}
		}
	}
}

float CModelPanelWeaponPreview::GetAnimationEndTime( bool bWeapon )
{
	if ( !GetModelPanel() )
		return -1.0f;

	if ( bWeapon )
	{
		if ( CMDL *pMdlWeaponConfig = ( m_hMdlWeapon != MDLHANDLE_INVALID ) ? GetModelPanel()->GetMergeMDL( m_hMdlWeapon ) : NULL )
		{
			return pMdlWeaponConfig->m_flCurrentAnimEndTime;	
		}
	}
	else
	{
		if ( GetModelPanel()->GetMDL() )
		{
			return GetModelPanel()->GetMDL()->m_flCurrentAnimEndTime;
		}
	}
	return -1.0f;
}

float CModelPanelWeaponPreview::GetAnimationTime( bool bWeapon )
{
	if ( !GetModelPanel() )
		return -1.0f;

	if ( bWeapon )
	{
		if ( CMDL *pMdlWeaponConfig = ( m_hMdlWeapon != MDLHANDLE_INVALID ) ? GetModelPanel()->GetMergeMDL( m_hMdlWeapon ) : NULL )
		{
			return pMdlWeaponConfig->m_flTime - pMdlWeaponConfig->m_flTimeBasisAdjustment;
		}
	}
	else
	{
		if ( GetModelPanel()->GetMDL() )
		{
			return GetModelPanel()->GetMDL()->m_flTime - GetModelPanel()->GetMDL()->m_flTimeBasisAdjustment;
		}
	}

	return -1.0f;
}

void CModelPanelWeaponPreview::SetAnimationTime( float flTime, bool bWeapon  )
{
	if ( !GetModelPanel() )
		return;

	if ( bWeapon )
	{
		if ( CMDL *pMdlWeaponConfig = ( m_hMdlWeapon != MDLHANDLE_INVALID ) ? GetModelPanel()->GetMergeMDL( m_hMdlWeapon ) : NULL )
		{
			pMdlWeaponConfig->m_flTime = pMdlWeaponConfig->m_flTimeBasisAdjustment + flTime;
		}
	}
	else
	{
		if ( GetModelPanel()->GetMDL() )
		{
			GetModelPanel()->GetMDL()->m_flTime = GetModelPanel()->GetMDL()->m_flTimeBasisAdjustment + flTime;
		}
	}
}

const char *CModelPanelWeaponPreview::GetWeaponCustomVTFName()
{
	if ( !GetModelPanel() || !GetModelPanel()->GetMergeMDL( m_hMdlWeapon ) || !GetModelPanel()->GetMergeMDL( m_hMdlWeapon )->GetCustomMaterialCount() )
		return NULL;

	ICustomMaterial *pCustomMaterial = GetModelPanel()->GetMergeMDL( m_hMdlWeapon )->GetCustomMaterial( 0 );
	ICompositeTexture *pCompositeTexture = pCustomMaterial->GetTexture( 0 );
	return pCompositeTexture->GetName();
}

const char *CModelPanelWeaponPreview::GetPatternVTFName()
{
	if ( m_ItemData.GetVisualsDataProcessor( 0 ) )
	{
		return m_ItemData.GetVisualsDataProcessor( 0 )->GetPatternVTFName();
	}

	return NULL;
}

bool HelperWeaponPreviewGetStatTrakScore( void *, int *puiScore )
{
	if ( !g_pModelPanelWeaponPreview )
		return false;
	CEconItemView const *pItemView = g_pModelPanelWeaponPreview->GetWeaponPreviewItemData();
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
				nKillEater = MAX( nKillEater, static_cast< int >( nKillEaterAltScore ) );
			}
			break;
		}
	}

	if ( puiScore )
		*puiScore = nKillEater;
	return true;
}

bool HelperWeaponPreviewGetLabel( void *, const char **p_szLabel )
{
	if ( !g_pModelPanelWeaponPreview )
		return false;
	CEconItemView const *pItemView = g_pModelPanelWeaponPreview->GetWeaponPreviewItemData();
	if ( !pItemView )
		return false;

	if ( p_szLabel && pItemView->GetCustomName() )
		*p_szLabel = pItemView->GetCustomName();
	return true;
}

static int HelperWeaponPreviewCoordinate( char const *val, int minValue, int valRange, vgui::HScheme hScheme )
{
	int nBase = 0;
	int nFactor = 1;
	int nDenom = 1;
	bool bProportional = true;

	switch ( val[0] )
	{
	case 'c':
		nBase = ( minValue + valRange/2 );
		++ val;
		break;
	case 'l':
		nBase = minValue;
		++ val;
		break;
	case 'r':
		nBase = ( minValue + valRange );
		++ val;
		break;
	}

	switch ( val[0] )
	{
	case '%':
		nFactor = valRange;
		nDenom = 100;
		bProportional = false;
		++ val;
		break;
	}

	int nActualVal = Q_atoi( val );
	if ( bProportional )
		nActualVal = vgui::scheme()->GetProportionalScaledValueEx( hScheme, nActualVal );

	return ( nBase + ( nActualVal * nFactor ) / nDenom );
}

void AccumulateWeaponPreviewSettingsStringValue( KeyValues *kvGlobal, KeyValues *kvExtra, char const *szName )
{
	if ( char const *szNewValue = kvExtra->GetString( szName, NULL ) )
	{
		if ( char const *szOldValue = kvGlobal->GetString( szName, NULL ) )
		{
			DevMsg( "%s overrides '%s' with '%s'\n", szName, szOldValue, szNewValue );
		}
		else
		{
			DevMsg( "%s = '%s'\n", szName, szNewValue );
		}
		kvGlobal->SetString( szName, szNewValue );
	}
}

void AccumulateWeaponPreviewSettingsFloatValue( KeyValues *kvGlobal, KeyValues *kvExtra, char const *szName )
{
	KeyValues *kvKey = kvExtra->FindKey( szName );
	if ( !kvKey )
		return;

	KeyValues *kvOldKey = kvGlobal->FindKey( szName );
	if ( kvOldKey )
	{
		DevMsg( "%s overrides '%.3f' with '%.3f'\n", szName, kvOldKey->GetFloat(), kvKey->GetFloat() );
	}
	else
	{
		DevMsg( "%s = '%.3f'\n", szName, kvKey->GetFloat() );
	}
	kvGlobal->SetFloat( szName, kvKey->GetFloat() );
}

void AccumulateWeaponPreviewSettingsConfig( KeyValues *kvGlobal, KeyValues *kvExtra )
{
	AccumulateWeaponPreviewSettingsStringValue( kvGlobal, kvExtra, "root_mdl" );
	AccumulateWeaponPreviewSettingsStringValue( kvGlobal, kvExtra, "root_anim" );
	AccumulateWeaponPreviewSettingsStringValue( kvGlobal, kvExtra, "root_anim_loop" );
	AccumulateWeaponPreviewSettingsStringValue( kvGlobal, kvExtra, "weapon_anim" );
	AccumulateWeaponPreviewSettingsStringValue( kvGlobal, kvExtra, "weapon_anim_loop" );
	AccumulateWeaponPreviewSettingsStringValue( kvGlobal, kvExtra, "root_camera" );
	AccumulateWeaponPreviewSettingsFloatValue( kvGlobal, kvExtra, "root_camera_fov" );

	AccumulateWeaponPreviewSettingsStringValue( kvGlobal, kvExtra, "shadow_light" );
	AccumulateWeaponPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_znear" );
	AccumulateWeaponPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_zfar" );
	AccumulateWeaponPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_hfov" );
	AccumulateWeaponPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_vfov" );
	AccumulateWeaponPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_atten_quadratic" );
	AccumulateWeaponPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_atten_linear" );
	AccumulateWeaponPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_atten_constant" );
	AccumulateWeaponPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_atten_farz" );
	AccumulateWeaponPreviewSettingsFloatValue( kvGlobal, kvExtra, "shadow_light_brightness" );
	AccumulateWeaponPreviewSettingsStringValue( kvGlobal, kvExtra, "shadow_light_color" );
	AccumulateWeaponPreviewSettingsStringValue( kvGlobal, kvExtra, "shadow_light_texture" );

	AccumulateWeaponPreviewSettingsStringValue( kvGlobal, kvExtra, "light_ambient" );

	AccumulateWeaponPreviewSettingsStringValue(kvGlobal, kvExtra, "camera_offset" );;
	AccumulateWeaponPreviewSettingsStringValue(kvGlobal, kvExtra, "camera_orient");

	if ( kvExtra->GetBool( "light_directional_clearall" ) )
	{
		if ( KeyValues *kvLights = kvGlobal->FindKey( "light_directional" ) )
		{
			DevMsg( "light_directional_clearall clears the following directional lights:\n" );
			KeyValuesDumpAsDevMsg( kvGlobal->FindKey( "light_directional" ), 1, 1 );
			kvGlobal->RemoveSubKey( kvLights );
			kvLights->deleteThis();
		}
	}

	if ( kvExtra->GetBool( "mergemdls_clearall" ) )
	{
		if ( KeyValues *kvMergeMdls = kvGlobal->FindKey( "mergemdls" ) )
		{
			DevMsg( "mergemdls_clearall clears the following mergemdls:\n" );
			KeyValuesDumpAsDevMsg( kvGlobal->FindKey( "mergemdls" ), 1, 1 );
			kvGlobal->RemoveSubKey( kvMergeMdls );
			kvMergeMdls->deleteThis();
		}
	}
	
	if ( kvExtra->GetBool( "weaponmergemdls_clearall" ) )
	{
		if ( KeyValues *kvMergeMdls = kvGlobal->FindKey( "weaponmergemdls" ) )
		{
			DevMsg( "weaponmergemdls_clearall clears the following weaponmergemdls:\n" );
			KeyValuesDumpAsDevMsg( kvGlobal->FindKey( "weaponmergemdls" ), 1, 1 );
			kvGlobal->RemoveSubKey( kvMergeMdls );
			kvMergeMdls->deleteThis();
		}
	}

	for ( KeyValues *kvSub = kvExtra->GetFirstSubKey(); kvSub; kvSub = kvSub->GetNextKey() )
	{
		if ( !Q_stricmp( kvSub->GetName(), "light_directional_add" ) )
		{
			char const *szVal = kvSub->GetString();
			DevMsg( "light_directional_add '%s'\n", szVal );
			kvGlobal->FindKey( "light_directional", true )->AddSubKey( kvSub->MakeCopy() );
		}
		else if ( !Q_stricmp( kvSub->GetName(), "mergemdl_add" ) )
		{
			char const *szVal = kvSub->GetString();
			DevMsg( "mergemdl_add '%s'\n", szVal );
			kvGlobal->FindKey( "mergemdls", true )->AddSubKey( kvSub->MakeCopy() );
		}
		else if ( !Q_stricmp( kvSub->GetName(), "weaponmergemdl_add" ) )
		{
			char const *szVal = kvSub->GetString();
			DevMsg( "weaponmergemdl_add '%s'\n", szVal );
			kvGlobal->FindKey( "weaponmergemdls", true )->AddSubKey( kvSub->MakeCopy() );
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
						DevMsg( "mergemdl_clear '%s'\n", szVal );
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
						DevMsg( "weaponmergemdl_clear '%s'\n", szVal );
						kvMergeMdls->RemoveSubKey( kvMergeMdl );
						break;
					}
				}
			}
		}
	}
}

KeyValues * BuildWeaponPreviewSettingsForEconItemView( const CEconItemView *pItem, KeyValues *kvManifest, PreviewMode previewMode = PreviewMode_Workbench )
{
	extern bool Helper_IsSpray( const CEconItemDefinition * pEconItemDefinition );
	//
	// Determine rule processing params
	//
	char szItemModelBaseName[MAX_PATH] = {};
	if ( char const *szModel = ( previewMode == PreviewMode_Hold ) ? pItem->GetPlayerDisplayModel() : pItem->GetPedestalDisplayModel() )
	{
		V_FileBase( szModel, szItemModelBaseName, Q_ARRAYSIZE( szItemModelBaseName ) );
	}
	DevMsg( "BuildWeaponPreviewSettingsForEconItemView: model = '%s'\n", szItemModelBaseName );
	
	char const *szItemType = pItem->GetStaticData() ? pItem->GetStaticData()->GetWeaponTypeString() : "other";
	if ( !szItemType )
	{
		if ( pItem->IsStickerTool() )
		{
			szItemType = "sticker_tool";
		}
		else if ( Helper_IsSpray( pItem->GetItemDefinition() ) )
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
	DevMsg( "BuildWeaponPreviewSettingsForEconItemView: type = '%s'\n", szItemType );

	int nItemTeam = pItem->GetStaticData() ? pItem->GetStaticData()->GetUsedByTeam() : TEAM_UNASSIGNED;
	char const *szItemTeam = ( nItemTeam == TEAM_UNASSIGNED ) ? "Any" : ( nItemTeam == TEAM_CT ) ? "CT" : "T";
	DevMsg( "BuildWeaponPreviewSettingsForEconItemView: type = '%s'\n", szItemTeam );

	bool bWorkshop = ( pItem->GetCustomPaintKit() && ( pItem->GetCustomPaintKit()->nID & 0xFFFFFFF0 ) == PREVIEW_PAINTKIT_ID_BASE ) ? true : false;
	bool bWorkshopArms = ( previewMode == PreviewMode_Hold );
	bool bWorkshopGreenScreen = ( previewMode == PreviewMode_GreenScreen );
	DevMsg( "BuildWeaponPreviewSettingsForEconItemView: workshop = '%s'\n", !bWorkshop ? "None" : ( bWorkshopArms ? "Arms" : ( bWorkshopGreenScreen ? "Green Screen" : "Workbench" ) ) );

	int nItemQuality = pItem->GetQuality();
	DevMsg( "BuildWeaponPreviewSettingsForEconItemView: quality = %d\n", nItemQuality );

	int nItemRarity = pItem->GetRarity();
	DevMsg( "BuildWeaponPreviewSettingsForEconItemView: rarity = %d\n", nItemRarity );

	static CSchemaAttributeDefHandle pAttrDef_KillEater( "kill eater" );
	bool bStatTrak = pItem->FindAttribute( pAttrDef_KillEater );
	DevMsg( "BuildWeaponPreviewSettingsForEconItemView: stattrak = %s\n", bStatTrak ? "yes" : "no" );

	bool bUidNameTag = (pItem->GetCustomName() != NULL);
	DevMsg( "BuildWeaponPreviewSettingsForEconItemView: nametag = %s\n", bUidNameTag ? "yes" : "no" );

	bool bPreviewingStickers = ( previewMode == PreviewMode_Stickers );
	DevMsg( "BuildWeaponPreviewSettingsForEconItemView: previewing stickers = %s\n", bPreviewingStickers ? "yes" : "no" );
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
				DevMsg( "BuildWeaponPreviewSettingsForEconItemView section '%s' skipped due to model rule '%s'\n", kvSection->GetName(), szRuleModel );
				continue;
			}
		}
		if ( char const *szRuleModelPartial = kvRuleRequirement->GetString( "model_partial", NULL ) )
		{
			if ( !Q_stristr( szItemModelBaseName, szRuleModelPartial ) )
			{
				DevMsg( "BuildWeaponPreviewSettingsForEconItemView section '%s' skipped due to model_partial rule '%s'\n", kvSection->GetName(), szRuleModelPartial );
				continue;
			}
		}
		if ( char const *szRuleType = kvRuleRequirement->GetString( "type", NULL ) )
		{
			if ( Q_stricmp( szRuleType, szItemType ) )
			{
				DevMsg( "BuildWeaponPreviewSettingsForEconItemView section '%s' skipped due to type rule '%s'\n", kvSection->GetName(), szRuleType );
				continue;
			}
		}
			
		if ( char const *szRuleTeam = kvRuleRequirement->GetString( "team", NULL ) )
		{
			if ( Q_stricmp( szRuleTeam, szItemTeam ) )
			{
				DevMsg( "BuildWeaponPreviewSettingsForEconItemView section '%s' skipped due to team rule '%s'\n", kvSection->GetName(), szRuleTeam );
				continue;
			}
		}

		int nRuleQuality = kvRuleRequirement->GetInt( "quality", -1 );
		if ( ( nRuleQuality >= 0 ) && ( nItemQuality != nRuleQuality ) )
		{
			DevMsg( "BuildWeaponPreviewSettingsForEconItemView section '%s' skipped due to quality rule '%d'\n", kvSection->GetName(), nRuleQuality );
			continue;
		}

		int nRuleRarity = kvRuleRequirement->GetInt( "rarity", -1 );
		if ( ( nRuleRarity >= 0 ) && ( nItemRarity < nRuleRarity ) )
		{
			DevMsg( "BuildWeaponPreviewSettingsForEconItemView section '%s' skipped due to rarity rule '%d'\n", kvSection->GetName(), nRuleRarity );
			continue;
		}

		int nRuleStatTrak = kvRuleRequirement->GetInt( "stattrak", -1 );
		if ( ( nRuleStatTrak >= 0 ) && ( bStatTrak != !!nRuleStatTrak ) )
		{
			DevMsg( "BuildWeaponPreviewSettingsForEconItemView section '%s' skipped due to stattrak rule '%d'\n", kvSection->GetName(), nRuleStatTrak );
			continue;
		}

		int nRuleUidNameTag = kvRuleRequirement->GetInt( "nametag", -1 );
		if ( ( nRuleUidNameTag >= 0 ) && ( bUidNameTag != !!nRuleUidNameTag ) )
		{
			DevMsg( "BuildWeaponPreviewSettingsForEconItemView section '%s' skipped due to nametag rule '%d'\n", kvSection->GetName(), nRuleUidNameTag );
			continue;
		}

		int nRuleWorkshop = kvRuleRequirement->GetInt( "workshop", -1 );
		if ( ( nRuleWorkshop >= 0 ) && ( bWorkshop != !!nRuleWorkshop ) )
		{
			DevMsg( "BuildWeaponPreviewSettingsForEconItemView section '%s' skipped due to workshop rule '%d'\n", kvSection->GetName(), nRuleWorkshop );
			continue;
		}

		int nRuleWorkshopArms = kvRuleRequirement->GetInt( "workshop_arms", -1 );
		if ( ( nRuleWorkshopArms >= 0 ) && ( bWorkshopArms != !!nRuleWorkshopArms ) )
		{
			DevMsg( "BuildWeaponPreviewSettingsForEconItemView section '%s' skipped due to workshop_arms rule '%d'\n", kvSection->GetName(), nRuleWorkshopArms );
			continue;
		}
		else if ( nRuleWorkshopArms >= 0 && !bWorkshop )
		{
			DevMsg( "BuildWeaponPreviewSettingsForEconItemView section '%s' skipped due to workshop mode off\n", kvSection->GetName() );
			continue;
		}

		int nRuleWorkshopGreenScreen = kvRuleRequirement->GetInt( "workshop_greenscreen", -1 );
		if ( ( nRuleWorkshopGreenScreen >= 0 ) && ( bWorkshopGreenScreen != !!nRuleWorkshopGreenScreen ) )
		{
			DevMsg( "BuildWeaponPreviewSettingsForEconItemView section '%s' skipped due to workshop_greeenscreen rule '%d'\n", kvSection->GetName(), nRuleWorkshopGreenScreen );
			continue;
		}
		else if ( nRuleWorkshopGreenScreen >= 0 && !bWorkshop )
		{
			DevMsg( "BuildWeaponPreviewSettingsForEconItemView section '%s' skipped due to workshop mode off\n", kvSection->GetName() );
			continue;
		}

		int nRuleStickerPreview = kvRuleRequirement->GetInt("sticker_preview", -1);
		if ((nRuleStickerPreview >= 0) && (bPreviewingStickers != !!nRuleStickerPreview))
		{
			DevMsg("BuildWeaponPreviewSettingsForEconItemView section '%s' skipped due to sticker preview rule '%d'\n", kvSection->GetName(), nRuleStickerPreview);
			continue;
		}

		// Here we know that all the rules passed!
		DevMsg( "BuildWeaponPreviewSettingsForEconItemView section '%s' rules matched\n", kvSection->GetName() );
		AccumulateWeaponPreviewSettingsConfig( kvConfig, kvSection->FindKey( "config" ) );
	}

	DevMsg( "BuildWeaponPreviewSettingsForEconItemView configuration:\n" );
	KeyValuesDumpAsDevMsg( kvConfig, 1, 1 );

	return kvConfig;
}

static const CEconItemView * HelperLaunchWeaponPreviewItemResolve( uint64 itemid )
{
	if ( CombinedItemIdIsDefIndexAndPaint( itemid ) )
		return CEconItemView::FindOrCreateEconItemViewForItemID( itemid );
	else if ( itemid == ITEMID_WEAPONPREVIEW_EXTERNAL1 )
		return CModelPanelWeaponPreview::sm_pExternalEconItemView1;
	else if ( itemid == ITEMID_WEAPONPREVIEW_EXTERNAL2 )
		return CModelPanelWeaponPreview::sm_pExternalEconItemView2;
	else
		return CEconItemView::FindOrCreateEconItemViewForItemID( itemid );
}

void LaunchWeaponPreviewPanelHelper( char const *szString )
{
	if ( !szString || !*szString )
	{	// Kill the panel
		delete g_pModelPanelWeaponPreview;
		g_pModelPanelWeaponPreview = NULL;
		return;
	}

	if ( g_pModelPanelWeaponPreview )
		return;

	vgui::Panel *pRootPanel = BasePanel();
	if ( !pRootPanel )
		return;

	bool bPreviewingSingleSticker = false;

	const CEconItemView *pEquippedWeapon = NULL;
	if ( !Q_stricmp( szString, "ak" ) )
	{
		CCSPlayerInventory *pLocalInv = CSInventoryManager()->GetLocalCSInventory();
		if ( !pLocalInv )
			return;
		
		pEquippedWeapon = pLocalInv->GetItemInLoadout( TEAM_TERRORIST, LOADOUT_POSITION_RIFLE1 );
	}
	else if ( char const *szInventoryItemId = StringAfterPrefix( szString, "img://inventory_" ) )
	{
		uint64 itemid = Q_atoui64( szInventoryItemId );
		pEquippedWeapon = HelperLaunchWeaponPreviewItemResolve( itemid );
	}
	else if ( StringHasPrefix( szString, "img://itemdata_" ) )
	{
		CUtlVector< char* > urlFragments;

		V_SplitString( szString, "_", urlFragments );

		uint16 iDefIndex = ( uint16 ) atoi( urlFragments[1] );
		uint16 iPaintIndex = ( uint16 ) atoi( urlFragments[2] );
		uint64 ullItemId = CombinedItemIdMakeFromDefIndexAndPaint( iDefIndex, iPaintIndex );
		pEquippedWeapon = CEconItemView::FindOrCreateEconItemViewForItemID( ullItemId );
		urlFragments.PurgeAndDeleteElements();
	}
	else if ( char const *szInventoryItemIdSticker = StringAfterPrefix( szString, "vmt://stickerpreview_" ) )
	{
		
		uint64 itemid = Q_atoui64( szInventoryItemIdSticker );
		pEquippedWeapon = HelperLaunchWeaponPreviewItemResolve( itemid );

		if ( !pEquippedWeapon->GetStaticData()->IsTool() )
			return;

		bPreviewingSingleSticker = true;

	}
	else if ( char const *szInventoryItemIdSpray = StringAfterPrefix( szString, "vmt://spraypreview_" ) )
	{

		uint64 itemid = Q_atoui64( szInventoryItemIdSpray );
		pEquippedWeapon = HelperLaunchWeaponPreviewItemResolve( itemid );

		if ( !pEquippedWeapon->GetStaticData()->IsTool() )
			return;

		bPreviewingSingleSticker = true;

	}
	
	if ( !pEquippedWeapon )
		return;

	if ( !bPreviewingSingleSticker )
	{
		char const *szEquippedPedestalDisplayModel = pEquippedWeapon->GetPedestalDisplayModel();
		if ( !szEquippedPedestalDisplayModel || !*szEquippedPedestalDisplayModel )
		{
			DevWarning( "LaunchWeaponPreviewPanelHelper cannot find model for item %llu/%p!\n", pEquippedWeapon->GetItemID(), pEquippedWeapon->GetItemDefinition() );
			return;
		}
	}

	KeyValues *kvManifest = new KeyValues( "manifest" );
	KeyValues::AutoDelete autodelete_kvManifest( kvManifest );
	if ( !kvManifest->LoadFromFile( g_pFullFileSystem, "resource/ui/econ/ItemModelPanelWeaponPreviewManifest.res" ) )
		return;

	//[OGS] UIEvent, Inspect. Categorized by player state, coarse item type.
	if ( !engine->IsConnected() )
	{
		//Main Menu
		CSAppLifetimeGameStats()->RecordUIEvent( pEquippedWeapon->GetItemID() ? "WeaponInspectMainMenuPainted" : "WeaponInspectMainMenuDefault"  );		
	}
	else
	{
		//Connected
		CSAppLifetimeGameStats()->RecordUIEvent( pEquippedWeapon->GetItemID() ? "WeaponInspectConnectedPainted" : "WeaponInspectConnectedDefault"  );		
	}				

	PreviewMode PanelPreviewMode = PreviewMode_Default;
	if ( Q_stristr(szString, "?stickers") )
	{
		PanelPreviewMode = PreviewMode_Stickers;
	}

	KeyValues *kvPreviewSettings = BuildWeaponPreviewSettingsForEconItemView( pEquippedWeapon, kvManifest, PanelPreviewMode );
	KeyValues::AutoDelete autodelete_kvPreviewSettings( kvPreviewSettings );

	int x,y,wide,tall;
	pRootPanel->GetBounds( x, y, wide, tall );

	g_pModelPanelWeaponPreview = new CModelPanelWeaponPreview( NULL, "EconWeaponInspectionPanel", kvPreviewSettings, pEquippedWeapon );
	g_pModelPanelWeaponPreview->MakePopup( false );
	
	bool bInGame = engine->IsConnected();
	bool bInGameLiveClientView = bInGame && !enginevgui->IsGameUIVisible();
	vgui::VPANEL gameuiPanel = enginevgui->GetPanel( bInGameLiveClientView ? PANEL_GAMEDLL : PANEL_GAMEUIDLL );
	g_pModelPanelWeaponPreview->SetParent( gameuiPanel );

	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/ClientScheme.res", "ClientScheme");
	g_pModelPanelWeaponPreview->SetScheme(scheme);
	g_pModelPanelWeaponPreview->SetProportional( true );

	if ( KeyValues *kvXY = kvManifest->FindKey( bInGame ? ( bInGameLiveClientView ? "MainWindowLayout/rootbounds_inlivegame" : "MainWindowLayout/rootbounds_ingame" ) : "MainWindowLayout/rootbounds_mainmenu" ) )
	{
		int bndXYleft = x;
		int bndXYtop = y;
		int bndXYright = x + wide;
		int bndXYbottom = y + tall;

		if ( char const *val = kvXY->GetString( "x_left" ) )
			bndXYleft = HelperWeaponPreviewCoordinate( val, x, wide, scheme );
		if ( char const *val = kvXY->GetString( "x_right" ) )
			bndXYright = HelperWeaponPreviewCoordinate( val, x, wide, scheme );
		if ( char const *val = kvXY->GetString( "y_top" ) )
			bndXYtop = HelperWeaponPreviewCoordinate( val, y, tall, scheme );
		if ( char const *val = kvXY->GetString( "y_bottom" ) )
			bndXYbottom = HelperWeaponPreviewCoordinate( val, y, tall, scheme );

		g_pModelPanelWeaponPreview->SetBounds( bndXYleft, bndXYtop, bndXYright - bndXYleft, bndXYbottom - bndXYtop );
	}

	g_pModelPanelWeaponPreview->SetMouseInputEnabled( false );
	g_pModelPanelWeaponPreview->SetKeyBoardInputEnabled( false );
	g_pModelPanelWeaponPreview->SetVisible( true );
	g_pModelPanelWeaponPreview->MoveToFront();
	g_pModelPanelWeaponPreview->PerformLayout();
}


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
	// $$$REI Leaky creation of reference econ items.  Need a system to manage econitemview lifetimes.
	if ( bStickerGraphic )
		pNewItemView = InventoryManager()->LeakyCreateReferenceEconItem( protoData.defindex(),
			protoData.stickers().size() ? protoData.stickers( 0 ).sticker_id() : 0,
			protoData.stickers().size() ? protoData.stickers( 0 ).tint_id() : 0 );
	else if ( bMusicKit )
		pNewItemView = InventoryManager()->LeakyCreateReferenceEconItem( protoData.defindex(), protoData.musicindex() );
	else
		pNewItemView = InventoryManager()->LeakyCreateReferenceEconItem( protoData.defindex(), protoData.paintindex() );
	
	pNewItemView->SetItemQuality( protoData.quality() );
	pNewItemView->SetItemQualityOverride( protoData.quality() );
	pNewItemView->SetItemRarityOverride( protoData.rarity() );
	uint32 uiPaintWear = protoData.paintwear();
	pNewItemView->SetOrAddAttributeValueByName( "set item texture wear", *reinterpret_cast< float * >( &uiPaintWear ) );
	pNewItemView->SetOrAddAttributeValueByName( "set item texture seed", protoData.paintseed() );
	if ( protoData.has_killeatervalue() )
	{
		pNewItemView->SetOrAddAttributeValueByName( "kill eater score type", protoData.killeaterscoretype() );
		uint32 nStatTrak = protoData.killeatervalue();
		float flStatTrak = *reinterpret_cast< float* >( ( uint32 * ) &nStatTrak );
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
		pNewItemView->SetOrAddAttributeValueByName( CFmtStr( "sticker slot %d id", infoSticker.slot() ), *reinterpret_cast< float * >( &uiStickerID ) );
		pNewItemView->SetOrAddAttributeValueByName( CFmtStr( "sticker slot %d wear", infoSticker.slot() ), infoSticker.wear() );
		pNewItemView->SetOrAddAttributeValueByName( CFmtStr( "sticker slot %d scale", infoSticker.slot() ), infoSticker.has_scale() ? infoSticker.scale() : 1.0f );
		pNewItemView->SetOrAddAttributeValueByName( CFmtStr( "sticker slot %d rotation", infoSticker.slot() ), infoSticker.rotation() );
	}

	if ( protoData.stickers().size() && protoData.stickers( 0 ).tint_id() )
	{
		uint32 uiTintID = protoData.stickers( 0 ).tint_id();
		pNewItemView->SetOrAddAttributeValueByName( "spray tint id", *reinterpret_cast< float * >( &uiTintID ) );
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
	CSAppLifetimeGameStats()->RecordUIEvent( "WeaponInspectExternalItem" );

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

#if DEVELOPMENT_ONLY
CON_COMMAND_F( csgo_launch_action_preview_dev, "Preview string for model panel", FCVAR_DONTRECORD | FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	LaunchWeaponPreviewPanelHelper( ( args.ArgC() >= 2 ) ? args.Arg( 1 ) : "" );
}
CON_COMMAND_F( csgo_launch_action_preview_gloves_dev, "Preview a glove with the given defindex, paintindex and wear", FCVAR_DONTRECORD | FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( args.ArgC() < 4 )
	{
		Warning( "Usage: csgo_launch_action_preview_gloves_dev <defindex> <paintindex> <paintwear [0, 1]>\n" );
		Warning( "All parameters are required. Wear is a float between 0 and 1\n" );
		return;
	}

	CEconItemPreviewDataBlock block;
	block.set_defindex( V_atoi( args.Arg( 1 ) ) );
	block.set_paintindex( V_atoi( args.Arg( 2 ) ) );
	block.set_paintwear( V_atof( args.Arg( 3 ) ) );
	block.set_quality( 3 );
	block.set_rarity( 7 );

	Helper_LaunchPreviewWithPreviewDataBlock( block );
}
#endif

CON_COMMAND_F( csgo_econ_action_preview, "Preview an economy item", FCVAR_DONTRECORD | FCVAR_HIDDEN | FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( args.ArgC() < 2 )
		return;

	// kill the workshop preview dialog if it's up
	if ( g_pWorkshopWorkbenchDialog )
	{
		delete g_pWorkshopWorkbenchDialog;
		g_pWorkshopWorkbenchDialog = NULL;
	}

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
	uint8 *uiDecoded = ( uint8 * ) stackalloc( numBytes );
	for ( uint32 j = 0; j < numBytes; ++ j )
	{
		uint32 uHi = ( pchEncodedAscii[2*j+0] >= 'A' ) ? ( 10 + pchEncodedAscii[2*j+0] - 'A') : ( pchEncodedAscii[2*j+0] - '0' );
		uint32 uLo = ( pchEncodedAscii[2*j+1] >= 'A' ) ? ( 10 + pchEncodedAscii[2*j+1] - 'A') : ( pchEncodedAscii[2*j+1] - '0' );
		if ( uHi >= 16 || uLo >= 16 ) { Assert( 0 ); return; }
		uint32 uVal = ( uHi * 16 ) + uLo;
		if ( uVal >= 256 ) { Assert( 0 ); return; }
		uiDecoded[j] = uint8( uVal );
	}
	for ( uint32 j = 1; j < numBytes; ++ j )
	{
		uiDecoded[j] = ( uiDecoded[j] ^ uiDecoded[0] );
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
	// $$$REI Leaky creation of reference econ item.  Need a system to manage econitemview lifetimes
	CEconItemView *pNewItemView = InventoryManager()->LeakyCreateReferenceEconItem( nDefinitionIndex, pPaintKit->nID );
	pNewItemView->SetItemQuality( 0 );
	pNewItemView->SetItemQualityOverride( 0 );
	pNewItemView->SetItemRarityOverride( 0 );
	float flWear = ( float )nWear / 100.0f;
	flWear = RemapValClamped( flWear, 0.0f, 1.0f, pPaintKit->flWearRemapMin, pPaintKit->flWearRemapMax );
	pNewItemView->SetOrAddAttributeValueByName( "set item texture wear",  flWear );

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
	CSAppLifetimeGameStats()->RecordUIEvent( "WorkshopWeaponPreview" );

	KeyValues *kvPreviewSettings = BuildWeaponPreviewSettingsForEconItemView( pNewItemView, kvManifest, previewMode );
	KeyValues::AutoDelete autodelete_kvPreviewSettings( kvPreviewSettings );

	if ( bCreate )
	{
		int x, y, wide, tall;
		parent->GetBounds( x, y, wide, tall );

		g_pModelPanelWeaponPreview = new CModelPanelWeaponPreview( parent, "WorkshopWeaponPreviewPanel", kvPreviewSettings, pNewItemView );
		g_pModelPanelWeaponPreview->SetWeaponTextureSize( COMPOSITE_TEXTURE_SIZE_2048 );
	
		g_pModelPanelWeaponPreview->SetParent( parent );

		vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/ClientScheme.res", "ClientScheme");
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
	InventoryManager()->UnsafeRemoveReferenceEconItem( nDefinitionIndex, pPaintKit->nID, 0 );
}

KeyValues *GetWorkshopWorkbenchKeyValuesFromFile( const char *pFilename )
{
	KeyValues* pPaintKitKV = new KeyValues( "workshop workbench" );
	if ( pFilename && pFilename[0] != 0 )
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
		pPaintKitKV =  NULL;
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
