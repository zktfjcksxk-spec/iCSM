//=========== Copyright Valve Corporation, All rights reserved. ===============//
// Implementation of the "grisu" exact floating-point printing algoritm from
// Loitsch2010 "Printing Floating-Point Numbers Accurately and Quickly With Integers"
//
// See floatprint.cpp for bugs/features.
//===========================================================================//

#ifndef FLOATPRINT_H
#define FLOATPRINT_H

#ifdef _WIN32
#pragma once
#endif

#include "basetypes.h"

// Turn this on to enable testing code
#define FLOATPRINT_TEST 0

struct FPPrettyPrintOptions
{
	// Whether to successfully round-trip NaN values by printing their raw bits as hex
	bool bDisplayNaNBits;

	// Whether to include '.' in non-scientific-notation integers.  Default false
	// false:
	//		1.0			-> "1"
	//		1.1			-> "1.1"
	//		1000		-> "1000"
	// true:
	//		1.0			-> "1."
	//		1.1			-> "1.1"
	//		1000		-> "1000."
	bool bAlwaysIncludeDecimalPoint;

	// Maximum number of digits above the decimal before switching to scientific notation.  Default 10 (enough to display any int32)
	// 0:
	//		1			-> 1.e0
	//		1.1			-> 1.1e0
	//		1000		-> 1.e3
	// 3:
	//		1			-> 1
	//		1.1			-> 1.1
	//		1000		-> 1.e3
	// 4:
	//		1			-> 1
	//		1.1			-> 1.1
	//		1000		-> 1000
	int nMaxDigitsAboveDecimal;

	// Whether to display the leading zero in values below 1.  Default true
	// true:
	//		0.1			-> 0.1
	//		1e-9		-> 0.000000001
	//		1.5e-9		-> 0.0000000015
	//		1.5e-10		-> 1.5e-10
	// false:
	//		0.1			-> .1
	//		1e-9		-> .000000001
	//		1.5e-9		-> .0000000015
	//		1.5e-10		-> 1.5e-10
	bool bDisplayLeadingZero;

	// Maximum number of zeroes below the decimal before switching to scientific notation.  Default 8
	// -1:
	//		0.1			-> 1.e-1
	//		1e-9		-> 1.e-9
	//		1.5e-9		-> 1.5e-9
	//		1.5e-10		-> 1.5e-10
	// 0:
	//		0.1			-> 0.1
	//		1e-9		-> 1.e-9
	//		1.5e-9		-> 1.5e-9
	//		1.5e-10		-> 1.5e-10
	// 8:
	//		0.1			-> 0.1
	//		1e-9		-> 0.000000001
	//		1.5e-9		-> 0.0000000015
	//		1.5e-10		-> 1.5e-10
	int nMaxZeroesBelowDecimal;

	// Maximum length below the decimal before switching to scientific notation.  Default 1000 (basically, infinite)
	// -1:
	//		0.1			-> 0.1
	//		1e-9		-> 0.000000001
	//		1.5e-9		-> 0.0000000015
	//		1.5e-10		-> 1.5e-10
	// 9:
	//		0.1			-> 0.1
	//		1e-9		-> 0.000000001
	//		1.5e-9		-> 1.5e-9
	//		1.5e-10		-> 1.5e-10
	int nMaxBelowDecimalLength;

	constexpr int RequiredBufSize( int nFloatSize, int nSignificantDigits, int nExponentDigits ) const noexcept;
	constexpr FPPrettyPrintOptions WithDisplayNaNBits( bool opt ) const noexcept;
	constexpr FPPrettyPrintOptions WithAlwaysIncludeDecimalPoint( bool opt ) const noexcept;
	constexpr FPPrettyPrintOptions WithDisplayLeadingZero( bool opt ) const noexcept;
	constexpr FPPrettyPrintOptions WithMaxDigitsAboveDecimal( int opt ) const noexcept;
	constexpr FPPrettyPrintOptions WithMaxZeroesBelowDecimal( int opt ) const noexcept;
	constexpr FPPrettyPrintOptions WithMaxBelowDecimalLength( int opt ) const noexcept;

