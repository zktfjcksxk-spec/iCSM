#ifndef MODELPANEL_WEAPONPREVIEW_H
#define MODELPANEL_WEAPONPREVIEW_H

#ifdef _WIN32
#pragma once
#endif

#include "basemodel_panel.h"
#include "game/client/irendercaptureconfiguration.h"

#define ITEMID_WEAPONPREVIEW_EXTERNAL1 0x7FFF7FFF7FFF7FFFull
#define ITEMID_WEAPONPREVIEW_EXTERNAL2 0x7FFF7FFF7FFF7FFEull

#define PREVIEW_PAINTKIT_ID_BASE	0x42420


//-----------------------------------------------------------------------------
// Class implementing weapon preview model
//-----------------------------------------------------------------------------
class CModelPanelWeaponPreview : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CModelPanelWeaponPreview, vgui::EditablePanel );
public:
	static itemid_t sm_ullExternalEconItemIdLast;
	static CEconItemView *sm_pExternalEconItemView1;
	static CEconItemView *sm_pExternalEconItemView2;

public:
	CModelPanelWeaponPreview( vgui::Panel *parent, const char *name, KeyValues *kvConfiguration, const CEconItemView *pItem );
	virtual ~CModelPanelWeaponPreview();

public:
	CBaseModelPanel *GetModelPanel( void ) { return m_pModelPanel; }
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme ) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;

	CEconItemView const *GetWeaponPreviewItemData() const { return &m_ItemData; }

	void UpdateItem( KeyValues *kvConfiguration, CEconItemView *pItem, bool bForceAnimReset, bool bUpdatingStickers = false );
	void PlayAnimOnWeapon( const char* pszAnimName );

	float GetAnimationEndTime( bool bWeapon );
	float GetAnimationTime( bool bWeapon );
	void SetAnimationTime( float flTime, bool bWeapon );

	const char *GetWeaponCustomVTFName();
	const char *GetPatternVTFName();

	void SetWeaponTextureSize( CompositeTextureSize_t size ) { m_weaponTextureSize = size; }

	void ApplySticker( int nSlot, int nStickerId );

protected:
	virtual void PaintBackground();

	KeyValues *m_kvConfiguration;
	CompositeTextureSize_t m_weaponTextureSize;
	CTextureReference m_shadowDummyColorBufferTexture;
	CTextureReference m_shadowDepthTexture;
	CTextureReference m_shadowFlashlightCookie;
	CRenderCaptureConfigurationState m_cfgRenderCapture;

protected:
	CEconItemView m_ItemData;
	MDLHandle_t m_hMdlWeapon;
	vgui::DHANDLE<CBaseModelPanel>	m_pModelPanel;
};


//-----------------------------------------------------------------------------
// Helper functions for the Workshop Workbench Dialog
//-----------------------------------------------------------------------------
enum PreviewMode
{
	PreviewMode_Hold,
	PreviewMode_Workbench,
	PreviewMode_GreenScreen,
	PreviewMode_NumModes,

	// These enums are not for the workbench dialog. The other modes need to move into a different enum that is specific to the workbench dialog preview modes instead of the general inspect modes. [FIXME]
	PreviewMode_Stickers,
	PreviewMode_Default
};

extern KeyValues *GetWorkshopWorkbenchKeyValuesFromFile( const char *pFilename );
extern void CreateChildWeaponPreviewPanel( KeyValues *pPaintParams, const char* pWeapon, int nWear, bool bShowStatTrak, bool bShowNameTag, PreviewMode previewMode, int nSeed, vgui::Panel *parent );
extern void UpdateChildWeaponPreviewPanel( KeyValues *pPaintParams, const char* pWeapon, int nWear, bool bShowStatTrak, bool bShowNameTag, PreviewMode previewMode, int nSeed, vgui::Panel *parent, bool bCreate, bool bForceAnimReset );
extern void CleaupChildWorkshopPreviewPanel();
extern void ChildWeaponPreviewPanelPlayAnimation( const char *pAnimName );

#endif // MODELPANEL_WEAPONPREVIEW_H
