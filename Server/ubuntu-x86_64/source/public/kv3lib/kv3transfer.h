//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================
#pragma once


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#include "kv3lib/keyvalues3.h"
#include "kv3lib/kv3transfer_constants.h"
#include "filesystem.h"

#include "tier1/utlbuffer.h"
#include "tier1/utlhashtable.h"
#include "tier1/utlstack.h"
#include "tier1/utlsymbollarge.h"
#include "tier1/smartptr.h"
#include "mathlib/ssemath.h"

//#define KV3TRANSFER_NOINLINE __declspec(noinline)
#define KV3TRANSFER_NOINLINE

#ifdef CSTRIKE15
// $$$REI Hack: We don't have utltypetraits.h in S1 and I'm not sure it works with the ancient version of GCC we are using.
// $$$REI However, it's safe to Construct/Destruct elements of pod types, which is what we use this for here.
template<typename T> inline bool IsComplexType() { return true; }
#endif

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#define KV3TRANSFERID_IKV3TransferInterface_UtlSymbolLarge MAKE_KV3TRANSFER_INTERFACE_ID( 'S', 'Y', 'M', 'L' )

class IKV3TransferInterface_UtlSymbolLarge
{
public:
	static KV3Transfer_InterfaceId_t GetInterfaceId() { return KV3TRANSFERID_IKV3TransferInterface_UtlSymbolLarge; }

	virtual CUtlSymbolLarge AddString( const char *pString ) = 0;
};

template< class TSymbolTable >
class CKV3Transfer_UtlSymbolLargeInterface: public IKV3TransferInterface_UtlSymbolLarge
{
public:
	CKV3Transfer_UtlSymbolLargeInterface( TSymbolTable *pSymbolTable )
		: m_pSymbolTable( pSymbolTable )
	{
	}

	virtual CUtlSymbolLarge AddString( const char *pString ) OVERRIDE
	{
		return m_pSymbolTable->AddString( pString );
	}

	TSymbolTable *m_pSymbolTable;
};


//--------------------------------------------------------------------------------------------------
// Simple block allocator for ensuring contiguous allocations - requires a max size up-front
//--------------------------------------------------------------------------------------------------
class CKV3TransferBlockAllocator
{
public:
	CKV3TransferBlockAllocator();
	~CKV3TransferBlockAllocator();

	//--------------------------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------------------------
	void Init( uint nReserveSize );
	void Init( void *pExternalAllocation, uint nSize );
	void Free();

	byte *TakeControlOfAllocaction(); // call FreeAllocation when done
	static void FreeAllocation( byte *pAlloc );

	//--------------------------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------------------------
	template< class T > T *Alloc( int nCount );
	byte *AllocBlockBytes( int nSize, uintp nAlign );

	uint64 GetBlockSize()
	{
		return (uint64)( m_pNextAlloc - m_pBufferBase );
	}
private:
	byte *m_pBufferBase;
	byte *m_pNextAlloc;
	size_t m_nBufferSize;
	bool m_bExternalAllocation;
};


