#if !defined CS_PLAYER_SHARED_H
#define CS_PLAYER_SHARED_H

// 
// Configuration for using high priority entities by CS players
//
class CConfigurationForHighPriorityUseEntity_t
{
public:
	enum EPriority_t
	{	// Priority of use entities, higher number is higher priority for use
		k_EPriority_Default,
		k_EPriority_SurvivalBRC4,	// this will put player into immediate danger to evacuate
		k_EPriority_SurvivalDZDoor,	// this consumes player's money
		k_EPriority_Contract,		// this is a permanent player mod
		k_EPriority_TabletUpgrade,	// this is a permanent player mod
		k_EPriority_Parachute,		// this is a permanent player mod
		k_EPriority_Hostage,		// this is quick and can be relocated and dropped off in another place
		k_EPriority_HeavyArmor,		// this is next on priority because it protects the player from further damage AND can be relocated to other place and taken off there
		k_EPriority_Bomb,			// this is highest priority because defusing the bomb gets player out of immediate danger
	};
	enum EPlayerUseType_t
	{
		k_EPlayerUseType_Start,		// Player wants to initiate the use
		k_EPlayerUseType_Progress	// Player wants to make progress using the entity
	};
	enum EDistanceCheckType_t
	{
		k_EDistanceCheckType_3D,
		k_EDistanceCheckType_2D
	};

	CBaseEntity *m_pEntity;
	EPriority_t m_ePriority;
	EDistanceCheckType_t m_eDistanceCheckType;
	Vector m_pos;
	float m_flMaxUseDistance;
	float m_flLosCheckDistance;
	float m_flDotCheckAngle;
	float m_flDotCheckAngleMax;

public:
	// Check if this high priority use entity is better for use than the other one
	bool IsBetterForUseThan( CConfigurationForHighPriorityUseEntity_t const &other ) const;

	// Check if this entity can be used by the given player according to its use rules
	bool UseByPlayerNow( CCSPlayer *pPlayer, EPlayerUseType_t ePlayerUseType );
};

#ifdef GAME_DLL
//
// Helper configuration that allows other entities to establish prolonged +use contracts with players
//
class CEntitySupportForProlongedUse_t
{
public:
	CEntitySupportForProlongedUse_t();

	enum EBeginUseResult_t
	{
		k_EBeginUseResult_Unreachable,			// Target is unreachable or other transient error => entity should silently ignore it
		k_EBeginUseResult_BeingUsedByOther,		// Target is being used by other player => entity should inform the player (already ratelimited)
		k_EBeginUseResult_Started,				// Player started the use, entity should play effects and sounds, but the player is already locked in
		k_EBeginUseResult_InProgress,			// Target is in progress of being used by this player => entity should silently ignore it
		k_EBeginUseResult_Completed,			// Target use has been completed by this or other player => entity may ignore silently (or reset our flag)
	};
	EBeginUseResult_t ETryToBeginUse( CBaseAnimating *pThisEntity, CCSPlayer *pActivator );

	enum EUseInProgressOutcome_t
	{
		k_EUseInProgressOutcome_NotInUse,
		k_EUseInProgressOutcome_StillUsing,
		k_EUseInProgressOutcome_Aborted,
		k_EUseInProgressOutcome_Completed,
	};
	EUseInProgressOutcome_t ESupportUseThink( CBaseAnimating *pThisEntity, CCSPlayer **ppActivator );

public:
	bool BIsConfigured() const { return m_configCSPlayerBlockingUseAction != k_CSPlayerBlockingUseAction_None; }
	void ConfigSetPlayerBlockingUseAction( CSPlayerBlockingUseAction_t eAction );
	void ConfigSetUseDurationToCompletion( float flDuration );
	void ConfigSetSingleUse( bool bSingleUse );

protected:
	CSPlayerBlockingUseAction_t m_configCSPlayerBlockingUseAction;
	float m_configUseDurationToCompletion;
	bool m_configSingleUse;

protected:
	CHandle<CCSPlayer> m_hCurrentlyUsingPlayer;
	bool m_bCurrentlyBeingUsed;
	float m_flLastUseDetectionTime;
	float m_flUseCompletionTime;
	bool m_bSuccessfulUseCompleted;
};
#endif

struct HalloweenMaskModelStruct
{
	const char* model;
};

extern const HalloweenMaskModelStruct s_HalloweenMaskModels[];
extern const int s_HalloweenMaskModelsSize;

extern const HalloweenMaskModelStruct s_HalloweenMaskModelsCompetitive[];
extern const int s_HalloweenMaskModelsCompetitiveSize;

extern const HalloweenMaskModelStruct s_HalloweenMaskModelsTF2[];
extern const int s_HalloweenMaskModelsTF2Size;

enum EPlayerProgressBarReason {
	// These are stored negated in CCSPlayer::m_iProgressBarDuration

	k_EPlayerProgressBarReason_Unknown = 0, // legacy (old demos)
	k_EPlayerProgressBarReason_Incomplete = 1,
	k_EPlayerProgressBarReason_Complete = 2,
};


#endif // CS_PLAYER_SHARED_H
