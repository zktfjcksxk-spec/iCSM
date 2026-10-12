//--------------------------------------------------------------------------------------------------------

#ifndef GRASSBURN_H
#define GRASSBURN_H

#ifdef CLIENT_DLL
#define CGrassBurn C_GrassBurn
#endif

#ifdef CLIENT_DLL
class CGrassBurn : public C_BaseEntity
{
	DECLARE_CLASS( CGrassBurn, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();
#else
class CGrassBurn : public CPointEntity
{
	DECLARE_CLASS( CGrassBurn, CPointEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
#endif

	CGrassBurn();

	//CNetworkArray( Vector, m_vecGrassBurns, 32 );
	CNetworkVar( float, m_flGrassBurnClearTime );

#ifndef CLIENT_DLL
	void	Spawn( void );
	virtual int UpdateTransmitState( void ) OVERRIDE { return SetTransmitState( FL_EDICT_ALWAYS ); }

	void ResetGrassBurn( void ) { m_flGrassBurnClearTime.Set( gpGlobals->curtime ); }

#else

	virtual void OnPreDataChanged( DataUpdateType_t updateType ) OVERRIDE;
	virtual void OnDataChanged( DataUpdateType_t updateType ) OVERRIDE;

	void Update( void );

	void TorchGrassAt( Vector vecPosition );

private:
	float m_bClientPendingClear;
	float m_flGrassBurnClearTimeLocal;
	CUtlVector<Vector> m_vecGrassBurnPositions;
#endif

};

CGrassBurn *GetGrassBurn( void );

#endif //GRASSBURN_H