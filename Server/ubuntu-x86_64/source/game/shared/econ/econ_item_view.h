//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef ECON_ITEM_CONSTANTS_H
#define ECON_ITEM_CONSTANTS_H
#ifdef _WIN32
#pragma once
#endif

#include "game_item_schema.h"
#include "econ_item_constants.h"
#include "localization_provider.h"
#include "econ_item_interface.h"
#include "econ_item.h"
#include "tier1/utlhashtable.h"

#if defined(CLIENT_DLL)
#include "iclientrenderable.h"

#if defined(CSTRIKE15)
	#include "materialsystem/custommaterialowner.h"
#endif //#if defined(CSTRIKE15)

#include "materialsystem/icompositetexturegenerator.h"

#endif //#if defined(CLIENT_DLL)

#include "materialsystem/MaterialSystemUtil.h"

#include "game/shared/csgo/econ_shared_defs.h"

#if defined(TF_DLL)
#include "tf_item_schema.h"
#endif //#if defined(TF_DLL)

#if defined(CLIENT_DLL) 
#define CEconItemView C_EconItemView

#if defined(CSTRIKE15)
struct CSWeaponID; // $$$REI temp forward declare
#if !defined ( PANORAMA_ENABLE )
	typedef void (*ImageReadyCallback_t)( const CEconItemView *pItemView, CUtlBuffer &rawImageRgba, int nWidth, int nHeight, uint64 nItemID );
#else
	#include <functional>
	typedef std::function< void( const CEconItemView *pItemView, CUtlBuffer &rawImageRgba, int nWidth, int nHeight, uint64 nItemID ) > ImageReadyCallback_t;
#endif
	struct RenderToRTData_t;
	class CMergedMDL;

#endif //#if defined(CSTRIKE15)

#endif //#if defined(CLIENT_DLL)

#include "econ_item_view_helpers.h"

#define RGB_INT_RED  12073019
#define RGB_INT_BLUE 5801378
#define ECON_ITEM_GENERATED_PINBOARD_ICON_WIDTH 512
#define ECON_ITEM_GENERATED_PINBOARD_ICON_HEIGHT 384
#define ECON_ITEM_GENERATED_ICON_DIR "resource/Flash/econ/weapons/cached/"

// def indices 1-499 are reserved for default aka stock items
#if defined(CSTRIKE15)
// However CS:GO currently expects base items to fit in a byte due to db storage of item usage
// (Note we use 256 here, not 255 because we create arrays of this size. Not sure it was a good idea to specify MAX and not MAX+1)
#define SCHEMA_BASE_ITEM_MAX 256
#else
#define SCHEMA_BASE_ITEM_MAX 499
#endif

class CEconItemAttribute;
class CAttributeManager;

#if defined(CSTRIKE_CLIENT_DLL)
class IVisualsDataProcessor;
#endif

struct stickerMaterialReference_t
{
	CMaterialReference	m_pMaterialReferenceFirstPerson;
	CMaterialReference	m_pMaterialReferenceThirdPerson;
	int					m_nSlotIndex;
};

