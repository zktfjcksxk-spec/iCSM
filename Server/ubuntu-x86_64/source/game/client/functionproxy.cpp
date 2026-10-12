//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "functionproxy.h"
#include <keyvalues.h>
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterial.h"
#include "iclientrenderable.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Helper class to deal with floating point inputs
//-----------------------------------------------------------------------------
bool CFloatInput::Init( IMaterial *pMaterial, KeyValues *pKeyValues, const char *pKeyName, float flDefault )
{
	m_pFloatVar = NULL;
	KeyValues *pSection = pKeyValues->FindKey( pKeyName );
	if (pSection)
	{
		if (pSection->GetDataType() == KeyValues::TYPE_STRING)
		{
			const char *pVarName = pSection->GetString();

			// Look for numbers...
			float flValue;
			int nCount = sscanf( pVarName, "%f", &flValue );
			if (nCount == 1)
			{
				m_flValue = flValue;
				return true;
			}

			// Look for array specification...
			char pTemp[256];
			if (strchr(pVarName, '['))
			{		 
				// strip off the array...
				Q_strncpy( pTemp, pVarName, 256 );
				char *pArray = strchr( pTemp, '[' );
				*pArray++ = 0;

				char* pIEnd;
				m_FloatVecComp = strtol( pArray, &pIEnd, 10 );

				// Use the version without the array...
				pVarName = pTemp;
			}
			else
			{
				m_FloatVecComp = -1;
			}

			bool bFoundVar;
			m_pFloatVar = pMaterial->FindVar( pVarName, &bFoundVar, true );
			if (!bFoundVar)
				return false;
		}
		else
		{
			m_flValue = pSection->GetFloat();
		}
	}
	else
	{
		m_flValue = flDefault;
	}
	return true;
}

float CFloatInput::GetFloat() const
{
	if (!m_pFloatVar)
		return m_flValue;
	
	if( m_FloatVecComp < 0 )
		return m_pFloatVar->GetFloatValue();

	int iVecSize = m_pFloatVar->VectorSize();
	if ( m_FloatVecComp >= iVecSize )
		return 0;
	
	// don't stomp stack if invalid vector size
	if ( iVecSize > 4 )
		return 0;

	float v[4];
	m_pFloatVar->GetVecValue( v, iVecSize );
	return v[m_FloatVecComp];
}


//-----------------------------------------------------------------------------
// Helper class to deal with arbitrary inputs
//-----------------------------------------------------------------------------
CValueInput::CValueInput()
{
	mType = VALUE_TYPE_INT;
	m_nVal = 0;
	m_VecInfo = -1;
}

CValueInput::~CValueInput()
{
	switch ( mType )
	{
	case VALUE_TYPE_VECTOR:
		delete[] m_pflVec;
		break;
	case VALUE_TYPE_STRING:
		delete[] m_pStr;
		break;
	}
}