//--------------------------------------------------------------------------------------------------
// WARNING: These objects will *not* have its destructor called.
//--------------------------------------------------------------------------------------------------
template< class T > T *CKV3TransferBlockAllocator::Alloc( int nCount )
{
	T *pResult = (T*)AllocBlockBytes( sizeof(T) * nCount, VALIGNOF_PORTABLE(T) );
	if ( IsComplexType<T>() )
	{
		for ( int i = 0; i < nCount; ++i )
		{
			Construct( pResult + i );
		}
	}
	return pResult;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
inline const char *KV3TransferClassname( const KeyValues3 *pKV3 )
{
	const KeyValues3 *pClassNameMember = pKV3->FindMember( KV3TRANSFER_CLASSNAME_MEMBER );
	return pClassNameMember ? pClassNameMember->GetValueString() : "";
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#if 0
#define KV3TransferDebugMsg Msg
#else
#define KV3TransferDebugMsg( ... )
#endif


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CKV3TransferContextBase
{
public:
	CKV3TransferContextBase();

	void NoteFailure( PRINTF_FORMAT_STRING const char *pFormat, ... );

	KV3TransferResult_t GetResult();
	const char *GetErrorMessage() { return m_ErrorMessage.Get(); }

	template< class TInterface >
	TInterface *FindInterface()
	{
		return (TInterface*)FindInterfaceVoid( TInterface::GetInterfaceId() );
	}

	template< class TInterface >
	void AddInterface( TInterface *pInterface )
	{
		return AddInterfaceVoid( TInterface::GetInterfaceId(), (void*)pInterface );
	}

protected:
	void *FindInterfaceVoid( KV3Transfer_InterfaceId_t nId );
	void AddInterfaceVoid( KV3Transfer_InterfaceId_t nId, void *pInterface );

	KV3TransferResult_t m_Result;
	CUtlString m_ErrorMessage;

	struct TransferInterface_t
	{
		KV3Transfer_InterfaceId_t m_nId;
		void *m_pInterface;
	};

	enum { MAX_TRANSFER_INTERFACES = 4 }; // increase if necessary
	TransferInterface_t m_Interfaces[MAX_TRANSFER_INTERFACES];
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
template< class T >
bool GetPolymorphicClassName( const T *pObject, char (&pOutPolymorphicClassName)[KV3TRANSFER_CLASSNAME_MAX_LENGTH], ... )
{
	AssertMsg( false, "GetPolymorphicClassName should only be called if your KV3TRANSFER_BEHAVIOR is KV3TRANSFER_CLASS_AS_POLYMORPHIC_TABLE" );
	return false;
}

template< class T >
bool GetPolymorphicClassName( const T *pObject, char (&pOutPolymorphicClassName)[KV3TRANSFER_CLASSNAME_MAX_LENGTH], typename std::enable_if< T::KV3TRANSFER_BEHAVIOR == KV3TRANSFER_CLASS_AS_POLYMORPHIC_TABLE >::type *, ...  )
{
	T::KV3TransferPolymorphicClassname( pObject, pOutPolymorphicClassName );
	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CKV3TransferSaveContext: public CKV3TransferContextBase
{
public:
	//--------------------------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------------------------
	CKV3TransferSaveContext();

	KeyValues3 *CreateTargetMember( CKV3MemberName memberNameAndHash )
	{
		KeyValues3 *pSaveToMember = m_pTargetObject->FindMember( memberNameAndHash );
		if ( pSaveToMember )
		{
			NoteFailure( "Double-save to Member '%s'", memberNameAndHash.m_pString );
			return pSaveToMember;
		}

		return m_pTargetObject->SetMemberToNull( memberNameAndHash );
	}

	template< typename T >
	void SaveValueToMember( CKV3MemberName memberNameAndHash, const T &sourceValue )
	{
		KeyValues3 *pSaveToMember = CreateTargetMember( memberNameAndHash );
		SaveValueDirect( sourceValue, pSaveToMember );
	}

	template< typename T >
	void SaveValueDirect( const T &sourceValue, KeyValues3 *pSaveToMember );

	template< typename T>
	void SaveClassPointer( T* const &pClassInstance, KeyValues3 *pSaveToValue )
	{
		if ( pClassInstance == nullptr )
		{
			pSaveToValue->SetToNull();
			return; // no more work
		}

		// figure out how this object wants to be serialized
		KV3TransferClassBehavior_t nClassBehavior = (KV3TransferClassBehavior_t)T::KV3TRANSFER_BEHAVIOR;

		// for polymorphic classes, figure out the classname
		char pPolymorphicClassName[ KV3TRANSFER_CLASSNAME_MAX_LENGTH ];
		if ( ( nClassBehavior == KV3TRANSFER_CLASS_AS_POLYMORPHIC_TABLE ) && !GetPolymorphicClassName< T >( pClassInstance, pPolymorphicClassName, 0 /* for proper ... dispatch */ ) )
		{
			NoteFailure( "Failed to determine polymorphic class name" );
			pSaveToValue->SetToNull();
			return;
		}
		
		// prep the kv3
		if ( !PrepareTargetForClass( pSaveToValue, nClassBehavior, pPolymorphicClassName ) )
			return; // something went wrong

		// recurse into the class
		PushTarget( pSaveToValue );
		pClassInstance->KV3TransferSave( this );
		PopTarget();
	}


	KeyValues3 *TargetObject() { return m_pTargetObject; }

private:
	bool PrepareTargetForClass( KeyValues3 *pObjectValue, KV3TransferClassBehavior_t nClassBehavior, const char *pPolymorphicClassName );
	void PushTarget( KeyValues3 *pTarget );
	void PopTarget();

	KeyValues3 *m_pTargetObject;
	CUtlStack< KeyValues3 *> m_TargetStack;
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CKV3TransferLoadContext: public CKV3TransferContextBase
{
public:
	CKV3TransferLoadContext();
	~CKV3TransferLoadContext();

	void SetBlockAllocator( uint nReserveSize );
	void SetExternalBlockAllocation( void *pExternal, uint nSize );
	CKV3TransferBlockAllocator *GetBlockAllocator() { return m_pBlockAllocator; }

	uint64 GetExternalBlockAllocationUsage();

	void PushSource( const KeyValues3 *pSource );
	void PopSource();

	const KeyValues3 *SourceObject() { return m_pSourceObject; }

	template< typename T >
	KV3TRANSFER_NOINLINE
	void LoadValueFromMember( CKV3MemberName memberNameAndHash, T &destValue )
	{
		const KeyValues3 *pLoadFromMember = FindSourceMember( memberNameAndHash );
		if ( pLoadFromMember )
		{
			LoadValueDirect( destValue, pLoadFromMember );
		}
		else
		{
			// missing Member - use a null value
			// This is important eg. if our target is a structure whose fields we
			// want to assign default values - so we need to recurse into it even
			// though we don't have any data.
			KeyValues3 nullValue;
			LoadValueDirect( destValue, &nullValue );
		}
	}

	template< typename T >
	KV3TRANSFER_NOINLINE
	void LoadValueFromMemberIfPresent( CKV3MemberName memberNameAndHash, T &destValue )
	{
		const KeyValues3 *pLoadFromMember = FindSourceMember( memberNameAndHash );
		if ( !pLoadFromMember )
		{
			// missing Member - explicitly requested a silent ignore
			return;
		}

		LoadValueDirect( destValue, pLoadFromMember );
	}

	template< typename T >
	KV3TRANSFER_NOINLINE
	void LoadValueFromMemberOrDefault( CKV3MemberName memberName, T &destValue, const char *pDefaultString )
	{
		const KeyValues3 *pLoadFromMember = FindSourceMember( memberName );
		if ( pLoadFromMember )
		{
			LoadValueDirect( destValue, pLoadFromMember );
		}
		else
		{
			// missing Member - use the default value
			LoadDefaultDirect( destValue, pDefaultString );
		}
	}

	const KeyValues3 *FindSourceMember( CKV3MemberName memberName )
	{
		return m_pSourceObject->FindMember( memberName );
	}

	template< typename T >
	KV3TRANSFER_NOINLINE
	void LoadValueDirect( T &destValue, const KeyValues3 *pLoadFromMember );

	template< typename T >
	KV3TRANSFER_NOINLINE
	void LoadDefaultDirect( T &destValue, const char *pDefaultValue );

	template< typename T >
	KV3TRANSFER_NOINLINE
	void LoadOwningPointer( T* &value, const KeyValues3 *pLoadFromValue )
	{
		Assert( pLoadFromValue != NULL );
		if ( pLoadFromValue->GetType() == KEYVALUES3_TYPE_NULL )
		{
			// special case: null value -> NULL pointer
			value = NULL;
			return;
		}

		// see if we need to do a polymorphic class allocation or not
		KV3TransferClassBehavior_t nBehavior = (KV3TransferClassBehavior_t)T::KV3TRANSFER_BEHAVIOR;

		if ( nBehavior == KV3TRANSFER_CLASS_AS_POLYMORPHIC_TABLE )
		{
			char pClassName[KV3TRANSFER_CLASSNAME_MAX_LENGTH];
			pLoadFromValue->GetMemberAsString( KV3TRANSFER_CLASSNAME_MEMBER, pClassName, sizeof(pClassName), "" );

			if ( pClassName[0] == '\0' )
			{
				AssertMsg1( false, "CKV3TransferLoadContext: Tried to load a polymorphic pointer with no '%s' key", KV3TRANSFER_CLASSNAME_MEMBER );
				value = NULL;
				NoteFailure( "Tried to load a polymorphic pointer with no '%s' key", KV3TRANSFER_CLASSNAME_MEMBER );
				return;
			}

			value = T::KV3TransferAllocateClassInstance( GetBlockAllocator(), pClassName );
			if ( value == NULL )
			{
				NoteFailure( "Failed to allocate an instance of class '%s'", pClassName );
				return;
			}
		}
		else
		{
			// non-polymorphic case
			Assert( nBehavior != KV3TRANSFER_CLASS_UNIMPLEMENTED );

			value = T::KV3TransferAllocateClassInstance( GetBlockAllocator(), nullptr );
			if ( value == NULL )
			{
				NoteFailure( "Failed to allocate an instance of a class" );
				return;
			}
		}

		// recurse into the class
		LoadClassInstance< T >( value, pLoadFromValue );
	}

	template< typename T >
	KV3TRANSFER_NOINLINE
	inline void LoadClassInstance( T *pClassInstance, const KeyValues3 *pNestedValue )
	{
		PushSource( pNestedValue );

		pClassInstance->KV3TransferLoad( this );

		PopSource();
	};

private:
	CKV3TransferBlockAllocator *m_pBlockAllocator;
	const KeyValues3 *m_pSourceObject; // always equal to the top of m_SourceStack (or NULL)
	CUtlStack< const KeyValues3 *> m_SourceStack;
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
template< typename TEnum >
typename std::enable_if< KV3Transfer_EnumHelpers_t< TEnum >::is_present, const char* >::type KV3Transfer_EnumeratorNameFromValue( TEnum nValue )
{
	const KV3Transfer_EnumHelpers_StringPairList_t< TEnum > stringList = KV3Transfer_EnumHelpers_t< TEnum >::Pairs();
	for ( size_t i = 0; i < stringList.m_nCount; ++i )
	{
		if ( stringList.m_pList[ i ].m_nEnum == nValue )
		{
			return stringList.m_pList[ i ].m_pString;
		}
	}
	AssertMsg1( false, "Unrecognized enumerator value %d", (int)nValue );
	return "";
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
template< typename TEnum >
typename std::enable_if< KV3Transfer_EnumHelpers_t< TEnum >::is_present, bool >::type KV3Transfer_EnumeratorValueFromName( const char *pEnumeratorName, TEnum *pOutValue )
{
	const KV3Transfer_EnumHelpers_StringPairList_t< TEnum > stringList = KV3Transfer_EnumHelpers_t< TEnum >::Pairs();
	for ( size_t i = 0; i < stringList.m_nCount; ++i )
	{
		if ( 0 == V_stricmp_fast( stringList.m_pList[ i ].m_pString, pEnumeratorName ) )
		{
			*pOutValue = stringList.m_pList[ i ].m_nEnum;
			return true;
		}
	}
	
	*pOutValue = KV3Transfer_EnumHelpers_t< TEnum >::ENUM_DEFAULT;
	return false;
}


//--------------------------------------------------------------------------------------------------
// T (Class/Enum)
//--------------------------------------------------------------------------------------------------
template< typename T >
struct CKV3TransferValHelper
{
	//--------------------------------------------------------------------------------------------------
	// Class value (non pointer)
	//--------------------------------------------------------------------------------------------------
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const T &value, typename std::enable_if< std::is_class< U >::value >::type* = 0 )
	{
		pContext->SaveClassPointer( &value, pSaveToValue );
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, T &value, typename std::enable_if< std::is_class< U >::value >::type* = 0 )
	{
		// recurse into the class
		pContext->LoadClassInstance<T>( &value, pLoadFromValue );
	}

	//--------------------------------------------------------------------------------------------------
	// Enum
	//--------------------------------------------------------------------------------------------------
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const T &value, typename std::enable_if< std::is_enum< U >::value >::type* = 0 )
	{
		const char *pEnumeratorName = KV3Transfer_EnumeratorNameFromValue( value );

		if ( pEnumeratorName )
		{
			pSaveToValue->SetValueString( pEnumeratorName );
		}
		else
		{
			// no name for this value - just do an int
			if ( sizeof( T ) == sizeof( uint64 ) )
			{
				pSaveToValue->SetValueInt64( value );
			}
			else
			{
				pSaveToValue->SetValueInt( value );
			}
		}
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, T &value, typename std::enable_if< std::is_enum< U >::value >::type* = 0 )
	{
		if ( pLoadFromValue->GetType() == KEYVALUES3_TYPE_STRING )
		{
			if ( KV3Transfer_EnumeratorValueFromName( pLoadFromValue->GetValueString(), &value ) )
			{
				return;
			}
		}

		// interpret as int
		value = (T)pLoadFromValue->GetValueInt64();
	}

	template< typename U >
	static void LoadDefault( CKV3TransferLoadContext *pContext, const char *pDefault, T &value, typename std::enable_if< std::is_enum< U >::value >::type* = 0 )
	{
		if ( !KV3Transfer_EnumeratorValueFromName( pDefault, &value ) )
		{
			// unknown - use int
			value = (T)V_atoi( pDefault );
		}
	}
};


//--------------------------------------------------------------------------------------------------
// T* (Class Pointer - we assume that it's an owning pointer; cycles and diamonds aren't supported)
//--------------------------------------------------------------------------------------------------
template< typename T >
struct CKV3TransferValHelper< T* >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, T* const &value )
	{
		pContext->SaveClassPointer( value, pSaveToValue );
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, T* &value )
	{
		pContext->LoadOwningPointer( value, pLoadFromValue );
	}
};


//--------------------------------------------------------------------------------------------------
// CSmartPtr is also treated as an owning pointer
//--------------------------------------------------------------------------------------------------
template< class T >
struct CKV3TransferValHelper< CSmartPtr< T > >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const CSmartPtr< T > &value )
	{
		pContext->SaveClassPointer( value.Get(), pSaveToValue );
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, CSmartPtr< T > &value )
	{
		T *pPointer = nullptr;
		pContext->LoadOwningPointer( pPointer, pLoadFromValue );
		value = pPointer;
	}
};


//--------------------------------------------------------------------------------------------------
#define DECLARE_KV3_TRANSFER_HELPER_FOR_SIMPLE_TYPE( type_name, set_func, get_func, default_parser ) \
template<> \
struct CKV3TransferValHelper< type_name > \
{ \
	template< typename U > \
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const type_name &value ) \
	{ \
		pSaveToValue->set_func( value ); \
	} \
	template< typename U > \
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, type_name &value ) \
	{ \
		value = (type_name)pLoadFromValue->get_func(); \
	} \
	template< typename U > \
	static void LoadDefault( CKV3TransferLoadContext *pContext, const char *pDefault, type_name &value ) \
	{ \
		value = (type_name)default_parser( pDefault ); \
	} \
};

