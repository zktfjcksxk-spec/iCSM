//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "debugoverlay.h"

#include "sos_op.h"
#include "sos_op_occlusion.h"
#include "vphysics_interface.h"

#include "snd_dma.h"
#include "../../cl_splitscreen.h"
#include "../../enginetrace.h"
#include "render.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"




extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;

float S_CalcOcclusion( int nSlot, channel_t *ch, const Vector &vecListenerOrigin, Vector vSoundSource, float flOccludedDBLoss );


//-----------------------------------------------------------------------------
// CSosOperatorOcclusion
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorOcclusion, "calc_occlusion" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorOcclusion, m_flInputTraceInterval, SO_SINGLE, "input_trace_interval" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorOcclusion, m_flInputScalar, SO_SINGLE, "input_scalar" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorOcclusion, m_flInputPosition, SO_VEC3, "input_position" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorOcclusion, m_flOutput, SO_SINGLE, "output" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorOcclusion, "calc_occlusion"  )

void CSosOperatorOcclusion::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorOcclusion_t *pStructMem = (CSosOperatorOcclusion_t *)pVoidMem;

	SOS_INIT_INPUT_VAR( m_flInputScalar, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInputPosition, SO_VEC3, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 1.0 )

	SOS_INIT_INPUT_VAR( m_flInputTraceInterval, SO_SINGLE, -1.0 )
	pStructMem->m_flLastTraceTime = -1.0;
	pStructMem->m_flOccludedDBLoss = snd_obscured_gain_db.GetFloat();
	
}

void CSosOperatorOcclusion::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorOcclusion_t *pStructMem = (CSosOperatorOcclusion_t *)pVoidMem;

	if( !pChannel )
	{
		Log_Warning( LOG_SND_OPERATORS, "Error: Sound operator %s requires valid channel pointer, being called without one\n", pStack->GetOperatorName( nOpIndex ));
		return;
	}


	// scalar of 0.0 = off, do nothing
	float flScalar = pStructMem->m_flInputScalar[0];
	if( flScalar == 0.0 )
	{
		pStructMem->m_flOutput[0] = 1.0;
		return;
	}

	float flCurHostTime = g_pSoundServices->GetHostTime();


	bool bIntervalHasPassed = ( ( pStructMem->m_flInputTraceInterval[0] >= 0.0 &&
								( pStructMem->m_flInputTraceInterval[0] <= ( flCurHostTime - pStructMem->m_flLastTraceTime ) ))
								|| pChannel->flags.bfirstpass );			
								
	bool bOkToTrace = SND_ChannelOkToTrace( pChannel );

	bool bDoNewTrace = true;

	// During signon just apply regular state machine since world hasn't been
	//  created or settled yet...
	if ( !SND_IsInGame() && !toolframework->InToolMode() )
	{
		bDoNewTrace = false;
	}

	if( ( !pChannel->flags.bfirstpass && !pChannel->flags.isSentence ) && !bIntervalHasPassed || !bOkToTrace )
	{
		bDoNewTrace = false;
	}

	float flResult = 0.0;
	if( bDoNewTrace )
	{

// 		Log_Msg( LOG_SND_OPERATORS, "UPDATING: Sound operator %s\n", pStack->GetOperatorName( nOpIndex ));

		Vector vOrigin;
		vOrigin[0] = pStructMem->m_flInputPosition[0];
		vOrigin[1] = pStructMem->m_flInputPosition[1];
		vOrigin[2] = pStructMem->m_flInputPosition[2];

		// find the loudest, ie: least occluded ss player
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{

			float flGain = S_CalcOcclusion( hh,
				pChannel,
				pScratchPad->m_vPlayerOrigin[ hh ],
				vOrigin,
				pStructMem->m_flOccludedDBLoss );

			// inverse scale
			flGain = 1.0 - ( ( 1.0 - flGain ) * flScalar );

			flGain = SND_FadeToNewGain( &(pChannel->gain[ hh ]), pChannel, flGain );

			flResult = MAX( flResult, flGain );

		}
		pStructMem->m_flLastTraceTime = flCurHostTime;
	}
	else
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			float flGain = SND_FadeToNewGain(  &(pChannel->gain[ hh ] ), pChannel, -1.0 );
			flResult = MAX( flResult, flGain );
		}
	}

	pStructMem->m_flOutput[0] = flResult;
}

