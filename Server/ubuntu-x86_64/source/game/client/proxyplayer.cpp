//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#include <keyvalues.h>
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialsystem.h"
#include "functionproxy.h"
#include "c_cs_player.h"
#include "weapon_csbase.h"
#include "predicted_viewmodel.h"
#include "cs_client_gamestats.h"
#include "econ/econ_item_schema.h"
#include "cstrike15_gcconstants.h"
#include "br_items.h"
#include "prop_counter.h"
#include "weapon_c4.h"
#include "weapon_tablet.h"

#if defined ( PANORAMA_ENABLE )
#include "panorama/csgo_panorama.h"
#include "panorama/ui_itempreview_panel.h"
#endif

#include "imaterialproxydict.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


class CMoneyProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
};

bool CMoneyProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CResultProxy::Init( pMaterial, pKeyValues ) )
		return false;

	return true;
}

void CMoneyProxy::OnBind( void *pC_BaseEntity )
{
	if ( !pC_BaseEntity )
		return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( !pEntity )
		return;

	C_BaseViewModel *pViewModel = dynamic_cast<C_BaseViewModel*>(pEntity);
	if ( pViewModel )
	{
		C_CSPlayer *pPlayer = ToCSPlayer( pViewModel->GetPredictionOwner() );
		if ( pPlayer )
		{
			m_pResult->SetIntValue( pPlayer->GetAccount() );
		}
	}
}

EXPOSE_MATERIAL_PROXY( CMoneyProxy, MoneyProxy );


class CC4CompassArrowProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
};

bool CC4CompassArrowProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CResultProxy::Init( pMaterial, pKeyValues ) )
		return false;

	return true;
}

void CC4CompassArrowProxy::OnBind( void *pC_BaseEntity )
{
	/* Removed for partner depot */
}

EXPOSE_MATERIAL_PROXY( CC4CompassArrowProxy, C4CompassArrow );

class CTabletUpgradeProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
private:
	CValueInput m_nTabletUpgrade;
	CValueInput m_nValueWhenTrue;
	CValueInput m_nValueWhenFalse;
};

bool CTabletUpgradeProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CResultProxy::Init( pMaterial, pKeyValues ) )
		return false;

	m_nTabletUpgrade.Init( pMaterial, pKeyValues, "upgrade", -1 );
	m_nValueWhenTrue.Init( pMaterial, pKeyValues, "valuetrue", 1 );
	m_nValueWhenFalse.Init( pMaterial, pKeyValues, "valuefalse", 0 );

	return true;
}

void CTabletUpgradeProxy::OnBind( void *pC_BaseEntity )
{
	if ( !pC_BaseEntity )
		return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( !pEntity )
		return;

	CTablet *pTablet = NULL;

	// we need to get to the weapon
	if ( pEntity->IsWeaponWorldModel() )
	{
		CBaseWeaponWorldModel *pWepWorld = dynamic_cast<CBaseWeaponWorldModel*>(pEntity);
		if ( pWepWorld )
		{
			pTablet = dynamic_cast<CTablet*>(pWepWorld->m_hCombatWeaponParent->Get());
		}
	}
	else
	{
		pTablet = dynamic_cast<CTablet*>(pEntity);
	}

	if ( pTablet )
	{
		if ( pTablet->HasTabletUpgrade( (tablet_upgrade_type_t)m_nTabletUpgrade.GetInt() ) )
		{
			m_pResult->SetFloatValue( m_nValueWhenTrue.GetFloat() );
		}
		else
		{
			m_pResult->SetFloatValue( m_nValueWhenFalse.GetFloat() );
		}
	}

}

EXPOSE_MATERIAL_PROXY( CTabletUpgradeProxy, TabletUpgradeProxy );

class CCounterProxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
private:

	int GetDigitFrameForValue( float flValue, int iDesiredDigit );

	CFloatInput	m_flDisplayDigit;
	IMaterialVar *m_pTextureScrollVar;
};

bool CCounterProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !m_flDisplayDigit.Init( pMaterial, pKeyValues, "displayDigit", 0 ) )
		return false;

	bool foundVar;

	m_pTextureScrollVar = pMaterial->FindVar( "$basetexturetransform", &foundVar, false );

	if ( !foundVar )
		return false;

	return true;
}

int CCounterProxy::GetDigitFrameForValue( float flValue, int iDesiredDigit )
{
	// ok, let's convert seconds to mins:seconds
	//int nMins = flValue / 60.0f;
	//int nSecs = fmod( flValue, 60.0f );
	//int nDigitFrame = (100 * nMins) + nSecs;

	int nValue = flValue;

	// get the [0-9] value of the digit we want
	int iDigitCount = MIN( iDesiredDigit, 10 );
	for ( int i = 0; i < iDigitCount; i++ )
	{
		nValue /= 10;
	}
	nValue %= 10;

	return nValue;
}

