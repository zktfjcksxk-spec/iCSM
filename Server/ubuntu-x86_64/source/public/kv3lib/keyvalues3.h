//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
// KeyValues3 is a runtime representation and set of disk encodings for generic structured data similar to KeyValues or JSON.
// 
// (NOTE: KeyValues3 is something of a misnomer, since a single KeyValues3 object is actually a value with no associated "key".)
// 
// Compared with KeyValues, KV3 has these advantages:
// * Better support for structured data (first-class arrays, binary blobs)
// * Better error reporting when parsing (line numbers)
// * Better-specified text format (with support for multi-line comments and multi-line string literals)
// * Runtime representation is often leaner and faster
// * Opt-in support for pool allocations and key symbol table (CKeyValues3Context)
// * Opt-in compressed binary serialization path (much faster loading, and smaller files)
// * Support for datamodel-style upconversion transformations via format UUIDs (See kv3formats.h)
//
// Compared with KeyValues, KV3 has these disadvantages:
// * No support for some advanced features like #include, #base, or conditional values
//
// (Historical note: KeyValues is the original text format used by the Source engine, and KeyValues2 was a text encoding used by datamodel.)
//==================================================================================================

//==================================================================================================
// A Digression on the Root Key
//==================================================================================================
//
// One notable difference between KeyValues and KV3 is that an instance of the KeyValues class is a (Key+Value) pair,
// whereas an instance of the KeyValues3 class is an unnamed value. (In a vaccum KeyValues3 might be better named 'Variant')
//
// This property of KV1 is the cause for the 'meaningless' root key at the start of most KV1 files:
//		"root_key_name"
//		{
//			"first"
//			{
//				"attr1"	"0.5"
//				"attr2"	"potato"
//			}
//			"second"
//			{
//				"attr1"	"2.5"
//				"attr2"	"tomato"
//			}
//		}
//
// And the non-optional string in the constructor for the class:
//		void Foo()
//		{
//			KeyValues someKeyValues( "root_key_name" );
//			someKeyValues.SetInt( "a", 1 )
//		}
//
// This is in contrast with the KV3 equivalents:
//		<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
//		{
//			list =
//			[
//				{
//					name = "first"
//					attr1 = 0.5
//					attr2 = "potato"
//				},
//				{
//					name = "second"
//					attr1 = 2.5
//					attr2 = "tomato"
//				},
//			]
//		}
//
// And:
//		void Foo()
//		{
//			KeyValues3 someKeyValues; // No name
//			someKeyValues.SetMemberInt( "a", 1 )
//		}
//
// Note that in KV3 there is no equivalent to "root_key_name" because the root is an unnamed variant.
// (Although the 'list' member shows up instead, since the root of a KV3 file is conventionally a table.)
//
// In general KV3 only introduces names (keys) once you have a value of type table (hence the 'member' terminology),
// and those names are owned by the table rather than properties of the values in that table. This makes
// the size of an anonymous KV3 value smaller, and allows member names to be stored in one place (making lookups faster.)
//==================================================================================================
#pragma once


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#include "tier1/utlstringtoken.h"
#include "tier1/utlsymbollarge.h"

#include "kv3lib/kv3formats.h"


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class KeyValues3;
class CKeyValues3Context;
class CKeyValues3Table;
class CKeyValues3Cluster;
class CKeyValues3Metadata;
struct KeyValues3Array_t;
struct KeyValues3BinaryBlob_t;
struct KeyValues3BinaryBlobExternal_t;
class CUtlString;
class CUtlBuffer;

class Vector;
class Vector2D;
class Vector4D;
class QAngle;
class Quaternion;

//--------------------------------------------------------------------------------------------------
// For lookup speed, KV3 usually uses the hash of table member names. We use the same algorithm
// as CUtlStringToken for convenience/interoperability, but try and use KeyValues3LowercaseHash_t
// to explicitly identify that we're in KV3 land.
//--------------------------------------------------------------------------------------------------
typedef CUtlStringToken KeyValues3LowercaseHash_t;

inline uint32 KeyValues3LowercaseHashToInt( KeyValues3LowercaseHash_t n ) { return n.m_nHashCode; }
inline KeyValues3LowercaseHash_t KV3MakeLowerHash( const char *pStr ) { return MakeStringToken( pStr ); }

#define KEYVALUES3_LOWERCASE_HASH_INVALID		CUtlStringToken()

#define KV3_DEBUG_CHECK_FOR_HASH_COLLISIONS		0 // perform slow checks for kv3 hash collisions


//--------------------------------------------------------------------------------------------------
// Utility class which stores a pointer to a string and the corresponding token. This is 
// specifically designed to work with literal values, the goal is to allow a function to be called
// with a single literal value but result in both the pointer to the value and the token being 
// generated.
//
// Note that the constructor is templatized on the length of the literal string value, this is to 
// ensure the CUtlStringToken literal constructors which get compiled int a constant will be invoked
//--------------------------------------------------------------------------------------------------
class CKV3MemberName
{
public:
	CKV3MemberName()
		: m_Hash()
		, m_pString( nullptr )
	{}

