//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "c_plantedc4.h"
#include "engine/IEngineSound.h"
#include "cs_gamerules.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include <bitbuf.h>
#include "precache_register.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PLANTEDC4_MSG_JUSTBLEW 1
#define TIME_TO_DETONATE_WARNING 10

extern ConVar spec_show_xray;

IMPLEMENT_CLIENTCLASS_DT(C_PlantedC4, DT_PlantedC4, CPlantedC4)
	RecvPropBool( RECVINFO(m_bBombTicking) ),
	RecvPropInt( RECVINFO(m_nBombSite) ),
	RecvPropFloat( RECVINFO(m_flC4Blow) ),
	RecvPropFloat( RECVINFO(m_flTimerLength) ),
	RecvPropFloat( RECVINFO(m_flDefuseLength) ),
	RecvPropFloat( RECVINFO(m_flDefuseCountDown) ),
	RecvPropBool( RECVINFO(m_bBombDefused) ),
	RecvPropEHandle( RECVINFO(m_hBombDefuser) ),
END_RECV_TABLE()

// PRECACHE_REGISTER_BEGIN( GLOBAL, C_PlantedC4 )
// PRECACHE( PARTICLE_SYSTEM, "bomb_explosion_huge" )
// PRECACHE_REGISTER_END()

CUtlVector< C_PlantedC4* > g_PlantedC4s;

C_PlantedC4::C_PlantedC4() :
	m_GlowObject( this, Vector( 1.0f, 1.0f, 1.0f ), 0.0f, false, false )
{
	g_PlantedC4s.AddToTail( this );

	m_flNextRadarFlashTime = gpGlobals->curtime;
	m_bRadarFlash = true;

	// Don't beep right away, leave time for the planting sound
	m_flNextGlow = gpGlobals->curtime + 1.0;
	m_flNextBeep = gpGlobals->curtime + 1.0;

	m_hLocalDefusingPlayerHandle = NULL;
}


C_PlantedC4::~C_PlantedC4()
{
	g_PlantedC4s.FindAndRemove( this );
}

void C_PlantedC4::SetDormant( bool bDormant )
{
	BaseClass::SetDormant( bDormant );
	
	// Remove us from the list of planted C4s.
	if ( bDormant )
	{
		g_PlantedC4s.FindAndRemove( this );
		m_bTenSecWarning = false;
	}
	else
	{
		if ( g_PlantedC4s.Find( this ) == -1 )
			g_PlantedC4s.AddToTail( this );
	}
}

void C_PlantedC4::UpdateOnRemove( void )
{
	DestroyDefuserRopes();
	BaseClass::UpdateOnRemove();
}

void C_PlantedC4::Spawn( void )
{
	BaseClass::Spawn();

	AddFlag( FL_OBJECT );

	SetNextClientThink( CLIENT_THINK_ALWAYS );

	m_bTenSecWarning = false;
	m_bExplodeWarning = false;
	m_bTriggerWarning = false;

	m_bLocalBRMusicPlayed = false;

	DestroyDefuserRopes();
}

void C_PlantedC4::DestroyDefuserRopes( void )
{
	FOR_EACH_VEC_BACK( m_hDefuserRopes, i )
	{
		if ( m_hDefuserRopes[i] )
			UTIL_Remove( m_hDefuserRopes[i] );
	}
	m_hDefuserRopes.RemoveAll();
	
	if ( m_hDefuserMultimeter )
		UTIL_Remove( m_hDefuserMultimeter );
}

