
#include "cbase.h"

#include "br_config.h"
#include "kv3lib/keyvalues3.h"

#include <cstdarg>

enum KVParseMsgStatus
{
	KVPARSEMSG_ALL,

	KVPARSEMSG_INFO,
	KVPARSEMSG_WARNING,
	KVPARSEMSG_ERROR,

	KVPARSEMSG_NONE
};

#if 1
#define KVPARSE_OK		true
#define KVPARSE_FAIL	false
typedef bool KvParseResult_t;
#else
enum EKvParseResult {
	KVPARSE_OK,
	KVPARSE_FAIL,
};

class KvParseResult_t
{
public:
	KvParseResult_t( EKvParseResult result ) : mResult( result ) {}

	explicit operator bool() {
		return mResult == KVPARSE_OK;
	}

	EKvParseResult mResult;
};
#endif

struct KVParseMsg
{
	KVParseMsgStatus m_Status;
	const char* m_szFilename;
	int m_nLineNumber;
	CUtlString m_Message;
};

class KVParseContext
{
public:
	explicit KVParseContext( KVParseMsgStatus warningLevel );

	// Prints collected errors to developer console
	void LogErrors() const;

	// always returns KVPARSE_FAIL, used in return ParseError(...) pattern
	KvParseResult_t ParseError( const KeyValues3* pCtx, PRINTF_FORMAT_STRING const char* msgFmt, ... ) FMTFUNCTION( 3, 4 );
	KvParseResult_t ParseErrorInfo( const KeyValues3* pCtx, PRINTF_FORMAT_STRING const char* msgFmt, ... ) FMTFUNCTION( 3, 4 );

	void ParseMsg( KVParseMsgStatus status, const KeyValues3* pCtx, PRINTF_FORMAT_STRING const char* msgFmt, ... ) FMTFUNCTION( 4, 5 );

private:
	KVParseMsgStatus m_WarningLevel;
	KVParseMsgStatus m_WorstMessageLevel;
	CUtlVector<KVParseMsg> m_Messages;

	void InternalAddMsgV( KVParseMsgStatus status, const KeyValues3* pCtx, PRINTF_FORMAT_STRING const char* msgFmt, va_list args );
};

KVParseContext::KVParseContext( KVParseMsgStatus warningLevel )
	: m_WarningLevel( warningLevel )
	, m_WorstMessageLevel( KVPARSEMSG_ALL )
	, m_Messages()
{
}

void KVParseContext::LogErrors() const
{
	static const Color colors[KVPARSEMSG_NONE] = {
		Color( 255,255,255,255 ),
		Color( 255,255,255,255 ),
		Color( 255,255,  0,255 ),
		Color( 255,  0,  0,255 )
	};

	FOR_EACH_VEC( m_Messages, i )
	{
		const KVParseMsg& msg = m_Messages[i];
		int iColor = msg.m_Status;
		if ( iColor < 0 || iColor >= KVPARSEMSG_NONE )
			iColor = 0;

		ConColorMsg( colors[iColor], "%s(%d): %s\n", msg.m_szFilename, msg.m_nLineNumber, msg.m_Message.Get() );
	}
}

KvParseResult_t KVParseContext::ParseError( const KeyValues3* pCtx, const char* msgFmt, ... )
{
	va_list args;
	va_start( args, msgFmt );
	InternalAddMsgV( KVPARSEMSG_ERROR, pCtx, msgFmt, args );
	va_end( args );

	return KVPARSE_FAIL; // used in return ParseError(...) pattern
}

KvParseResult_t KVParseContext::ParseErrorInfo( const KeyValues3* pCtx, const char* msgFmt, ... )
{
	va_list args;
	va_start( args, msgFmt );
	InternalAddMsgV( KVPARSEMSG_INFO, pCtx, msgFmt, args );
	va_end( args );

	return KVPARSE_FAIL; // used in return ParseError(...) pattern
}

void KVParseContext::ParseMsg( KVParseMsgStatus status, const KeyValues3* pCtx, PRINTF_FORMAT_STRING const char* msgFmt, ... )
{
	va_list args;
	va_start( args, msgFmt );
	InternalAddMsgV( status, pCtx, msgFmt, args );
	va_end( args );
}

