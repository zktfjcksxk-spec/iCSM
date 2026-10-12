//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
//=============================================================================//

#include "cbase.h"
#include "br_items.h"

#ifndef CLIENT_DLL

#include "cs_gamerules.h"
#include "cs_player.h"
#include "weapon_csbase.h"
#include "weapon_tablet.h"
#include "dangerzone_controller.h"
#include "br_config.h"
#include "cvisibilitymonitor.h"
#include "item_cash.h"
#include "weapon_fists.h"
#include "particle_parse.h"		// for DispatchParticleEffect

#else

#include "c_physicsprop.h"
#include "c_physbox.h"
#include "c_props.h"
#include "c_cs_player.h"

//#define CPhysBox C_PhysBox
//#define CPhysicsProp C_PhysicsProp

#endif

#ifdef GAME_DLL

DEVELOPMENT_ONLY_CONVAR( dev_dz_ammobox_dispensing_rate, 0.5 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_ammobox_dispensing_rate_boosted, 0.33 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_moneycrate_dispensing_rate, 0.7 );

DEVELOPMENT_ONLY_CONVAR( dev_dz_crate_water_damping_factor, 4 );

DEVELOPMENT_ONLY_CONVAR( dev_dz_jammer_duration, 150 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_jammer_health, 60 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_paradrop_beep_think, 1.6 );

extern ConVar sv_dz_contractkill_reward;

#endif // GAME_DLL

// --------------------------------------------------------------
// --------------------------------------------------------------

#ifdef CLIENT_DLL

IMPLEMENT_CLIENTCLASS_DT( C_PhysPropLootCrate, DT_PhysPropLootCrate, CPhysPropLootCrate )
RecvPropBool( RECVINFO( m_bRenderInPSPM ) ),
RecvPropBool( RECVINFO( m_bRenderInTablet ) ),
RecvPropInt( RECVINFO( m_iHealth ) ),
RecvPropInt( RECVINFO( m_iMaxHealth ) ),
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT( C_PhysPropAmmoBox, DT_PhysPropAmmoBox, CPhysPropAmmoBox )
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT( C_PhysPropWeaponUpgrade, DT_PhysPropWeaponUpgrade, CPhysPropWeaponUpgrade )
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT( C_PhysPropRadarJammer, DT_PhysPropRadarJammer, CPhysPropRadarJammer )
END_RECV_TABLE()

IMPLEMENT_AUTO_LIST( ITabletRenderedLootCrate );
#endif

IMPLEMENT_AUTO_LIST( IPhysPropRadarJammer );

// --------------------------------------------------------------
// --------------------------------------------------------------

#ifdef CLIENT_DLL
C_PhysPropLootCrate::C_PhysPropLootCrate()
	: C_PhysicsPropMultiplayer()
	, ITabletRenderedLootCrate( false ) // only add crates to list that draw in tablet
	, m_bRenderInPSPM( false )
	, m_bRenderInTablet( false )
	, m_iMaxHealth( 0 )
{
}

void C_PhysPropLootCrate::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetAllowFastPath( false ); // otherwise weapondecal materials on crates don't render?!

		if ( m_bRenderInTablet )
			ITabletRenderedLootCrate::Add( this );
	}

	// draw after the skybox but before the main scene in a special pass
	if ( m_bRenderInPSPM != IsRenderingPostSkyboxPreMainScene() )
	{
		RenderPostSkyboxPreMainScene( m_bRenderInPSPM );
	}

}
#endif

#ifndef CLIENT_DLL


LINK_ENTITY_TO_CLASS( prop_loot_crate, CPhysPropLootCrate );

IMPLEMENT_SERVERCLASS_ST( CPhysPropLootCrate, DT_PhysPropLootCrate )
SendPropBool( SENDINFO( m_bRenderInPSPM ) ),
SendPropBool( SENDINFO( m_bRenderInTablet ) ),
SendPropInt( SENDINFO( m_iHealth ), 10 ),
SendPropInt( SENDINFO( m_iMaxHealth ), 10 ),
END_SEND_TABLE()

CPhysPropLootCrate::CPhysPropLootCrate()
{
	m_pszCrateName = NULL;
	m_bOwnedByPlayer = false;
	m_bRenderInPSPM = false;
	m_bTakeDamageFromDangerZone = false;

	m_flDampingOriginalSpeed = -1;
	m_flDampingOriginalRot = -1;
}

CPhysPropLootCrate::~CPhysPropLootCrate()
{
}

void CPhysPropLootCrate::OnEntityEvent( EntityEvent_t event, void *pEventData )
{
	BaseClass::OnEntityEvent( event, pEventData );

	// apply custom extra damping when crates are in water. FIXME: shouldn't this be a model property?

	if ( !VPhysicsGetObject() )
		return;

	if ( m_flDampingOriginalSpeed < 0 || m_flDampingOriginalRot < 0 )
	{
		VPhysicsGetObject()->GetDamping( &m_flDampingOriginalSpeed, &m_flDampingOriginalRot );
		Assert( m_flDampingOriginalSpeed >= 0 && m_flDampingOriginalRot >= 0 );
	}

	if ( event == ENTITY_EVENT_WATER_TOUCH )
	{
		//VPhysicsGetObject()->SetBuoyancyRatio( 0.09f );

		float flWaterDampingSpeed = MAX( m_flDampingOriginalSpeed * dev_dz_crate_water_damping_factor.GetFloat(), dev_dz_crate_water_damping_factor.GetFloat() );
		float flWaterDampingRot = MAX( m_flDampingOriginalRot * dev_dz_crate_water_damping_factor.GetFloat(), dev_dz_crate_water_damping_factor.GetFloat() );

		VPhysicsGetObject()->SetDamping( &flWaterDampingSpeed, &flWaterDampingRot );
	}
	else if ( event == ENTITY_EVENT_WATER_UNTOUCH )
	{
		VPhysicsGetObject()->SetDamping( &m_flDampingOriginalSpeed, &m_flDampingOriginalRot );
	}

}

void CPhysPropLootCrate::Precache()
{
	// precache a bunch of hardcoded stuff

	PrecacheModel( "models/gibs/metal_gib1.mdl" );
	PrecacheModel( "models/gibs/metal_gib2.mdl" );
	PrecacheModel( "models/gibs/metal_gib3.mdl" );
	PrecacheModel( "models/gibs/metal_gib4.mdl" );
	PrecacheModel( "models/gibs/metal_gib5.mdl" );

	PrecacheScriptSound( "Wood_Furniture.Break" );
	PrecacheScriptSound( "Breakable.Metal" );
	PrecacheScriptSound( "Survival.DestroyMoneyBag" );
	PrecacheScriptSound( "Survival.BriefcaseUnlocking" );
	PrecacheScriptSound( "Survival.BriefcaseUnlockSuccess" );
	PrecacheScriptSound( "Survival.ParachuteEquipping" );
	PrecacheScriptSound( "Survival.ParachuteEquipped" );
	PrecacheScriptSound( "Survival.UpgradeTabletStart" );
	PrecacheScriptSound( "Survival.UpgradeTabletSuccess" );
	PrecacheScriptSound( "Survival.ArmorPickup" );
	PrecacheScriptSound( "Survival.ParadropBeacon" );


	bool bFoundSurvivalConfigModel = false;

	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	if ( pBRrules && pBRrules->GetConfig() )
	{
		FOR_EACH_VEC( pBRrules->GetConfig()->GetPrecacheModelNames(), i )
		{
			const char* szModelName = pBRrules->GetConfig()->GetPrecacheModelNames()[i];
			Assert( szModelName );
			
			int nModelIndex = PrecacheModel( szModelName );
			Assert( nModelIndex != -1 );

			if ( nModelIndex != -1 )
			{
				bFoundSurvivalConfigModel = true;
				PrecacheGibsForModel( nModelIndex );
			}
		}
	}

	if ( pBRrules )
		Assert( bFoundSurvivalConfigModel );

	BaseClass::Precache();
}


bool CPhysPropLootCrate::SetupCrate( const char *pszCrateName )
{
	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	if ( pBRrules )
	{
		const CBrConfig::Crate_t *pCrate = pBRrules->GetConfig()->GetCrate( pszCrateName );
		if ( pCrate )
		{
			m_pszCrateName = pCrate->m_pszName;

			m_bEligibleForScreenHighlight = true;

			SetModel( pCrate->m_pszModel );
			return true;
		}

		return false;
	}

	return false;
}


bool OnVisibleGameEventCallBack( CBaseEntity *pProxy, CBasePlayer *pViewingPlayer )
{
	CPhysPropLootCrate *pProxyPtr = assert_cast < CPhysPropLootCrate * >( pProxy );

	if ( !pProxyPtr )
		return true;

	IGameEvent * event = gameeventmanager->CreateEvent( "loot_crate_visible" );
	if ( event )
	{
		event->SetInt( "userid", pViewingPlayer->GetUserID() );
		event->SetInt( "subject", pProxyPtr->entindex() );
		event->SetString( "type", pProxyPtr->GetTypeName() );
		gameeventmanager->FireEvent( event );
	}

	return false;
}


DEVELOPMENT_ONLY_CONVAR( dev_dz_paradrop_damping, 7 );
void CPhysPropLootCrate::Spawn( void )
{
	BaseClass::Spawn();

	m_flSpawnTime = gpGlobals->curtime;

	VisibilityMonitor_AddEntity( this, 600.0f, &OnVisibleGameEventCallBack, NULL );

	SetMaxHealth( GetHealth() );

	SetCollisionGroup( COLLISION_GROUP_NONE );

	SetPhysicsMode( PHYSICS_MULTIPLAYER_SOLID );

	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin() + Vector(0,0,20), GetAbsOrigin() - Vector(0,0,20), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
	if ( tr.DidHit() )
	{
		Vector vecFloor = tr.endpos;
		vecFloor.z += 10;
		QAngle temp = GetAbsAngles();
		Teleport( &vecFloor, &temp, &vec3_origin );
	}

}

float CPhysPropLootCrate::GetDmgModBullet( void )
{
	return 1.0f;
}
float CPhysPropLootCrate::GetDmgModClub( void )
{
	return 1.0f;
}
float CPhysPropLootCrate::GetDmgModExplosive( void )
{
	return 1.0f;
}
float CPhysPropLootCrate::GetDmgModFire( void )
{
	return 1.0f;
}

