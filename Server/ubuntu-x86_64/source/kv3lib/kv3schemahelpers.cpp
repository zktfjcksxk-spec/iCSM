//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#if 0 // $$$REI Disable in CSGO

#include "kv3lib/kv3schemahelpers.h"
#include "kv3lib/kv3transfer.h"
#include "schemalib/schemaiterator.h"
#include "tier1/splitstring.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void NormalizeKV3TreeToSchemaClasses( KeyValues3 *pRoot )
{
	CUtlVectorFixedGrowable<KeyValues3*,64> tablesToFixup;
	for ( CKeyValues3Iterator it( pRoot ); it.IsValid(); it.Advance() )
	{
		if ( it.Get()->GetType() == KEYVALUES3_TYPE_TABLE )
		{
			tablesToFixup.AddToTail( it.Get() );
		}
	}

	FOR_EACH_VEC( tablesToFixup, i )
	{
		KeyValues3 *pKV3 = tablesToFixup[i];
		const char *pTableClass = KV3TransferClassname( pKV3 );
		if ( pTableClass )
		{
			RemoveKeysNotInClass( pKV3, pTableClass );
			ImprintSchemaClassOnKV3( pKV3, pTableClass );
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
template< class TSimpleOp, class TFixedOp, class TClassOp, class TVarArrayOp, class TFailureOp >
void OperateSchemaTypeOnKV3( KeyValues3 *pData, FieldIntrospectionHandle_t pField, const CSchemaType *pType,
	const TSimpleOp &simpleValueOp, const TFixedOp &fixedArrayOp, const TClassOp &classOp, const TVarArrayOp &varArrayOp, const TFailureOp &failureOp )
{
	//--------------------------------------------------------------------------------------------------
	ISchemaCollectionManipulator *pCollectionManip = pType->GetManipulatorInterface<ISchemaCollectionManipulator>();
	if ( pCollectionManip )
	{
		varArrayOp( pData, pCollectionManip->CollectionOfType() );
		return;
	}

	//--------------------------------------------------------------------------------------------------
	switch ( pType->GetTypeCategory() )
	{
		case SCHEMA_TYPE_DECLARED_CLASS:
		{
			classOp( pData, pType->As<CSchemaType_DeclaredClass>()->GetClass() );
			return;
		}

		case SCHEMA_TYPE_DECLARED_ENUM:
		{
			simpleValueOp( pData, KEYVALUES3_TYPE_STRING );
			return;
		}

		case SCHEMA_TYPE_BUILTIN:
		{
			SchemaBuiltinType_t nBuiltin = ((CSchemaType_Builtin*)pType)->GetBuiltinType();
			switch( nBuiltin )
			{
				case SCHEMA_BUILTIN_TYPE_CHAR:
				case SCHEMA_BUILTIN_TYPE_INT8:
				case SCHEMA_BUILTIN_TYPE_INT16:
				case SCHEMA_BUILTIN_TYPE_INT32:
				case SCHEMA_BUILTIN_TYPE_INT64:
				case SCHEMA_BUILTIN_TYPE_UINT8:
				case SCHEMA_BUILTIN_TYPE_UINT16:
				case SCHEMA_BUILTIN_TYPE_UINT32:
					simpleValueOp( pData, KEYVALUES3_TYPE_INT64 );
					return;

				case SCHEMA_BUILTIN_TYPE_UINT64:
					simpleValueOp( pData, KEYVALUES3_TYPE_UINT64 );
					return;

				case SCHEMA_BUILTIN_TYPE_FLOAT32:
				case SCHEMA_BUILTIN_TYPE_FLOAT64:
					simpleValueOp( pData, KEYVALUES3_TYPE_DOUBLE );
					return;

				case SCHEMA_BUILTIN_TYPE_BOOL:
					simpleValueOp( pData, KEYVALUES3_TYPE_BOOL );
					return;

				default:
				{
					AssertMsg( false, "Invalid SCHEMA_TYPE_BUILTIN category (%d)\n", nBuiltin );
					failureOp( pData );
					return;
				}
			}
		}

		case SCHEMA_TYPE_FIXED_ARRAY:
		{
			CSchemaType_FixedArray *pFixedArray = (CSchemaType_FixedArray*)pType;

			// special case - char[] is a string
			if ( pFixedArray->GetArrayOfType() == SchemaTypeOf<char>() )
			{
				simpleValueOp( pData, KEYVALUES3_TYPE_STRING );
				return;
			}
			else
			{
				// array
				fixedArrayOp( pData, pFixedArray->GetArrayCount(), pFixedArray->GetArrayOfType() );
				return;
			}
		}

		case SCHEMA_TYPE_POINTER:
		{
			CSchemaType *pPointedTo = ((CSchemaType_Ptr*)pType)->GetPointedToType();
			if ( pPointedTo->GetTypeCategory() != SCHEMA_TYPE_DECLARED_CLASS )
			{
				AssertMsg( false, "Non-class in ImprintSchemaTypeOnKV3 (%s)\n", pType->GetName() );
				failureOp( pData );
				return;
			}

			classOp( pData, pPointedTo->As<CSchemaType_DeclaredClass>()->GetClass() );
			return;
		}

		case SCHEMA_TYPE_ATOMIC:
		{
			CSchemaType_Atomic *pAtomic = (CSchemaType_Atomic*)pType;

			if ( pAtomic->GetAtomicCategory() == SCHEMA_ATOMIC_T )
			{
				CSchemaType_Atomic_T *pAtomic_T = ((CSchemaType_Atomic_T*)pType);
				switch ( pAtomic->GetAtomicId() )
				{
					case 4004: // SCHEMA_ATOMIC_TYPE_ID_FOR( CResourceArray ):
						varArrayOp( pData, pAtomic_T->GetParam1() );
						return;
					case 4003: // SCHEMA_ATOMIC_TYPE_ID_FOR( CResourcePointer ):
					{
						if ( pAtomic_T->GetParam1()->GetTypeCategory() != SCHEMA_TYPE_DECLARED_CLASS )
						{
							AssertMsg( false, "Pointer to non-class in ImprintSchemaTypeOnKV3 (%s)\n", pType->GetName() );
							failureOp( pData );
							return;
						}

						classOp( pData, pAtomic_T->GetParam1()->As<CSchemaType_DeclaredClass>()->GetClass() );
						return;
					}
					default:
						simpleValueOp( pData, KEYVALUES3_TYPE_STRING ); // unknown type - default to string
						return;
				}
			}

			switch ( pAtomic->GetAtomicId() )
			{
				case SCHEMA_ATOMIC_TYPE_ID_FOR( fltx4 ):
					simpleValueOp( pData, KEYVALUES3_TYPE_DOUBLE ); // fltx4 = single smeared float
					return;
				case SCHEMA_ATOMIC_TYPE_ID_FOR( Vector2D ):
					fixedArrayOp( pData, 2, SchemaTypeOf<double>() );
					return;
				case SCHEMA_ATOMIC_TYPE_ID_FOR( Vector ):
					fixedArrayOp( pData, 3, SchemaTypeOf<double>() );
					return;
				case SCHEMA_ATOMIC_TYPE_ID_FOR( Vector4D ):
					fixedArrayOp( pData, 4, SchemaTypeOf<double>() );
					return;
				case SCHEMA_ATOMIC_TYPE_ID_FOR( QAngle ):
					fixedArrayOp( pData, 3, SchemaTypeOf<double>() );
					return;
				case SCHEMA_ATOMIC_TYPE_ID_FOR( Color ):
				{
					if ( pField && MPropertyColorWithNoAlpha::IsPresent( pField ) )
					{
						fixedArrayOp( pData, 3, SchemaTypeOf<int>() );
						return;
					}
					else
					{
						fixedArrayOp( pData, 4, SchemaTypeOf<int>() );
						return;
					}
				}
				default:
					simpleValueOp( pData, KEYVALUES3_TYPE_STRING ); // unknown type - default to string
					return;
			}
		}

		default:
		{
			AssertMsg( false, "Unrecognized KV3/Schema property category '%s'", pType->GetName() );
			failureOp( pData );
			return;
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void EnumerateTablesAssociatedWithClasses_R( CUtlVectorFixedGrowable<KeyValues3*,64> *pOutTableList, CUtlVectorFixedGrowable<ClassIntrospectionHandle_t,64> *pOutTableClassList, KeyValues3 *pRoot, CSchemaType *pRootType )
{
	auto simpleValueOp = [&]( KeyValues3 *pData, KeyValues3Type_t nType )
	{
	};

	auto fixedArrayOp = [&]( KeyValues3 *pData, int nArrayCount, CSchemaType *pArrayOf )
	{
		for ( int i = 0; i < pData->GetArrayElementCount(); ++i )
		{
			EnumerateTablesAssociatedWithClasses_R( pOutTableList, pOutTableClassList, pData->GetArrayElement( i ), pArrayOf );
		}
	};

	auto varArrayOp = [&]( KeyValues3 *pData, CSchemaType *pArrayOf )
	{
		for ( int i = 0; i < pData->GetArrayElementCount(); ++i )
		{
			EnumerateTablesAssociatedWithClasses_R( pOutTableList, pOutTableClassList, pData->GetArrayElement( i ), pArrayOf );
		}
	};

	auto classOp = [&]( KeyValues3 *pData, ClassIntrospectionHandle_t pBaseClass )
	{
		// if virtual, we need to determine the actual class
		ClassIntrospectionHandle_t pActualClass = pBaseClass;

		if ( pBaseClass->HasFlag( SCHEMA_CLASS_HAS_VIRTUAL_MEMBERS ) )
		{
			const char *pClassName = pData->GetMemberString( KV3TRANSFER_CLASSNAME_MEMBER, NULL );
			pActualClass = SchemaTypeScope()->FindDeclaredClass( pClassName );
		}

		// add to the list
		pOutTableList->AddToTail( pData );
		pOutTableClassList->AddToTail( pActualClass );

		// recurse
		for ( CSchemaFieldIterator it( pActualClass, SCHEMA_BASE_TRAVERSAL_FULL ); it.IsValid(); it.Advance() )
		{
			KeyValues3 *pMember = pData->FindMember( CKV3MemberName( it.GetFieldName() ) );
			if ( pMember != NULL )
			{
				EnumerateTablesAssociatedWithClasses_R( pOutTableList, pOutTableClassList, pMember, it.GetType() );
			}
		}
	};

	auto failureOp = [&]( KeyValues3 *pData )
	{
	};

	// NULL for pField because it's only used to distinguish between Color with/without alpha, not topology
	OperateSchemaTypeOnKV3( pRoot, NULL, pRootType, simpleValueOp, fixedArrayOp, classOp, varArrayOp, failureOp );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void RemoveKeysInKV3TreeEqualToDefault( KeyValues3 *pRoot )
{
	CUtlVectorFixedGrowable<KeyValues3*,64> tablesToFixup;
	CUtlVectorFixedGrowable<ClassIntrospectionHandle_t,64> tableClassList;

	const char *pRootClass = KV3TransferClassname( pRoot );
	ClassIntrospectionHandle_t pClass = SchemaTypeScope()->FindDeclaredClass( pRootClass );
	if ( pClass )
	{
		EnumerateTablesAssociatedWithClasses_R( &tablesToFixup, &tableClassList, pRoot, pClass->GetTypeOf() );
	}

	// This is a little tricky in the case of nested tables that are equal to default: { foo = default /* ... */ sub_table = { bar = default } }
	// If you try to remove sub_table member from the outer table FIRST, then you could have a stale pointer to sub_table later in the tablesToFixup list!
	// However we built our list using a depth first traversal, so if we traverse it backwards, we're guaranteed to encounter sub_table before the root,
	// avoiding the problem!
	Assert( tablesToFixup.Count() == tableClassList.Count() );
	FOR_EACH_VEC_BACK( tablesToFixup, i )
	{
		KeyValues3 *pKV3 = tablesToFixup[i];
		ClassIntrospectionHandle_t pTableClass = tableClassList[i];
		RemoveKeysInTableEqualToDefault_Shallow( pKV3, pTableClass );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void ImprintSchemaTypeOnKV3( KeyValues3 *pData, FieldIntrospectionHandle_t pField, const CSchemaType *pType, const char *pValue )
{
	auto simpleValueOp = [pValue]( KeyValues3 *pOpData, KeyValues3Type_t nType )
	{
		pOpData->EnsureTypeIs( nType );
		if ( pValue )
		{
			pOpData->ParseValueFromString( pValue );
		}
	};

	auto fixedArrayOp = [pValue,pField]( KeyValues3 *pOpData, int nArrayCount, CSchemaType *pArrayOf )
	{
		pOpData->EnsureIsAnyArray( nArrayCount );

		// TODO: support commas?
		CSplitString split( pValue ? pValue : "", " " ); // this is fairly slow
		int nSplitLen = split.Count();
		for ( int i = 0; i < nSplitLen && i < pOpData->GetArrayElementCount(); ++i )
		{
			ImprintSchemaTypeOnKV3( pOpData->GetArrayElement( i ), pField, pArrayOf, split[i] );
		}
	};

	auto varArrayOp = [pValue,pField]( KeyValues3 *pOpData, CSchemaType *pArrayOf )
	{
		pOpData->EnsureTypeIs( KEYVALUES3_TYPE_ARRAY );
		for ( int i = 0; i < pOpData->GetArrayElementCount(); ++i )
		{
			ImprintSchemaTypeOnKV3( pOpData->GetArrayElement( i ), pField, pArrayOf, pValue );
		}
	};

	auto classOp = [pValue]( KeyValues3 *pOpData, ClassIntrospectionHandle_t pClass )
	{
		pOpData->EnsureTypeIs( KEYVALUES3_TYPE_TABLE );
		ImprintSchemaClassOnKV3( pOpData,pClass );
	};

	auto failureOp = [pValue]( KeyValues3 *pOpData )
	{
		pOpData->SetToNull();
	};

	OperateSchemaTypeOnKV3( pData, pField, pType, simpleValueOp, fixedArrayOp, classOp, varArrayOp, failureOp );

	//--------------------------------------------------------------------------------------------------
	// special case
	const CSchemaType_Atomic_T *pAtomic_T = pType->As<CSchemaType_Atomic_T>();
	if ( pAtomic_T && pAtomic_T->GetAtomicId() == 4001 ) // SCHEMA_ATOMIC_TYPE_ID_FOR(CResourceExtReference)
	{
		pData->SetFlag( KEYVALUES3_FLAG_RESOURCE_REFERENCE, true );
	}
	else if ( pAtomic_T && pAtomic_T->GetAtomicId() == 4000 ) // SCHEMA_ATOMIC_TYPE_ID_FOR(CStrongHandle) )
	{
		pData->SetFlag( KEYVALUES3_FLAG_RESOURCE_REFERENCE, true );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
// doesn't modify existing values
void ImprintSchemaClassOnKV3( KeyValues3 *pTarget, ClassIntrospectionHandle_t pClass )
{
	if ( pClass->HasFlag( SCHEMA_CLASS_HAS_VIRTUAL_MEMBERS ) )
	{
		// virtual class, so we need to make sure our _class value is present, in case it's a subclass disambiguator
		// (if _class is already set, leave it alone and trust that someone knew what they were talking about)
		const char *pPrevClassName = pTarget->GetMemberString( KV3TRANSFER_CLASSNAME_MEMBER, NULL );
		if ( pPrevClassName == NULL )
		{
			// didn't have a _class key - stomp with our best guess
			pTarget->SetMemberString( KV3TRANSFER_CLASSNAME_MEMBER, pClass->GetName() );
		}
	}
	else
	{
		// no vtable, don't need a class specifier
		pTarget->RemoveMember( KV3TRANSFER_CLASSNAME_MEMBER );
	}

	for ( CSchemaFieldIterator it( pClass, SCHEMA_BASE_TRAVERSAL_FULL ); it.IsValid(); it.Advance() )
	{
		KeyValues3 *pMember = pTarget->FindMember( CKV3MemberName( it.GetFieldName() ) );
		if ( pMember == NULL )
		{
			// add member
			pMember = pTarget->SetMemberToNull( CKV3MemberName( it.GetFieldName() ) );
			const char *pDefaultValue = MDefaultString::GetValue( it, NULL );
			ImprintSchemaTypeOnKV3( pMember, it.GetRawFieldHandle(), it.GetType(), pDefaultValue );
		}
		else
		{
			ImprintSchemaTypeOnKV3( pMember, it.GetRawFieldHandle(), it.GetType(), NULL );
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void RemoveKeysNotInClass( KeyValues3 *pTarget, ClassIntrospectionHandle_t pClass )
{
	CSchemaFieldIterator fieldFinder( pClass, SCHEMA_BASE_TRAVERSAL_FULL );

	for ( int i = pTarget->GetMemberCount() - 1; i >= 0; --i )
	{
		const char *pMemberName = pTarget->GetMemberName( i );

		if ( fieldFinder.FindField( pMemberName ) )
		{
			continue;
		}

		// special case
		if ( !V_strcmp( pMemberName, KV3TRANSFER_CLASSNAME_MEMBER ) )
		{
			continue;
		}

		pTarget->RemoveMember( i );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool KV3IsEqualToDefault( KeyValues3 *pTarget, FieldIntrospectionHandle_t pAssociatedField, const char *pDefaultValue )
{
	bool bEqualToDefault = false;

	auto simpleValueOp = [pDefaultValue,&bEqualToDefault]( KeyValues3 *pData, KeyValues3Type_t nType )
	{
		bEqualToDefault = pData->EqualsValueFromString( pDefaultValue );
	};

	auto fixedArrayOp = [pDefaultValue,&bEqualToDefault]( KeyValues3 *pData, int nArrayCount, CSchemaType *pArrayOf )
	{
		bEqualToDefault = pData->EqualsValueFromString( pDefaultValue );
	};

	auto varArrayOp = [pDefaultValue,&bEqualToDefault]( KeyValues3 *pData, CSchemaType *pArrayOf )
	{
		bEqualToDefault = pData->EqualsValueFromString( pDefaultValue );
	};

	auto classOp = [&bEqualToDefault]( KeyValues3 *pData, ClassIntrospectionHandle_t pClass )
	{
		// check all the fields (NOTE: this won't check "_class" and that's good, because if it's
		// present it probably means that we have a vtable and need to retain our class identity)
		int nNumEqualToDefault = 0;
		for ( CSchemaFieldIterator it( pClass, SCHEMA_BASE_TRAVERSAL_FULL ); it.IsValid(); it.Advance() )
		{
			KeyValues3 *pMember = pData->FindMember( CKV3MemberName( it.GetFieldName() ) );
			if ( !pMember )
				continue;

			const char *pFieldDefaultValue = MDefaultString::GetValue( it, NULL );
			if ( !KV3IsEqualToDefault( pMember, it.GetRawFieldHandle(), pFieldDefaultValue ) )
			{
				// not equal
				return;
			}
			nNumEqualToDefault++;
		}

		// all the present members were equal to their default, but see if there were any misc keys
		// that don't correspond to class members
		bEqualToDefault = ( nNumEqualToDefault == pData->GetMemberCount() );
	};

	auto failureOp = []( KeyValues3 *pData )
	{
	};

	OperateSchemaTypeOnKV3( pTarget, pAssociatedField, pAssociatedField->GetType(), simpleValueOp, fixedArrayOp, classOp, varArrayOp, failureOp );
	return bEqualToDefault;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void RemoveKeysInTableEqualToDefault_Shallow( KeyValues3 *pTarget, ClassIntrospectionHandle_t pClass )
{
	for ( CSchemaFieldIterator it( pClass, SCHEMA_BASE_TRAVERSAL_FULL ); it.IsValid(); it.Advance() )
	{
		KeyValues3 *pMember = pTarget->FindMember( CKV3MemberName( it.GetFieldName() ) );
		if ( !pMember )
			continue;

		const char *pDefaultValue = MDefaultString::GetValue( it, NULL );

		if ( KV3IsEqualToDefault( pMember, it.GetRawFieldHandle(), pDefaultValue ) )
		{
			pTarget->RemoveMember( pMember );
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void ImprintSchemaClassOnKV3( KeyValues3 *pTarget, const char *pClassName )
{
	ClassIntrospectionHandle_t pClass = SchemaTypeScope()->FindDeclaredClass( pClassName );
	if ( !pClass )
		return;

	ImprintSchemaClassOnKV3( pTarget, pClass );
}

void RemoveKeysNotInClass( KeyValues3 *pTarget, const char *pClassName )
{
	ClassIntrospectionHandle_t pClass = SchemaTypeScope()->FindDeclaredClass( pClassName );
	if ( !pClass )
		return;

	RemoveKeysNotInClass( pTarget, pClass );
}

void RemoveKeysInTableEqualToDefault_Shallow( KeyValues3 *pTarget, const char *pClassName )
{
	ClassIntrospectionHandle_t pClass = SchemaTypeScope()->FindDeclaredClass( pClassName );
	if ( !pClass )
		return;

	RemoveKeysInTableEqualToDefault_Shallow( pTarget, pClass );
}

#endif
