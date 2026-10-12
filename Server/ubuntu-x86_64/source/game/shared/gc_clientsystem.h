//=========== Copyright (c), Valve Corporation, All rights reserved. ==========
//
//
//
//=============================================================================
#ifndef GC_CLIENTSYSTEM_H
#define GC_CLIENTSYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
	#include "clientsteamcontext.h"
#endif

#include "econ/econ_store.h"

//=============================================================================
//
//	Client GC System.
//
//=============================================================================
class CGCClientSystem : public CAutoGameSystemPerFrame
{
	DECLARE_CLASS_GAMEROOT( CGCClientSystem, CAutoGameSystem );

public:

	// Constructor/Destructor.
	CGCClientSystem();
	~CGCClientSystem();

	// Init/Shutdown.
	virtual void PostInit();
	virtual void LevelInitPreEntity();
	virtual void LevelShutdownPostEntity();
	virtual void Shutdown();

	void Activate() { SetupGC(); }
	
	// Updates.  Gameservers do this at a slightly different place than clients
	#ifdef CLIENT_DLL
		virtual void Update( float frametime );
	#else
		virtual void GCThink();
	#endif

	// Connection status
	bool BConnectedtoGC() const;
	void SetSessionNeed( uint32 nSessionNeed, bool bWantSession ) { m_GCClient.SetSessionNeed( nSessionNeed, bWantSession ); }

	// GC Messages
	bool BSendMessage( uint32 unMsgType, const uint8 *pubData, uint32 cubData );
	bool BSendMessage( const GCSDK::CGCMsgBase& msg );
	bool BSendMessage( const GCSDK::CProtoBufMsgBase& msg );

	// GC SOCache
	GCSDK::CGCClientSharedObjectCache *GetSOCache( GCSDK::SOID_t soid );
	GCSDK::CGCClientSharedObjectCache *FindOrAddSOCache( GCSDK::SOID_t soid );

	// GC Client
	GCSDK::CGCClient *GetGCClient();

//	// Steam
//	#ifndef CLIENT_DLL
//		void GameServerActivate();
//	#endif

	void ProcessWelcomeMessage( const CMsgClientWelcome & msg );
	void ProcessUpdatedGcRTime32Notification( RTime32 gcTime );

	bool HasReceivedGCHello() const;
	double GetReceivedGCHelloPlatTime() const { return m_dblPlatTimeAtWelcome; }
	RTime32 GetGCCurrentTime() const;
	RTime32 GCTimeToLocalTime( RTime32 gcTime ) const;
	RTime32 GCTimeFromLocalTime( RTime32 gcTime ) const;

	enum ECurrency GetCurrency() const { return m_eCurrency; }
	CMsgClientWelcome_Location const GetLocation() const { return m_location; }
	char const * GetTxnCountryCode() const { return m_sTxnCountryCode.Get(); }

	void SetPartnerAccountBalance( uint32 unPartnerAccountBalance, char const *szBalanceReplenishmentURL )
	{
		m_unPartnerAccountBalance = unPartnerAccountBalance;
		m_strPartnerAccountBalanceURL = szBalanceReplenishmentURL;
		m_bPartnerAccountBalanceValid = true;
	}
	bool BPartnerAccountBalanceValid() const { return m_bPartnerAccountBalanceValid; }
	uint32 GetPartnerAccountBalance() const { return m_unPartnerAccountBalance; }
	char const * GetPartnerAccountBalanceURL() const { return m_strPartnerAccountBalanceURL; }

protected:

	void SetupGC();
	virtual void InitGC();
	virtual void PreInitGC() {}
	virtual void PostInitGC() {}

	void AddAllowedConnectionlessMessage( uint32 unMsg ) { m_GCClient.AddAllowedConnectionlessMessage( unMsg ); }

	bool	m_bHasReceivedGCWelcome;
	int		m_nGCTimeOffset;
	RTime32 m_rtGCTimeAtWelcome;
	double m_dblPlatTimeAtWelcome;
	enum ECurrency m_eCurrency;
	CUtlString m_strPartnerAccountBalanceURL;
	CUtlString m_sTxnCountryCode;
	uint32 m_unPartnerAccountBalance;
	bool m_bPartnerAccountBalanceValid;

	CMsgClientWelcome_Location m_location;

private:

	bool m_bInittedGC;
	GCSDK::CGCClient m_GCClient;

	#ifdef CLIENT_DLL
		void SteamLoggedOnCallback( const SteamLoggedOnChange_t &loggedOnState );
	#else
		STEAM_GAMESERVER_CALLBACK( CGCClientSystem, OnLogonSuccess, SteamServersConnected_t, m_CallbackLogonSuccess );
	#endif


	friend class CGCClientSystemJob;
};

void SetGCClientSystem( CGCClientSystem* pGCClientSystem );
CGCClientSystem *GCClientSystem();

#endif // GC_CLIENTSYSTEM_H