extern ConVar sv_fistpunch_damage_hard;
int CPhysPropLootCrate::OnTakeDamage( const CTakeDamageInfo &info )
{
	// Allow certain boxes (paradrops) to die in the danger zone
	bool bDmgFromWorldAllowed = false;
	if ( m_bTakeDamageFromDangerZone && info.GetAttacker() && info.GetAttacker() == GetDangerZoneController() )
	{
		bDmgFromWorldAllowed = true;
	}

	if ( !bDmgFromWorldAllowed && info.GetAttacker() && info.GetAttacker()->IsWorld() )
	{
		// Ignore world damage, so the crates don't shatter when they slide off trees/rocks/funny-shaped stuff they initially land on.

		// However this also prevents drones from lifting up and dropping crates to smash them, which might have been fun...
		return 0;
	}
	
	CTakeDamageInfo dmgInfoLocal = info; // local copy for modify

	CBasePlayer *pAttackingPlayer = ToBasePlayer( dmgInfoLocal.GetAttacker() );

	bool bDamageIsFromThrownMelee = false;
	CBaseEntity *pInflictor = dmgInfoLocal.GetInflictor();
	if ( pInflictor && pInflictor->ClassMatches( "weapon_melee" ) )
	{
		CWeaponCSBase *pWep = dynamic_cast<CWeaponCSBase*>(pInflictor);
		if ( pWep )
		{
			if ( !pAttackingPlayer && pWep->GetPreviousOwner() )
			{
				dmgInfoLocal.SetDamageType( DMG_SLASH ); // override the attack type
				pAttackingPlayer = ToBasePlayer( pWep->GetPreviousOwner() ); // override the attacker as the last holder of this weapon
				bDamageIsFromThrownMelee = true;

				// hack to stop crates from flying too far when hit by thrown weapons. FIXME: when/if to restore original damping??
				IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
				if ( pPhysicsObject )
				{
					float flDamp = 10.0f;
					pPhysicsObject->SetDamping( &flDamp, &flDamp );
				}
			}
		}
	}

	CrateType_t crateType = GetCrateType();

	if ( crateType == METAL_CRATE )
	{
		if ( dmgInfoLocal.GetDamageType() & DMG_CLUB )
		{
			// punching metal crates does nothing
			IGameEvent *event = gameeventmanager->CreateEvent( "open_crate_instr" );
			if ( event )
			{
				event->SetInt( "userid", pAttackingPlayer->GetUserID() );
				event->SetInt( "subject", entindex() );
				event->SetString( "type", GetTypeName() );
				gameeventmanager->FireEvent( event );
			}

			return 0;
		}
		else if ( dmgInfoLocal.GetDamageType() & DMG_BLAST )
		{
			float flDamage = dmgInfoLocal.GetDamage();

			// increase explosive damage against metal crates
			dmgInfoLocal.SetDamage( flDamage * 3.0f );
			dmgInfoLocal.SetMaxDamage( flDamage * 3.0f );
		}

	}

	if ( pAttackingPlayer && (dmgInfoLocal.GetDamageType() & DMG_CLUB) ) // attacking crate with fists
	{
		CCSPlayer *pPlayer = ToCSPlayer( pAttackingPlayer );
		if ( pPlayer )
		{
			float flDamagePortion = (float)GetMaxHealth() / 3.0f;

			bool bIsHardPunch = dmgInfoLocal.GetDamage() >= sv_fistpunch_damage_hard.GetFloat();
			if ( bIsHardPunch )
			{
				dmgInfoLocal.SetDamage( flDamagePortion * 2.0f );
				dmgInfoLocal.SetMaxDamage( flDamagePortion * 2.0f );
			}
			else
			{
				dmgInfoLocal.SetDamage( flDamagePortion );
				dmgInfoLocal.SetMaxDamage( flDamagePortion );
			}
		}
	}
	else if ( pAttackingPlayer && (dmgInfoLocal.GetDamageType() & DMG_SLASH) ) // attacking crate with a tool like a knife or axe
	{
		CCSPlayer *pPlayer = ToCSPlayer( pAttackingPlayer );
		if ( pPlayer )
		{
			if ( bDamageIsFromThrownMelee && (crateType == METAL_CRATE || crateType == PARADROP_CRATE) )
			{
				// hitting metal crates or paradrops with a THROWN tool hits it for 75% dmg
				float flDamagePortion = (float)GetMaxHealth() * 0.75f;
				dmgInfoLocal.SetDamage( flDamagePortion );
				dmgInfoLocal.SetMaxDamage( flDamagePortion );
			}
			else if ( crateType == METAL_CRATE )
			{
				// attacking metal crates with a tool takes three hits
				float flDamagePortion = (float)GetMaxHealth() / 3.0f;
				dmgInfoLocal.SetDamage( flDamagePortion );
				dmgInfoLocal.SetMaxDamage( flDamagePortion );
			}
			else
			{
				// attacking non-metal crates with a tool (thrown or not) is an instant pop
				dmgInfoLocal.SetDamage( GetHealth() );
				dmgInfoLocal.SetMaxDamage( GetHealth() );
			}
		}
	}

	float flProjectedHealth = GetHealth() - dmgInfoLocal.GetDamage();
	if ( flProjectedHealth > 0 && flProjectedHealth < 2 )
	{
		dmgInfoLocal.SetDamage( GetHealth() * 2.0f ); // hack, don't leave crates with slivers of health
	}

	if ( pAttackingPlayer && VPhysicsGetObject() )
	{
		Vector vecVelToAdd = 0.1f * Vector( RandomFloat( -100, 100 ), RandomFloat( -100, 100 ), RandomFloat( 100, 200 ) );
		VPhysicsGetObject()->AddVelocity( &vecVelToAdd, NULL );
	}

	float flRatio_old = (float)GetHealth() / (float)GetMaxHealth();

	CFmtStr fmtTypeName;
	FormatTypeNameForLogging( fmtTypeName );
	CCSPlayer::UtilLogPrintfTakeDamageLine( dmgInfoLocal, this, fmtTypeName );

	int nRet = BaseClass::OnTakeDamage( dmgInfoLocal );

	float flRatio_new = (float)GetHealth() / (float)GetMaxHealth();

	// darken the model
	//color24 temp = GetRenderColor();
	//temp.r = (flRatio_new * 85.0f) + 170;
	//temp.g = (flRatio_new * 85.0f) + 170;
	//temp.b = (flRatio_new * 85.0f) + 170;
	//SetRenderColor( temp.r, temp.g, temp.b );

	m_nSkin = 1; // swap to skin that supports damage proxy

	if ( flRatio_old > 0.7f && flRatio_new <= 0.7f )
	{
		if ( ( crateType == METAL_CRATE ) || ( crateType == PARADROP_CRATE ) )
		{
			EmitSound( "Land_MetalVehicle.StepLeft" );
		}
		else
		{
			EmitSound( "Land_Cardboard.StepLeft" );
		}
	}

	if ( GetHealth() && dmgInfoLocal.GetAttacker() && dmgInfoLocal.GetAttacker()->IsPlayer() )
	{
		CSingleUserRecipientFilter filter( ToBasePlayer( dmgInfoLocal.GetAttacker() ) );
		filter.MakeReliable();

		CCSUsrMsg_UpdateScreenHealthBar msg;
		msg.set_entidx( entindex() );
		msg.set_healthratio_old( flRatio_old );
		msg.set_healthratio_new( flRatio_new );
		msg.set_style( 0 ); // green/red health style
		SendUserMessage( filter, CS_UM_UpdateScreenHealthBar, msg );
	}

	return nRet;
}

void CPhysPropLootCrate::FormatTypeNameForLogging( CFmtStr &fmt )
{
	fmt.Format( "%s{%s%s}", GetClassname(), GetCrateType() == METAL_CRATE ? "metal_" : "", GetCrateName() );
}

void CPhysPropLootCrate::Use( CBaseEntity * pActivator, CBaseEntity * pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator || !pActivator->IsPlayer() )
		return;

	// punching metal crates does nothing
	IGameEvent *event = gameeventmanager->CreateEvent( "open_crate_instr" );
	if ( event )
	{
		event->SetInt( "userid", ToBasePlayer( pActivator )->GetUserID() );
		event->SetInt( "subject", entindex() );
		event->SetString( "type", GetTypeName() );
		gameeventmanager->FireEvent( event );
	}

	CCSPlayer *pCSPlayer = ToCSPlayer( pActivator );
	if ( pCSPlayer )
	{
		CWeaponCSBase *pWeapon = ( CWeaponCSBase * )pCSPlayer->GetActiveWeapon();

		// only switch if player doesn't already have a melee as active weapon
		// if metal crate, try to switch to better melee anyway
		if ( !pWeapon || !pWeapon->IsMeleeWeapon() || GetCrateType() == METAL_CRATE )
		{
			bool bSwitched = false;
			CBaseCombatWeapon* pKnife = pCSPlayer->Weapon_OwnsThisType( "weapon_knife" );
			if ( pKnife )
			{
				bSwitched = pCSPlayer->Weapon_Switch( pKnife );
			}

			if ( !bSwitched )
			{
				CBaseCombatWeapon* pMelee = pCSPlayer->Weapon_OwnsThisType( "weapon_melee" );
				if ( pMelee )
				{
					bSwitched = pCSPlayer->Weapon_Switch( pMelee );
				}
			}

			if ( !bSwitched && GetCrateType() != METAL_CRATE )
			{
				CBaseCombatWeapon* pFists = pCSPlayer->Weapon_OwnsThisType( "weapon_fists" );
				if ( pFists )
				{
					bSwitched = pCSPlayer->Weapon_Switch( pFists );
				}
			}
		}
	}
}

void CPhysPropLootCrate::OnBreak( const Vector &vecVelocity, const AngularImpulse &angVel, CBaseEntity *pBreaker )
{
	if ( GetCrateType() == MONEY_CRATE )
	{
		EmitSound( "Survival.DestroyMoneyBag" );
	}
	else if ( GetCrateType() != METAL_CRATE )
	{
		EmitSound( "Breakable.Metal" );
	}
	else
	{
		EmitSound( "Wood_Furniture.Break" );
	}

	SetRenderColor( 255, 255, 255 );

	CCSPlayer *pPlayer = pBreaker->IsPlayer() ? ToCSPlayer( pBreaker ) : NULL;
	SpawnCrateItems( pPlayer );
	if ( pPlayer )
	{
		extern ConVar contributionscore_crate_break;
		pPlayer->AddContributionScore( contributionscore_crate_break.GetInt() );
		IGameEvent * event = gameeventmanager->CreateEvent( "loot_crate_opened" );
		if ( event )
		{
			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetString( "type", GetTypeName() );
			event->SetInt( "priority", 5 );
			gameeventmanager->FireEvent( event );
		}

		pPlayer->m_nWorldCratesOpened++;
	}
}