void CCounterProxy::OnBind( void *pC_BaseEntity )
{
	if ( !m_pTextureScrollVar )
		return;

	if ( !pC_BaseEntity )
		return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( !pEntity )
		return;

	C_PropCounter *pCounter = dynamic_cast<C_PropCounter*>(pEntity);
	if ( pCounter )
	{
		float flCounterValue = clamp( pCounter->GetDisplayValue(), 0.0f, 99999.0f );

		int nNumDigits = 1;
		if ( flCounterValue > 0.0f )
			nNumDigits = clamp( log10( flCounterValue ) + 1, 1, 5 );

		int nDisplayDigit = (int)m_flDisplayDigit.GetFloat();

		if ( nNumDigits > nDisplayDigit )
		{
			int nDigitFrame = GetDigitFrameForValue( flCounterValue, (int)m_flDisplayDigit.GetFloat() );

			VMatrix mat = VMatrix::GetIdentityMatrix();
			Vector vecTranslation = vec3_origin;
			vecTranslation.y = (float)nDigitFrame * 0.1f;

			// flip anim
			if ( nDigitFrame != GetDigitFrameForValue( flCounterValue + 1, (int)m_flDisplayDigit.GetFloat() ) )
			{
				float flFraction = fmod( flCounterValue, 1.0f );
				vecTranslation.y += clamp( Bias( flFraction, 0.05f ), 0, 1 ) * 0.1f;
			}

			mat.SetTranslation( vecTranslation );
			m_pTextureScrollVar->SetMatrixValue( mat );
		}
		else
		{
			VMatrix mat = VMatrix::GetIdentityMatrix();
			Vector vecTranslation = vec3_origin;
			
			if ( nNumDigits == nDisplayDigit )
			{
				// $
				vecTranslation.x = -0.25f;
				vecTranslation.y = 0.1f;
			}
			else
			{
				// blank
				vecTranslation.x = -0.25f;
				vecTranslation.y = 0.0f;
			}
			
			mat.SetTranslation( vecTranslation );
			m_pTextureScrollVar->SetMatrixValue( mat );
		}
	}

}

EXPOSE_MATERIAL_PROXY( CCounterProxy, CounterProxy );

//-----------------------------------------------------------------------------
// Returns the proximity of the player to the entity
//-----------------------------------------------------------------------------

class CPlayerProximityProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	float	m_Factor;
};

bool CPlayerProximityProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	m_Factor = pKeyValues->GetFloat( "scale", 0.002 );
	return true;
}

void CPlayerProximityProxy::OnBind( void *pC_BaseEntity )
{
	if (!pC_BaseEntity)
		return;

	// Find the distance between the player and this entity....
	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	C_BaseEntity* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	Vector delta;
	VectorSubtract( pEntity->WorldSpaceCenter(), pPlayer->WorldSpaceCenter(), delta );

	Assert( m_pResult );
	SetFloatResult( delta.Length() * m_Factor );
}

EXPOSE_MATERIAL_PROXY( CPlayerProximityProxy, PlayerProximity );


//-----------------------------------------------------------------------------
// Returns true if the player's team matches that of the entity the proxy material is attached to
//-----------------------------------------------------------------------------

class CPlayerTeamMatchProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
};

bool CPlayerTeamMatchProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	return true;
}

void CPlayerTeamMatchProxy::OnBind( void *pC_BaseEntity )
{
	if (!pC_BaseEntity)
		return;

	// Find the distance between the player and this entity....
	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	C_BaseEntity* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	Assert( m_pResult );
	SetFloatResult( (pEntity->GetTeamNumber() == pPlayer->GetTeamNumber()) ? 1.0 : 0.0 );
}

EXPOSE_MATERIAL_PROXY( CPlayerTeamMatchProxy, PlayerTeamMatch );


//-----------------------------------------------------------------------------
// Returns the player view direction
//-----------------------------------------------------------------------------
class CPlayerViewProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	float	m_Factor;
};

bool CPlayerViewProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	m_Factor = pKeyValues->GetFloat( "scale", 2 );
	return true;
}