void KVParseContext::InternalAddMsgV( KVParseMsgStatus status, const KeyValues3* pCtx, PRINTF_FORMAT_STRING const char* msgFmt, va_list args )
{
	if ( status > m_WorstMessageLevel )
		m_WorstMessageLevel = status;

	if ( status < m_WarningLevel )
		return;

	KVParseMsg* pMsg = m_Messages.AddToTailGetPtr();
	pMsg->m_Status = status;
	if( pCtx && pCtx->HasMetadata() )
	{
		pMsg->m_szFilename = pCtx->Metadata_GetFilename();
		pMsg->m_nLineNumber = pCtx->Metadata_GetLineNumber();
	}
	else
	{
		pMsg->m_szFilename = "<unknown>";
		pMsg->m_nLineNumber = -1;
	}

	pMsg->m_Message.FormatV( msgFmt, args );
}

inline KvParseResult_t KV3ParseInt( KVParseContext* ctx, int* target, const KeyValues3* pKV )
{
	*target = pKV->GetValueInt();
	return KVPARSE_OK;
}
inline KvParseResult_t KV3ParseFloat( KVParseContext* ctx, float* target, const KeyValues3* pKV )
{
	*target = pKV->GetValueFloat();
	return KVPARSE_OK;
}
inline KvParseResult_t KV3ParseBool( KVParseContext* ctx, bool* target, const KeyValues3* pKV )
{
	*target = pKV->GetValueBool();
	return KVPARSE_OK;
}
KvParseResult_t KV3ParseString( KVParseContext* ctx, CUtlString* target, const KeyValues3* pKV )
{
	const char* str = pKV->GetValueString( nullptr );
	if ( !str )
	{
		*target = "";
		return ctx->ParseError( pKV, "Expected string" );
	}

	*target = str;
	return KVPARSE_OK;
}
KvParseResult_t KV3ParseRawString( KVParseContext* ctx, const char** target, const KeyValues3* pKV )
{
	const char* str = pKV->GetValueString( nullptr );
	if ( !str )
	{
		*target = "";
		return ctx->ParseError( pKV, "Expected string" );
	}

	*target = str;
	return KVPARSE_OK;
}

//////////////////////////////////////////////////////////////////////////
// vector parsing
template <typename T, typename ElementParser>
KvParseResult_t KV3ParseVector( KVParseContext* ctx, CUtlVector<T>* target, const KeyValues3* pKV, ElementParser&& elementParser, const char* szElementType, bool bAppend = false)
{
	if ( pKV->GetType() != KEYVALUES3_TYPE_ARRAY )
		return ctx->ParseError( pKV, "Expected array" );

	if ( !bAppend )
		target->RemoveAll();

	KvParseResult_t status = KVPARSE_OK;
	int count = pKV->GetArrayElementCount();
	for ( int iElement = 0; iElement < count; ++iElement )
	{
		T* subTarget = target->AddToTailGetPtr();
		if ( !elementParser( ctx, subTarget, pKV->GetArrayElement( iElement ) ) )
		{
			// remove new element that failed to parse
			target->RemoveMultipleFromTail( 1 );
			status = ctx->ParseErrorInfo( pKV, "... while parsing an array of %s (element %d)", szElementType, iElement );
			// continue parsing
		}
	}

	return status;
}

template <typename T, typename ElementParser>
struct KV3Parser_VectorImpl
{
	ElementParser mParser;
	bool mAppend;
	const char* mElementType;

	KV3Parser_VectorImpl( const ElementParser& elementParser, const char* szElementType, bool append )
		: mParser( elementParser )
		, mElementType(szElementType)
		, mAppend( append )
	{}

	KvParseResult_t operator()( KVParseContext* ctx, CUtlVector<T>* target, const KeyValues3* pKV )
	{
		return KV3ParseVector( ctx, target, pKV, mParser, mElementType, mAppend );
	}
};

template <typename T, typename ElementParser>
KV3Parser_VectorImpl<T, ElementParser> KV3Parser_Vector( const ElementParser& elementParser, const char* szElementType, bool append = false )
{
	return KV3Parser_VectorImpl<T, ElementParser>( elementParser, szElementType, append );
};


//////////////////////////////////////////////////////////////////////////
// dictionary as collection of named items

template <typename T, typename ElementParser>
KvParseResult_t KV3ParseDictVector( KVParseContext* ctx, CUtlVector<T>* target, const KeyValues3* pKV, ElementParser&& elementParser, const char* szElementType, bool bAppend = false )
{
	if ( pKV->GetType() != KEYVALUES3_TYPE_TABLE )
		return ctx->ParseError( pKV, "Expected table of %s", szElementType );

	if ( !bAppend )
		target->RemoveAll();

	KvParseResult_t status = KVPARSE_OK;
	int count = pKV->GetMemberCount();
	for ( int iElement = 0; iElement < count; ++iElement )
	{
		T* subTarget = target->AddToTailGetPtr();
		if ( !elementParser( ctx, subTarget, pKV->GetKV3MemberName(iElement), pKV->GetMember( iElement ) ) )
		{
			// remove new element that failed to parse
			target->RemoveMultipleFromTail( 1 );
			status = ctx->ParseErrorInfo( pKV, "... while parsing a table of %s (element %s)", szElementType, pKV->GetMemberName( iElement ) );
		}
	}

	return status;
}