	template< int T_LENGTH > FORCEINLINE CKV3MemberName( const char ( &str )[ T_LENGTH ] )
		: m_Hash( str )
		, m_pString( str )
	{}

	explicit FORCEINLINE CKV3MemberName( const char *pString )
		: m_Hash( KV3MakeLowerHash( pString ) )
		, m_pString( pString )
	{
	}

	FORCEINLINE CKV3MemberName( const char *pString, KeyValues3LowercaseHash_t nHash )
		: m_Hash( nHash )
		, m_pString( pString )
	{
	}

	CKV3MemberName( const CKV3MemberName &rhs )
		: m_Hash( rhs.m_Hash )
		, m_pString( rhs.m_pString )
	{
	}
	
	CKV3MemberName &operator=( const CKV3MemberName &rhs )
	{
		m_Hash = rhs.m_Hash;
		m_pString = rhs.m_pString;
		return *this;
	}

	bool operator==( const CKV3MemberName &rhs ) const
	{
		if ( m_Hash == rhs.m_Hash )
		{
			if ( KV3_DEBUG_CHECK_FOR_HASH_COLLISIONS )
			{
				AssertDbg( !V_stricmp_fast( m_pString, rhs.m_pString ) );
			}
			return true;
		}

		return false;		
	}

	bool operator!=( const CKV3MemberName &rhs ) const
	{
		return !( *this == rhs );
	}
	
	KeyValues3LowercaseHash_t m_Hash;
	const char * m_pString;
};


//--------------------------------------------------------------------------------------------------
// Public-facing types that a KV3 can store.
// (See KeyValues3InternalType_t for actual storage details.)
//--------------------------------------------------------------------------------------------------
enum KeyValues3Type_t: uint8
{
	KEYVALUES3_TYPE_INVALID,

	KEYVALUES3_TYPE_NULL,			// no value

	KEYVALUES3_TYPE_BOOL,
	KEYVALUES3_TYPE_INT64,
	KEYVALUES3_TYPE_UINT64,
	KEYVALUES3_TYPE_DOUBLE,

	KEYVALUES3_TYPE_STRING,			// utf8
	KEYVALUES3_TYPE_BINARY_BLOB,	// raw bytes

	KEYVALUES3_TYPE_ARRAY,			// Ordered list of KV3 values
	KEYVALUES3_TYPE_TABLE,			// Ordered list of (string,KV3) members

	KEYVALUES3_TYPE_MAX,
};


//--------------------------------------------------------------------------------------------------
// Internal implementation details
//--------------------------------------------------------------------------------------------------
enum KeyValues3InternalType_t: uint8;
const int KEYVALUES3_BITS_FOR_TYPE = 4;
#define KEYVALUES3_SHORT_STRING_LENGTH 7 // save 1 for \0


//--------------------------------------------------------------------------------------------------
// KeyValues3 can have flags associated with the value (generally considered as additional type annotations)
// These are considered part of the data and get serialized / copied / etc.
//--------------------------------------------------------------------------------------------------
enum KeyValues3Flag_t: uint16
{
	KEYVALUES3_FLAG_NONE						= 0,

	// *****************************************************************************************************
	// NOTE: THE VALUES OF THESE FLAGS ARE SERIALIZED IN THE BINARY ENCODING, DO NOT HAPHAZARDLY CHANGE THEM
	// *****************************************************************************************************
	KEYVALUES3_FLAG_RESOURCE_REFERENCE			= 1<<0,
	// KEYVALUES3_FLAG_UNUSED = 1<<1,
	KEYVALUES3_FLAG_MULTILINE_STRING			= 1<<2, // indicates the text serialization should use multiline """ syntax

	KEYVALUES3_FLAG_LAST_VALUE					= KEYVALUES3_FLAG_MULTILINE_STRING,
};

const int KEYVALUES3_BITS_FOR_FLAGS = 16; // "16 bits should be enough for anyone." - Bill Gates
COMPILE_TIME_ASSERT( ( 1<<(KEYVALUES3_BITS_FOR_FLAGS-1) ) >= KEYVALUES3_FLAG_LAST_VALUE );


//--------------------------------------------------------------------------------------------------
// Helper to iterate over every key (recursively) in a KV3 tree
//--------------------------------------------------------------------------------------------------
class CKeyValues3Iterator 
{
public:
	CKeyValues3Iterator();
	CKeyValues3Iterator( KeyValues3 *pKV3 );

	void Init( KeyValues3 *pKV3 );
	bool IsValid() const;
	void Advance();

