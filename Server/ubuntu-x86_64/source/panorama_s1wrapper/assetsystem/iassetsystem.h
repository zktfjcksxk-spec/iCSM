#ifndef INCLUDED_IASSETSYSTEM_H
#define INCLUDED_IASSETSYSTEM_H
//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

// Locations where assets can be found
enum AssetLocation_t
{
	ASSET_LOCATION_INVALID = -1,

	ASSET_LOCATION_GAME = 0, // 'game'
	ASSET_LOCATION_CONTENT, // 'content'

	ASSET_LOCATION_COUNT
};


typedef int ModSearchIndex_t;  // index of the mod in search path order (smaller number = leafier mod)
#define MOD_SEARCH_INDEX_INVALID ( ModSearchIndex_t(-1) )

// These special indices can be used with methods like GetAbsolutePath to control
// which mod the retrieval is done for.
//
// The leafiest mod for the given asset location.
#define MOD_SEARCH_INDEX_LEAFIEST_FOR_LOCATION ( ModSearchIndex_t(-2) )
// The leafiest mod for the whole asset (across all locations).
#define MOD_SEARCH_INDEX_LEAFIEST_FOR_ASSET    ( ModSearchIndex_t(-3) )
// The leafiest mod for all assets (across all locations).
#define MOD_SEARCH_INDEX_LEAFIEST_MOD          ( ModSearchIndex_t(0) )

abstract_class IAsset
{
public:

	virtual const char *GetAbsolutePath_Transient( AssetLocation_t nLocation, ModSearchIndex_t nModFilter = MOD_SEARCH_INDEX_LEAFIEST_FOR_LOCATION ) const = 0;

};

abstract_class IAssetSystem 
{
public:
 	virtual IAsset *FindAssetByFilename( const char *pFilename ) = 0;
	virtual void OpenForEdit( const CUtlVector<IAsset*> &assets, AssetLocation_t nFileLocation = ASSET_LOCATION_CONTENT ) = 0;
 	virtual bool GetAbsoluteFileCRC( const char *pAbsolutePath, CRC32_t *pOutCRC ) = 0;
};

extern IAssetSystem *g_pAssetSystem;

#ifndef Log_Detailed
#define Log_Detailed( Channel, /* [LoggingMetaData_t *], [Color], Message, */ ... ) //InternalMsg( Channel, LS_DETAILED, /* [LoggingMetaData_t *], [Color], Message, */ ##__VA_ARGS__ )
#endif
#define k_EFioStandardMetadata 0

class CCommandContext
{
public:
//	CCommandContext( CommandTarget_t target = CT_NO_TARGET ) : m_nTarget( target ) {}
	int m_nTarget;
};

#endif // INCLUDED_IASSETSYSTEM_H