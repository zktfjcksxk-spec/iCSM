//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "fx_cs_shared.h"
#include "weapon_csbase.h"
#include "rumble_shared.h"

#ifndef CLIENT_DLL
	#include "ilagcompensationmanager.h"
#endif

ConVar weapon_accuracy_logging( "weapon_accuracy_logging", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_ARCHIVE );
ConVar steam_controller_haptics( "steam_controller_haptics", "1", FCVAR_RELEASE );
ConVar weapon_near_empty_sound( "weapon_near_empty_sound", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar weapon_debug_max_inaccuracy( "weapon_debug_max_inaccuracy", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "Force all shots to have maximum inaccuracy" );
ConVar weapon_debug_inaccuracy_only_up( "weapon_debug_inaccuracy_only_up", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "Force weapon inaccuracy to be in exactly the up direction" );
ConVar snd_max_pitch_shift_inaccuracy("snd_max_pitch_shift_inaccuracy", "0.08", 0);

ConVar weapon_accuracy_shotgun_spread_patterns( "weapon_accuracy_shotgun_spread_patterns", "1", FCVAR_REPLICATED | FCVAR_RELEASE );

#ifdef CLIENT_DLL

#include "fx_impact.h"
#include "c_rumble.h"
#include "inputsystem/iinputsystem.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

	// this is a cheap ripoff from CBaseCombatWeapon::WeaponSound():
	void FX_WeaponSound(
		int iEntIndex,
		const CEconItemView* pWeaponView,
		WeaponSound_t sound_type,
		const Vector &vOrigin,
		float flSoundTime,
		int nPitch )
	{
		const CCSWeaponInfo* pWeaponInfo = GetWeaponInfoFromItem( pWeaponView );
		if ( !pWeaponInfo )
			return;

		const char* shootsound = pWeaponInfo->GetShootSound( pWeaponView, sound_type );
		if ( !shootsound || !shootsound[0] )
			return;

		CBroadcastRecipientFilter filter; // this is client side only
		if ( !te->CanPredict() )
			return;

		EmitSound_t params;
		params.m_pSoundName = shootsound;
		params.m_flSoundTime = flSoundTime;
		params.m_pOrigin = &vOrigin;
		params.m_pflSoundDuration = nullptr;
		params.m_bWarnOnDirectWaveReference = true;
		params.m_nPitch = nPitch;

		if (nPitch != PITCH_NORM)
		{
			params.m_nFlags = params.m_nFlags | SND_OVERRIDE_PITCH;
		}
				
		CBaseEntity::EmitSound( filter, iEntIndex, params ); 
	}

	class CGroupedSound
	{
	public:
		string_t m_SoundName;
		Vector m_vPos;
	};

	CUtlVector<CGroupedSound> g_GroupedSounds;

	
	// Called by the ImpactSound function.
	void ShotgunImpactSoundGroup( const char *pSoundName, const Vector &vEndPos )
	{
		int i;
		// Don't play the sound if it's too close to another impact sound.
		for ( i=0; i < g_GroupedSounds.Count(); i++ )
		{
			CGroupedSound *pSound = &g_GroupedSounds[i];

			if ( vEndPos.DistToSqr( pSound->m_vPos ) < 300*300 )
			{
				if ( Q_stricmp( pSound->m_SoundName, pSoundName ) == 0 )
					return;
			}
		}

		// Ok, play the sound and add it to the list.
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, NULL, pSoundName, &vEndPos );

		i = g_GroupedSounds.AddToTail();
		g_GroupedSounds[i].m_SoundName = pSoundName;
		g_GroupedSounds[i].m_vPos = vEndPos;
	}


	void StartGroupingSounds()
	{
		Assert( g_GroupedSounds.Count() == 0 );
		SetImpactSoundRoute( ShotgunImpactSoundGroup );
	}


	void EndGroupingSounds()
	{
		g_GroupedSounds.Purge();
		SetImpactSoundRoute( NULL );
	}

#else

	#include "te_shotgun_shot.h"

	// Server doesn't play sounds anyway.
	void StartGroupingSounds() {}
	void EndGroupingSounds() {}
	void FX_WeaponSound ( int iEntIndex,
		const CEconItemView* pWeaponView,
		WeaponSound_t sound_type,
		const Vector &vOrigin,
		float flSoundTime, int nPitch ) {};