void CPhysPropLootCrate::GetSpawnPositionForItemNumber( int nItem, int nTotalItemCount, Vector &posOut, QAngle &angOut )
{
	posOut = GetAbsOrigin();
	angOut = GetAbsAngles();

	if ( nTotalItemCount <= 0 )
		return;

	matrix3x4_t matAttach1;
	matrix3x4_t matAttach2;

	if ( !GetAttachment( LookupAttachment( "item_spawn_min" ), matAttach1 ) || !GetAttachment( LookupAttachment( "item_spawn_max" ), matAttach2 ) )
	{
		return;
	}

	if ( GetCrateType() == PARADROP_CRATE )
	{
		VMatrix matTemp = VMatrix( matAttach1.GetLeft(), matAttach1.GetUp(), matAttach1.GetForward() );
		MatrixAngles( matTemp.As3x4(), angOut ); // align long guns vertically out of paradrop crates
	}
	else if ( GetCrateType() == METAL_CRATE )
	{
		// fine to use abs angles for these
	}
	else // we always want to spawn items right-way up from other crates
	{
		angOut[PITCH] = 0.0f;
		angOut[ROLL] = 0.0f;
	}

	if ( nItem == 0 )
	{
		posOut = Lerp( 0.5f, matAttach1.GetOrigin(), matAttach2.GetOrigin() );
	}
	else if ( nItem == 1 )
	{
		posOut = matAttach1.GetOrigin();
	}
	else if ( nItem == 2 )
	{
		posOut = matAttach2.GetOrigin();
	}
	else
	{
		float flRatio = clamp( ((float)nItem) / ((float)nTotalItemCount + 1), 0, 1 );
		posOut = Lerp( flRatio, matAttach1.GetOrigin(), matAttach2.GetOrigin() );
	}
}

void CPhysPropLootCrate::SpawnCrateItems( CCSPlayer *pBreaker )
{
	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	if ( pBRrules )
	{
		const CBrConfig::Crate_t *pCrate = pBRrules->GetConfig()->GetCrate( m_pszCrateName );
		if ( pCrate )
		{
			CUtlVector< CBaseEntity * > vecItems;
			pBRrules->SpawnItemFromLootList( GetAbsOrigin(), GetAbsAngles(), pCrate->m_pLootList, pBreaker, &vecItems, this );

			// pass down original source to spawned br items
			if ( !m_strOriginalSource.IsEmpty() )
			{
				FOR_EACH_VEC( vecItems, i )
				{
					CBrBaseItem *pBrItem = dynamic_cast< CBrBaseItem* >( vecItems[i] );
					if ( pBrItem )
					{
						pBrItem->SetOriginalSource( m_strOriginalSource.Get() );
					}
				}
			}

			// mark the weapons to not be automatically "bumped" by players
			extern ConVar mp_weapon_next_owner_touch_time;
			FOR_EACH_VEC( vecItems, i )
			{
				if ( CWeaponCSBase *pWeaponItem = dynamic_cast< CWeaponCSBase * >( vecItems[ i ] ) )
				{
					pWeaponItem->SetNextOwnerTouchTime( gpGlobals->curtime + mp_weapon_next_owner_touch_time.GetFloat(), true );
				}
			}
		}
	}
}


// --------------------------------------------------------------
// --------------------------------------------------------------

LINK_ENTITY_TO_CLASS( prop_metal_crate, CPhysPropMetalCrate );

LINK_ENTITY_TO_CLASS( prop_money_crate, CPhysPropMoneyCrate );

CPhysPropMoneyCrate::CPhysPropMoneyCrate()
{
	m_nCurrentCashCount = m_nCashCount = 0;
	m_flSpawnTime = 0.f;
}

void CPhysPropMoneyCrate::Spawn()
{
	BaseClass::Spawn();

	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	if ( pBRrules )
	{
		const CBrConfig::Crate_t *pCrate = pBRrules->GetConfig()->GetCrate( GetCrateName() );
		if ( pCrate )
		{
			const CBrConfig::LootList_t::ItemList_t* pItemList = pCrate->m_pLootList->RandomItemList();
			Assert( pItemList );
			if ( pItemList )
			{
				Assert( pItemList->m_Items.Count() == 1 );
				m_nCurrentCashCount = m_nCashCount = pItemList->m_Items[0].GetAmmo();
			}
		}
	}

	SetHealth( 50 );
	SetMaxHealth( 50 );
	m_takedamage = DAMAGE_YES;
}

void CPhysPropMoneyCrate::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator || !pActivator->IsPlayer() )
		return;

	if ( (gpGlobals->curtime - m_flTimeLastUsed) < dev_dz_moneycrate_dispensing_rate.GetFloat() )
		return;

	if ( m_nCurrentCashCount == 0 )
		return;

	m_flTimeLastUsed = gpGlobals->curtime;

	CCSPlayer *pPlayer = ToCSPlayer( pActivator );

	if ( pPlayer )
	{
		CBaseCombatWeapon* pFistsWep = pPlayer->Weapon_OwnsThisType( "weapon_fists" );
		CFists* pFists = dynamic_cast<CFists*>(pFistsWep);
		if ( pFists )
		{
			pFists->PlayUninterruptableActivity( ACT_VM_PICKUP );
		}
	}

	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	if ( pPlayer && pBRrules )
	{
		UTIL_SpawnPhysicalCash( pPlayer->GetAbsOrigin(), 0, 1, "lootbag", CASH_SPAWN_AT_POS_INSTANT_PICKUP );

		// float flRatio_old = (float)m_nCurrentCashCount / (float)(m_nCashCount);

		m_nCurrentCashCount--;
		EmitSound( "Survival.GrabMoneyFromBag" );

		// float flRatio_new = (float)m_nCurrentCashCount / (float)(m_nCashCount);

		// increase 'deflated' bodygroup
		{
			SetBodygroup( 0, clamp( GetBodygroup( 0 ) + 1, 0, 4 ) );
		}

		if ( !m_nCurrentCashCount )
		{

			CDynamicProp *pDuffleFadeOut = dynamic_cast<CDynamicProp *>(CreateEntityByName( "dynamic_prop" ));
			if ( pDuffleFadeOut )
			{
				pDuffleFadeOut->SetAbsAngles( GetAbsAngles() );
				pDuffleFadeOut->SetAbsOrigin( GetAbsOrigin() );
				pDuffleFadeOut->KeyValue( "model", STRING( GetModelName() ) );
				pDuffleFadeOut->Spawn();
				pDuffleFadeOut->SetBodygroup( 0, GetBodygroup( 0 ) );
				pDuffleFadeOut->SetCollisionGroup( COLLISION_GROUP_NONE );
				pDuffleFadeOut->AddSolidFlags( FSOLID_NOT_SOLID );
				pDuffleFadeOut->SUB_StartFadeOut( 0.2f );
			}

			UTIL_Remove( this );
		}
		//else
		//{
		//	CSingleUserRecipientFilter filter( pPlayer );
		//	filter.MakeReliable();
		//
		//	CCSUsrMsg_UpdateScreenHealthBar msg;
		//	msg.set_entidx( entindex() );
		//	msg.set_healthratio_old( flRatio_old );
		//	msg.set_healthratio_new( flRatio_new );
		//	msg.set_style( 2 ); // money style
		//	SendUserMessage( filter, CS_UM_UpdateScreenHealthBar, msg );
		//}
	}
}

void CPhysPropMoneyCrate::SpawnCrateItems( CCSPlayer *pBreaker )
{
	if ( m_nCurrentCashCount > 0 )
	{
		UTIL_SpawnPhysicalCash( GetAbsOrigin(), 0, m_nCurrentCashCount, "lootbag" );
	}

	CDynamicProp *pDuffleFadeOut = dynamic_cast<CDynamicProp *>(CreateEntityByName( "dynamic_prop" ));
	if ( pDuffleFadeOut )
	{
		pDuffleFadeOut->SetAbsAngles( GetAbsAngles() );
		pDuffleFadeOut->SetAbsOrigin( GetAbsOrigin() );
		pDuffleFadeOut->KeyValue( "model", STRING( GetModelName() ) );
		pDuffleFadeOut->Spawn();
		pDuffleFadeOut->SetBodygroup( 0, GetBodygroup( 0 ) );
		pDuffleFadeOut->SetCollisionGroup( COLLISION_GROUP_NONE );
		pDuffleFadeOut->AddSolidFlags( FSOLID_NOT_SOLID );
		pDuffleFadeOut->SUB_StartFadeOut( 0.2f );
	}
}

LINK_ENTITY_TO_CLASS( prop_paradrop_crate, CPhysPropParadropCrate );

BEGIN_DATADESC( CPhysPropParadropCrate )
DEFINE_THINKFUNC( CrateThink )
END_DATADESC()

CPhysPropParadropCrate::CPhysPropParadropCrate()
{
	m_bFalling = true;
	m_nNumThinksAtZeroVerticalVelocity = 0;
}


void CPhysPropParadropCrate::Spawn( void )
{
	BaseClass::Spawn();

	SetSequence( 0 );
	SetPlaybackRate( 1.0f );
	ResetSequenceInfo(); // hack - shouldn't need to do this

	if ( VPhysicsGetObject() && m_bFalling )
	{
		float flDamp = dev_dz_paradrop_damping.GetFloat();
		float angDrag = dev_dz_paradrop_damping.GetFloat() * 0.5f;
		VPhysicsGetObject()->SetDamping( &flDamp, &flDamp );
		VPhysicsGetObject()->SetDragCoefficient( &flDamp, &angDrag );

		// start with non-zero downward velocity
		Vector vecDown = Vector( 0, 0, -1 );
		VPhysicsGetObject()->AddVelocity( &vecDown, NULL );

		// Schedule beeps
		SetContextThink( &CPhysPropParadropCrate::BeepThink, gpGlobals->curtime, "BeepThink" );
	}

#ifdef _DEBUG
	DevMsg( "PARADROP: Phys crate %p spawned at %.1f @ (%.1f, %.1f, %.1f)\n", this, gpGlobals->curtime, XYZ( GetAbsOrigin() ) );
#endif


	SetThink( &CPhysPropParadropCrate::CrateThink );
	SetNextThink( gpGlobals->curtime );

	IGameEvent *event = gameeventmanager->CreateEvent( "survival_paradrop_spawn" );
	if ( event )
	{
		event->SetInt( "entityid", entindex() );
		gameeventmanager->FireEvent( event );
	}
}