template <typename T, typename ElementParser>
struct KV3Parser_DictToVectorImpl
{
	ElementParser mParser;
	const char* mElementType;
	bool mAppend;
	explicit KV3Parser_DictToVectorImpl( const ElementParser& elementParser, const char* szElementType, bool append )
		: mParser( elementParser )
		, mElementType( szElementType )
		, mAppend( append )
	{}

	KvParseResult_t operator()( KVParseContext* ctx, CUtlVector<T>* target, const KeyValues3* pKV )
	{
		return KV3ParseDictVector( ctx, target, pKV, mParser, mElementType, mAppend );
	}
};

template <typename T, typename ElementParser>
KV3Parser_DictToVectorImpl<T, ElementParser> KV3Parser_DictToVector( const ElementParser& elementParser, const char* szElementType, bool append = false )
{
	return KV3Parser_DictToVectorImpl<T, ElementParser>( elementParser, szElementType, append );
};


//////////////////////////////////////////////////////////////////////////
// struct decomposition into elements.
/////////////////////////////////////////////////////////////////////////
// single element


template <typename TElementPtr, typename TElementParser> struct KV3ItemRequired;
template <typename TElementPtr, typename TElementParser> struct KV3ItemWithDefault;

template <typename ItemInfo> struct KV3DictElementImpl;

template <typename TElement, typename TElementParser>
struct KV3DictElementImpl<KV3ItemRequired<TElement, TElementParser>>
{
	CKV3MemberName mName;
	TElement *mpElement;
	mutable TElementParser mParser;
	bool mRequired;

	KV3DictElementImpl( CKV3MemberName name, TElement* pElement, const TElementParser& parser, bool required )
		: mName( name )
		, mpElement( pElement )
		, mParser( parser )
		, mRequired( required )
	{}

	KvParseResult_t FillDefault() const
	{
		return mRequired ? KVPARSE_FAIL : KVPARSE_OK;
	}
};

template <typename TElement, typename TElementParser>
struct KV3DictElementImpl<KV3ItemWithDefault<TElement, TElementParser>>
{
	CKV3MemberName mName;
	TElement *mpElement;
	mutable TElementParser mParser;
	TElement mDefault;

	KV3DictElementImpl( CKV3MemberName name, TElement* pElement, const TElementParser& parser, const TElement& dflt )
		: mName( name )
		, mpElement( pElement )
		, mParser( parser )
		, mDefault( dflt )
	{}

	KvParseResult_t FillDefault() const
	{
		*mpElement = mDefault;
		return KVPARSE_OK;
	}
};


template <typename TElement, typename TElementParser>
auto MkKv3Element( CKV3MemberName name, TElement* mpElement, const TElementParser& parser ) -> KV3DictElementImpl<KV3ItemRequired<TElement, TElementParser>>
{
	return KV3DictElementImpl<KV3ItemRequired<TElement, TElementParser>>( name, mpElement, parser, true );
}

template <typename TElement, typename TElementParser>
auto MkKv3ElementOptional( CKV3MemberName name, TElement* mpElement, const TElementParser& parser ) -> KV3DictElementImpl<KV3ItemRequired<TElement, TElementParser>>
{
	return KV3DictElementImpl<KV3ItemRequired<TElement, TElementParser>>( name, mpElement, parser, false );
}

template <typename TElement, typename TElementParser, typename TDefaultElement>
auto MkKv3Element( CKV3MemberName name, TElement* mpElement, const TElementParser& parser, TDefaultElement&& dflt ) -> KV3DictElementImpl<KV3ItemWithDefault<TElement, TElementParser>>
{
	return KV3DictElementImpl<KV3ItemWithDefault<TElement, TElementParser>>( name, mpElement, parser, Forward<TDefaultElement>( dflt ) );
}

//////////////////////////////////////////////////////////////////////////
// collection of elements

template <typename... Elements>
struct KV3ParseDictElementsImpl {
};

// base case
template <>
struct KV3ParseDictElementsImpl<>
{
	static KvParseResult_t Parse( KVParseContext* ctx, const KeyValues3* pKV, const char* szStructType )
	{
		return KVPARSE_OK;
	}
};

