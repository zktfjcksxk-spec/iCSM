//============ Copyright (c) Valve Corporation, All rights reserved. ============

#include "kv3lib/keyvalues3.h"
#include "kv3lib/kv3transfer.h" // to map DynData classses to KV3TRANSFER_CLASSNAME_MEMBER

#include "tier1/utlbuffer.h"
#include "kv3lib/utltokenizer.h"
#include "tier1/fmtstr.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


DEFINE_UUID( SCHEMA_ENCODING_TEXT_INITIAL,			0x2cc83121, 0xf14f, 0x4a36, 0xab, 0xb8, 0x62, 0xf4, 0xc2, 0x79, 0x96, 0x89 );		// {2CC83121-F14F-4A36-ABB8-62F4C2799689}
DEFINE_UUID( SCHEMA_ENCODING_TEXT_WITH_TYPES,		0x7e125a45, 0x3d83, 0x4043, 0xb2, 0x92, 0x9e, 0x24, 0xf8, 0xef, 0x27, 0xb4 );		// {7E125A45-3D83-4043-B292-9E24F8EF27B4}

DEFINE_UUID( SCHEMA_ENCODING_BINARY_INITIAL,		0xeb2564ea, 0xbdab, 0x49fe, 0xb8, 0xb3, 0x40, 0xde, 0x85, 0xbe, 0x6c, 0xa3 );		// {EB2564EA-BDAB-49FE-B8B3-40DE85BE6CA3}

DEFINE_UUID( SCHEMA_FORMAT_GENERIC_INITIAL,			0x198980d8, 0x3a93, 0x4919, 0xb4, 0xc6, 0xdd, 0x1f, 0xb0, 0x7a, 0x3a, 0x4b );		// {198980D8-3A93-4919-B4C6-DD1FB07A3A4B}


DEFINE_UUID( KV3_ENCODING_TEXT_INITIAL,				0xe21c7f3c, 0x8a33, 0x41c5, 0x99, 0x77, 0xa7, 0x6d, 0x3a, 0x32, 0xaa, 0xd );		// {E21C7F3C-8A33-41C5-9977-A76D3A32AA0D}
DEFINE_UUID( KV3_FORMAT_GENERIC_INITIAL,			0x7412167c, 0x6e9, 0x4698, 0xaf, 0xf2, 0xe6, 0x3e, 0xb5, 0x90, 0x37, 0xe7 );		// {7412167C-06E9-4698-AFF2-E63EB59037E7}