void CPlayerViewProxy::OnBind( void *pC_BaseEntity )
{
	if (!pC_BaseEntity)
		return;

	// Find the view angle between the player and this entity....
	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	C_BaseEntity* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	Vector delta;
	VectorSubtract( pEntity->WorldSpaceCenter(), pPlayer->WorldSpaceCenter(), delta );
	VectorNormalize( delta );

	Vector forward;
	AngleVectors( pPlayer->GetAbsAngles(), &forward );

	Assert( m_pResult );
	SetFloatResult( DotProduct( forward, delta ) * m_Factor );
}

EXPOSE_MATERIAL_PROXY( CPlayerViewProxy, PlayerView );


//-----------------------------------------------------------------------------
// Returns the player speed
//-----------------------------------------------------------------------------
class CPlayerSpeedProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	float	m_Factor;
};

bool CPlayerSpeedProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	m_Factor = pKeyValues->GetFloat( "scale", 0.005 );
	return true;
}

void CPlayerSpeedProxy::OnBind( void *pC_BaseEntity )
{
	// Find the player speed....
	C_BaseEntity* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	Assert( m_pResult );
	SetFloatResult( pPlayer->GetLocalVelocity().Length() * m_Factor );
}

EXPOSE_MATERIAL_PROXY( CPlayerSpeedProxy, PlayerSpeed );


//-----------------------------------------------------------------------------
// Returns the player position
//-----------------------------------------------------------------------------
class CPlayerPositionProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	float	m_Factor;
};

bool CPlayerPositionProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;
	  
	m_Factor = pKeyValues->GetFloat( "scale", 0.005 );
	return true;
}

void CPlayerPositionProxy::OnBind( void *pC_BaseEntity )
{
	// Find the player speed....
	C_BaseEntity* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	// This is actually a vector...
	Assert( m_pResult );
	Vector res;
	VectorMultiply( pPlayer->WorldSpaceCenter(), m_Factor, res ); 
	m_pResult->SetVecValue( res.Base(), 3 );
}

EXPOSE_MATERIAL_PROXY( CPlayerPositionProxy, PlayerPosition );


//-----------------------------------------------------------------------------
// Returns the entity speed
//-----------------------------------------------------------------------------
class CEntitySpeedProxy : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity );
};

void CEntitySpeedProxy::OnBind( void *pC_BaseEntity )
{
	// Find the view angle between the player and this entity....
	if (!pC_BaseEntity)
		return;

	// Find the view angle between the player and this entity....
	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );

	Assert( m_pResult );
	m_pResult->SetFloatValue( pEntity->GetLocalVelocity().Length() );
}

EXPOSE_MATERIAL_PROXY( CEntitySpeedProxy, EntitySpeed );


//-----------------------------------------------------------------------------
// Returns a random # from 0 - 1 specific to the entity it's applied to
//-----------------------------------------------------------------------------
class CEntityRandomProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_Factor;
};

bool CEntityRandomProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Factor.Init( pMaterial, pKeyValues, "scale", 1 ))
		return false;

	return true;
}

void CEntityRandomProxy::OnBind( void *pC_BaseEntity )
{
	// Find the view angle between the player and this entity....
	if (!pC_BaseEntity)
		return;

	// Find the view angle between the player and this entity....
	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );

	Assert( m_pResult );
	m_pResult->SetFloatValue( pEntity->ProxyRandomValue() * m_Factor.GetFloat() );
}

EXPOSE_MATERIAL_PROXY( CEntityRandomProxy, EntityRandom );

#include "utlrbtree.h"

//-----------------------------------------------------------------------------
// StatTrak 'kill odometer' support: given a numerical value expressed as a string, pick a texture frame to represent a given digit
//-----------------------------------------------------------------------------
class CStatTrakDigitProxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );

	virtual bool HelperOnBindGetStatTrakScore( void *pC_BaseEntity, int *piScore );

private:
	CFloatInput	m_flDisplayDigit; // the particular digit we want to display
	CFloatInput	m_flTrimZeros;
};


bool CStatTrakDigitProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_flDisplayDigit.Init( pMaterial, pKeyValues, "displayDigit", 0 ))
		return false;

	if (!m_flTrimZeros.Init( pMaterial, pKeyValues, "trimZeros", 0 ))
		return false;

	return true;
}

#include "c_cs_player.h"
#include "weapon_csbase.h"
#include "predicted_viewmodel.h"

