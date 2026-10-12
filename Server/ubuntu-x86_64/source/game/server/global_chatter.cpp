
#include "cbase.h"
#include "global_chatter.h"
#include "ai_baseactor.h"
#include "ai_criteria.h"

#define GLOBAL_CHATTER_CACHE_CHATTERINFO 0

static void sGlobalChatterInfoUpdated( IConVar *var, const char *pOldValue, float flOldValue );

ConVar global_chatter_info( "global_chatter_info", "", FCVAR_RELEASE,
	"Map/mode-specific responserules criteria used by calls to UTIL_GlobalChatter",
	sGlobalChatterInfoUpdated
	);

// TODO: Does this need to be an entity?
class CGlobalChatter : public CBaseEntity
{
public:
	DECLARE_CLASS( CGlobalChatter, CBaseEntity );

	CGlobalChatter();
	~CGlobalChatter();

	virtual void Spawn() OVERRIDE;

	void Chatter( const char* concept );
	void UpdateInfo(); // reads global_chatter_info convar

	CHandle<CAI_BaseActor> mActor;
#if GLOBAL_CHATTER_CACHE_CHATTERINFO
	CUtlVector<ResponseContext_t> mResponseContexts;
#endif

public:
	static CGlobalChatter* GetInstance();
};
LINK_ENTITY_TO_CLASS( global_chatter, CGlobalChatter );

CGlobalChatter* gGlobalChatter = nullptr;

CGlobalChatter::CGlobalChatter()
{
	//DevMsg( 1, "Global chatter create\n" );
}

CGlobalChatter::~CGlobalChatter()
{
	if ( gGlobalChatter == this )
		gGlobalChatter = nullptr;

	//DevMsg( 1, "Global chatter destroy\n" );
}

void CGlobalChatter::Spawn()
{
	if ( gGlobalChatter != nullptr )
	{
		UTIL_Remove( this );
		return;
	}

	gGlobalChatter = this;
	UpdateInfo();
}

void CGlobalChatter::Chatter( const char* concept )
{
	// Get our NPC (or spawn it if it doesn't exist)
	CAI_BaseActor* pActor = mActor;
	if ( !pActor )
	{
		//DevMsg( 1, "Global chatter actor spawn\n" );

		mActor = pActor = assert_cast<CAI_BaseActor*>( CBaseEntity::CreateNoSpawn( "generic_actor", vec3_origin, vec3_angle ) );

		// Needs a model or bad things happen.
		// We'll use an invisible chicken because it's always precached.
		// And also because it's hilarious.
		pActor->SetModel( "models/chicken/chicken.mdl" );

		// Spawn it
		DispatchSpawn( pActor );

		// Disable collision, gravity, and movement
		pActor->SetMoveType( MOVETYPE_NOCLIP );
		pActor->AddSolidFlags( FSOLID_NOT_SOLID );
		pActor->SetSolid( SOLID_NONE );
		pActor->AddFlag( FL_DONTTOUCH );

		// Disable rendering
		pActor->AddEffects( EF_NODRAW | EF_NOSHADOW | EF_NORECEIVESHADOW );

		// Disable damage
		pActor->m_takedamage = DAMAGE_NO;	// This disables all forms of damage, including radius damage from grenades..
		pActor->AddFlag( FL_GODMODE );		// Just in case some damage gets through, this flag makes us invincible.
		pActor->AddFlag( FL_NOTARGET );		// Stop any AI characters from trying to aim at this object.
	}

	//DevMsg( 2, "GlobalChatter \"%s\" context \"%s\"\n", concept, global_chatter_info.GetString() );

	// If it's too slow to parse chatterContext all the time (i.e. if this code starts getting used way more ofthen
	// than it is, implement the extra stuff here.
#if GLOBAL_CHATTER_CACHE_CHATTERINFO
#error Not fully implemented yet, needs to add all relevant criteria here
	// Create criteria
	AI_CriteriaSet criteriaSet;
	FOR_EACH_VEC( mResponseContexts, i )
	{
		criteriaSet.AppendCriteria( mResponseContexts[i].m_iszName, mResponseContexts[i].m_iszValue );
	}

	pActor->Speak( concept, criteriaSet );
#else
	// Get additional chatter context (if any).
	// (if we have the empty string, pass null in so we don't get extra warnings from SplitContextInfo)
	const char* chatterContext = global_chatter_info.GetString();
	if ( chatterContext != nullptr && chatterContext[0] == '\0' )
		chatterContext = nullptr;

	pActor->Speak( concept, chatterContext );
#endif
}

/*static*/ CGlobalChatter* CGlobalChatter::GetInstance()
{
	return gGlobalChatter;
}

void CGlobalChatter::UpdateInfo()
{
#if GLOBAL_CHATTER_CACHE_CHATTERINFO
	mResponseContexts.Purge();

	// parse criteria
	const char* szContext = global_chatter_info.GetString();

	// No context data specified, early out.  (SplitContext spams an error message in this case)
	if ( !szContext || !*szContext )
		return;

	const char *p = szContext;
	while ( p )
	{
		char key[128];
		char value[128];
		float duration = 0.0f;

		p = SplitContext( p, key, sizeof( key ), value, sizeof( value ), &duration, szContext );
		// ignore durations here

		ResponseContext_t contextElem;
		contextElem.m_iszName = AllocPooledString( key );
		contextElem.m_iszValue = AllocPooledString( value );
		contextElem.m_fExpirationTime = 0.0f;

		mResponseContexts.AddToTail( contextElem );
	}
#endif
}

static void sGlobalChatterInfoUpdated( IConVar *var, const char *pOldValue, float flOldValue )
{
	CGlobalChatter* pChatter = CGlobalChatter::GetInstance();

	if ( pChatter )
		pChatter->UpdateInfo();
}

void UTIL_GlobalChatter( const char* concept )
{
	CGlobalChatter* pChatter = CGlobalChatter::GetInstance();

	if ( !pChatter )
	{
		// spawn a CGlobalChatter
		pChatter = assert_cast<CGlobalChatter*>( CBaseEntity::Create( "global_chatter", vec3_origin, vec3_angle ) );
		Assert( pChatter == CGlobalChatter::GetInstance() );

		// Not collideable / shootable / etc.
		pChatter->SetMoveType( MOVETYPE_NOCLIP );
		pChatter->AddFlag( FL_GODMODE );
		pChatter->AddFlag( FL_DONTTOUCH );
		pChatter->AddFlag( FL_NOTARGET );
		pChatter->AddSolidFlags( FSOLID_NOT_SOLID );
		pChatter->AddEffects( EF_NODRAW | EF_NOSHADOW | EF_NORECEIVESHADOW );
		pChatter->SetSolid( SOLID_NONE );
		pChatter->m_takedamage = DAMAGE_NO;
	}

	pChatter->Chatter( concept );
}