bool g_bKV3_HACK_LoadObjectReferencesAsStrings = false;
#define KV3_OLDSCHEMA_OBJECT_NAME_KEY "__oldschema_object_name"

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CLoadKV3FromDynData
{
public:
	CLoadKV3FromDynData( KeyValues3 *pSingleRoot, CUtlString *pOutErrorMessage, CUtlBuffer *pSrcBuffer, const char *pReferenceFilename );
	~CLoadKV3FromDynData();
	bool Parse();

private:
	bool ReadHeader();
	bool ReadObject( KeyValues3 *pTarget, CUtlString *pOutInstanceName );
	bool HandleMember( KeyValues3 *pObject, const char *pMemberName );
	bool ReadValue( KeyValues3 *target );

	bool HandleType();

	bool ReadCompoundValue( KeyValues3 *target );
	bool ReadObjectReference( KeyValues3 *target );
	bool ReadLiteralValue( KeyValues3 *target );

	void ReportError( const char *pError );
	void ReportErrorNoLine( const char *pError );

	bool m_bLoadedOk;
	CUtlTokenizer m_Tokenizer;
	bool m_bReadTypes;
	KeyValues3 *m_pPrimaryRoot;
	CUtlString *m_pOutErrorMessage;
	CUtlBuffer *m_pSrcBuffer;

	struct LoadedObject_t
	{
		LoadedObject_t() : m_ObjName(), m_pObject(nullptr), m_bReferenceResolved(false) {}

		CUtlString m_ObjName;
		KeyValues3 *m_pObject;
		bool m_bReferenceResolved;
	};
	CUtlVector<LoadedObject_t> m_AdditionalRootObjects;

	struct UnresolvedElementReference_t
	{
		CUtlString m_ObjName;
		KeyValues3 *m_pTargetKV;
	};
	CUtlVector<UnresolvedElementReference_t> m_UnresolvedElementReferences;

	bool ResolveUnresolvedReferences();

	KeyValues3 *AllocTempKV3();
	void FreeAllTempKV3();
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LoadKV3FromOldSchemaText( KeyValues3 *pRootTarget, CUtlString *pOutErrorMessage, CUtlBuffer *pSrcBuffer, const KV3ID_t &expectedFormat, const char *pReferenceFilename )
{
	CLoadKV3FromDynData loader( pRootTarget, pOutErrorMessage, pSrcBuffer, pReferenceFilename );
	if ( !loader.Parse() )
	{
		return false;
	}

	return ConvertKV3Format( pRootTarget, KV3_FORMAT_GENERIC, expectedFormat, pOutErrorMessage );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#define TOKEN_BEGIN_OBJECT_STR "{"
#define TOKEN_END_OBJECT_STR "}"
#define TOKEN_BEGIN_ARRAY_STR "["
#define TOKEN_END_ARRAY_STR "]"
#define TOKEN_BEGIN_TUPLE_STR "("
#define TOKEN_END_TUPLE_STR ")"
#define TOKEN_EQUAL_STR "="
#define TOKEN_REFERENCE_STR "&"
#define TOKEN_ARRAY_TUPLE_SEPARATOR_STR ","
#define TOKEN_WHITESPACE_STR " \t\n"
#define TOKEN_QUOTECHARS_STR "\'\""
#define TOKEN_NULL_STR "NULL"
#define TOKEN_BEGIN_HEADER_STR "<!--"
#define TOKEN_END_HEADER_STR "-->"
#define TOKEN_BEGIN_UUID_STR "{"
#define TOKEN_END_UUID_STR "}"
#define TOKEN_HEADER_ID "schema"
#define TOKEN_SEMI ";"

#define TOKEN_TYPE_INT "int"
#define TOKEN_TYPE_UINT "uint"
#define TOKEN_TYPE_INT64 "int64"
#define TOKEN_TYPE_UINT64 "uint64"
#define TOKEN_TYPE_FLOAT "float"
#define TOKEN_TYPE_FLOAT64 "float64"
#define TOKEN_TYPE_STRING "string"
#define TOKEN_TYPE_SYMBOL "symbol"
#define TOKEN_TYPE_BOOL "bool"
#define TOKEN_TYPE_PTR_SYMBOL "*"
#define TOKEN_TYPE_BITFIELD "bitfield"
#define TOKEN_TYPE_DYNAMIC "dynamic"

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#define ALL_TOKEN_DELIMITERS_STR \
	TOKEN_BEGIN_OBJECT_STR \
	TOKEN_END_OBJECT_STR \
	TOKEN_BEGIN_ARRAY_STR \
	TOKEN_END_ARRAY_STR \
	TOKEN_BEGIN_TUPLE_STR \
	TOKEN_END_TUPLE_STR \
	TOKEN_EQUAL_STR \
	TOKEN_REFERENCE_STR \
	TOKEN_ARRAY_TUPLE_SEPARATOR_STR \
	TOKEN_WHITESPACE_STR \
	TOKEN_QUOTECHARS_STR \
	TOKEN_TYPE_PTR_SYMBOL \
	TOKEN_SEMI


const char *KV3_TEMP_UNSERIALIZATION_MEMBER = "__temp_unserialization_member__";

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CLoadKV3FromDynData::CLoadKV3FromDynData( KeyValues3 *pSingleRoot, CUtlString *pOutErrorMessage, CUtlBuffer *pSrcBuffer, const char *pReferenceFilename )
	: m_Tokenizer( pSrcBuffer, pReferenceFilename )
{
	m_Tokenizer.SetTokenDelimiters( ALL_TOKEN_DELIMITERS_STR );
	m_bReadTypes = true;
	m_pPrimaryRoot = pSingleRoot;
	m_pOutErrorMessage = pOutErrorMessage;
	m_pSrcBuffer = pSrcBuffer;
	m_bLoadedOk = true;

	Assert( m_pPrimaryRoot->FindMember( CKV3MemberName( KV3_TEMP_UNSERIALIZATION_MEMBER ) ) == nullptr );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CLoadKV3FromDynData::~CLoadKV3FromDynData()
{
	Assert( m_pPrimaryRoot->FindMember( CKV3MemberName( KV3_TEMP_UNSERIALIZATION_MEMBER ) ) == nullptr );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CLoadKV3FromDynData::ReportError( const char *pError )
{
	CUtlTokenReference nextToken;
	if ( m_Tokenizer.PeekToken( 0, &nextToken ) )
	{
		ReportErrorNoLine( CFmtStr( "Line %d at \"%s\": %s", m_Tokenizer.GetCurrentLineNumber(), nextToken.AsString(), pError ).Access() );
	}
	else
	{
		ReportErrorNoLine( CFmtStr( "Line %d: %s", m_Tokenizer.GetCurrentLineNumber(), pError ).Access() );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CLoadKV3FromDynData::ReportErrorNoLine( const char *pError )
{
	m_bLoadedOk = false;
	Msg( "Unserialization Error: %s\n", pError );

	if ( m_pOutErrorMessage )
	{
		*m_pOutErrorMessage += pError;
		*m_pOutErrorMessage += "\n";
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromDynData::Parse()
{
	m_Tokenizer.Rewind();
	if ( !ReadHeader() )
	{
		ReportError( "Invalid header" );
		return false;
	}

	if ( !ReadObject( m_pPrimaryRoot, NULL ) )
	{
		return false;
	}

	if ( m_Tokenizer.AnyTokensRemaining() )
	{
		while ( m_Tokenizer.AnyTokensRemaining() )
		{
			LoadedObject_t &objRecord = m_AdditionalRootObjects[ m_AdditionalRootObjects.AddToTail() ];

			KeyValues3 *pObject = AllocTempKV3();
			if ( !ReadObject( pObject, &objRecord.m_ObjName ) )
			{
				return false;
			}

			objRecord.m_pObject = pObject;
			objRecord.m_bReferenceResolved = false;
		}
	}


	if ( !m_bLoadedOk )
	{
		return false;
	}

	return ResolveUnresolvedReferences();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromDynData::ReadHeader( )
{
	if ( !m_Tokenizer.ConsumeAtomicToken( TOKEN_BEGIN_HEADER_STR ) ||
		 !m_Tokenizer.ConsumeAtomicToken( TOKEN_HEADER_ID ) )
	{
		return false;
	}

	CUtlTokenReference encodingName;
	CUtlTokenReference encodingId;
	if ( !m_Tokenizer.ConsumeIdentifier( &encodingName ) ||
		!m_Tokenizer.ConsumeAtomicToken( TOKEN_BEGIN_UUID_STR ) ||
		!m_Tokenizer.ConsumeArbitraryToken( &encodingId ) ||
		!m_Tokenizer.ConsumeAtomicToken( TOKEN_END_UUID_STR )
		)
	{
		return false;
	}

	CUtlTokenReference formatName;
	CUtlTokenReference formatId;
	if ( !m_Tokenizer.ConsumeIdentifier( &formatName ) ||
		 !m_Tokenizer.ConsumeAtomicToken( TOKEN_BEGIN_UUID_STR ) ||
		 !m_Tokenizer.ConsumeArbitraryToken( &formatId ) ||
		 !m_Tokenizer.ConsumeAtomicToken( TOKEN_END_UUID_STR )
		 )
	{
		return false;
	}

	V_uuid_t encodingUuid;
	V_uuid_t formatUuid;
	if ( !Plat_UUIDFromString( &encodingUuid, encodingId.AsString() ) ||
		 !Plat_UUIDFromString( &formatUuid, formatId.AsString() ) )
	{
		return false;
	}

	if ( !m_Tokenizer.ConsumeAtomicToken( TOKEN_END_HEADER_STR ) )
	{
		return false;
	}

	// valid syntax, now check the format + encoding names + uuid
	if ( !V_stricmp( formatName.AsString(), "generic" ) )
	{
		if ( formatUuid != SCHEMA_FORMAT_GENERIC_INITIAL )
			return false;
	}

	if ( encodingUuid == SCHEMA_ENCODING_TEXT_INITIAL )
	{
		m_bReadTypes = false;
		return true;
	}
	else if ( encodingUuid == SCHEMA_ENCODING_TEXT_WITH_TYPES )
	{
		m_bReadTypes = true;
		return true;
	}
	else
	{
		return false;
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromDynData::ReadObject( KeyValues3 *pTarget, CUtlString *pOutInstanceName )
{
	// Object Declaration:
	// GRAMMAR: ( class_name ( instance_name )? )? '{' (member_name '=' member_value)* '}'

	// GRAMMAR: ( class_name ( instance_name )? )?
	CUtlTokenReference className;
	CUtlTokenReference instanceName;
	if ( m_Tokenizer.ConsumeIdentifier( &className ) )
	{
		// GRAMMAR: ( instance_name )?
		m_Tokenizer.ConsumeIdentifier( &instanceName ); // ok if it fails
	}

	// GRAMMAR: '{'
	if ( !m_Tokenizer.ConsumeAtomicToken( TOKEN_BEGIN_OBJECT_STR ) )
	{
		ReportError( "Expected '" TOKEN_BEGIN_OBJECT_STR "'" );
		return false;
	}

	//--------------------------------------------------------------------------------------------------
	// begin
	KeyValues3 *pObject = pTarget;
	Assert( pObject != NULL );
	pObject->SetToEmptyTable();

	if ( className.IsValid() )
	{
		pObject->SetMemberString( KV3TRANSFER_CLASSNAME_MEMBER, className.AsString() );
	}

	if ( pOutInstanceName )
	{
		*pOutInstanceName = instanceName.AsString();
	}

	if ( instanceName.IsValid() )
	{
		pObject->SetMemberString( KV3_OLDSCHEMA_OBJECT_NAME_KEY, instanceName.AsString() );
	}

	//--------------------------------------------------------------------------------------------------
	// members
	for ( ;; )
	{
		if ( m_Tokenizer.PeekAtomicToken( 0, TOKEN_END_OBJECT_STR ) )
		{
			// GRAMMAR: '}'
			m_Tokenizer.ConsumeAtomicToken( TOKEN_END_OBJECT_STR );
			break;
		}
		else
		{
			// GRAMMAR: member_type
			if ( m_bReadTypes )
			{
				if ( !HandleType() )
				{
					// error was reported by HandleType
					return false;
				}
			}

			// GRAMMAR: member_name '=' member_value
			CUtlTokenReference memberName;
			if ( !m_Tokenizer.ConsumeIdentifier( &memberName ) )
			{
				ReportError( CFmtStr("Expected <member name> after type").Access() );
				return false;
			}


			if ( !m_Tokenizer.ConsumeAtomicToken( TOKEN_EQUAL_STR ) )
			{
				ReportError( CFmtStr("Expected \"" TOKEN_EQUAL_STR "\"after member '%s'", memberName.AsString()).Access() );
				return false;
			}

			if ( !HandleMember( pObject, memberName.AsString() ) )
			{
				ReportError( CFmtStr("Invalid data for member '%s'", memberName.AsString()).Access() );
				return false;
			}
		}
	}

	//--------------------------------------------------------------------------------------------------
	// end
	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromDynData::HandleType()
{
	CUtlTokenReference typeRoot;
	if ( !m_Tokenizer.ConsumeIdentifier( &typeRoot ) )
	{
		ReportError( "Expected type of next member" );
		return NULL;
	}

	//--------------------------------------------------------------------------------------------------
	// modifiers
	for ( ;; )
	{
		if ( m_Tokenizer.PeekAtomicToken( 0, TOKEN_BEGIN_ARRAY_STR ) )
		{
			if ( !m_Tokenizer.ConsumeAtomicToken( TOKEN_BEGIN_ARRAY_STR ) || !m_Tokenizer.ConsumeAtomicToken( TOKEN_END_ARRAY_STR ) )
			{
				ReportError( "Expected " TOKEN_BEGIN_ARRAY_STR TOKEN_END_ARRAY_STR );
				break;
			}
		}
		else if ( m_Tokenizer.PeekAtomicToken( 0, TOKEN_BEGIN_TUPLE_STR ) )
		{
			CUtlTokenReference tupleLength;
			if ( !m_Tokenizer.ConsumeAtomicToken( TOKEN_BEGIN_TUPLE_STR ) ||
				 !m_Tokenizer.ConsumeArbitraryToken(&tupleLength) ||
				 !m_Tokenizer.ConsumeAtomicToken( TOKEN_END_TUPLE_STR ) ||
				 !tupleLength.IsInteger() )
			{
				ReportError( "Expected " TOKEN_BEGIN_TUPLE_STR " <count> " TOKEN_END_TUPLE_STR );
				break;
			}
		}
		else if ( m_Tokenizer.ConsumeAtomicToken( TOKEN_TYPE_PTR_SYMBOL ) )
		{
		}
		else
		{
			// no more modifiers - we're done!
			return true;
		}
	}

	return false;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromDynData::HandleMember( KeyValues3 *pObject, const char *pMemberName )
{
	// build a target for the member
	KeyValues3 *pMember = pObject->SetMemberToNull( CKV3MemberName( pMemberName ) );

	// read
	return ReadValue( pMember );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromDynData::ReadValue( KeyValues3 *target )
{
	if ( target->HasMetadata() )
	{
		target->Metadata_SetFileLineNumber( m_Tokenizer.GetFilename(), m_Tokenizer.GetCurrentLineNumber() );
	}

	// Member Value:
	// GRAMMAR: array_value | ( '&' referenced_object_name ) | object_declaration | member_literal

	if ( m_Tokenizer.PeekAtomicToken( 0, TOKEN_BEGIN_ARRAY_STR ) || m_Tokenizer.PeekAtomicToken( 0, TOKEN_BEGIN_TUPLE_STR ) )
	{
		// array or tuple
		return ReadCompoundValue( target );
	}
	else if ( m_Tokenizer.ConsumeAtomicToken( TOKEN_REFERENCE_STR ) || m_Tokenizer.PeekAtomicToken( 0, TOKEN_NULL_STR ) )
	{
		// object or null reference
		return ReadObjectReference( target );
	}
	else if ( m_Tokenizer.PeekIdentifier( 0 ) &&
		( m_Tokenizer.PeekAtomicToken( 1, TOKEN_BEGIN_OBJECT_STR ) ||
		m_Tokenizer.PeekAtomicToken( 2, TOKEN_BEGIN_OBJECT_STR ) ) )
	{
		// inline object
		return ReadObject( target, NULL );
	}
	else if ( m_Tokenizer.PeekAtomicToken( 0, TOKEN_BEGIN_OBJECT_STR ) )
	{
		// inline dynamic
		return ReadObject( target, NULL );
	}
	else
	{
		// literal value
		return ReadLiteralValue( target );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromDynData::ReadCompoundValue( KeyValues3 *compoundTarget )
{
	//--------------------------------------------------------------------------------------------------
	// figure out what our end token is
	bool bTuple = false;
	const char *pExpectedEndToken = NULL;
	if ( m_Tokenizer.ConsumeAtomicToken( TOKEN_BEGIN_ARRAY_STR ) )
	{
		pExpectedEndToken = TOKEN_END_ARRAY_STR;
	}
	else if ( m_Tokenizer.ConsumeAtomicToken( TOKEN_BEGIN_TUPLE_STR ) )
	{
		bTuple = true;
		pExpectedEndToken = TOKEN_END_TUPLE_STR;
	}
	else
	{
		ReportError( "Expected '" TOKEN_BEGIN_ARRAY_STR "' or '" TOKEN_BEGIN_TUPLE_STR "'" );
		return false;
	}

	//--------------------------------------------------------------------------------------------------
	// loop over the array items
	compoundTarget->SetArrayElementCount( 0 );

	int nArrayIndex = 0;
	for ( ;; )
	{
		if ( m_Tokenizer.ConsumeAtomicToken( pExpectedEndToken ) )
		{
			// finished!
			break;
		}
		else
		{
			compoundTarget->SetArrayElementCount( nArrayIndex + 1 );
			KeyValues3 *pElement = compoundTarget->GetArrayElement( nArrayIndex );

			// handle the value
			if( !ReadValue( pElement ) )
			{
				ReportError( "Expected value or '" TOKEN_END_ARRAY_STR "'" );
				return false;
			}

			// separator or end
			if ( !m_Tokenizer.ConsumeAtomicToken( TOKEN_ARRAY_TUPLE_SEPARATOR_STR ) &&
				 !m_Tokenizer.PeekAtomicToken( 0, pExpectedEndToken ) )
			{
				ReportError( CFmtStr( "Expected '" TOKEN_ARRAY_TUPLE_SEPARATOR_STR "' or '%s'", pExpectedEndToken ) );
				return false;
			}

			nArrayIndex++;
		}
	}

	//--------------------------------------------------------------------------------------------------
	// all done
	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromDynData::ReadObjectReference( KeyValues3 *target )
{
	CUtlTokenReference instanceName;
	if ( m_Tokenizer.ConsumeAtomicToken( TOKEN_NULL_STR ) )
	{
		target->SetToNull();
		return true;
	}
	else if ( m_Tokenizer.ConsumeIdentifier( &instanceName ) )
	{
		target->SetToNull();

		UnresolvedElementReference_t &unresolvedReference = m_UnresolvedElementReferences[ m_UnresolvedElementReferences.AddToTail() ];
		unresolvedReference.m_ObjName = instanceName.AsString();
		unresolvedReference.m_pTargetKV = target;
		return true;
	}
	else
	{
		ReportError( "Expected '" TOKEN_REFERENCE_STR "' and instance name or '" TOKEN_NULL_STR "'" );
		return false;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromDynData::ReadLiteralValue( KeyValues3 *target )
{
	CUtlTokenReference memberValue;
	if ( !m_Tokenizer.ConsumeArbitraryToken( &memberValue ) )
	{
		ReportError( "Expected token" );
		return false;
	}

	if ( target == NULL ) // skipping
	{
		return true;
	}

	//--------------------------------------------------------------------------------------------------

	if ( memberValue.IsBool() )
	{
		if ( memberValue.IsEqual( "true" ) )
		{
			target->SetValueBool( true );
		}
		else if ( memberValue.IsEqual( "false" ) )
		{
			target->SetValueBool( false );
		}
		else
		{
			ReportError( "Failed to assign bool literal value" );
		}
	}
	else if ( memberValue.IsIdentifier() )
	{
		target->SetValueString( memberValue.AsString() );
	}
	else if ( memberValue.IsInteger() ) 
	{
		target->SetValueInt( V_atoi( memberValue.AsString() ) );
	}
	else if ( memberValue.IsFloat() )
	{
		target->SetValueDouble( V_atod( memberValue.AsString() ) );
	}
	else if ( memberValue.IsStringLiteral() )
	{
		CUtlString strUnescaped;
		memberValue.AsUnescapedString( &strUnescaped );
		target->SetValueString( strUnescaped.Get() );

		if ( memberValue.IsMultiLineStringLiteral() )
		{
//			target->SetValueTag( KEYVALUES3_TAG_BUILTIN_MULTILINE_STRING, true );
		}
	}
	else
	{
		ReportErrorNoLine( CFmtStr( "Line %d: Invalid literal value \"%s\"", memberValue.GetLineNumber(), memberValue.AsString() ).Access() );
		return false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromDynData::ResolveUnresolvedReferences()
{
	int nNumUnresolved = m_UnresolvedElementReferences.Count();
	for ( int i = 0; i < nNumUnresolved; ++i )
	{
		KeyValues3 *pObject = NULL;

		UnresolvedElementReference_t &ref = m_UnresolvedElementReferences[ i ];

		for ( int j = 0; j < m_AdditionalRootObjects.Count(); ++j )
		{
			LoadedObject_t &loadedObject = m_AdditionalRootObjects[j];
			if ( loadedObject.m_ObjName.IsEqual_CaseSensitive( ref.m_ObjName.Get() ) )
			{
				if ( loadedObject.m_bReferenceResolved )
				{
					ReportError( CFmtStr( "Cycle or diamond double-reference to '%s'", ref.m_ObjName.Get() ) );
					return false;
				}
				loadedObject.m_bReferenceResolved = true;
				pObject = loadedObject.m_pObject;
				break;
			}
		}

		if ( pObject == NULL )
		{
			if ( g_bKV3_HACK_LoadObjectReferencesAsStrings )
			{
				ref.m_pTargetKV->SetValueString( ref.m_ObjName.Get() );
			}
			else
			{
				ReportError( CFmtStr( "Unresolved object reference '%s'", ref.m_ObjName.Get() ) );
				return false;
			}
		}
		else
		{
			// This needs to be a move because pObject may contain additional references inside it
			// that need to be fixed up later in this process - if we made a copy, then we'd fix up
			// the wrong copy and the loaded data would end up with a bunch of un-fixed-up references.
			ref.m_pTargetKV->MoveFrom( pObject );
		}
	}

	for ( LoadedObject_t &loadedObject: m_AdditionalRootObjects )
	{
		Assert( loadedObject.m_bReferenceResolved ); // leaked root object
		Assert( loadedObject.m_pObject->IsNull() );
	}

	FreeAllTempKV3();

	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *CLoadKV3FromDynData::AllocTempKV3()
{
	KeyValues3 *pTempMember = m_pPrimaryRoot->FindOrCreateMember( CKV3MemberName( KV3_TEMP_UNSERIALIZATION_MEMBER ) );
	int nNumExistingTemp = pTempMember->GetArrayElementCount();
	pTempMember->SetArrayElementCount( nNumExistingTemp + 1 );
	return pTempMember->GetArrayElement( nNumExistingTemp );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CLoadKV3FromDynData::FreeAllTempKV3()
{
	KeyValues3 *pTempMember = m_pPrimaryRoot->FindOrCreateMember( CKV3MemberName( KV3_TEMP_UNSERIALIZATION_MEMBER ) );
	if ( pTempMember )
	{
		m_pPrimaryRoot->RemoveMember( pTempMember );
	}
}