bool CStatTrakDigitProxy::HelperOnBindGetStatTrakScore( void *pC_BaseEntity, int *piScore )
{
	if ( !pC_BaseEntity )
		return false;

	if ( !piScore )
		return false;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity )
	{
		// StatTrak modules are children of their accompanying viewmodels
		C_BaseViewModel *pViewModel = dynamic_cast< C_BaseViewModel* >( pEntity->GetMoveParent() );
		if ( pViewModel )
		{
			C_CSPlayer *pPlayer = ToCSPlayer( pViewModel->GetPredictionOwner() );
			if ( pPlayer )
			{
				CWeaponCSBase *pWeap = pPlayer->GetActiveCSWeapon();
				if ( pWeap )
				{
					if ( CEconItemView *pItemView = pWeap->GetEconItemView() )
					{
						// Always get headshot-trak(TM)
						*piScore = pItemView->GetKillEaterValueByType( 0 );
					}
				}
			}
		}
	}
	return true;
}

void CStatTrakDigitProxy::OnBind( void *pC_BaseEntity )
{
	int nKillEaterAltScore = 0;
	bool bHasScoreToDisplay = HelperOnBindGetStatTrakScore( pC_BaseEntity, &nKillEaterAltScore );
	if ( !bHasScoreToDisplay )
	{	// Force flashing numbers
		SetFloatResult( (int) fmod( gpGlobals->curtime, 10.0f ) );
		return;
	}

	int iDesiredDigit = (int)m_flDisplayDigit.GetFloat();

	// trim preceding zeros
	if ( m_flTrimZeros.GetFloat() > 0 )
	{
		if ( pow( 10.0f, iDesiredDigit ) > nKillEaterAltScore )
		{
			SetFloatResult( 10.0f ); //assumed blank frame
			return;
		}
	}

	// get the [0-9] value of the digit we want
	int iDigitCount = MIN( iDesiredDigit, 10 );
	for ( int i=0; i<iDigitCount; i++ )
	{
		nKillEaterAltScore /= 10;
	}
	nKillEaterAltScore %= 10;

	SetFloatResult( nKillEaterAltScore );
}

EXPOSE_MATERIAL_PROXY( CStatTrakDigitProxy, StatTrakDigit );


//-----------------------------------------------------------------------------
// StatTrak 'kill odometer' support: given a numerical value expressed as a string, pick a texture frame to represent a given digit
//-----------------------------------------------------------------------------
class CStatTrakDigitProxyForModelWeaponPreviewPanel : public CStatTrakDigitProxy
{
public:
	virtual bool HelperOnBindGetStatTrakScore( void *pC_BaseEntity, int *puiScore ) OVERRIDE
	{
		/* Removed for partner depot */
		return false;
	}
};
EXPOSE_MATERIAL_PROXY( CStatTrakDigitProxyForModelWeaponPreviewPanel, StatTrakDigitForModelWeaponPreview );

class CAttachmentDriverProxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
private:
	CValueInput m_AttachmentDriverName;
	CStudioHdr *m_pHdr;
	int m_nAttachmentIndex;
	bool m_bValid;
};

bool CAttachmentDriverProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CResultProxy::Init( pMaterial, pKeyValues ) )
		return false;

	m_nAttachmentIndex = -1;
	m_pHdr = NULL;
	m_bValid = false;

	if ( !m_AttachmentDriverName.Init( pMaterial, pKeyValues, "attachmentname", "" ) )
		return false;

	m_bValid = true;

	return true;
}

void CAttachmentDriverProxy::OnBind( void *pC_BaseEntity )
{
	if ( !m_bValid )
		return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity && pEntity->GetBaseAnimating() )
	{
		C_BaseAnimating *pAnimating = pEntity->GetBaseAnimating();
		if ( pAnimating )
		{
			CStudioHdr *pHdr = pAnimating->GetModelPtr();
			if ( pHdr )
			{
				if ( m_nAttachmentIndex < 0 || pHdr != m_pHdr )
				{
					m_pHdr = pHdr;
					m_nAttachmentIndex = pAnimating->LookupAttachment( m_AttachmentDriverName.GetString() );
					if ( m_nAttachmentIndex < 0 )
					{
						m_bValid = false;
						return;
					}
				}

				Vector vecAttachPos;
				pAnimating->GetAttachmentLocal( m_nAttachmentIndex, vecAttachPos );

				SetFloatResult( clamp( vecAttachPos.Length(), 0.0f, 1.0f ) );
				
				return;
			}
		}
	}

	m_bValid = false;
}

EXPOSE_MATERIAL_PROXY( CAttachmentDriverProxy, AttachmentDriver );

class CAnimCycleProxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
private:
	CValueInput m_nSeq;
	CValueInput m_nFallback;
	CValueInput m_nInvert;
	CValueInput m_nBias;
	CValueInput m_nRemapRangeMin;
	CValueInput m_nRemapRangeMax;
	CValueInput m_nNoOpFallback;
};

