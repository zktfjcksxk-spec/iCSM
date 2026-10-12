//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "c_world.h"
#include "ivmodemanager.h"
#include "decals.h"
#include "engine/ivmodelinfo.h"
#include "ivieweffects.h"
#include "shake.h"
#include "precache_register.h"
#include "weapon_cache.h"

#ifdef PORTAL2
#include "paint_stream_manager.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CWorld
#undef CWorld
#endif

C_GameRules *g_pGameRules = NULL;
static C_World *g_pClientWorld;
static void ForceEnableDisplacementShadowRenderingForNewMaps();


void ClientWorldFactoryInit()
{
	g_pClientWorld = new C_World;
}

void ClientWorldFactoryShutdown()
{
	delete g_pClientWorld;
	g_pClientWorld = NULL;
}

static IClientNetworkable* ClientWorldFactory( int entnum, int serialNum )
{
	Assert( g_pClientWorld != NULL );

	g_pClientWorld->Init( entnum, serialNum );
	return g_pClientWorld;
}


IMPLEMENT_CLIENTCLASS_FACTORY( C_World, DT_World, CWorld, ClientWorldFactory );

BEGIN_RECV_TABLE( C_World, DT_World )
	RecvPropFloat(RECVINFO(m_flWaveHeight)),
	RecvPropVector(RECVINFO(m_WorldMins)),
	RecvPropVector(RECVINFO(m_WorldMaxs)),
	RecvPropInt(RECVINFO(m_bStartDark)),
	RecvPropFloat(RECVINFO(m_flMaxOccludeeArea)),
	RecvPropFloat(RECVINFO(m_flMinOccluderArea)),
	RecvPropFloat(RECVINFO(m_flMaxPropScreenSpaceWidth)),
	RecvPropFloat(RECVINFO(m_flMinPropScreenSpaceWidth)),
	RecvPropString(RECVINFO(m_iszDetailSpriteMaterial)),
	RecvPropInt(RECVINFO(m_bColdWorld)),
	RecvPropInt(RECVINFO(m_iTimeOfDay)),
#ifdef PORTAL2
	RecvPropInt(RECVINFO(m_nMaxBlobCount)),
#endif
END_RECV_TABLE()


C_World::C_World( void )
{
}

C_World::~C_World( void )
{
}

bool C_World::Init( int entnum, int iSerialNum )
{
	m_flWaveHeight = 0.0f;

	return BaseClass::Init( entnum, iSerialNum );
}

void C_World::Release()
{
	// Manually reset the data changed event ref.
	// CL_ProcessPacketEntities, during a full update, runs through it's entitylist
	// deleting all entities, and then calls PurgeOnDataChangedEvents, which throws away all data
	// changed event refs. g_pClientWorld is not part of the entitylist, and isn't deleted and recreated, 
	// so ends up holding on to it's now-invalid data changed event ref, which causes a crash on 
	// exit when C_World is deleted and ClearDataChangedEvent( invalidRef ) is called by ~C_BaseEntity()
	ResetDataChangedEventRef();
	Term();
}

void C_World::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );
}

void C_World::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// Always force reset to normal mode upon receipt of world in new map
	if ( updateType == DATA_UPDATE_CREATED )
	{
		modemanager->SwitchMode( false, true );

		if ( m_bStartDark )
		{
			ScreenFade_t sf;
			memset( &sf, 0, sizeof( sf ) );
			sf.a = 255;
			sf.r = 0;
			sf.g = 0;
			sf.b = 0;
			sf.duration = (float)(1<<SCREENFADE_FRACBITS) * 5.0f;
			sf.holdTime = (float)(1<<SCREENFADE_FRACBITS) * 1.0f;
			sf.fadeFlags = FFADE_IN | FFADE_PURGE;
			FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
			{
				ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
				GetViewEffects()->Fade( sf );
			}
		}

		OcclusionParams_t params;
		params.m_flMaxOccludeeArea = m_flMaxOccludeeArea;
		params.m_flMinOccluderArea = m_flMinOccluderArea;
		engine->SetOcclusionParameters( params );

		modelinfo->SetLevelScreenFadeRange( m_flMinPropScreenSpaceWidth, m_flMaxPropScreenSpaceWidth );

#ifdef PORTAL2
		PaintStreamManager.AllocatePaintBlobPool( m_nMaxBlobCount );
#endif
	}
}

// -----------------------------------------
//	Sprite Index info
// -----------------------------------------
#if !defined( TF_DLL ) && !defined ( DOTA_DLL ) && !defined ( PORTAL2 )
int		g_sModelIndexLaser;			// holds the index for the laser beam
int		g_sModelIndexLaserDot;		// holds the index for the laser beam dot
int		g_sModelIndexFireball;		// holds the index for the fireball
int		g_sModelIndexWExplosion;	// holds the index for the underwater explosion
int		g_sModelIndexBubbles;		// holds the index for the bubbles model
#endif 

