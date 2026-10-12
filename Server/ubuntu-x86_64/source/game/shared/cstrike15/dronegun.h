
#ifndef _DRONEGUN_H_
#define _DRONEGUN_H_

#ifdef GAME_DLL
#include "cs_player.h"
#include "effects.h"
#include "GameEventListener.h"
#endif

#ifdef CLIENT_DLL
#define CDronegun C_Dronegun
#endif

#ifdef GAME_DLL
struct droneguntarget_t
{
	int m_nEntIndex;
	Vector m_vecLastKnownPos;
	QAngle m_angLastKnownAngle;
	int m_nThreat;
	EHANDLE m_hEnt;

	droneguntarget_t()
	{
		m_nEntIndex = -1;
		m_vecLastKnownPos = vec3_origin;
		m_angLastKnownAngle = vec3_angle;
		m_nThreat = 0;
		m_hEnt = INVALID_EHANDLE;
	}
};
#endif

class CDronegun : public CBaseAnimating
#ifndef CLIENT_DLL
	, public CGameEventListener
#endif
{
public:
	DECLARE_CLASS( CDronegun, CBaseAnimating );
	DECLARE_NETWORKCLASS();

	CDronegun();
	~CDronegun();

	virtual void Precache() OVERRIDE;
	virtual void Spawn() OVERRIDE;

	CNetworkVar( Vector, m_vecAttentionTarget );
	CNetworkVar( Vector, m_vecTargetOffset );
	CNetworkVar( bool, m_bHasTarget );
	Vector m_vecAttentionCurrent;

	int m_nPoseParamPitch;
	int m_nPoseParamYaw;
	int m_nAttachMuzzle;
	bool m_bVarInit;

	int GetHealth( void ) const { return m_iHealth; }
	void VarInit( void );
	void ConvertTargetPosToPoseParams( Vector vecTarget );
	void ApproachTarget( float flInterval );
	bool CachePoseParamIndices( void );

	void DoSpark( void );

#ifdef GAME_DLL

	bool IsDisoriented( void ) { return m_flDisorientEndTime > gpGlobals->curtime; }
	void DisorientForDuration( float flDuration );
	float m_flDisorientEndTime;

	virtual void FireGameEvent( IGameEvent *event );

	bool IsKillable() const { return m_iHealth < 999; }
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual void Event_Killed( const CTakeDamageInfo &info );

	void BreakApart( void );

	void TraceToGround();
	void ServerThink();

	CUtlVector<droneguntarget_t> m_vecTargets;
	CHandle<CEnvGunfire> m_hEnvGunfire;
	float m_flLastShootTime;
	float m_flLastSound1;
	float m_flLastSound2;
	float m_flLastSound3;
#endif

#ifdef CLIENT_DLL
	virtual void PreDataUpdate( DataUpdateType_t updateType );
	virtual void PostDataUpdate( DataUpdateType_t updateType );
	float m_flClientPoseParamYaw;
	float m_flClientPoseParamPitch;

	virtual void ClientThink();
	float m_flLastClientThinkTime;
	float m_flNextSpark;

	virtual void PostBuildTransformations( CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion q[] ) OVERRIDE;
	Vector m_vecLaserTracePos;
	virtual void GetRenderBounds( Vector& theMins, Vector& theMaxs );
#endif

	private:
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iHealth );
};

#endif // _DRONEGUN_H_