bool CValueInput::InitFromKV( IMaterial *pMaterial, KeyValues *pKeyValues, const char *pKeyName )
{
	KeyValues *pSection = pKeyValues->FindKey( pKeyName );

	if ( !pSection )
	{
		// Not a parse failure, just "initialize with default"
		mType = VALUE_TYPE_INVALID;
		return true;
	}

	if ( pSection->GetDataType() == KeyValues::TYPE_STRING )
	{
		const char *szValue = pSection->GetString();

		// empty string, treat as string
		if ( !szValue || !*szValue )
		{
			mType = VALUE_TYPE_STRING;
			char* result = new char[1];
			*result = 0;
			m_pStr = result;
			return true;
		}

		// Look for numbers...
		char* p;
		int nValue = strtol( szValue, &p, 10 );
		if ( *p == 0 )
		{
			mType = VALUE_TYPE_INT;
			m_nVal = nValue;
			return true;
		}

		float flValue = strtod( szValue, &p );
		if ( *p == 0 )
		{
			mType = VALUE_TYPE_FLOAT;
			m_flVal = flValue;
			return true;
		}

		// Look for vector
		if ( *szValue == '[' )
		{
			CUtlVector<float> vec;
			const char* pParse = szValue + 1; // skip '['
			
			for ( ;; )
			{
				// skip whitespace
				while ( V_isspace( *pParse ) )
					++pParse;

				// bad parse, no ]
				if ( *pParse == 0 )
					return false;

				// check for end of vector
				if ( *pParse == ']' )
					break;

				// get next number
				char* nextParse;
				float val = strtod( pParse, &nextParse );

				// garbage in string, fail
				if ( nextParse == pParse )
					return false;

				pParse = nextParse;

				// add the value
				vec.AddToTail( val );
			}

			// successful parse if we get here
			mType = VALUE_TYPE_VECTOR;
			m_VecInfo = vec.Count();
			m_pflVec = new float[m_VecInfo];
			memcpy( m_pflVec, vec.Base(), sizeof( float ) * m_VecInfo );
			return true;
		}

		// Otherwise it must be a variable reference, either for an entire variable (e.g. "$alpha", "$color")
		// or a variable component (e.g. "$color[3]").  Look it up now.

		// Check for array specification...
		// e.g. $color[3] refers to the 3rd element of the vector variable $color
		char pTemp[256];
		if ( strchr( szValue, '[' ) )
		{
			// strip off the array...
			Q_strncpy( pTemp, szValue, 256 );
			char *pArray = strchr( pTemp, '[' );
			*pArray++ = 0;

			char* pIEnd;
			m_VecInfo = strtol( pArray, &pIEnd, 10 );

			// no number in brackets
			if ( pIEnd == pArray )
				return false;

			// look for terminating ']'
			pArray = pIEnd;
			while ( V_isspace( *pArray ) )
				pArray++;

			if ( *pArray != ']' )
				return false;

			// look for end of string
			pArray++;
			while ( V_isspace( *pArray ) )
				pArray++;
			if ( *pArray != 0 )
				return false;

			// Now pTemp contains the variable name, look it up
			szValue = pTemp;
		}
		else
		{
			m_VecInfo = -1;
		}

		bool bFoundVar;
		IMaterialVar* pVar = pMaterial->FindVar( szValue, &bFoundVar, true );
		if ( !bFoundVar )
			return false;

		mType = VALUE_TYPE_VAR;
		m_pVar = pVar;
		return true;
	}
	else
	{
		// Not sure if we want to check for more specific stuff here (ints?) or always treat as float?
		mType = VALUE_TYPE_FLOAT;
		m_flVal = pSection->GetFloat();
		return true;
	}
}


bool CValueInput::Init( IMaterial *pMaterial, KeyValues *pKeyValues, const char *pKeyName, float flDefault )
{
	if ( !InitFromKV( pMaterial, pKeyValues, pKeyName ) )
		return false;

	if ( mType == VALUE_TYPE_INVALID )
	{
		mType = VALUE_TYPE_FLOAT;
		m_flVal = flDefault;
	}

	return true;
}

bool CValueInput::Init( IMaterial* pMaterial, KeyValues* pKeyValues, const char* pKeyName, int nDefault )
{
	if ( !InitFromKV( pMaterial, pKeyValues, pKeyName ) )
		return false;

	if ( mType == VALUE_TYPE_INVALID )
	{
		mType = VALUE_TYPE_INT;
		m_nVal = nDefault;
	}

	return true;
}

bool CValueInput::Init( IMaterial* pMaterial, KeyValues* pKeyValues, const char* pKeyName, const char* pStringDefault )
{
	if ( !InitFromKV( pMaterial, pKeyValues, pKeyName ) )
		return false;

	if ( mType == VALUE_TYPE_INVALID )
	{
		mType = VALUE_TYPE_STRING;
		char* result = new char[strlen( pStringDefault ) + 1];
		strcpy( result, pStringDefault );
		m_pStr = result;
	}

	return true;
}