bool CAnimCycleProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CResultProxy::Init( pMaterial, pKeyValues ) )
		return false;
	
	m_nSeq.Init( pMaterial, pKeyValues, "sequence", -1 );
	m_nFallback.Init( pMaterial, pKeyValues, "fallback", 0 );
	m_nNoOpFallback.Init( pMaterial, pKeyValues, "noopfallback", 0 );
	m_nInvert.Init( pMaterial, pKeyValues, "invert", 0 );
	m_nBias.Init( pMaterial, pKeyValues, "bias", 0.5f );
	m_nRemapRangeMin.Init( pMaterial, pKeyValues, "remaprangemin", 0.0f );
	m_nRemapRangeMax.Init( pMaterial, pKeyValues, "remaprangemax", 1.0f );

	return true;
}

void CAnimCycleProxy::OnBind( void *pC_BaseEntity )
{
	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity && pEntity->GetBaseAnimating() )
	{
		C_BaseAnimating *pAnimating = pEntity->GetBaseAnimating();
		if ( pAnimating )
		{
			if ( m_nSeq.GetInt() >= 0 && m_nSeq.GetInt() != pAnimating->GetSequence() )
			{
				if ( m_nNoOpFallback.GetInt() == 0 )
				{
					SetFloatResult( m_nFallback.GetFloat() );
				}
				return;
			}

			float flResult = fmod( clamp( pAnimating->GetCycle(), 0.0f, 0.9999f ), 1.0f );
			if ( flResult > 0.9998f )
				flResult = 1.0f;

			flResult = RemapValClamped( flResult, m_nRemapRangeMin.GetFloat(), m_nRemapRangeMax.GetFloat(), 0.0f, 1.0f );

			if ( m_nInvert.GetInt() )
			{
				flResult = 1.0f - flResult;
			}

			if ( flResult > 0.0f && flResult < 1.0f && m_nBias.GetFloat() != 0.5f )
			{
				flResult = Bias( flResult, m_nBias.GetFloat() );
			}

			SetFloatResult( clamp( flResult, 0.0f, 1.0f ) );
		}
	}
}

EXPOSE_MATERIAL_PROXY( CAnimCycleProxy, AnimCycle );

class CTaserMeterProxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
private:
	IMaterialVar *m_pTextureScrollVar;
};

bool CTaserMeterProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{

	bool foundVar;
	m_pTextureScrollVar = pMaterial->FindVar( "$basetexturetransform", &foundVar, false );
	if ( !foundVar )
		return false;

	return true;
}

#include "weapon_csbasegun.h"
void CTaserMeterProxy::OnBind( void *pC_BaseEntity )
{
	if ( !m_pTextureScrollVar )
		return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity )
	{
		C_BaseViewModel *pViewModel = dynamic_cast<C_BaseViewModel*>(pEntity);
		if ( pViewModel )
		{
			CWeaponCSBaseGun *pGun = dynamic_cast<CWeaponCSBaseGun*>(pViewModel->GetWeapon());
			if ( pGun )
			{
				float flCharge = pGun->GetCharge() - 1.0f;

				VMatrix mat( 1.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 1.0f, 0.0f, flCharge,
					0.0f, 0.0f, 1.0f, 0.0f,
					0.0f, 0.0f, 0.0f, 1.0f );

				m_pTextureScrollVar->SetMatrixValue( mat );
			}

		}

	}

}

EXPOSE_MATERIAL_PROXY( CTaserMeterProxy, TaserMeter );


#ifdef IRONSIGHT
//-----------------------------------------------------------------------------
// IronSightAmount proxy
//-----------------------------------------------------------------------------
class CIronSightAmountProxy : public CResultProxy
{
public:
	virtual bool Init(IMaterial *pMaterial, KeyValues *pKeyValues);
	virtual void OnBind(void *pC_BaseEntity);
private:
	bool bInvert;
};


bool CIronSightAmountProxy::Init(IMaterial *pMaterial, KeyValues *pKeyValues)
{
	if (!CResultProxy::Init(pMaterial, pKeyValues))
		return false;

	bInvert = false;
	CFloatInput	m_flInvert;
	if ( m_flInvert.Init( pMaterial, pKeyValues, "invert" ) )
		bInvert = ( m_flInvert.GetFloat() > 0 );

	return true;
}