#endif

ConVar debug_aim_angle("debug_aim_angle", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY);
	
// This runs on both the client and the server.
// On the server, it only does the damage calculations.
// On the client, it does all the effects.
void FX_FireBullets( 
	int iEntIndex,
	CWeaponCSBase* pWeapon,
	const CEconItemView* pWeaponView,
	const Vector &vOrigin,
	const QAngle &vAngles,
	int	iMode,
	int iSeed,
	float fInaccuracy,
	float fSpread,
	float fAccuracyFishtail,
	float flSoundTime,
	WeaponSound_t sound_type,
	float flRecoilIndex
	)
{
	bool bDoEffects = true;

	if ( fInaccuracy > 1.0f )
		fInaccuracy = 1.0f;

	if ( !pWeaponView )
	{
		// Can't fire with no weapon or view
		if ( !pWeapon )
			return;

		pWeaponView = pWeapon->GetEconItemView();

		// Can't fire if weapon isn't valid
		if ( !pWeaponView || !pWeaponView->IsValid() )
			return;
	}
	Assert( pWeaponView && pWeaponView->IsValid() );

#ifdef CLIENT_DLL
	C_CSPlayer *pPlayer = ToCSPlayer( ClientEntityList().GetBaseEntity( iEntIndex ) );
#else
	CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iEntIndex ) );
#endif

	if ( !pPlayer )
	{
		// probably an env_gunfire
#ifndef CLIENT_DLL
		// if this is server code, send the effect over to client as temp entity
		// Dispatch one message for all the bullet impacts and sounds.
		TE_FireBullets( 
			-1,
			pWeapon,
			pWeaponView,
			vOrigin, 
			vAngles, 
			iMode,
			iSeed,
			fInaccuracy,
			fSpread,
			fAccuracyFishtail,
			sound_type,
			flRecoilIndex
			);
#endif

		// Don't do damage here, leave it to the 
		return;
	}

#ifdef CLIENT_DLL
	CWeaponCSBase* pClientWeapon = pPlayer ? pPlayer->GetActiveCSWeapon() : NULL;
	if ( pClientWeapon )
	{
		if ( gpGlobals->curtime - pClientWeapon->m_flLastClientFireBulletTime > 0.02f ) // this should be enough even for the negev, at ~1000 rof
		{
			pClientWeapon->m_flLastClientFireBulletTime = gpGlobals->curtime;
		}
		else
		{
			return; // we already traced this shot on the client!
		}
	}
#endif

	QAngle adjustedAngles = vAngles;
	adjustedAngles.y += fAccuracyFishtail;

	if ( pPlayer && debug_aim_angle.GetBool() )
	{
		QAngle old = pPlayer->EyeAngles() + pPlayer->GetAimPunchAngle();
#ifdef CLIENT_DLL
		DevMsg("Client ");
#else
		DevMsg("Server ");
#endif
		DevMsg("old: %f %f new: %f %f\n",
			old[YAW], old[PITCH],
			vAngles[YAW], vAngles[PITCH]
			);
		if ( debug_aim_angle.GetInt() == 2 )
		{
			adjustedAngles = old;
		}
	}

	const CEconItemDefinition* pItemDef = pWeaponView->GetStaticData();
	if ( !pItemDef )
	{
		DevMsg( "FX_FireBullets: GetItemDefinition failed\n" );
		return;
	}

#if !defined(CLIENT_DLL)
	if ( weapon_accuracy_logging.GetBool() )
	{
		char szFlags[256];

		V_strcpy(szFlags, " ");

// #if defined(CLIENT_DLL)
// 		V_strcat(szFlags, "CLIENT ", sizeof(szFlags));
// #else
// 		V_strcat(szFlags, "SERVER ", sizeof(szFlags));
// #endif
// 
		if ( pPlayer->GetMoveType() == MOVETYPE_LADDER )
			V_strcat(szFlags, "LADDER ", sizeof(szFlags));

		if ( FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
			V_strcat(szFlags, "GROUND ", sizeof(szFlags));

		if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING) )
			V_strcat(szFlags, "DUCKING ", sizeof(szFlags));

		float fVelocity = pPlayer->GetAbsVelocity().Length2D();

		Msg("FireBullets @ %10f [ %s ]: inaccuracy=%f  spread=%f  max dispersion=%f  mode=%2i  vel=%10f  seed=%3i  %s\n", 
			gpGlobals->curtime, pItemDef->GetItemBaseName(), fInaccuracy, fSpread, fInaccuracy + fSpread, iMode, fVelocity, iSeed, szFlags);
	}