void CPhysPropParadropCrate::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
	BaseClass::VPhysicsCollision( index, pEvent );

	IPhysicsObject *pPhysObj = pEvent->pObjects[!index];

	if ( m_bFalling )
	{
		CBaseEntity *pOther = static_cast<CBaseEntity *>(pPhysObj->GetGameData());

		// Don't allow the player to bump an object active if we've requested not to
		if ( pOther && VPhysicsGetObject() )
		{
			// clear the drag and damping on paradrop crates
			if ( pOther->IsWorld() )
			{
				RemoveChuteFromParadrop();
			}
		}
	}
}


bool CPhysPropParadropCrate::SetupCrate( const char *pszCrateName )
{
	bool bResult = BaseClass::SetupCrate( pszCrateName );
	if ( bResult )
	{
		m_bRenderInPSPM = true;
		m_bRenderInTablet = true;
		m_bEligibleForScreenHighlight = false;
	}

	return bResult;
}


int CPhysPropParadropCrate::UpdateTransmitState()
{
	// falling paradrop crates need to always transmit
	if ( m_bFalling || m_bRenderInTablet )
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	return BaseClass::UpdateTransmitState();
}

int CPhysPropParadropCrate::OnTakeDamage( const CTakeDamageInfo &info )
{
	if ( m_bFalling )
		return 0;

	if ( info.GetAttacker() && info.GetAttacker() == GetDangerZoneController() )
	{
		m_bTakeDamageFromDangerZone = true;
		CTakeDamageInfo info2 = info;
		info2.SetDamage( GetHealth() * 2 ); // paradrop crates shatter in the danger zone
		return BaseClass::OnTakeDamage( info2 );
	}

	if ( (info.GetDamageType() & DMG_BLAST) ) // paradrop crates are super weak to explosives
	{
		CTakeDamageInfo info2 = info;
		info2.SetDamage( GetHealth() * 2 ); // always break
		return BaseClass::OnTakeDamage( info2 );
	}

	return BaseClass::OnTakeDamage( info );
}

void CPhysPropParadropCrate::SetOwnedByPlayer( bool bOwnedByPlayer )
{
	BaseClass::SetOwnedByPlayer( bOwnedByPlayer );
	m_bRenderInTablet = !IsOwnedByPlayer();
}

void CPhysPropParadropCrate::OnBreak( const Vector &vecVelocity, const AngularImpulse &angVel, CBaseEntity *pBreaker )
{
	SetContextThink( &CPhysPropParadropCrate::BeepThink, TICK_NEVER_THINK, "BeepThink" );
	m_bRenderInTablet = false;
	BaseClass::OnBreak( vecVelocity, angVel, pBreaker );

	IGameEvent *event = gameeventmanager->CreateEvent( "survival_paradrop_break" );
	if ( event )
	{
		event->SetInt( "entityid", entindex() );
		gameeventmanager->FireEvent( event );
	}
}

void CPhysPropParadropCrate::BeepThink()
{
	if ( IsOwnedByPlayer() )
		return;	// crates summoned by players do not beep, do not shatter in danger zone, and don't show up on tablet

	EmitSound( "Survival.ParadropBeacon" ); // meep
	SetContextThink( &CPhysPropParadropCrate::BeepThink, gpGlobals->curtime + dev_dz_paradrop_beep_think.GetFloat(), "BeepThink" );

	if ( !m_bFalling && m_bRenderInTablet )
	{
		if ( CDangerZoneController *pDangerZone = GetDangerZoneController() )
		{
			if ( !pDangerZone->IsWithinPlayArea( GetAbsOrigin() ) )
			{	// We are swarmed by the danger zone - time to stop rendering in the tablet
				/*
				// WAS: shatter in the zone
				CTakeDamageInfo dmgDangerZone( pDangerZone, pDangerZone, GetHealth() * 2, DMG_BURN );
				TakeDamage( dmgDangerZone );
				*/
				// Now we just stop rendering in the tablet, but continue beeping
				m_bRenderInTablet = false;
			}
		}
	}
}

void CPhysPropParadropCrate::CrateThink()
{
	if ( m_bFalling )
	{
		IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
		if ( pPhysicsObject )
		{
			Vector vecVel;
			pPhysicsObject->GetVelocity( &vecVel, NULL );

			if ( vecVel.z >= 0 && vecVel.z < 1.0f )
			{
				m_nNumThinksAtZeroVerticalVelocity++;

				if ( m_nNumThinksAtZeroVerticalVelocity > 10 )
					RemoveChuteFromParadrop();
			}
			else
			{
				CDangerZoneController *pZone = GetDangerZoneController();
				if ( pZone && !pZone->IsWithinPlayArea( GetAbsOrigin() ) )
				{
					Vector vecToZoneOrigin = (pZone->GetEndGameZoneOrigin() - GetAbsOrigin()).Normalized();
					vecToZoneOrigin *= 8.0f;
					pPhysicsObject->AddVelocity( &vecToZoneOrigin, NULL );
				}
			}
		}
	}

	StudioFrameAdvance();
	SetNextThink( gpGlobals->curtime );
}


void CPhysPropParadropCrate::RemoveChuteFromParadrop( void )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if ( pPhysicsObject )
	{
		if ( (gpGlobals->curtime - m_flSpawnTime) < 3.0f )
			return; // give paradrops a chance to gather some fall speed

		if ( m_bFalling )
		{
			float flDamp = 0.0f;
			float angDrag = 0.0f;
			pPhysicsObject->SetDamping( &flDamp, &flDamp );
			pPhysicsObject->SetDragCoefficient( &flDamp, &angDrag );

#ifdef _DEBUG
			DevMsg( "PARADROP: Phys crate %p finished falling at %.1f @ (%.1f, %.1f, %.1f)\n", this, gpGlobals->curtime, XYZ( GetAbsOrigin() ) );
#endif
		}

		m_bFalling = false;

		m_bRenderInPSPM = false;

		m_nNumThinksAtZeroVerticalVelocity = 0;
		
		static int nChuteFadeSeq = LookupSequence( "chute_fade" );
		if ( nChuteFadeSeq != -1 && GetSequence() != 1 )
		{
			SetSequence( nChuteFadeSeq ); // hide parachute
			SetCycle( 0 );
			ResetSequenceInfo(); // hack - shouldn't need to do this
		}
	}
}

// --------------------------------------------------------------
// --------------------------------------------------------------

LINK_ENTITY_TO_CLASS( radar_jammer, CPhysPropRadarJammer );

BEGIN_DATADESC( CPhysPropRadarJammer )
DEFINE_THINKFUNC( JammerThink )
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CPhysPropRadarJammer, DT_PhysPropRadarJammer )
END_SEND_TABLE()

#define MODEL_JAMMER "models/props_survival/jammer/jammer.mdl"

const char *sJammerGibs[] = {
	"models/props_survival/jammer/jammer_gib01.mdl",
	"models/props_survival/jammer/jammer_gib02.mdl",
	"models/props_survival/jammer/jammer_gib03.mdl",
	"models/props_survival/jammer/jammer_gib04.mdl",
	"models/props_survival/jammer/jammer_gib05.mdl",
	"models/props_survival/jammer/jammer_gib06.mdl"
};

CPhysPropRadarJammer::CPhysPropRadarJammer()
{
	m_flSpawnTime = gpGlobals->curtime;
	m_flLastSoundTime = gpGlobals->curtime;
}

void CPhysPropRadarJammer::Precache()
{
	PrecacheModel( MODEL_JAMMER );
	SetModelName( MAKE_STRING( MODEL_JAMMER ) );
	BaseClass::Precache();

	for ( int i = 0; i < ARRAYSIZE( sJammerGibs ); i++ )
	{
		PrecacheModel( sJammerGibs[i] );
	}
}

void CPhysPropRadarJammer::Spawn( void )
{
	SetModel( MODEL_JAMMER );

	BaseClass::Spawn();

	SetMaxHealth( dev_dz_jammer_health.GetInt() );
	SetHealth( dev_dz_jammer_health.GetInt() );

	m_takedamage = DAMAGE_YES;

	SetCollisionGroup( COLLISION_GROUP_PUSHAWAY );

	SetThink( &CPhysPropRadarJammer::JammerThink );
	SetNextThink( gpGlobals->curtime );

	m_bEligibleForScreenHighlight = false;
}

int CPhysPropRadarJammer::OnTakeDamage( const CTakeDamageInfo &info )
{
	if ( info.GetInflictor() && info.GetInflictor()->IsWorld() )
		return 0;

	float flRatio_old = (float)GetHealth() / (float)GetMaxHealth();

	int nRet = BaseClass::OnTakeDamage( info );

	float flRatio_new = (float)GetHealth() / (float)GetMaxHealth();

	if ( GetHealth() && info.GetAttacker() && info.GetAttacker()->IsPlayer() )
	{
		CSingleUserRecipientFilter filter( ToBasePlayer( info.GetAttacker() ) );
		filter.MakeReliable();

		CCSUsrMsg_UpdateScreenHealthBar msg;
		msg.set_entidx( entindex() );
		msg.set_healthratio_old( flRatio_old );
		msg.set_healthratio_new( flRatio_new );
		msg.set_style( 0 ); // green/red health style
		SendUserMessage( filter, CS_UM_UpdateScreenHealthBar, msg );
	}

	return nRet;
}

void CPhysPropRadarJammer::OnBreak( const Vector &vecVelocity, const AngularImpulse &angVel, CBaseEntity *pBreaker )
{
	JammerDie();
}

