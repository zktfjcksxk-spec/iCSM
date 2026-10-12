//--------------------------------------------------------------------------------------------------------

#ifndef PROP_COUNTER_H
#define PROP_COUNTER_H

#ifdef CLIENT_DLL

class C_PropCounter : public C_BaseAnimating
{
	DECLARE_CLASS( C_PropCounter, C_BaseAnimating );
	DECLARE_CLIENTCLASS();

public:
	float GetDisplayValue( void );

	virtual void OnPreDataChanged( DataUpdateType_t updateType ) OVERRIDE;
	virtual void OnDataChanged( DataUpdateType_t updateType ) OVERRIDE;

private:
	float m_flDisplayValue;
	float m_flDisplayValueLocal;
	float m_flTimeOfLastValueChange;
	float m_flPreviousValue;
};

#else

class CPropCounter : public CBaseAnimating
{
	DECLARE_CLASS( CPropCounter, CBaseAnimating );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CPropCounter();

	virtual void Precache();
	virtual void Spawn( void );

	virtual int ShouldTransmit( const CCheckTransmitInfo *pInfo ) { return FL_EDICT_ALWAYS; }

public:
	void SetDisplayValue( float flValue ) { m_flDisplayValue = flValue; }

private:

	CNetworkVar( float, m_flDisplayValue );
	int m_nInitialValue;
};

#endif

#endif //PROP_COUNTER_H