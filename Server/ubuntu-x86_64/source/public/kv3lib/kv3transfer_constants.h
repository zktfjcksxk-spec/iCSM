//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================
#pragma once
//
// DO NOT ADD INCULDES
//
// The goal of this file is to have be allow kv3 transfer specializations to be declared in header
// files without pulling in a bunch of dependencies. The only things that should be put here are 
// constants, macros, enumerations, etc. that don't require any external dependencies.
//


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CKV3TransferBlockAllocator;
class CKV3TransferSaveContext;
class CKV3TransferLoadContext;


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#define KV3TRANSFER_CLASSNAME_MEMBER "_class"
#define KV3TRANSFER_CLASSNAME_MAX_LENGTH 256


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
enum KV3TransferResult_t
{
	KV3TRANSFER_FAIL,
	KV3TRANSFER_SUCCESS,
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#define MAKE_KV3TRANSFER_INTERFACE_ID_BYTE_POS( byteVal, shft )	KV3Transfer_InterfaceId_t( uint32(uint8(byteVal)) << uint8(shft * 8) )
#define MAKE_KV3TRANSFER_INTERFACE_ID( a, b, c, d )					KV3Transfer_InterfaceId_t( MAKE_KV3TRANSFER_INTERFACE_ID_BYTE_POS(a, 0) | MAKE_KV3TRANSFER_INTERFACE_ID_BYTE_POS(b, 1) | MAKE_KV3TRANSFER_INTERFACE_ID_BYTE_POS(c, 2) | MAKE_KV3TRANSFER_INTERFACE_ID_BYTE_POS(d, 3) )

#define KV3TRANSFER_INTERFACE_ID_INVALID KV3Transfer_InterfaceId_t(-1)
typedef uint32 KV3Transfer_InterfaceId_t;


//--------------------------------------------------------------------------------------------------
// KV3TransferClassBehavior_t
//
// Describes how a class will be transferred to/from a KeyValues3 value under KV3Transfer. 
//--------------------------------------------------------------------------------------------------
enum KV3TransferClassBehavior_t
{
	// Default - causes a serialization error if the class is encountered
	KV3TRANSFER_CLASS_UNIMPLEMENTED,

	// Your class will be serialized as a table (and is NOT polymorphic; ie. pointer to it should never resolve to a different derived class)
	// KV3TransferSave/Load will be passed an empty table KV3 to populate.
	KV3TRANSFER_CLASS_AS_SIMPLE_TABLE,

	// Your class will be serialized as a table with a "_class" key identifying its exact polymorphic class
	// - You must also implement KV3TransferPolymorphicClassname() to identify your derived class name, and KV3TransferAllocateClassInstance() to unserialize pointers.
	// - KV3TransferSave/Load will be passed an table KV3 to populate, with the "_class" key already set to the polymorphic class name.
	// - KV3Transfer_AllocateClassInstance() will be used to allocate instances of your class when necessary.
	KV3TRANSFER_CLASS_AS_POLYMORPHIC_TABLE,

	// Your class will completely handle its serialization.
	// KV3TransferSave/Load will be passed a null KV3 to handle however you want.
	KV3TRANSFER_CLASS_AS_DATA,
};


//--------------------------------------------------------------------------------------------------
// Your class should look like this if you want to serialize as KV3TRANSFER_CLASS_AS_SIMPLE_TABLE or KV3TRANSFER_CLASS_AS_DATA
//--------------------------------------------------------------------------------------------------
class CKV3TransferExample_Simple
{
public:
	enum { KV3TRANSFER_BEHAVIOR = KV3TRANSFER_CLASS_AS_SIMPLE_TABLE };

	void KV3TransferSave( CKV3TransferSaveContext *pContext ) const {}
	void KV3TransferLoad( CKV3TransferLoadContext *pContext ) {}
};


//--------------------------------------------------------------------------------------------------
// If you need to unserialize a pointer, then you need to also provide an allocator (KV3TransferAllocateClassInstance)
//--------------------------------------------------------------------------------------------------
class CKV3TransferExample_SimpleWithAllocation
{
	enum { KV3TRANSFER_BEHAVIOR = KV3TRANSFER_CLASS_AS_SIMPLE_TABLE };

	static CKV3TransferExample_SimpleWithAllocation *KV3TransferAllocateClassInstance( CKV3TransferBlockAllocator *pBlockAllocator, const char *pDerivedClassName )
	{
		return new CKV3TransferExample_SimpleWithAllocation;
	}

	void KV3TransferSave( CKV3TransferSaveContext *pContext ) const {}
	void KV3TransferLoad( CKV3TransferLoadContext *pContext ) {}
};


//--------------------------------------------------------------------------------------------------
// For a polymorphic class, you need to provide a mapping between derived classes and a 'classname' (doesn't have to be the C++ classname)
//--------------------------------------------------------------------------------------------------
class CKV3TransferExample_Polymorphic
{
public:
	enum { KV3TRANSFER_BEHAVIOR = KV3TRANSFER_CLASS_AS_POLYMORPHIC_TABLE };