bool CValueInput::Init( IMaterial* pMaterial, KeyValues* pKeyValues, const char* pKeyName, const float* pVecDefault, int numEntries )
{
	if ( !InitFromKV( pMaterial, pKeyValues, pKeyName ) )
		return false;

	if ( mType == VALUE_TYPE_INVALID )
	{
		mType = VALUE_TYPE_VECTOR;
		m_VecInfo = numEntries;
		m_pflVec = new float[numEntries];
		memcpy( m_pflVec, pVecDefault, sizeof( float ) * numEntries );
	}

	return true;
}

MaterialVarType_t CValueInput::GetType( int& vecSize ) const
{
	vecSize = -1;

	switch(mType)
	{
	case VALUE_TYPE_INVALID:
		return MATERIAL_VAR_TYPE_FLOAT;

	case VALUE_TYPE_VAR:
		if ( m_VecInfo >= 0 )
			return MATERIAL_VAR_TYPE_FLOAT;
		
		if ( m_pVar->GetType() == MATERIAL_VAR_TYPE_VECTOR )
			vecSize = m_pVar->VectorSize();

		return m_pVar->GetType();

	case VALUE_TYPE_INT:
		return MATERIAL_VAR_TYPE_INT;

	case VALUE_TYPE_FLOAT:
		return MATERIAL_VAR_TYPE_FLOAT;

	case VALUE_TYPE_VECTOR:
		vecSize = m_VecInfo;
		return MATERIAL_VAR_TYPE_VECTOR;

	case VALUE_TYPE_STRING:
		return MATERIAL_VAR_TYPE_STRING;
	}

	Assert( false );
	return MATERIAL_VAR_TYPE_FLOAT;
}

int CValueInput::GetInt() const
{
	switch(mType)
	{
	case VALUE_TYPE_INVALID:
		return 0;

	case VALUE_TYPE_VAR:
		if ( m_VecInfo >= 0 )
		{
			int iVecSize = m_pVar->VectorSize();

			if ( m_VecInfo >= iVecSize )
				return 0;

			if ( iVecSize > 4 )
				return 0;

			float vecVals[4];
			m_pVar->GetVecValue( vecVals, iVecSize );
			return ( int )vecVals[m_VecInfo];
		}

		return m_pVar->GetIntValue();

	case VALUE_TYPE_INT:
		return m_nVal;

	case VALUE_TYPE_FLOAT:
		return ( int )m_flVal;

	case VALUE_TYPE_VECTOR:
		if ( m_VecInfo <= 0 )
			return 0;

		return ( int )m_pflVec[0];

	case VALUE_TYPE_STRING:
		return V_atoi( m_pStr );
	}

	Assert( false );
	return 0;
}

float CValueInput::GetFloat() const
{
	switch ( mType )
	{
	case VALUE_TYPE_INVALID:
		return 0;

	case VALUE_TYPE_VAR:
		if ( m_VecInfo >= 0 )
		{
			int iVecSize = m_pVar->VectorSize();

			if ( m_VecInfo >= iVecSize )
				return 0;

			if ( iVecSize > 4 )
				return 0;

			float vecVals[4];
			m_pVar->GetVecValue( vecVals, iVecSize );
			return vecVals[m_VecInfo];
		}

		return m_pVar->GetFloatValue();

	case VALUE_TYPE_INT:
		return ( float )m_nVal;

	case VALUE_TYPE_FLOAT:
		return m_flVal;

	case VALUE_TYPE_VECTOR:
		if ( m_VecInfo <= 0 )
			return 0;

		return m_pflVec[0];

	case VALUE_TYPE_STRING:
		return V_atof( m_pStr );
	}

	Assert( false );
	return 0;
}

