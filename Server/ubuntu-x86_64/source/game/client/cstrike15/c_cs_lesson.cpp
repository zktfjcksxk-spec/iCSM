//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Client handler implementations for instruction players how to play
//
//=============================================================================//

#include "cbase.h"

#include "c_gameinstructor.h"
#include "c_baselesson.h"
#include "c_cs_player.h"
#include "cs_gamerules.h"

#include "gametypes.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern CUtlDict< int, int > g_LessonActionMap;

extern ConVar gameinstructor_verbose;


enum Mod_LessonAction
{
	// Enum starts from end of LessonAction
	LESSON_ACTION_GAME_TYPE_IS = LESSON_ACTION_MOD_START,
	LESSON_ACTION_GAME_MODE_IS,
	LESSON_ACTION_CASUAL_OR_TRAINING,
	LESSON_ACTION_HAS_DEFUSER,
	LESSON_ACTION_HAS_ANY_ITEMS,
	LESSON_ACTION_HAS_MULTIPLE_ITEMS,
	LESSON_ACTION_HAS_MULTIPLE_WEAPONS,
	LESSON_ACTION_HAS_BUYMENU_OPEN,
	LESSON_ACTION_ROUND_STATE_IS,
	LESSON_ACTION_GET_WEAPON_TYPE,
	LESSON_ACTION_HAS_WEAPON_IN_SLOT,

	LESSON_ACTION_TOTAL
};

void CScriptedIconLesson::Mod_PreReadLessonsFromFile( void )
{
	// Add custom actions to the map
	CScriptedIconLesson::LessonActionMap.Insert( "game type is", LESSON_ACTION_GAME_TYPE_IS );
	CScriptedIconLesson::LessonActionMap.Insert( "game mode is", LESSON_ACTION_GAME_MODE_IS );
	CScriptedIconLesson::LessonActionMap.Insert( "is casual or training", LESSON_ACTION_CASUAL_OR_TRAINING );
	CScriptedIconLesson::LessonActionMap.Insert( "has defuser", LESSON_ACTION_HAS_DEFUSER );
	CScriptedIconLesson::LessonActionMap.Insert( "has any items", LESSON_ACTION_HAS_ANY_ITEMS );
	CScriptedIconLesson::LessonActionMap.Insert( "has multiple items", LESSON_ACTION_HAS_MULTIPLE_ITEMS );
	CScriptedIconLesson::LessonActionMap.Insert( "has multiple weapons", LESSON_ACTION_HAS_MULTIPLE_WEAPONS );
	CScriptedIconLesson::LessonActionMap.Insert( "is buymenu open", LESSON_ACTION_HAS_BUYMENU_OPEN );
	CScriptedIconLesson::LessonActionMap.Insert( "round state is", LESSON_ACTION_ROUND_STATE_IS );		  //0=freezetime; 1=mid-round; 2=round is over;
	CScriptedIconLesson::LessonActionMap.Insert( "get weapon type", LESSON_ACTION_GET_WEAPON_TYPE );
	CScriptedIconLesson::LessonActionMap.Insert( "has weapon in slot", LESSON_ACTION_HAS_WEAPON_IN_SLOT );
}