bool C_PlantedC4::CreateDefuserRopes( void )
{
	DestroyDefuserRopes();

	// make sure we are being defused
	C_CSPlayer *pDefusingPlayer = m_hBombDefuser->Get();
	if ( !pDefusingPlayer )
	{
		DevWarning( "Cannot find defusing player to create bomb defuse wire.\n" );
		return false;
	}

	if ( pDefusingPlayer->IsDormant() )
		return false;

	if ( !m_hDefuserMultimeter )
	{
		C_BaseAnimating *pMultimeter = new C_BaseAnimating;
		if ( pMultimeter->InitializeAsClientEntity( "models/weapons/w_eq_multimeter.mdl", false ) )
		{
			pMultimeter->AddSolidFlags( FSOLID_NOT_SOLID );
			pMultimeter->AddEffects( EF_BONEMERGE );
			pMultimeter->SetParent( pDefusingPlayer );
			m_hDefuserMultimeter = pMultimeter;
		}
		else
		{
			m_hDefuserMultimeter = NULL;
		}
	}

	if ( !m_hDefuserMultimeter )
	{
		DevWarning( "Could not create defuser tool model.\n" );
		return false;
	}

	// validate attachments
	int nDefuseToolAttachmentA = m_hDefuserMultimeter->LookupAttachment("weapon_defusewire_a");
	int nDefuseToolAttachmentB = m_hDefuserMultimeter->LookupAttachment("weapon_defusewire_b");

	int nBombClipAttachmentA = LookupAttachment("weapon_defusewire_a");
	int nBombClipAttachmentB = LookupAttachment("weapon_defusewire_b");

	if ( nDefuseToolAttachmentA < 0 ||
		 nDefuseToolAttachmentB < 0 ||
		 nBombClipAttachmentA < 0 ||
		 nBombClipAttachmentB < 0 )
	{
		DevWarning( "Could not locate attachment for bomb defuse wire.\n" );
		return false;
	}

	// create the wires
	C_RopeKeyframe* pRopeA = C_RopeKeyframe::Create( this, m_hDefuserMultimeter, nBombClipAttachmentA, nDefuseToolAttachmentA, 0.9, "cable/phonecable", 8, ROPE_SIMULATE, ROPE_TYPE_DEFUSECABLES );
	if ( pRopeA )
	{
		pRopeA->AddRopeFlags( ROPE_USE_WIND );
		pRopeA->SetSlack( 142 );
		pRopeA->AddToLeafSystem();
		m_hDefuserRopes[ m_hDefuserRopes.AddToTail() ] = pRopeA;
		//pRopeA->AddEffects( EF_NORECEIVESHADOW );
	}
	else
	{
		DevWarning( "Failed to create bomb defuse wire.\n" );
		return false;
	}

	C_RopeKeyframe* pRopeB = C_RopeKeyframe::Create( this, m_hDefuserMultimeter, nBombClipAttachmentB, nDefuseToolAttachmentB, 0.9, "cable/phonecable_red", 8, ROPE_SIMULATE, ROPE_TYPE_DEFUSECABLES );
	if ( pRopeB )
	{
		pRopeB->AddRopeFlags( ROPE_USE_WIND );
		pRopeB->SetSlack( 142 );
		pRopeB->AddToLeafSystem();
		m_hDefuserRopes[ m_hDefuserRopes.AddToTail() ] = pRopeB;
		//pRopeA->AddEffects( EF_NORECEIVESHADOW );
	}
	else
	{
		DevWarning( "Failed to create bomb defuse wire.\n" );
		return false;
	}
	
	m_hLocalDefusingPlayerHandle = m_hBombDefuser->Get(); // remember this player handle in case it changes

	return true;
}