	KeyValues3 *Get() const;
	KeyValues3 *operator->() const { return Get(); }
	
private:
	struct StackEntry_t
	{
		KeyValues3 *m_pKV;
		int m_nIndex; // -1 means 'self' and not any child
	};
	CUtlVectorFixedGrowable< StackEntry_t, 4 > m_Stack;
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class KeyValues3
{
public:
	//--------------------------------------------------------------------------------------------------
	KeyValues3();
	KeyValues3( const KeyValues3 &rhs );
	~KeyValues3();

	KeyValues3 &operator=( const KeyValues3 &rhs );


	//--------------------------------------------------------------------------------------------------
	// General interface
	//--------------------------------------------------------------------------------------------------
	KeyValues3Type_t GetType() const;

	void			MoveFrom( KeyValues3 *pOther ); // other turns into null
	void			CopyFrom( const KeyValues3 *pOther ); // leaves other intact
	void			CopyMatchingKeysFrom( const KeyValues3 *pOther ); // only copies keys that already exist in this table, leaves other keys in this table untouched (recursive tables are completely stomped)
	bool			IsIdenticalTo( const KeyValues3 *pOther, bool bAssertOnFailure ) const; // identical order tables/array; doubles can differ by at most KEYVALUES3_IDENTICAL_EPSILON

	void			EnsureTypeIs( KeyValues3Type_t nType );
	void			EnsureIsArray( int nCount, KeyValues3Type_t nType ); // must be an array of N values of the specified type
	void			EnsureIsAnyArray( int nCount ); // doesn't matter what it's an array of, just make sure it has the right number of elements

	bool			IsNull() const;
	void			SetToNull();

	// If this KV3 lives in a context, access it. May return null
	CKeyValues3Context *GetParentContext() const;


	//--------------------------------------------------------------------------------------------------
	// Array interface
	//--------------------------------------------------------------------------------------------------
	void			SetToEmptyArray();
	
	int				GetArrayLength() const { return GetArrayElementCount(); }
	int				GetArrayElementCount() const;
	
	void			SetArrayElementCount( int nCount );

	const KeyValues3 *GetArrayElement( int nIndex ) const;
	KeyValues3 *	GetArrayElement( int nIndex );

	void			ArraySwapItems( int nIndex1, int nIndex2 );
	void			ArrayInsertMultipleBefore( int nIndexToInsertBefore, int nCount );
	void			ArrayRemoveMultiple( int nFirstIndexToRemove, int nCount );
	KeyValues3 *	ArrayAddToTail();

	//--------------------------------------------------------------------------------------------------
	// Common 'bunch of floats' (vector, matrix, etc.) types are all stored in KV3 as arrays of floats.
	// If you try and use a Get* that doesn't match the current value, it will return false and fill the remaining target floats with zeros
	// (ie. if you cll GetVector4D on [5,7] it will set your Vector4D to <5,7,0,0>)
	//--------------------------------------------------------------------------------------------------
	void			SetValueFloatArray( int nFloats, const float *pValues );
	bool			GetValueFloatArray( int nFloats, float *pOutValues ) const;


	//--------------------------------------------------------------------------------------------------
	// Value interface
	//--------------------------------------------------------------------------------------------------
	bool			GetValueBool() const;
	int				GetValueInt() const;
	int64			GetValueInt64() const;
	uint64			GetValueUint64() const;
	float			GetValueFloat() const;
	double			GetValueDouble() const;
	const char *	GetValueString( const char *pDefaultValue = "" ) const; // returns pDefaultValue if not KEYVALUES3_TYPE_STRING
	void			GetValueAsString( char *pOutData, int nBufSize ) const; // coerces to string for simple types
	void			GetValueAsString( CUtlString *pOutString ) const;
	bool			GetValueVector( Vector *pOutValue ) const										{ return GetValueFloatArray( 3, (float*)pOutValue ); }
	bool			GetValueVector2D( Vector2D *pOutValue ) const									{ return GetValueFloatArray( 2, (float*)pOutValue ); }
	bool			GetValueVector4D( Vector4D *pOutValue ) const									{ return GetValueFloatArray( 4, (float*)pOutValue ); }
	bool			GetValueQAngle( QAngle *pOutValue ) const										{ return GetValueFloatArray( 3, (float*)pOutValue ); }
	bool			GetValueQuaternion( Quaternion *pOutValue ) const								{ return GetValueFloatArray( 4, (float*)pOutValue ); }
	bool			GetValueMatrix( matrix3x4_t *pOutValue ) const									{ return GetValueFloatArray( 12, (float*)pOutValue ); }

	void			SetValueBool( bool bValue );
	void			SetValueString( const char *value );
	void			SetValueResourceString( const char *value );
	void			SetValueInt( int value );
	void			SetValueInt64( int64 value );
	void			SetValueUint64( uint64 value );
	void			SetValueFloat( double value ); // backing store is a double - (does the conversion at the call-site)
	void			SetValueDouble( double value );
	void			SetValueVector( const Vector &value )											{ SetValueFloatArray( 3, (float*)&value ); }
	void			SetValueVector2D( const Vector2D &value )										{ SetValueFloatArray( 2, (float*)&value ); }
	void			SetValueVector4D( const Vector4D &value )										{ SetValueFloatArray( 4, (float*)&value ); }
	void			SetValueQAngle( const QAngle &value )											{ SetValueFloatArray( 3, (float*)&value ); }
	void			SetValueQuaternion( const Quaternion &value )									{ SetValueFloatArray( 4, (float*)&value ); }
	void			SetValueMatrix( const matrix3x4_t &value )										{ SetValueFloatArray( 12, (float*)&value ); }

	template< class T > T GetValueAsNumeric() const
	{
		KeyValues3Type_t nType = GetType();
		if ( nType == KEYVALUES3_TYPE_BOOL )
		{
			return (T)m_bAsBool;
		}
		else if ( nType == KEYVALUES3_TYPE_INT64 )
		{
			return (T)m_nAsInt64;
		}
		else if ( nType == KEYVALUES3_TYPE_UINT64 )
		{
			return (T)m_nAsUint64;
		}
		else if ( nType == KEYVALUES3_TYPE_DOUBLE )
		{
			return (T)m_flAsDouble;
		}
		else if ( nType == KEYVALUES3_TYPE_STRING )
		{
			// TODO
			return (T)V_atod( GetValueString() );
		}
		else if ( nType == KEYVALUES3_TYPE_NULL )
		{
			return 0;
		}
		else
		{
			AssertMsg1( false, "GetAsNumeric() - unsupported type '%d'!", nType );
			return 0;
		}
	}


	//--------------------------------------------------------------------------------------------------
	// Fancy string interface
	//--------------------------------------------------------------------------------------------------
	// Try and convert the string to a value of the current type
	// Doesn't change the type of this kv3
	// (parse / string-split / atoi / atoi64 / atoui64 as appropriate)
	void			ParseValueFromString( const char *pString );
	bool			EqualsValueFromString( const char *pString ); // whether we're already the value you'd get by calling ParseValueFromString()
	
	// Sets type to one of the int types
	void			SetIntFromString( const char *pString );


	//--------------------------------------------------------------------------------------------------
	// Table interface
	//--------------------------------------------------------------------------------------------------
	void			SetToEmptyTable();

	// GetTable is only non-NULL if KEYVALUES3_TYPE_TABLE
	CKeyValues3Table *GetTable();
	const CKeyValues3Table *GetTable() const;

	KeyValues3 *	FindMember( CKV3MemberName memberName );
	const KeyValues3 *FindMember( CKV3MemberName memberName ) const { return const_cast<KeyValues3*>(this)->FindMember( memberName ); }
	KeyValues3 *	FindOrCreateMember( CKV3MemberName memberName, bool *pOutCreated = nullptr );

	int				GetMemberCount() const;
	KeyValues3 *	GetMember( int nIndex );
	const KeyValues3 *GetMember( int nIndex ) const;
	const char *	GetMemberName( int nIndex ) const;
	CKV3MemberName	GetKV3MemberName( int nIndex ) const;
	void			GetMemberNameEscaped( int nIndex, CUtlString *pOutName ) const; // if necessary, adds quotes and escape sequences

	void			SetMemberToCopyOfValue( CKV3MemberName memberName, KeyValues3 *pValue );

	// RemoveMember returns true on success
	bool			RemoveMember( int nIndex );
	bool			RemoveMember( CKV3MemberName memberName );
	bool			RemoveMember( KeyValues3 *pMember );


	//--------------------------------------------------------------------------------------------------
	// Table (Member) Accessors
	//--------------------------------------------------------------------------------------------------
	int				GetMemberInt( CKV3MemberName memberName, int defaultValue = 0 ) const;
	int64			GetMemberInt64( CKV3MemberName memberName, int64 defaultValue = 0 ) const;
	uint64			GetMemberUint64( CKV3MemberName memberName, uint64 defaultValue = 0 ) const;
	float			GetMemberFloat( CKV3MemberName memberName, float defaultValue = 0.0f ) const;
	double			GetMemberDouble( CKV3MemberName memberName, double defaultValue = 0.0 ) const;
	void			GetMemberAsString( CKV3MemberName memberName, char *pOutData, int nBufSize, const char *defaultValue = "" ) const;
	void			GetMemberAsString( CKV3MemberName memberName, CUtlString *pOutData, const char *defaultValue = "" ) const;
	const char		*GetMemberString( CKV3MemberName memberName, const char *defaultValue = "" ) const;
	bool			GetMemberBool( CKV3MemberName memberName, bool defaultValue = false ) const;
	bool			GetMemberFloatArray( CKV3MemberName memberName, int nFloats, float *pOutValues ) const;
	bool			GetMemberVector( CKV3MemberName memberName, Vector *pOutValue ) const			{ return GetMemberFloatArray( memberName, 3, (float*)pOutValue ); }
	bool			GetMemberVector2D( CKV3MemberName memberName, Vector2D *pOutValue ) const		{ return GetMemberFloatArray( memberName, 2, (float*)pOutValue ); }
	bool			GetMemberVector4D( CKV3MemberName memberName, Vector4D *pOutValue ) const		{ return GetMemberFloatArray( memberName, 4, (float*)pOutValue ); }
	bool			GetMemberQAngle( CKV3MemberName memberName, QAngle *pOutValue ) const			{ return GetMemberFloatArray( memberName, 3, (float*)pOutValue ); }
	bool			GetMemberQuaternion( CKV3MemberName memberName, Quaternion *pOutValue ) const	{ return GetMemberFloatArray( memberName, 4, (float*)pOutValue ); }
	bool			GetMemberMatrix( CKV3MemberName memberName, matrix3x4_t *pOutValue ) const		{ return GetMemberFloatArray( memberName, 12, (float*)pOutValue ); }
	
	KeyValues3 *	SetMemberString( CKV3MemberName memberName, const char *value );
	KeyValues3 *	SetMemberResourceString( CKV3MemberName memberName, const char *value );
	KeyValues3 *	SetMemberInt( CKV3MemberName memberName, int value );
	KeyValues3 *	SetMemberInt64( CKV3MemberName memberName, int64 value );
	KeyValues3 *	SetMemberUint64( CKV3MemberName memberName, uint64 value );
	KeyValues3 *	SetMemberFloat( CKV3MemberName memberName, float value );
	KeyValues3 *	SetMemberDouble( CKV3MemberName memberName, double value );
	KeyValues3 *	SetMemberToNull( CKV3MemberName memberName );
	KeyValues3 *	SetMemberBool( CKV3MemberName memberName, bool value );
	KeyValues3 *	SetMemberToEmptyTable( CKV3MemberName memberName );
	void			SetMemberFloatArray( CKV3MemberName memberName, int nFloats, const float *pValues );
	void			SetMemberVector( CKV3MemberName memberName, const Vector &value )				{ SetMemberFloatArray( memberName, 3, (float*)&value ); }
	void			SetMemberVector2D( CKV3MemberName memberName, const Vector2D &value )			{ SetMemberFloatArray( memberName, 2, (float*)&value ); }
	void			SetMemberVector4D( CKV3MemberName memberName, const Vector4D &value )			{ SetMemberFloatArray( memberName, 4, (float*)&value ); }
	void			SetMemberQAngle( CKV3MemberName memberName, const QAngle &value )				{ SetMemberFloatArray( memberName, 3, (float*)&value ); }
	void			SetMemberQuaternion( CKV3MemberName memberName, const Quaternion &value )		{ SetMemberFloatArray( memberName, 4, (float*)&value ); }
	void			SetMemberMatrix( CKV3MemberName memberName, const matrix3x4_t &value )			{ SetMemberFloatArray( memberName, 12, (float*)&value ); }



	//--------------------------------------------------------------------------------------------------
	// Binary blob interface ( KEYVALUES3_TYPE_BINARY_BLOB )
	//--------------------------------------------------------------------------------------------------
	const byte *	GetBinaryBlobBase() const;
	int				GetBinaryBlobSize() const;
	byte			GetBinaryBlobByte( int nIndex ) const;
	void			SetToBinaryBlob( const byte *pData, int nSize ); // makes a copy
	void			SetToZeroedBinaryBlob( int nSize );


	//--------------------------------------------------------------------------------------------------
	// Flag interface
	//--------------------------------------------------------------------------------------------------
	bool			HasFlag( KeyValues3Flag_t nFlag ) const;
	void			SetFlag( KeyValues3Flag_t nFlag, bool bSet = true ); // set or clear
	void			SetAllFlags( KeyValues3Flag_t nFlag ); // stomp
	bool			HasAnyFlags() const;
	KeyValues3Flag_t GetAllFlags() const;


	//--------------------------------------------------------------------------------------------------
	// Metadata interface (only present when in a CKeyValues3Context with SetMetadataEnabled)
	//--------------------------------------------------------------------------------------------------
	bool			HasMetadata() const;
	int				Metadata_GetLineNumber() const;
	const char *	Metadata_GetFilename() const;
	void			Metadata_SetFileLineNumber( const char *pFilename, int nLine ) const;

private:
	friend class CKeyValues3Cluster;
	friend class CKeyValues3Context;
	friend class CKeyValues3Table;
	friend class CKV3ExternalDataWriter;

	// Context-based constructor is private because it has special requirements
	KeyValues3( CKeyValues3Context *pContext );

	KeyValues3 *	Internal_FindMember( CKV3MemberName memberName );
	const KeyValues3 *Internal_FindMember( CKV3MemberName memberName ) const { return const_cast<KeyValues3*>(this)->Internal_FindMember( memberName ); }
	void			Internal_SetToShortString( const char *value );
	void			Internal_PrepareForInternalType( KeyValues3InternalType_t newType );
	void			Internal_PrepareForType( KeyValues3Type_t newType ) { Internal_PrepareForInternalType(KeyValues3InternalType_t(newType)); }
	void			Internal_ClearDataWithoutFreeing();
	void			Internal_NewAllocation();
	void			Internal_FreeAllocation();
	void			Internal_SetArrayCount( int nCount );
	const byte *	Internal_BinaryBlobBase() const;
	int				Internal_BinaryBlobSize() const;
	CKeyValues3Metadata *Internal_GetMetadata( CKeyValues3Context **pOutContext ) const;

	// ******************************************************************************************************************
	// **** CAUTION: External storage means the lifetime of the external value must exceed or equal this KeyValues3 *****
	// ******************************************************************************************************************
	void			Internal_AimAtExternalString( const char *value );
	void			Internal_AimAtExternalBinaryBlob( const byte *pData, int nSize );
	KeyValues3 *	Internal_FindOrCreateMemberAimedAtExternalName( CKV3MemberName memberName );

	//--------------------------------------------------------------------------------------------------
	// 
	//--------------------------------------------------------------------------------------------------
	// sizeof(int64) due to alignment
	uint64 m_bContextIndependent				: 1; // heap allocated, owns any pointed-to allocations
	uint64 m_InternalType						: KEYVALUES3_BITS_FOR_TYPE; // KeyValues3InternalType_t
	uint64 m_Flags								: KEYVALUES3_BITS_FOR_FLAGS; // KeyValues3Flag_t
	uint64 m_BitFieldPadding					: 64 - (1 + KEYVALUES3_BITS_FOR_TYPE + KEYVALUES3_BITS_FOR_FLAGS); // Android compiler is shortening this field to a uint32 when using less than 32 bits of the uint64.

	union // sizeof(int64)
	{
		const char *				m_pAsCharPtr; // reinterpret string symbol as char* for debugger
		int64						m_nAsInt64;
		double						m_flAsDouble;

		CKeyValues3Table *			m_pTable;
		KeyValues3Array_t *			m_pArray; // NULL means 0 length array
		KeyValues3BinaryBlob_t *	m_pBinaryBlob;
		KeyValues3BinaryBlobExternal_t *m_pBinaryBlobExternal;

		uint64						m_nAsUint64;
		bool						m_bAsBool;
		char						m_AsShortString[ KEYVALUES3_SHORT_STRING_LENGTH + 1 ];
	};
};

COMPILE_TIME_ASSERT( sizeof(KeyValues3) == 16 );


//--------------------------------------------------------------------------------------------------
// Implementation details for KV3 Clusters (see CKeyValues3Cluster)
//--------------------------------------------------------------------------------------------------
#define KV3_CLUSTER_SIZEOF			1024
#define KV3_CLUSTER_ALIGNOF			KV3_CLUSTER_SIZEOF // necessary for CKeyValues3Cluster::ClusterFromPointer to work
#define KV3_CLUSTER_VALUE_COUNT		uint64(62) // number of KeyValues3 per cluster
#define KV3_CLUSTER_FULL			( (uint64(1)<<KV3_CLUSTER_VALUE_COUNT) - 1 )


//--------------------------------------------------------------------------------------------------
// CKeyValues3Context is a helper class that allows you to share memory and reduce allocation count
// for a large KV3 tree. Table member names are stored in a shared symbol table, individual KV3 objects
// are allocated in clusters, etc.
//--------------------------------------------------------------------------------------------------
struct KeyValues3ContextImpl_t;
#define KV3_CONTEXT_SIZEOF			( 2 * KV3_CLUSTER_SIZEOF )

class ALIGN_N( KV3_CLUSTER_ALIGNOF ) CKeyValues3Context: public CAlignedNewDeleteNoBase< KV3_CLUSTER_SIZEOF >
{
public:
	CKeyValues3Context();
	~CKeyValues3Context();