void CIronSightAmountProxy::OnBind(void *pC_BaseEntity)
{

	if (!pC_BaseEntity)
		return;
	
	C_BaseEntity *pEntity = BindArgToEntity(pC_BaseEntity);
	if (pEntity)
	{
		C_BaseViewModel *pViewModel = dynamic_cast<C_BaseViewModel*>(pEntity);
		if (pViewModel)
		{
			C_CSPlayer *pPlayer = ToCSPlayer(pViewModel->GetPredictionOwner());
			if (pPlayer)
			{
				CWeaponCSBase *pWeapon = pPlayer->GetActiveCSWeapon();
				if ( pWeapon && pWeapon->GetIronSightController() )
				{
					if ( bInvert )
					{
						SetFloatResult(Bias( 1.0f - pWeapon->GetIronSightController()->GetIronSightAmount(), 0.2f));
					}
					else
					{
						SetFloatResult(Bias(pWeapon->GetIronSightController()->GetIronSightAmount(), 0.2f));
					}
				}
			}
		}
	}

}


EXPOSE_MATERIAL_PROXY(CIronSightAmountProxy, IronSightAmount);
#endif //IRONSIGHT

//-----------------------------------------------------------------------------
// StatTrakIllum proxy
//-----------------------------------------------------------------------------
class CStatTrakIllumProxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_flMinVal;
	CFloatInput	m_flMaxVal;
};


bool CStatTrakIllumProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_flMinVal.Init( pMaterial, pKeyValues, "minVal", 0.5 ))
		return false;

	if (!m_flMaxVal.Init( pMaterial, pKeyValues, "maxVal", 1 ))
		return false;

	return true;
}

void CStatTrakIllumProxy::OnBind( void *pC_BaseEntity )
{

	if (!pC_BaseEntity)
		return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity )
	{
		// StatTrak modules are children of their accompanying viewmodels
		C_BaseViewModel *pViewModel = dynamic_cast< C_BaseViewModel* >( pEntity->GetMoveParent() );
		if ( pViewModel )
		{
			SetFloatResult( Lerp( pViewModel->GetStatTrakGlowMultiplier(), m_flMinVal.GetFloat(), m_flMaxVal.GetFloat() ) );
			return;
		}
	}

}


EXPOSE_MATERIAL_PROXY( CStatTrakIllumProxy, StatTrakIllum );


//-----------------------------------------------------------------------------
// WeaponLabelTextProxy
//-----------------------------------------------------------------------------
class CWeaponLabelTextProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
	virtual bool HelperOnBindGetLabel( void *pC_BaseEntity, const char **p_szLabel );

private:
	CFloatInput	m_flDisplayDigit;
	IMaterialVar *m_pTextureOffsetVar;
};

bool CWeaponLabelTextProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{

	if (!m_flDisplayDigit.Init( pMaterial, pKeyValues, "displayDigit", 0 ))
		return false;

	bool foundVar;
	m_pTextureOffsetVar = pMaterial->FindVar( "$basetexturetransform", &foundVar, false );
	if( !foundVar )
		return false;

	return true;
}

bool CWeaponLabelTextProxy::HelperOnBindGetLabel( void *pC_BaseEntity, const char **p_szLabel )
{
	if ( !pC_BaseEntity )
		return false;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity )
	{
		// uid modules are children of their accompanying viewmodels
		C_BaseViewModel *pViewModel = dynamic_cast< C_BaseViewModel* >( pEntity->GetMoveParent() );
		if ( pViewModel )
		{
			CBaseCombatWeapon *pWeapon = pViewModel->GetWeapon();
			if ( pWeapon )
			{
				CEconItemView *pItem = pWeapon->GetEconItemView();
				if ( pItem )
				{
					*p_szLabel = pItem->GetCustomName();
					return true;
				}
			}
		}
	}

	return false;
}

int V_utf8_strlen( const char* pszStr )
{
	int len = 0;
	while ( *pszStr )
	{
		++pszStr;
		++len;

		// advance past multi-byte characters
		while ( ( uint8( *pszStr ) & 0xc0 ) == 0x80 )
			++pszStr;
	}

	return len;
}

