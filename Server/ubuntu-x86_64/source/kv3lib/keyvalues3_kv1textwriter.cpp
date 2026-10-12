//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#include "keyvalues3_text.h"

#include "kv3lib/keyvalues3.h"

#include "tier1/utlbuffer.h"
#include "tier1/utlstring.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool SaveKV3AsKV1Text_R( bool bIsRoot, const KeyValues3 *pData, CUtlString *pOutErrorMessage, CUtlBuffer *pDestBuffer, KV1TextEscapeBehavior_t nEscapeBehavior )
{
	switch ( pData->GetType() )
	{
		case KEYVALUES3_TYPE_NULL:
		{
			pDestBuffer->Printf( "\"\"" );
			break;
		}
		case KEYVALUES3_TYPE_BOOL:
		{
			pDestBuffer->Printf( "\"%s\"", pData->GetValueBool() ? "1" : "0" );
			break;
		}
		case KEYVALUES3_TYPE_INT64:
		{
			pDestBuffer->Printf( "\"%lld\"", pData->GetValueInt64() );
			break;
		}
		case KEYVALUES3_TYPE_UINT64:
		{
			pDestBuffer->Printf( "\"%llu\"", pData->GetValueUint64() );
			break;
		}
		case KEYVALUES3_TYPE_DOUBLE:
		{
			pDestBuffer->Printf( "\"%f\"", pData->GetValueDouble() );
			break;
		}
		case KEYVALUES3_TYPE_STRING:
		{
			if ( nEscapeBehavior == KV1_NO_ESCAPE_SEQUENCES )
			{
				pDestBuffer->Printf( "\"%s\"", pData->GetValueString() );
			}
			else
			{
				pDestBuffer->PutDelimitedString( GetCStringCharConversion(), pData->GetValueString() );
			}
			break;
		}
		case KEYVALUES3_TYPE_BINARY_BLOB:
		{
			AssertMsg( false, "Cannot save KV3 binary blob to KV1." );
			pDestBuffer->Printf( "\"[BINARY BLOB]\"" );
			break;
		}
		case KEYVALUES3_TYPE_ARRAY:
		{
			AssertMsg( false, "Cannot save KV3 array to KV1." );
			pDestBuffer->Printf( "\"[ARRAY]\"" );
			break;
		}
		case KEYVALUES3_TYPE_TABLE:
		{
			pDestBuffer->Printf( "{\n" );
			pDestBuffer->PushTab();
			int nCount = pData->GetMemberCount();
			for ( int i = 0; i < nCount; ++i )
			{
				if ( bIsRoot && !V_strcmp( pData->GetMemberName( i ), ROOT_KEYNAME_MEMBER_NAME ) )
				{
					continue;
				}
				pDestBuffer->Printf( "\"%s\"", pData->GetMemberName( i ) );

				const KeyValues3 *pMember = pData->GetMember( i );
				if ( pMember->GetType() == KEYVALUES3_TYPE_ARRAY || pMember->GetType() == KEYVALUES3_TYPE_TABLE )
				{
					pDestBuffer->Printf( "\n" );
				}
				else
				{
					pDestBuffer->Printf( " " );
				}

				if ( !SaveKV3AsKV1Text_R( false, pMember, pOutErrorMessage, pDestBuffer, nEscapeBehavior ) )
				{
					return false;
				}

				pDestBuffer->PutString( "\n" );
			}
			pDestBuffer->PopTab();
			pDestBuffer->PutString( "}" );
			break;
		}
		default:
		{
			Assert( 0 );
			return false;
		}
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool SaveKV3AsKV1Text( const KeyValues3 *pRoot, CUtlString *pOutErrorMessage, CUtlBuffer *pDestBuffer, KV1TextEscapeBehavior_t nEscapeBehavior )
{
	CUtlString rootKeyname;
	pRoot->GetMemberAsString( ROOT_KEYNAME_MEMBER_NAME, &rootKeyname );

	pDestBuffer->Printf( "\"%s\"\n", rootKeyname.Get() );
	return SaveKV3AsKV1Text_R( true, pRoot, pOutErrorMessage, pDestBuffer, nEscapeBehavior );
}