	constexpr int RequiredBufSizeFloat() const noexcept;
	constexpr int RequiredBufSizeDouble() const noexcept;
};

// It's safe for these to be overestimates, they just control buffer size requirements
#define kVPrintFloat_SignificantDigits		9
#define kVPrintFloat_ExponentDigits			3		// e-40 to e+38
#define kVPrintFloat_BufLen					21		// when using default pretty-printing options
#define kVPrintFloatRaw_BufLen				(1 + kVPrintFloat_SignificantDigits)

#define kVPrintDouble_SignificantDigits		17
#define kVPrintDouble_ExponentDigits		3		// e-300ish to e+300ish
#define kVPrintDouble_BufLen				29		// when using default pretty-printing options
#define kVPrintDoubleRaw_BufLen				(1 + kVPrintDouble_SignificantDigits)

// Pretty-prints f to buf
void V_PrintFloat( float f, char* buf, int bufLen );
void V_PrintFloat( float f, char* buf, int bufLen, const FPPrettyPrintOptions& options );
void V_PrintDouble( double f, char* buf, int bufLen );
void V_PrintDouble( double f, char* buf, int bufLen, const FPPrettyPrintOptions& options );

// Returns a buffer containing an integer string and an exponent
// such that f is the closest float to (atoi(buf) * 10^exponent)
//
// returns false in the case of NaN / Infinity; these can be printed with
// V_FPPrettySpecial.
bool V_PrintFloatRaw( float f, char* buf, int bufLen, int* pExponent, int *pLengthUsed );
bool V_PrintDoubleRaw( double f, char* buf, int bufLen, int* pExponent, int* pLengthUsed );

// Pretty-print the results from V_Print(Float|Double)Raw
void V_FPPretty( char* buf, int bufLen, const char* srcBuf, int exponent, int lengthUsed, const FPPrettyPrintOptions& options );
void V_FPPrettySpecialFloat( char* buf, int bufLen, float f, const FPPrettyPrintOptions& options );
void V_FPPrettySpecialDouble( char* buf, int bufLen, double f, const FPPrettyPrintOptions& options );

// match printf format specification.  mode = 'e', 'E', 'f', 'F', 'g', or 'G', the format character specified
void V_FPPrettyPrintf( char* buf, int bufLen, const char* srcBuf, int exponent, int lengthUsed, char mode, int leftPrecision, int rightPrecision );
void V_FPPrettyPrintfSpecial( int );

#if FLOATPRINT_TEST
void Test_Grisu();
#endif

//////////////////////////////////////////////////////////////////////////
// Inline function definitions

constexpr int FPPrettyPrintOptions::RequiredBufSize( int kFloatSize, int kSignificantDigits, int kExponentDigits ) const noexcept
{
	// "Inf" or "-Inf" or "NaN" or "NaN(0x________)"
	#define kNanInfLen ( bDisplayNaNBits ? 4 : ( 7 + 2 * kFloatSize ) )

	// -_________000000000[.]
	#define kIntegerValueLen ( ( bAlwaysIncludeDecimalPoint ? 2 : 1 ) + nMaxDigitsAboveDecimal )

	// -____._____
	#define kSplitValueLen ( kSignificantDigits + 2 )

	// -[0].000000_________
	#define kFractionLen ( ( bDisplayLeadingZero ? 3 : 2 ) + MIN( nMaxBelowDecimalLength, nMaxZeroesBelowDecimal + kSignificantDigits ) )

	// -_.________e-___
	#define kScientificNotationLen ( 4 + kSignificantDigits + kExponentDigits )

	// Add NUL terminator
	return 1 + MAX( kIntegerValueLen, MAX( MAX( kNanInfLen, kSplitValueLen ), MAX( kFractionLen, kScientificNotationLen ) ) );

	#undef kNanInfLen
	#undef kIntegerValueLen
	#undef kSplitValueLen
	#undef kFractionLen
	#undef kScientificNotationLen
}