#endif

	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfoFromItem( pWeaponView );
	if ( !pWeaponInfo )
	{
		DevMsg( "FX_FireBullets: GetFileWeaponInfoFromHandle failed for weapon %s\n", pItemDef->GetItemBaseName() );
		return;
	}

	// Do the firing animation event.
#ifndef CLIENT_DLL
	if ( pPlayer && !pPlayer->IsDormant() )
	{
		if ( iMode == Primary_Mode )
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );
		else
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_SECONDARY );
	}
#endif // CLIENT_DLL


#ifdef CLIENT_DLL
	if ( pPlayer && pPlayer->m_bUseNewAnimstate )
	{
		pPlayer->ProcessMuzzleFlashEvent();
	}
#endif


#ifndef CLIENT_DLL
	// if this is server code, send the effect over to client as temp entity
	// Dispatch one message for all the bullet impacts and sounds.
	TE_FireBullets(
		pPlayer->entindex(),
		pWeapon,
		pWeaponView,
		vOrigin,
		vAngles,
		iMode,
		iSeed,
		fInaccuracy,
		fSpread,
		fAccuracyFishtail,
		sound_type,
		flRecoilIndex
		);

	// Let the player remember the usercmd he fired a weapon on. Assists in making decisions about lag compensation.
	if ( pPlayer )
		pPlayer->NoteWeaponFired();

	bDoEffects = false; // no effects on server
#endif

	iSeed++;

	int		iDamage = pWeaponInfo->GetDamage( pWeaponView );
	float	flRange = pWeaponInfo->GetRange( pWeaponView );
	float	flPenetration = pWeaponInfo->GetPenetration( pWeaponView );
	float	flRangeModifier = pWeaponInfo->GetRangeModifier( pWeaponView );
	
	int		iAmmoType = pWeaponInfo->GetPrimaryAmmoType( pWeaponView );

	if ( bDoEffects)
	{
		const float MaxPitchShiftInaccuracy = snd_max_pitch_shift_inaccuracy.GetFloat();
		float flPitchShift = pWeaponInfo->GetInaccuracyPitchShift( pWeaponView ) * (fInaccuracy < MaxPitchShiftInaccuracy ? fInaccuracy : MaxPitchShiftInaccuracy);

		if ( sound_type == SINGLE && pWeaponInfo->GetInaccuracyAltSoundThreshold( pWeaponView ) > 0.0f && fInaccuracy < pWeaponInfo->GetInaccuracyAltSoundThreshold( pWeaponView ) )
		{
			sound_type = SINGLE_ACCURATE;
			flPitchShift = 0.0f;
		}

		FX_WeaponSound( pPlayer->entindex(), pWeaponView, sound_type, vOrigin, flSoundTime, PITCH_NORM + int(flPitchShift) );

		// If the gun's nearly empty, also play a subtle "nearly-empty" sound, since the weapon 
		// is lighter and acoustically different when weighed down by fewer bullets.
		// But really it's so you get a fun low ammo warning from an audio cue.
		if ( weapon_near_empty_sound.GetBool() &&
			 pWeapon && pWeapon->GetMaxClip1() > 1 && // not a single-shot weapon
			 (((float)pWeapon->m_iClip1) / ((float)pWeapon->GetMaxClip1()) <= 0.2) ) // 20% or fewer bullets remaining
		{
			FX_WeaponSound( pPlayer->entindex(), pWeaponView, NEARLYEMPTY, vOrigin, flSoundTime, PITCH_NORM );
		}
	}

	// Fire bullets, calculate impacts & effects

	if ( !pPlayer )
		return;
	
	StartGroupingSounds();

#ifdef GAME_DLL
	pPlayer->StartNewBulletGroup();
#endif

#if !defined (CLIENT_DLL)
	// Move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation( pPlayer, LAG_COMPENSATE_HITBOXES_ALONG_RAY, vOrigin, vAngles, flRange );