void CValueInput::GetVecValue( float* target, int nComps ) const
{
	if ( mType == VALUE_TYPE_VAR && m_VecInfo == 0 )
	{
		m_pVar->GetVecValue( target, nComps );
		return;
	}

	if ( mType == VALUE_TYPE_VECTOR )
	{
		int vCopy = m_VecInfo;
		if ( vCopy > nComps )
			vCopy = nComps;

		int i;
		for ( i = 0; i < vCopy; ++i )
			target[i] = m_pflVec[i];

		// fill any extra entries with 0
		for ( ; i < nComps; ++i )
			target[i] = 0.0f;

		return;
	}

	// otherwise just splat float value over all components
	float result = GetFloat();
	for ( int i = 0; i < nComps; ++i )
		target[i] = result;
}

const char* CValueInput::GetString() const
{
	switch ( mType )
	{
	case VALUE_TYPE_VAR:
		if ( m_VecInfo >= 0 )
			return "";

		return m_pVar->GetStringValue();

	case VALUE_TYPE_INVALID:
	case VALUE_TYPE_INT:
	case VALUE_TYPE_FLOAT:
	case VALUE_TYPE_VECTOR:
		return "";

	case VALUE_TYPE_STRING:
		return m_pStr;
	}

	Assert( false );
	return 0;
}

void CValueInput::AssignTo( IMaterialVar* pTarget, int vecComponent ) const
{
	// Set a single component of the target if requested
	if ( vecComponent >= 0 )
	{
		pTarget->SetVecComponentValue( GetFloat(), vecComponent );
		return;
	}

	// Figure out the result type we want.  Usually we will use the
	// type of the target variable, but if the target doesn't have a type,
	// we'll use our own type.
	MaterialVarType_t resultType = pTarget->GetType();
	int vecSize = -1;
	if ( resultType == MATERIAL_VAR_TYPE_VECTOR )
	{
		vecSize = pTarget->VectorSize();
	}
	else if ( resultType == MATERIAL_VAR_TYPE_UNDEFINED )
	{
		resultType = GetType( vecSize );
	}

	// Handle simple case of copying directly
	if ( mType == VALUE_TYPE_VAR && m_VecInfo < 0 && m_pVar->GetType() == resultType )
	{
		pTarget->CopyFrom( m_pVar );
		return;
	}

	switch ( resultType )
	{
	case MATERIAL_VAR_TYPE_INT:
		pTarget->SetIntValue( GetInt() );
		return;

	case MATERIAL_VAR_TYPE_STRING:
		pTarget->SetStringValue( GetString() );
		return;

	case MATERIAL_VAR_TYPE_FLOAT:
		if ( vecComponent >= 0 )
			pTarget->SetVecComponentValue( GetFloat(), vecComponent );
		else
			pTarget->SetFloatValue( GetFloat() );
		return;

	case MATERIAL_VAR_TYPE_VECTOR:
		if ( vecSize < 0 )
		{
			// vector with no size?
			Assert( false );
		}
		else
		{
			// only handle 4-vectors at most right now
			if ( vecSize >= 4 )
				vecSize = 4;

			float vecComps[4];
			GetVecValue( vecComps, vecSize );
			pTarget->SetVecValue( vecComps, vecSize );
		}
		return;

	default:
		 // Don't know how to fill in this type of variable.
		Assert( false );
		return;
	}
}



//-----------------------------------------------------------------------------
//
// Result proxy; a result (with vector friendliness)
//
//-----------------------------------------------------------------------------

CResultProxy::CResultProxy() : m_pResult(0)
{
}

CResultProxy::~CResultProxy()
{
}


bool CResultProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	char const* pResult = pKeyValues->GetString( "resultVar" );
	if( !pResult )
		return false;

	// Look for array specification...
	char pTemp[256];
	if (strchr(pResult, '['))
	{		 
		// strip off the array...
		Q_strncpy( pTemp, pResult, 256 );
		char *pArray = strchr( pTemp, '[' );
		*pArray++ = 0;

		char* pIEnd;
		m_ResultVecComp = strtol( pArray, &pIEnd, 10 );

		// Use the version without the array...
		pResult = pTemp;
	}
	else
	{
		m_ResultVecComp = -1;
	}

	bool foundVar;
	m_pResult = pMaterial->FindVar( pResult, &foundVar, true );
	if( !foundVar )
		return false;

	if ( !Q_stricmp( pResult, "$alpha" ) )
	{
		pMaterial->SetMaterialVarFlag( MATERIAL_VAR_ALPHA_MODIFIED_BY_PROXY, true );
	}

	return true;
}


