//====== Copyright © Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//==================================================================

#include "cbase.h"
#include "grassburn.h"
#ifdef CLIENT_DLL
#include "c_world.h"
#endif
#include "cs_gamerules.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

ConVar sv_grassburn( "sv_grassburn", "0", FCVAR_RELEASE | FCVAR_REPLICATED );

EHANDLE g_hGrassBurn = INVALID_EHANDLE;

CGrassBurn *GetGrassBurn( void )
{
	if ( !CSGameRules() || !CSGameRules()->IsPlayingSurvival() )
		return NULL;

	if ( g_hGrassBurn.Get() )
	{
		CGrassBurn *pGrassBurn = static_cast<CGrassBurn*>(g_hGrassBurn.Get());

		g_hGrassBurn = pGrassBurn;
		return pGrassBurn;
	}
#ifndef CLIENT_DLL
	else
	{
		CGrassBurn *pGrassBurn = dynamic_cast<CGrassBurn*>(CreateEntityByName( "grassburn" ));
		if ( pGrassBurn )
		{
			pGrassBurn->SetAbsOrigin( vec3_origin );
			pGrassBurn->SetAbsAngles( vec3_angle );
			DispatchSpawn( pGrassBurn );

			g_hGrassBurn = pGrassBurn;
			return pGrassBurn;
		}
	}
#endif

	return NULL;
}

#ifdef CLIENT_DLL

#undef CGrassBurn
IMPLEMENT_CLIENTCLASS_DT( C_GrassBurn, DT_GrassBurn, CGrassBurn )
RecvPropFloat( RECVINFO( m_flGrassBurnClearTime ) ),
END_RECV_TABLE()
#define CGrassBurn C_GrassBurn

#else
LINK_ENTITY_TO_CLASS( grassburn, CGrassBurn );

BEGIN_DATADESC( CGrassBurn )
DEFINE_FIELD( m_flGrassBurnClearTime, FIELD_FLOAT ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CGrassBurn, DT_GrassBurn )
SendPropFloat( SENDINFO( m_flGrassBurnClearTime ) ),
END_SEND_TABLE()
#endif

CGrassBurn::CGrassBurn( void )
{
#ifdef CLIENT_DLL
	m_bClientPendingClear = true;
	m_vecGrassBurnPositions.RemoveAll();
#else
	ResetGrassBurn();
#endif
}

#ifdef CLIENT_DLL
void CGrassBurn::OnPreDataChanged( DataUpdateType_t updateType )
{
	m_flGrassBurnClearTimeLocal = m_flGrassBurnClearTime;
	BaseClass::OnPreDataChanged( updateType );
}

void CGrassBurn::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		g_hGrassBurn = this;
	}

	if ( m_flGrassBurnClearTimeLocal != m_flGrassBurnClearTime )
	{
		m_bClientPendingClear = true;
		m_vecGrassBurnPositions.RemoveAll();
	}
}

void CGrassBurn::TorchGrassAt( Vector vecPosition )
{
	m_vecGrassBurnPositions.AddToTail( vecPosition );
}

void CGrassBurn::Update( void )
{
	if ( !sv_grassburn.GetBool() )
		return;

	if ( !CSGameRules() || !CSGameRules()->IsPlayingSurvival() )
		return;

	if ( !m_bClientPendingClear && !m_vecGrassBurnPositions.Count() )
		return;

	ITexture *pRtOutput = materials->FindTexture( "_rt_GrassBurn", TEXTURE_GROUP_RENDER_TARGET );
	if ( !pRtOutput )
	{
		Assert( false );
		return;
	}

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->PushRenderTargetAndViewport();
	pRenderContext->SetRenderTarget( pRtOutput );
	pRenderContext->Viewport( 0, 0, pRtOutput->GetActualWidth(), pRtOutput->GetActualHeight() );

	if ( m_bClientPendingClear )
	{
		// clear the target
		pRenderContext->ClearColor3ub( 0, 0, 0 );
		pRenderContext->ClearBuffers( true, false, false );

		m_bClientPendingClear = false;
	}

	if ( m_vecGrassBurnPositions.Count() )
	{
		IMaterial *pMatTorch = materials->FindMaterial( "dev/grassburn", TEXTURE_GROUP_OTHER, true );
		C_World *pWorld = GetClientWorldEntity();
		if ( pWorld )
		{
			float flLongestSide = pWorld->GetLongest2DSide();
			Assert( flLongestSide > 0 );
			if ( flLongestSide > 0 )
			{
				float flSplatRadius = 100.0f * (1.0f / flLongestSide);

				Vector vecToneMapPrev = pRenderContext->GetToneMappingScaleLinear();
				pRenderContext->SetToneMappingScaleLinear( Vector( 1, 1, 1 ) );

				FOR_EACH_VEC_BACK( m_vecGrassBurnPositions, n )
				{
					float flX = RemapVal( m_vecGrassBurnPositions[n].x, pWorld->m_WorldMins.x, pWorld->m_WorldMins.x + flLongestSide, 0.0f, 1.0f );
					float flY = RemapVal( m_vecGrassBurnPositions[n].y, pWorld->m_WorldMins.y, pWorld->m_WorldMins.y + flLongestSide, 1.0f, 0.0f );
					pRenderContext->DrawScreenSpaceRectangle( pMatTorch, flX - flSplatRadius, flY - flSplatRadius, flSplatRadius * 2.0f, flSplatRadius * 2.0f, 0, 0, 1, 1 );
					m_vecGrassBurnPositions.Remove( n );
				}

				pRenderContext->SetToneMappingScaleLinear( vecToneMapPrev );
			}
		}
		else
		{
			Assert( false );
		}
	}

	pRenderContext->PopRenderTargetAndViewport();
}
#endif

#ifndef CLIENT_DLL
void CGrassBurn::Spawn( void )
{
	Assert( !g_hGrassBurn.Get() );

	ResetGrassBurn();

	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
}
#endif