void CPhysPropRadarJammer::JammerThink( void )
{
	float flTimeAlive = gpGlobals->curtime - m_flSpawnTime;

	if ( flTimeAlive > dev_dz_jammer_duration.GetFloat() )
	{
		JammerDie();
		return;
	}

	float flDesiredSoundInterval = RemapValClamped( flTimeAlive, 0.0f, dev_dz_jammer_duration.GetFloat(), 3.0f, 0.2f );

	float flTimeSinceLastSound = gpGlobals->curtime - m_flLastSoundTime;
	if ( flTimeSinceLastSound > flDesiredSoundInterval )
	{
		EmitSound( "Sensor.WarmupBeep" ); // meep
		m_flLastSoundTime = gpGlobals->curtime;
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CPhysPropRadarJammer::JammerDie( void )
{
	EmitSound( "ambient.electrical_zap_7" );
	DispatchParticleEffect( "explosion_hegrenade_interior", GetAbsOrigin(), QAngle( 0, 0, 0 ) );

	SetThink( NULL );
	
	// gibs
	for ( int i = 0; i < ARRAYSIZE( sJammerGibs ); i++ )
	{
		CPhysicsProp *pProp = dynamic_cast<CPhysicsProp *>(CreateEntityByName( "prop_physics" ));
		if ( pProp )
		{
			pProp->SetAbsOrigin( GetAbsOrigin() );
			pProp->SetAbsAngles( GetAbsAngles() );
			pProp->KeyValue( "model", sJammerGibs[i] );
			pProp->Spawn();
			pProp->SetCollisionGroup( COLLISION_GROUP_DEBRIS );

			IPhysicsObject *pPhysicsObject = pProp->VPhysicsGetObject();
			if ( pPhysicsObject )
			{
				Vector vecVel = (RandomVectorInUnitSphere() + Vector( 0, 0, 1 )).Normalized() * RandomFloat( 50, 150 );
				pPhysicsObject->AddVelocity( &vecVel, NULL );
			}

			pProp->SetNextThink( gpGlobals->curtime + 10.0f );
			pProp->SetThink( &CBaseEntity::SUB_FadeOut );
		}
	}

	UTIL_Remove( this );
}

// --------------------------------------------------------------
// --------------------------------------------------------------


void CBrBaseItem::ProlongedUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CCSPlayer* player = pActivator ? ToCSPlayer( pActivator ) : NULL;
	if ( !player )
		return;

	switch ( m_prolongeduse.ETryToBeginUse( this, player ) )
	{
	case CEntitySupportForProlongedUse_t::k_EBeginUseResult_BeingUsedByOther:
		ClientPrint( player, HUD_PRINTCENTER, GetProlongedUsedByOtherMessage( player ) );
		return;
	case CEntitySupportForProlongedUse_t::k_EBeginUseResult_Started:
		SetThink( &CBrBaseItem::ProlongedUseThink );
		SetNextThink( gpGlobals->curtime );
		OnProlongedUseStarted( player );
		return;
	default:
		return;
	}
}

void CBrBaseItem::ProlongedUseThink()
{
	CCSPlayer *pActivator = NULL;
	switch ( m_prolongeduse.ESupportUseThink( this, &pActivator ) )
	{
	case CEntitySupportForProlongedUse_t::k_EUseInProgressOutcome_NotInUse:
		SetThink( NULL );
		return;

	default:
	case CEntitySupportForProlongedUse_t::k_EUseInProgressOutcome_StillUsing:
		SetNextThink( gpGlobals->curtime + 0.1f );
		return;

	case CEntitySupportForProlongedUse_t::k_EUseInProgressOutcome_Aborted:
		SetThink( NULL );
		return;

	case CEntitySupportForProlongedUse_t::k_EUseInProgressOutcome_Completed:
		SetUse( NULL );
		SetThink( NULL );
		OnProlongedUseSucceeded( pActivator );
		return;
	}
}

// --------------------------------------------------------------
// --------------------------------------------------------------


LINK_ENTITY_TO_CLASS( prop_ammo_box_generic, CPhysPropAmmoBox );

BEGIN_DATADESC( CPhysPropAmmoBox )
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CPhysPropAmmoBox, DT_PhysPropAmmoBox )
END_SEND_TABLE()

#define MODEL_AMMOBOX_GENERIC "models/props_survival/crates/crate_ammobox.mdl"

DEVELOPMENT_ONLY_CONVAR( dev_dz_ammobox_capacity, 4 ); // 4 uses
CPhysPropAmmoBox::CPhysPropAmmoBox()
{
	m_flTimeLastUsed = 0;
	m_nUsesRemaining = dev_dz_ammobox_capacity.GetInt();
}

CPhysPropAmmoBox::~CPhysPropAmmoBox()
{
}

void CPhysPropAmmoBox::Precache()
{
	PrecacheModel( MODEL_AMMOBOX_GENERIC );

	SetModelName( MAKE_STRING( MODEL_AMMOBOX_GENERIC ) );

	BaseClass::Precache();
}

void CPhysPropAmmoBox::Spawn( void )
{
	SetModel( MODEL_AMMOBOX_GENERIC );

	VisibilityMonitor_AddEntity( this, 600.0f, NULL, NULL );

	BaseClass::Spawn();
	SetCollisionGroup( COLLISION_GROUP_DEBRIS );
}

int CPhysPropAmmoBox::OnTakeDamage( const CTakeDamageInfo &info )
{
	return 0;
}

void CPhysPropAmmoBox::Use( CBaseEntity * pActivator, CBaseEntity * pCaller, USE_TYPE useType, float value )
{
	Assert( m_nUsesRemaining );

	if ( !pActivator || !pActivator->IsPlayer() )
		return;

	CCSPlayer *pPlayer = ToCSPlayer( pActivator );
	if ( !pPlayer )
		return;

	// check we don't have a primary or secondary weapon deployed, and switch to our best gun if we have one
	CWeaponCSBase *pActiveWeapon = pPlayer->GetActiveCSWeapon();
	CBaseCombatWeapon *pWeaponPrimary = pPlayer->Weapon_GetSlot( GEAR_SLOT_RIFLE );
	CBaseCombatWeapon *pWeaponSecondary = pPlayer->Weapon_GetSlot( GEAR_SLOT_PISTOL );
	if ( pActiveWeapon && pActiveWeapon != pWeaponPrimary && pActiveWeapon != pWeaponSecondary ) // neither of our guns are active
	{
		if ( pWeaponPrimary ) // try to deploy primary, if we have it
		{
			m_flTimeLastUsed = gpGlobals->curtime;
			pPlayer->Weapon_Switch( pWeaponPrimary );
			return;
		}
		else if ( pWeaponSecondary ) // otherwise secondary, if we have it
		{
			m_flTimeLastUsed = gpGlobals->curtime;
			pPlayer->Weapon_Switch( pWeaponSecondary );
			return;
		}	
	}

	float flBoostAmt = pPlayer->GetCurrentHealthShotBoostAmount();
	float flDispensingRate = Lerp( flBoostAmt, dev_dz_ammobox_dispensing_rate.GetFloat(), dev_dz_ammobox_dispensing_rate_boosted.GetFloat() );

	if ( (gpGlobals->curtime - m_flTimeLastUsed) < flDispensingRate )
		return;

	m_flTimeLastUsed = gpGlobals->curtime;

	if ( pActiveWeapon->m_flLastDeployTime > gpGlobals->curtime - 0.6f )
		return; // wait until a weapon is a bit deployed before filling ammo

	auto lambdaAmmoRefill = [&]( bool bSuccess, int nPriority = 0 )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "ammo_refill" );
		if ( event )
		{
			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetBool( "success", bSuccess );
			event->SetInt( "priority", nPriority );
			gameeventmanager->FireEvent( event );
		}
	};
	
	if ( !pActiveWeapon || !WeaponIsBallistic( pActiveWeapon->GetWeaponType() ) )
	{
		lambdaAmmoRefill( false );
		return;
	}

	int nBulletsToTake = 0;

	const char* szWeaponClassname = pActiveWeapon->GetClassname();
	if ( pActiveWeapon->GetWeaponType() == WEAPONTYPE_PISTOL )
	{
		nBulletsToTake = 4;

		// hack in custom costs for some guns
		if ( V_stristr( szWeaponClassname, "deagle" ) )
		{
			nBulletsToTake = 2;
		}
		else if ( V_stristr( szWeaponClassname, "revolver" ) )
		{
			nBulletsToTake = 1;
		}

	}
	else if ( pActiveWeapon->GetWeaponType() == WEAPONTYPE_SUBMACHINEGUN )
	{
		nBulletsToTake = 4;
	}
	else if ( pActiveWeapon->GetWeaponType() == WEAPONTYPE_RIFLE )
	{
		nBulletsToTake = 2;
	}
	else if ( pActiveWeapon->GetWeaponType() == WEAPONTYPE_SHOTGUN )
	{
		nBulletsToTake = 2;
	}
	else if ( pActiveWeapon->GetWeaponType() == WEAPONTYPE_SNIPER_RIFLE )
	{
		nBulletsToTake = 1;
	}
	else if ( pActiveWeapon->GetWeaponType() == WEAPONTYPE_MACHINEGUN )
	{
		nBulletsToTake = 2;
	}
	else
	{
		lambdaAmmoRefill( false );
		return;
	}

	int nCurrentReserve = pActiveWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );
	int nHaveSpaceFor = pActiveWeapon->GetReserveAmmoMax( AMMO_POSITION_PRIMARY ) - nCurrentReserve;

	nBulletsToTake = MIN( nBulletsToTake, nHaveSpaceFor );
	if ( nBulletsToTake <= 0 )
	{
		lambdaAmmoRefill( false );
		return;
	}

	lambdaAmmoRefill( true, 5 - nBulletsToTake );
	
	CFmtStr fmtInfoLog( "%s_%d", GetClassname(), m_nUsesRemaining );
	CTakeDamageInfo infoLog( pPlayer, pPlayer, nBulletsToTake, DMG_BULLET );
	CCSPlayer::UtilLogPrintfTakeDamageLine( infoLog, this, fmtInfoLog );
		
	m_nUsesRemaining--;

	pActiveWeapon->SetReserveAmmoCount( AMMO_POSITION_PRIMARY, nCurrentReserve + nBulletsToTake );

	EmitSound( "Player.PickupWeapon" );

	for ( int iBG = 4; iBG > 0; iBG-- )
	{
		SetBodygroup( iBG, (m_nUsesRemaining < iBG) ? 1 : 0 );
	}

	if ( !m_nUsesRemaining )
	{
		SetBodygroupPreset( "only_box" );

		// spawn a debris version of the empty crate
		CPhysicsProp *pProp = dynamic_cast<CPhysicsProp *>(CreateEntityByName( "prop_physics" ));
		if ( pProp )
		{
			pProp->SetAbsOrigin( GetAbsOrigin() );
			pProp->SetAbsAngles( GetAbsAngles() );
			pProp->KeyValue( "model", MODEL_AMMOBOX_GENERIC );
			pProp->Spawn();
			pProp->SetCollisionGroup( COLLISION_GROUP_DEBRIS );

			pProp->SetBodygroupPreset( "only_box" );
			pProp->m_nSkin = 1;
			
			IPhysicsObject *pPhysicsObject = pProp->VPhysicsGetObject();
			if ( pPhysicsObject )
			{
				Vector vecVel = (RandomVectorInUnitSphere() + Vector( 0, 0, 1 )).Normalized() * RandomFloat( 10, 50 );
				pPhysicsObject->AddVelocity( &vecVel, NULL );
			}
		}

		UTIL_Remove( this );

	}
	else
	{
		CSingleUserRecipientFilter filter( ToBasePlayer( pPlayer ) );
		filter.MakeReliable();

		CCSUsrMsg_UpdateScreenHealthBar msg;
		msg.set_entidx( entindex() );
		msg.set_healthratio_old( (float)(m_nUsesRemaining+1) / dev_dz_ammobox_capacity.GetFloat() );
		msg.set_healthratio_new( (float)m_nUsesRemaining / dev_dz_ammobox_capacity.GetFloat() );
		msg.set_style( 1 ); // ammo box style
		SendUserMessage( filter, CS_UM_UpdateScreenHealthBar, msg );
	}

	const char *pszOriginalSource = m_strOriginalSource.IsEmpty() ? "" : CFmtStr( "%s_", m_strOriginalSource.Get() );
	UTIL_LogPrintf( "\"%s<%i><%s>\" take <%sammo> from entindex[%d][%d %d %d][%d-%d=%d] to weapon[%s][<%d>/<%d+%d=%d>]\n",
		pPlayer->GetPlayerName(),
		pPlayer->entindex(),
		pPlayer->GetNetworkIDString(),
		pszOriginalSource,
		entindex(), (int)GetAbsOrigin().x, (int)GetAbsOrigin().y, (int)GetAbsOrigin().z,
		m_nUsesRemaining+1, 1, m_nUsesRemaining, // ???
		szWeaponClassname, pActiveWeapon->Clip1(), nCurrentReserve, nBulletsToTake, nCurrentReserve + nBulletsToTake
		);
	
}


