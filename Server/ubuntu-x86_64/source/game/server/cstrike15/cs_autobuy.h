//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Headers and defines for Autobuy and Rebuy 
//
//=============================================================================//

/**
 * Weapon classes as used by the AutoBuy
 * Has to be different that the previous ones because these are bitmasked values as a weapon can be from
 * more than one class.  This also includes all the classes of equipment that a player can buy.
 */
enum AutoBuyClassType
{
	AUTOBUYCLASS_PRIMARY = 1,
	AUTOBUYCLASS_SECONDARY = 2,
	//AUTOBUYCLASS_AMMO = 4,
	AUTOBUYCLASS_ARMOR = 8,
	AUTOBUYCLASS_DEFUSER = 16,
	AUTOBUYCLASS_PISTOL = 32,
	AUTOBUYCLASS_SMG = 64,
	AUTOBUYCLASS_RIFLE = 128,
	AUTOBUYCLASS_SNIPERRIFLE = 256,
	AUTOBUYCLASS_SHOTGUN = 512,
	AUTOBUYCLASS_MACHINEGUN = 1024,
	AUTOBUYCLASS_GRENADE = 2048,
	AUTOBUYCLASS_NIGHTVISION = 4096,
	AUTOBUYCLASS_SHIELD = 8192,
};

struct AutoBuyInfoStruct
{
	AutoBuyClassType m_class;
	loadout_positions_t m_LoadoutPosition;
	char *m_command;
	//char *m_classname;
};

class RebuyStruct
{
public:
	enum RebuyEquipType {
		kRebuyEquip_Primary = 0,
		kRebuyEquip_Secondary,
		kRebuyEquip_Taser,
		kRebuyEquip_Armor,
		kRebuyEquip_Defuser,
		kRebuyEquip_Nvgs,
		kRebuyEquipCount
	};

	static const int kRebuyGrenadeCount = 8;

	RebuyStruct()
	{
		Clear();
	}

	void Clear()
	{
		for ( int i = 0; i < kRebuyEquipCount; ++i )
			m_equipment[i] = LOADOUT_POSITION_INVALID;

		for ( int i = 0; i < kRebuyGrenadeCount; ++i )
			m_grenades[i] = LOADOUT_POSITION_INVALID;

		m_isNotEmpty = false;
	}

	bool isEmpty( void )
	{
		return !m_isNotEmpty;
	}

	void SetEquipment( int equipId, loadout_positions_t loadoutSlot )
	{
		if ( equipId < 0 || equipId >= kRebuyEquipCount )
			return;

		m_equipment[equipId] = loadoutSlot;
		m_isNotEmpty = true;
	}

	loadout_positions_t GetEquipment( int equipId )
	{
		if ( equipId < 0 || equipId >= kRebuyEquipCount )
			return LOADOUT_POSITION_INVALID;

		return m_equipment[equipId];
	}

	void SetGrenade( int index, loadout_positions_t grenade )
	{
		if ( index < 0 || index >= kRebuyGrenadeCount )
			return;

		m_grenades[index] = grenade;
		m_isNotEmpty = true;
	}

	loadout_positions_t GetGrenade( int index )
	{
		if ( index < 0 || index >= kRebuyGrenadeCount )
			return LOADOUT_POSITION_INVALID;

		return m_grenades[index];
	}

private:
	loadout_positions_t m_equipment[kRebuyEquipCount];
	loadout_positions_t	m_grenades[kRebuyGrenadeCount];

	bool				m_isNotEmpty;					
};

extern AutoBuyInfoStruct g_autoBuyInfo[];
