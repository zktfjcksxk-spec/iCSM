//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CS_WEAPON_PARSE_H
#define CS_WEAPON_PARSE_H
#ifdef _WIN32
#pragma once
#endif

#include "cs_weapon_id.h"

#include "weapon_parse.h"

bool WeaponIsBallistic( CSWeaponType wepType );


#if USE_WEAPON_DATA_CACHE
class CCSWeaponData;
typedef CCSWeaponData CCSWeaponInfo;
#else
class CCSWeaponInfo;
#endif

//--------------------------------------------------------------------------------------------------------
// Utility information functions
//--------------------------------------------------------------------------------------------------------
bool IsGunWeapon( CSWeaponType weaponType );

const CCSWeaponInfo* GetWeaponInfoFromItem( const CEconItemView* pItemView );
const CCSWeaponInfo* GetWeaponInfoFromItem( const CEconItemDefinition* pItemDef );
CSWeaponID GetWeaponIDFromItem( const CEconItemView* pItemView );
CSWeaponID GetWeaponIDFromItem( const CEconItemDefinition* pItemDef );
int Helper_GetWeaponAccuracy( const CCSWeaponInfo* pWeaponInfo, const CEconItemView * pItem );
float GetEffectiveRangeRawValue( const CEconItemView *pItem );
float GetWeaponInaccuracyValue( const CEconItemView *pItem );
float GetWeaponSpreadValue( const CEconItemView *pItem );

#endif // CS_WEAPON_PARSE_H