inline bool StringToBool( const char *pValue )
{
	if ( pValue == NULL || pValue[0] == '\0' )
		return false;

	if ( !V_stricmp( pValue, "true" ) )
		return true;

	if ( !V_stricmp( pValue, "false" ) )
		return true;

	return V_atoi( pValue ) != 0;
}

DECLARE_KV3_TRANSFER_HELPER_FOR_SIMPLE_TYPE( bool, SetValueBool, GetValueBool, StringToBool );
DECLARE_KV3_TRANSFER_HELPER_FOR_SIMPLE_TYPE( char, SetValueInt, GetValueInt, V_atoi );
DECLARE_KV3_TRANSFER_HELPER_FOR_SIMPLE_TYPE( int8, SetValueInt, GetValueInt, V_atoi );
DECLARE_KV3_TRANSFER_HELPER_FOR_SIMPLE_TYPE( uint8, SetValueInt, GetValueInt, V_atoi );
DECLARE_KV3_TRANSFER_HELPER_FOR_SIMPLE_TYPE( int16, SetValueInt, GetValueInt, V_atoi );
DECLARE_KV3_TRANSFER_HELPER_FOR_SIMPLE_TYPE( uint16, SetValueInt, GetValueInt, V_atoi );
DECLARE_KV3_TRANSFER_HELPER_FOR_SIMPLE_TYPE( int32, SetValueInt, GetValueInt, V_atoi );
DECLARE_KV3_TRANSFER_HELPER_FOR_SIMPLE_TYPE( uint32, SetValueInt64, GetValueInt64, V_atoi64 );
DECLARE_KV3_TRANSFER_HELPER_FOR_SIMPLE_TYPE( int64, SetValueInt64, GetValueInt64, V_atoi64 );
DECLARE_KV3_TRANSFER_HELPER_FOR_SIMPLE_TYPE( uint64, SetValueInt64, GetValueInt64, V_atoi64 );
DECLARE_KV3_TRANSFER_HELPER_FOR_SIMPLE_TYPE( float, SetValueFloat, GetValueFloat, V_atof );
DECLARE_KV3_TRANSFER_HELPER_FOR_SIMPLE_TYPE( double, SetValueDouble, GetValueDouble, V_atod );