// recursive case
template <typename Element, typename... Elements>
struct KV3ParseDictElementsImpl<Element, Elements...>
{
	static KvParseResult_t Parse( KVParseContext* ctx, const KeyValues3* pKV, const char* szStructType, const KV3DictElementImpl<Element>& element, const KV3DictElementImpl<Elements>&... elements )
	{
		KvParseResult_t status = KVPARSE_OK;
		const KeyValues3* pMember = pKV->FindMember( element.mName );
		if ( !pMember && !element.FillDefault() )
			status = ctx->ParseError( pKV, "Missing required element '%s' in %s", element.mName.m_pString, szStructType );

		if ( !element.mParser( ctx, element.mpElement, pMember ) )
			status = ctx->ParseErrorInfo( pKV, "... while parsing element '%s' in %s", element.mName.m_pString, szStructType );

		if ( !KV3ParseDictElementsImpl<Elements...>::Parse( ctx, pKV, szStructType, elements... ) )
			status = KVPARSE_FAIL;

		return status;
	}
};

template <typename... Elements>
KvParseResult_t KV3ParseDictElements( KVParseContext* ctx, const KeyValues3* pKV, const char* szStructType, const KV3DictElementImpl<Elements>&... elements )
{
	if ( pKV->GetType() != KEYVALUES3_TYPE_TABLE )
		return ctx->ParseError( pKV, "Expecting table containing %s", szStructType );

	return KV3ParseDictElementsImpl<Elements...>::Parse( ctx, pKV, szStructType, elements... );
}


const CBrConfig::LootList_t::ItemList_t* CBrConfig::LootList_t::RandomItemList() const
{
	int flRandomChance = ( m_flTotalChance > 0 ) ? RandomInt( 0, m_flTotalChance - 1 ) : 0;
	int flCurrentTotal = 0;
	FOR_EACH_VEC( m_ItemList, iItemList )
	{
		const CBrConfig::LootList_t::ItemList_t* pItemList = &m_ItemList[ iItemList ];
		flCurrentTotal += pItemList->m_flChance;
		if ( flCurrentTotal > flRandomChance )
			return pItemList;
	}

	return m_ItemList.Count() ? &m_ItemList.Head() : NULL;
}


CBrConfig::~CBrConfig()
{
	RemoveAllData();

	if ( mRawData )
		delete mRawData;
}


/*static*/ CBrConfig* CBrConfig::Load( const char* filename, IBrConfigLoadingHostContainer *pHostContainer )
{
	CKeyValues3Context *pKV3Context = new CKeyValues3Context;
	pKV3Context->SetMetadataEnabled( true );

	// Normally this work is done by ReadFile, but we are doing it here so we can get the full filename for use in metadata.
	const char* pPath = "MOD";
	char chFilenameBuf[MAX_UNICODE_PATH_IN_UTF8] = { 0 };
	const char* szResolvedName = g_pFullFileSystem->RelativePathToFullPath( filename, pPath, chFilenameBuf, sizeof( chFilenameBuf ) - 1 );
	if ( szResolvedName )
	{
		filename = szResolvedName;
		pPath = nullptr;
	}

	CUtlString errorMsg;
	if ( !LoadKV3FromFile( pKV3Context, &errorMsg, filename, pPath ) )
	{
		Msg( "%s", errorMsg.Access() );
		delete pKV3Context;
		return nullptr;
	}
	errorMsg.Clear();

	CBrConfig* pConfig = new CBrConfig;
	pConfig->m_pHostContainer = pHostContainer;
	pConfig->mRawData = pKV3Context;

	if ( !pConfig->Parse( pKV3Context->Root() ) )
	{
		delete pConfig;
		return nullptr;
	}

	return pConfig;
}

/*static*/ void CBrConfig::Free( CBrConfig* pConfig )
{
	if ( pConfig )
		delete pConfig;
}

const CBrConfig::BaseItem_t *CBrConfig::GetFirstMatchingBaseItemFromClassname( const char *pszClassname )
{
	// FIXME: is this safe?! What if these baseitem pointers change under us?

	int iIndex = m_dictItemsByClassname.Find( pszClassname );
	if ( iIndex != m_dictItemsByClassname.InvalidIndex() )
	{
		return m_dictItemsByClassname.Element( iIndex );
	}

	// fallback is linear string compare search! SLOW!

	for ( int i = m_dictItems.First(); m_dictItems.IsValidIndex( i ); i = m_dictItems.Next( i ) )
	{
		BaseItem_t* pBaseItem = m_dictItems.Element( i );
		if ( pBaseItem )
		{
			if ( !V_strcmp( pBaseItem->m_pszEntityName, pszClassname ) )
			{

				m_dictItemsByClassname.Insert( pszClassname, pBaseItem );

				return pBaseItem;
			}
		}
	}

	m_dictItemsByClassname.Insert( pszClassname, NULL ); // assume we'll never find it

	return NULL;
}

