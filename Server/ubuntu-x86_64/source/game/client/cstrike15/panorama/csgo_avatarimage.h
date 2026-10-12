//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/uischeduleddel.h"
#include "panorama/iavatarimagemgr.h"

DECLARE_PANEL_EVENT0(CSGOAvatarImageLoaded);

//-----------------------------------------------------------------------------
// Purpose: Avatar image for a single CSGO friend, based on DOTA equivalent
//-----------------------------------------------------------------------------
class CCSGO_AvatarImage : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_AvatarImage, panorama::CPanel2D );

public:
	CCSGO_AvatarImage(panorama::CPanel2D *pParent, const char *pchID);
	virtual ~CCSGO_AvatarImage();

	virtual bool SetSteamID(const CSteamID &steamID);
	const CSteamID &GetSteamID() const { return m_steamID; }

	// Convenience methods for using just an account id
	bool SetAccountID(uint32 unAccountID);
	uint32 GetAccountID() const { return m_steamID.GetAccountID(); }

	// Set a default image from a URL (file:///, http:///).
	// The default image will be used while the avatar is loading or if we don't
	// have an avatar for the given steam ID
	void SetDefaultImage( const char *pchImageURL );

	// Property handling
	virtual bool BSetProperty(panorama::CPanoramaSymbol symName, const char *pchValue) OVERRIDE;

	// JS Bindings
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;
	CUtlString JSGetSteamID() const;
	void JSSetSteamID( CUtlString steamID );
	CUtlString JSGetAccountID() const;
	void JSSetAccountID( CUtlString accountID );

	void SetNotifyAvatarLoaded( bool bNotify = true ) { m_bNotifyAvatarLoaded = bNotify; }

	// Override for painting
	virtual void Paint() OVERRIDE;

	// Handle styles changing
	virtual void OnStylesChanged() OVERRIDE;

	// Handle layout
	virtual void OnLayoutTraverse(float flFinalWidth, float flFinalHeight) OVERRIDE;

	bool EventReloadImages();
	void ReloadImages();
	void UnloadImages();

private:
	void ClearAvatar();
	void PreloadAvatarInSteam();
	panorama::IImageSource *GetSteamImage();

	bool OnImageLoaded(const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage);
	bool OnReadyForDisplay(const panorama::CPanelPtr< panorama::IUIPanel > &pPanel);
	bool OnUnreadyForDisplay(const panorama::CPanelPtr< panorama::IUIPanel > &pPanel);

	struct ImageDataInMemory_t
	{
		explicit ImageDataInMemory_t( const CSteamID &steamID );
		~ImageDataInMemory_t();
		unsigned char *pRGBA;
		uint32 nWidth, nHeight;
	};
	void LoadAvatarInternal( ImageDataInMemory_t *pImageDataInMemory = NULL );

	CSteamID m_steamID;
	panorama::IImageSource *m_pAvatarLoaded;
	bool m_bAvatarPreloaded;
	bool m_bNotifyAvatarLoaded;
	panorama::CImagePanel *m_pAvatarImage;
	CUtlString m_defaultImageURL;
};


//-----------------------------------------------------------------------------
// Purpose: Avatar image manager. Functionality to access all CCSGO_AvatarImage
//	instances of for a given steam id
//-----------------------------------------------------------------------------

class CSGO_AvatarImageMgrSteamIDData_t
{
public:

	CSGO_AvatarImageMgrSteamIDData_t() : 
		m_pfnImageBitsProvider( nullptr ), 
		m_pLargeSteamAvatar( nullptr ), m_pMediumSteamAvatar( nullptr ), m_pSmallSteamAvatar( nullptr )
	{}
	~CSGO_AvatarImageMgrSteamIDData_t()
	{
		SAFE_RELEASE( m_pLargeSteamAvatar );
		SAFE_RELEASE( m_pMediumSteamAvatar );
		SAFE_RELEASE( m_pSmallSteamAvatar );
	}

	AvatarImageBitsProvider m_pfnImageBitsProvider;

	panorama::IImageSource *m_pLargeSteamAvatar;
	panorama::IImageSource *m_pMediumSteamAvatar;
	panorama::IImageSource *m_pSmallSteamAvatar;

	CUtlVector< CCSGO_AvatarImage* > m_images;
};

class CCSGO_AvatarImageMgr : public IAvatarImageMgr
{
public:

	CCSGO_AvatarImageMgr();
	~CCSGO_AvatarImageMgr() { Cleanup(); }

	virtual void SetImageBitsProvider( uint32 accountID, AvatarImageBitsProvider pfnBitsProvider ) OVERRIDE;
	virtual void Cleanup() OVERRIDE;
	
	// Custom bits provider
	AvatarImageBitsProvider GetImageBitsProvider( CSteamID steamID );
	// Steam avatar
	panorama::IImageSource *GetLargeSteamAvatar( CSteamID steamID );
	panorama::IImageSource *GetMediumSteamAvatar( CSteamID steamID );
	panorama::IImageSource *GetSmallSteamAvatar( CSteamID steamID );

	void AddAvatarImagePanel( CSteamID steamID, CCSGO_AvatarImage* pImage );
	void RemoveAvatarImagePanel( CSteamID steamID, CCSGO_AvatarImage* pImage );

private:

	// FindOrAddSteamID doesn't have a corresponsing RemoveSteamID. By design. Cleanup()
	// is called by CBaseClientState::Clear when it's safe to remove all registered
	// bits providers
	int FindOrAddSteamID( CSteamID steamID );
	panorama::IImageSource *CreateImageFromSteamIndex( int iAvatar );
	void ReloadAvatarImagePanels( const CSGO_AvatarImageMgrSteamIDData_t &steamIDData ) const;

	CUtlMap< CSteamID, CSGO_AvatarImageMgrSteamIDData_t*, unsigned short > m_mapIdsToImages;
	
	bool m_bSteamCallbacksConfigured;
	void EnsureSteamCallbacksConfigured();
	STEAM_CALLBACK_MANUAL( CCSGO_AvatarImageMgr, OnPersonaStateChanged, PersonaStateChange_t, m_CallbackPersonaStateChanged );
	STEAM_CALLBACK_MANUAL( CCSGO_AvatarImageMgr, OnLargeAvatarImageLoaded, AvatarImageLoaded_t, m_CallbackAvatarImageLoaded );
};

extern CCSGO_AvatarImageMgr g_AvatarImageMgr;