uint32 V_utf8_readchar( const char* pszStr )
{
	// attempt to read this character
	uint8 b0 = pszStr[0];
	if ( ( b0 & 0x80 ) == 0 )
	{
		// 1-byte / ascii
		return b0;
	}
	else if ( ( b0 & 0xe0 ) == 0xc0 )
	{
		// 2-byte
		uint8 b1 = pszStr[1];
		if ( ( b1 & 0xc0 ) == 0x80 )
			return ( uint32( b0 & 0x1f ) << 6 ) | ( b1 & 0x3f );
	}
	else if ( ( b0 & 0xf0 ) == 0xe0 )
	{
		// 3-byte
		uint8 b1 = pszStr[1];
		if ( ( b1 & 0xc0 ) == 0x80 )
		{
			uint8 b2 = pszStr[2];
			if ( ( b2 & 0xc0 ) == 0x80 )
				return ( uint32( b0 & 0x0f ) << 12 ) | ( uint32( b1 & 0x3f ) << 6 ) | ( b2 & 0x3f );
		}
	}
	else if ( ( b0 & 0xf8 ) == 0xf0 )
	{
		// 4-byte
		uint8 b1 = pszStr[1];
		if ( ( b1 & 0xc0 ) == 0x80 )
		{
			uint8 b2 = pszStr[2];
			if ( ( b2 & 0xc0 ) == 0x80 )
			{
				uint8 b3 = pszStr[3];
				if ( ( b3 & 0xc0 ) == 0x80 )
					return ( uint32( b0 & 0x0f ) << 18 ) | ( uint32( b1 & 0x3f ) << 12 ) | ( uint32( b2 & 0x3f ) << 6 ) | ( b3 & 0x3f );
			}
		}
	}

	// invalid encoding
	return 0;
}

uint32 V_utf8_index( const char* pszStr, int idx )
{
	if ( idx < 0 )
		return 0;

	while ( *pszStr )
	{
		if ( idx == 0 )
			return V_utf8_readchar( pszStr );

		// otherwise, advance character
		++pszStr;
		--idx;
		while ( ( uint8( *pszStr ) & 0xc0 ) == 0x80 )
			++pszStr;
	}

	// index outside of string
	return 0;
}

void CWeaponLabelTextProxy::OnBind( void *pC_BaseEntity )
{
	const char *p_szLabel = NULL;
	bool bHasLabel = HelperOnBindGetLabel( pC_BaseEntity, &p_szLabel );
	if ( !bHasLabel || !p_szLabel )
		p_szLabel = "";

	//get the digit index we need to display
	int nDigit = (int)m_flDisplayDigit.GetFloat();

	// Calculate UTF-8 text length to center text within NUM_UID_CHARS
	int nStrLen = V_utf8_strlen( p_szLabel );
	int nPrependSpaces = ( NUM_UID_CHARS - nStrLen ) / 2;
	nDigit -= nPrependSpaces;

	int nCharIndex = 0;
	if ( nDigit >= 0 && nDigit < nStrLen )
	{
		uint32 nUtf8Char = V_utf8_index( p_szLabel, nDigit );
		if ( nUtf8Char >= 32 && nUtf8Char < 128 )
			nCharIndex = nUtf8Char - 32;

		// invalid characters appear as spaces (ascii 32 / charindex 0)
	}

	int nIndexHoriz = nCharIndex % 12;
	int nIndexVertical = nCharIndex / 12;

	float flOffsetX = 0.083333f * nIndexHoriz;
	float flOffsetY =    0.125f * nIndexVertical;

	VMatrix mat( 1.0f,	0.0f,	0.0f,	flOffsetX,
		0.0f,	1.0f,	0.0f,	flOffsetY,
		0.0f,	0.0f,	1.0f,	0.0f,
		0.0f,	0.0f,	0.0f,	1.0f );

	m_pTextureOffsetVar->SetMatrixValue( mat );
}

EXPOSE_MATERIAL_PROXY( CWeaponLabelTextProxy, WeaponLabelText );


class CWeaponLabelTextProxyForModelWeaponPreviewPanel : public CWeaponLabelTextProxy
{
public:
	virtual bool HelperOnBindGetLabel( void *pC_BaseEntity, const char **p_szLabel )
	{
		/* Removed for partner depot */
		return false;
	}
};
EXPOSE_MATERIAL_PROXY( CWeaponLabelTextProxyForModelWeaponPreviewPanel, WeaponLabelTextPreview );


int g_HighlightedSticker = -1;
int g_PeelSticker = -1;