const CBrConfig::BaseItem_t *CBrConfig::GetBaseItem( const char *pszItemName ) const
{
	int iIndex = m_dictItems.Find( pszItemName );
	if ( iIndex != m_dictItems.InvalidIndex() )
	{
		return m_dictItems.Element( iIndex );
	}

	return NULL;
}


const CBrConfig::LootList_t *CBrConfig::GetLootList( const char *pszLootListName ) const
{
	int iIndex = m_dictLootLists.Find( pszLootListName );
	if ( iIndex != m_dictLootLists.InvalidIndex() )
	{
		return m_dictLootLists.Element( iIndex );
	}

	return NULL;
}


const CBrConfig::Crate_t *CBrConfig::GetCrate( const char *pszCrateName ) const
{
	int iIndex = m_dictCrates.Find( pszCrateName );
	if ( iIndex != m_dictCrates.InvalidIndex() )
	{
		return m_dictCrates.Element( iIndex );
	}

	return NULL;
}


const CBrConfig::Event_t *CBrConfig::GetEvent( const char *pszEventName ) const
{
	int iIndex = m_dictEvents.Find( pszEventName );
	if ( iIndex != m_dictEvents.InvalidIndex() )
	{
		return m_dictEvents.Element( iIndex );
	}

	return NULL;
}


void CBrConfig::RemoveAllData()
{
	m_dictItems.PurgeAndDeleteElements();
	m_dictLootLists.PurgeAndDeleteElements();
	m_dictCrates.PurgeAndDeleteElements();
	m_dictEvents.PurgeAndDeleteElements();
	m_GameStartItems.Purge();
	m_vecPrecacheModelNames.RemoveAll();
	m_dictItemsByClassname.Purge(); // not delete because anything it points to was deleted from m_dictItems
}

bool CBrConfig::Parse( const KeyValues3* pRootKV )
{
	const KeyValues3* pItemsKV = pRootKV->FindMember( "items" );
	if ( !ParseItems( pItemsKV ) )
		return false;

	const KeyValues3* pLootListsKV = pRootKV->FindMember( "lootlists" );
	if ( !ParseLootLists( pLootListsKV ) )
		return false;

	const KeyValues3* pCratesKV = pRootKV->FindMember( "crates" );
	if ( !ParseCrates( pCratesKV ) )
		return false;

	const KeyValues3* pEventsKV = pRootKV->FindMember( "events" );
	if ( !ParseEvents( pEventsKV ) )
		return false;

	const KeyValues3* pGameStartItemsKV = pRootKV->FindMember( "gamestart" );
	if ( !ParseGameStartItems( pGameStartItemsKV ) )
		return false;

	return true;
}

bool CBrConfig::ParseItems( const KeyValues3 *pItemsKV )
{
	if ( !pItemsKV || pItemsKV->GetType() != KEYVALUES3_TYPE_ARRAY )
	{
		Warning( "Expecting array 'items'" );
		return false;
	}

	int count = pItemsKV->GetArrayElementCount();
	for ( int iElement = 0; iElement < count; ++iElement )
	{
		const KeyValues3 *pItemKV = pItemsKV->GetArrayElement( iElement );

		const char *pszName = pItemKV->GetMemberString( "name", nullptr );
		Assert( pszName );

		if ( m_dictItems.Find( pszName ) != m_dictItems.InvalidIndex() )
		{
			Warning( "Trying to add redundant item name '%s'", pszName );
		}
		else
		{
			BaseItem_t *pNewItem = new BaseItem_t;
			pNewItem->m_pszEntityName = pItemKV->GetMemberString( "entity", nullptr );
			Assert( pNewItem->m_pszEntityName );
			pNewItem->m_nDefaultAmmo = pItemKV->GetMemberInt( "default_ammo", -1 );
			pNewItem->m_nSecurityDoorValue = pItemKV->GetMemberInt( "security_door_value", 0 );

			extern ConVar sv_dz_cash_bundle_size;
			pNewItem->m_nSecurityDoorValue *= sv_dz_cash_bundle_size.GetInt();

			Vector vecTemp;
			pItemKV->GetMemberVector( "highlight_color", &vecTemp );
			pNewItem->m_cHighlightColor.SetColor( vecTemp.x, vecTemp.y, vecTemp.z, 255 );

			m_dictItems.Insert( pszName, pNewItem );
		}
	}

	return true;
}