	static void KV3TransferPolymorphicClassname( const CKV3TransferExample_Polymorphic *pInstance, char (&pOutPolymorphicClassName)[KV3TRANSFER_CLASSNAME_MAX_LENGTH] )
	{
		const char *pDerivedClassName = ""; // eg. pInstance->GetDerivedClassName();
		V_strcpy_safe( pOutPolymorphicClassName, pDerivedClassName );
	}
	static CKV3TransferExample_Polymorphic *KV3TransferAllocateClassInstance( CKV3TransferBlockAllocator *pBlockAllocator, const char *pDerivedClassName )
	{
		CKV3TransferExample_Polymorphic *pNew = nullptr; // eg. CreateDerivedPolymorphicClass( pDerivedClassName );
		return pNew;
	}

	virtual void KV3TransferSave( CKV3TransferSaveContext *pContext ) const {}
	virtual void KV3TransferLoad( CKV3TransferLoadContext *pContext ) {}
};


//--------------------------------------------------------------------------------------------------
//
// Override these functions (probably in your enum header) if you want to use KV3Transfer for a non-schematized enum:
//
// const char *KV3Transfer_EnumeratorNameFromValue( TEnum nValue );
// bool KV3Transfer_EnumeratorValueFromName( const char *pEnumeratorName, TEnum *pOutValue );
//
// Or use BEGIN_KV3TRANSFER_ENUM_HELPERS, below.
//
//--------------------------------------------------------------------------------------------------

template< typename TEnum >
const char *KV3Transfer_EnumeratorNameFromValue( ... )
{
	AssertMsg( false, "KV3Transfer_EnumeratorNameFromValue not specified for your enum" );
    TEnum FORCE_COMPILER_ERROR = (uint64*)1;
	return NULL;
}

template< typename TEnum >
bool KV3Transfer_EnumeratorValueFromName( ... )
{
	AssertMsg( false, "KV3Transfer_EnumeratorValueFromName not specified for your enum" );
    TEnum FORCE_COMPILER_ERROR = (uint64*)1;
	return false;
}


//--------------------------------------------------------------------------------------------------
//
// Helpers to declare a string-enum mapping without schematization.
//
// Example:
//
//	BEGIN_KV3TRANSFER_ENUM_HELPERS( GestureType_t, GESTURE_TYPE_COUNT, GESTURE_TYPE_BOTH )
//		KV3TRANSFER_ENUM_HELPER( GESTURE_TYPE_BOTH	, "Both"		) 
//		KV3TRANSFER_ENUM_HELPER( GESTURE_TYPE_LEFT	, "Left"		) 
//		KV3TRANSFER_ENUM_HELPER( GESTURE_TYPE_RIGHT	, "Right"		) 
//		KV3TRANSFER_ENUM_HELPER( GESTURE_TYPE_HEAD	, "Head"		) 
//	END_KV3TRANSFER_ENUM_HELPERS()
//
// (See KV3Transfer_EnumeratorNameFromValue specialization in kv3transfer.h)
//
//--------------------------------------------------------------------------------------------------
template< class T > struct KV3Transfer_EnumHelpers_t
{
	static bool const is_present = false;
};

template< class TEnum > struct KV3Transfer_EnumHelpers_StringPair_t
{
	TEnum m_nEnum;
	const char *m_pString;
};

template< class TEnum > struct KV3Transfer_EnumHelpers_StringPairList_t
{
	KV3Transfer_EnumHelpers_StringPair_t< TEnum > *m_pList;
	size_t m_nCount;
};

#define BEGIN_KV3TRANSFER_ENUM_HELPERS( _ENUM_NAME, _ENUM_COUNT, _DEFAULT_VALUE )									\
	template<> struct KV3Transfer_EnumHelpers_t< _ENUM_NAME >														\
	{																												\
		static bool const is_present			= true;																\
		static const _ENUM_NAME ENUM_COUNT		= _ENUM_COUNT;														\
		static const _ENUM_NAME ENUM_DEFAULT	= _DEFAULT_VALUE;													\
																													\
		static KV3Transfer_EnumHelpers_StringPairList_t< _ENUM_NAME > Pairs()										\
		{																											\
			static KV3Transfer_EnumHelpers_StringPair_t< _ENUM_NAME > s_Elements[] =								\
			{																										\

#define KV3TRANSFER_ENUM_HELPER( _VALUE, _STRING )																	\
				{	_VALUE			, _STRING,		},

#define END_KV3TRANSFER_ENUM_HELPERS()																				\
			};																										\
			COMPILE_TIME_ASSERT( ARRAYSIZE( s_Elements ) == ENUM_COUNT );											\
			return { s_Elements, ARRAYSIZE( s_Elements ) };															\
		}																											\
	};