//--------------------------------------------------------------------------------------------------
template< typename T >
struct CKV3TransferValHelper< CUtlVector< T > >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const CUtlVector< T > &value )
	{
		int nCount = value.Count();

		pSaveToValue->SetArrayElementCount( nCount );

		for ( int i = 0; i < nCount; ++i )
		{
			pContext->SaveValueDirect( value[ i ], pSaveToValue->GetArrayElement( i ) );
		}
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, CUtlVector< T > &value )
	{
		int nCount = pLoadFromValue->GetArrayElementCount();
		value.SetCount( nCount );

		for ( int i = 0; i < nCount; ++i )
		{
			pContext->LoadValueDirect( value[ i ], pLoadFromValue->GetArrayElement( i ) );
		}
	}
};

//--------------------------------------------------------------------------------------------------
template<>
struct CKV3TransferValHelper< CUtlBinaryBlock >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const CUtlBinaryBlock &value )
	{
		pSaveToValue->SetToBinaryBlob( reinterpret_cast<const byte*>(value.Get()), value.Length() );
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, CUtlBinaryBlock &value )
	{
		value.Set( pLoadFromValue->GetBinaryBlobBase(), pLoadFromValue->GetBinaryBlobSize() );
	}
};


//--------------------------------------------------------------------------------------------------
template<>
struct CKV3TransferValHelper< KeyValues3 >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const KeyValues3 &value )
	{
		pSaveToValue->CopyFrom( &value );
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, KeyValues3 &value )
	{
		value.CopyFrom( pLoadFromValue );
	}
};


