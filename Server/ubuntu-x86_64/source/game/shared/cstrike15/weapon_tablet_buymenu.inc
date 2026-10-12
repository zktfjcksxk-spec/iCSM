static TabletBuyMenuEntry s_TabletBuyMenu[] =	// keep uniq numbers unique and only add new numbers, keep obsolete entries and make their command to be empty string
// Last used uniq value = 17 for drone_megapistol_purchase
{	// uniq		name					price	command (set to empty for obsolete entries)			icons									upgr.		category							loadout position
	{ 1, "#TabletPistol",				10,		"drone_pistol_purchase",				"defidx-img-decision-type=1",						-1,0,0,		TabletBuyMenuEntry::k_EWeapons,		LOADOUT_POSITION_INVALID },
	{ 2, "#TabletSMG",					25,		"drone_weapon_purchase",				"defidx-img-decision-type=2",						-1,0,0,		TabletBuyMenuEntry::k_EWeapons,		LOADOUT_POSITION_INVALID },
	{ 17, "#TabletPistolPremium",		35,		"drone_megapistol_purchase",			"defidx-img-decision-type=5",						-1,5,1,		TabletBuyMenuEntry::k_EWeapons,		LOADOUT_POSITION_INVALID },
	{ 3, "#TabletRifle",				55,		"drone_rifle_purchase",					"defidx-img-decision-type=3",						-1,0,0,		TabletBuyMenuEntry::k_EWeapons,		LOADOUT_POSITION_INVALID },
	{ 4, "#TabletSSG08",				60,		"drone_ssg08_purchase",					"icons/equipment/ssg08.svg",						-1,0,0,		TabletBuyMenuEntry::k_EWeapons,		LOADOUT_POSITION_INVALID }, // LOADOUT_POSITION_RIFLE2
	{ 17, "#TabletPistolPremium",		75,		"drone_megapistol_purchase",			"defidx-img-decision-type=5",						-1,5,64,	TabletBuyMenuEntry::k_EWeapons,		LOADOUT_POSITION_INVALID },

	{ 0, "#TabletKnife",				5,		"drone_knife_purchase",					"icons/equipment/knife_t.svg",						-1,0,0,		TabletBuyMenuEntry::k_EArmor,		LOADOUT_POSITION_INVALID }, // LOADOUT_POSITION_MELEE
	{ 6, "#TabletArmorHelmet",			10,		"drone_armor_purchase",					"icons/equipment/armor_helmet.svg",					-1,0,0,		TabletBuyMenuEntry::k_EArmor,		LOADOUT_POSITION_INVALID },
	{ 7, "#TabletAmmo",					20,		"drone_ammo_purchase",					"icons/equipment/ammobox.svg",						-1,0,0,		TabletBuyMenuEntry::k_EArmor,		LOADOUT_POSITION_INVALID },
	{ 12, "#TabletMedical",				20,		"drone_healthshot_purchase",			"icons/equipment/healthshot.svg",					-1,0,0,		TabletBuyMenuEntry::k_EArmor,		LOADOUT_POSITION_INVALID },
	
	{ 10, "#TabletUtility",				5,		"drone_utility_purchase",				"icons/equipment/grenadepack.svg",					-1,0,0,		TabletBuyMenuEntry::k_EUtility,		LOADOUT_POSITION_INVALID },
	{ 16, "#TabletDmgNades",			15,		"drone_dmgnades_purchase",				"icons/equipment/grenadepack2.svg",					-1,0,0,		TabletBuyMenuEntry::k_EUtility,		LOADOUT_POSITION_INVALID },
	{ 11, "#TabletJammer",				35,		"drone_jammer_purchase",				"icons/equipment/radarjammer.svg",					-1,0,0,		TabletBuyMenuEntry::k_EUtility,		LOADOUT_POSITION_INVALID },
	
	{ 13, "#TabletUpgradeDroneIntel",	20,		"drone_drone_upgrade_purchase",			"survival/buymenu/survival_buymenu_drone.png",		1,0,0,		TabletBuyMenuEntry::k_ETablet,		LOADOUT_POSITION_INVALID },
	{ 14, "#TabletUpgradeZoneIntel",	35,		"drone_zone_upgrade_purchase",			"survival/buymenu/survival_buymenu_zone.png",		2,0,0,		TabletBuyMenuEntry::k_ETablet,		LOADOUT_POSITION_INVALID },
	{ 15, "#TabletUpgradeHighres",		85,		"drone_highres_upgrade_purchase",		"survival/buymenu/survival_buymenu_hires.png",		0,0,0,		TabletBuyMenuEntry::k_ETablet,		LOADOUT_POSITION_INVALID },
};