#endif

	// [sbodenbender] rumble when shooting
	// since we are handling bullet fx in CS differently than other titles, call 
	// rumble effect directly instead of Player::RumbleEffect
	//=============================================================================

		
#if defined (CLIENT_DLL)
	if ( pPlayer && pPlayer->IsLocalPlayer() && pWeaponInfo->GetBullets( pWeaponView ) > 0 )
	{
		int rumbleEffect = pWeaponInfo->GetRumbleEffect( pWeaponView );
		if( rumbleEffect != RUMBLE_INVALID )
		{
			RumbleEffect( XBX_GetUserId( pPlayer->GetSplitScreenPlayerSlot() ), rumbleEffect, 0, RUMBLE_FLAG_RESTART );
		}
		if ( rumbleEffect != RUMBLE_INVALID && rumbleEffect <= 6 && steam_controller_haptics.GetBool() && g_pInputSystem->IsSteamControllerActive() && steamapicontext->SteamController() )
		{
			ControllerHandle_t handles[MAX_STEAM_CONTROLLERS];
			int nControllers = steamapicontext->SteamController()->GetConnectedControllers( handles );
	
			for ( int i = 0; i < nControllers; ++i )
			{
				steamapicontext->SteamController()->TriggerHapticPulse( handles[ i ], k_ESteamControllerPad_Right, (2000*rumbleEffect)/5 );
				steamapicontext->SteamController()->TriggerHapticPulse( handles[ i ], k_ESteamControllerPad_Left, (2000*rumbleEffect)/5 );
			}
		}
	}
#endif

	bool bForceMaxInaccuracy = weapon_debug_max_inaccuracy.GetBool();
	bool bForceInaccuracyDirection = weapon_debug_inaccuracy_only_up.GetBool();

	static const CSchemaItemDefHandle hRevolver( "weapon_revolver" ); // $$$REI should be able to check type more easily than this?
	static const CSchemaItemDefHandle hNegev( "weapon_negev" ); // $$$REI should be able to check type more easily than this?

	RandomSeed( iSeed );	// init random system with this seed

	const int kMaxBullets = 16;
	float fTheta0 = 0.0f, fRadius0 = 0.0f; // Initialize here to prevent spurious 'potentially unused local variable' warnings
	float flBulletX[kMaxBullets], flBulletY[kMaxBullets];
	int nBullets = pWeaponInfo->GetBullets( pWeaponView );
	Assert( nBullets <= kMaxBullets );

	// the RNG can be desynchronized by FireBullet(), so pre-generate all spread offsets
	for ( int iBullet = 0; iBullet < nBullets; iBullet++ )
	{
		// Initialize on first loop, or every time with the new spread model
		if ( iBullet == 0 || weapon_accuracy_shotgun_spread_patterns.GetBool() )
		{
			// Get inaccuracy magnitude
			float flInaccuracyDensity = RandomFloat();
			fTheta0 = RandomFloat( 0.0f, 2.0f * M_PI );

			// Accuracy curve density adjustment FOR R8 REVOLVER SECONDARY FIRE, NEGEV WILD BEAST
			if ( pWeaponView->GetItemDefinition() == hRevolver && iMode == Secondary_Mode ) /*R8 REVOLVER SECONDARY FIRE*/
			{
				flInaccuracyDensity = 1.0f - flInaccuracyDensity*flInaccuracyDensity;
			}
			else if ( pWeaponView->GetItemDefinition() == hNegev && flRecoilIndex < 3 ) /*NEGEV WILD BEAST*/
			{
				for ( int j = 3; j > flRecoilIndex; --j )
				{
					flInaccuracyDensity *= flInaccuracyDensity;
				}
				flInaccuracyDensity = 1.0f - flInaccuracyDensity;
			}

			if ( bForceMaxInaccuracy )
				flInaccuracyDensity = 1.0f;

			fRadius0 = flInaccuracyDensity * fInaccuracy;

			// Get inaccuracy angle
			if ( bForceInaccuracyDirection )
				fTheta0 = M_PI * 0.5f;
		}

		float fTheta1, flSpreadDensity;

		if ( weapon_accuracy_shotgun_spread_patterns.GetBool() )
		{
			extern WeaponRecoilData g_WeaponRecoilData;
			g_WeaponRecoilData.GetSpreadOffsets( pItemDef->GetDefinitionIndex(), iMode, iBullet + ((int)flRecoilIndex) * nBullets, fTheta1, flSpreadDensity );
		}
		else
		{
			flSpreadDensity = RandomFloat();
			fTheta1 = RandomFloat( 0.0f, 2.0f * M_PI );
		}

		// Spread curve density adjustment for R8 REVOLVER SECONDARY FIRE, NEGEV WILD BEAST
		if ( pWeaponView->GetItemDefinition() == hRevolver && iMode == Secondary_Mode )
		{
			flSpreadDensity = 1.0f - flSpreadDensity*flSpreadDensity;
		}
		if ( pWeaponView->GetItemDefinition() == hNegev && flRecoilIndex < 3 ) /*NEGEV WILD BEAST*/
		{
			for ( int j = 3; j > flRecoilIndex; --j )
			{
				flSpreadDensity *= flSpreadDensity;
			}
			flSpreadDensity = 1.0f - flSpreadDensity;
		}

		if ( bForceMaxInaccuracy )
			flSpreadDensity = 1.0f;

		if ( bForceInaccuracyDirection )
			fTheta1 = M_PI * 0.5f;

		float fRadius1 = flSpreadDensity * fSpread;

		flBulletX[iBullet] = fRadius0 * cosf( fTheta0 ) + fRadius1 * cosf( fTheta1 );
		flBulletY[iBullet] = fRadius0 * sinf( fTheta0 ) + fRadius1 * sinf( fTheta1 );
	}