//--------------------------------------------------------------------------------------------------
template< typename T, size_t N >
struct CKV3TransferValHelper< CUtlVectorFixedGrowable< T, N > >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const CUtlVectorFixedGrowable< T, N > &value )
	{
		int nCount = value.Count();

		pSaveToValue->SetArrayElementCount( nCount );

		for ( int i = 0; i < nCount; ++i )
		{
			pContext->SaveValueDirect( value[ i ], pSaveToValue->GetArrayElement( i ) );
		}
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, CUtlVectorFixedGrowable< T, N > &value )
	{
		int nCount = pLoadFromValue->GetArrayElementCount();
		value.SetCount( nCount );

		for ( int i = 0; i < nCount; ++i )
		{
			pContext->LoadValueDirect( value[ i ], pLoadFromValue->GetArrayElement( i ) );
		}
	}
};


//--------------------------------------------------------------------------------------------------
template<>
struct CKV3TransferValHelper< CUtlString >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const CUtlString &value )
	{
		pSaveToValue->SetValueString( value.Get() );
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, CUtlString &value )
	{
		pLoadFromValue->GetValueAsString( &value );
	}

	template< typename U >
	static void LoadDefault( CKV3TransferLoadContext *pContext, const char *pDefault, CUtlString &value )
	{
		value = pDefault;
	}
};


//--------------------------------------------------------------------------------------------------
template<>
struct CKV3TransferValHelper< CUtlSymbolLarge >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const CUtlSymbolLarge &value )
	{
		pSaveToValue->SetValueString( value.String() );
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, CUtlSymbolLarge &value )
	{
		IKV3TransferInterface_UtlSymbolLarge *pUtlSymbolLargeInterface = pContext->FindInterface< IKV3TransferInterface_UtlSymbolLarge >();
		AssertMsg( pUtlSymbolLargeInterface != NULL, "To load a CUtlSymbolLarge you must specify a symbol table via LoadContext.AddInterface( CKV3Transfer_UtlSymbolLargeInterface )" );

		value = pUtlSymbolLargeInterface->AddString( pLoadFromValue->GetValueString() );
	}

	template< typename U >
	static void LoadDefault( CKV3TransferLoadContext *pContext, const char *pDefault, CUtlSymbolLarge &value )
	{
		IKV3TransferInterface_UtlSymbolLarge *pUtlSymbolLargeInterface = pContext->FindInterface< IKV3TransferInterface_UtlSymbolLarge >();
		AssertMsg( pUtlSymbolLargeInterface != NULL, "To load a CUtlSymbolLarge you must specify a symbol table via LoadContext.AddInterface( CKV3Transfer_UtlSymbolLargeInterface )" );

		value = pUtlSymbolLargeInterface->AddString( pDefault );
	}
};