// --------------------------------------------------------------
// --------------------------------------------------------------

BEGIN_DATADESC( CPhysPropWeaponUpgrade )
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CPhysPropWeaponUpgrade, DT_PhysPropWeaponUpgrade )
END_SEND_TABLE()

CPhysPropWeaponUpgrade::CPhysPropWeaponUpgrade()
{
	m_flTimeLastUsed = 0;
	m_nEventPriority = 2;
}

void CPhysPropWeaponUpgrade::Precache()
{
	if ( !CSGameRules() || !CSGameRules()->IsPlayingSurvival() )
		return;

	PrecacheModel( GetModelPath() );
	//PrecacheModel( "models/player/custom_player/legacy/tm_phoenix_heavy.mdl" );
	//PrecacheModel( "models/weapons/v_models/arms/phoenix_heavy/v_sleeve_phoenix_heavy.mdl" );
	//PrecacheModel( "models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_black.mdl" );

	//SetModelName( MAKE_STRING( MODEL_UPGRADE_ARMOR ) );

	BaseClass::Precache();
}

void CPhysPropWeaponUpgrade::Spawn( void )
{
	SetModel( GetModelPath() );

	BaseClass::Spawn();

	if ( m_prolongeduse.BIsConfigured() )
	{
		SetCollisionGroup( COLLISION_GROUP_DEBRIS );
		AddFlag( FL_OBJECT ); // have this entity participate in high-priority entity pickup rules
	}
	else
	{
		SetSolid( SOLID_BBOX );
		AddSolidFlags( GetSolidFlags() | FSOLID_NOT_STANDABLE | FSOLID_TRIGGER );
		SetCollisionGroup( COLLISION_GROUP_WEAPON );
		SetTouch( &CPhysPropWeaponUpgrade::ItemTouch );
	}
}

int CPhysPropWeaponUpgrade::OnTakeDamage( const CTakeDamageInfo &info )
{
	return 0;
}

void CPhysPropWeaponUpgrade::ItemTouch( CBaseEntity *pOther )
{
	if ( m_prolongeduse.BIsConfigured() )
		return;

	if ( !pOther->IsPlayer() )
		return;

	CCSPlayer *pPlayer = ToCSPlayer( pOther );
	if ( !pPlayer )
		return;

	if ( !CanUseUpgrade( pPlayer, false ) )
		return;

	// Trigger regular use to avoid code duplication
	this->Use( pOther, pOther, USE_ON, 0.0f );
}

void CPhysPropWeaponUpgrade::Use( CBaseEntity * pActivator, CBaseEntity * pCaller, USE_TYPE useType, float value )
{
	//
	// Ensure that activator is player
	//
	if ( !pActivator || !pActivator->IsPlayer() )
		return;

	CCSPlayer *pPlayer = ToCSPlayer( pActivator );
	if ( !pPlayer )
		return;

	// Ensure that player can use this upgrade
	if ( !CanUseUpgrade( pPlayer, true ) )
		return;

	if ( m_prolongeduse.BIsConfigured() )
	{	// This upgrade participates in prolonged use mechanic, we should redirect use there
		ProlongedUse( pActivator, pCaller, useType, value );
		return;
	}

	// Prevent double-use in case this upgrade is "instant-use"
	if ( (gpGlobals->curtime - m_flTimeLastUsed) < 0.2f )
		return;
	m_flTimeLastUsed = gpGlobals->curtime;

	// Trigger the end result of our upgrade
	OnProlongedUseSucceeded( pPlayer );
}

void CPhysPropWeaponUpgrade::OnProlongedUseSucceeded( CCSPlayer *pPlayer )
{
	CTakeDamageInfo infoLog( pPlayer, pPlayer, 1, DMG_DIRECT );
	CCSPlayer::UtilLogPrintfTakeDamageLine( infoLog, this, GetClassname() );

	UseUpgrade( pPlayer );

	// Fire the event after upgrade was used to allow its override of event priority
	IGameEvent *event = gameeventmanager->CreateEvent( "dz_item_interaction" );
	if ( event )
	{
		event->SetInt( "userid", pPlayer->GetUserID() );
		event->SetInt( "subject", this->entindex() );
		event->SetString( "type", GetClassname() );
		event->SetInt( "priority", pPlayer->GetPickupPriority( m_nEventPriority ) );
		gameeventmanager->FireEvent( event );
	}

	UTIL_Remove( this );
}

static const int SURVIVAL_MAX_NORMAL_ARMOR = 100;
class CPhysPropWeaponUpgradeArmor : public CPhysPropWeaponUpgrade
{
public:
	DECLARE_CLASS( CPhysPropWeaponUpgradeArmor, CPhysPropWeaponUpgrade );

	virtual void Precache() OVERRIDE
	{
		if ( !CSGameRules() || !CSGameRules()->IsPlayingSurvival() )
			return;

		BaseClass::Precache();
	}

	virtual const char *GetModelPath() const OVERRIDE
	{
		return "models/props_survival/upgrades/upgrade_dz_armor.mdl";
	}

	virtual void Spawn( void ) OVERRIDE
	{
		BaseClass::Spawn();
		SetHealth( 100 );
	}

protected:
	virtual bool CanUseUpgrade( CCSPlayer *pPlayer, bool bExplainErrorToUser ) OVERRIDE
	{
		// by picking up this item, player's state won't change. disallow the player from picking it up
		if ( pPlayer->ArmorValue() >= SURVIVAL_MAX_NORMAL_ARMOR )
		{
			if ( bExplainErrorToUser )
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_FullArmor" );

			return false;
		}

		return true;
	}

	virtual void UseUpgrade( CCSPlayer *pPlayer ) OVERRIDE
	{
		pPlayer->IncrementArmorValue( GetHealth(), SURVIVAL_MAX_NORMAL_ARMOR );

		pPlayer->m_flLastEquippedArmorTime = gpGlobals->curtime;
		bool bShowBothArmorAndHelmet =
			pPlayer->m_flLastEquippedHelmetTime && ( gpGlobals->curtime - pPlayer->m_flLastEquippedHelmetTime < 2.0f )
			&& pPlayer->m_flLastEquippedArmorTime && ( gpGlobals->curtime - pPlayer->m_flLastEquippedArmorTime < 2.0f );

		ClientPrint( pPlayer, HUD_PRINTCENTER, bShowBothArmorAndHelmet ? "#SFUI_ArmorAndHelmetEquipped" : "#SFUI_ArmorEquipped" );

		EmitSound( "Survival.ArmorPickup" );

		pPlayer->SetBodygroupPreset( "show_vest" );
	}
};
LINK_ENTITY_TO_CLASS( prop_weapon_upgrade_armor, CPhysPropWeaponUpgradeArmor );

class CPhysPropWeaponUpgradeHelmet : public CPhysPropWeaponUpgrade
{
public:
	DECLARE_CLASS( CPhysPropWeaponUpgradeHelmet, CPhysPropWeaponUpgrade );

	virtual const char *GetModelPath() const OVERRIDE
	{
		return "models/props_survival/upgrades/upgrade_dz_helmet.mdl";
	}

	virtual void Spawn( void ) OVERRIDE
	{
		BaseClass::Spawn();
		SetHealth( 5 );
	}

protected:
	virtual bool CanUseUpgrade( CCSPlayer *pPlayer, bool bExplainErrorToUser ) OVERRIDE
	{
		// by picking up this item, player's state won't change. disallow the player from picking it up
		if ( pPlayer->m_bHasHelmet && pPlayer->ArmorValue() >= SURVIVAL_MAX_NORMAL_ARMOR )
		{
			if ( bExplainErrorToUser )
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_FullArmor" );

			return false;
		}

		return true;
	}