void CSosOperatorOcclusion::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	//	CSosOperatorOcclusion_t *pStructMem = (CSosOperatorOcclusion_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

}
void CSosOperatorOcclusion::OpHelp( ) const
{

}
void CSosOperatorOcclusion::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorOcclusion_t *pStructMem = (CSosOperatorOcclusion_t *)pVoidMem;
	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if ( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ) )
				{

				}
				else if ( !V_strcasecmp( pParamString, "occlusio_db_loss" ) )
				{
					pStructMem->m_flOccludedDBLoss = V_atof( pValueString ) ;
				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}

extern IPhysicsSurfaceProps* physprops;
extern ConVar snd_occlusion;
ConVar snd_occlusion_pos_override("snd_occlusion_pos_override", "", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT);
ConVar snd_occlusion_material_override("snd_occlusion_material_override", "", FCVAR_DEVELOPMENTONLY|FCVAR_CHEAT);
ConVar snd_occlusion_visualize("snd_occlusion_visualize", "0", FCVAR_DEVELOPMENTONLY|FCVAR_CHEAT);
ConVar snd_occlusion_visualize_filter("snd_occlusion_visualize_filter", "", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT);
ConVar snd_occlusion_indirect_radius("snd_occlusion_indirect_radius", "120.0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT);
ConVar snd_occlusion_indirect_min("snd_occlusion_indirect_min", "0.3", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT);
ConVar snd_occlusion_indirect_max("snd_occlusion_indirect_max", "0.85", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT);
ConVar snd_occlusion_collide_min_distance("snd_occlusion_collide_min_distance", "4.0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );

extern ConVar snd_hwcompat;

//Weightings and scalings for people playing without 3D audio so we can translate the 3D
//effect to a simple volume.
ConVar snd_occlusion_eq_low("snd_occlusion_eq_low", "0.10", FCVAR_CHEAT );
ConVar snd_occlusion_eq_mid("snd_occlusion_eq_mid", "1.0", FCVAR_CHEAT );
ConVar snd_occlusion_eq_high("snd_occlusion_eq_high", "0.20", FCVAR_CHEAT );
ConVar snd_occlusion_no_eq_scale("snd_occlusion_no_eq_scale", "1.05", FCVAR_CHEAT );

namespace
{
struct SOccludingWall
{
	float fThickness;
	const surfacedata_t* pSurfaceData;
};

int AccumulateAllOccludingWalls( const Vector& src, const Vector& dst, SOccludingWall* walls, int nWalls )
{
	int nWall = 0;

	Vector pos = src;

	CTraceFilterWorldAndPropsOnly filter;

	const surfacedata_t* pSurfaceData = nullptr;

	trace_t tr;
	Ray_t ray;
	ray.Init( pos, dst );
	g_pEngineTraceClient->TraceRay( ray, MASK_BLOCK_AUDIO, &filter, &tr );

	int nMaxTries = nWalls;

	bool hit = false;

	int nCount = 0;

	while ( tr.DidHit() && tr.fraction < 0.99 && nWall < nWalls && nMaxTries-- > 0 )
	{
		hit = true;

		Vector diff;
		VectorSubtract(dst, pos, diff);

		if ( tr.startsolid && tr.fraction < 0.0001f && tr.fractionleftsolid < 0.0001f && tr.allsolid )
		{
			tr.fractionleftsolid = 1.0f;
			diff = diff * ( snd_occlusion_collide_min_distance.GetFloat() / diff.Length() );
			tr.endpos = tr.startpos + diff;
		}

		if ( pSurfaceData != nullptr && tr.startsolid )
		{
			walls[nWall].fThickness = tr.fractionleftsolid*diff.Length();
			walls[nWall].pSurfaceData = pSurfaceData;
			++nWall;
		}

		pSurfaceData = physprops->GetSurfaceData(tr.surface.surfaceProps);

		for( int i = 0; i != 3; ++i )
		{
			pos[i] = tr.endpos[i]*0.9999 + dst[i]*0.0001;
		}

		Ray_t ray2;
		ray2.Init(pos, dst);
		g_pEngineTraceClient->TraceRay(ray2, MASK_BLOCK_AUDIO, &filter, &tr);

		++nCount;
	}

	if ( nMaxTries <= 0 )
	{
		if ( nWall > 0 )
		{
			//We couldn't find our way all the way through so just assume the last wall takes the remainder of the distance.
			const float fRemainingDistance = (tr.endpos - dst).Length();
			walls[nWall-1].fThickness += fRemainingDistance;
			return nWall;
		}
		return -1;
	}

	if ( pSurfaceData != nullptr && ( !tr.DidHit() || tr.fraction >= 0.99 ) && tr.startsolid )
	{
		Vector diff;
		VectorSubtract(dst, pos, diff);
		walls[nWall].fThickness = tr.fractionleftsolid*diff.Length();
		walls[nWall].pSurfaceData = pSurfaceData;
		++nWall;
	}

	return nWall;
}

ConVar snd_occlusion_rays("snd_occlusion_rays", "4", FCVAR_CHEAT | FCVAR_REPLICATED );
ConVar snd_occlusion_bounces("snd_occlusion_bounces", "1", FCVAR_CHEAT | FCVAR_REPLICATED );

float FindIndirectSoundPaths(const Vector& vListenerRef, const Vector& vSource, Vector* vSoundOrigin)
{
	Vector vListener = vListenerRef;
	if( snd_occlusion_pos_override.GetString() && *snd_occlusion_pos_override.GetString() )
	{
		const char* p = snd_occlusion_pos_override.GetString();
		vListener[0] = Q_atof(p);
		while(*p && *p != ' ')
		{
			++p;
		}

		if(*p)
		{
			++p;
		}

		vListener[1] = Q_atof(p);
		while(*p && *p != ' ')
		{
			++p;
		}

		if(*p)
		{
			++p;
		}

		vListener[2] = Q_atof(p);
	}

	CTraceFilterWorldAndPropsOnly filter;

	const float fRadius = snd_occlusion_indirect_radius.GetFloat();


	const int nSourcePoints = snd_occlusion_rays.GetInt();
	Vector* vSoundSources = new Vector[nSourcePoints];
	int* vNext = new int[nSourcePoints];

	Vector vsrc_forward, vsrc_right, vsrc_up;

	VectorSubtract(vListener, vSource, vsrc_forward);
	VectorNormalize(vsrc_forward);
	VectorVectors(vsrc_forward, vsrc_right, vsrc_up);

	if ( vsrc_up.z < 0.0f )
	{
		vsrc_up.z *= -1.0f;
	}

	{
		int i = 0;
		vSoundSources[i++] = vsrc_forward + vsrc_right + vsrc_up;
		vSoundSources[i++] = vsrc_forward - vsrc_right + vsrc_up;
		vSoundSources[i++] = -vsrc_forward + vsrc_right + vsrc_up;
		vSoundSources[i++] = -vsrc_forward - vsrc_right + vsrc_up;
	}

	vNext[0] = 1;
	vNext[1] = 2;
	vNext[2] = 3;
	vNext[3] = 0;

	for (int i = 4; i < nSourcePoints/2; )
	{
		const int end_gen = i;
		for ( int n = 0; n != end_gen && i < nSourcePoints; ++n )
		{
			int next = vNext[n];
			vNext[n] = i;
			vNext[i] = next;

			vSoundSources[i] = vSoundSources[n] + vSoundSources[next];
			VectorNormalize(vSoundSources[i]);
			++i;
		}
	}

	for ( int i = nSourcePoints/2; i < nSourcePoints && nSourcePoints >= 8; ++i )
	{
		vSoundSources[i] = vSoundSources[i - nSourcePoints/2] - vsrc_up;
	}

	for ( int i = 0; i != nSourcePoints; ++i )
	{
		VectorNormalize( vSoundSources[i] );
		vSoundSources[i] *= fRadius;
		vSoundSources[i] += vSource;
	}

	const bool visualize = snd_occlusion_visualize.GetBool();

	int nBounces = snd_occlusion_bounces.GetInt();

	int nHit = 0, nFirstOrder = 0;
	for ( int n = 0; n != nSourcePoints && nHit < nSourcePoints/2; ++n )
	{
		Vector src = vSource;
		bool bHit = true;
		for ( int m = 0; m != nBounces && bHit; ++m )
		{
			trace_t tr;
			Ray_t ray;
			ray.Init(src, vSoundSources[n]);
			g_pEngineTraceClient->TraceRay(ray, MASK_BLOCK_AUDIO, &filter, &tr);

			if(visualize)
			{
				CDebugOverlay::AddLineOverlay(src, tr.endpos, 255, m < 1 ? 255 : 0, m < 2 ? 255 : 0, 255, true, 1.0f);
			}

			bHit = tr.DidHit();
			if ( bHit )
			{
				Vector vAngle = tr.endpos - tr.startpos;

				Vector vReflection = vAngle - 2 * DotProduct( vAngle, tr.plane.normal ) * tr.plane.normal;

				VectorNormalize( vReflection );

				src = tr.endpos;
				vSoundSources[ n ] = src + vReflection*fRadius;
			}

			ray.Init( tr.endpos, vListener );

			g_pEngineTraceClient->TraceRay(ray, MASK_BLOCK_AUDIO, &filter, &tr);

			if ( !tr.DidHit() )
			{
				if ( m == 0 )
				{
					++nFirstOrder;
				}

				++nHit;
				*vSoundOrigin += tr.startpos;
				
//				Vector vDir;
//				S_CalculateHRTFAngle(ch, tr.startpos, &vDir);

				break;
			}
		}
	}

	delete [] vSoundSources;
	delete [] vNext;

	if ( nHit >= nSourcePoints/2 )
	{
		return 1.0f;
	}

	return (float)nHit/(float)(nSourcePoints/2);
}

struct SRecentDebugOcclusionRecord
{
	Vector origin;
	float t;
};

const int NumRecentDebugOcclusionRecords = 256;

SRecentDebugOcclusionRecord g_recent_debug_occlusion_records[NumRecentDebugOcclusionRecords];
int g_index_recent_debug_occlusion_records = 0;

}

float S_CalcOcclusion( int nSlot, channel_t *ch, const Vector &vecListenerOrigin, Vector vSoundSource, float flOccludedDBLoss )
{
	if( snd_occlusion.GetBool() && !ch->hrtf.ignore_occlusion && ( ch->wavtype == CHAR_HRTF || ch->wavtype == CHAR_HRTF_BLEND ) )
	{
		const int NumWalls = 10;
		SOccludingWall walls[NumWalls];
		const int nWalls = AccumulateAllOccludingWalls( vecListenerOrigin, vSoundSource, walls, NumWalls );
		if ( nWalls == 0 )
		{
			if ( snd_occlusion_visualize.GetBool() )
			{
				char name[256];
				ch->sfx->getname( name, sizeof( name ) );

				if ( V_strstr( name, snd_occlusion_visualize_filter.GetString() ) )
				{
					int nLine = 0;
					float t = Plat_FloatTime();

					for ( int i = 0; i < NumRecentDebugOcclusionRecords && i < g_index_recent_debug_occlusion_records; ++i )
					{
						if ( t < g_recent_debug_occlusion_records[i].t + 0.5f )
						{
							bool bNear = true;
							const Vector& origin = g_recent_debug_occlusion_records[i].origin;
							for ( int j = 0; j != 3; ++j )
							{
								if ( abs(origin[j] - ch->origin[j]) > 0.1f )
								{
									bNear = false;
								}
							}

							if ( bNear )
							{
								++nLine;
							}
						}
					}

					SRecentDebugOcclusionRecord& record = g_recent_debug_occlusion_records[g_index_recent_debug_occlusion_records%NumRecentDebugOcclusionRecords];
					record.t = t;
					record.origin = ch->origin;
					++g_index_recent_debug_occlusion_records;

					char buf[512];
					V_snprintf(buf, sizeof(buf), "Unoccluded: %s", name);

					float vColor[4] = {0.5f, 1.0f, 0.5f, 1.0f};

					CDebugOverlay::AddTextOverlay(ch->origin, nLine, 1.0f, vColor[0], vColor[1], vColor[2], vColor[3], buf);
				}
			}
			ch->hrtf.use_occlusion = false;
			return 1.0f;
		}

		Vector dir;
		VectorSubtract(vecListenerOrigin, vSoundSource, dir);

		ch->hrtf.use_occlusion = true;

		ch->hrtf.path.direction.x = 1.0f;
		ch->hrtf.path.direction.y = 0.0f;
		ch->hrtf.path.direction.z = 0.0f;

		ch->hrtf.path.distanceAttenuation = 1.0f;
		ch->hrtf.path.airAbsorption[0] = 1.0f;
		ch->hrtf.path.airAbsorption[1] = 1.0f;
		ch->hrtf.path.airAbsorption[2] = 1.0f;

		ch->hrtf.path.propagationDelay = 0.0f;
		ch->hrtf.path.occlusionFactor = 0.0f;

		ch->hrtf.path.transmissionFactor[0] = 1.0f;
		ch->hrtf.path.transmissionFactor[1] = 1.0f;
		ch->hrtf.path.transmissionFactor[2] = 1.0f;

		const char *pDebugOverride = snd_occlusion_material_override.GetString();
		if ( nWalls < 0 )
		{
			ch->hrtf.path.transmissionFactor[0] = 0.0f;
			ch->hrtf.path.transmissionFactor[1] = 0.0f;
			ch->hrtf.path.transmissionFactor[2] = 0.0f;
		}
		else if ( pDebugOverride != nullptr && *pDebugOverride )
		{
			const float a = V_atof( pDebugOverride );
			pDebugOverride = V_strstr( pDebugOverride, " " );
			float b = a;
			if ( pDebugOverride != nullptr )
			{
				while ( *pDebugOverride == ' ' )
					++pDebugOverride;
				b = V_atof( pDebugOverride );

				pDebugOverride = V_strstr( pDebugOverride, " " );
			}

			float c = b;

			if ( pDebugOverride != nullptr )
			{
				while(*pDebugOverride == ' ')
					++pDebugOverride;
				c = V_atof( pDebugOverride );
			}

			for(int i = 0; i != nWalls; ++i)
			{
				ch->hrtf.path.transmissionFactor[0] *= pow(1.0f - (a*0.01f), walls[i].fThickness);
				ch->hrtf.path.transmissionFactor[1] *= pow(1.0f - (b*0.01f), walls[i].fThickness);
				ch->hrtf.path.transmissionFactor[2] *= pow(1.0f - (c*0.01f), walls[i].fThickness);
			}
		}
		else
		{
			for ( int i = 0; i != nWalls; ++i )
			{
				ch->hrtf.path.transmissionFactor[0] *= pow( 1.0f - (walls[ i ].pSurfaceData->audio.lowPitchOcclusion*0.01f), walls[ i ].fThickness );
				ch->hrtf.path.transmissionFactor[1] *= pow( 1.0f - (walls[ i ].pSurfaceData->audio.midPitchOcclusion*0.01f), walls[ i ].fThickness );
				ch->hrtf.path.transmissionFactor[2] *= pow( 1.0f - (walls[ i ].pSurfaceData->audio.highPitchOcclusion*0.01f), walls[ i ].fThickness );
			}
		}

		Vector vEffectiveSource;
		float fIndirectRatio = snd_occlusion_indirect_min.GetFloat() + FindIndirectSoundPaths( vecListenerOrigin, vSoundSource, &vEffectiveSource ) * ( snd_occlusion_indirect_max.GetFloat() - snd_occlusion_indirect_min.GetFloat() );

		if( fIndirectRatio > 0.0f )
		{
			for( int i = 0; i != 3; ++i )
			{
				ch->hrtf.path.transmissionFactor[i] = fIndirectRatio + (1.0 - fIndirectRatio) * ch->hrtf.path.transmissionFactor[i];
			}
		}

		if ( snd_hwcompat.GetBool() )
		{
			const float eq = 0.0f;
			const float fTotalWeight = snd_occlusion_eq_low.GetFloat() + snd_occlusion_eq_mid.GetFloat() + snd_occlusion_eq_high.GetFloat();
			if ( fTotalWeight > 0.0f )
			{
				float fVolume = ( snd_occlusion_eq_low.GetFloat()/fTotalWeight ) * ch->hrtf.path.transmissionFactor[0] +
				                ( snd_occlusion_eq_mid.GetFloat()/fTotalWeight ) * ch->hrtf.path.transmissionFactor[1] +
				                ( snd_occlusion_eq_high.GetFloat()/fTotalWeight ) * ch->hrtf.path.transmissionFactor[2];

				//apply an extra nerf to volume to make sure that playing without eq is never an advantage.
				fVolume = 1.0f - ( 1.0f - fVolume )*snd_occlusion_no_eq_scale.GetFloat();
				if ( fVolume < 0.0f )
				{
					fVolume = 0.0f;
				}
				else if ( fVolume > 1.0f )
				{
					fVolume = 1.0f;
				}

				for ( int i = 0; i != 3; ++i )
				{
					ch->hrtf.path.transmissionFactor[i] = ch->hrtf.path.transmissionFactor[i] * eq + fVolume * ( 1.0f - eq );
				}
			}
		}

		if(snd_occlusion_visualize.GetBool())
		{
			float fThickness = 0.0f;
			for(int i = 0; nWalls > 0 && i != nWalls; ++i)
			{
				fThickness += walls[ i ].fThickness;
			}

			char name[256];
			ch->sfx->getname(name, sizeof(name));

			if(V_strstr(name, snd_occlusion_visualize_filter.GetString()))
			{
				char buf[512];
				V_snprintf(buf, sizeof(buf), "%s: nwalls=%d, thick=%0.2f indirect=%0.2f; transmit: %0.2f/%0.2f/%0.2f", name, nWalls, fThickness, fIndirectRatio, ch->hrtf.path.transmissionFactor[0], ch->hrtf.path.transmissionFactor[1], ch->hrtf.path.transmissionFactor[2]);
				
				float vColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				if ( nWalls < 0 )
				{
					//V_snprintf(buf, sizeof(buf), "%s: Completely occluded", name);
					vColor[1] = 0.4f;
					vColor[2] = 0.4f;
				}

				int nLine = 0;
				float t = Plat_FloatTime();

				for(int i = 0; i < NumRecentDebugOcclusionRecords && i < g_index_recent_debug_occlusion_records; ++i)
				{
					if(t < g_recent_debug_occlusion_records[i].t + 0.5f)
					{
						bool bNear = true;
						const Vector& origin = g_recent_debug_occlusion_records[i].origin;
						for(int j = 0; j != 3; ++j)
						{
							if(abs(origin[j] - ch->origin[j]) > 0.1f)
							{
								bNear = false;
							}
						}

						if(bNear)
						{
							++nLine;
						}
					}
				}

				SRecentDebugOcclusionRecord& record = g_recent_debug_occlusion_records[g_index_recent_debug_occlusion_records%NumRecentDebugOcclusionRecords];
				record.t = t;
				record.origin = ch->origin;
				++g_index_recent_debug_occlusion_records;

				CDebugOverlay::AddTextOverlay(ch->origin, nLine, 1.0f, vColor[0], vColor[1], vColor[2], vColor[3], buf);
			}
		}

		return 1.0f;
	}

	float gain = 1.0;
	int count = 1;

	trace_t tr;
	CTraceFilterWorldOnly filter;	// UNDONE: also test for static props?
	Ray_t ray;
	ray.Init( MainViewOrigin( nSlot ), vSoundSource );
	g_pEngineTraceClient->TraceRay( ray, MASK_BLOCK_AUDIO, &filter, &tr );
	// total traces this frame
	g_snd_trace_count++;

	if (tr.DidHit() && tr.fraction < 0.99)
	{
		// can't see center of sound source:
		// build extents based on dB sndlvl of source,
		// test to see how many extents are visible,
		// drop gain by snd_gain_db per extent hidden
		Vector vSoundSources[4];
		soundlevel_t sndlvl = DIST_MULT_TO_SNDLVL( ch->dist_mult );
		float radius;
		Vector vsrc_forward;
		Vector vsrc_right;
		Vector vsrc_up;
		Vector vecl;
		Vector vecr;
		Vector vecl2;
		Vector vecr2;
		int i;

		// get radius

		if ( ch->radius > 0 )
			radius = ch->radius;
		else
			radius = dB_To_Radius( sndlvl);		// approximate radius from soundlevel

		// set up extent vSoundSources - on upward or downward diagonals, facing player

		for (i = 0; i < 4; i++)
			vSoundSources[i] = vSoundSource;

		// vsrc_forward is normalized vector from sound source to listener

		VectorSubtract( vecListenerOrigin, vSoundSource, vsrc_forward );
		VectorNormalize( vsrc_forward );
		VectorVectors( vsrc_forward, vsrc_right, vsrc_up );

		VectorAdd( vsrc_up, vsrc_right, vecl );

		// if src above listener, force 'up' vector to point down - create diagonals up & down

		if ( vSoundSource.z > vecListenerOrigin.z + (10 * 12) )
			vsrc_up.z = -vsrc_up.z;

		VectorSubtract( vsrc_up, vsrc_right, vecr );
		VectorNormalize( vecl );
		VectorNormalize( vecr );

		// get diagonal vectors from sound source 

		vecl2 = radius * vecl;
		vecr2 = radius * vecr;
		vecl = (radius / 2.0) * vecl;
		vecr = (radius / 2.0) * vecr;

		// vSoundSources from diagonal vectors

		vSoundSources[0] += vecl;
		vSoundSources[1] += vecr;
		vSoundSources[2] += vecl2;
		vSoundSources[3] += vecr2;

		// drop gain for each point on radius diagonal that is obscured

		for (count = 0, i = 0; i < 4; i++)
		{
			// UNDONE: some vSoundSources are in walls - in this case, trace from the wall hit location

			Ray_t rayDiag;
			rayDiag.Init( MainViewOrigin( nSlot ), vSoundSources[i] );
			g_pEngineTraceClient->TraceRay( rayDiag, MASK_BLOCK_AUDIO, &filter, &tr );

			if (tr.DidHit() && tr.fraction < 0.99 && !tr.startsolid )
			{
				count++;	// skip first obscured point: at least 2 points + center should be obscured to hear db loss
				if (count > 1)
					gain = gain * dB_To_Gain( flOccludedDBLoss );
			}
		}
	}


	if ( snd_showstart.GetInt() == 7)
	{
		static float g_drop_prev = 0;
		float drop = (count-1) * flOccludedDBLoss;

		if (drop != g_drop_prev)
		{
			DevMsg( "dB drop: %1.4f \n", drop);
			g_drop_prev = drop;
		}
	}

	return gain;

}
