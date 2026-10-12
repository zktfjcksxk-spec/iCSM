//========= Copyright © 2017, Valve Corporation, All rights reserved. ============//
//
// Purpose: Load rules data for Survival mode
//
//=============================================================================//

#ifndef BR_CONFIG_H
#define BR_CONFIG_H
#ifdef _WIN32
#pragma once
#endif

class KeyValues3;
class CKeyValues3Context;

class IBrConfigLoadingHostContainer
{
public:
	virtual void OnConfigRandomDecisionMade( int nType, int nValue ) {}
};

class CBrConfig
{
public:
	CBrConfig() = default;
	static CBrConfig* Load( const char* filename, IBrConfigLoadingHostContainer *pHostContainer );
	static void Free( CBrConfig* pConfig );
	~CBrConfig();

	struct BaseItem_t
	{
		const char *m_pszEntityName;
		int m_nDefaultAmmo;
		int m_nSecurityDoorValue;
		Color m_cHighlightColor;
	};

	struct LootList_t
	{
		struct ItemList_t
		{
			int	m_flChance;
			int m_nMaxSecurityDoorValue;

			struct Item_t
			{
				const BaseItem_t *m_pBaseItem;
				int m_nOverrideAmmo;

				Item_t()
				{
					m_pBaseItem = NULL;
					m_nOverrideAmmo = -1;
				}

				int GetAmmo() const { return m_nOverrideAmmo != -1 ? m_nOverrideAmmo : m_pBaseItem->m_nDefaultAmmo; }
			};
			CUtlVector< Item_t > m_Items;
		};

		const ItemList_t *RandomItemList() const;

		CUtlVector< ItemList_t > m_ItemList;
		int m_flTotalChance;
		int m_nMaxSecurityDoorValue;
	};

	struct Crate_t
	{
		const char *m_pszName;
		const LootList_t *m_pLootList;
		const char *m_pszModel;
		const char *m_pszEntityName;
		Color m_cHighlightColor;
	};

	// content should only have lootlist or crate (not both)
	struct Content_t
	{
		int GetPrice() const { return m_pLootList ? m_pLootList->m_nMaxSecurityDoorValue : m_pCrate->m_pLootList->m_nMaxSecurityDoorValue; }
		const LootList_t *GetLootList() const { return m_pLootList ? m_pLootList : m_pCrate->m_pLootList; }
		bool IsCrate() const { return m_pCrate != nullptr; }
		const LootList_t *m_pLootList;
		const Crate_t *m_pCrate;
	};

	struct Event_t
	{
		Content_t m_content;
	};

	struct GameStartItem_t
	{
		const char *m_pszName;
		Content_t m_content;
		int m_nQuantity;
		float m_flWeightRadius;
		int m_nPriority;
	};

	const BaseItem_t *GetFirstMatchingBaseItemFromClassname( const char *pszClassname );

	const BaseItem_t *GetBaseItem( const char *pszItemName ) const;
	const LootList_t *GetLootList( const char *pszLootListName ) const;
	const Crate_t *GetCrate( const char *pszCrateName ) const;
	const Event_t *GetEvent( const char *pszEventName ) const;
	bool GetContent( Content_t *pContent, const char *pszContentName ) const;

	const CUtlVector< GameStartItem_t >& GetGameStartItems() const { return m_GameStartItems; }

	const CUtlVector< const char* >& GetPrecacheModelNames() const { return m_vecPrecacheModelNames; }

private:
	IBrConfigLoadingHostContainer *m_pHostContainer; // reference to the host of the config
	CKeyValues3Context* mRawData;	// used to keep strings alive

	void RemoveAllData();
	bool Parse( const KeyValues3 *pRootKV );
	bool ParseItems( const KeyValues3 *pItemsKV );
	bool ParseLootLists( const KeyValues3 *pLootListsKV );
	bool ParseCrates( const KeyValues3 *pCratesKV );
	bool ParseEvents( const KeyValues3 *pEventsKV );
	bool ParseGameStartItems( const KeyValues3 *pGameStartItemsKV );
	bool IsValidContent( const char *pszContentName, const char *pszContext );

	CUtlDict< BaseItem_t* >			m_dictItems;
	CUtlDict< LootList_t* >			m_dictLootLists;
	CUtlDict< Crate_t* >			m_dictCrates;
	CUtlDict< Event_t* >			m_dictEvents;
	CUtlVector< GameStartItem_t >	m_GameStartItems;
	CUtlVector< const char* >		m_vecPrecacheModelNames;
	CUtlDict< BaseItem_t* >			m_dictItemsByClassname;

	// no copy
	CBrConfig( const CBrConfig& );
	CBrConfig& operator=( const CBrConfig& );

	friend class KVParser_BRConfig; // for loading
};



#endif // BR_CONFIG_H