	virtual void UseUpgrade( CCSPlayer *pPlayer ) OVERRIDE
	{
		pPlayer->m_bHasHelmet = true;
		pPlayer->IncrementArmorValue( GetHealth(), SURVIVAL_MAX_NORMAL_ARMOR );

		pPlayer->m_flLastEquippedHelmetTime = gpGlobals->curtime;
		bool bShowBothArmorAndHelmet =
			pPlayer->m_flLastEquippedHelmetTime && ( gpGlobals->curtime - pPlayer->m_flLastEquippedHelmetTime < 2.0f )
			&& pPlayer->m_flLastEquippedArmorTime && ( gpGlobals->curtime - pPlayer->m_flLastEquippedArmorTime < 2.0f );

		ClientPrint( pPlayer, HUD_PRINTCENTER, bShowBothArmorAndHelmet ? "#SFUI_ArmorAndHelmetEquipped" : "#SFUI_HelmetEquipped" );

		EmitSound( "Survival.ArmorPickup" );

		pPlayer->SetBodygroupPreset( "show_helmet" );
	}
};
LINK_ENTITY_TO_CLASS( prop_weapon_upgrade_helmet, CPhysPropWeaponUpgradeHelmet );

class CPhysPropWeaponUpgradeArmorHelmet : public CPhysPropWeaponUpgrade
{
public:
	DECLARE_CLASS( CPhysPropWeaponUpgradeArmorHelmet, CPhysPropWeaponUpgrade );

	virtual const char *GetModelPath() const OVERRIDE
	{
		return "models/props_survival/upgrades/upgrade_dz_armor_helmet.mdl";
	}

	virtual void Spawn( void ) OVERRIDE
	{
		BaseClass::Spawn();
		SetHealth( 100 );
	}

protected:
	virtual bool CanUseUpgrade( CCSPlayer *pPlayer, bool bExplainErrorToUser ) OVERRIDE
	{
		// by picking up this item, player's state won't change. disallow the player from picking it up
		if ( pPlayer->m_bHasHelmet && pPlayer->ArmorValue() >= SURVIVAL_MAX_NORMAL_ARMOR )
		{
			if ( bExplainErrorToUser )
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_FullArmor" );

			return false;
		}

		return true;
	}

	virtual void UseUpgrade( CCSPlayer *pPlayer ) OVERRIDE
	{
		pPlayer->m_bHasHelmet = true;
		pPlayer->IncrementArmorValue( GetHealth(), SURVIVAL_MAX_NORMAL_ARMOR );

		pPlayer->m_flLastEquippedArmorTime = gpGlobals->curtime;
		pPlayer->m_flLastEquippedHelmetTime = gpGlobals->curtime;
		ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_ArmorAndHelmetEquipped" );

		EmitSound( "Survival.ArmorPickup" );

		pPlayer->SetBodygroupPreset( "show_helmet" );
		pPlayer->SetBodygroupPreset( "show_vest" );
	}
};
LINK_ENTITY_TO_CLASS( prop_weapon_upgrade_armor_helmet, CPhysPropWeaponUpgradeArmorHelmet );

class CPhysPropWeaponUpgradeParachute : public CPhysPropWeaponUpgrade
{
public:
	DECLARE_CLASS( CPhysPropWeaponUpgradeParachute, CPhysPropWeaponUpgrade );

	CPhysPropWeaponUpgradeParachute()
	{
		m_prolongeduse.ConfigSetPlayerBlockingUseAction( k_CSPlayerBlockingUseAction_EquippingParachute );
	}

	virtual const char *GetModelPath() const OVERRIDE
	{
		return "models/props_survival/upgrades/parachutepack.mdl";
	}

	virtual CConfigurationForHighPriorityUseEntity_t::EPriority_t GetProlongedUsePriority( CCSPlayer *pPlayer ) OVERRIDE
	{
		return CanUseUpgrade( pPlayer, false ) ? CConfigurationForHighPriorityUseEntity_t::k_EPriority_Parachute : CConfigurationForHighPriorityUseEntity_t::k_EPriority_Default;
	}

	virtual char const * GetProlongedUsedByOtherMessage( CCSPlayer *pPlayer ) OVERRIDE
	{
		return "#SFUIHUD_InfoPanel_EquippingParachute_Other";
	}

	virtual void OnProlongedUseStarted( CCSPlayer *pPlayer ) OVERRIDE
	{
		EmitSound( "Survival.ParachuteEquipping" );
	}

protected:
	virtual bool CanUseUpgrade( CCSPlayer *pPlayer, bool bExplainErrorToUser ) OVERRIDE
	{
		// by picking up this item, player's state won't change. disallow the player from picking it up
		if ( pPlayer->m_nParachuteState != PLAYER_PARACHUTE_NO_CHUTE )
		{
			if ( bExplainErrorToUser )
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_ParachuteAlreadyEquipped" );

			return false;
		}

		return true;
	}

	virtual void UseUpgrade( CCSPlayer *pPlayer ) OVERRIDE
	{
		if ( pPlayer->m_nParachuteState == PLAYER_PARACHUTE_NO_CHUTE )
		{
			pPlayer->GiveParachute();

			EmitSound( "Survival.ParachuteEquipped" );
			EmitSound( "Survival.ItemPickup" );

			IGameEvent * event = gameeventmanager->CreateEvent( "parachute_pickup" );
			if ( event )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				gameeventmanager->FireEvent( event );
			}
		}
	}
};
LINK_ENTITY_TO_CLASS( prop_weapon_upgrade_chute, CPhysPropWeaponUpgradeParachute );

class CPhysPropWeaponUpgradeContractKill : public CPhysPropWeaponUpgrade
{
public:
	DECLARE_CLASS( CPhysPropWeaponUpgradeContractKill, CPhysPropWeaponUpgrade );

	CPhysPropWeaponUpgradeContractKill()
	{
		m_prolongeduse.ConfigSetPlayerBlockingUseAction( k_CSPlayerBlockingUseAction_EquippingContract );
	}

	virtual const char *GetModelPath() const OVERRIDE
	{
		return "models/props_survival/briefcase/briefcase.mdl";
	}

	virtual CConfigurationForHighPriorityUseEntity_t::EPriority_t GetProlongedUsePriority( CCSPlayer *pPlayer ) OVERRIDE
	{
		return CanUseUpgrade( pPlayer, false ) ? CConfigurationForHighPriorityUseEntity_t::k_EPriority_Contract : CConfigurationForHighPriorityUseEntity_t::k_EPriority_Default;
	}

	virtual char const * GetProlongedUsedByOtherMessage( CCSPlayer *pPlayer ) OVERRIDE
	{
		return "#SFUIHUD_InfoPanel_EquippingContract_Other";
	}

	virtual void OnProlongedUseStarted( CCSPlayer *pPlayer ) OVERRIDE
	{
		EmitSound( "Survival.BriefcaseUnlocking" );
	}

protected:
	virtual bool CanUseUpgrade( CCSPlayer *pPlayer, bool bExplainErrorToUser ) OVERRIDE
	{
		CCSPlayer *pExistingContractKillTarget = ToCSPlayer( pPlayer->m_hSurvivalAssassinationTarget.Get() );
		if ( pExistingContractKillTarget && pExistingContractKillTarget->IsAlive() )
		{
			if ( bExplainErrorToUser )
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_ContractKillAlreadyOpen" );

			return false;
		}

		return true;
	}

	virtual void UseUpgrade( CCSPlayer *pPlayer ) OVERRIDE
	{
		// choose a contract kill target
		
		float flMaxDist = 0;
		CCSPlayer* pFarthestPlayer = NULL;

		CUtlVector<CCSPlayer*> vecPotentialKillTargets;
		for ( int iPlayer = 1; iPlayer <= MAX_PLAYERS; ++iPlayer )
		{
			CCSPlayer* pOtherPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayer ) );
			if ( !pOtherPlayer || pOtherPlayer == pPlayer || !pOtherPlayer->IsAlive() ) // we don't want to hunt ourselves
				continue;

			CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
			if ( pBRrules && pBRrules->IsPlayingTeamMode() && pOtherPlayer->m_nSurvivalTeam == pPlayer->m_nSurvivalTeam )
			{
				continue; // don't get a contract kill for your teammate!
			}

			// already completing a contract kill makes you a much more likely target
			for ( int nOtherKills = 0; nOtherKills < pOtherPlayer->m_nCompletedSurvivalAssassinations; nOtherKills++ )
			{
				vecPotentialKillTargets.AddToTail( pOtherPlayer );
				vecPotentialKillTargets.AddToTail( pOtherPlayer );
				vecPotentialKillTargets.AddToTail( pOtherPlayer );
			}

			// owning a primary weapon makes you a much more likely target
			CBaseCombatWeapon *pWeaponPrimary = pOtherPlayer->Weapon_GetSlot( GEAR_SLOT_RIFLE );
			if ( pWeaponPrimary )
			{
				vecPotentialKillTargets.AddToTail( pOtherPlayer );
				vecPotentialKillTargets.AddToTail( pOtherPlayer );
			}

			// owning a secondary weapon makes you a slightly more likely target
			CBaseCombatWeapon *pWeaponSecondary = pOtherPlayer->Weapon_GetSlot( GEAR_SLOT_PISTOL );
			if ( pWeaponSecondary )
			{
				vecPotentialKillTargets.AddToTail( pOtherPlayer );
			}

			// currently hunting someone else?
			CCSPlayer *pOtherPlayersContractKillTarget = ToCSPlayer( pOtherPlayer->m_hSurvivalAssassinationTarget.Get() );
			if ( pOtherPlayersContractKillTarget && pOtherPlayersContractKillTarget->IsAlive() )
			{
				vecPotentialKillTargets.AddToTail( pOtherPlayer );
				vecPotentialKillTargets.AddToTail( pOtherPlayer );
			}

#if DEVELOPMENT_ONLY
			if ( pOtherPlayer->IsBot() )
			{
				vecPotentialKillTargets.AddToTail( pOtherPlayer ); // for testing
			}
#endif