#if !defined( CLIENT_DLL )
	{	/// Make sure take damage listener stays in scope only for the duration of FireBullet loop below!
	class CFireBulletTakeDamageListener : public CCSPlayer::ITakeDamageListener
	{
	public:
		CFireBulletTakeDamageListener( CCSPlayer *pPlayerShooting ) :
			m_pPlayerShooting(pPlayerShooting),
			m_bEnemyHit( false ),
			m_bShotFiredAndOnTargetRecorded( false )
		{}
		virtual void OnTakeDamageListenerCallback( CCSPlayer *pVictim, CTakeDamageInfo &infoTweakable ) OVERRIDE
		{
			if ( m_pPlayerShooting && pVictim->IsOtherEnemy( m_pPlayerShooting ) )
			{
				m_bEnemyHit = true;

				if ( infoTweakable.GetDamageType() & DMG_HEADSHOT )
				{
					m_rbHsPlayers.InsertIfNotFound( pVictim );	// remember that at least one pellet hit a headshot
				}
				else if ( m_rbHsPlayers.Find( pVictim ) != m_rbHsPlayers.InvalidIndex() )
				{
#if 0
					DevMsg( "DMG: Pellet modified for headshot visualization %s -> %s = (0x%08X +hs)\n",
						m_pPlayerShooting ? m_pPlayerShooting->GetPlayerName() : "[unknown]",
						pVictim->GetPlayerName(), infoTweakable.GetDamageType() );
#endif
					infoTweakable.SetDamageType( infoTweakable.GetDamageType() | DMG_HEADSHOT );	// since previous pellets hit a headshot we visualize it as a headshot
				}

				// Since we know that bullet was fired and that we hit the target
				// we should record the accuracy stats right now, otherwise we may TerminateRound
				// based on a kill from this bullet and not have this data recorded
				RecordShotFiredAndOnTargetData();
			}
		}
		void BulletBurstCompleted()
		{
			RecordShotFiredAndOnTargetData();
		}

		bool DidHitEnemy() const
		{
			return m_bEnemyHit;
		}
	private:
		void RecordShotFiredAndOnTargetData()
		{
			if ( m_bShotFiredAndOnTargetRecorded )
				return;
			m_bShotFiredAndOnTargetRecorded = true;

			if ( m_pPlayerShooting && CSGameRules() && !CSGameRules()->IsWarmupPeriod() && !m_pPlayerShooting->IsBot() )
			{
				// Track in QMM total number of shots that connected with an opponent
				if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_pPlayerShooting->GetHumanPlayerAccountID() ) )
				{
					++pQMM->m_numShotsFiredTotal;
					if ( m_bEnemyHit )
						++pQMM->m_numShotsOnTargetTotal;
				}
			}
		}
	private:
		CCSPlayer *m_pPlayerShooting;
		bool m_bEnemyHit;
		bool m_bShotFiredAndOnTargetRecorded;
		CUtlRBTree< CCSPlayer *, int, CDefLess< CCSPlayer * > > m_rbHsPlayers;	// players who were dinked in the head as part of this bullet batch
	} fbtdl( pPlayer );
