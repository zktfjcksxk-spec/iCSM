//--------------------------------------------------------------------------------------------------------

#ifndef DANGERZONE_CONTROLLER_H
#define DANGERZONE_CONTROLLER_H

#include "cs_gamerules.h"

#ifdef CLIENT_DLL
#define CDangerZoneController C_DangerZoneController
#define CDangerZone C_DangerZone
#endif

enum SurvivalNeighborHex_t
{
	NEIGHBOR_HEX_E = 0,
	NEIGHBOR_HEX_NE,
	NEIGHBOR_HEX_NW,
	NEIGHBOR_HEX_W,
	NEIGHBOR_HEX_SW,
	NEIGHBOR_HEX_SE,
	NEIGHBOR_HEX_COUNT
};

#define NUM_BOMB_WAVE 5

#ifdef CLIENT_DLL
#define DANGERZONE_DERIVE_FROM C_BaseEntity
#else
#define DANGERZONE_DERIVE_FROM CPointEntity
#endif

class CDangerZone : public DANGERZONE_DERIVE_FROM
{
	DECLARE_CLASS( CDangerZone, DANGERZONE_DERIVE_FROM );
public:
#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif // GAME_DLL
	DECLARE_NETWORKCLASS();

	CDangerZone();

#ifndef CLIENT_DLL
	void	Spawn( void );
	virtual int UpdateTransmitState( void ) OVERRIDE { return SetTransmitState( FL_EDICT_ALWAYS ); }

	void	InitDangerZone( const Vector& vecPos, int nIdx );
	void	SpawnDangerZoneCanister( void );

	void	SetBombWave( int iWaveID );
	void	SetDropOrder( int nDropOrder );
	void	SetBombLaunchTime( float flBombLaunchTime, const Vector& vecLandingPos );

	bool	BCanBePotentialEndGameHex();
	int		GetPotentialEndGamePoints( CUtlVector< CBaseEntity* > *pVecSpawnPoints = NULL );

	CDangerZone *GetNeighbor( int nNeighborIdx );
	CDangerZone *GetOppositeNeighbor( int nNeighborIdx );

	int		GetZoneIndex() const { return m_nMyZoneIndex; }

	void	UpdateExtraRadius();
	void	SetExtraRadius( float flExtraRadius, float flStartTime, float flTotalLerpTime );

	void	Log();

#endif

	bool	IsDangerZoneEnabled( void ) const;

	Vector	GetDangerZoneOrigin( float flAddTime = 0.0f );
	float	GetDangerZoneRadius( float flAddTime = 0.0f );
	float	GetPredictedWaveRadius( int iAddWave = 0 );
	bool	IsWithinPlayArea( const Vector& vecPos, float flAddTime = 0.0f );
	Vector	MovePointIntoPlayArea( const Vector& vecPos, float flBiasToCenter = 0.5f, float flAddTime = 0.0f );
	float	DistanceInsideDangerZone( const Vector& vecPos, float flAddTime = 0.0f );

	bool	IsBombIncoming() const { return m_iWave >= 0; }
	bool	IsBombInflight() const;
	float	GetZoneStartTime() const;

	int		GetDropOrder() const { return m_nDropOrder; }

	int		GetWaveID() const { return m_iWave; }
	const Vector &GetBombLandingPos() const { return m_vecDangerZoneOriginStartedAt.Get(); }

private:
	float GetDangerZoneLerp( float flAddTime = 0.0f );

	CNetworkVar( Vector, m_vecDangerZoneOriginStartedAt );
	CNetworkVar( float, m_flBombLaunchTime );
	CNetworkVar( float, m_flExtraRadius );
	CNetworkVar( float, m_flExtraRadiusStartTime );
	CNetworkVar( float, m_flExtraRadiusTotalLerpTime );
	CNetworkVar( int, m_nDropOrder );
	CNetworkVar( int, m_iWave );

#ifndef CLIENT_DLL
	int m_nMyZoneIndex;
	int m_nZoneNeighbors[NEIGHBOR_HEX_COUNT]; // 0 is east, 1 is ne, 2 is nw, etc

	enum EZoneHexType_t
	{
		k_EZoneHexType_Unknown,
		k_EZoneHexType_HasItems,
		k_EZoneHexType_Empty
	};
	EZoneHexType_t m_eZoneHexType;
#endif // !CLIENT_DLL
};