bool CBrConfig::ParseLootLists( const KeyValues3 *pLootListsKV )
{
	if ( !pLootListsKV || pLootListsKV->GetType() != KEYVALUES3_TYPE_TABLE )
	{
		Warning( "Expecting table 'lootlists'\n" );
		return false;
	}

	int iNumLootList = pLootListsKV->GetMemberCount();
	for ( int iLootList = 0; iLootList < iNumLootList; ++iLootList )
	{
		const KeyValues3 *pLootListKV = pLootListsKV->GetMember( iLootList );
		if ( pLootListKV->GetType() != KEYVALUES3_TYPE_ARRAY )
		{
			Warning( "Expecting array 'chance'\n" );
			return false;
		}

		const char *pszLootListName = pLootListsKV->GetMemberName( iLootList );
		if ( !IsValidContent( pszLootListName, "lootlist" ) )
		{
			return false;
		}

		int iNumChance = pLootListKV->GetArrayElementCount();
		
		//
		// Some lootlists can decide to select only one possible item at load time
		//
		bool bSelectOnlyOne = ( iNumChance > 1 ) &&
			pLootListKV->GetArrayElement( 0 ) &&
			pLootListKV->GetArrayElement( 0 )->GetMemberInt( "select_only_one" );

		char const *szSelectAction = bSelectOnlyOne ? pLootListKV->GetArrayElement( 0 )->GetMemberString( "select_action" ) : "";
		char const *szNewListName = StringAfterPrefix( szSelectAction, "move_one_to_new_list:" );
		bool bKeepOnlyOne = !V_stricmp( "keep_only_one", szSelectAction );
		if ( bSelectOnlyOne && !szNewListName && !bKeepOnlyOne )
		{
			Warning( "Invalid 'select_only_one' configuration for lootlist '%s' with select_action = '%s'\n", pszLootListName, szSelectAction );
			return false;
		}
		
		int iLoadStartIndex = 0, iLoadEndIndex = iNumChance;
		int iSelectOnlyOneIndexDecided = 0;
		if ( bSelectOnlyOne )
		{
			// Roll the chance to select which only one item to keep
			iLoadStartIndex = 1;
			int flRandomCollectionTotalChance = 0;
			for ( int iChance = iLoadStartIndex; iChance < iLoadEndIndex; ++iChance )
			{
				const KeyValues3 *pChanceKV = pLootListKV->GetArrayElement( iChance );
				int flChance = pChanceKV->GetMemberInt( "chance", 0 );
				flRandomCollectionTotalChance += flChance;
			}
			if ( flRandomCollectionTotalChance > 0 )
			{
				int flRandomChance = RandomInt( 0, flRandomCollectionTotalChance - 1 );
				int flCurrentRandomRunningTotal = 0;
				for ( int iChance = iLoadStartIndex; iChance < iLoadEndIndex; ++iChance )
				{
					const KeyValues3 *pChanceKV = pLootListKV->GetArrayElement( iChance );
					int flChance = pChanceKV->GetMemberInt( "chance", 0 );
					flCurrentRandomRunningTotal += flChance;
					if ( flCurrentRandomRunningTotal > flRandomChance )
					{
						iLoadStartIndex = iChance;
						break;
					}
				}
			}
			else
			{
				iLoadStartIndex = RandomInt( 1, iNumChance - 1 );
			}
			iLoadEndIndex = iLoadStartIndex + 1;

			//
			// Remember which index we selected
			//
			iSelectOnlyOneIndexDecided = iLoadStartIndex;
			if ( int nDecisionType = pLootListKV->GetArrayElement( 0 )->GetMemberInt( "decision_type" ) )
			{
				int nDecisionValue = pLootListKV->GetArrayElement( iSelectOnlyOneIndexDecided )->GetMemberInt( "decision_value" );
				if ( m_pHostContainer )
					m_pHostContainer->OnConfigRandomDecisionMade( nDecisionType, nDecisionValue );
			}
			
			// Prepare for loading
			if ( szNewListName )
			{	// We'll branch off the selected item into its own lootlist later, load everything for now
				iLoadStartIndex = 1;
				iLoadEndIndex = iNumChance;
			}
		}

		// Now load the entries as decided by the configuration pre-parse
		LootList_t *pNewLootList = new LootList_t;
		int flTotalChance = 0.f;
		int nMaxSecurityDoorValue = 0;
		pNewLootList->m_ItemList.EnsureCount( iLoadEndIndex - iLoadStartIndex );
		Assert( pNewLootList->m_ItemList.Count() == iLoadEndIndex - iLoadStartIndex );
		for ( int iChance = iLoadStartIndex; iChance < iLoadEndIndex; ++iChance )
		{
			// add new itemlist to this lootlist
			LootList_t::ItemList_t *pItemList = &pNewLootList->m_ItemList[ iChance - iLoadStartIndex ];

			const KeyValues3 *pChanceKV = pLootListKV->GetArrayElement( iChance );
			pItemList->m_flChance = pChanceKV->GetMemberInt( "chance", 0 );
			Assert( pItemList->m_flChance > 0 );

			pItemList->m_nMaxSecurityDoorValue = 0;

			flTotalChance += pItemList->m_flChance;
			
			const KeyValues3 *pItemsKV = pChanceKV->FindMember( "items" );
			if ( !pItemsKV || pItemsKV->GetType() != KEYVALUES3_TYPE_ARRAY )
			{
				Warning( "Expecting array 'items'" );
				return false;
			}

			int iNumItem = pItemsKV->GetArrayElementCount();
			pItemList->m_Items.EnsureCount( iNumItem );
			Assert( pItemList->m_Items.Count() == iNumItem );
			for ( int iItem = 0; iItem < iNumItem; ++iItem )
			{
				const KeyValues3 *pItemKV = pItemsKV->GetArrayElement( iItem );
				const char *pszItemName = pItemKV->GetMemberString( "name", nullptr );
				Assert( pszItemName );
				int nOverrideAmmo = pItemKV->GetMemberInt( "override_ammo", -1 );

				const BaseItem_t *pBaseItem = GetBaseItem( pszItemName );
				if ( !pBaseItem )
				{
					Warning( "Can't find '%s' from base 'items' list", pszItemName );
					return false;
				}

				// add new item to this itemlist
				LootList_t::ItemList_t::Item_t *pItem = &pItemList->m_Items[ iItem ];
				pItem->m_pBaseItem = pBaseItem;
				pItem->m_nOverrideAmmo = nOverrideAmmo;
				
				pItemList->m_nMaxSecurityDoorValue = MAX( pItemList->m_nMaxSecurityDoorValue, pBaseItem->m_nSecurityDoorValue );
				nMaxSecurityDoorValue = MAX( nMaxSecurityDoorValue, pBaseItem->m_nSecurityDoorValue );
			}
		}

		pNewLootList->m_flTotalChance = flTotalChance;
		pNewLootList->m_nMaxSecurityDoorValue = nMaxSecurityDoorValue;

		m_dictLootLists.Insert( pszLootListName, pNewLootList );

		//
		// Post processing the select_only_one rules
		//
		if ( bSelectOnlyOne && szNewListName )
		{
			if ( !IsValidContent( szNewListName, "lootlist" ) )
				return false;

			LootList_t::ItemList_t & itemSelected1 = pNewLootList->m_ItemList[ iSelectOnlyOneIndexDecided - 1 ]; // only take the 1 item we selected;

			LootList_t *pSplit1LootList = new LootList_t;
			pSplit1LootList->m_ItemList.EnsureCount( 1 );
			pSplit1LootList->m_ItemList.Head() = itemSelected1;
			
			pNewLootList->m_flTotalChance -= itemSelected1.m_flChance;
			pSplit1LootList->m_flTotalChance = itemSelected1.m_flChance;
			itemSelected1.m_flChance = 0; // make sure this item doesn't get selected in the original list
			
			pSplit1LootList->m_nMaxSecurityDoorValue = itemSelected1.m_nMaxSecurityDoorValue;

			m_dictLootLists.Insert( szNewListName, pSplit1LootList );
		}
	}

	return true;
}

