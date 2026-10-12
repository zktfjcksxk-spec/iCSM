//========= Copyright Valve Corporation, All rights reserved. ============//

#include "cbase.h"
#include "c_point_worldtext.h"
#include "model_types.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

PRECACHE_REGISTER_BEGIN( GLOBAL, PointWorldTextMaterial )
PRECACHE( MATERIAL, "editor/worldtext" )
PRECACHE_REGISTER_END()

IMPLEMENT_CLIENTCLASS_DT(C_PointWorldText, DT_PointWorldText, CPointWorldText)
	RecvPropString(RECVINFO(m_szText)),
	RecvPropFloat(RECVINFO(m_flTextSize)),
	RecvPropInt( RECVINFO( m_textColor ), 0, RecvProxy_Int32ToColor32 ),
END_RECV_TABLE()

LINK_ENTITY_TO_CLASS(point_worldtext, C_PointWorldText);

C_PointWorldText::C_PointWorldText() :
C_BaseEntity(),
m_flTextSize( 10.0f )
{
	V_memset( m_szText, 0, sizeof( m_szText ) );
	m_textColor.r = 255;
	m_textColor.g = 255;
	m_textColor.b = 255;
	m_textColor.a = 255;
}


C_PointWorldText::~C_PointWorldText()
{
}

void C_PointWorldText::Spawn()
{
	BaseClass::Spawn();
	SetNextClientThink( 0.0f ); // Think once to set render bounds
}

bool C_PointWorldText::ShouldDraw()
{
	return true;
}

void C_PointWorldText::ClientThink()
{
	BaseClass::ClientThink();
	UpdateRenderBounds();
}

void C_PointWorldText::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );
	
	// Text might have changed. Update bounds.
	UpdateRenderBounds();
}

void C_PointWorldText::ComputeCornerVertices( QAngle angles, Vector origin, Vector *pVerts, float flBloat ) const
{
	Vector ViewForward( 1.0f, 0.0f, 0.0f );
	Vector ViewUp( 0.0f, 1.0f, 0.0f );
	Vector ViewRight( 0.0f, 0.0f, -1.0f );
	AngleVectors( angles, &ViewForward, &ViewRight, &ViewUp );

	float flStrLength = V_strlen( m_szText );
	flStrLength = Max( flStrLength, 1.0f );

	pVerts[ 0 ] = origin - flBloat * ( ViewRight + ViewUp );
	pVerts[ 1 ] = pVerts[ 0 ] + ( m_flTextSize * ( 1.0f + ( flStrLength - 1.0f ) * 0.6f ) + 2.0f * flBloat ) * ViewRight;
	pVerts[ 2 ] = pVerts[ 1 ] + ( m_flTextSize + 2.0f * flBloat )* ViewUp;
	pVerts[ 3 ] = pVerts[ 0 ] + ( m_flTextSize + 2.0f * flBloat )* ViewUp;
}

void C_PointWorldText::UpdateRenderBounds()
{
	if ( !C_BaseEntity::IsAbsQueriesValid() )
	{
		return;
	}

	Vector cornerVerts[ 4 ];
	ComputeCornerVertices( QAngle(0.0f, 0.0f, 0.0f), vec3_origin, cornerVerts, 1.0f );

	m_localBBMin = VectorMin( VectorMin( cornerVerts[ 0 ], cornerVerts[ 1 ] ), VectorMin( cornerVerts[ 2 ], cornerVerts[ 3 ] ) );
	m_localBBMax = VectorMax( VectorMax( cornerVerts[ 0 ], cornerVerts[ 1 ] ), VectorMax( cornerVerts[ 2 ], cornerVerts[ 3 ] ) );

	ComputeCornerVertices( GetAbsAngles(), GetAbsOrigin(), cornerVerts, 1.0f );

	m_worldBBMin = VectorMin( VectorMin( cornerVerts[ 0 ], cornerVerts[ 1 ] ), VectorMin( cornerVerts[ 2 ], cornerVerts[ 3 ] ) );
	m_worldBBMax = VectorMax( VectorMax( cornerVerts[ 0 ], cornerVerts[ 1 ] ), VectorMax( cornerVerts[ 2 ], cornerVerts[ 3 ] ) );
}

