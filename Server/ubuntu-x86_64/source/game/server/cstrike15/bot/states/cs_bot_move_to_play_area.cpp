//========= Copyright © Valve Corporation, All rights reserved. ============//
//
//=============================================================================//

#include "cbase.h"
#include "cs_bot.h"

#include "dangerzone_controller.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------------
CNavArea *MoveToPlayAreaState::FindNavInPlayArea( CCSBot *me ) const
{
	if ( GetDangerZoneController() && !GetDangerZoneController()->IsWithinPlayArea( me->GetAbsOrigin() ) )
	{
		const Vector vecPointInsideDangerZone = GetDangerZoneController()->MovePointIntoPlayArea( me->GetAbsOrigin(), RandomFloat( 0.1f, 0.6f ) );
		CNavArea *pNav = TheNavMesh->GetNearestNavArea( vecPointInsideDangerZone, true, 1000, false, false );
		return pNav;
	}

	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
void MoveToPlayAreaState::OnEnter( CCSBot *me )
{
	me->StandUp();
	me->Run();
	me->StopWaiting();
	me->DestroyPath();
	me->EquipKnife();

	me->SetDisposition( CCSBot::IGNORE_ENEMIES );

	me->PrintIfWatched( "Trying to get back into the playable area\n" );

	m_safeArea = NULL;
	m_searchTimer.Invalidate();
}
//--------------------------------------------------------------------------------------------------------------
void MoveToPlayAreaState::OnUpdate( CCSBot *me )
{

	if ( GetDangerZoneController() )
	{

		if ( GetDangerZoneController()->IsWithinPlayArea( me->GetAbsOrigin() ) )
		{
			if ( !m_searchTimer.HasStarted() )
			{
				m_searchTimer.Start( RandomFloat( 1.0f, 3.0f ) );
			}
			else if ( m_searchTimer.IsElapsed() )
			{
				// back in the zone
				me->UpdateLookAround();
				me->EquipBestWeapon();
				me->FireWeaponAtEnemy();

				me->Idle();
				return;
			}
		}
		
	}
	else
	{
		me->Idle();
	}

	if ( !m_safeArea )
	{
		m_safeArea = FindNavInPlayArea( me );
	}
	else
	{

		if ( me->UpdatePathMovement() != CCSBot::PROGRESSING )
		{
			me->ComputePath( m_safeArea->GetCenter(), FASTEST_ROUTE );
		}

	}


}

//--------------------------------------------------------------------------------------------------------------
void MoveToPlayAreaState::OnExit( CCSBot *me )
{
	me->EquipBestWeapon();
}