int		g_sModelIndexSmoke;			// holds the index for the smoke cloud
int		g_sModelIndexBloodSpray;	// holds the sprite index for splattered blood
int		g_sModelIndexBloodDrop;		// holds the sprite index for the initial blood

//-----------------------------------------------------------------------------
// Purpose: Precache global weapon resources
//-----------------------------------------------------------------------------
PRECACHE_REGISTER_BEGIN( GLOBAL, WeaponSprites )
#if !defined( TF_DLL ) && !defined ( DOTA_DLL ) && !defined ( PORTAL2 )
	PRECACHE_INDEX( MODEL, "sprites/zerogxplode.vmt", g_sModelIndexFireball )
	PRECACHE_INDEX( MODEL, "sprites/WXplo1.vmt", g_sModelIndexWExplosion )
	PRECACHE_INDEX( MODEL, "sprites/steam1.vmt", g_sModelIndexSmoke )
	PRECACHE_INDEX( MODEL, "sprites/bubble.vmt", g_sModelIndexBubbles )
	PRECACHE_INDEX( MODEL, "sprites/bloodspray.vmt", g_sModelIndexBloodSpray )
	PRECACHE_INDEX( MODEL, "sprites/blood.vmt", g_sModelIndexBloodDrop )
	PRECACHE_INDEX( MODEL, "sprites/laserbeam.vmt", g_sModelIndexLaser )
	PRECACHE_INDEX( MODEL, "sprites/laserdot.vmt", g_sModelIndexLaserDot )
#endif
PRECACHE_REGISTER_END()

void W_Precache(void)
{
	g_pWeaponSystem->PrecacheWeaponClasses();
}

void C_World::Precache( void )
{
	// Get weapon precaches
	W_Precache();	
}

void C_World::Spawn( void )
{
	Precache();
	ForceEnableDisplacementShadowRenderingForNewMaps();
}


C_World *GetClientWorldEntity()
{
	Assert( g_pClientWorld != NULL );
	return g_pClientWorld;
}

/*static*/ C_World* C_World::GetWorldEntity()
{
	return ::GetClientWorldEntity();
}

// de_dust2 is not in here on purpose. It is the first map for which we are forcing displacement rendering
// into shadow maps even on the "very low" shadow setting. Remove maps in this list as they get uprez-ed and
// re-released if the map uses lots of displacements as walls.
static const char* g_mapsWithoutDisplacementRenderingOnVeryLowShadowSettings[] =
{
	"ar_baggage",
	"ar_dizzy",
	"ar_monastery",
	"ar_shoots",
	"coop_cementplant",
	"cs_agency",
	"cs_assault",
	"cs_backalley",
	"cs_cruise",
	"cs_downtown",
	"cs_insertion",
	"cs_italy",
	"cs_militia",
	"cs_motel",
	"cs_office",
	"cs_rush",
	"cs_siege",
	"cs_thunder",
	"cs_workout",
	"de_ali",
	"de_austria",
	"de_aztec",
	"de_bank",
	"de_bazaar",
	"de_blackgold",
	"de_cache",
	"de_canals",
	"de_castle",
	"de_cbble",
	"de_chinatown",
	"de_coast",
	"de_dust",
	"de_empire",
	"de_facade",
	"de_favela",
	"de_gwalior",
	"de_inferno",
	"de_lake",
	"de_lite",
	"de_log",
	"de_marquis",
	"de_mikla",
	"de_mirage",
	"de_mist",
	"de_nuke",
	"de_overgrown",
	"de_overpass",
	"de_rails",
	"de_resort",
	"de_royal",
	"de_ruins",
	"de_safehouse",
	"de_santorini",
	"de_seaside",
	"de_season",
	"de_shipped",
	"de_shortdust",
	"de_shorttrain",
	"de_stmarc",
	"de_sugarcane",
	"de_thrill",
	"de_train",
	"de_tulip",
	"de_vertigo",
	"de_zoo",
	"gd_assault",
	"gd_bank",
	"gd_cbble",
	"gd_crashsite",
	"gd_lake",
	"gd_rialto",
	"gd_sugarcane",
	"training1",
	nullptr
};

extern bool g_bCSMForceEnableDisplacements;	// exists and is used in viewrender.cpp

static void ForceEnableDisplacementShadowRenderingForNewMaps()
{
	// Check map against whitelist and turn displacement rendering into csm on for all new maps
	const char *pMapName = IGameSystem::MapName();
	if ( !pMapName )
		return;

	const char **pCheckName = g_mapsWithoutDisplacementRenderingOnVeryLowShadowSettings;

	g_bCSMForceEnableDisplacements = true;
	while ( *pCheckName )
	{
		if ( V_stricmp( pMapName, *pCheckName ) == 0 )
		{
			g_bCSMForceEnableDisplacements = false;
			break;
		}
		pCheckName++;
	}
}