			// find farthest player
			float flDist = pOtherPlayer->GetAbsOrigin().DistToSqr( pPlayer->GetAbsOrigin() );
			if ( flDist > flMaxDist )
			{
				pFarthestPlayer = pOtherPlayer;
				flMaxDist = flDist;
			}

		}

		// add in the farthest player
		if ( pFarthestPlayer )
		{
			vecPotentialKillTargets.AddToTail( pFarthestPlayer );
		}

		if ( vecPotentialKillTargets.Count() )
		{
			CCSPlayer *pContractKillTarget = vecPotentialKillTargets.Random();
			EmitSound( "Survival.BriefcaseUnlockSuccess" );
						
			pPlayer->m_hSurvivalAssassinationTarget = pContractKillTarget;

			// give the contract to your teammates too
			CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
			if ( pBRrules && pBRrules->IsPlayingTeamMode() )
			{
				PlayerTeammateVector_t vecTeammates;
				pBRrules->GetPlayerTeammates( pPlayer, vecTeammates );
				FOR_EACH_VEC( vecTeammates, iTeammate )
				{
					vecTeammates[iTeammate]->m_hSurvivalAssassinationTarget = pContractKillTarget;
				}
			}

			extern ConVar sv_dz_cash_bundle_size;
			int reward = sv_dz_contractkill_reward.GetInt() * sv_dz_cash_bundle_size.GetInt();
			ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_ContractKillStart", CFmtStr( "%d", reward ) );
		}
		else
		{
			AssertMsg( false, "Couldn't locate a valid contract kill target!" );
		}

		CDynamicProp *pOpenedContractKill = dynamic_cast<CDynamicProp *>(CreateEntityByName( "dynamic_prop" ));
		if ( pOpenedContractKill )
		{
			pOpenedContractKill->SetAbsAngles( GetAbsAngles() );
			pOpenedContractKill->SetAbsOrigin( GetAbsOrigin() );
			pOpenedContractKill->KeyValue( "model", GetModelPath() );
			pOpenedContractKill->Spawn();
			pOpenedContractKill->SetSequence( 1 );
			pOpenedContractKill->SetPlaybackRate( 1 );
			pOpenedContractKill->SetCollisionGroup( COLLISION_GROUP_NONE );
			pOpenedContractKill->AddSolidFlags( FSOLID_NOT_SOLID );

			pOpenedContractKill->SUB_StartFadeOut( 60.0f );
			//pOpenedContractKill->SetThink( &CBaseEntity::SUB_Remove );
			//pOpenedContractKill->SetNextThink( gpGlobals->curtime + 10.0f );
		}
	}
};
LINK_ENTITY_TO_CLASS( prop_weapon_upgrade_contractkill, CPhysPropWeaponUpgradeContractKill );

#if 0
// Heavy armor upgrade is not shipping with survival at launch
class CPhysPropWeaponUpgradeHeavyArmor : public CPhysPropWeaponUpgrade
{
public:
	DECLARE_CLASS( CPhysPropWeaponUpgradeHeavyArmor, CPhysPropWeaponUpgrade );

	CPhysPropWeaponUpgradeHeavyArmor()
	{
		m_prolongeduse.ConfigSetPlayerBlockingUseAction( k_CSPlayerBlockingUseAction_EquippingHeavyArmor );
		m_prolongeduse.ConfigSetUseDurationToCompletion( 4.0f ); // takes a while to put all the stuff on
	}

	virtual const char *GetModelPath() const OVERRIDE
	{
		return "models/props_survival/upgrades/upgrade_heavy_armor.mdl";
	}

	virtual CConfigurationForHighPriorityUseEntity_t::EPriority_t GetProlongedUsePriority( CCSPlayer *pPlayer ) OVERRIDE
	{
		return CanUseUpgrade( pPlayer, false ) ? CConfigurationForHighPriorityUseEntity_t::k_EPriority_HeavyArmor : CConfigurationForHighPriorityUseEntity_t::k_EPriority_Default;
	}

	virtual char const * GetProlongedUsedByOtherMessage( CCSPlayer *pPlayer ) OVERRIDE
	{
		return "#SFUIHUD_InfoPanel_EquippingHeavyArmor_Other";
	}

	virtual void OnProlongedUseStarted( CCSPlayer *pPlayer ) OVERRIDE
	{
		EmitSound( "Player.EquipArmor_T" );
		EmitSound( "T_Default.Suit" );
	}

	virtual void Spawn( void ) OVERRIDE
	{
		BaseClass::Spawn();
		SetHealth( 200 );
	}

protected:
	virtual bool CanUseUpgrade( CCSPlayer *pPlayer, bool bExplainErrorToUser ) OVERRIDE
	{
		CBaseCombatWeapon *pPrimary = pPlayer->Weapon_GetSlot( GEAR_SLOT_RIFLE );
		if ( pPrimary )
		{
			if ( bExplainErrorToUser )
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_NeedToDropPrimaryWepForHeavy" );
			return false;
		}
		return true;
	}

	virtual void UseUpgrade( CCSPlayer *pPlayer ) OVERRIDE
	{
		pPlayer->GiveNamedItem( "item_heavyassaultsuit" );
		pPlayer->SetArmorValue( GetHealth() );

		color32_s clr = { 0, 0, 0, 255 };
		UTIL_ScreenFade( pPlayer, clr, 0.3f, 0.02f, FFADE_IN | FFADE_SOFTCURVE );

		// playing all these sounds for now to produce a real kerfuffle
		EmitSound( "Player.EquipArmor_CT" );
		EmitSound( "Player.EquipArmor_T" );
		EmitSound( "T_Default.Suit" );

		ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_HeavyArmorEquipped" );

		m_nEventPriority = 7;
	}
};
LINK_ENTITY_TO_CLASS( prop_weapon_upgrade_heavyarmor, CPhysPropWeaponUpgradeHeavyArmor );
#endif

class CPhysPropWeaponUpgradeTablet : public CPhysPropWeaponUpgrade
{
public:
	DECLARE_CLASS( CPhysPropWeaponUpgradeTablet, CPhysPropWeaponUpgrade );

	CPhysPropWeaponUpgradeTablet()
	{
		m_prolongeduse.ConfigSetPlayerBlockingUseAction( k_CSPlayerBlockingUseAction_EquippingTabletUpgrade );
	}

	virtual CConfigurationForHighPriorityUseEntity_t::EPriority_t GetProlongedUsePriority( CCSPlayer *pPlayer ) OVERRIDE
	{
		return CanUseUpgrade( pPlayer, false ) ? CConfigurationForHighPriorityUseEntity_t::k_EPriority_TabletUpgrade : CConfigurationForHighPriorityUseEntity_t::k_EPriority_Default;
	}

	virtual char const * GetProlongedUsedByOtherMessage( CCSPlayer *pPlayer ) OVERRIDE
	{
		return "#SFUIHUD_InfoPanel_EquippingTabletUpgrade_Other";
	}

	virtual void OnProlongedUseStarted( CCSPlayer *pPlayer ) OVERRIDE
	{
		EmitSound( "Survival.UpgradeTabletStart" );
	}

	virtual void Precache() OVERRIDE
	{
		if ( !CSGameRules() || !CSGameRules()->IsPlayingSurvival() )
			return;

		BaseClass::Precache();
	}

protected:
	virtual tablet_upgrade_type_t GetTabletUpgradeType() const = 0;

	virtual bool CanUseUpgrade( CCSPlayer *pPlayer, bool bExplainErrorToUser ) OVERRIDE
	{
		CBaseCombatWeapon* pTablet = pPlayer->Weapon_OwnsThisType( "weapon_tablet" );
		if ( !pTablet )
		{
			if ( bExplainErrorToUser )
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_TabletUpgradeNoTablet" );
			return false;
		}
		else if ( static_cast< CTablet* >( pTablet )->HasTabletUpgrade( GetTabletUpgradeType() ) )
		{
			if ( bExplainErrorToUser )
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_TabletUpgradeAlreadyUpgraded" );
			return false;
		}

		return true;
	}

	virtual void UseUpgrade( CCSPlayer *pPlayer ) OVERRIDE
	{
		EmitSound( "Survival.UpgradeTabletSuccess" );

		CBaseCombatWeapon* pCurrentWep = pPlayer->GetActiveWeapon();
		CTablet* pTablet = static_cast< CTablet* >( pPlayer->Weapon_OwnsThisType( "weapon_tablet" ) );
		// auto switch to tablet
		if ( pCurrentWep != pTablet )
		{
			pPlayer->Weapon_Switch( pTablet );
		}

		pTablet->SetTabletUpgrade( GetTabletUpgradeType(), true );

		switch ( GetTabletUpgradeType() )
		{
		case TABLET_UPGRADE_HIGHRES:
			ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_TabletUpgradeHighres" );
			break;
		case TABLET_UPGRADE_DRONEINTEL:
			ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_TabletUpgradeDroneIntel" );
			break;
		case TABLET_UPGRADE_ZONEINTEL:
			ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_TabletUpgradeZoneIntel" );
			break;
		}
	}
};

class CPhysPropWeaponUpgradeTabletHighres : public CPhysPropWeaponUpgradeTablet
{
public:
	DECLARE_CLASS( CPhysPropWeaponUpgradeTabletHighres, CPhysPropWeaponUpgradeTablet );

	virtual const char *GetModelPath() const OVERRIDE
	{
		return "models/props_survival/upgrades/upgrade_tablet_hires.mdl";
	}

protected:
	virtual tablet_upgrade_type_t GetTabletUpgradeType() const OVERRIDE { return TABLET_UPGRADE_HIGHRES; }
};
LINK_ENTITY_TO_CLASS( prop_weapon_upgrade_tablet_highres, CPhysPropWeaponUpgradeTabletHighres );

class CPhysPropWeaponUpgradeTabletZoneIntel : public CPhysPropWeaponUpgradeTablet
{
public:
	DECLARE_CLASS( CPhysPropWeaponUpgradeTabletZoneIntel, CPhysPropWeaponUpgradeTablet );

	virtual const char *GetModelPath() const OVERRIDE
	{
		return "models/props_survival/upgrades/upgrade_tablet_zone.mdl";
	}

protected:
	virtual tablet_upgrade_type_t GetTabletUpgradeType() const OVERRIDE { return TABLET_UPGRADE_ZONEINTEL; }
};
LINK_ENTITY_TO_CLASS( prop_weapon_upgrade_tablet_zoneintel, CPhysPropWeaponUpgradeTabletZoneIntel );

class CPhysPropWeaponUpgradeTabletDroneIntel : public CPhysPropWeaponUpgradeTablet
{
public:
	DECLARE_CLASS( CPhysPropWeaponUpgradeTabletDroneIntel, CPhysPropWeaponUpgradeTablet );

	virtual const char *GetModelPath() const OVERRIDE
	{
		return "models/props_survival/upgrades/upgrade_tablet_drone.mdl";
	}

protected:
	virtual tablet_upgrade_type_t GetTabletUpgradeType() const OVERRIDE { return TABLET_UPGRADE_DRONEINTEL; }
};
LINK_ENTITY_TO_CLASS( prop_weapon_upgrade_tablet_droneintel, CPhysPropWeaponUpgradeTabletDroneIntel );


#endif // GAME_DLL
