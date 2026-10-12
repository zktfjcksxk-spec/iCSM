//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "csgo_avatarimage.h"
#include "clientsteamcontext.h"
#include "panorama/uijsregistration.h"
#include "gameui_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY(CCSGO_AvatarImage, CSGOAvatarImage);

DEFINE_PANORAMA_EVENT(CSGOAvatarImageLoaded);

DECLARE_PANORAMA_EVENT0( ReloadAllAvatarImages );
DEFINE_PANORAMA_EVENT( ReloadAllAvatarImages );

CCSGO_AvatarImageMgr g_AvatarImageMgr;

using namespace panorama;

void HideAvatarImagesChanged( IConVar *pVar, const char* pszOldValue, float flOldValue )
{
	DispatchEvent( ReloadAllAvatarImages(), nullptr );
}
ConVar cl_hide_avatar_images( "cl_hide_avatar_images", 0, FCVAR_ARCHIVE, "Hide avatar images for other players. \n\t0 - Off.\n\t1 - Block All\n\t2 - Block all but friends", HideAvatarImagesChanged  );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCSGO_AvatarImage::CCSGO_AvatarImage(panorama::CPanel2D *pParent, const char *pchID)
	: panorama::CPanel2D(pParent, pchID)
{
	m_pAvatarImage = new panorama::CImagePanel(this, NULL);
	m_pAvatarImage->SetScaling(k_EImageScalingStretchBothToFitPreserveAspectRatio);

	m_pAvatarImage->AddClass("AvatarImage");
	RegisterEventHandlerOnPanel( ImageLoaded(), m_pAvatarImage->UIPanel(), this, &CCSGO_AvatarImage::OnImageLoaded );

	m_pAvatarLoaded = nullptr;
	m_bAvatarPreloaded = false;
	m_bNotifyAvatarLoaded = false;

	RegisterForReadyEvents(true);

	if (!UIEngine()->BHaveEventHandlersRegisteredForType(CCSGO_AvatarImage::GetPanelSymbol()))
	{
		RegisterEventHandlerOnPanelType(ReadyForDisplay(), &CCSGO_AvatarImage::OnReadyForDisplay);
		RegisterEventHandlerOnPanelType(UnreadyForDisplay(), &CCSGO_AvatarImage::OnUnreadyForDisplay);
	}
	RegisterForUnhandledEvent( ReloadAllAvatarImages(), this, &CCSGO_AvatarImage::EventReloadImages );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSGO_AvatarImage::~CCSGO_AvatarImage()
{
	SAFE_RELEASE( m_pAvatarLoaded )
	
	// Remove from avatar image mgr
	if ( m_steamID.IsValid() )
	{
		g_AvatarImageMgr.RemoveAvatarImagePanel( m_steamID, this );
	}
}

//
// Initialize from global/external bits provider
//
CCSGO_AvatarImage::ImageDataInMemory_t::ImageDataInMemory_t( const CSteamID &steamID )
{
	V_memset( this, 0, sizeof( *this ) );

	if ( steamID.IsValid() && steamID.BIndividualAccount() )
	{
		AvatarImageBitsProvider pBitsProvider = g_AvatarImageMgr.GetImageBitsProvider( steamID );
		if ( pBitsProvider )
		{
			if ( !pBitsProvider( steamID.ConvertToUint64(), &pRGBA, &nWidth, &nHeight ) )
			{
				V_memset( this, 0, sizeof( *this ) );
			}
		}
	}
}

CCSGO_AvatarImage::ImageDataInMemory_t::~ImageDataInMemory_t()
{
	if ( pRGBA )
	{
		free( pRGBA );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Triggers loading and displaying a friend's avatar
//-----------------------------------------------------------------------------
bool CCSGO_AvatarImage::SetSteamID(const CSteamID &steamID)
{
	if (m_steamID == steamID)
	{
		if (m_bNotifyAvatarLoaded)
		{
			DispatchEventAsync(0.0f, CSGOAvatarImageLoaded(), this);
		}

		return false;
	}

	Assert( steamID == CSteamID() || steamID.IsValid() );

	if ( m_steamID.IsValid() )
	{
		g_AvatarImageMgr.RemoveAvatarImagePanel( m_steamID, this );
	}

	m_steamID = steamID;

	g_AvatarImageMgr.AddAvatarImagePanel( m_steamID, this );

	// Explicitly clear the avatar image so that you don't see stale avatars
	// for controls that are re-used between different users.
	m_pAvatarImage->Clear();

	ClearAvatar();

	//
	// Attempt to load via bits provider first
	// (in-memory data contained in GOTV demo file)
	//
	ImageDataInMemory_t imgDataInMemory( steamID );
	if ( imgDataInMemory.pRGBA )
	{
		LoadAvatarInternal( &imgDataInMemory );
	}
	else
	{
		PreloadAvatarInSteam();

		if ( GetParentWindow()->BIsVisible() )
		{
			LoadAvatarInternal( &imgDataInMemory );
		}
	}

	return true;
}

bool CCSGO_AvatarImage::SetAccountID(uint32 unAccountID)
{
	return SetSteamID(CSteamID(unAccountID, ClientSteamContext().GetConnectedUniverse(), k_EAccountTypeIndividual));
}

void CCSGO_AvatarImage::SetDefaultImage( const char *pchImageURL )
{
	m_defaultImageURL = pchImageURL;
	
	// Force a reload
	ClearAvatar();
}

bool CCSGO_AvatarImage::BSetProperty(CPanoramaSymbol symName, const char *pchValue)
{
	static const CPanoramaSymbol k_symSteamID("steamid");
	static const CPanoramaSymbol k_symAccountID("accountid");
	static const CPanoramaSymbol k_symNotifyAvatarLoaded("notifyavatarloaded");
	static const CPanoramaSymbol k_symDefaultSource( "defaultsrc" );
	static const CPanoramaSymbol k_symScaling( "scaling" );

	if (symName == k_symSteamID)
	{
		CSteamID steamID;
		if (!V_stricmp(pchValue, "local"))
			steamID = ClientSteamContext().GetLocalPlayerSteamID();
		else
			steamID = CSteamID(V_atoui64(pchValue));

		SetSteamID(steamID);
		return true;
	}
	else if (symName == k_symAccountID)
	{
		uint32 unAccountID = 0;
		if (!V_stricmp(pchValue, "local"))
			unAccountID = ClientSteamContext().GetLocalPlayerSteamID().GetAccountID();
		else
			unAccountID = (uint32)V_atoui64(pchValue);

		SetAccountID(unAccountID);
		return true;
	}
	else if ( symName == k_symNotifyAvatarLoaded )
	{
		return CSSHelpers::BParseTrueFalse(pchValue, &m_bNotifyAvatarLoaded );
	}
	else if ( symName == k_symDefaultSource )
	{
		if (pchValue && pchValue[0] != '\0' )
		{
			SetDefaultImage( pchValue );
		}
		return true;
	}
	else if ( symName == k_symScaling )
	{
		m_pAvatarImage->SetScaling( pchValue );
		return true;
	}
	else
	{
		return BaseClass::BSetProperty(symName, pchValue);
	}
}

void CCSGO_AvatarImage::SetupJavascriptObjectTemplate( )
{
	BaseClass::SetupJavascriptObjectTemplate( );

	panorama::RegisterJSAccessor( "steamid", PANORAMA_DELEGATE( &CCSGO_AvatarImage::JSGetSteamID ), PANORAMA_DELEGATE( &CCSGO_AvatarImage::JSSetSteamID ) );
	panorama::RegisterJSAccessor( "accountid", PANORAMA_DELEGATE( &CCSGO_AvatarImage::JSGetAccountID ), PANORAMA_DELEGATE( &CCSGO_AvatarImage::JSSetAccountID ) );

	panorama::RegisterJSMethod( "Clear", PANORAMA_DELEGATE( &CCSGO_AvatarImage::UnloadImages ) );
	panorama::RegisterJSMethod( "SetNotifiyAvatarLoaded", PANORAMA_DELEGATE( &CCSGO_AvatarImage::SetNotifyAvatarLoaded ) );
	panorama::RegisterJSMethod( "SetDefaultImage", PANORAMA_DELEGATE( &CCSGO_AvatarImage::SetDefaultImage ) );
}

CUtlString CCSGO_AvatarImage::JSGetSteamID() const
{
	CUtlString result;
	result.Format("%llu", m_steamID.ConvertToUint64());
	return result;
}

void CCSGO_AvatarImage::JSSetSteamID( CUtlString steamID )
{
	XUID xuid = ConvertToUint64( steamID.String() );
	SetSteamID( CSteamID( xuid ) );
}

CUtlString CCSGO_AvatarImage::JSGetAccountID() const
{
	CUtlString result;
	result.Format( "%u", m_steamID.GetAccountID() );
	return result;
}

void CCSGO_AvatarImage::JSSetAccountID( CUtlString accountID )
{
	SetAccountID((uint32)V_atoui64(accountID.String()));
}

void CCSGO_AvatarImage::Paint()
{
	if ( !m_pAvatarLoaded && !BIsTransparent() && BIsVisible() )
		LoadAvatarInternal();
	else if (!m_bAvatarPreloaded)
		PreloadAvatarInSteam();

	BaseClass::Paint();
}

//-----------------------------------------------------------------------------
// Purpose: Clears image data
//-----------------------------------------------------------------------------
void CCSGO_AvatarImage::ClearAvatar()
{
	SAFE_RELEASE( m_pAvatarLoaded );
	m_bAvatarPreloaded = false;
}


//-----------------------------------------------------------------------------
// Purpose: Preload just inside of steam, so we'll do the HTTP requests and such and cache locally
//-----------------------------------------------------------------------------
void CCSGO_AvatarImage::OnStylesChanged()
{
	if (!m_bAvatarPreloaded && GetParentWindow()->BIsVisible())
		PreloadAvatarInSteam();

	BaseClass::OnStylesChanged();
}


//-----------------------------------------------------------------------------
// Purpose: Preload just inside of steam, so we'll do the HTTP requests and such and cache locally
//-----------------------------------------------------------------------------
void CCSGO_AvatarImage::OnLayoutTraverse(float flFinalWidth, float flFinalHeight)
{
	BaseClass::OnLayoutTraverse(flFinalWidth, flFinalHeight);

	// If we have an avatar loaded, but it's the wrong size, then load the right size
	if ( m_pAvatarLoaded )
	{
		panorama::IImageSource *pAvatarImage = GetSteamImage();
		if ( pAvatarImage != m_pAvatarLoaded )
		{
			ClearAvatar();
		}
	}
}


bool CCSGO_AvatarImage::EventReloadImages()
{
	UnloadImages();
	ReloadImages();
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Preload just inside of steam, so we'll do the HTTP reuqests and such and cache locally
//-----------------------------------------------------------------------------
void CCSGO_AvatarImage::PreloadAvatarInSteam()
{
	// Just doing the request here causes us to pre-load into system memory in Steam, which we want so we can draw 
	// fast with our delayed gpu texture load.
	GetSteamImage();
	m_bAvatarPreloaded = true;
}


//-----------------------------------------------------------------------------
// Purpose: Get the Steam image id to use for this avatar image instance
//-----------------------------------------------------------------------------
panorama::IImageSource *CCSGO_AvatarImage::GetSteamImage()
{
	if (!m_steamID.IsValid())
		return nullptr;

	// Try to figure out the actual size we're going to be shown at and pick the appropriate steam 
	float flLayoutWidth = 0.0f;
	float flLayoutHeight = 0.0f;
	if (BHasBeenLayedOut())
	{
		// If we've been layed out, we have the actual size
		flLayoutWidth = GetActualLayoutWidth();
		flLayoutHeight = GetActualLayoutHeight();
	}
	else
	{
		// Otherwise, start with the small size. It might be a little blurry until we upgrade to the large size later
		flLayoutWidth = 32.0f;
		flLayoutHeight = 32.0f;
	}

	
	panorama::IImageSource *pAvatarImage = nullptr;

	// If large version of avatar not yet loaded, but a smaller size exists, start with the smaller size.
	// The larger size will then be displayed following the Steam callback OnAvatarImageLoaded.
	float flMaxDimension = Max( flLayoutWidth, flLayoutHeight );
	if( flMaxDimension <= 0.0f || flMaxDimension > 64.0f )
	{
		pAvatarImage = g_AvatarImageMgr.GetLargeSteamAvatar( m_steamID );
	}
	if( ( !pAvatarImage ) && ( flMaxDimension > 32.0f ) )
	{
		pAvatarImage = g_AvatarImageMgr.GetMediumSteamAvatar( m_steamID );
	}
	if( !pAvatarImage )
	{
		pAvatarImage = g_AvatarImageMgr.GetSmallSteamAvatar( m_steamID );
	}

	return pAvatarImage;
}


//-----------------------------------------------------------------------------
// Purpose: Panorama is telling us we should reload our images now
//-----------------------------------------------------------------------------
void CCSGO_AvatarImage::ReloadImages()
{
	if ( !m_pAvatarLoaded )
	{
		LoadAvatarInternal();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Panorama is telling us it wants us to free images for a while
//-----------------------------------------------------------------------------
void CCSGO_AvatarImage::UnloadImages()
{
	m_pAvatarImage->Clear();
	SAFE_RELEASE( m_pAvatarLoaded );
}

bool Helper_ShouldBlockAvatar( const CSteamID& steamID )
{
	// Never hide local player avatar
	if ( steamID == ClientSteamContext().GetLocalPlayerSteamID() )
		return false;

	if ( cl_hide_avatar_images.GetInt() == 1 )
	{
		return true; // block all
	}
	else if ( cl_hide_avatar_images.GetInt() == 2 )
	{
		if ( !steamapicontext || !steamapicontext->SteamFriends() || !steamID.IsValid())
			return false;

		// Block anyone not on friend list
		return !steamapicontext->SteamFriends()->HasFriend( steamID, k_EFriendFlagImmediate );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Triggers loading and displaying a friend's avatar
//-----------------------------------------------------------------------------
void CCSGO_AvatarImage::LoadAvatarInternal( ImageDataInMemory_t *pImageDataInMemory )
{
	if ( Helper_ShouldBlockAvatar( m_steamID ) )
	{
		// Just use a default if we're hiding avatar images
		if ( m_defaultImageURL.IsEmpty() )
			m_pAvatarImage->SetImageJS( "file://{images}/icons/ui/pro_player.svg" );
		else
			m_pAvatarImage->SetImageJS( m_defaultImageURL );

		return;
	}

	if ( !pImageDataInMemory )
	{	// recurse with pre-loaded bits
		ImageDataInMemory_t imgDataInMemory( m_steamID );
		LoadAvatarInternal( &imgDataInMemory );
		return;
	}

	//VPROF_BUDGET( "CCSGO_AvatarImage::LoadAvatarInternal", VPROF_BUDGETGROUP_TENFOOT );
	ClearAvatar();

	bool bUseDefaultAvatarImage = true;
	panorama::IImageSource *pAvatarImage = GetSteamImage();

	if ( pImageDataInMemory->pRGBA )
	{
		unsigned char *pRGBA = pImageDataInMemory->pRGBA;
		uint32 nWidth = pImageDataInMemory->nWidth, nHeight = pImageDataInMemory->nHeight;

		uint32 unBytes = nWidth * nHeight * 4;
		CUtlBuffer bufRGBA( pRGBA, nWidth * nHeight * 4 );

		// Debug write to file
		//FileHandle_t fp = g_pFullFileSystem->Open( "c:\\temp\\pixels.raw", "wb" );
		//g_pFullFileSystem->Write( pRGBA, 64 * 64 * 4, fp );
		//g_pFullFileSystem->Close( fp );

		bufRGBA.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
		bufRGBA.SeekPut(CUtlBuffer::SEEK_HEAD, unBytes);	// The image loader uses the put position to work out size

		m_pAvatarImage->SetImage(bufRGBA, 64, 64, NULL);

		bUseDefaultAvatarImage = false;
	}
	else if ( pAvatarImage )
	{
		m_pAvatarImage->SetImage( pAvatarImage );
		bUseDefaultAvatarImage = false;
	}

	if ( bUseDefaultAvatarImage && !m_defaultImageURL.IsEmpty() )
	{
		m_pAvatarImage->SetImageJS( m_defaultImageURL );
	}

	m_bAvatarPreloaded = true;

	if ( pAvatarImage )
		pAvatarImage->AddRef();
	SAFE_RELEASE( m_pAvatarLoaded );
	m_pAvatarLoaded = pAvatarImage;
}

				
//-----------------------------------------------------------------------------
// Purpose: called when our avatar image is ready to display
//-----------------------------------------------------------------------------
bool CCSGO_AvatarImage::OnImageLoaded(const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage)
{
	if (pPanel.Get() == m_pAvatarImage->UIPanel())
	{
		InvalidateSizeAndPosition();
		if (m_bNotifyAvatarLoaded)
		{
			DispatchEventAsync(0.0f, CSGOAvatarImageLoaded(), this);
		}
	}

	// Always return false so CImagePanel handler gets called next
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: called when we should prepare the avatar for display
//-----------------------------------------------------------------------------
bool CCSGO_AvatarImage::OnReadyForDisplay(const panorama::CPanelPtr< panorama::IUIPanel > &pPanel)
{
	ReloadImages();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: called when the avatar is no longer needed to display
//-----------------------------------------------------------------------------
bool CCSGO_AvatarImage::OnUnreadyForDisplay(const panorama::CPanelPtr< panorama::IUIPanel > &pPanel)
{
	UnloadImages();
	return true;
}

//-----------------------------------------------------------------------------
//
// CCSGO_AvatarImageMgrSteamIDData
//
//-----------------------------------------------------------------------------

bool SteamIDLessFunc( const CSteamID& left, const CSteamID& right )
{
	return left < right;
}

//-----------------------------------------------------------------------------
// CCSGO_AvatarImageMgr
//-----------------------------------------------------------------------------
CCSGO_AvatarImageMgr::CCSGO_AvatarImageMgr():
	m_mapIdsToImages( 0, 0, SteamIDLessFunc ),
	m_bSteamCallbacksConfigured( false )
{}

//-----------------------------------------------------------------------------
// CCSGO_AvatarImageMgr::AddImage
//-----------------------------------------------------------------------------
void CCSGO_AvatarImageMgr::AddAvatarImagePanel( CSteamID steamID, CCSGO_AvatarImage* pImage )
{
	EnsureSteamCallbacksConfigured();
	
	int nMapEntryIdx = FindOrAddSteamID( steamID );

	// Add the image pointer, if it doesn't already exist
	CSGO_AvatarImageMgrSteamIDData_t *pSteamIDData = m_mapIdsToImages[ nMapEntryIdx ];
	CUtlVector< CCSGO_AvatarImage* > *pImages = &pSteamIDData->m_images;
	
	int nImageIdx = pImages->Find( pImage );
	if ( nImageIdx != pImages->InvalidIndex() )
	{
		Warning( "CCSGO_AvatarImageMgr::AddImage - Error: image already added\n");
		return;
	}

	pImages->AddToTail( pImage );
}

//-----------------------------------------------------------------------------
// CCSGO_AvatarImageMgr::RemoveImage
//-----------------------------------------------------------------------------
void CCSGO_AvatarImageMgr::RemoveAvatarImagePanel( CSteamID steamID, CCSGO_AvatarImage* pImage )
{
	int nMapEntryIdx = m_mapIdsToImages.Find( steamID );
	if ( !m_mapIdsToImages.IsValidIndex( nMapEntryIdx ) )
	{
		Warning( "CCSGO_AvatarImageMgr::RemoveImage - Error: Steam ID not in map\n");
		return;
	}

	// Add the image pointer, if it doesn't already exist
	CSGO_AvatarImageMgrSteamIDData_t *pSteamIDData = m_mapIdsToImages[ nMapEntryIdx ];
	CUtlVector< CCSGO_AvatarImage* > *pImages = &pSteamIDData->m_images;

	int nImageIdx = pImages->Find( pImage );
	if ( nImageIdx == pImages->InvalidIndex() )
	{
		Warning( "CCSGO_AvatarImageMgr::RemoveImage - Error: Image not found\n");
		return;
	}

	pImages->Remove( nImageIdx );
}

//-----------------------------------------------------------------------------
// CCSGO_AvatarImageMgr::SetImageBitsProvider
//-----------------------------------------------------------------------------
void CCSGO_AvatarImageMgr::SetImageBitsProvider( uint32 accountID, AvatarImageBitsProvider pfnBitsProvider )
{
	CSteamID steamID( accountID, ClientSteamContext().GetConnectedUniverse(), k_EAccountTypeIndividual );
	int nMapEntryIdx = FindOrAddSteamID( steamID );

	// Save bits provider
	CSGO_AvatarImageMgrSteamIDData_t *pSteamIDData = m_mapIdsToImages[ nMapEntryIdx ];
	pSteamIDData->m_pfnImageBitsProvider = pfnBitsProvider;

	// Reload all associated images
	ReloadAvatarImagePanels( *pSteamIDData );
}

//-----------------------------------------------------------------------------
// CCSGO_AvatarImageMgr::GetImageBitsProvider
//-----------------------------------------------------------------------------
AvatarImageBitsProvider CCSGO_AvatarImageMgr::GetImageBitsProvider( CSteamID steamID )
{
	AvatarImageBitsProvider pRetVal = nullptr;

	int nMapEntryIdx = m_mapIdsToImages.Find( steamID );
	if ( m_mapIdsToImages.IsValidIndex( nMapEntryIdx ) )
	{
		pRetVal = m_mapIdsToImages[ nMapEntryIdx ]->m_pfnImageBitsProvider;
	}

	return pRetVal;
}

//-----------------------------------------------------------------------------
// CCSGO_AvatarImageMgr::Cleanup
//-----------------------------------------------------------------------------
void CCSGO_AvatarImageMgr::Cleanup()
{
	for ( int nMapEntryIdx = m_mapIdsToImages.FirstInorder(); nMapEntryIdx != m_mapIdsToImages.InvalidIndex(); /* advance inside */ )
	{
		CSGO_AvatarImageMgrSteamIDData_t *pSteamIDData = m_mapIdsToImages[nMapEntryIdx];

		if ( pSteamIDData->m_images.Count() )
		{
			// There is at least one avatar image panel referencing the steamIDData
			// so only delete the custom bits provider and reload all associated
			if ( pSteamIDData->m_pfnImageBitsProvider )
			{
				pSteamIDData->m_pfnImageBitsProvider = nullptr;
				ReloadAvatarImagePanels( *pSteamIDData );
			}
			nMapEntryIdx = m_mapIdsToImages.NextInorder( nMapEntryIdx );
		}
		else
		{
			// No more avatar image panel referencing the given steamIDData, it is 
			// safe to delete the entry.
			delete pSteamIDData;

			int nEntryIdxToRemove = nMapEntryIdx;
			nMapEntryIdx = m_mapIdsToImages.NextInorder( nMapEntryIdx );
			m_mapIdsToImages.RemoveAt( nEntryIdxToRemove );
		}
	}
}

//-----------------------------------------------------------------------------
// CCSGO_AvatarImageMgr::FindOrAddSteamID
//-----------------------------------------------------------------------------
int CCSGO_AvatarImageMgr::FindOrAddSteamID( CSteamID steamID )
{
	// See if we have an entry for this steam id, add one if there isn't already one
	int nMapEntryIdx = m_mapIdsToImages.Find( steamID );
	if ( !m_mapIdsToImages.IsValidIndex( nMapEntryIdx ) )
	{
		CSGO_AvatarImageMgrSteamIDData_t *pSteamIDData = new CSGO_AvatarImageMgrSteamIDData_t;
		nMapEntryIdx = m_mapIdsToImages.Insert( steamID, pSteamIDData );
	}

	return nMapEntryIdx;
}

//-----------------------------------------------------------------------------
// CCSGO_AvatarImageMgr::EnsureSteamCallbacksConfigured
//-----------------------------------------------------------------------------
void CCSGO_AvatarImageMgr::EnsureSteamCallbacksConfigured()
{
	if ( m_bSteamCallbacksConfigured )
		return;
	m_bSteamCallbacksConfigured = true;

	m_CallbackPersonaStateChanged.Register( this, &CCSGO_AvatarImageMgr::OnPersonaStateChanged );
	m_CallbackAvatarImageLoaded.Register( this, &CCSGO_AvatarImageMgr::OnLargeAvatarImageLoaded );
}

//-----------------------------------------------------------------------------
// CCSGO_AvatarImageMgr::OnLargeAvatarImageLoaded
//		Called when an avatar is loaded from a previous GetLargeFriendAvatar call
//-----------------------------------------------------------------------------
void CCSGO_AvatarImageMgr::OnLargeAvatarImageLoaded( AvatarImageLoaded_t *pParam )
{
	if ( pParam )
	{
		int nMapEntryIdx = m_mapIdsToImages.Find( pParam->m_steamID );
		if ( m_mapIdsToImages.IsValidIndex( nMapEntryIdx ) )
		{
			CSGO_AvatarImageMgrSteamIDData_t *pSteamIDData = m_mapIdsToImages[nMapEntryIdx];

			// Reseting large steam avatar
			SAFE_RELEASE( pSteamIDData->m_pLargeSteamAvatar );

			// Reload all associated images
			ReloadAvatarImagePanels( *pSteamIDData );
		}
	}
}

//-----------------------------------------------------------------------------
// CCSGO_AvatarImageMgr::OnPersonaStateChanged
//-----------------------------------------------------------------------------
void CCSGO_AvatarImageMgr::OnPersonaStateChanged( PersonaStateChange_t *pParam )
{
	if ( pParam && ( pParam->m_nChangeFlags & k_EPersonaChangeAvatar ) )
	{
		int nMapEntryIdx = m_mapIdsToImages.Find( CSteamID( pParam->m_ulSteamID ) );
		if ( m_mapIdsToImages.IsValidIndex( nMapEntryIdx ) )
		{
			CSGO_AvatarImageMgrSteamIDData_t *pSteamIDData = m_mapIdsToImages[nMapEntryIdx];

			// Resetting all steam avatars
			SAFE_RELEASE( pSteamIDData->m_pLargeSteamAvatar );
			SAFE_RELEASE( pSteamIDData->m_pMediumSteamAvatar );
			SAFE_RELEASE( pSteamIDData->m_pSmallSteamAvatar );

			// Reload all associated images
			ReloadAvatarImagePanels( *pSteamIDData );
		}
	}
}

//-----------------------------------------------------------------------------
// CCSGO_AvatarImageMgr::GetLargeSteamAvatar
//-----------------------------------------------------------------------------
panorama::IImageSource *CCSGO_AvatarImageMgr::GetLargeSteamAvatar( CSteamID steamID )
{
	int nMapEntryIdx = FindOrAddSteamID( steamID );
	CSGO_AvatarImageMgrSteamIDData_t *pSteamIDData = m_mapIdsToImages[ nMapEntryIdx ];

	if ( !pSteamIDData->m_pLargeSteamAvatar )
	{
		ISteamFriends *pSteamFriends = steamapicontext->SteamFriends();
		int iAvatar = pSteamFriends->GetLargeFriendAvatar( steamID );
		if ( !iAvatar || ( iAvatar == -1 ) )
		{
			bool bRequestEnqueuedAsync = pSteamFriends->RequestUserInformation( steamID, false );
			if ( !bRequestEnqueuedAsync )	// try again one more time, Steam says everything is available
			{
				iAvatar = steamapicontext->SteamFriends()->GetLargeFriendAvatar( steamID );
			}
		}

		pSteamIDData->m_pLargeSteamAvatar = CreateImageFromSteamIndex( iAvatar );
	}
	
	return pSteamIDData->m_pLargeSteamAvatar;
}

//-----------------------------------------------------------------------------
// CCSGO_AvatarImageMgr::GetMediumSteamAvatar
//-----------------------------------------------------------------------------
panorama::IImageSource *CCSGO_AvatarImageMgr::GetMediumSteamAvatar( CSteamID steamID )
{
	int nMapEntryIdx = FindOrAddSteamID( steamID );
	CSGO_AvatarImageMgrSteamIDData_t *pSteamIDData = m_mapIdsToImages[nMapEntryIdx];

	if ( !pSteamIDData->m_pMediumSteamAvatar )
	{
		ISteamFriends *pSteamFriends = steamapicontext->SteamFriends();
		int iAvatar = pSteamFriends->GetMediumFriendAvatar( steamID );
		if ( !iAvatar || ( iAvatar == -1 ) )
		{
			bool bRequestEnqueuedAsync = pSteamFriends->RequestUserInformation( steamID, false );
			if ( !bRequestEnqueuedAsync )	// try again one more time, Steam says everything is available
			{
				iAvatar = steamapicontext->SteamFriends()->GetMediumFriendAvatar( steamID );
			}
		}

		pSteamIDData->m_pMediumSteamAvatar = CreateImageFromSteamIndex( iAvatar );
	}

	return pSteamIDData->m_pMediumSteamAvatar;
}

//-----------------------------------------------------------------------------
// CCSGO_AvatarImageMgr::GetSmallSteamAvatar
//-----------------------------------------------------------------------------
panorama::IImageSource *CCSGO_AvatarImageMgr::GetSmallSteamAvatar( CSteamID steamID )
{
	int nMapEntryIdx = FindOrAddSteamID( steamID );
	CSGO_AvatarImageMgrSteamIDData_t *pSteamIDData = m_mapIdsToImages[nMapEntryIdx];

	if ( !pSteamIDData->m_pSmallSteamAvatar )
	{
		ISteamFriends *pSteamFriends = steamapicontext->SteamFriends();
		int iAvatar = pSteamFriends->GetSmallFriendAvatar( steamID );
		if ( !iAvatar || ( iAvatar == -1 ) )
		{
			bool bRequestEnqueuedAsync = pSteamFriends->RequestUserInformation( steamID, false );
			if ( !bRequestEnqueuedAsync )	// try again one more time, Steam says everything is available
			{
				iAvatar = steamapicontext->SteamFriends()->GetSmallFriendAvatar( steamID );
			}
		}

		pSteamIDData->m_pSmallSteamAvatar = CreateImageFromSteamIndex( iAvatar );
	}

	return pSteamIDData->m_pSmallSteamAvatar;
}

//-----------------------------------------------------------------------------
// CCSGO_AvatarImageMgr::CreateSteamImage
//-----------------------------------------------------------------------------
IImageSource *CCSGO_AvatarImageMgr::CreateImageFromSteamIndex( int iAvatar )
{
	if ( ( iAvatar != -1 ) && ( iAvatar != 0 ) )
	{
		uint32 unAvatarWidth, unAvatarHeight;
		steamapicontext->SteamUtils()->GetImageSize( iAvatar, &unAvatarWidth, &unAvatarHeight );
		if ( unAvatarWidth > 0 && unAvatarHeight > 0 )
		{
			// Get the actual raw RGBA data from Steam and turn it into a texture in our game engine
			CUtlBuffer bufRGBA;
			uint32 unBytes = unAvatarWidth * unAvatarHeight * 4;
			bufRGBA.EnsureCapacity( unBytes );
			if ( steamapicontext->SteamUtils()->GetImageRGBA( iAvatar, (uint8*)bufRGBA.Base(), unBytes ) )
			{
				bufRGBA.SeekPut( CUtlBuffer::SEEK_HEAD, unBytes );
				return panorama::UIEngine()->UIImageManager()->LoadImageFromMemory( nullptr, nullptr, bufRGBA, unAvatarWidth, unAvatarHeight, k_EImageFormatR8G8B8A8, UIImageLoadParams_t() );
			}
		}
	}
	
	return nullptr;
}

//-----------------------------------------------------------------------------
// CCSGO_AvatarImageMgr::ReloadAvatarImagePanels
//		Reloading all associated avatar panels associated with the given steamIDData
//-----------------------------------------------------------------------------
void CCSGO_AvatarImageMgr::ReloadAvatarImagePanels( const CSGO_AvatarImageMgrSteamIDData_t &steamIDData ) const
{
	int nImages = steamIDData.m_images.Count();
	for ( int i = 0; i < nImages; i++ )
	{
		steamIDData.m_images[i]->UnloadImages();
		steamIDData.m_images[i]->ReloadImages();
	}
}