void C_PlantedC4::ClientThink( void )
{
	BaseClass::ClientThink();

	C_CSPlayer *pDefusingPlayer = m_hBombDefuser->Get();

	if ( m_hBombDefuser->Get() != m_hLocalDefusingPlayerHandle )
		DestroyDefuserRopes(); // we're still being defused, but the defusing player changed

	C_CSPlayer *pLocalPlayer = GetHudPlayer();
	if ( pDefusingPlayer && ( pDefusingPlayer != pLocalPlayer || pLocalPlayer->ShouldDraw() ) )
	{
		if ( m_hDefuserRopes.Count() == 0 )
		{
			CreateDefuserRopes();
		}
	}
	else
	{
		DestroyDefuserRopes();
	}

	// If it's dormant, don't beep or anything..
	if ( IsDormant() )
		return;

	if ( !m_bLocalBRMusicPlayed && CSGameRules() && CSGameRules()->IsPlayingSurvival() && pLocalPlayer )
	{
		// in survival you only get tense bomb music if you're close enough
		bool bCloseEnoughToPlayBombMusic = (pLocalPlayer->GetAbsOrigin().DistTo( GetAbsOrigin() ) < 1000);

		if ( bCloseEnoughToPlayBombMusic )
		{
			CLocalPlayerFilter filter;
			PlayMusicSelection( filter, CSMUSIC_BOMB );
			m_bLocalBRMusicPlayed = true;
		}
	}

	bool bPlayingSurvival = CSGameRules() && CSGameRules()->IsPlayingSurvival();

	Vector glowColor;
	bool bRenderGlow = (CanSeeSpectatorOnlyTools() && spec_show_xray.GetInt() && !bPlayingSurvival);

	if ( !m_bBombTicking )
	{
		// disable C4 thinking if not armed
		// undone.  we need the bome to resume thinking once it's been actiavted (for training) - mtw
		//SetNextClientThink( CLIENT_THINK_NEVER );

		if ( m_bBombDefused )
		{
			glowColor.x = (64.0f/255.0f);
			glowColor.y = (255.0f/255.0f);
			glowColor.z = (64.0f/255.0f);
			m_GlowObject.SetRenderFlags( bRenderGlow, false );
			m_GlowObject.SetColor( glowColor );
			m_GlowObject.SetAlpha( bRenderGlow ? 0.95f : 0.0f );
		}

		return;
	}

	float flPredictedDamage = 0;
	if ( pLocalPlayer && CSGameRules() && CSGameRules()->IsPlayingSurvival() )
	{
		// magic numbers copied from csgamerules
		float fDist = pLocalPlayer->GetAbsOrigin().DistTo( GetAbsOrigin() );
		float fSigma = 1750.0f / 3.0f; // flRadius ( 1750.0f ) specifies 3rd standard deviation (0.0111 damage at this range)
		float fGaussianFalloff = exp( -fDist * fDist / (2.0f * fSigma * fSigma) );
		flPredictedDamage = 500.0f * fGaussianFalloff; // 500 is hardcoded bomb damage from all over
	}

	if( ( m_flC4Blow - gpGlobals->curtime ) < 1.0f &&
		!m_bTriggerWarning )
	{

		if ( CSGameRules() && CSGameRules()->IsPlayingSurvival() )
		{

			CLocalPlayerFilter filter;
			CSoundParameters params;
			EmitSound_t ep( params );
			if ( GetParametersForSound( "C4.ExplodeTriggerTrip", params, NULL ) )
			{
				if ( flPredictedDamage < 1 )
				{
					ep.m_SoundLevel = SNDLVL_85dB;
				}
				else
				{
					ep.m_SoundLevel = (soundlevel_t)(int)(RemapValClamped( flPredictedDamage, 1, 100, SNDLVL_90dB, SNDLVL_150dB ));
				}
				
				EmitSound( filter, SOUND_FROM_WORLD, ep );
			}

		}
		else
		{
			EmitSound( "C4.ExplodeTriggerTrip" );
		}

		int ledAttachmentIndex = LookupAttachment("led");
		DispatchParticleEffect( "c4_timer_light_trigger", PATTACH_POINT_FOLLOW, this, ledAttachmentIndex, false, -1 );

		m_bTriggerWarning = true;
	}

	if( ( m_flC4Blow - gpGlobals->curtime ) < 0.0f &&
		!m_bExplodeWarning )
	{
		if ( CSGameRules() && CSGameRules()->IsPlayingSurvival() )
		{

			CLocalPlayerFilter filter;
			CSoundParameters params;
			EmitSound_t ep( params );
			if ( GetParametersForSound( "C4.ExplodeWarning", params, NULL ) )
			{
				if ( flPredictedDamage < 1 )
				{
					ep.m_SoundLevel = SNDLVL_85dB;
				}
				else
				{
					ep.m_SoundLevel = (soundlevel_t)(int)(RemapValClamped( flPredictedDamage, 1, 100, SNDLVL_90dB, SNDLVL_150dB ));
				}

				EmitSound( filter, SOUND_FROM_WORLD, ep );
			}

		}
		else
		{
			EmitSound( "C4.ExplodeWarning" );
		}

		m_bExplodeWarning = true;
	}

	if( gpGlobals->curtime > m_flNextBeep )
	{
		// as it gets closer to going off, increase the radius

		CLocalPlayerFilter filter;
		float attenuation;
		float freq;

		//the percent complete of the bomb timer
		float fComplete = ( ( m_flC4Blow - gpGlobals->curtime ) / m_flTimerLength );
		
		fComplete = clamp( fComplete, 0.0f, 1.0f );

		attenuation = MIN( 0.3 + 0.6 * fComplete, 1.0 );
		
		CSoundParameters params;

		if ( ( m_flC4Blow - gpGlobals->curtime ) > 1.0f && GetParametersForSound( GetBombSite()%2 == 1 ? "C4.PlantSoundB" : "C4.PlantSound", params, NULL ) )
		{
			EmitSound_t ep( params );
			
			if ( CSGameRules() && CSGameRules()->IsPlayingSurvival() )
			{
				// scale the beep loudness by predicted damage

				if ( flPredictedDamage < 1 )
				{
					ep.m_SoundLevel = SNDLVL_85dB;
				}
				else
				{
					ep.m_SoundLevel = (soundlevel_t)(int)(RemapValClamped( flPredictedDamage, 1, 100, SNDLVL_90dB, SNDLVL_150dB ));
				}

				//Msg( "Predicted damage: %.2f, SoundLevel: %i dB\n", flPredictedDamage, (int)ep.m_SoundLevel );
			}
			else
			{
				ep.m_SoundLevel = ATTN_TO_SNDLVL( attenuation );
			}

			ep.m_pOrigin = &GetAbsOrigin();

			EmitSound( filter, SOUND_FROM_WORLD, ep );
		}

		freq = MAX( 0.1 + 0.9 * fComplete, 0.15 );

		m_flNextBeep = gpGlobals->curtime + freq;
	}
	if( ( m_flC4Blow - gpGlobals->curtime ) <= TIME_TO_DETONATE_WARNING &&
		!m_bTenSecWarning )
	{
		m_bTenSecWarning = true;
		
		if ( !CSGameRules()->IsPlayingTraining() )
		{
			bool bCloseEnoughToPlayBombMusic = true;

			if ( CSGameRules() && CSGameRules()->IsPlayingSurvival() && pLocalPlayer )
			{
				// in survival you only get tense bomb music if you're close enough
				bCloseEnoughToPlayBombMusic = (pLocalPlayer->GetAbsOrigin().DistTo( GetAbsOrigin() ) < 2000);
			}

			if ( bCloseEnoughToPlayBombMusic )
			{
				CLocalPlayerFilter filter;
				PlayMusicSelection( filter, CSMUSIC_BOMBTEN );
				m_bLocalBRMusicPlayed = true;
			}
		}
	}

	glowColor.x = (170.0f/255.0f);
	glowColor.y = (0.0f/255.0f);
	glowColor.z = (0.0f/255.0f);

	if ( (gpGlobals->curtime + 0.25f) > m_flNextGlow )
		glowColor.x = (255.0f/255.0f);

	if ( gpGlobals->curtime > m_flNextGlow )
	{
		if ( gpGlobals->curtime > m_flNextGlow && ( m_flC4Blow - gpGlobals->curtime ) > 1.0f )
		{
			int ledAttachmentIndex = LookupAttachment("led");
			DispatchParticleEffect( "c4_timer_light", PATTACH_POINT_FOLLOW, this, ledAttachmentIndex, false, -1 );
		}

		float freq = 0.1 + 0.9 * ( ( m_flC4Blow - gpGlobals->curtime ) / m_flTimerLength );

		if( freq < 0.15 ) freq = 0.15;

		m_flNextGlow = gpGlobals->curtime + freq;
	}	

	// Start glowing
	m_GlowObject.SetRenderFlags( bRenderGlow, false );
	m_GlowObject.SetColor( glowColor );
	m_GlowObject.SetAlpha( bRenderGlow ? 0.95f : 0.0f );
}

// Using a new particle system, networked down from the server (KWD)
void C_PlantedC4::Explode( void )
{
	// don't hide it in training, we have a particle effect that covers it up and then eventally it all gets removed
	if ( !CSGameRules()->IsPlayingTraining() )
		AddEffects( EF_NODRAW );

	SetDormant( true );
}

float C_PlantedC4::GetDetonationProgress( void )
{
	float fComplete = 0.0f;
	if ( m_flTimerLength > 0.0f )
	{
		fComplete = ( ( m_flC4Blow - gpGlobals->curtime ) / m_flTimerLength );
	}

	return fComplete;
}
