//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_C4_H
#define WEAPON_C4_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_csbase.h"
#include "utlvector.h"

#define NUM_BEEPS 7

#if defined( CLIENT_DLL )

	#define CC4 C_C4

#else

	// ------------------------------------------------------------------------------------------ //
	// CPlantedC4 class.
	// ------------------------------------------------------------------------------------------ //

	class CBombTarget;

	class CPlantedC4 : public CBaseAnimating
	{
	public:
		DECLARE_CLASS( CPlantedC4, CBaseAnimating );
		DECLARE_DATADESC();
		DECLARE_SERVERCLASS();

		DECLARE_PREDICTABLE();

		CPlantedC4();
		virtual ~CPlantedC4();

		virtual void Spawn();

		virtual int  UpdateTransmitState();
		virtual void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );
		virtual int  ShouldTransmit( const CCheckTransmitInfo *pInfo );

		static CPlantedC4* ShootSatchelCharge( CCSPlayer *pevOwner, Vector vecStart, QAngle vecAngles );
		virtual void Precache();
		
		// Set these flags so CTs can use the C4 to disarm it.
		virtual int	ObjectCaps() { return BaseClass::ObjectCaps() | (FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE); }

		void SetBombTarget( CBombTarget* pBombTarget );

		inline bool IsBombActive( void ) { return m_bBombTicking; }
		inline int GetBombSite( void ) { return m_nBombSite; }

		CCSPlayer* GetPlanter( void ) { return m_pPlanter; }
		void SetPlanter( CCSPlayer* player ) { m_pPlanter = player; }

		CCSPlayer* GetDefuser( void ) { return m_pBombDefuser; }

		void SetPlantedAfterPickup( bool plantedAfterPickup ) { m_bPlantedAfterPickup = plantedAfterPickup; }

		bool m_bPlantedAtQuestTarget;

		void Explode();

		void SetHolidayBodyGroup( CBaseAnimating* pBomb );

	public:

		CNetworkVar( bool, m_bBombTicking );
		CNetworkVar( float, m_flC4Blow );
		CNetworkVar( int, m_nBombSite );

		COutputEvent m_OnBombDefused; 
		COutputEvent m_OnBombBeginDefuse; 
		COutputEvent m_OnBombDefuseAborted;

		// used for the training map where the map spawns the bomb and sets the timer manually
		virtual void ActivateSetTimerLength( float flTimerLength );

		bool m_bCannotBeDefused;

	protected:
		virtual void Init( CCSPlayer *pevOwner, Vector vecStart, QAngle vecAngles, bool	bTrainingPlacedByPlayer );

		void C4Think();

		bool			m_bTrainingPlacedByPlayer;
		bool			m_bHasExploded;

		virtual void OnExplode( trace_t *pGroundTrace );
		virtual void OnDefuse( CCSPlayer* pDefuser );
		virtual void RadioAboutToExplode( int teamNumber );

		void DoExplosionDamage( float flDamage );
		void DoExplosionEffects( trace_t* pGroundTrace );

		CBombTarget* GetBombTarget();

	private:
		void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

		// Replicate timer length to the client for effects
		CNetworkVar( float, m_flTimerLength );

		// Info for defusing.
		bool			m_bBeingDefused;
		CHandle<CCSPlayer> m_pBombDefuser;
		float			m_fLastDefuseTime;
		CHandle<CBombTarget> m_pBombSite;

		CNetworkVar( float, m_flDefuseLength );		//How long does the defuse take? Depends on if a defuser was used
		CNetworkVar( float, m_flDefuseCountDown );	//What time does the defuse complete?
		CNetworkVar( bool, m_bBombDefused ); 
		CNetworkVar( CHandle<CCSPlayer>, m_hBombDefuser );

		// Control panel
		void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
		void GetControlPanelClassName( int nPanelIndex, const char *&pPanelName );
		void SpawnControlPanels( void );
		void RemoveControlPanels( void );

		typedef CHandle<CVGuiScreen>	ScreenHandle_t;
		CUtlVector<ScreenHandle_t>	m_hScreens;

		int m_iProgressBarTime;
		bool m_bVoiceAlertFired;
		bool m_bVoiceAlertPlayed[TEAM_MAXCOUNT];
		float m_flNextBotBeepTime; // only send beep events to the bots once per second

		// [tj] We need to store who planted the bomb so we can track who deserves credits for the kills
		CHandle<CCSPlayer>  m_pPlanter;

		// [tj] We need to know if this was planted by a player who recovered the bomb
		bool m_bPlantedAfterPickup;
	};

	// ------------------------------------------------------------------------------------------ //
	// CPlantedC4 class.
	// ------------------------------------------------------------------------------------------ //

	class CPlantedC4Training : public CPlantedC4
	{
	public:
		DECLARE_CLASS( CPlantedC4Training, CPlantedC4 );
		DECLARE_DATADESC();
		DECLARE_PREDICTABLE();
		//DECLARE_SERVERCLASS(); // This networks to clients as a CPlantedC4; it has no additional netvars and there is no C_PlantedC4Training

		void	InputActivateSetTimerLength( inputdata_t &inputdata );
		COutputEvent m_OnBombExploded;	//Fired when the bomb explodes

	protected:
		virtual void OnDefuse( CCSPlayer* pDefuser ) OVERRIDE;
		virtual void OnExplode( trace_t *pGroundTrace ) OVERRIDE;
	};

	extern CUtlVector< CPlantedC4* > g_PlantedC4s;