bool CScriptedIconLesson::Mod_ProcessElementAction( int iAction, bool bNot, const char *pchVarName, EHANDLE &hVar, const CGameInstructorSymbol *pchParamName, float fParam, C_BaseEntity *pParam, const char *pchParam, bool &bModHandled )
{
	// Assume we're going to handle the action
	bModHandled = true;

	C_BaseEntity *pVar = hVar.Get();

	switch ( iAction )
	{
		case LESSON_ACTION_GAME_TYPE_IS:
		{
			return ( bNot ) ? ( g_pGameTypes->GetCurrentGameType() != fParam ) : ( g_pGameTypes->GetCurrentGameType() == fParam );
			break;
		}
		case LESSON_ACTION_GAME_MODE_IS:
		{
			return ( bNot ) ? ( g_pGameTypes->GetCurrentGameMode() != fParam ) : ( g_pGameTypes->GetCurrentGameMode() == fParam );
			break;
		}
		case LESSON_ACTION_CASUAL_OR_TRAINING:
		{
			return ( (g_pGameTypes->GetCurrentGameType() == CS_GameType_Classic && g_pGameTypes->GetCurrentGameMode() == CS_GameMode::Classic_Casual) || g_pGameTypes->GetCurrentGameType() == CS_GameType_Training );
			break;
		}
		case LESSON_ACTION_HAS_DEFUSER:
			{
				C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( pVar );

				if ( !pPlayer )
				{
					if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
					{
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->HasDefuser()", pchVarName );
						ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
						ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as CSPlayer returned NULL!\n" );
					}

					return false;
				}

				bool bHasDefuser = pPlayer->HasDefuser();

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->HasSpareWeapon()", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bHasDefuser ? "true" : "false" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
				}

				return ( bNot ) ? ( !bHasDefuser ) : ( bHasDefuser );
			}

		case LESSON_ACTION_HAS_ANY_ITEMS:
			{
				C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( pVar );

				if ( !pPlayer )
				{
					if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
					{
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->GetCSWeapon( ITEM )", pchVarName );
						ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
						ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as CSPlayer returned NULL!\n" );
					}

					return false;
				}

				bool bHasItem = false;
				for ( int i = 0; i < MAX_WEAPONS; ++i )
				{
					C_BaseCombatWeapon* pWeapon = pPlayer->GetWeapon( i );
					C_WeaponCSBase* pCSWeapon = dynamic_cast< C_WeaponCSBase* >( pWeapon );

					if ( pCSWeapon )
					{
						CSWeaponType nWeaponType = pCSWeapon->GetWeaponType();
						if ( nWeaponType == WEAPONTYPE_KNIFE || nWeaponType == WEAPONTYPE_GRENADE || nWeaponType == WEAPONTYPE_C4 )
						{
							bHasItem = true;
							break;
						}
					}
				}

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->GetCSWeapon( ITEM )", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bHasItem ? "true" : "false" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
				}

				return ( bNot ) ? ( !bHasItem ) : ( bHasItem );
			}
		case LESSON_ACTION_HAS_MULTIPLE_ITEMS:
			{
				C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( pVar );

				if ( !pPlayer )
				{
					if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
					{
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] item count ", pchVarName );
						ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
						ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as CSPlayer returned NULL!\n" );
					}

					return false;
				}

				int nItems = 0;
				for ( int i = 0; i < MAX_WEAPONS; ++i )
				{
					C_BaseCombatWeapon* pWeapon = pPlayer->GetWeapon( i );
					C_WeaponCSBase* pCSWeapon = assert_cast< C_WeaponCSBase* >( pWeapon );

					if ( pCSWeapon )
					{
						CSWeaponType nWeaponType = pCSWeapon->GetWeaponType();
						if ( nWeaponType == WEAPONTYPE_KNIFE || nWeaponType == WEAPONTYPE_GRENADE || nWeaponType == WEAPONTYPE_C4 )
						{
							nItems++;
						}
					}
				}

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] item count ", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%d ", ( nItems ) );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
				}

				return ( bNot ) ? ( nItems < 2 ) : ( nItems >= 2 );
			}
		case LESSON_ACTION_HAS_MULTIPLE_WEAPONS:
			{
				C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( pVar );

				if ( !pPlayer )
				{
					if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
					{
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] weapon count ", pchVarName );
						ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
						ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as CSPlayer returned NULL!\n" );
					}
					return false;
				}

				int nWeapons = 0;
				for ( int i = 0; i < MAX_WEAPONS; ++i )
				{
					C_BaseCombatWeapon* pWeapon = pPlayer->GetWeapon( i );
					C_WeaponCSBase* pCSWeapon = assert_cast< C_WeaponCSBase* >( pWeapon );

					if ( pCSWeapon )
					{
						CSWeaponType nWeaponType = pCSWeapon->GetWeaponType();
						if ( nWeaponType == WEAPONTYPE_PISTOL
							|| nWeaponType == WEAPONTYPE_SUBMACHINEGUN
							|| nWeaponType == WEAPONTYPE_RIFLE
							|| nWeaponType == WEAPONTYPE_SHOTGUN
							|| nWeaponType == WEAPONTYPE_SNIPER_RIFLE
							|| nWeaponType == WEAPONTYPE_MACHINEGUN )
							nWeapons++;
					}
				}

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] weapon count ", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%d ", ( nWeapons ) );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
				}

				return ( bNot ) ? ( nWeapons < 2 ) : ( nWeapons >= 2 );
			}
		case LESSON_ACTION_HAS_BUYMENU_OPEN:
			{
				C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( pVar );
				if ( !pPlayer )
				{
					if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
					{
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->IsBuyMenuOpen()", pchVarName );
						ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
						ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as CSPlayer returned NULL!\n" );
					}

					return false;
				}

				bool bHasMenuOpen = pPlayer->IsBuyMenuOpen();

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->IsBuyMenuOpen()", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bHasMenuOpen ? "true" : "false" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
				}

				return ( bNot ) ? ( !bHasMenuOpen ) : ( bHasMenuOpen );
			}
		case LESSON_ACTION_ROUND_STATE_IS:
			{
				int RoundState;
				if ( CSGameRules()->IsFreezePeriod() )
					{
						if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
						{
							ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t Round is in freeze time.\n" );
						}
						RoundState = 0;// freeze time
                    }
                else if ( CSGameRules()->IsRoundOver() )
                    {
						if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
						{
							ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t Round is over.\n" );
						}
						RoundState = 2;// round is over
                    }
                else
                    {
						if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
						{
							ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t Round is in progress.\n" );
						}
						RoundState = 1;// in the round
                    }

				return ( bNot ) ? ( RoundState != fParam ) : ( RoundState == fParam );
            }
		case LESSON_ACTION_GET_WEAPON_TYPE:
			{
				CWeaponCSBase *pWeapon = dynamic_cast< CWeaponCSBase* >( pVar );
				if ( !pWeapon )
				{
					if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
					{
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->Weapon_CanUse([%s])\n" ) : ( "\t[%s]->Weapon_CanUse([%s])\n" ), pchVarName, pchParamName->String() );
						ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tParam BaseCombatWeapon returned NULL!\n" );
					}

					return false;
				}

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t Weapon type is %d (%s).\n", pWeapon->GetWeaponType(), pWeapon->GetEconItemView()->GetStaticData()->GetWeaponTypeString() );
				}

				m_fOutput = pWeapon->GetWeaponType();

				return true;
			}
			case LESSON_ACTION_HAS_WEAPON_IN_SLOT:
			{
				C_CSPlayer *pPlayer = ( pVar->IsPlayer() ? ToCSPlayer( pVar ) : NULL );
				if ( !pPlayer )
				{
					if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
					{
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->HasWeaponInSlot()", pchVarName );
						ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
						ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as CSPlayer returned NULL!\n" );
					}

					return false;
				}

				bool bHasWeaponInSlot = pPlayer->HasWeaponInSlot( (gear_slot_t)(int)fParam );

				return ( bNot ) ? !bHasWeaponInSlot : bHasWeaponInSlot;
			}

		default:
			// Didn't handle this action
			bModHandled = false;
			break;
	}

	return false;
}


bool C_GameInstructor::Mod_HiddenByOtherElements( void )
{
	//C_ASW_Marine *pLocalMarine = C_ASW_Marine::GetLocalMarine();
	//if ( pLocalMarine && pLocalMarine->IsControllingTurret() )
	//{
	//	return true;
	//}

	return false;
}