void CC_HighlightSticker(const CCommand& args)
{
	int nParam = atoi(args[1]);
	if ( nParam >= 0 && nParam <= 4 )
	{
		g_HighlightedSticker = nParam;
	}
}
static ConCommand highlight_sticker("highlight_sticker", CC_HighlightSticker, "", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

void CC_PeelSticker(const CCommand& args)
{
	int nParam = atoi(args[1]);
	if (nParam >= 0 && nParam <= 4)
	{
		g_PeelSticker = nParam;
	}
}
static ConCommand peel_sticker("peel_sticker", CC_PeelSticker, "", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY);

//-----------------------------------------------------------------------------
// Sticker selection proxy
//-----------------------------------------------------------------------------
class CStickerSelectionProxy : public CResultProxy
{
public:
	virtual bool Init(IMaterial *pMaterial, KeyValues *pKeyValues);
	virtual void OnBind(void *pC_BaseEntity);
	virtual void CheckMyGlobal( void );
	
	int m_nStickerIndex;
	float m_flSelectedness;
};

bool CStickerSelectionProxy::Init(IMaterial *pMaterial, KeyValues *pKeyValues)
{
	if (!CResultProxy::Init(pMaterial, pKeyValues))
		return false;

	CFloatInput flStickerIndex;
	if (!flStickerIndex.Init(pMaterial, pKeyValues, "stickerindex", 0))
		return false;
	m_nStickerIndex = (int)flStickerIndex.GetFloat();

	m_flSelectedness = 0.0f;

	return true;
}

void CStickerSelectionProxy::CheckMyGlobal( void )
{
	if (g_HighlightedSticker > -1 && m_nStickerIndex == g_HighlightedSticker)
	{
		m_flSelectedness = 1.0f;
		g_HighlightedSticker = -1;
	}
}

void CStickerSelectionProxy::OnBind(void *pC_BaseEntity)
{

	//if (!pC_BaseEntity)
	//	return;

	CheckMyGlobal();

	if ( m_flSelectedness > 0.01f )
	{
		m_flSelectedness = Approach( 0.0f, m_flSelectedness, gpGlobals->frametime * 0.5f );
		SetFloatResult( m_flSelectedness );
	}
	else if ( m_flSelectedness > 0.0f )
	{
		SetFloatResult( 0.0f );
	}

}

EXPOSE_MATERIAL_PROXY(CStickerSelectionProxy, StickerSelection);

class CStickerPeelProxy : public CStickerSelectionProxy
{
public:
	virtual void CheckMyGlobal( void );
};

void CStickerPeelProxy::CheckMyGlobal(void)
{
	if (g_PeelSticker > -1 && m_nStickerIndex == g_PeelSticker)
	{
		m_flSelectedness = 1.0f;
		g_PeelSticker = -1;
	}
}

EXPOSE_MATERIAL_PROXY(CStickerPeelProxy, StickerPeel);


//-----------------------------------------------------------------------------
// CrosshairColor proxy
//-----------------------------------------------------------------------------
extern ConVar cl_crosshaircolor_r;
extern ConVar cl_crosshaircolor_g;
extern ConVar cl_crosshaircolor_b;
class CCrossHairColorProxy : public CResultProxy
{
public:
	virtual bool Init(IMaterial *pMaterial, KeyValues *pKeyValues);
	virtual void OnBind(void *pC_BaseEntity);

	Vector m_vecLocalCrossHairColor;
};

bool CCrossHairColorProxy::Init(IMaterial *pMaterial, KeyValues *pKeyValues)
{
	if (!CResultProxy::Init(pMaterial, pKeyValues))
		return false;
	m_vecLocalCrossHairColor.Init();
	return true;
}

void CCrossHairColorProxy::OnBind(void *pC_BaseEntity)
{
	if ( m_vecLocalCrossHairColor.x != cl_crosshaircolor_r.GetFloat() || 
		 m_vecLocalCrossHairColor.y != cl_crosshaircolor_g.GetFloat() || 
		 m_vecLocalCrossHairColor.z != cl_crosshaircolor_b.GetFloat() )
	{

		m_vecLocalCrossHairColor.x = cl_crosshaircolor_r.GetFloat();
		m_vecLocalCrossHairColor.y = cl_crosshaircolor_g.GetFloat();
		m_vecLocalCrossHairColor.z = cl_crosshaircolor_b.GetFloat();

		SetVecResult(   (float)m_vecLocalCrossHairColor.x * 0.0039,
						(float)m_vecLocalCrossHairColor.y * 0.0039,
						(float)m_vecLocalCrossHairColor.z * 0.0039, 1);
	}
}

EXPOSE_MATERIAL_PROXY(CCrossHairColorProxy, CrossHairColor);


float g_flEconInspectPreviewTime = 0;

class CEconInspectPreviewTimeProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
};

bool CEconInspectPreviewTimeProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CResultProxy::Init( pMaterial, pKeyValues ) )
		return false;

	return true;
}

void CEconInspectPreviewTimeProxy::OnBind( void *pC_BaseEntity )
{
	Assert( m_pResult );
	SetFloatResult( gpGlobals->curtime - g_flEconInspectPreviewTime );
}

EXPOSE_MATERIAL_PROXY( CEconInspectPreviewTimeProxy, EconInspectPreviewTime );

