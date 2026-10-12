//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
// CUtlTypesafeInt: A typesafe wrapper around an integer to prevent unintentional conversion
//
// Usage:
//		DECLARE_TYPESAFE_INT( IndexTypeA );
//		DECLARE_TYPESAFE_INT( IndexTypeB );
//		IndexTypeA a( 5 );
//		IndexTypeB b = a; // will NOT compile
//
//==================================================================================================

#ifndef UTLTYPESAFEINT_H
#define UTLTYPESAFEINT_H

/// A typesafe wrapper around an integer
template<class TUniqueIdentifier, class TIntegralType = int>
class CUtlTypesafeInt
{
public:
	CUtlTypesafeInt() { }
	explicit CUtlTypesafeInt( TIntegralType nVal ) { m_nVal = nVal; }

	inline void Set( TIntegralType nVal ) { m_nVal = nVal; }
	inline TIntegralType Get() const { return m_nVal; }

	inline void FromInt( TIntegralType nVal ) { m_nVal = nVal; }
	inline TIntegralType ToInt() const { return m_nVal; }

	inline TIntegralType* ToPtr() { return &m_nVal; }
	inline const TIntegralType* ToPtr() const { return &m_nVal; }

	// Math!
	inline bool operator<( const CUtlTypesafeInt& other ) const { return m_nVal < other.m_nVal; }
	inline bool operator<=( const CUtlTypesafeInt& other ) const { return m_nVal <= other.m_nVal; }
	inline bool operator>( const CUtlTypesafeInt& other ) const { return m_nVal > other.m_nVal; }
	inline bool operator>=( const CUtlTypesafeInt& other ) const { return m_nVal >= other.m_nVal; }
	inline CUtlTypesafeInt& operator++() { ++m_nVal; return *this; }
	inline CUtlTypesafeInt& operator--() { --m_nVal; return *this; }
	inline CUtlTypesafeInt operator++( int ) { CUtlTypesafeInt result( m_nVal ); m_nVal++; return result; }
	inline CUtlTypesafeInt operator--( int ) { CUtlTypesafeInt result( m_nVal ); m_nVal++; return result; }
	inline CUtlTypesafeInt operator+( const TIntegralType i ) const { return CUtlTypesafeInt( m_nVal + i ); }
	inline CUtlTypesafeInt operator-( const TIntegralType i ) const { return CUtlTypesafeInt( m_nVal - i ); }
	inline CUtlTypesafeInt operator+( const CUtlTypesafeInt& other ) const { return CUtlTypesafeInt( m_nVal + other.m_nVal ); }
	inline CUtlTypesafeInt operator-( const CUtlTypesafeInt& other ) const { return CUtlTypesafeInt( m_nVal - other.m_nVal ); }
	inline bool operator==( const CUtlTypesafeInt& other ) const { return m_nVal == other.m_nVal; }
	inline bool operator!=( const CUtlTypesafeInt& other ) const { return m_nVal != other.m_nVal; }

private:
	TIntegralType m_nVal;
};

#define DECLARE_TYPESAFE_INT( name ) typedef CUtlTypesafeInt< struct name##_id* > name;
#define DECLARE_TYPESAFE_INT8( name ) typedef CUtlTypesafeInt< struct name##_id*, int8 > name;
#define DECLARE_TYPESAFE_INT16( name ) typedef CUtlTypesafeInt< struct name##_id*, int16 > name;
#define DECLARE_TYPESAFE_INT32( name ) typedef CUtlTypesafeInt< struct name##_id*, int32 > name;
#define DECLARE_TYPESAFE_INT64( name ) typedef CUtlTypesafeInt< struct name##_id*, int64 > name;
#define DECLARE_TYPESAFE_UINT8( name ) typedef CUtlTypesafeInt< struct name##_id*, uint8 > name;
#define DECLARE_TYPESAFE_UINT16( name ) typedef CUtlTypesafeInt< struct name##_id*, uint16 > name;
#define DECLARE_TYPESAFE_UINT32( name ) typedef CUtlTypesafeInt< struct name##_id*, uint32 > name;
#define DECLARE_TYPESAFE_UINT64( name ) typedef CUtlTypesafeInt< struct name##_id*, uint64 > name;
#define DECLARE_TYPESAFE_INTP( name ) typedef CUtlTypesafeInt< struct name##_id*, intp > name;

#endif // UTLTYPESAFEINT_H