	KeyValues3 *Root();
	const KeyValues3 *Root() const { return const_cast<CKeyValues3Context*>(this)->Root(); }

	KeyValues3 *operator->() { return Root(); }
	const KeyValues3 *operator->() const { return Root(); }

	bool IsIdenticalTo( const CKeyValues3Context *pOther, bool bAssertOnFailure ) const;

	void CopyFrom( const CKeyValues3Context *pOther );

	// Set metadata enabled before unserializing to get eg. line number attributions (see CKeyValues3::Metadata_GetLineNumber)
	void SetMetadataEnabled( bool bMetadata );
	bool IsMetadataEnabled() const;

private:
	friend class KeyValues3;
	friend class CKeyValues3Table;

	static const char *AllocSharedString( KeyValues3 *pParent, const char *pStr );
	static void FreeSharedString( KeyValues3 *pParent, const char *pStr );

	static const char *AllocString( KeyValues3 *pParent, const char *pStr );
	static void FreeString( KeyValues3 *pParent, const char *pStr );

	static KeyValues3 *AllocKV( KeyValues3 *pParent );
	static void FreeKV( KeyValues3 *pParent, KeyValues3 *pKV );

	KeyValues3 *AllocKV_Internal();
	void FreeKV_Internal( KeyValues3 *pKV );