//--------------------------------------------------------------------------------------------------
template< typename T, int TFixedArrayCount >
struct CKV3TransferValHelper< T[ TFixedArrayCount ] >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, T const value[] )
	{
		pSaveToValue->SetArrayElementCount( TFixedArrayCount );

		for ( int i = 0; i < TFixedArrayCount; ++i )
		{
			pContext->SaveValueDirect( value[ i ], pSaveToValue->GetArrayElement( i ) );
		}
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, T value[] )
	{
		int nCount = pLoadFromValue->GetArrayElementCount();

		int i = 0;
		for ( ; i < nCount && i < TFixedArrayCount; ++i )
		{
			pContext->LoadValueDirect( value[ i ], pLoadFromValue->GetArrayElement( i ) );
		}

		// fill in any remaining entries with null values
		for ( ; i < TFixedArrayCount; ++i )
		{
			KeyValues3 nullValue;
			pContext->LoadValueDirect( value[ i ], &nullValue );
		}
	}
};


//--------------------------------------------------------------------------------------------------
// byte[] prefers to serialize as binary blob
//--------------------------------------------------------------------------------------------------
template< int TFixedArrayCount >
struct CKV3TransferValHelper< byte const[ TFixedArrayCount ] >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, byte const value[] )
	{
		pSaveToValue->SetToBinaryBlob( value, TFixedArrayCount );
	}
};

template< int TFixedArrayCount >
struct CKV3TransferValHelper< byte[ TFixedArrayCount ] >
{
	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, byte value[] )
	{
		if ( pLoadFromValue->GetType() == KEYVALUES3_TYPE_BINARY_BLOB )
		{
			int nCount = MIN( pLoadFromValue->GetBinaryBlobSize(), TFixedArrayCount );
			V_memcpy( value, pLoadFromValue->GetBinaryBlobBase(), nCount );
		}
		else
		{
			int nCount = pLoadFromValue->GetArrayElementCount();

			int i = 0;
			for ( ; i < nCount && i < TFixedArrayCount; ++i )
			{
				pContext->LoadValueDirect( value[ i ], pLoadFromValue->GetArrayElement( i ) );
			}

			// fill in any remaining entries with null values
			for ( ; i < TFixedArrayCount; ++i )
			{
				KeyValues3 nullValue;
				pContext->LoadValueDirect( value[ i ], &nullValue );
			}
		}
	}

	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, byte const value[] )
	{
		pSaveToValue->SetToBinaryBlob( value, TFixedArrayCount );
	}

	template< typename U >
	static void LoadDefault( CKV3TransferLoadContext *pContext, const char *pDefault, byte value[] )
	{
		TEMPLATE_USAGE_INVALID( U );
	}
};


//--------------------------------------------------------------------------------------------------
// this specialization had to be split up into two versions, since it's not templatized on the type,
// so it can't match both const char[] and char[], unlike the above T[], which matches both
template< int TFixedArrayCount >
struct CKV3TransferValHelper< const char[ TFixedArrayCount ] >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const char value[] )
	{
		pSaveToValue->SetValueString( value );
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, char value[] )
	{
		TEMPLATE_USAGE_INVALID( U );
	}

	template< typename U >
	static void LoadDefault( CKV3TransferLoadContext *pContext, const char *pDefault, char value[] )
	{
		TEMPLATE_USAGE_INVALID( U );
	}
};

template< int TFixedArrayCount >
struct CKV3TransferValHelper< char[ TFixedArrayCount ] >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const char value[] )
	{
		pSaveToValue->SetValueString( value );
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, char value[] )
	{
		pLoadFromValue->GetValueAsString( value, TFixedArrayCount );
	}

	template< typename U >
	static void LoadDefault( CKV3TransferLoadContext *pContext, const char *pDefault, char value[] )
	{
		V_strncpy( value, pDefault, TFixedArrayCount );
	}
};


//--------------------------------------------------------------------------------------------------
template< typename T, int TFloatCount, bool TIgnoreSizeMismatch = false >
struct CKV3TransferValHelperAsFloats
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const T &value )
	{
		COMPILE_TIME_ASSERT( ( sizeof(T) == sizeof(float) * TFloatCount ) || TIgnoreSizeMismatch );
		const float *pFloats = reinterpret_cast< const float* >( &value );

		pSaveToValue->SetArrayElementCount( TFloatCount );
		for ( int i = 0; i < TFloatCount; ++i )
		{
			pSaveToValue->GetArrayElement( i )->SetValueFloat( pFloats[i] );
		}
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, T &value )
	{
		float *pFloats = reinterpret_cast< float* >( &value );

		int nArrayCount = pLoadFromValue->GetArrayElementCount();
		int i = 0;
		for ( ; i < TFloatCount && i < nArrayCount; ++i )
		{
			pFloats[i] = pLoadFromValue->GetArrayElement( i )->GetValueFloat();
		}

		for ( ; i < TFloatCount; ++i )
		{
			pFloats[i] = 0; // insufficient values - fill the rest of the values with zeroes
		}
	}

	template< typename U >
	static void LoadDefault( CKV3TransferLoadContext *pContext, const char *pDefault, T &value )
	{
		float *pFloats = reinterpret_cast< float* >( &value );

		char pTemp[64];
		const char *pScan = pDefault;
		for ( int iNum = 0; iNum < TFloatCount; ++iNum )
		{
			int iChar = 0;
			while ( *pScan && !V_isspace( *pScan ) )
			{
				pTemp[ iChar ] = *pScan;
				iChar++;
				pScan++;
			}
			while ( *pScan && V_isspace( *pScan ) )
			{
				pScan++;
			}
			pTemp[ iChar ] = '\0';

			pFloats[ iNum ] = V_atof( pTemp );
		}
	}
};