bool CBrConfig::ParseCrates( const KeyValues3 *pCratesKV )
{
	if ( !pCratesKV || pCratesKV->GetType() != KEYVALUES3_TYPE_ARRAY )
	{
		Warning( "Expecting array 'crates'" );
		return false;
	}

	int iNumCrate = pCratesKV->GetArrayElementCount();
	for ( int iCrate = 0; iCrate < iNumCrate; ++iCrate )
	{
		const KeyValues3 *pCrateKV = pCratesKV->GetArrayElement( iCrate );
		const char *pszCrateName = pCrateKV->GetMemberString( "name", nullptr );
		Assert( pszCrateName );

		const char *pszLootListName = pCrateKV->GetMemberString( "lootlist", nullptr );
		Assert( pszLootListName );

		const LootList_t *pLootList = GetLootList( pszLootListName );
		if ( !pLootList )
		{
			Warning( "Can't find lootlist '%s'", pszLootListName );
			return false;
		}

		const char *pszModelName = pCrateKV->GetMemberString( "model", nullptr );
		Assert( pszModelName );

		m_vecPrecacheModelNames.AddToTail( pszModelName );

		const char *pszEntityName = pCrateKV->GetMemberString( "entity", nullptr );
		Assert( pszEntityName );

		Crate_t *pNewCrate = new Crate_t;
		pNewCrate->m_pszName = pszCrateName;
		pNewCrate->m_pLootList = pLootList;
		pNewCrate->m_pszModel = pszModelName;
		pNewCrate->m_pszEntityName = pszEntityName;

		Vector vecTemp;
		pCrateKV->GetMemberVector( "highlight_color", &vecTemp );
		pNewCrate->m_cHighlightColor.SetColor( vecTemp.x, vecTemp.y, vecTemp.z, 255 );

		m_dictCrates.Insert( pszCrateName, pNewCrate );
	}

	return true;
}