enum EEconItemInventoryImagePurpose_t
{
	k_EEconItemInventoryImagePurpose_Generic = 0,
	k_EEconItemInventoryImagePurpose_Graffiti = 1,
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CAttributeList
{
	friend class CEconItemView;
	friend class CTFPlayer;

	DECLARE_CLASS_NOBASE( CAttributeList );
public:
	DECLARE_EMBEDDED_NETWORKVAR();
	DECLARE_DATADESC();

	CAttributeList();
	CAttributeList&	operator=( const CAttributeList &src );

	void					Init();
	void					SetManager( CAttributeManager *pManager );

	void					IterateAttributes( class IEconItemAttributeIterator *pIterator );

	// Remove all attributes on this item
	void					DestroyAllAttributes( void );

	// Prevent clients from modifying attributes directly from this list. They should do it from the CEconItemView.
private:
	// Add an attribute to this item
	void					AddAttribute( CEconItemAttribute *pAttribute );
	void					SetOrAddAttributeValueByName( const char *pszAttribDefName, float flValue );

	// Remove an attribute by name
	void					RemoveAttribute( const char *pszAttribDefName );
	void					RemoveAttributeByIndex( int iIndex );

public:
	// Returns the attribute that matches the attribute defname
	CEconItemAttribute *GetAttributeByName( const char *pszAttribDefName );
	const CEconItemAttribute *GetAttributeByName( const char *pszAttribDefName ) const;

	// Returns the attribute matching the specified class, carried by this entity, if it exists.
	CEconItemAttribute		*GetAttributeByClass( const char *szAttribClass );
	const CEconItemAttribute *GetAttributeByClass( const char *szAttribClass ) const;

	// Returns the attribute that matches the attribute defindex
	CEconItemAttribute		*GetAttributeByDefIndex( uint16 unAttrDefIndex );
	const CEconItemAttribute *GetAttributeByDefIndex( uint16 unAttrDefIndex ) const;


	// The only way to set the value of an attribute after its creation is through the attribute list 
	// that contains it. This way the matching attribute manager is told one of its attributes has changed.
	void					SetValue( CEconItemAttribute *pAttrib, float flValue );

	void					UpdateManagerCache( void );

private:
	// Attribute accessing
	int						GetNumAttributes( void ) const { return m_Attributes.Count(); }
	CEconItemAttribute		*GetAttribute( int iIndex ) { Assert( iIndex >= 0 && iIndex < m_Attributes.Count()); return &m_Attributes[iIndex]; }
	const CEconItemAttribute *GetAttribute( int iIndex ) const { Assert( iIndex >= 0 && iIndex < m_Attributes.Count()); return &m_Attributes[iIndex]; }

	// Our list of attributes
	CUtlVector<CEconItemAttribute>		m_Attributes;

	CAttributeManager		*m_pManager;
};

//-----------------------------------------------------------------------------
// Purpose: An attribute that knows how to read itself from a datafile, describe itself to the user,
//			and serialize itself between Servers, Clients, and Steam.
//			Unlike the attributes created in the Game DLL, this attribute doesn't know how to actually
//			do anything in the game, it just knows how to describe itself.
//-----------------------------------------------------------------------------
class CEconItemAttribute
{
	DECLARE_CLASS_NOBASE( CEconItemAttribute );
public:
	DECLARE_EMBEDDED_NETWORKVAR();

	CEconItemAttribute( const attrib_definition_index_t iAttributeIndex, float flValue );
	CEconItemAttribute( const attrib_definition_index_t iAttributeIndex, uint32 unValue );

	CEconItemAttribute & operator=( const CEconItemAttribute &val );
	// Get the index of this attribute's definition inside the script file
	attrib_definition_index_t			GetAttribIndex( void ) const { return m_iAttributeDefinitionIndex; }
	void			SetAttribIndex( attrib_definition_index_t iIndex ) { m_iAttributeDefinitionIndex = iIndex; }

	// Get the static data contained in this attribute's definition
	const CEconItemAttributeDefinition *GetStaticData( void ) const;

	// Get the float value of this attribute.
	float			GetValue( void ) const;

	// Get the value of this attribute as an unsigned integer.
	uint32			GetIntValue( void ) const;

	float			GetInitialValue( void ) const { return m_flInitialValue; }
	int				GetRefundableCurrency( void ) const { return m_nRefundableCurrency; }
	void			AddRefundableCurrency( int nAdd ) { m_nRefundableCurrency += nAdd; }
	void			RemovedRefundableCurrency( int nSubtract ) { m_nRefundableCurrency -= nSubtract; }