#endif

#define WEAPON_C4_CLASSNAME "weapon_c4"
#define PLANTED_C4_CLASSNAME "planted_c4"

class CC4 : public CWeaponCSBase
{
public:
	DECLARE_CLASS( CC4, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CC4();
	virtual ~CC4();
	
	virtual void Spawn();

	void ItemPostFrame();
	virtual void WeaponReset( void );
	virtual void PrimaryAttack();
	virtual void WeaponIdle();
	virtual void UpdateShieldState( void );
	virtual float GetMaxSpeed() const;

	virtual bool			Deploy( void );								// returns true is deploy was successful
	virtual bool			Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

	#ifdef CLIENT_DLL
	
		void ClientThink( void );
		virtual void	OnDataChanged( DataUpdateType_t type );
		virtual void	UpdateOnRemove( void );
		virtual bool OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );
		char *GetScreenText( void );
		char m_szScreenText[32];

		CUtlReference<CNewParticleEffect> m_hC4LED;
		void CreateLEDEffect( void );
		void RemoveLEDEffect( void );
		EHANDLE		   m_hParticleEffectOwner;

		virtual Vector GetGlowColor( void ) { return Vector( (240.0f/255.0f), (225.0f/255.0f), (90.0f/255.0f) ); }
	#else
		virtual void Precache();
		virtual int  UpdateTransmitState();
		virtual int  ShouldTransmit( const CCheckTransmitInfo *pInfo );
		virtual void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
		virtual unsigned int PhysicsSolidMaskForEntity( void ) const;

		virtual bool ShouldRemoveOnRoundRestart();

        void SetDroppedFromDeath (bool droppedFromDeath) { m_bDroppedFromDeath = droppedFromDeath; }
	
		void Think( void );
		void ResetToLastValidPlayerHeldPosition();
		virtual void PhysicsTouchTriggers(const Vector *pPrevAbsOrigin = NULL);

private:
		Vector m_vecLastValidPlayerHeldPosition;
public:

	#endif

	void AbortBombPlant();

	void PlayArmingBeeps( void );
	void PlayPlantInitSound( void );
	virtual void	OnPickedUp( CBaseCombatCharacter *pNewOwner );
	virtual void	Drop( const Vector &vecVelocity );

	CNetworkVar( bool, m_bStartedArming );
	CNetworkVar( float, m_fArmedTime );
	CNetworkVar( bool, m_bBombPlacedAnimation );
	CNetworkVar( bool, m_bShowC4LED );
	CNetworkVar( bool, m_bIsPlantingViaUse );

	virtual bool IsRemoveable( void ) { return false; }

private:	
	bool m_bPlayedArmingBeeps[NUM_BEEPS];
	bool m_bBombPlanted;

	// [tj] we want to store if this bomb was dropped because the original owner was killed
	bool m_bDroppedFromDeath;

private:
	
	CC4( const CC4 & );
};

	extern CUtlVector< CC4* > g_WeaponC4s;

#endif // WEAPON_C4_H