//-----------------------------------------------------------------------------
// A little code to allow us to set single components of vectors
//-----------------------------------------------------------------------------
void CResultProxy::SetFloatResult( float result )
{
	if (m_pResult->GetType() == MATERIAL_VAR_TYPE_VECTOR)
	{		
		if ( m_ResultVecComp >= 0 )
		{
			m_pResult->SetVecComponentValue( result, m_ResultVecComp );
		}
		else
		{
			float v[4];
			int vecSize = m_pResult->VectorSize();

			for (int i = 0; i < vecSize; ++i)
				v[i] = result;

			m_pResult->SetVecValue( v, vecSize );
		}		
	}
	else
	{
		m_pResult->SetFloatValue( result );
	}
}

void CResultProxy::SetVecResult( float x, float y, float z, float w )
{
	if (m_pResult->GetType() == MATERIAL_VAR_TYPE_VECTOR)
	{		
		float v[4] = { x, y, z, w };
		int vecSize = m_pResult->VectorSize();
		m_pResult->SetVecValue( v, vecSize );
	}
	else
	{
		m_pResult->SetFloatValue( x );
	}
}

C_BaseEntity *CResultProxy::BindArgToEntity( void *pArg )
{
	IClientRenderable *pRend = (IClientRenderable *)pArg;
	if ( pRend )
		return pRend->GetIClientUnknown()->GetBaseEntity();
	else
		return NULL;
}

IMaterial *CResultProxy::GetMaterial()
{
	return m_pResult->GetOwningMaterial();
}


//-----------------------------------------------------------------------------
//
// Base functional proxy; two sources (one is optional) and a result
//
//-----------------------------------------------------------------------------

CFunctionProxy::CFunctionProxy() : m_pSrc1(0), m_pSrc2(0)
{
}

CFunctionProxy::~CFunctionProxy()
{
}


bool CFunctionProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	char const* pSrcVar1 = pKeyValues->GetString( "srcVar1" );
	if( !pSrcVar1 )
		return false;

	bool foundVar;
	m_pSrc1 = pMaterial->FindVar( pSrcVar1, &foundVar, true );
	if( !foundVar )
		return false;

	// Source 2 is optional, some math ops may be single-input
	char const* pSrcVar2 = pKeyValues->GetString( "srcVar2" );
	if( pSrcVar2 && (*pSrcVar2) )
	{
		m_pSrc2 = pMaterial->FindVar( pSrcVar2, &foundVar, true );
		if( !foundVar )
			return false;
	}
	else
	{
		m_pSrc2 = 0;
	}

	return true;
}


void CFunctionProxy::ComputeResultType( MaterialVarType_t& resultType, int& vecSize )
{
	// Feh, this is ugly. Basically, don't change the result type
	// unless it's undefined.
	resultType = m_pResult->GetType();
	if (resultType == MATERIAL_VAR_TYPE_VECTOR)
	{
		if (m_ResultVecComp >= 0)
			resultType = MATERIAL_VAR_TYPE_FLOAT;
		vecSize = m_pResult->VectorSize();
	}
	else if (resultType == MATERIAL_VAR_TYPE_UNDEFINED)
	{
		resultType = m_pSrc1->GetType();
		if (resultType == MATERIAL_VAR_TYPE_VECTOR)
		{
			vecSize = m_pSrc1->VectorSize();
		}
		else if ((resultType == MATERIAL_VAR_TYPE_UNDEFINED) && m_pSrc2)
		{
			resultType = m_pSrc2->GetType();
			if (resultType == MATERIAL_VAR_TYPE_VECTOR)
			{
				vecSize = m_pSrc2->VectorSize();
			}
		}
	}
}

