//========= Copyright Valve Corporation, All rights reserved. ============//

#pragma once

//------------------------------------------------------------------------------
// Purpose: Text message in world space
//------------------------------------------------------------------------------
class C_PointWorldText : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_PointWorldText, C_BaseEntity );
		
	DECLARE_CLIENTCLASS();

	C_PointWorldText();

	virtual ~C_PointWorldText();

	virtual void Spawn() OVERRIDE;
	virtual bool ShouldDraw() OVERRIDE;
	virtual void ClientThink() OVERRIDE;
	virtual int DrawModel( int flags, const RenderableInstance_t &instance ) OVERRIDE;
	virtual void GetRenderBounds( Vector& mins, Vector& maxs ) OVERRIDE;
	virtual void GetRenderBoundsWorldspace( Vector& mins, Vector& maxs ) OVERRIDE;
	virtual void PostDataUpdate( DataUpdateType_t updateType ) OVERRIDE;

private:
	void UpdateRenderBounds();
	void ComputeCornerVertices( QAngle angles, Vector origin, Vector *pVerts, float flBloat ) const;
	void CalcBounds();

	Vector m_localBBMin;
	Vector m_localBBMax;
	Vector m_worldBBMin;
	Vector m_worldBBMax;

	char m_szText[ MAX_PATH ];
	float m_flTextSize;
	color32 m_textColor;
};