//--------------------------------------------------------------------------------------------------
template<> struct CKV3TransferValHelper< Vector2D >: public CKV3TransferValHelperAsFloats< Vector2D, 2 > {};
template<> struct CKV3TransferValHelper< Vector >: public CKV3TransferValHelperAsFloats< Vector, 3 > {};
template<> struct CKV3TransferValHelper< VectorAligned >: public CKV3TransferValHelperAsFloats< VectorAligned, 3, true > {};
template<> struct CKV3TransferValHelper< Vector4D >: public CKV3TransferValHelperAsFloats< Vector4D, 4 > {};

template<> struct CKV3TransferValHelper< QAngle >: public CKV3TransferValHelperAsFloats< QAngle, 3 > {};
template<> struct CKV3TransferValHelper< RadianEuler >: public CKV3TransferValHelperAsFloats< RadianEuler, 3 > {};
template<> struct CKV3TransferValHelper< DegreeEuler >: public CKV3TransferValHelperAsFloats< DegreeEuler, 3 > {};
template<> struct CKV3TransferValHelper< Quaternion >: public CKV3TransferValHelperAsFloats< Quaternion, 4 > {};

template<> struct CKV3TransferValHelper< CTransform >: public CKV3TransferValHelperAsFloats< CTransform, 8 > {};

template<> struct CKV3TransferValHelper< matrix3x4_t >: public CKV3TransferValHelperAsFloats< matrix3x4_t, 12 > {};
template<> struct CKV3TransferValHelper< matrix3x4a_t >: public CKV3TransferValHelperAsFloats< matrix3x4a_t, 12 > {};

#ifndef CSTRIKE15 // $$$REI No VMatrix in CSGO
template<> struct CKV3TransferValHelper< VMatrix >: public CKV3TransferValHelperAsFloats< VMatrix, 16 > {};
#endif

//--------------------------------------------------------------------------------------------------
template<>
struct CKV3TransferValHelper< fltx4 >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const fltx4 &value )
	{
		TEMPLATE_USAGE_INVALID( U );
		AssertMsg( false, "No support for saving fltx4" );
		pSaveToValue->SetValueFloat( 0 );
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, fltx4 &value )
	{
		value = ReplicateX4( pLoadFromValue->GetValueFloat() );
	}

	template< typename U >
	static void LoadDefault( CKV3TransferLoadContext *pContext, const char *pDefault, fltx4 &value )
	{
		value = ReplicateX4( V_atof( pDefault ) );
	}
};


//--------------------------------------------------------------------------------------------------
template< typename T, int TUInt8Count >
struct CKV3TransferValHelperAsUint8s
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const T &value )
	{
		COMPILE_TIME_ASSERT( sizeof(T) == sizeof(uint8) * TUInt8Count );
		const uint8 *pUInt8s = reinterpret_cast< const uint8* >( &value );

		pSaveToValue->SetArrayElementCount( TUInt8Count );
		for ( int i = 0; i < TUInt8Count; ++i )
		{
			pSaveToValue->GetArrayElement( i )->SetValueInt( pUInt8s[i] );
		}
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, T &value )
	{
		uint8 *pUInt8s = reinterpret_cast< uint8* >( &value );

		int nArrayCount = pLoadFromValue->GetArrayElementCount();
		int i = 0;
		for ( ; i < TUInt8Count && i < nArrayCount; ++i )
		{
			pUInt8s[i] = (uint8)pLoadFromValue->GetArrayElement( i )->GetValueInt();
		}

		for ( ; i < TUInt8Count; ++i )
		{
			pUInt8s[i] = 0; // insufficient values - fill the rest of the values with zeroes
		}
	}

	template< typename U >
	static void LoadDefault( CKV3TransferLoadContext *pContext, const char *pDefault, T &value )
	{
		uint8 *pUInt8s = reinterpret_cast< uint8* >( &value );

		char pTemp[64];
		const char *pScan = pDefault;
		for ( int iNum = 0; iNum < TUInt8Count; ++iNum )
		{
			int iChar = 0;
			while ( *pScan && !V_isspace( *pScan ) )
			{
				pTemp[ iChar ] = *pScan;
				iChar++;
				pScan++;
			}
			while ( *pScan && V_isspace( *pScan ) )
			{
				pScan++;
			}
			pTemp[ iChar ] = '\0';

			pUInt8s[ iNum ] = (uint8)V_atoi( pTemp );
		}
	}
};