#endif

	for ( int iBullet = 0; iBullet < pWeaponInfo->GetBullets( pWeaponView ); iBullet++ )
	{
		if ( !pPlayer )
			break;

		int nPenetrationCount = 4;

		pPlayer->FireBullet(
			vOrigin,
			adjustedAngles,
			flRange,
			flPenetration,
			nPenetrationCount,
			iAmmoType,
			iDamage,
			iBullet,
			flRangeModifier,
			pPlayer,
			bDoEffects,
			flBulletX[iBullet], flBulletY[iBullet]
			);
	}

#if !defined( CLIENT_DLL )
	fbtdl.BulletBurstCompleted();

	// Self-damage if we missed
	extern ConVar mp_weapon_self_inflict_amount;
	if ( pPlayer && pWeapon
		&& !fbtdl.DidHitEnemy() && mp_weapon_self_inflict_amount.GetFloat() > 0.0f
		// hack: tasers don't do self-damage
		&& pWeaponInfo->GetWeaponID( pWeaponView ) != WEAPON_TASER
		)
	{
		// Set buddha-mode on the player to prevent them from dying from this damage.
		// This will allow their health to reduce to 1 but no lower.
		// This solves some problems with self_inflict as a game mode:
		// (1) Frustrating to feel like you can't shoot at all because you will probably die
		// (2) Lots of code treats suicide in a way that causes unintended effects, for example:
		//     - awarding money to the wrong player
		//     - subtracting contribution score (= xp)
		//     - being kicked from the server for too many suicides

		// We need to remember the current state so we can restore it after we are done dealing damage.
		bool wasBuddha = ( pPlayer->m_debugOverlays & OVERLAY_BUDDHA_MODE ) != 0;

		if(!wasBuddha)
			pPlayer->m_debugOverlays |= OVERLAY_BUDDHA_MODE;

		// DMG_PREVENT_PHYSICS_FORCE disables tagging from your own shots.
		// It also can be used to disable the self-damage marker but we are going to test with that enabled for the next playtest.
		CTakeDamageInfo info( pPlayer, pPlayer, pWeapon->GetDamage() * pWeaponInfo->GetBullets( pWeapon->GetEconItemView() ) * mp_weapon_self_inflict_amount.GetFloat(), DMG_PREVENT_PHYSICS_FORCE );
		pPlayer->OnTakeDamage( info );

		if(!wasBuddha)
			pPlayer->m_debugOverlays &= ~OVERLAY_BUDDHA_MODE;
	}

	} /// Closes the lifetime scope of take damage listener in scope only for the duration of FireBullet loop above.
#endif

#if !defined (CLIENT_DLL)
	lagcompensation->FinishLagCompensation( pPlayer );
#endif

	EndGroupingSounds();
}

// This runs on both the client and the server.
// On the server, it dispatches a TE_PlantBomb to visible clients.
// On the client, it plays the planting animation.
void FX_PlantBomb( int iPlayerIndex, const Vector &vOrigin, PlantBombOption_t option )
{
#ifdef CLIENT_DLL
	C_CSPlayer *pPlayer = ToCSPlayer( ClientEntityList().GetBaseEntity( iPlayerIndex ) );
#else
	CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayerIndex) );
#endif

	// Do the firing animation event.
	if ( pPlayer && !pPlayer->IsDormant() )
	{
		switch ( option )
		{
		case PLANTBOMB_PLANT:
			{
				pPlayer->DoAnimStateEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );
			}
			break;

		case PLANTBOMB_ABORT:
			{
				pPlayer->DoAnimStateEvent( PLAYERANIMEVENT_CLEAR_FIRING );
			}
			break;
		}
	}

#ifndef CLIENT_DLL
	// if this is server code, send the effect over to client as temp entity
	// Dispatch one message for all the bullet impacts and sounds.
	TE_PlantBomb( iPlayerIndex, vOrigin, option );
#endif
}

