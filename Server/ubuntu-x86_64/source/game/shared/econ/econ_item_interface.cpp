
#include "cbase.h"
#include "econ_item_interface.h"
#include "econ_item_view_helpers.h"

#ifdef GC_DLL
#include "econ/econ_assetapi_context.h"	// for AddAssetDescription
#endif

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
RTime32 IEconItemInterface::GetExpirationDate() const
{
	COMPILE_TIME_ASSERT( sizeof( float ) == sizeof( RTime32 ) );

	// dynamic attributes, if present, will override any static expiration timer
	static CSchemaAttributeDefHandle pAttrib_ExpirationDate( "expiration date" );

	attrib_value_t unAttribExpirationTimeBits;
	COMPILE_TIME_ASSERT( sizeof( unAttribExpirationTimeBits ) == sizeof( RTime32 ) );

	if ( pAttrib_ExpirationDate && FindAttribute( pAttrib_ExpirationDate, &unAttribExpirationTimeBits ) )
		return *(RTime32 *)&unAttribExpirationTimeBits;

	// do we have a static timer set in the schema for all instances to expire?
	return GetItemDefinition()
		? GetItemDefinition()->GetExpirationDate()
		: RTime32( 0 );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
RTime32 IEconItemInterface::GetTradableAfterDateTime() const
{
	static CSchemaAttributeDefHandle pAttrib( "tradable after date" );
	Assert( pAttrib );

	if ( !pAttrib )
		return 0;

	RTime32 rtTimestamp;
	if ( !FindAttribute( pAttrib, &rtTimestamp ) )
		return 0;

	return rtTimestamp;
}

RTime32 IEconItemInterface::GetUseAfterDateTime() const
{
	static CSchemaAttributeDefHandle pAttrib( "use after date" );
	Assert( pAttrib );

	if ( !pAttrib )
		return 0;

	RTime32 rtTimestamp;
	if ( !FindAttribute( pAttrib, &rtTimestamp ) )
		return 0;

	return rtTimestamp;
}


struct VectorRange { int start; int count; int used; };
CUtlVector< const CEconLootListDefinition* > g_LootListRefs;
CUtlMap<itemid_t, VectorRange, int, CDefLess<itemid_t> > g_ItemLootListRefs; // only faux item ids
bool g_bInitializedLootListRefs = false;

itemid_t Helper_GenerateFauxItemIdFromItemEntry( const item_list_entry_t& entry )
{
	static CSchemaItemDefHandle hSticker( "sticker" );
	static CSchemaItemDefHandle hSpray( "spray" );
	static CSchemaItemDefHandle hSprayPaint( "spraypaint" );

	if ( entry.m_nItemDef == 0 )
	{
		return 0;
	}
	else if ( entry.m_nItemDef == hSticker->GetDefinitionIndex()
		|| entry.m_nItemDef == hSpray->GetDefinitionIndex()
		|| entry.m_nItemDef == hSprayPaint->GetDefinitionIndex() )
	{
		if ( entry.m_nStickerKit )
		{
			return CombinedItemIdMakeFromDefIndexAndPaint( entry.m_nItemDef, entry.m_nStickerKit );
		}
	}
	else if ( entry.m_nPaintKit != 0 )
	{
		return CombinedItemIdMakeFromDefIndexAndPaint( entry.m_nItemDef, entry.m_nPaintKit );
	}

	return 0;
}

static void InitLootListRefs()
{
	if ( g_bInitializedLootListRefs )
		return;

	g_bInitializedLootListRefs = true;

	// pass 1: figure out counts for each item
	const CUtlDict<CEconLootListDefinition>& lootLists = GetItemSchema()->GetLootLists();
	CUtlVectorFixedGrowable<const CEconLootListDefinition*, 8> subLists;
	FOR_EACH_DICT_FAST( lootLists, iLootList )
	{
		// we are going to iterate over sub-loot-lists multiple times here, but it should be pretty fast
		// since we only do this once and most sub-loot lists are only in 1 parent
		Assert( subLists.IsEmpty() );
		const CEconLootListDefinition* pOrigList = &lootLists.Element( iLootList );
		subLists.AddToTail( pOrigList );
		while ( !subLists.IsEmpty() )
		{
			const CEconLootListDefinition* pLootListDef = subLists.Tail();
			subLists.RemoveMultipleFromTail( 1 );

			const CUtlVector<item_list_entry_t>& listContents = pLootListDef->GetLootListContents();
			FOR_EACH_VEC( listContents, iListContents )
			{
				const item_list_entry_t& entry = listContents[iListContents];

				if ( entry.m_bIsNestedList )
				{
					subLists.AddToTail( GetItemSchema()->GetLootListByIndex( entry.m_nItemDef ) );
				}
				else if ( itemid_t fauxItemId = Helper_GenerateFauxItemIdFromItemEntry( entry ) )
				{
					int iItemRef = g_ItemLootListRefs.Find( fauxItemId );
					if ( iItemRef == g_ItemLootListRefs.InvalidIndex() )
					{
						// First copy of this entry, insert
						static const VectorRange newRange = { 0,0,0 };
						iItemRef = g_ItemLootListRefs.Insert( fauxItemId, newRange );
					}
					// allocate a loot list element for this item
					g_ItemLootListRefs.Element( iItemRef ).count++;
				}
			}
		}
	}

	// allocate array space in g_LootListRefs;
	int totalListItems = 0;
	FOR_EACH_MAP_FAST( g_ItemLootListRefs, iItemRef )
	{
		VectorRange& range = g_ItemLootListRefs.Element( iItemRef );
		range.start = totalListItems;
		totalListItems += range.count;
	}
	g_LootListRefs.SetCount( totalListItems );

	// pass 2: fill g_LootListRefs
	FOR_EACH_DICT_FAST( lootLists, iLootList )
	{
		// we are going to iterate over sub-loot-lists multiple times here, but it should be pretty fast
		// since we only do this once and most sub-loot lists are only in 1 parent
		Assert( subLists.IsEmpty() );
		const CEconLootListDefinition* pOrigList = &lootLists.Element( iLootList );
		subLists.AddToTail( pOrigList );
		while ( !subLists.IsEmpty() )
		{
			const CEconLootListDefinition* pLootListDef = subLists.Tail();
			subLists.RemoveMultipleFromTail( 1 );

			const CUtlVector<item_list_entry_t>& listContents = pLootListDef->GetLootListContents();
			FOR_EACH_VEC( listContents, iListContents )
			{
				const item_list_entry_t& entry = listContents[iListContents];

				if ( entry.m_bIsNestedList )
				{
					subLists.AddToTail( GetItemSchema()->GetLootListByIndex( entry.m_nItemDef ) );
				}
				else if ( itemid_t fauxItemId = Helper_GenerateFauxItemIdFromItemEntry( entry ) )
				{
					int iItemRef = g_ItemLootListRefs.Find( fauxItemId );
					Assert( iItemRef != g_ItemLootListRefs.InvalidIndex() );
					if ( iItemRef != g_ItemLootListRefs.InvalidIndex() )
					{
						VectorRange& range = g_ItemLootListRefs.Element( iItemRef );
						Assert( range.used < range.count );
						if ( range.used < range.count )
						{
							g_LootListRefs[range.start + range.used] = pOrigList;
							range.used++;
						}
					}
				}
			}
		}
	}

	// verify that we initialized properly
	FOR_EACH_MAP_FAST( g_ItemLootListRefs, i )
	{
		VectorRange& range = g_ItemLootListRefs.Element( i );
		Assert( range.used == range.count );
	}

	// now initialize revolving loot list table

}

bool Helper_IsItemInLootList( IEconItemInterface * pItem, const CEconLootListDefinition * pLootListDef )
{
	// TODO: Move this to econ item schema post-init
	if ( !g_bInitializedLootListRefs )
	{
		InitLootListRefs();
	}

	extern bool Helper_IsGraphicTool( const CEconItemDefinition * pEconItemDefinition );
	itemid_t fauxItemId = 0;
	if ( Helper_IsGraphicTool( pItem->GetItemDefinition() ) )
	{
		static CSchemaAttributeDefHandle pAttrDef_StickerID( "sticker slot 0 id" );
		attrib_value_t unStickerKit = 0;
		if ( pItem->FindAttribute( pAttrDef_StickerID, &unStickerKit ) && unStickerKit != 0 )
			fauxItemId = CombinedItemIdMakeFromDefIndexAndPaint( pItem->GetItemDefinition()->GetDefinitionIndex(), unStickerKit );
	}
	else
	{
		fauxItemId = CombinedItemIdMakeFromDefIndexAndPaint( pItem->GetItemDefinition()->GetDefinitionIndex(), pItem->GetCustomPaintKitIndex() );
	}

	// look up item in loot list cache
	int iEntry = g_ItemLootListRefs.Find( fauxItemId );
	if ( iEntry != g_ItemLootListRefs.InvalidIndex() )
	{
		VectorRange range = g_ItemLootListRefs.Element( iEntry );
		int rangeEnd = range.start + range.count;
		for ( int i = range.start; i < rangeEnd; ++i )
		{
			if ( g_LootListRefs[i] == pLootListDef )
				return true;
		}
	}
	
	// not found
	return false;
}





RTime32 IEconItemInterface::GetCacheRefreshDateTime() const
{
	RTime32 rtExpiration = 0;
	/** Removed for partner depot **/
	return rtExpiration;
}

IEconItemInterface::~IEconItemInterface()
{

}

int IEconItemInterface::GetCustomPaintKitIndex( void ) const
{
	static CSchemaAttributeDefHandle pAttrDef_PaintKit( "set item texture prefab" );
	float flPaintKit = 0;
	FindAttribute_UnsafeBitwiseCast<attrib_value_t>( this, pAttrDef_PaintKit, &flPaintKit );

	return flPaintKit;
}

int IEconItemInterface::GetCustomPaintKitSeed( void ) const
{
	static CSchemaAttributeDefHandle pAttrDef_PaintKitSeed( "set item texture seed" );
	float flPaintSeed = 0;
	FindAttribute_UnsafeBitwiseCast<attrib_value_t>( this, pAttrDef_PaintKitSeed, &flPaintSeed );

	return flPaintSeed;
}

float IEconItemInterface::GetCustomPaintKitWear( float flWearDefault /*= 0.0f*/ ) const
{
	static CSchemaAttributeDefHandle pAttrDef_PaintKitWear( "set item texture wear" );
	float flPaintKitWear = flWearDefault;
	FindAttribute_UnsafeBitwiseCast<attrib_value_t>( this, pAttrDef_PaintKitWear, &flPaintKitWear );

	return flPaintKitWear;
}

float IEconItemInterface::GetStickerAttributeBySlotIndexFloat( int nSlotIndex, EStickerAttributeType type, float flDefault ) const
{
	const CSchemaAttributeDefHandle &attrDef = GetStickerAttributeDefHandle( nSlotIndex, type );
	if ( attrDef )
	{
		if ( attrDef->IsStoredAsFloat() )
		{
			float flOutput = 0.0f;
			if ( FindAttribute_UnsafeBitwiseCast< attrib_value_t >( this, attrDef, &flOutput ) )
			{
				return flOutput;
			}
		}
		else
		{
			Assert( false );
		}
	}

	return flDefault;	
}

uint32 IEconItemInterface::GetStickerAttributeBySlotIndexInt( int nSlotIndex, EStickerAttributeType type, uint32 uiDefault ) const
{
	const CSchemaAttributeDefHandle &attrDef = GetStickerAttributeDefHandle( nSlotIndex, type );
	if ( attrDef )
	{
		if ( attrDef->IsStoredAsFloat() )
		{
			Assert( false );
		}
		else
		{
			uint32 unOutput;
			if ( FindAttribute( attrDef, &unOutput ) )
			{
				return unOutput;
			}
		}
	}

	return uiDefault;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool IEconItemInterface::IsTradable() const
{
	/** Removed for partner depot **/
	return false;
}

bool IEconItemInterface::IsPotentiallyTradable() const
{
	if ( GetItemDefinition() == NULL )
		return false;

	// check attributes
	
	static CSchemaAttributeDefHandle pAttrDef_AlwaysTradableAndUsableInCrafting( "always tradable" );
	static CSchemaAttributeDefHandle pAttrib_CannotTrade( "cannot trade" );

	
	Assert( pAttrDef_AlwaysTradableAndUsableInCrafting != NULL );
	Assert( pAttrib_CannotTrade != NULL );

	if ( pAttrDef_AlwaysTradableAndUsableInCrafting == NULL || pAttrib_CannotTrade == NULL )
		return false;

// NOTE: we are not checking the time delay on trade restriction here - the item is considered potentially tradable in future = true
// 	if ( GetTradableAfterDateTime() > CRTime::RTime32TimeCur() )
// 		return false;

	// Explicit rules by item quality to validate trading eligibility
	switch ( GetQuality() )
	{
	case AE_UNIQUE:
		break;
	case AE_STRANGE:
		return ( GetOrigin() == kEconItemOrigin_FoundInCrate || GetOrigin() == kEconItemOrigin_Crafted || GetOrigin() == kEconItemOrigin_Purchased );
	case AE_UNUSUAL:
		return ( GetOrigin() == kEconItemOrigin_FoundInCrate );
	case AE_TOURNAMENT:
		return ( GetOrigin() == kEconItemOrigin_FoundInCrate || GetOrigin() == kEconItemOrigin_StorePromotion );
	default:
		// all other qualities are untradable
		return false;
	}

	if ( FindAttribute( pAttrDef_AlwaysTradableAndUsableInCrafting ) )
		return true;

	if ( FindAttribute( pAttrib_CannotTrade ) )
		return false;

	// items gained in this way are not tradable
	switch ( GetOrigin() )
	{
	case kEconItemOrigin_Invalid:
	case kEconItemOrigin_Achievement:
	case kEconItemOrigin_Foreign:
	case kEconItemOrigin_PreviewItem:
	case kEconItemOrigin_SteamWorkshopContribution:
	case kEconItemOrigin_StockItem:
		return false;
	}

	// certain quality levels are not tradable
	if ( GetQuality() >= AE_COMMUNITY && GetQuality() <= AE_SELFMADE )
		return false;

	// explicitly marked cannot trade?
	if ( ( kEconItemFlags_CheckFlags_CannotTrade & GetFlags() ) != 0 )
		return false;

	// tagged to not be a part of the economy?
	if ( ( kEconItemFlag_NonEconomy & GetFlags() ) != 0 )
		return false;

	// This code catches stock items with name tags (rarity is stock) and prevents them from trading/marketing
	// until we have a better solution for extracting value out of reselling these items and avoiding scams
	if ( GetRarity() <= 0 )
		return false;

	if ( GetItemSchema()->BHasTradeRestrictedStickerKits() )
	{
		// September 2017 Chinese launch: we are making some time-limited sticker gifts, and don't want them
		// to be on the market, so we will be checking all sticker slots for a sticker that cannot be traded
		// and that will make the items "untradable". However stickers also have "tradable after date" attribute,
		// so that attribute will persist and tell owners that the non-tradable items will become tradable when
		// the date arrives.
		//
		// When the game no longer has trade restricted sticker kits, we will not be doing six look ups
		// for sticker attributes on every item.
		//
		for ( int iStickerSlot = 0; iStickerSlot < g_nNumStickerAttrs; ++iStickerSlot )
		{
			uint32 nStickerId = GetStickerAttributeBySlotIndexInt( iStickerSlot, k_EStickerAttribute_ID, 0 );
			if ( nStickerId > 0 )
			{
				const CStickerKit *pStickerKit = GetItemSchema()->GetStickerKitDefinition( nStickerId );
				if ( pStickerKit
					&& pStickerKit->bTradeRestricted )
					return false;
			}
		}
	}

	return true;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool IEconItemInterface::IsMarketable() const
{
	/** Removed for partner depot **/
	return false;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool IEconItemInterface::IsCommodity() const
{
	const CEconItemDefinition *pItemDef = GetItemDefinition();
	if ( pItemDef == NULL )
		return false;

	static CSchemaAttributeDefHandle pAttrib_IsCommodity( "is commodity" );
	Assert( pAttrib_IsCommodity != NULL );
	if ( pAttrib_IsCommodity == NULL )
		return false;

	attrib_value_t unAttribValue;
	if ( FindAttribute( pAttrib_IsCommodity, &unAttribValue ) && unAttribValue )
		return true;

	return false;
}

bool IEconItemInterface::IsHiddenFromDropList() const
{
	const CEconItemDefinition *pItemDef = GetItemDefinition();
	if ( pItemDef == NULL )
		return false;

	static CSchemaAttributeDefHandle pAttrib_HideFromDropList( "hide from drop list" );
	Assert( pAttrib_HideFromDropList != NULL );
	if ( pAttrib_HideFromDropList == NULL )
		return false;

	attrib_value_t unAttribValue;
	if ( FindAttribute( pAttrib_HideFromDropList, &unAttribValue ) && unAttribValue )
		return true;

	return false;
}


// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool IEconItemInterface::IsUsableInCrafting() const
{
	if ( GetItemDefinition() == NULL )
		return false;

	// check attribute
	static CSchemaAttributeDefHandle pAttrDef_AlwaysTradableAndUsableInCrafting( "always tradable" );
	Assert( pAttrDef_AlwaysTradableAndUsableInCrafting );

	if ( FindAttribute( pAttrDef_AlwaysTradableAndUsableInCrafting ) )
		return true;

	// explicitly marked not usable in crafting?
	if ( ( kEconItemFlags_CheckFlags_NotUsableInCrafting & GetFlags() ) != 0 )
		return false;

	// items gained in this way are not craftable
	switch ( GetOrigin() )
	{
	case kEconItemOrigin_Invalid:
	case kEconItemOrigin_Foreign:
	case kEconItemOrigin_PreviewItem:
	case kEconItemOrigin_Purchased:
	case kEconItemOrigin_StorePromotion:
	case kEconItemOrigin_SteamWorkshopContribution:
		return false;
	}

	// certain quality levels are not craftable
	if ( GetQuality() >= AE_COMMUNITY && GetQuality() <= AE_SELFMADE )
		return false;

	// tagged to not be a part of the economy?
	if ( ( kEconItemFlag_NonEconomy & GetFlags() ) != 0 )
		return false;

	return true;
}
