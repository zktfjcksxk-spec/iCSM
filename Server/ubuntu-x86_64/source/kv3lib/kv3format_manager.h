//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#pragma once


#include "kv3lib/keyvalues3.h"
#include "tier1/utlstring.h"


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
struct KV3FormatConversionContext_t
{
	KV3FormatConversionContext_t() : m_OutError(), m_pRoot(nullptr) {}

	CUtlString m_OutError;
	KeyValues3 *m_pRoot;
};

enum KV3FormatConversionResult_t
{
	KV3_FORMAT_CONVERSION_FAILED,
	KV3_FORMAT_CONVERSION_SUCCESS,
};

typedef KV3FormatConversionResult_t (*ConversionFn_t)( KV3FormatConversionContext_t &conversionCtx );


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CKV3FormatManager
{
public:
	CKV3FormatManager();
	~CKV3FormatManager();
	void RegisterFormatConversion( const KV3ID_t &fromFormat, const KV3ID_t &toFormat, ConversionFn_t pConversionFn );
	bool Convert( KeyValues3 *pRoot, const KV3ID_t &fromFormat, const KV3ID_t &toFormat, CUtlString *pOutError ) const;

private:
	struct Conversion_t
	{
		Conversion_t()
			: m_FromFormat()
			, m_ToFormat()
			, m_pConversionFn( nullptr )
			, m_bCurrentlyVisitingDuringSearch( false )
		{
		}

		Conversion_t( const KV3ID_t &fromFormat, const KV3ID_t &toFormat, ConversionFn_t pConversionFn )
			: m_FromFormat( fromFormat )
			, m_ToFormat( toFormat )
			, m_pConversionFn( pConversionFn )
			, m_bCurrentlyVisitingDuringSearch(false)
		{
		}

		ConversionFn_t m_pConversionFn;
		KV3ID_t m_FromFormat;
		KV3ID_t m_ToFormat;
		bool m_bCurrentlyVisitingDuringSearch; // used during search
	};

	typedef CUtlVectorFixedGrowable< Conversion_t*, 8 > ConversionPath_t;

	bool FindConversionPath( ConversionPath_t *pOutPath, const KV3ID_t &fromFormat, const KV3ID_t &toFormat ) const;
	bool FindConversionPath_R( ConversionPath_t *pOutPath, const KV3ID_t &fromFormat, const KV3ID_t &toFormat ) const;

	CUtlVector< Conversion_t* > m_AllConversions;

	mutable bool m_bAllowNewRegistrations;
};

extern CKV3FormatManager g_KV3FormatConverter;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class CKV3FormatConversionRegistration
{
public:
	CKV3FormatConversionRegistration( const KV3ID_t &fromFormat, const KV3ID_t &toFormat, ConversionFn_t pConversionFn )
	{
		g_KV3FormatConverter.RegisterFormatConversion( fromFormat, toFormat, pConversionFn );
	}
};

#if 0 // $$$REI Temporarily remove due to undefined constructor order-of-initialization problem.
#define DECLARE_KV3_FORMAT_CONVERSION( fromFormat, toFormat )																										\
	KV3FormatConversionResult_t KV3Convert_##fromFormat##_To_##toFormat( KV3FormatConversionContext_t &conversionCtx );												\
	static CKV3FormatConversionRegistration KV3Convert_##fromFormat##_To_##toFormat##_Reg( fromFormat, toFormat, &KV3Convert_##fromFormat##_To_##toFormat );		\
	KV3FormatConversionResult_t KV3Convert_##fromFormat##_To_##toFormat( KV3FormatConversionContext_t &conversionCtx )
#endif