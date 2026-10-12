//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: These are a couple of base proxy classes to help us with
// getting/setting source/result material vars
//
// $NoKeywords: $
//=============================================================================//

#ifndef FUNCTIONPROXY_H
#define FUNCTIONPROXY_H

#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterialvar.h"

class IMaterialVar;
class C_BaseEntity;


//-----------------------------------------------------------------------------
// Helper class to deal with floating point inputs
//-----------------------------------------------------------------------------
class CFloatInput
{
public:
	bool  Init( IMaterial *pMaterial, KeyValues *pKeyValues, const char *pKeyName, float flDefault = 0.0f );
	float GetFloat() const;

private:
	float m_flValue;
	IMaterialVar *m_pFloatVar;
	int	m_FloatVecComp;
};

//-----------------------------------------------------------------------------
// Helper class to deal with arbitrary inputs
//-----------------------------------------------------------------------------
class CValueInput
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues, const char *pKeyName, float flDefault = 0.0f );
	bool Init( IMaterial* pMaterial, KeyValues* pKeyValues, const char* pKeyName, int nDefault );
	bool Init( IMaterial* pMaterial, KeyValues* pKeyValues, const char* pKeyName, const char* pStringDefault );
	bool Init( IMaterial* pMaterial, KeyValues* pKeyValues, const char* pKeyName, const float* pVecDefault, int numEntries );

	CValueInput();
	~CValueInput();

	MaterialVarType_t GetType( int& vecSize ) const; // returns type and vector size

	int GetInt() const;
	float GetFloat() const;
	void GetVecValue( float* target, int nComps ) const;
	const char* GetString() const;

	void AssignTo( IMaterialVar* pVar, int vecComponent = -1 ) const;

private:
	enum ValueType {
		VALUE_TYPE_INVALID, // unininitialized

		VALUE_TYPE_VAR,		// points to a material var which could change value
		VALUE_TYPE_INT,		// constant int
		VALUE_TYPE_FLOAT,	// constant float
		VALUE_TYPE_VECTOR,	// constant vector of floats of given size
		VALUE_TYPE_STRING,	// constant string
	};

	ValueType mType;

	union {
		IMaterialVar* m_pVar;	// VALUE_TYPE_VAR
		int m_nVal;				// VALUE_TYPE_INT
		float m_flVal;			// VALUE_TYPE_FLOAT
		float* m_pflVec;		// VALUE_TYPE_VECTOR
		const char* m_pStr;		// VALUE_TYPE_STRING
	};

	// If VALUE_TYPE_VAR, this will be -1, or the index within the
	//                    referred-to vector if specified (e.g. "$color[3]" => 3)
	// If VALUE_TYPE_VECTOR, this will be the length of m_pflVec
	int m_VecInfo;

	CValueInput( const CValueInput& ) = delete;
	CValueInput& operator= ( const CValueInput& ) = delete;

	bool InitFromKV( IMaterial *pMaterial, KeyValues *pKeyValues, const char *pKeyName );
};


//-----------------------------------------------------------------------------
// Result proxy; a result (with vector friendliness)
//-----------------------------------------------------------------------------
class CResultProxy : public IMaterialProxy
{
public:
	CResultProxy();
	virtual ~CResultProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();

protected:
	C_BaseEntity *BindArgToEntity( void *pArg );
	void SetFloatResult( float result );
	void SetVecResult( float x, float y, float z, float w );

	IMaterialVar* m_pResult;
	int m_ResultVecComp;
};


//-----------------------------------------------------------------------------
// Base functional proxy; two sources (one is optional) and a result
//-----------------------------------------------------------------------------
class CFunctionProxy : public CResultProxy
{
public:
	CFunctionProxy();
	virtual ~CFunctionProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );

protected:
	void ComputeResultType( MaterialVarType_t& resultType, int& vecSize );

	IMaterialVar* m_pSrc1;
	IMaterialVar* m_pSrc2;
};

#endif // FUNCTIONPROXY_H