	static KeyValues3Array_t *ReallocArray( KeyValues3 *pParent, KeyValues3Array_t *pOldArray, int nCount );
	static void FreeArray( KeyValues3 *pParent, KeyValues3Array_t *pArray );

	static KeyValues3Array_t *ArrayInsertMultipleBefore( KeyValues3 *pParent, KeyValues3Array_t *pOldArray, int nIndexToInsertBefore, int nInsertCount );
	static KeyValues3Array_t *ArrayRemoveMultiple( KeyValues3 *pParent, KeyValues3Array_t *pOldArray, int nFirstIndexToRemove, int nRemoveCount );

	static KeyValues3BinaryBlob_t *AllocZeroedBlob( KeyValues3 *pParent, int nSize );
	static KeyValues3BinaryBlob_t *AllocBlob( KeyValues3 *pParent, const byte *pData, int nSize );
	static KeyValues3BinaryBlobExternal_t *AllocBlobExternal( KeyValues3 *pParent, const byte *pData, int nSize );
	static void FreeBlob( KeyValues3 *pParent, KeyValues3BinaryBlob_t *pBlob );
	static void FreeBlobExternal( KeyValues3 *pParent, KeyValues3BinaryBlobExternal_t *pBlob );

	static CKeyValues3Table *AllocTable( KeyValues3 *pParent );
	static void FreeTable( KeyValues3 *pParent, CKeyValues3Table *pTable );

