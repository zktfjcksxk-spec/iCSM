
#include "cbase.h"

#include "item_dogtags.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ItemDogtags, DT_ItemDogtags )

#ifdef GAME_DLL
BEGIN_NETWORK_TABLE( CItemDogtags, DT_ItemDogtags )
SendPropEHandle( SENDINFO( m_OwningPlayer ) ),
SendPropEHandle( SENDINFO( m_KillingPlayer ) ),
END_NETWORK_TABLE()
#else
BEGIN_NETWORK_TABLE( CItemDogtags, DT_ItemDogtags )
RecvPropEHandle( RECVINFO( m_OwningPlayer ) ),
RecvPropEHandle( RECVINFO( m_KillingPlayer ) ),
END_NETWORK_TABLE()
#endif

LINK_ENTITY_TO_CLASS_ALIASED( item_dogtags, ItemDogtags )
PRECACHE_REGISTER( item_dogtags )

#define DOGTAG_MODEL "models/inventory_items/dogtags.mdl"
#define DOGTAG_SOUND_PICKUP "DogTags.Pickup"
#define DOGTAG_SOUND_PICKUP_DENY "DogTags.PickupDeny"


enum {
	kDogtagPickupRuleKiller = 0,	// killer only (default)
	kDogtagPickupRuleKillerTeam,	// killer and killer's teammates
	kDogtagPickupRuleRescue,		// teammates of dead player
	kDogtagPickupRuleKillerRescue,	// killer and teammates of dead player
	kDogtagPickupRuleAll,			// everyone
};

bool CItemDogtags::CanBePickedUpBy( CCSPlayer* pPlayer )
{
	static ConVarRef mp_dogtag_pickup_rule( "mp_dogtag_pickup_rule" );

	CCSPlayer* pKiller = m_KillingPlayer.Get();
	CCSPlayer* pVictim = m_OwningPlayer.Get();

	switch ( mp_dogtag_pickup_rule.GetInt() )
	{
	case kDogtagPickupRuleKiller:
	default:
		return ( pKiller == pPlayer );
	case kDogtagPickupRuleKillerTeam:
		return ( pKiller && !pKiller->IsOtherEnemy( pPlayer ) );
	case kDogtagPickupRuleRescue:
		return ( pVictim && !pVictim->IsOtherEnemy( pPlayer ) );
	case kDogtagPickupRuleKillerRescue:
		return ( pKiller == pPlayer
			  || pVictim && !pVictim->IsOtherEnemy( pPlayer ) );
	case kDogtagPickupRuleAll:
		return true;
	}
}

#ifdef GAME_DLL
bool CItemDogtags::ItemCanBeTouchedByPlayer( CBasePlayer *pPlayer )
{
	CCSPlayer* csPlayer = dynamic_cast< CCSPlayer* >( pPlayer );
	if ( !csPlayer )
		return false;

	return CanBePickedUpBy( csPlayer );
}

void CItemDogtags::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	return;
}

bool CItemDogtags::MyTouch( CBasePlayer* pPlayer )
{
	// Should always be a player
	CCSPlayer* pGetter = dynamic_cast< CCSPlayer* >( pPlayer );
	if ( !pGetter )
		return false;

	// if this player isn't allowed to pick up this dog tag, fail
	if ( !CanBePickedUpBy( pGetter ) )
		return false;

	// Let the rules know that the dogtag should be picked up.
	bool deny = false;
	CSGameRules()->GetDogTag( pGetter, this, &deny );

	// Play a sound
	if ( deny )
	{
		// picked up teammates dogtag, denying enemy score
		pGetter->EmitSound( DOGTAG_SOUND_PICKUP_DENY );
	}
	else
	{
		// picked up a scoring dogtag
		pGetter->EmitSound( DOGTAG_SOUND_PICKUP );
	}

	return true;
}

void CItemDogtags::SetDogtagInfo( CCSPlayer* pKiller, CCSPlayer* pTarget )
{
	m_KillingPlayer = pKiller;
	m_OwningPlayer = pTarget;

	if ( pTarget )
		ChangeTeam( pTarget->GetTeamNumber() );
}

void CItemDogtags::LifetimeExpired()
{
	UTIL_Remove( this );
}
#endif // GAME_DLL

void CItemDogtags::Spawn()
{
	Precache();

	SetSolid( SOLID_BBOX );

	// $$$REI TODO is this required/wanted?
	// Weapons won't show up in trace calls if they are being carried...
	RemoveEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );

	SetModel( DOGTAG_MODEL );

	BaseClass::Spawn();

#ifdef GAME_DLL
	// $$$REI TODO: Check if this should be in the higher level item code.  For now we do it here
	if(m_pPhysicsObject)
	{
		QAngle locAngVel = GetLocalAngularVelocity();
		Vector locAngImp = Vector( locAngVel.x, locAngVel.y, locAngVel.z );

		m_pPhysicsObject->AddVelocity( &GetAbsVelocity(), &locAngImp );
	}

	extern ConVar mp_dogtag_despawn_time;
	if ( mp_dogtag_despawn_time.GetFloat() > 0 )
	{
		SetThink( &CItemDogtags::LifetimeExpired );
		SetNextThink( gpGlobals->curtime + mp_dogtag_despawn_time.GetFloat() );
	}
#endif
}

void CItemDogtags::Precache()
{
	BaseClass::Precache();

	PrecacheModel( DOGTAG_MODEL );
	PrecacheScriptSound( DOGTAG_SOUND_PICKUP );
	PrecacheScriptSound( DOGTAG_SOUND_PICKUP_DENY );
}

#ifdef CLIENT_DLL
void CItemDogtags::OnDataChanged( DataUpdateType_t changeType )
{
	BaseClass::OnDataChanged( changeType );

	C_BasePlayer* pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	C_CSPlayer* pLocalCSPlayer = dynamic_cast< C_CSPlayer* >( pLocalPlayer );

	// If we can be picked up by the local player, glow
	if ( pLocalCSPlayer && CanBePickedUpBy( pLocalCSPlayer ) )
	{
		SetBodygroup( 0, 1 );
	}
	else
	{
		SetBodygroup( 0, 0 );
	}
}
#endif


CItemDogtags::CItemDogtags()
	: CItem()
{
#ifdef GAME_DLL
	AddSpawnFlags( SF_NORESPAWN );
#endif
}