//--------------------------------------------------------------------------------------------------
template<> struct CKV3TransferValHelper< Color >: public CKV3TransferValHelperAsUint8s< Color, 4 > {};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
template< typename T >
void CKV3TransferSaveContext::SaveValueDirect( const T &sourceValue, KeyValues3 *pSaveToMember )
{
	CKV3TransferValHelper< T >::template SaveValue< T >( this, pSaveToMember, sourceValue );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
template< typename T >
void CKV3TransferLoadContext::LoadValueDirect( T &destValue, const KeyValues3 *pLoadFromMember )
{
	CKV3TransferValHelper< T >::template LoadValue< T >( this, pLoadFromMember, destValue );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
template< typename T >
void CKV3TransferLoadContext::LoadDefaultDirect( T &destValue, const char *pDefaultValue )
{
	CKV3TransferValHelper< T >::template LoadDefault< T >( this, pDefaultValue, destValue );
}


//--------------------------------------------------------------------------------------------------
// Loads/creates an object of type T. Caller takes ownership and should just delete the returned object.
//--------------------------------------------------------------------------------------------------
template< typename T >
inline T *LoadKV3Object( CUtlString *pOutError, KeyValues3 *pKV3 )
{
	T *pData = NULL;
	CKV3TransferLoadContext loadArchive;
	loadArchive.LoadOwningPointer( pData, pKV3 );
	if ( loadArchive.GetResult() == KV3TRANSFER_FAIL )
	{
		Assert( pData == nullptr );
		Msg( "CKV3TransferLoadContext::LoadOwningPointer error: %s\n", loadArchive.GetErrorMessage() );
		if ( pOutError )
		{
			pOutError->Set( loadArchive.GetErrorMessage() );
		}
		return nullptr;
	}

	return pData;
}


//--------------------------------------------------------------------------------------------------
// Loads/creates an object of type T. Caller takes ownership and should just delete the returned object.
//--------------------------------------------------------------------------------------------------
template< typename T >
inline T *LoadKV3Object( CUtlString *pOutError, CUtlBuffer *pSrcBuffer )
{
	CKeyValues3Context diskKV3Data;
	if ( !LoadKV3( &diskKV3Data, pOutError, pSrcBuffer ) )
		return nullptr;

	T *pData = NULL;

	pData = LoadKV3Object< T >( pOutError, diskKV3Data.Root() );

	return pData;
}


//--------------------------------------------------------------------------------------------------
// Loads/creates an object of type T. Caller takes ownership and should just delete the returned object.
//--------------------------------------------------------------------------------------------------
template< typename T >
inline T *LoadKV3ObjectFromFile( CUtlString *pOutErrorMessage, const char *pFilename, const char *pPath = NULL )
{
	CUtlBuffer buf( 0, 0, 0 );
	if ( !g_pFullFileSystem->ReadFile( pFilename, pPath, buf ) )
	{
		if ( pOutErrorMessage )
		{
			pOutErrorMessage->Format( "Failed to read file '%s'", pFilename );
		}
		return nullptr;
	}

	return LoadKV3Object< T >( pOutErrorMessage, &buf );
}


//--------------------------------------------------------------------------------------------------
// Loads/creates an object of type T. Caller takes ownership and should just delete the returned object.
//--------------------------------------------------------------------------------------------------
template< typename T >
inline bool SaveKV3Object( const KV3ID_t &encodingId, const KV3ID_t &formatId, const T *pData, CUtlString *pOutError, CUtlBuffer *pDestBuffer )
{
	CKeyValues3Context kv3Data;
	CKV3TransferSaveContext saveCtx;
	saveCtx.SaveClassPointer( pData, kv3Data.Root() );

	if ( saveCtx.GetResult() != KV3TRANSFER_SUCCESS )
	{
		pOutError->Format( "KV3 save transfer failed: %s", saveCtx.GetErrorMessage() );
		return false;
	}

	if ( !SaveKV3( encodingId, formatId, kv3Data.Root(), pOutError, pDestBuffer ) )
		return false;

	return true;
}


//--------------------------------------------------------------------------------------------------
// Loads/creates an object of type T. Caller takes ownership and should just delete the returned object.
//--------------------------------------------------------------------------------------------------
template< typename T >
inline bool SaveKV3ObjectToFile( const KV3ID_t &encodingId, const KV3ID_t &formatId, const T *pData, CUtlString *pOutErrorMessage, const char *pFilename, const char *pPath = NULL )
{
	CUtlBuffer buf( 0, 0, encodingId == KV3_ENCODING_TEXT ? CUtlBuffer::TEXT_BUFFER : 0 );
	if ( !SaveKV3Object( encodingId, formatId, pData, pOutErrorMessage, &buf ) )
		return false;

	if ( !g_pFullFileSystem->WriteFile( pFilename, pPath, buf ) )
	{
		if ( pOutErrorMessage )
		{
			pOutErrorMessage->Format( "Unable to write file '%s'", pFilename );
		}
		return false;
	}

	return true;
}