class CDangerZoneController : public CBaseEntity
{
	DECLARE_CLASS( CDangerZoneController, CBaseEntity );

	DECLARE_NETWORKCLASS();
#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif
	
public:
	CDangerZoneController();

	bool	IsMasterDangerZoneEnabled( void );

	float   GetDistanceFromDangerZone( const Vector& vecPos, Vector* pZoneCenter=nullptr, float flAddTime = 0.0f );

	float	GetMyDangerZoneRadius( const Vector& vecPos, float flAddTime = 0.0f );
	bool	IsWithinPlayArea( const Vector& vecPos, float flRatio = 1.0f, float flAddTime = 0.0f );
	float	DistanceOutsidePlayArea( const Vector& vecPos, float flAddTime = 0.0f );
	Vector	MovePointIntoPlayArea( const Vector& vecPos, float flBiasToCenter = 0.5f, float flAddTime = 0.0f );
	Vector	IteratePointOutOfDangerZones( const Vector& vecPos );

	Vector GetEndGameZoneOrigin( float flAddTime = 0.f );
	float	GetEndGameZoneRadius( float flAddTime = 0.f ) const;

#ifndef CLIENT_DLL
	virtual void		Precache( void );
	void	Spawn( void );

	void	UpdateDangerZoneController( void );
	virtual int UpdateTransmitState( void ) OVERRIDE { return SetTransmitState( FL_EDICT_ALWAYS ); }
	void	PrecomputeDangerZones( void );
	void	ResetMasterDangerZone( void );

	void	ApplyDamageToPlayers( void );

	int		GetNumWaveRewardsGranted( void ) const { return m_numWaveRewardsGranted; }
	void	RewardBombWaveMoney();
	void	PlayWarningSound();
	void	FireRocketStartLaunchOutput();

	void	EnableDangerZone( void );
	void	DisableDangerZone( void );
	CPointEntity *GetGasCanLaunchPosition( const Vector& vecStartPos );
#else
	virtual void OnDataChanged( DataUpdateType_t updateType );
#endif

	int GetDangerZoneCount() const { return m_DangerZones.Count(); }
	CDangerZone *GetDangerZone( int nIndex );

	CDangerZone *FindDangerZoneClosestToPoint( const Vector& vecPos );

	float GetStartTime() const { return m_flStartTime.Get(); }
	int GetNextBombWaveIndex() const;
	float GetTimeUntilNextWave() const;

	bool ShouldShowZonePrediction() const;

	float GetGameTimeOfWaveEnd( int iWave ) const;
	float GetFinalExpansionStartTime() const;
	float GetFinalExpansionTotalTime() const { return m_flFinalExpansionTime; }
	float GetEndGameStartTime() const;

	void GetZonesFromWaveID( int iWave, CUtlVector< CDangerZone* > &vecZones );
	CDangerZone* GetFinalZone() const { return m_hTheFinalZone.Get(); }
	bool IsFinalZone( CDangerZone* pZone ) const { return m_hTheFinalZone.Get() == pZone; }

private:

	CNetworkVar( bool, m_bDangerZoneControllerEnabled );
	CNetworkVar( Vector, m_vecEndGameCircleStart );
	CNetworkVar( Vector, m_vecEndGameCircleEnd );
	CNetworkVar( float, m_flStartTime );
	CNetworkVar( float, m_flFinalExpansionTime );
	CNetworkArray( CHandle< CDangerZone >, m_DangerZones, WORLD_HEX_NUM );
	CNetworkArray( float, m_flWaveEndTimes, NUM_BOMB_WAVE );
	CNetworkVar( CHandle< CDangerZone >, m_hTheFinalZone );

#ifndef CLIENT_DLL
	void PlayHornSound();
	void UpdateTheFinalZone();
	void CreateWarningEvent( const char *pszName, float flEventTime, bool bWaveMoney );
	void ReverseFloodFillBombWaves();
	void UpdateBombDropOrder();
	void UpdateBombLanding();
	void UpdateExtraExpansionTime();

	float m_flLastDangerZoneStatusLogged;
	float m_flLastDangerZoneDamageTime;
	bool m_bFirstBombWarning;
	int m_numWaveRewardsGranted;

	CUtlVector<CPointEntity*> m_pGasCanLaunchers;
#endif

};

CDangerZoneController *GetDangerZoneController( void );

#endif //DANGERZONE_CONTROLLER_H