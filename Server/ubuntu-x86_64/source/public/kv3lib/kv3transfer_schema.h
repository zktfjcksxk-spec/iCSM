//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================
#pragma once


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#include "kv3lib/kv3transfer.h"
#include "schemasystem/schemabinding_n.h"
#include "schemalib/schematype.h"


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
template< typename TClass >
TClass *KV3TransferSchema_AllocateClassInstance( CKV3TransferBlockAllocator *pBlockAllocator, const char *pDerivedClassName )
{
	Assert( ( pDerivedClassName != nullptr ) == ( SCHEMA_TYPE_TRAITS_is_polymorphic( TClass ) ) );

	CSchemaType_DeclaredClass *pClassType;

	if ( pDerivedClassName == nullptr )
	{
		// non-polymorphic case
		pClassType = SchemaTypeOf< TClass >()->template As< CSchemaType_DeclaredClass >();
	}
	else
	{
		// polymorphic, pDeriveClassName is subclass
		pClassType = SchemaTypeScope()->Type_DeclaredClass( pDerivedClassName )->As<CSchemaType_DeclaredClass>();
	}

	if ( !pClassType )
	{
		return NULL;
	}

	ClassIntrospectionHandle_t pClass = pClassType->GetClass();

	if ( !pClass || pClass->GetBinding()->IsAbstractBinding() )
	{
		return NULL;
	}

	if ( pBlockAllocator )
	{
		void *pResult = pBlockAllocator->AllocBlockBytes( pClass->GetSizeOf(), pClass->GetAlignOf() );
		pClass->GetBinding()->ConstructInPlace( pResult );
		return (TClass*)pResult;
	}
	else
	{
		return (TClass*)pClass->GetBinding()->Allocate();
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
template< typename TClass >
bool KV3TransferSchema_ClassName( const TClass *pObject, char (&pOutPolymorphicClassName)[KV3TRANSFER_CLASSNAME_MAX_LENGTH] )
{
	Assert( SCHEMA_TYPE_TRAITS_is_polymorphic( TClass ) );
	const CSchemaClassBindingBase *pClassBinding = TClass::Schema_StaticBinding();
	const CSchemaClassBindingBase *pDerivedType = pClassBinding->DeduceDerivedType( pObject );
	V_strcpy_safe( pOutPolymorphicClassName, pDerivedType->GetName() );
	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
template< typename TEnum >
typename std::enable_if< SCHEMA_IS_SCHEMA_ENUM( TEnum ), const char* >::type KV3Transfer_EnumeratorNameFromValue( TEnum nValue )
{
	CSchemaType_DeclaredEnum *pEnum = SchemaTypeOf<TEnum>()->template As<CSchemaType_DeclaredEnum>();
	EnumIntrospectionHandle_t pEnumInfo = pEnum->GetEnum();
	EnumeratorIntrospectionHandle_t pEnumeratorInfo = pEnumInfo->FindMemberByValue( nValue );
	if ( pEnumeratorInfo )
	{
		return pEnumeratorInfo->GetName();
	}
	else
	{
		return NULL;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
template< typename TEnum >
typename std::enable_if< SCHEMA_IS_SCHEMA_ENUM( TEnum ), bool >::type KV3Transfer_EnumeratorValueFromName( const char *pEnumeratorName, TEnum *pOutValue )
{
	CSchemaType_DeclaredEnum *pEnum = SchemaTypeOf<TEnum>()->template As<CSchemaType_DeclaredEnum>();
	EnumIntrospectionHandle_t pEnumInfo = pEnum->GetEnum();
	EnumeratorIntrospectionHandle_t pEnumeratorInfo = pEnumInfo->FindMemberByName( pEnumeratorName );
	if ( pEnumeratorInfo )
	{
		// string was an enumerant name
		*pOutValue = (TEnum)pEnumeratorInfo->GetValue();
		return true;
	}

	return false;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
DECLARE_SCHEMA_META_TAG( MEmitKV3TransferIgnoreMultipleInheritance, META_TAG_ON_CLASS, META_TAG_ONLY() );
DECLARE_SCHEMA_META_TAG( MEmitKV3DontClearMissingFields, META_TAG_ON_CLASS, META_TAG_ONLY() );

//--------------------------------------------------------------------------------------------------
// Custom Save/Load functions
// Eg.
//	META( MEmitKV3TransferCustomSaveFn="SaveFn"; MEmitKV3TransferCustomLoadFn="LoadFn" );
// Where your class has methods:
//	void SaveFn( CKV3TransferSaveContext *pContext, const char *pMemberName, KeyValues3LowercaseHash_t memberNameHash, const T &value )
//	void LoadFn( CKV3TransferLoadContext *pContext, const char *pMemberName, KeyValues3LowercaseHash_t memberNameHash, T &value )
//--------------------------------------------------------------------------------------------------
DECLARE_SCHEMA_META_TAG( MEmitKV3TransferCustomSaveFn, META_TAG_ON_FIELD, META_VALUE( const char * ) );
DECLARE_SCHEMA_META_TAG( MEmitKV3TransferCustomLoadFn, META_TAG_ON_FIELD, META_VALUE( const char * ) ); // eg. META(  ); where: void SaveFn( CKV3TransferSaveContext *pContext, const char *pFieldName, T &value );

// Alternate name to use for the KV3 member instead of the C++ field name
DECLARE_SCHEMA_META_TAG( MKV3TransferName, META_TAG_ON_FIELD, META_VALUE( const char * ) );

// Signature is: void Transfer[Pre|Post]SaveFn( CKV3TransferSaveContext *pContext )
// Signature is: void Transfer[Pre|Post]LoadFn( CKV3TransferLoadContext *pContext )
// TYPEMETA( MEmitKV3TransferPreLoadFn = "MyFunc" );
DECLARE_SCHEMA_META_TAG( MEmitKV3TransferPreLoadFn, META_TAG_ON_CLASS, META_VALUE( const char * ) );
DECLARE_SCHEMA_META_TAG( MEmitKV3TransferPostLoadFn, META_TAG_ON_CLASS, META_VALUE( const char * ) );
DECLARE_SCHEMA_META_TAG( MEmitKV3TransferPreSaveFn, META_TAG_ON_CLASS, META_VALUE( const char * ) );
DECLARE_SCHEMA_META_TAG( MEmitKV3TransferPostSaveFn, META_TAG_ON_CLASS, META_VALUE( const char * ) );

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
DECLARE_SCHEMA_CODEGEN_TAG( MEmitKV3Transfer, META_TAG_ON_FIELD,
(
	// Multiple inheritance is not supported (could implement pointer tracking but it's a pain)
	if ( class_has_multiple_bases() && !class_has_meta( MEmitKV3TransferIgnoreMultipleInheritance ) )
	{
		emit( "" )
		emit( "#error Cannot emit KV3Transfer function for class '$class_name$' because it uses multiple inheritance. Mark the class with MEmitKV3TransferIgnoreMultipleInheritance to treat it as single-inheritance" )
		for_each_base
		{
			emit( "// Multiply inherited base class: $loop_value$" )
		}
		emit( "" )
	}

	// Emit Save Function
	emit( "void $class_name$::KV3TransferSave( CKV3TransferSaveContext *pContext ) const" )
	emit( "{" )
	emit( "	KV3TransferSave_$class_name$( pContext );" )
	emit( "}" )
	emit( "" )
	emit( "void $class_name$::KV3TransferSave_$class_name$( CKV3TransferSaveContext *pContext ) const" )
	emit( "{" )
	if ( class_has_meta( MEmitKV3TransferPreSaveFn ) )
	{
		emit( "	$@remove_quotes(class_tag_value.MEmitKV3TransferPreSaveFn)$( pContext ); // MEmitKV3TransferPreSaveFn" )
	}

	if ( class_has_bases() )
	{
		emit( "// !!! NOTE: if you're getting a compiler error here you probably forgot your CLASS_USES_KV3TRANSFER macro in your base class!" )
		emit( "	KV3TransferSave_$baseclass_name$( pContext ); // chain to base" )
		emit( "" )
	}

	for_all_fields
	{
		if ( item_has_meta( MEmitKV3TransferCustomSaveFn ) )
		{
			emit( "	$@remove_quotes(tag_value.MEmitKV3TransferCustomSaveFn)$( pContext, CKV3MemberName( $item_quoted_name$, StringTokenFromHashCode($@as_string_token_hash(item_name)$) ), $item_name$ ); // MEmitKV3TransferCustomSave" )
		}
		else if ( item_has_meta( MKV3TransferName ) )
		{
			emit( "	pContext->SaveValueToMember( CKV3MemberName( $tag_value.MKV3TransferName$, StringTokenFromHashCode( $@as_string_token_hash( @remove_quotes( tag_value.MKV3TransferName ) )$ ) ), $item_name$ ); // MKV3TransferName" )
		}
		else
		{
			emit( "	pContext->SaveValueToMember( CKV3MemberName( $item_quoted_name$, StringTokenFromHashCode($@as_string_token_hash(item_name)$) ), $item_name$ );" )
		}
	}

	if ( class_has_meta( MEmitKV3TransferPostSaveFn ) )
	{
		emit( "	$@remove_quotes(class_tag_value.MEmitKV3TransferPostSaveFn)$( pContext ); // MEmitKV3TransferPostSaveFn" )
	}
	emit( "}" )
	emit( "" )

	// Emit Load Function
	emit( "void $class_name$::KV3TransferLoad( CKV3TransferLoadContext *pContext )" )
	emit( "{" )
	emit( "	KV3TransferLoad_$class_name$( pContext );" )
	emit( "}" )
	emit( "" )
	emit( "void $class_name$::KV3TransferLoad_$class_name$( CKV3TransferLoadContext *pContext )" )
	emit( "{" )
	if ( class_has_meta( MEmitKV3TransferPreLoadFn ) )
	{
		emit( "	$@remove_quotes(class_tag_value.MEmitKV3TransferPreLoadFn)$( pContext ); // MEmitKV3TransferPreLoadFn" )
	}

	if ( class_has_bases() )
	{
		emit( "// !!! NOTE: if you're getting a compiler error here you probably forgot your CLASS_USES_KV3TRANSFER macro in your base class!" )
		emit( "	KV3TransferLoad_$baseclass_name$( pContext ); // chain to base" )
		emit( "" )
	}

	for_all_fields
	{
		if ( item_has_meta( MEmitKV3TransferCustomLoadFn ) )
		{
			emit( "	$@remove_quotes(tag_value.MEmitKV3TransferCustomLoadFn)$( pContext, CKV3MemberName( $item_quoted_name$, StringTokenFromHashCode($@as_string_token_hash(item_name)$) ), $item_name$ ); // MEmitKV3TransferCustomLoad" )
		}
		else if ( item_has_meta( MKV3TransferName ) )
		{
			emit( "	pContext->LoadValueFromMember( CKV3MemberName( $tag_value.MKV3TransferName$, StringTokenFromHashCode( $@as_string_token_hash( @remove_quotes( tag_value.MKV3TransferName ) )$ ) ), $item_name$ ); // MKV3TransferName" )
		}
		else if ( class_has_meta( MEmitKV3DontClearMissingFields ) )
		{
			emit( "	pContext->LoadValueFromMemberIfPresent( CKV3MemberName( $item_quoted_name$, StringTokenFromHashCode($@as_string_token_hash(item_name)$) ), $item_name$ ); // MEmitKV3DontClearMissingFields" )
		}
		else if ( item_has_meta( MDefaultString ) )
		{
			emit( "	pContext->LoadValueFromMemberOrDefault( CKV3MemberName( $item_quoted_name$, StringTokenFromHashCode($@as_string_token_hash(item_name)$) ), $item_name$, $tag_value.MDefaultString$ );" )
		}
		else
		{
			emit( "	pContext->LoadValueFromMember( CKV3MemberName( $item_quoted_name$, StringTokenFromHashCode($@as_string_token_hash(item_name)$) ), $item_name$ );" )
		}
	}

	if ( class_has_meta( MEmitKV3TransferPostLoadFn ) )
	{
		emit( "	$@remove_quotes(class_tag_value.MEmitKV3TransferPostLoadFn)$( pContext ); // MEmitKV3TransferPostLoadFn" )
	}

	emit( "}" )
	emit( "" )

	// Ensure that we didn't make a nonvirtual KV3Transfer on a virtual class
	emit( "COMPILE_TIME_ASSERT( SCHEMA_TYPE_TRAITS_is_polymorphic( $class_name$ ) == $class_name$::KV3TRANSFER_IS_VIRTUAL );" )
) );

#define CLASS_USES_KV3TRANSFER_DATA( classname ) \
	META_USE_CODEGEN_TAG( MEmitKV3Transfer ); \
	enum { KV3TRANSFER_BEHAVIOR = KV3TRANSFER_CLASS_AS_SIMPLE_TABLE }; \
	static classname *KV3TransferAllocateClassInstance( CKV3TransferBlockAllocator *pBlockAllocator, const char *pDerivedClassName )\
		{ return KV3TransferSchema_AllocateClassInstance<classname>( pBlockAllocator, pDerivedClassName ); } \
	void KV3TransferSave( CKV3TransferSaveContext *pContext ) const; \
	void KV3TransferLoad( CKV3TransferLoadContext *pContext ); \
	void KV3TransferSave_##classname( CKV3TransferSaveContext *pContext ) const; \
	void KV3TransferLoad_##classname( CKV3TransferLoadContext *pContext ); \
	enum { KV3TRANSFER_IS_VIRTUAL = 0 };

#define CLASS_USES_KV3TRANSFER_VIRTUAL( classname ) \
	META_USE_CODEGEN_TAG( MEmitKV3Transfer ); \
	enum { KV3TRANSFER_BEHAVIOR = KV3TRANSFER_CLASS_AS_POLYMORPHIC_TABLE }; \
	static void KV3TransferPolymorphicClassname( const classname *pObject, char (&pOutPolymorphicClassName)[KV3TRANSFER_CLASSNAME_MAX_LENGTH] ) \
		{ KV3TransferSchema_ClassName( pObject, pOutPolymorphicClassName ); } \
	static classname *KV3TransferAllocateClassInstance( CKV3TransferBlockAllocator *pBlockAllocator, const char *pDerivedClassName )\
		{ return KV3TransferSchema_AllocateClassInstance<classname>( pBlockAllocator, pDerivedClassName ); } \
	virtual void KV3TransferSave( CKV3TransferSaveContext *pContext ) const; \
	virtual void KV3TransferLoad( CKV3TransferLoadContext *pContext ); \
	void KV3TransferSave_##classname( CKV3TransferSaveContext *pContext ) const; \
	void KV3TransferLoad_##classname( CKV3TransferLoadContext *pContext ); \
	enum { KV3TRANSFER_IS_VIRTUAL = 1 };