bool CBrConfig::ParseEvents( const KeyValues3 *pEventsKV )
{
	if ( !pEventsKV || pEventsKV->GetType() != KEYVALUES3_TYPE_ARRAY )
	{
		Warning( "Expecting array 'events'" );
		return false;
	}

	int iNumEvent = pEventsKV->GetArrayElementCount();
	for ( int iEvent = 0; iEvent < iNumEvent; ++iEvent )
	{
		const KeyValues3 *pEventKV = pEventsKV->GetArrayElement( iEvent );
		const char *pszEventName = pEventKV->GetMemberString( "name", nullptr );
		Assert( pszEventName );
		const char *pszContentName = pEventKV->GetMemberString( "content", nullptr );

		Event_t *pNewEvent = new Event_t;
		if ( !GetContent( &pNewEvent->m_content, pszContentName ) )
			return false;

		m_dictEvents.Insert( pszEventName, pNewEvent );
	}

	return true;
}

int SortGameStartItems( const CBrConfig::GameStartItem_t *pItemA, const CBrConfig::GameStartItem_t *pItemB )
{
	return pItemB->m_nPriority < pItemA->m_nPriority;
}

bool CBrConfig::ParseGameStartItems( const KeyValues3 *pGameStartItemsKV )
{
	if ( !pGameStartItemsKV || pGameStartItemsKV->GetType() != KEYVALUES3_TYPE_ARRAY )
	{
		Warning( "Expecting array 'gamestart'" );
		return false;
	}

	int iNumItem = pGameStartItemsKV->GetArrayElementCount();
	m_GameStartItems.EnsureCount( iNumItem );
	for ( int iItem = 0; iItem < iNumItem; ++iItem )
	{
		const KeyValues3 *pItemKV = pGameStartItemsKV->GetArrayElement( iItem );
		
		GameStartItem_t *pNewItem = &m_GameStartItems[ iItem ];

		const char *pszContentName = pItemKV->GetMemberString( "content", nullptr );
		if ( !GetContent( &pNewItem->m_content, pszContentName ) )
			return false;

		pNewItem->m_pszName = pItemKV->GetMemberString( "name", nullptr );
		pNewItem->m_nQuantity = pItemKV->GetMemberInt( "quantity" );
		pNewItem->m_flWeightRadius = pItemKV->GetMemberFloat( "weight_radius" );
		pNewItem->m_nPriority = pItemKV->GetMemberInt( "priority" );
	}

	m_GameStartItems.Sort( SortGameStartItems );

	return true;
}

bool CBrConfig::GetContent( CBrConfig::Content_t *pContent, const char *pszContentName ) const
{
	Assert( pszContentName );
	pContent->m_pLootList = GetLootList( pszContentName );
	pContent->m_pCrate = GetCrate( pszContentName );

	AssertMsg1( pContent->m_pLootList || pContent->m_pCrate, "Missing content %s", pszContentName );

	return true;
}

bool CBrConfig::IsValidContent( const char *pszContentName, const char *pszContext )
{
	if ( m_dictLootLists.HasElement( pszContentName ) )
	{
		Warning( "Trying to add %s '%s' but it's already existed in 'lootlists'", pszContext, pszContentName );
		return false;
	}

	if ( m_dictCrates.HasElement( pszContentName ) )
	{
		Warning( "Trying to add %s '%s' but it's already existed in 'crates'", pszContext, pszContentName );
		return false;
	}

	return true;
}


//const CBrConfig::SpawnRule * CBrConfig::FindSpawnRule( const char * szSpawnRuleName )
//{
//	// find a spawn rule by name
//
//	FOR_EACH_VEC( mSpawnRules, nSpawnRule )
//	{
//		const CBrConfig::SpawnRule *pSpawnRule = &mSpawnRules.Element( nSpawnRule );
//
//		if ( pSpawnRule && !V_strcmp( pSpawnRule->name, szSpawnRuleName ) )
//		{
//			return pSpawnRule;
//		}
//	}
//
//	return nullptr;
//}