	KeyValues3ContextImpl_t &Impl();
	const KeyValues3ContextImpl_t &Impl() const;
	byte m_Impl[ KV3_CONTEXT_SIZEOF ]; // this is an inline KeyValues3ContextImpl_t, but we want to hide implementation details
}
ALIGN_N_POST( KV3_CLUSTER_ALIGNOF );

COMPILE_TIME_ASSERT( sizeof( CKeyValues3Context ) == KV3_CONTEXT_SIZEOF );


//------------------------------------------------------------------------------
// KV3 Loading
//------------------------------------------------------------------------------
// If you specify an expected format that doesn't match the loaded data, ConvertKV3Format will
// be invoked to convert to the expected format. (And the load will fail if the conversion is unsuccessful.)
//------------------------------------------------------------------------------

// General purpose loading - will examine the buffer to determine encoding and dispatch to the appropriate loader
bool LoadKV3( KeyValues3 *pTargetKV3, CUtlString *pOutErrorMessage, CUtlBuffer *pSrcBuffer, const KV3ID_t &expectedFormat = KV3_FORMAT_GENERIC, const char *pReferenceFilename = "" );
bool LoadKV3( CKeyValues3Context *pContext, CUtlString *pOutErrorMessage, CUtlBuffer *pSrcBuffer, const KV3ID_t &expectedFormat = KV3_FORMAT_GENERIC, const char *pReferenceFilename = "" );

// Helpers that wrap filesystem access
bool LoadKV3FromFile( KeyValues3 *pTargetKV3, CUtlString *pOutErrorMessage, const char *pFilename, const char *pPath, const KV3ID_t &expectedFormat = KV3_FORMAT_GENERIC );
bool LoadKV3FromFile( CKeyValues3Context *pContext, CUtlString *pOutErrorMessage, const char *pFilename, const char *pPath, const KV3ID_t &expectedFormat = KV3_FORMAT_GENERIC );

// If you know your data is text
bool LoadKV3Text( KeyValues3 *pRootTarget, CUtlString *pOutErrorMessage, CUtlBuffer *pBuffer, const KV3ID_t &expectedFormat = KV3_FORMAT_GENERIC, const char *pReferenceFilename = "" );
bool LoadKV3Text( KeyValues3 *pRootTarget, CUtlString *pOutErrorMessage, const char *pBuffer, const KV3ID_t &expectedFormat = KV3_FORMAT_GENERIC, const char *pReferenceFilename = "" );

// Load a raw KV3 text block with no GUID header - always interprets as KV3_ENCODING_TEXT and KV3_FORMAT_GENERIC
bool LoadKV3Text_NoHeader( KeyValues3 *pRootTarget, CUtlString *pOutErrorMessage, const char *pBuffer, const KV3ID_t &expectedFormat = KV3_FORMAT_GENERIC, const char *pReferenceFilename = "" );

// If you know your data is binary
// (If pLongTermScratchBuffer is provided, it must outlive pRootTarget. It will be used in place of some allocations and reduce copying.)
bool LoadKV3Binary( KeyValues3 *pRootTarget, CUtlString *pOutErrorMessage, CUtlBuffer *pSrcBuffer, CUtlBuffer *pLongTermScratchBuffer = nullptr, const KV3ID_t &expectedFormat = KV3_FORMAT_GENERIC, const char *pReferenceFilename = "" );

// Legacy support
bool LoadKV3FromOldSchemaText( KeyValues3 *pRootTarget, CUtlString *pOutErrorMessage, CUtlBuffer *pSrcBuffer, const KV3ID_t &expectedFormat = KV3_FORMAT_GENERIC, const char *pReferenceFilename = "" );

// Helpers to examine a buffer for KV3-ness
bool IsKV3Data( void *pSrcBuffer, int nBufLen );
bool IsKV3Data( CUtlBuffer *pSrcBuffer );
bool LooksLikeKV3TextData( void *pData, int nBufLen );
bool LooksLikeOldSchemaText( void *pData, int nBufLen );
bool LooksLikeKV3BinaryData( void *pData, int nBufLen );


//------------------------------------------------------------------------------
// Old KeyValues (KV1) support
//------------------------------------------------------------------------------
enum KV1TextEscapeBehavior_t
{
	KV1_NO_ESCAPE_SEQUENCES, // default behavior unless you call KeyValues::UsesEscapeSequences( true ) before reading/writing your KV
	KV1_HAS_ESCAPE_SEQUENCES,
};

bool LoadKV3FromKV1Text( KeyValues3 *pRoot, CUtlString *pOutErrorMessage, const char *pBuffer, KV1TextEscapeBehavior_t nEscapeBehavior = KV1_NO_ESCAPE_SEQUENCES, const char *pReferenceFilename = "" );
bool LoadKV3FromKV1File( KeyValues3 *pRoot, CUtlString *pOutErrorMessage, const char *pPath, const char *pFilename, KV1TextEscapeBehavior_t nEscapeBehavior = KV1_NO_ESCAPE_SEQUENCES );
bool SaveKV3AsKV1Text( const KeyValues3 *pRoot, CUtlString *pOutErrorMessage, CUtlBuffer *pDestBuffer, KV1TextEscapeBehavior_t nEscapeBehavior = KV1_NO_ESCAPE_SEQUENCES );

//------------------------------------------------------------------------------
// More Complex KV1 -> KV3 Translation
//------------------------------------------------------------------------------
//
// Designed to allow more nuanced conversion from KV1 to idiomatic KV3
//
//	KV1 Data:
//		"root"
//		{
//			"eye_data"
//			{
//				"eye"
//				{
//					XXX
//				}
//				"eye"
//				{
//					YYY
//				}
//			}
//		}
//	
//	Default KV1 -> KV3 Conversion (duplicate keys get suffixed with #123):
//		{
//			eye_data
//			{
//				eye =
//				{
//					XXX
//				}
//				eye#1 =
//				{
//					YYY
//				}
//			}
//		}
//	
//	With { KV1TOKV3_TRANSLATE_DUPLICATE_KEY_INTO_ARRAY, "eye_data.eye", "eye_list" }:
//		{
//			eye_data
//			{
//				eye_list =
//				[
//					{
//						XXX
//					},
//					{
//						YYY
//					},
//				]
//			}
//		}
//
//	With { KV1TOKV3_TRANSLATE_SUBKEYS_INTO_ARRAY, "eye_data", "" }:
//		{
//			eye_data =
//			[
//				XXX,
//				YYY
//			]
//		}
//
//------------------------------------------------------------------------------
enum KV1ToKV3TranslationType_t
{
	KV1TOKV3_TRANSLATE_DUPLICATE_KEY_INTO_ARRAY,	// for the matching key path, take all values and put them into a single array (with a key name of m_pNewKeyName)
	KV1TOKV3_TRANSLATE_SUBKEYS_INTO_ARRAY,			// for the matching key path, turn the value from a table into an array
	KV1TOKV3_TRANSLATE_SUBKEY_NAMES_INTO_ARRAY,		// for the matching key path, turn the all the keys into an array of values
	KV1TOKV3_TRANSLATE_UNIQIFY_KEYS,				// for the matching key path, uniqueify duplicate key names
};

struct KV1ToKV3Translation_t
{
	KV1ToKV3TranslationType_t m_nTranslationType;
	const char *m_pPath;
	const char *m_pNewKeyName;				// nullptr means don't rename
};

bool LoadKV3FromKV1Text_Translated( KeyValues3 *pRoot, CUtlString *pOutErrorMessage, const char *pBuffer, KV1TextEscapeBehavior_t nEscapeBehavior, const KV1ToKV3Translation_t *pTranslations, int nTranslations, const char *pReferenceFilename = "" );


//------------------------------------------------------------------------------
// KV3 Saving (See encoding and format IDs, below)
//------------------------------------------------------------------------------
bool SaveKV3( const KV3ID_t &encodingId, const KV3ID_t &formatId, const KeyValues3 *pRoot, CUtlString *pOutErrorMessage, CUtlBuffer *pDestBuffer );
bool SaveKV3Text( const KV3ID_t &encodingId, const KV3ID_t &formatId, const KeyValues3 *pRoot, CUtlString *pOutErrorMessage, CUtlBuffer *pDestBuffer );
bool SaveKV3Text_ToString( const KV3ID_t &encodingId, const KV3ID_t &formatId, const KeyValues3 *pRoot, CUtlString *pOutErrorMessage, CUtlString *pDestString );
bool SaveKV3Binary( const KV3ID_t &encodingId, const KV3ID_t &formatId, const KeyValues3 *pRoot, CUtlString *pOutErrorMessage, CUtlBuffer *pDestBuffer );

// Load a raw KV3 text block with no GUID header - for consumption by LoadKV3Text_NoHeader
bool SaveKV3Text_NoHeader( const KeyValues3 *pRoot, CUtlString *pOutErrorMessage, CUtlBuffer *pDestBuffer );

// Helper that wraps filesystem access
bool SaveKV3ToFile( const KV3ID_t &encodingId, const KV3ID_t &formatId, const KeyValues3 *pRoot, CUtlString *pOutErrorMessage, const char *pFilename, const char *pPath );


//------------------------------------------------------------------------------
// KV3 Format Conversion (automatically invoked by loader paths)
//------------------------------------------------------------------------------
// Chains as many conversions as necessary to convert pRoot from fromFormat to toFormat
// Returns false if conversion failed or no viable conversion path exists between the specified formats.
//------------------------------------------------------------------------------
bool ConvertKV3Format( KeyValues3 *pRoot, const KV3ID_t &fromFormat, const KV3ID_t &toFormat, CUtlString *pOutError );
