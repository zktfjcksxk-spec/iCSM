//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Hud element to show various game state icons near money
//=============================================================================//

#ifndef CSGO_HUDGAMEICONS_H_
#define CSGO_HUDGAMEICONS_H_

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?

class CCSGO_HudGameIcons_Symbols;

class CCSGO_HudGameIcons : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudGameIcons, panorama::CPanel2D );

public:
	CCSGO_HudGameIcons( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudGameIcons();

	// CHudElement functions
	virtual void Init( void ) OVERRIDE;
	virtual void LevelInit( void ) OVERRIDE;
	virtual void LevelShutdown( void ) OVERRIDE;
	virtual void ProcessInput( void ) OVERRIDE;
	virtual void SetActive( bool bActive ) OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;
	
	virtual void FireGameEvent( IGameEvent *pEvent ) OVERRIDE;

	// CPanel2D overrides
	virtual void OnLayoutReloaded() OVERRIDE;

private:
	void Reset();
	void UpdateHostages( C_CSPlayer* pLocalPlayer, bool bChangedPlayer );

	CUtlVector<panorama::CPanel2D*> m_HostagePanels;
	CHandle<C_CSPlayer> m_hLocalPlayer;
	
	// On the client we can't instantaneously tell a rescued hostage apart from a dead one.
	// So we mark them as rescued when we receive a hostage_rescue game event
	bool m_bRescuedHostages[MAX_HOSTAGES];
	void ResetHostages();
	void MarkHostageRescued( int hostageEntityId );

	bool m_bVisible;

	const CCSGO_HudGameIcons_Symbols& Symbols();
};

#endif // CSGO_HUDGAMEICONS_H_