void C_PointWorldText::GetRenderBounds( Vector& mins, Vector& maxs )
{
	mins = m_localBBMin;
	maxs = m_localBBMax;
}

void C_PointWorldText::GetRenderBoundsWorldspace( Vector& mins, Vector& maxs )
{
	mins = m_worldBBMin;
	maxs = m_worldBBMax;
}

#define CHAR_WIDTH 0.0625f // 1/16
#define CHAR_HEIGHT 0.0625f // 1/16

int C_PointWorldText::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if ( ( flags & STUDIO_SHADOWDEPTHTEXTURE ) || ( flags & STUDIO_SHADOWTEXTURE ) )
	{
		return 0;
	}

	const char *szText = m_szText;

	if ( !szText )
	{
		return 0;
	}

	int nNumChars = V_strlen( m_szText );
	if ( !nNumChars )
	{
		return 0;
	}

	Vector ViewForward( 1.0f, 0.0f, 0.0f );
	Vector ViewUp( 0.0f, 1.0f, 0.0f );
	Vector ViewRight( 0.0f, 0.0f, -1.0f );
	AngleVectors( GetAbsAngles(), &ViewForward, &ViewRight, &ViewUp );

	Vector vecStartPos;
	VectorCopy( GetAbsOrigin(), vecStartPos );

	IMaterial* pDebugText = g_pMaterialSystem->FindMaterial( "editor/worldtext", TEXTURE_GROUP_OTHER, true );
	if ( !pDebugText )
	{
		return 0;
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( pDebugText );

	IMesh* pMesh = pRenderContext->GetDynamicMesh();
	CMeshBuilder meshBuilder;

	float screenSize = m_flTextSize;

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, nNumChars );

	for ( int i = 0; i < nNumChars; i++ )
	{
		int nCharIdx = (int)( (char)*szText ) - 32;
		int nRow = nCharIdx / 16;
		int nCol = nCharIdx % 16;

		float flU = ( nCol * CHAR_WIDTH );
		float flV = ( nRow * CHAR_HEIGHT );

		flV += CHAR_HEIGHT;
		meshBuilder.Position3fv( vecStartPos.Base() );
		meshBuilder.TexCoord2f( 0, flU, flV );
		meshBuilder.Color4ub( m_textColor.r, m_textColor.g, m_textColor.b, 255 );
		meshBuilder.AdvanceVertex();

		vecStartPos += ( ViewUp * screenSize );
		flV -= CHAR_HEIGHT;
		meshBuilder.Position3fv( vecStartPos.Base() );
		meshBuilder.TexCoord2f( 0, flU, flV );
		meshBuilder.Color4ub( m_textColor.r, m_textColor.g, m_textColor.b, 255 );
		meshBuilder.AdvanceVertex();

		vecStartPos += ( ViewRight * screenSize );
		flU += CHAR_WIDTH;
		meshBuilder.Position3fv( vecStartPos.Base() );
		meshBuilder.TexCoord2f( 0, flU, flV );
		meshBuilder.Color4ub( m_textColor.r, m_textColor.g, m_textColor.b, 255 );
		meshBuilder.AdvanceVertex();

		vecStartPos -= ( ViewUp * screenSize );
		flV += CHAR_HEIGHT;
		meshBuilder.Position3fv( vecStartPos.Base() );
		meshBuilder.TexCoord2f( 0, flU, flV );
		meshBuilder.Color4ub( m_textColor.r, m_textColor.g, m_textColor.b, 255 );
		meshBuilder.AdvanceVertex();

		vecStartPos -= ( ViewRight * screenSize * 0.4f );

		szText++;
	}

	meshBuilder.End();
	pMesh->Draw();

	return 1;
}