	// Get & Set the setbonus flag. Allowed to set it here because it doesn't affect attribute caches.
	bool			GetSetBonus( void ) { return m_bSetBonus; }
	void			SetSetBonus( bool bBonus ) { m_bSetBonus = bBonus; }

private:
	// The only way to set the value of an attribute after its creation is through the attribute list 
	// that contains it. This way the matching attribute manager is told one of its attributes has changed.

	// Set the float value of this attribute.
	// Note that the value must be stored as a float!
	void			SetValue( float flValue );

	// Set the value of this attribute as an unsigned integer.
	// Note that the value must be stored as an integer!
	// See CEconItemAttributeDefinition
	void			SetIntValue( uint32 unValue );

	friend class CAttributeList;
	friend class CEconItemSystem;

	void			Init( void );

	//--------------------------------------------------------
private:
	// This is the index of the attribute into the attributes read from the data files
	CNetworkVar( attrib_definition_index_t, m_iAttributeDefinitionIndex );

	// This is the value of the attribute. Used to modify the item's variables.
	CNetworkVar( float,	m_flValue );

	// This is the value that the attribute was first set to by an item definition
	CNetworkVar( float, m_flInitialValue );
	CNetworkVar( int, m_nRefundableCurrency );

	CNetworkVar( bool,	m_bSetBonus ); // Attribute has been generated by a set bonus.

public:
	// Used for copy constructor only
	CEconItemAttribute( void );
};

//-----------------------------------------------------------------------------
// Purpose: An item that knows how to read itself from a datafile, describe itself to the user,
//			and serialize itself between Servers, Clients, and Steam.
//
//			In the client DLL, we derive it from CDefaultClientRenderable so that
//			it can be passed in the pProxyData parameter of material proxies.
//-----------------------------------------------------------------------------
#if defined(CLIENT_DLL)
class CEconItemView : public CDefaultClientRenderable, public IEconItemInterface, public CCustomMaterialOwner
#else
class CEconItemView : public IEconItemInterface
#endif
{
#if defined(CLIENT_DLL) || defined(GAME_DLL)
	DECLARE_CLASS_NOBASE( CEconItemView );
public:
	DECLARE_EMBEDDED_NETWORKVAR();
	DECLARE_DATADESC();
#endif

public:
	CEconItemView();
	CEconItemView( const CEconItemView &src );
	virtual ~CEconItemView();
	CEconItemView&	operator=( const CEconItemView &src );


	bool 				operator==( const CEconItemView &other ) const;
	bool				operator!=( const CEconItemView &other ) const	{ return !operator==( other ); }

	virtual const GameItemDefinition_t *GetItemDefinition() const
	{
		return GetStaticData();
	}

public:

	// IEconItemInterface implementation.
	virtual int32			GetQuality() const;
	virtual int32			GetRarity() const;
	virtual style_index_t	GetStyle() const;
	virtual uint8			GetFlags() const;
	virtual eEconItemOrigin GetOrigin() const;
	virtual uint16				GetQuantity() const;

	virtual const char	   *GetCustomName() const;
	virtual const char	   *GetCustomDesc() const;

	virtual int				GetItemSetIndex() const;

	virtual bool			GetInUse() const { return GetSOCData() ? GetSOCData()->GetInUse() : false; }

	virtual void			IterateAttributes( class IEconItemAttributeIterator *pIterator ) const OVERRIDE;

	bool					IsValid( void ) const { return m_bInitialized; }
	void					Invalidate( void );

	// Initialize from the specified data
	// client will load SO cache as needed
	void					Init( int iDefIndex, int iQuality, int iLevel, uint32 iAccountID = 0 );
	void					SetInitialized( bool bInit ) { m_bInitialized = bInit; }
	bool					Init( CEconItem* pItem );

	bool					IsKnife( void ) const;

	// Get the static data contained in this item's definition
	const GameItemDefinition_t	*GetStaticData( void ) const;

	void					SetNonSOEconItem( CEconItem* pItem ) { m_pNonSOEconItem = pItem; }

	void					MarkDescriptionDirty( void );

	virtual bool			IsStyleUnlocked( int iStyle ) const;

private:
	void					EnsureDescriptionIsBuilt( void ) const;
public:
	void					SetGrayedOutReason( const char *pszGrayedOutReason );

	// Set & Get the index of this item's definition inside the script file
	void					SetItemIndex( item_definition_index_t iIndex ) { m_iItemDefinitionIndex = iIndex; MarkDescriptionDirty(); }
	item_definition_index_t	GetItemIndex( void ) const { return m_iItemDefinitionIndex; }

	// Set & Get the quality & level of this item.
	void					SetItemQuality( int iQuality ) { m_iEntityQuality = iQuality; MarkDescriptionDirty(); }
	int						GetItemQuality( void ) const { return m_iEntityQuality; }
	void					SetItemLevel( uint32 unLevel ) { m_iEntityLevel = unLevel; MarkDescriptionDirty(); }
	uint32					GetItemLevel( void ) const { return m_iEntityLevel; }

	int						GetItemQuantity() const;
#ifdef CLIENT_DLL
	void					SetIsStoreItem( bool bIsStoreItem ) { m_bIsStoreItem = bIsStoreItem; MarkDescriptionDirty(); }
	void					SetIsTradeItem( bool bIsTradeItem ) { m_bIsTradeItem = bIsTradeItem; MarkDescriptionDirty(); }
	void					SetItemQuantity( int iQuantity ) { m_iEntityQuantity = iQuantity; MarkDescriptionDirty(); }
	void					SetClientItemFlags( uint8 unFlags );
	void					SetItemRarityOverride( int iRarity ) { m_iRarityOverride = iRarity; MarkDescriptionDirty(); }
	void					SetItemQualityOverride( int iQuality ) { m_iQualityOverride = iQuality; MarkDescriptionDirty(); }
	void					SetItemStyleOverride( style_index_t unNewStyleOverride );
#endif
	style_index_t			GetItemStyle() const;

	// Access the worldwide global index of this item
	void					SetItemID( itemid_t iIdx ) { m_iItemID = iIdx; m_iItemIDHigh = (m_iItemID >> 32); m_iItemIDLow = (m_iItemID & 0xFFFFFFFF); }
#ifdef CLIENT_DLL
	// On the client, we need to rebuild it from the high & low networked pieces
	virtual itemid_t		GetItemID( void ) const { uint64 iTmp = ((((int64)m_iItemIDHigh)<<32) | m_iItemIDLow); return (itemid_t)iTmp; }
#else
	itemid_t				GetItemID( void ) const { return m_iItemID; }
#endif
	uint32					GetItemIDHigh( void ) const { return m_iItemIDHigh; }
	uint32					GetItemIDLow( void )  const { return m_iItemIDLow; }

	itemid_t				GetFauxItemIDFromDefinitionIndex( void ) const;

	uint32					GetAccountID( void ) const { return m_iAccountID; }

	// Access the inventory position of this item
	void					SetInventoryPosition( uint32 iPosition ) { m_iInventoryPosition = iPosition; }
	const uint32			GetInventoryPosition( void ) const { return m_iInventoryPosition; } 

	// Return the model to use for model panels containing this item
	const char				*GetInventoryModel( void ) const;
	// Return the image to use for model panels containing this item
	const char				*GetInventoryImage( EEconItemInventoryImagePurpose_t ePurpose = k_EEconItemInventoryImagePurpose_Generic ) const;
	bool					HasGeneratedInventoryImage( void ) const;
	bool					GetInventoryImageData( int *iPosition, int *iSize ) const;
	const char				*GetInventoryOverlayImage( int idx ) const;
	int						GetInventoryOverlayImageCount( void ) const;

	IMaterial				*GetToolStickerMaterial( void ) const;
	bool					IsStickerTool( void ) const;

	// Return the model to use when displaying this model on the player character model, if any
	const char				*GetPlayerDisplayModel( int iClass = 0 ) const;

	// Return the model to use when displaying this model in the world. See the notes on this in econ_item_schema.h
	const char				*GetWorldDisplayModel() const;
	const char				*GetExtraWearableModel() const;
	const char				*GetIconDisplayModel() const;
	const char				*GetBuyMenuDisplayModel() const;
	const char				*GetWorldDroppedModel() const;
	const char				*GetPedestalDisplayModel() const;
	const char				*GetMagazineModel() const;

	// A piece of geometry that masks the scope lens out of the full-screen zoomed blur effect
	const char				*GetScopeLensMaskModel() const;

	// Return the viewmodel stattrak module model path
	const char				*GetStatTrakModelByType( int nStatTrakType ) const;

	// Return the viewmodel uid module model path
	const char				*GetUidModel() const;

	// Return the load-out slot that this item must be placed into
	int						GetAnimationSlot( void ) const;
	
	// Return an int that indicates whether the item should be dropped from a dead owner.
	int						GetDropType( void ) const;

	// Remove all attributes on this item
	void					DestroyAllAttributes( void );

	// Add an attribute to this item
	void					AddAttribute( CEconItemAttribute *pAttribute );
	
	void					SetOrAddAttributeValueByName( const char *pszAttribDefName, float flValue );
	
	void					InitNetworkedDynamicAttributesForDemos( void );
	void					UpdateNetworkedDynamicAttributesForDemos( attrib_definition_index_t nDef, float flNewValue );

#if defined(CSTRIKE_CLIENT_DLL)
	void					UpdateGeneratedMaterial( bool bIgnorePicMip = false, MDLHandle_t modelHandle = MDLHANDLE_INVALID, CompositeTextureSize_t diffuseTextureSize = COMPOSITE_TEXTURE_SIZE_512 );
#endif //#if defined(CSTRIKE_CLIENT_DLL)

	// Return the sticker model path for the given index
	const char				*GetStickerSlotModelBySlotIndex( int nIndex ) const;
	int						GetNumSupportedStickerSlots( void ) const;
	Vector					GetStickerSlotWorldProjectionStartBySlotIndex( int nIndex ) const;
	Vector					GetStickerSlotWorldProjectionEndBySlotIndex( int nIndex ) const;
	const char				*GetStickerWorldModelBoneParentNameBySlotIndex( int nIndex ) const;
	const char				*GetStickerSlotMaterialBySlotIndex( int nIndex ) const;
	IMaterial				*GetStickerIMaterialBySlotIndex( int nIndex, bool bThirdPerson = false ) const;
	void					GenerateStickerMaterials( void );
	bool					ItemHasAnyStickersApplied( void ) const;
	bool					ItemHasAnyFreeStickerSlots( void ) const;
	int						GetStickerSlotFirstFreeFromIndex( int nIndex = 0 ) const;


	void					SetCustomNameOverride( const char *pszCustomName );

	// These functions are used by the client, as well as the server (in cs_gamestats.cpp)
	// NOTE: I haven't been able to verify the cache is actually coherent and not timing
	//       dependent on when these are called.  -- REI
	void					GenerateKillEaterTypeVector( void ) const;
	void					GetKillEaterTypes( CUtlSortVector<uint32> &types ) const;
	int32					GetKillEaterValueByType( uint32 uKillEaterType ) const;

	// Can be modified on a const view since these are caches of data from elsewhere.
	mutable bool                  m_bKillEaterTypesCached;
	mutable CUtlSortVector<uint32> m_vCachedKillEaterTypes;
	mutable int                   m_nKillEaterValuesCacheFrame;
	mutable CUtlHashtable<uint32, int32>  m_vCachedKillEaterValues;

private:
	void					Cleanup( void );

#if defined(CSTRIKE_CLIENT_DLL)
	// create custom materials based on the custom attributes
	void					CreateCustomWeaponMaterials( CSWeaponID pWeapon, bool bIgnorePicMip, MDLHandle_t modelHandle, CompositeTextureSize_t diffuseTextureSize = COMPOSITE_TEXTURE_SIZE_512 );
	void					CreateCustomClothingMaterials( const char *pchSkinIdent, int nSlotId, int nTeam, bool bIgnorePicMip, MDLHandle_t modelHandle, CompositeTextureSize_t diffuseTextureSize = COMPOSITE_TEXTURE_SIZE_512 );

public:
	bool					Update( void );
	void					SaveInventoryImageAsPNG( int nWidth, int nHeight );
	const CUtlBuffer	   *GetInventoryImageRgba( int nWidth, int nHeight, ImageReadyCallback_t pImageReadyCallback );
	void					GenerateInventoryImage( ImageReadyCallback_t pImageReadyCallback );
	bool					CanGenerateInventoryImageRgba( void ) const;
	void					ClearInventoryImageRgba( void );
	bool					LoadCachedInventoryImage( void );
	void					SaveInventoryImage( CUtlBuffer &rawImageRgba );
	const char*				GenerateCachedInventoryImageName();

	static void				CleanInventoryImageCacheDir( void );

	void					FinishLoadCachedInventoryImage( void* pData, int numReadBytes, FSAsyncStatus_t asyncStatus );

	void					CleanupImageReadyCallbacks();
	void					DispatchImageReadyCallbacks( uint64 nItemID );

	enum AsyncFixupState_t
	{
		AFS_Init,					// Initial state, need to load data
		AFS_LoadingInProgress,		// Async load kicked-off on job thread
		AFS_LoadingDone,			// Async load done, data ready for fixup on main thread
		AFS_FixupDone,				// Data fixed up on main thread
	} m_asyncFixupState;

	int						m_nNumAsyncReadBytes;
	FSAsyncStatus_t			m_asyncStatus;
	CUtlBuffer				m_inventoryImageRgba;

private:
	static bool				m_sbHasCleanedInventoryImageCacheDir;

	bool					m_bInventoryImageRgbaRequested;
	bool					m_bInventoryImageTriedCache;
#if defined ( PANORAMA_ENABLE )
	CUtlVector < ImageReadyCallback_t* > m_vecImageReadyCallbacks;
#else
	CUtlVector < ImageReadyCallback_t > m_vecImageReadyCallbacks;
#endif
	int						m_nInventoryImageRgbaWidth;
	int						m_nInventoryImageRgbaHeight;

	FSAsyncControl_t		m_hAsyncControl;

	char					m_szCurrentLoadCachedFileName[ MAX_PATH ];

	RenderToRTData_t		*m_pRenderToRTData;
	IVTFTexture				*m_pScratchVTF;
	CMergedMDL				*m_pRenderToRTMDL;
#endif //#if defined(CSTRIKE_CLIENT_DLL)

	CUtlVector< stickerMaterialReference_t > m_pStickerMaterials;




	// Remove an attribute by name
	void					RemoveAttribute( const char *pszAttribDefName );

public:

	const bool				GetCombinedAttributeClassValue( float &flValue, string_t iszAttribClass ) const;
	
	// Returns the attribute matching the specified class, carried by this entity, if it exists.
	CEconItemAttribute		*GetAttributeByClass( const char *szAttribClass );
	const CEconItemAttribute *GetAttributeByClass( const char *szAttribClass ) const;
	// Returns the attribute that matches the attribute def index, if it exists
	CEconItemAttribute		*GetAttributeByDefIndex( int iAttributeDefIndex );
	const CEconItemAttribute *GetAttributeByDefIndex( int iAttributeDefIndex ) const;
	// Returns the attribute that matches the def name, if it exists
	CEconItemAttribute		*GetAttributeByName( const char *pszAttribDefName );
	const CEconItemAttribute *GetAttributeByName( const char *pszAttribDefName ) const;

	// Attribute accessing
	int						  GetNumAttributes( void ) const { return m_AttributeList.GetNumAttributes(); }
	CEconItemAttribute		 *GetAttribute( int iIndex ) { return m_AttributeList.GetAttribute( iIndex ); }
	const CEconItemAttribute *GetAttribute( int iIndex ) const { return m_AttributeList.GetAttribute( iIndex ); }
	CAttributeList			 *GetAttributeList( void ) { return m_AttributeList.Get(); }
	const CAttributeList	 *GetAttributeList( void ) const { return m_AttributeList.Get(); }

	// Items that have attributes that modify their RGB values
	int						GetModifiedRGBValue( bool bAltColor=false ) const;
	int						GetCustomPaintKitIndex( void ) const;

	const char*				GetCustomPaintKitDbgName( void ) const;

	// Returns true if subject is a part of this item's collection set and is not yet collected.
	bool					CanCollect( CEconItemView &subject ) const;

	// Returns the UGC file ID of the custom texture assigned to this item.  If non-zero, then it has a custom texture.
	uint64					 GetCustomUserTextureID() const;
	const CPaintKit			 *GetCustomPaintKit( void ) const;

	CEconItem				*GetSOCData( void ) const;

	bool					IsEquipped( void ) const { return GetSOCData() && GetSOCData()->IsEquipped(); }
	bool					IsEquippedForClass( equipped_class_t unClass ) const { return GetSOCData() && GetSOCData()->IsEquippedForClass( unClass ); }
	void					UpdateEquippedState( equipped_class_t unClass, equipped_slot_t unSlot ) { if ( GetSOCData() ) { GetSOCData()->UpdateEquippedState( unClass, unSlot ); } }
	equipped_slot_t			GetEquippedPositionForClass( equipped_class_t unClass ) const { return GetSOCData() ? GetSOCData()->GetEquippedPositionForClass( unClass ) : INVALID_EQUIPPED_SLOT; }

	EItemSlot				GetSlot( void ) const { return EItemSlotFromName( GetItemDefinition()->GetRawDefinition()->GetString("item_slot") ); }

	// Attached particle systems
	int						GetQualityParticleType() const;

	int						GetSkin() const;

#if defined(CSTRIKE_CLIENT_DLL)
	IVisualsDataProcessor	*GetVisualsDataProcessor( int nIndex ) const { return ( nIndex < m_ppVisualsDataProcessors.Count() ) ? m_ppVisualsDataProcessors[ nIndex ] : NULL; }
	IVisualsDataProcessor	*GetVisualsDataProcessorByName( const char* szName ) const;

	static const CEconItemView * FindOrCreateEconItemViewForItemID( uint64 uiItemId );
	
	bool					m_bRestoreCustomMaterialAfterPrecache;
#else
	typedef CUtlMap< itemid_t, uint64, int, CDefLess< itemid_t > > UtlMapLookupByID_t;
	static UtlMapLookupByID_t s_mapLookupByID;
#endif

#if defined ( GAME_DLL )
	void UpdateNetworkedCustomName();
#endif

protected:
	// Index of the item definition in the item script file.
	CNetworkVar( item_definition_index_t,	m_iItemDefinitionIndex );

	// The quality of this item.
	CNetworkVar( int,		m_iEntityQuality );

	// The level of this item.
	CNetworkVar( uint32,	m_iEntityLevel );

	// The global index of this item, worldwide.
	itemid_t			m_iItemID;
	CNetworkVar( uint32,	m_iItemIDHigh );
	CNetworkVar( uint32,	m_iItemIDLow );

	// Account ID of the person who has this in their inventory
	CNetworkVar( uint32,	m_iAccountID );

	// Position inside the player's inventory
	CNetworkVar( uint32,	m_iInventoryPosition );

	// This is an alternate source of data, if this item models something that isn't in the SO cache.
	CEconItem*		m_pNonSOEconItem;

	CNetworkVar( bool,		m_bInitialized );

	bool					m_bIsKnife;
#if defined( CLIENT_DLL )
	// exist on the client only
	bool					m_bIsStoreItem;
	bool					m_bIsTradeItem;
	int						m_iEntityQuantity;
	int						m_iRarityOverride;
	int						m_iQualityOverride;
	uint8					m_unClientFlags;

	
	// clients have the ability to force a style on an item view -- this is used for store previews,
	// character panels, etc.
	style_index_t			m_unOverrideStyle;

public:
	// Return the single-line name of this item.
	const wchar_t			*GetItemName( bool bUncustomized = false ) const;

	bool GetItemDisplayNameUtf8( char *szBuffer, size_t nMaxLenBytes, bool bUncustomized = false ) const;

	// Return the full structure with all of our description lines.
	const class CEconItemDescription *GetDescription() const { EnsureDescriptionIsBuilt(); return m_pDescription; }

private:
	mutable class CEconItemDescription	*m_pDescription;
	mutable char *m_pszGrayedOutReason;

#endif
#if defined(CSTRIKE_CLIENT_DLL)
	CUtlVector< IVisualsDataProcessor* > m_ppVisualsDataProcessors;
#endif

protected:

#if defined(CLIENT_DLL)
	// IClientRenderable
	virtual const Vector&	GetRenderOrigin( void ) { return vec3_origin; }
	virtual const QAngle&	GetRenderAngles( void ) { return vec3_angle; }
	virtual bool			ShouldDraw( void ) { return false; }
	virtual bool			IsTransparent( void ) { return false;}
	virtual const matrix3x4_t &RenderableToWorldTransform() { static matrix3x4_t mat; SetIdentityMatrix( mat ); return mat; }
	virtual void			GetRenderBounds( Vector& mins, Vector& maxs ) { return; }
#endif

private:
	CNetworkVarEmbedded( CAttributeList,	m_AttributeList );
	CNetworkVarEmbedded( CAttributeList,	m_NetworkedDynamicAttributesForDemos );

	// the above attribute lists store all attributes as floats... since we only have one string attribute
	// we're networking it separately instead of doing the work to expand attribute lists to support string attrs.
	CNetworkString( m_szCustomName, MAX_ITEM_CUSTOM_NAME_DATABASE_SIZE );

	char m_szCustomNameOverride[ MAX_ITEM_CUSTOM_NAME_DATABASE_SIZE ];
	GCSDK::CAutoPtr< char > m_autoptrInventoryImageGeneratedPath;


	// Item Tags. Used for sorting inventories




#ifdef CLIENT_DLL // Tag-related stuff.  Client-only!
public:
	typedef CUtlMap< CUtlConstString, CUtlConstString, int, StringLessFunctor > MapTags_t;

private:
	MapTags_t m_mapTags;

	typedef CUtlDict< CUtlConstWideString > dictTagsTokenLookup_t;
	static dictTagsTokenLookup_t s_dictTagTokenLookup;

	bool m_bInitializedTags;
	void InitializeTags();

public:
	//MapTags_t * GetTags( void );
	const MapTags_t * GetTags( void ) const;

//	KeyValues * GetTagsKv() const { return m_pKvTags; }
	static dictTagsTokenLookup_t * s_GetTagsTokenLookup( void ) { return  &s_dictTagTokenLookup; }
	void AddAssetTag( const char *pchInternalName, locchar_t *pchLocalizedName, const char *pchCategory = "Misc", locchar_t *pchCategoryLocalizedName = NULL, const char *pchColor = NULL );
	bool HasTag( const char * pchCategory, const char * pchValue = NULL ) const;
	const char * GetTagValue( const char * pchCategory, MapTags_t::IndexType_t &tReturnIndex, MapTags_t::IndexType_t tStartIndex = -1 ) const;
#endif // CLIENT_DLL

};

#endif // ECON_ITEM_CONSTANTS_H