constexpr FORCEINLINE FPPrettyPrintOptions FPPrettyPrintOptions::WithDisplayNaNBits( bool opt ) const noexcept
{
	return FPPrettyPrintOptions{ opt, bAlwaysIncludeDecimalPoint, nMaxDigitsAboveDecimal, bDisplayLeadingZero, nMaxZeroesBelowDecimal, nMaxBelowDecimalLength };
}
constexpr FORCEINLINE FPPrettyPrintOptions FPPrettyPrintOptions::WithAlwaysIncludeDecimalPoint( bool opt ) const noexcept
{
	return FPPrettyPrintOptions{ bDisplayNaNBits, opt, nMaxDigitsAboveDecimal, bDisplayLeadingZero, nMaxZeroesBelowDecimal, nMaxBelowDecimalLength };
}
constexpr FORCEINLINE FPPrettyPrintOptions FPPrettyPrintOptions::WithDisplayLeadingZero( bool opt ) const noexcept
{
	return FPPrettyPrintOptions{ bDisplayNaNBits, bAlwaysIncludeDecimalPoint, nMaxDigitsAboveDecimal, opt, nMaxZeroesBelowDecimal, nMaxBelowDecimalLength };
}
constexpr FORCEINLINE FPPrettyPrintOptions FPPrettyPrintOptions::WithMaxDigitsAboveDecimal( int opt ) const noexcept
{
	return FPPrettyPrintOptions{ bDisplayNaNBits, bAlwaysIncludeDecimalPoint, opt, bDisplayLeadingZero, nMaxZeroesBelowDecimal, nMaxBelowDecimalLength };
}
constexpr FORCEINLINE FPPrettyPrintOptions FPPrettyPrintOptions::WithMaxZeroesBelowDecimal( int opt ) const noexcept
{
	return FPPrettyPrintOptions{ bDisplayNaNBits, bAlwaysIncludeDecimalPoint, nMaxDigitsAboveDecimal, bDisplayLeadingZero, opt, nMaxBelowDecimalLength };
}
constexpr FORCEINLINE FPPrettyPrintOptions FPPrettyPrintOptions::WithMaxBelowDecimalLength( int opt ) const noexcept
{
	return FPPrettyPrintOptions{ bDisplayNaNBits, bAlwaysIncludeDecimalPoint, nMaxDigitsAboveDecimal, bDisplayLeadingZero, nMaxZeroesBelowDecimal, opt };
}

constexpr FORCEINLINE int FPPrettyPrintOptions::RequiredBufSizeFloat() const noexcept
{
	return RequiredBufSize( sizeof( float ), kVPrintFloat_SignificantDigits, kVPrintFloat_ExponentDigits );
}
constexpr FORCEINLINE int FPPrettyPrintOptions::RequiredBufSizeDouble() const noexcept
{
	return RequiredBufSize( sizeof( double ), kVPrintDouble_SignificantDigits, kVPrintDouble_ExponentDigits );
}

// REI: I'm not sure the last couple of options here are tuned well.
//
// It basically says "don't use scientific notation for any number
// with the most significant digit >= 10^-9."
//
// This can lead to very long strings; for example, the float with
// bit representation 0x30897060 renders as "0.0000000010000001".
//
// With MaxBelowDecimalLength(9), that float would render as
// "1.0000001e-9", but the very nearby float 0.000000001 would
// not render in scientific notation; nearby values would render
// very differently.
//
// I'm not sure which is the better choice at the moment.
constexpr FPPrettyPrintOptions kFPPrettyPrintOptionsDefault =
	FPPrettyPrintOptions()
		.WithDisplayNaNBits( true )
		.WithAlwaysIncludeDecimalPoint( false )
		.WithMaxDigitsAboveDecimal( 10 )		// allows printing all int32 values without scientific notation
		.WithDisplayLeadingZero( true )
		.WithMaxZeroesBelowDecimal( 8 )
		.WithMaxBelowDecimalLength( 1000 );		// see note above

#endif // FLOATPRINT_H
