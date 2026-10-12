//=========== Copyright Valve Corporation, All rights reserved. ===============//
// Implementation of the "grisu" exact floating-point printing algoritm from
// Loitsch2010 "Printing Floating-Point Numbers Accurately and Quickly With Integers"
//
// We implement Grisu2 which "almost always" prints the shortest possible representation
// of a given floating point value.  We always maintain the read/write identity; that is,
// read(write(x)) == x.
//
// In addition, we attempt to maintain the 'shortest' identity, that is,
// len(write(read(x))) <= len(x), but allow this constraint to fail in the name of
// performance.
//
// We do not attempt to print the 'optimal' string, that is, the one closest to the
// target number that also rounds to it.  See issue 1.
//
// I have exhaustively tested the read identity for all 2,139,095,040 possible float
// values, using atof() to parse the float back from the string value.  I have done
// random testing on double values and found no errors in 560 million tests
// (and counting)
//
// Algorithm, given a positive float/double f
//      1. Convert f to a diy_fp by extracting its exponent/mantissa
//      2. Generate m- and m+, the boundaries between f and its adjacent values.
//			* If we print any number strictly within this range, we will maintain
//			  the 'read' identity, since the closest valid floating point value is f.
//      3. Multiply these by a suitable power of 10 so that 1 < result < 10^10
//		4. Slightly nudge these bounds to conservatively account for any error in
//			the estimate of	10^k and from the multiplication.
//		5. Extract delta, the difference between m+ and m-.
//		6. Split m+ into integer and fractional parts (which have different printing
//			algorithms, but they are both easy)
//		7. Maintain e, the error between m+ and the digits we have printed so far.
//			This starts as equal to m+ as we haven't printed any digits; as we
//			print digits its magnitude is reduced by ~1/10.
//		8. Print digits of m+ until e < delta, reducing the exponent k (from 10^k)
//			by 1 for each digit.
//			* We are guaranteed that the value we print is always < m+, since
//			  we are printing its digits.
//			* Therefore value_so_far + e = m+
//			* Therefore value_so_far = m+ - e
//			* When e < delta, value_so_far > m- and we can stop printing.
//		9. Output the exponent k and the generated string s; the closest float to
//			s * 10^k must be our input float.
//		10. Pretty-print the result for better human-readability.
//
// Issues:
//
// 1 BUG/PERF: We currently print values 'above' the target instead of closest decimal
//		value.  We still generate strings that are shortest and closer to the target
//		than any other float/double, but sometimes they are closer to the rounding
//		edge than they need to be.  This gives better performance since it allows us
//		to work directly with M_plus (see the code) and not have to perform additional
//		operations on the target value.
//
// 2 BUG/FEATURE: When printing floats, we narrow the allowed output range to avoid
//		double-rounding bugs.  For example, imagine that exact doubles were
//      1, 1.25, 1.5, 1.75, 2, and exact floats only represented 1 and 2.
//
//      We make sure not to print '1.4' for 1, since if that is read in as a double,
//      it would round up 1.5, and then when cast to a float, would round up *again*
//      to 2, even though 2 is further from 1.4 than 1 is.  Instead, we might print
//      '1.3' since that would round down to 1.25 and then round correctly to 1.
//
//		(This particular example wouldn't happen since '1' is the shortest string
//		that rounds to 1, but there were other examples such as 9.3137999e-33 which
//		exhibited this double-rounding behavior)
//
// 3 BUG/PERF: We only print the shortest decimal string "almost always"; Grisu3 can detect
//		when it is not printing the shortest possible representation, but that requires a
//		slower fallback algorithm to handle these cases (e.g. Dragon4).  I currently don't
//		intend to implement a second algorithm for FP printing, and the cases we are using
//		this (serialization) it's OK to not always print the shortest value, as long as we
//		properly maintain read/write parity; that is, read(write(x)) == x.
//
// 4 BUG: We may print extra digits for denormalized values since we currently treat them
//      as if they were full precision.  We could fix this by changing Decode* to output m+
//		and m- instead of just the target value, but denormalized values are uncommon
//		enough that I am OK to just print extra digits for them as if they were normalized.

#include "basetypes.h"
#include "floatprint.h"
#include "bitvec.h" // Plat_BitScanReverse

#if FLOATPRINT_TEST
#include <time.h> // profiling
#endif

// These asserts should not be able to be caused by bad input; optionally disable them
// in debug builds for a bit of additional speed.
#define GRISU_INTERNAL_ASSERTS 1

#if GRISU_INTERNAL_ASSERTS
#define AssertInternalError(x) Assert(x)
#else
#define AssertInternalError(x) /*nothing*/
#endif

namespace
{	
	const uint64 kHighBitU64 = 0x8000000000000000ull;

	// Represents f * 2^e
	struct diy_fp {
		uint64 f;	// mantissa
		int e;		// exponent

		static const int kQ = 64; // number of bits in f
	};

	bool operator==( const diy_fp& lhs, const diy_fp& rhs )
	{
		return lhs.f == rhs.f && lhs.e == rhs.e;
	}

	// for example
	static const diy_fp kDiyFPOne = {
		kHighBitU64,
		1 - diy_fp::kQ
	};

	diy_fp DiyFP_Normalize( diy_fp x )
	{
		uint64 f = x.f;
		uint32 high = f >> 32;
		uint32 low = f & 0xffffffff;

		int shiftCount;
		if ( high == 0 )
		{
			AssertInternalError( low != 0 );
			shiftCount = 32 + ( 31 ^ Plat_BitScanReverse( low ) );
		}
		else
		{
			shiftCount = 31 ^ Plat_BitScanReverse( high );
		}

		x.f = f << shiftCount;
		x.e = x.e - shiftCount;

		return x;
	}

	diy_fp DiyFP_Subtract( diy_fp x, diy_fp y )
	{
		AssertInternalError( x.e == y.e );
		AssertInternalError( x.f >= y.f );
		diy_fp r = { x.f - y.f, x.e };
		return r; // might not be normalized (possibly even zero)
	}

	diy_fp DiyFP_Multiply( diy_fp x, diy_fp y )
	{
#if 0 // nonportable
		__int128 xf = ( __int128 )( x.f );
		__int128 yf = ( __int128 )( y.f );
		__int128 xf_yf = xf * yf;

		// round up by a half bit
		xf_yf += ( ( uint64 )1 ) << 63;

		// extract significant bits
		uint64 f = ( uint64 )( xf_yf >> 64 );

		diy_fp r = { xf_yf, x.e + y.e - diy_fp::kQ + 64 };
#else
		// Split uint64 into 2 base-2^32 digits and apply grade-school multiplication algorithm which avoids overflow
		static const uint64 max32 = 0xffffffff;
		uint64 xh = x.f >> 32;
		uint64 xl = x.f & max32;
		uint64 yh = y.f >> 32;
		uint64 yl = y.f & max32;

		//                          xh        xl
		//                 *        yh        yl
		// -------------------------------------
		//                     xl_yl_h   xl_yl_l
		//           xh_yl_h   xh_yl_l
		//           xl_yh_h   xl_yh_l
		// xh_yh_h   xh_yh_l
		// -------------------------------------
		//      final result + round up last bit

		uint64 xl_yl = xl*yl;
		uint64 xl_yl_h = xl_yl >> 32;
		// uint64 xl_yl_l = xl_yl & max32; // not needed

		uint64 xh_yl = xh*yl;
		uint64 xh_yl_h = xh_yl >> 32;
		uint64 xh_yl_l = xh_yl & max32;

		uint64 xl_yh = xl*yh;
		uint64 xl_yh_h = xl_yh >> 32;
		uint64 xl_yh_l = xl_yh & max32;

		uint64 xh_yh = xh*yh; // don't need to split this because we would just re-merge it

		uint64 low_carry = ( xl_yl_h + xh_yl_l + xl_yh_l + uint64( 1U << 31 ) ) >> 32;
		diy_fp r = { xh_yl_h + xl_yh_h + xh_yh + low_carry, x.e + y.e + diy_fp::kQ };
#endif
		return r;
	}

	enum FloatFlags {
		kFloatFlag_Negative = 0x1,
		kFloatFlag_Zero = 0x2,
		kFloatFlag_InfNan = 0x4,
	};

	//  Decodes a float and returns exponent/mantissa such that f == mantissa * 2^exponent and the high bit of mantissa is set
	FORCEINLINE void DecodeIEEE754Single( float f, uint8& flags, int &exponent, uint64& mantissa )
	{
		uint32 repr;
		COMPILE_TIME_ASSERT( sizeof( f ) == sizeof( repr ) );
		memcpy( &repr, &f, sizeof( repr ) ); // should optimize to mov

		const int kExponentOffset = -127;
		const int kExponentBits = 8;
		const int kMantissaBits = 23;
		const uint32 kExponentMask = ( 1U << kExponentBits ) - 1;
		const uint32 kMantissaMask = ( 1U << kMantissaBits ) - 1;

		// decode IEEE single precision float ( https://en.wikipedia.org/wiki/Single-precision_floating-point_format )
		flags = ( repr >> ( kExponentBits + kMantissaBits ) ) & 0x1; // sets kFloatFlag_Negative if sign bit set
		uint32 mantissaWork = repr & kMantissaMask;
		int exponent_unadjusted = ( ( repr >> kMantissaBits ) & kExponentMask );

		if ( exponent_unadjusted == kExponentMask )
		{
			mantissa = mantissaWork;
			exponent = 0;
			flags |= kFloatFlag_InfNan;
			return;
		}

		if ( exponent_unadjusted == 0 )
		{
			if( mantissaWork == 0 )
			{
				mantissa = 0;
				exponent = 0;
				flags |= kFloatFlag_Zero;
				return;
			}
			
			// Denormalized.   Normalize for output.
			int shiftCount = 32 + ( 31 ^ Plat_BitScanReverse( mantissaWork ) );
			mantissa = uint64( mantissaWork ) << shiftCount;
			exponent = kExponentOffset - kMantissaBits + 1 - shiftCount;
			return;
		}

		// normalized, add implicit 1 bit and shift to top
		mantissa = uint64( mantissaWork | ( 1 << kMantissaBits ) ) << ( 63 - kMantissaBits );
		exponent = exponent_unadjusted + kExponentOffset - 63;
	}

	FORCEINLINE void DecodeIEEE754Double( double f, uint8& flags, int& exponent, uint64& mantissa )
	{
		uint64 repr;
		COMPILE_TIME_ASSERT( sizeof( f ) == sizeof( repr ) );
		memcpy( &repr, &f, sizeof( repr ) );

		const int kExponentOffset = -1023;
		const int kExponentBits = 11;
		const int kMantissaBits = 52;
		const uint32 kExponentMask = ( 1U << kExponentBits ) - 1;
		const uint64 kMantissaMask = ( 1ull << kMantissaBits ) - 1;

		// decode IEEE double precision float ( https://en.wikipedia.org/wiki/Double-precision_floating-point_format )
		flags = ( repr >> ( kExponentBits + kMantissaBits ) ) & 0x1; // sets kFloatFlag_Negative if sign bit set
		uint64 mantissaWork = repr & kMantissaMask;
		int exponent_unadjusted = ( ( repr >> kMantissaBits ) & kExponentMask );

		if ( exponent_unadjusted == kExponentMask )
		{
			mantissa = mantissaWork;
			exponent = 0;
			flags |= kFloatFlag_InfNan;
			return;
		}

		if ( exponent_unadjusted == 0 )
		{
			if ( mantissaWork == 0 )
			{
				mantissa = 0;
				exponent = 0;
				flags |= kFloatFlag_Zero;
				return;
			}

			// Denormalized.   Normalize for output.
			diy_fp tmp = { mantissaWork, kExponentOffset - kMantissaBits + 1 };
			tmp = DiyFP_Normalize( tmp );
			mantissa = tmp.f;
			exponent = tmp.e;
			return;
		}

		// normalized, add implicit 1 bit and shift to top
		mantissa = ( mantissaWork | ( 1ull << kMantissaBits ) ) << ( 63 - kMantissaBits );
		exponent = exponent_unadjusted + kExponentOffset - 63;
	}

	diy_fp DiyFP_NormalizedFromFloat( float f )
	{
		uint8 flags;
		int exponent;
		uint64 mantissa;

		DecodeIEEE754Single( f, flags, exponent, mantissa );

		Assert( !flags ); // don't handle negative/zero/infinity/nan

		diy_fp r = { mantissa, exponent };
		return r;
	}

	diy_fp DiyFP_NormalizedFromDouble( double d )
	{
		uint8 flags;
		int exponent;
		uint64 mantissa;

		DecodeIEEE754Double( d, flags, exponent, mantissa );

		Assert( !flags ); // don't handle negative/zero/infinity/nan

		diy_fp r = { mantissa, exponent };
		return r;
	}

	// Returns an integer k s.t. alpha <= GetCachedPower(k).e + e <= gamma
	// given gamma >= alpha+3
	//
	// The goal here is to find k such that f*10^k is close to 1, from
	// which point we can use simple integer math to compute the digits
	// of f.
	const double k1_Log2_10 = 0.30102999566398114; // 1/lg2(10)
	int Compute_K ( int e, int alpha, int gamma )
	{
		AssertInternalError( gamma >= alpha + 3 );
		return ( int )ceil( ( alpha - e + 63 ) * k1_Log2_10 );
	}

#include "floatprint_cache.inc"
	// as generated by floatprint_cache.hs
	// defines:
	//   const int kPowCacheOffset;
	//   const int kPowCacheSize;
	//   const int kPowCacheIncrement;
	//   const diy_fp kPowCache[kPowCacheSize];

	// Increments k by a value in [0, kPowCacheIncrement) and returns 10^k
	FORCEINLINE diy_fp GetCachedPower( int *pK )
	{
		int k = *pK;

		// Calculate
		//    nEntry  = ceil((k+kPowCacheOffset) / kPowCacheIncrement)
		//   nOffset = nEntry * kPowCacheIncrement  - (k+kPowCacheOffset);
		//             (between 0 and kPowCacheIncrement - 1, the amount by which we would have to increase k to get this exact entry)
		int nEntryK = k + (kPowCacheOffset + kPowCacheIncrement);	// round up here, we handle the 'exact' case by subtracting 1 below which will floor back
		const int kMaxEntryK = kPowCacheIncrement * ( kPowCacheSize - 1 );
		int nEntry = ( nEntryK - 1 ) / kPowCacheIncrement;
		int nOffset = ( kMaxEntryK - nEntryK ) % kPowCacheIncrement;

		AssertInternalError( nEntry >= 0 && nEntry < kPowCacheSize );

		// Modify k and return
		*pK += nOffset;
		return kPowCache[nEntry];
	}

	void Grisu2DigitGen( diy_fp Mp, diy_fp delta, char* buf, int* pLen, int* pK )
	{
		AssertInternalError( Mp.e == delta.e );			// check exponents match (since we ignore delta.e and just use delta.f)
		AssertInternalError( -63 <= Mp.e && Mp.e <= 0 ); // check exponent range

		// Split into 'integer' and 'fractional' part.
		diy_fp one = { 1ull << -Mp.e, Mp.e };
		uint32 p1 = Mp.f >> -Mp.e;
		uint64 p2 = Mp.f & ( one.f - 1 ); // always in range [0,1)

		// We always get a result with p1 > 0
		// which means we should always print at least 1
		// character in the first loop.
		AssertInternalError( p1 > 0 );

		int len = 0;
		uint32 div = 1000000000u; // 10^9
		int kappa = 10; // 1 + exponent above.

		// k was too large
		AssertInternalError( p1 / div < 10 );

		// print integer digits.  This loop always executes exactly 10 times
		// with div = 10^9, 10^8 .. 10^0
		while ( kappa )
		{
			kappa--;

			int d = p1 / div;
			if ( d || len )
				buf[len++] = '0' + d;
			p1 %= div;
			div /= 10;

			// Check if we have printed enough digits
			// (that is, the remaining error is less than the
			// difference between M- and M+)
			if ( ( ( ( uint64 )p1 ) << -one.e ) + p2 <= delta.f ) {
				*pLen = len;
				*pK += kappa;
				return;
			}
		}

		// We should have always printed at least one digit by now (since p1 started > 0)
		AssertInternalError( len );

		// print remaining digits 'below the decimal'
		// by repeatedly multiplybing by 10 and taking the integer part
		// until we have printed enough for the remaining error to fall below delta
		do {
			p2 *= 10;
			int d = p2 >> -one.e;
			buf[len++] = '0' + d;
			p2 &= one.f - 1;
			kappa--;
			delta.f *= 10;
		} while ( p2 > delta.f );

		*pLen = len;
		*pK += kappa;
	}

	// Inputs:
	//     x, a normalize diy_fp > 0
	//     half_ulp, represents the floating point precision used to create x.
	//     buf, a buffer of the correct size (based on the precision of x)
	// Outputs:
	//     buf, a string of digits starting with a non-zero value, null terminated.
	//     the returned value e_out is the power-of-10 exponent (for pretty-printing buf)
	// Result:
	//     reading a floating-point number of the correct precision of the string
	//         CFmtStr( "%se%d", buf, e_out )
	//     is guaranteed to match the input x
	int Grisu2( diy_fp x, uint64 half_ulp, char* buf, int* pLen )
	{
		AssertInternalError( x.f & kHighBitU64 );	// x is normalized

		// Compute x's boundaries m- and m+
		uint64 xf = x.f;
		int xe = x.e;

		// m+ is guaranteed to not overflow, and m+ is always exactly 1/2 ulp above x.
		uint64 mpf = xf + half_ulp;
		AssertInternalError( mpf > xf ); // overflow due to bad value for half_ulp

		// m- is more complicated.
		uint64 mmf;
		if ( xf == kHighBitU64 )
		{
			// minimum; delta between x and x- is 1/2 ulp.
			// Therefore m- is 1/4 ulp from x.
			// (Note that we intentionally don't renormalize here)
			mmf = xf - ( half_ulp >> 1 );
		}
		else
		{
			mmf = xf - half_ulp;
		}
		AssertInternalError( mmf < xf ); // underflow due to bad value for half_ulp

		// These values guarantee a result above 1 and less than 10^10
		// after multiplying x by 10^k
		const int alpha = -59, gamma = -32;
		int k = Compute_K( x.e + diy_fp::kQ, alpha, gamma );
		diy_fp ten_k = GetCachedPower( &k );

		diy_fp M_plus = DiyFP_Multiply( diy_fp{ mpf, xe }, ten_k );
		diy_fp M_minus = DiyFP_Multiply( diy_fp{ mmf, xe }, ten_k );
		AssertInternalError( M_plus.e == M_minus.e );
		
		// Make bounds conservative regardless of error introduced by multiply
		M_plus.f--;
		M_minus.f++;

		// since we multiplied by 10^k, we need to include a factor of 10^-k
		k = -k;

		diy_fp delta = DiyFP_Subtract( M_plus, M_minus );

		Grisu2DigitGen( M_plus, delta, buf, pLen, &k );

		buf[*pLen] = 0;
		return k;
	}
}

// void PrettyPrintFP( char* dst, int k, const char* src, int len )
void V_FPPretty( char* dst, int dstLen, const char* src, int k, int len, const FPPrettyPrintOptions& options )
{
	// Pretty print a float value with a printed integer string in `src`
	// with the target value = src * 10^k
	//
	// Precondition: 'dst' is big enough.

	// TODO: Assert( dstLen >= options.RequiredBufSize( 0, lengthUsed, LENGTH( exponent ) );

	if ( *src == '-' )
	{
		src++;
		*dst++ = '-';
		len--;
	}

	if ( k + len > options.nMaxDigitsAboveDecimal
		|| k + len < -options.nMaxZeroesBelowDecimal
		|| k < -options.nMaxBelowDecimalLength )
	{
		// scientific notation for very big/small floats
		*dst++ = *src++;
		*dst++ = '.';
		strcpy( dst, src );
		dst += ( len - 1 );
		*dst++ = 'e';
		sprintf( dst, "%d", k + len - 1 ); // for some reason there is no standard itoa()-like function
	}
	else if ( 0 <= k )
	{
		// no decimals
		strcpy( dst, src );
		dst += len;

		// add trailing zeros
		memset( dst, '0', k );
		dst += k;

		if ( options.bAlwaysIncludeDecimalPoint )
			*dst++ = '.';

		// null terminate
		*dst = 0;
	}
	else if ( k <= -len )
	{
		// value is < 1
		if(options.bDisplayLeadingZero)
			*dst++ = '0';

		*dst++ = '.';

		memset( dst, '0', -k - len );
		dst += -k - len;
		strcpy( dst, src );
	}
	else // -len < k < 0
	{
		// value is > 1 but has some decimal places
		memcpy( dst, src, len + k );
		dst += len + k;
		src += len + k;
		*dst++ = '.';
		strcpy( dst, src );
	}
}


bool V_PrintFloatRaw( float f, char* buf, int bufLen, int* pExponent, int* pLenUsed )
{
	Assert( bufLen >= kVPrintFloatRaw_BufLen );
	if ( bufLen < kVPrintFloatRaw_BufLen )
		return false;

	uint8 flags;
	int exponent;
	uint64 mantissa;

	DecodeIEEE754Single( f, flags, exponent, mantissa );

	if ( flags & kFloatFlag_InfNan )
		return false;

	int lenUsed = 0;
	if ( flags & kFloatFlag_Negative )
	{
		*buf++ = '-';
		lenUsed++;
	}

	if ( flags & kFloatFlag_Zero )
	{
		*buf++ = '0';
		*buf = 0;
		*pExponent = 0;
		*pLenUsed = lenUsed + 1;
		return true;
	}

	// We actually use slightly less than half a ULP here.  The goal is to avoid
	// double-rounding errors when reading a float as a double, then downcasting
	// back to float.  Instead of making sure the value we print rounds to the
	// correct float, we make sure the value we print rounds to a double which
	// would round to the correct float.
	const uint64 kFloatHalfUlp = ( kHighBitU64 >> 24 ) - ( kHighBitU64 >> 52 );

	int len;
	int k = Grisu2( diy_fp{ mantissa, exponent }, kFloatHalfUlp, buf, &len );
	*pLenUsed = lenUsed + len;
	*pExponent = k;
	return true;
}

void V_FPPrettySpecialFloat( char* buf, int bufLen, float f, const FPPrettyPrintOptions& options )
{
	uint32 repr;
	memcpy( &repr, &f, sizeof( f ) );

	// TODO: Check bufsize?

	if ( ( repr & 0x007fffffu ) == 0 )
	{
		// infinity
		if ( ( repr & 0x80000000u ) != 0 )
			*buf++ = '-';
		*buf++ = 'i';
		*buf++ = 'n';
		*buf++ = 'f';
		*buf = 0;
		return;
	}

	// nan
	if ( options.bDisplayNaNBits )
	{
		sprintf( buf, "nan(0x%08x)", repr );
	}
	else
	{
		*buf++ = 'n';
		*buf++ = 'a';
		*buf++ = 'n';
		*buf = 0;
	}
}

void V_PrintFloat( float f, char* buf, int bufLen )
{
	COMPILE_TIME_ASSERT( kVPrintFloat_BufLen >= kFPPrettyPrintOptionsDefault.RequiredBufSizeFloat() );
	Assert( bufLen >= kVPrintFloat_BufLen );
	if ( bufLen < kVPrintFloat_BufLen )
	{
		if ( bufLen ) *buf = 0;
		return;
	}

	char buftmp[kVPrintFloatRaw_BufLen];
	int k, len;
	if ( V_PrintFloatRaw( f, buftmp, sizeof( buftmp ), &k, &len ) )
		V_FPPretty( buf, bufLen, buftmp, k, len, kFPPrettyPrintOptionsDefault );
	else
		V_FPPrettySpecialFloat( buf, bufLen, f, kFPPrettyPrintOptionsDefault );
}


void V_PrintFloat( float f, char* buf, int bufLen, const FPPrettyPrintOptions &options )
{
	Assert( bufLen >= options.RequiredBufSizeFloat() );
	if ( bufLen < options.RequiredBufSizeFloat() )
	{
		if ( bufLen ) *buf = 0;
		return;
	}

	char buftmp[kVPrintFloatRaw_BufLen];
	int k, len;
	if ( V_PrintFloatRaw( f, buftmp, sizeof( buftmp ), &k, &len ) )
		V_FPPretty( buf, bufLen, buftmp, k, len, options );
	else
		V_FPPrettySpecialFloat( buf, bufLen, f, options );
}


bool V_PrintDoubleRaw( double f, char* buf, int bufLen, int* pExponent, int* pLenUsed )
{
	Assert( bufLen >= kVPrintDoubleRaw_BufLen );
	if ( bufLen < kVPrintDoubleRaw_BufLen )
		return false;

	uint8 flags;
	int exponent;
	uint64 mantissa;

	DecodeIEEE754Double( f, flags, exponent, mantissa );

	if ( flags & kFloatFlag_InfNan )
		return false;

	int lenUsed = 0;
	if ( flags & kFloatFlag_Negative )
	{
		*buf++ = '-';
		lenUsed++;
	}

	if ( flags & kFloatFlag_Zero )
	{
		*buf++ = '0';
		*buf = 0;
		*pExponent = 0;
		*pLenUsed = lenUsed + 1;
		return true;
	}

	const uint64 kDoubleHalfUlp = kHighBitU64 >> 53;

	int len;
	int k = Grisu2( diy_fp{ mantissa, exponent }, kDoubleHalfUlp, buf, &len );
	*pLenUsed = lenUsed + len;
	*pExponent = k;
	return true;
}

void V_FPPrettySpecialDouble( char* buf, int bufLen, double f, const FPPrettyPrintOptions& options )
{
	uint64 repr;
	memcpy( &repr, &f, sizeof( f ) );

	// TODO: Check bufsize?

	if ( ( repr & 0x000fffffffffffffull ) == 0 )
	{
		// infinity
		if ( ( repr & 0x8000000000000000ull ) != 0 )
			*buf++ = '-';
		*buf++ = 'i';
		*buf++ = 'n';
		*buf++ = 'f';
		*buf = 0;
		return;
	}

	// nan
	if ( options.bDisplayNaNBits )
	{
		sprintf( buf, "nan(0x%08x%08x)", ( uint32 )( repr >> 32 ), ( uint32 )( repr & 0xffffffffu ) );
	}
	else
	{
		*buf++ = 'n';
		*buf++ = 'a';
		*buf++ = 'n';
		*buf = 0;
	}
}

void V_PrintDouble( double f, char* buf, int bufLen )
{
	COMPILE_TIME_ASSERT( kVPrintDouble_BufLen >= kFPPrettyPrintOptionsDefault.RequiredBufSizeDouble() );
	Assert( bufLen >= kVPrintDouble_BufLen );
	if ( bufLen < kVPrintDouble_BufLen )
	{
		if ( bufLen ) *buf = 0;
		return;
	}

	char buftmp[kVPrintDoubleRaw_BufLen];
	int k, len;
	if ( V_PrintDoubleRaw( f, buftmp, sizeof( buftmp ), &k, &len ) )
		V_FPPretty( buf, bufLen, buftmp, k, len, kFPPrettyPrintOptionsDefault );
	else
		V_FPPrettySpecialDouble( buf, bufLen, f, kFPPrettyPrintOptionsDefault );
}


void V_PrintDouble( double f, char* buf, int bufLen, const FPPrettyPrintOptions &options )
{
	Assert( bufLen >= options.RequiredBufSizeDouble() );
	if ( bufLen < options.RequiredBufSizeDouble() )
	{
		if ( bufLen ) *buf = 0;
		return;
	}

	char buftmp[kVPrintDoubleRaw_BufLen];
	int k, len;
	if ( V_PrintDoubleRaw( f, buftmp, sizeof( buftmp ), &k, &len ) )
		V_FPPretty( buf, bufLen, buftmp, k, len, options );
	else
		V_FPPrettySpecialFloat( buf, bufLen, f, options );
}




#if FLOATPRINT_TEST
void GrisuTest_DiyFP()
{
	diy_fp test;

	Assert( DiyFP_NormalizedFromFloat( 1.0f ) == kDiyFPOne );

	const uint32 denorm = 0x00400000;
	float ftest;
	memcpy( &ftest, &denorm, sizeof( ftest ) );
	test = DiyFP_NormalizedFromFloat( ftest );

	Assert( DiyFP_NormalizedFromDouble( 1.0 ) == kDiyFPOne );

	// Should equal 2^-127
	Assert( test.f == kDiyFPOne.f );
	Assert( test.e == kDiyFPOne.e - 127 );

	const uint64 denorm_d = 0x0008000000000000ull;
	double dtest;
	memcpy( &dtest, &denorm_d, sizeof( dtest ) );
	test = DiyFP_NormalizedFromDouble( dtest );

	// should equal 2^-1023
	Assert( test.f == kDiyFPOne.f );
	Assert( test.e == kDiyFPOne.e - 1023 );

	Assert( DiyFP_Normalize( DiyFP_Multiply( kDiyFPOne, kDiyFPOne ) ) == kDiyFPOne );

	// 0.5 * 0.5
	test = kDiyFPOne;
	test.e--;
	test = DiyFP_Normalize( DiyFP_Multiply( test, test ) );

	// should equal 0.25 = 2^-2
	Assert( test.f == kDiyFPOne.f );
	Assert( test.e == kDiyFPOne.e - 2 );

	// 3 * 5
	diy_fp three = DiyFP_NormalizedFromFloat( 3.0f );
	diy_fp five = DiyFP_NormalizedFromDouble( 5.0 );
	diy_fp fifteen = DiyFP_NormalizedFromFloat( 15.0f );
	test = DiyFP_Normalize( DiyFP_Multiply( three, five ) );
	Assert( test == fifteen );
}

void GrisuTest_FloatPrint_Basic()
{
	char szPrintedFloat[kVPrintFloat_BufLen];

	// Some quick tests of the 'shortness' property and pretty-printer
	V_PrintFloat( 0.0f, szPrintedFloat, sizeof( szPrintedFloat ) );
	Assert( !V_strcmp( szPrintedFloat, "0" ) );
	V_PrintFloat( 1.0f, szPrintedFloat, sizeof( szPrintedFloat ) );
	Assert( !V_strcmp( szPrintedFloat, "1" ) );
	V_PrintFloat( 1000000.0f, szPrintedFloat, sizeof( szPrintedFloat ) );
	Assert( !V_strcmp( szPrintedFloat, "1000000" ) );
	V_PrintFloat( 10.0f, szPrintedFloat, sizeof( szPrintedFloat ) );
	Assert( !V_strcmp( szPrintedFloat, "10" ) );
	V_PrintFloat( 100.0f, szPrintedFloat, sizeof( szPrintedFloat ) );
	Assert( !V_strcmp( szPrintedFloat, "100" ) );
	V_PrintFloat( 12.5f, szPrintedFloat, sizeof( szPrintedFloat ) );
	Assert( !V_strcmp( szPrintedFloat, "12.5" ) );
	V_PrintFloat( 0.1f, szPrintedFloat, sizeof( szPrintedFloat ) );
	Assert( !V_strcmp( szPrintedFloat, "0.1" ) );
	V_PrintFloat( 0.7f, szPrintedFloat, sizeof( szPrintedFloat ) );
	Assert( !V_strcmp( szPrintedFloat, "0.7" ) );
	V_PrintFloat( 8388608.0f, szPrintedFloat, sizeof( szPrintedFloat ) );
	Assert( !V_strcmp( szPrintedFloat, "8388608" ) );
	V_PrintFloat( 1248.9875f, szPrintedFloat, sizeof( szPrintedFloat ) );
	Assert( !V_strcmp( szPrintedFloat, "1248.9876" ) ); // this is the same float as 1248.9875
}

void GrisuTest_FloatPrint_EdgeCases()
{
	// Test cases that were broken due to atof rounding to nearest double andGet then rounding that to the nearest float
	// We fixed these by slightly narrowing the conservative range for reading.

	uint32 kBadFloats[] = {
		0x0a4170a7u,
		0x152e43fdu,
		0x15ae43fdu,
		0x162e43fdu,
		0x16ae43fdu,
		0x172e43fdu,
		0x78fee4afu,
		0x797ee4afu,
	};

	for ( int iTest = 0; iTest < V_ARRAYSIZE( kBadFloats ); ++iTest )
	{
		uint32 repr = kBadFloats[iTest];
		float f;
		memcpy( &f, &repr, sizeof( f ) );
		char szTest[kVPrintFloat_BufLen];
		V_PrintFloat( f, szTest, sizeof( szTest ) );

		float fRes = atof( szTest );
		uint32 reprRes;
		memcpy( &reprRes, &fRes, sizeof( fRes ) );

		Assert( reprRes == repr );
	}
}

void GrisuTest_DoublePrint_Basic()
{
	// specific test cases for doubles
	uint64 kTestDoubles[] = {
		0x0000000000000001ull,	// minimum denormalized positive value
		0x7fefffffffffffffull, // maximum normal (non-inf/nan) positive value
	};

	for ( int iTest = 0; iTest < V_ARRAYSIZE( kTestDoubles ); ++iTest )
	{
		uint64 repr = kTestDoubles[iTest];
		double f;
		memcpy( &f, &repr, sizeof( f ) );
		char szTest[kVPrintDouble_BufLen];
		V_PrintDouble( f, szTest, sizeof( szTest ) );

		double fRes = atof( szTest );
		uint64 reprRes;
		memcpy( &reprRes, &fRes, sizeof( fRes ) );

		Assert( reprRes == repr );
	}

#if 0
	// Triggers an MSVCRT assert, not in this code at all.
	const char* kTestString = "-1.191042159666863e-308";
	double testdbl = atof( kTestString );
	char buf[100];
	sprintf( buf, "%.17g", testdbl );
#endif
}

void GrisuTest_FloatPrint_Exhaustive()
{
	FILE* pLogFile = fopen( "d:\\dev\\csgo\\trunk\\exhaustive_float.txt", "w" );

	DevMsg( "starting exhaustive test of float printing, this will take a long time\n" );

	char szPrintedFloat[kVPrintFloat_BufLen];
	char maxLenStr[kVPrintFloat_BufLen] = { 0 };
	int maxLen = 0;
	uint32 maxLenR = 0;
	float maxLenF = 0;

	uint32 repr, repr_out;
	float f, f_out;

	for ( uint32 exponent = 0; exponent < 255; ++exponent )
	{
		// Get some "random" mantissas so we can DevMsg their results to show progress
		extern unsigned FASTCALL HashInt( const int );
		uint32 rand1 = ( HashInt( exponent + 0 ) << 16 ) ^ HashInt( exponent + 1 );
		uint32 rand2 = ( HashInt( exponent + 2 ) << 16 ) ^ HashInt( exponent + 3 );
		uint32 rand3 = ( HashInt( exponent + 4 ) << 16 ) ^ HashInt( exponent + 5 );
		uint32 rand4 = ( HashInt( exponent + 6 ) << 16 ) ^ HashInt( exponent + 7 );
		rand1 &= ( 1 << 23 ) - 1;
		rand2 &= ( 1 << 23 ) - 1;
		rand3 &= ( 1 << 23 ) - 1;
		rand4 &= ( 1 << 23 ) - 1;

		for ( uint32 mantissa = 0; mantissa < ( 1 << 23 ); ++mantissa )
		{
			repr = ( exponent << 23 ) | mantissa;
			memcpy( &f, &repr, sizeof( f ) );

			V_PrintFloat( f, szPrintedFloat, sizeof( szPrintedFloat ) );
			int len = strlen( szPrintedFloat );
			if ( len > maxLen )
			{
				maxLen = len;
				maxLenR = repr;
				maxLenF = f;
				strcpy( maxLenStr, szPrintedFloat );
			}

			// Read back in
			f_out = atof( szPrintedFloat );
			memcpy( &repr_out, &f_out, sizeof( f ) );

			if ( repr_out != repr )
			{
				DevMsg( "Error: 0x%08x (%f) -> '%s' -> 0x%08x (%f)\n", repr, f, szPrintedFloat, repr_out, f_out );
				fprintf( pLogFile, "Error: 0x%08x (%f) -> '%s' -> 0x%08x (%f)\n", repr, f, szPrintedFloat, repr_out, f_out );
			}

			if ( mantissa == rand1 || mantissa == rand2 || mantissa == rand3 || mantissa == rand4 )
			{
				DevMsg( "Sample: 0x%08x (%f) -> '%s' -> 0x%08x (%f)\n", repr, f, szPrintedFloat, repr_out, f_out );
			}
		}

		DevMsg( "exponent %d complete\n", exponent );
	}

	maxLen += 2; // 2 characters required for negative values + null string terminator
	DevMsg( "test complete.\nMaxlen %d from 0x%08x (%g) -> '-%s'\n", maxLen, 0x80000000u | maxLenR, -maxLenF, maxLenStr );
	fprintf( pLogFile, "test complete.\nMaxlen %d from 0x%08x (%g) -> '-%s'\n", maxLen, 0x80000000u | maxLenR, -maxLenF, maxLenStr );
	fclose( pLogFile );
}

void GrisuTest_DoublePrint_Forever( uint64 startMillion )
{
	FILE* pLogFile = fopen( "d:\\dev\\csgo\\trunk\\double_test.txt", "a" );

	fprintf( pLogFile, "\nstarting new batch of tests...\n" );
	fflush( pLogFile );

	DevMsg( "starting basically infinite test of double printing, kill the process when you are tired\n" );

	char szPrintedDouble[kVPrintDouble_BufLen];
	int maxLen = 0;
	uint64 repr, repr_out;
	double f, f_out;

	const uint64 incr = 0xcbf29ce484222325ull;	// fnv offset basis seems as good as any for mixing bits
	const uint64 kMillion = 1000000;
	uint64 numTests = kMillion * startMillion; // resume as if we had already done this many million tests.

	for ( repr = numTests * incr; ++numTests; repr += incr )
	{
		// skip infinity/nan
		if ( ( ( repr >> 52 ) & 0x7ff ) == 0x7ff )
			continue;

		// Write out
		memcpy( &f, &repr, sizeof( f ) );
		V_PrintDouble( f, szPrintedDouble, sizeof( szPrintedDouble ) );
		int len = strlen( szPrintedDouble ) + 1; // +1 for null character at the end
		if ( len > maxLen )
		{
			maxLen = len;
			DevMsg( "Maxlen %d: 0x%08x%08x (%.17g) -> '%s'\n", maxLen, ( uint32 )( repr >> 32 ), ( uint32 )( repr & 0xffffffffull ), f, szPrintedDouble );
			fprintf( pLogFile, "Maxlen %d: 0x%08x%08x (%.17g) -> '%s'\n", maxLen, ( uint32 )( repr >> 32 ), ( uint32 )( repr & 0xffffffffull ), f, szPrintedDouble );
			fflush( pLogFile );
		}

		// Read back in
		f_out = atof( szPrintedDouble );
		memcpy( &repr_out, &f_out, sizeof( f ) );
		if ( repr_out != repr )
		{
			DevMsg( "Error: 0x%08x%08x (%.17g) -> '%s' -> 0x%08x%08x (%.17g)\n",
				( uint32 )( repr >> 32 ), ( uint32 )( repr & 0xffffffffull ), f,
				szPrintedDouble,
				( uint32 )( repr_out >> 32 ), ( uint32 )( repr_out & 0xffffffffull ), f_out );
			fprintf( pLogFile, "Error: 0x%08x%08x (%.17g) -> '%s' -> 0x%08x%08x (%.17g)\n",
				( uint32 )( repr >> 32 ), ( uint32 )( repr & 0xffffffffull ), f,
				szPrintedDouble,
				( uint32 )( repr_out >> 32 ), ( uint32 )( repr_out & 0xffffffffull ), f_out );
			fflush( pLogFile );
		}

		// print status every 10 million tests
		if ( ( numTests % ( 10 * kMillion ) ) == 0 )
		{
			DevMsg( "test %lldM: 0x%08x%08x (%.17g) -> '%s'\n", numTests / kMillion, ( uint32 )( repr >> 32 ), ( uint32 )( repr & 0xffffffffull ), f, szPrintedDouble );
			fprintf( pLogFile, "test %lldM: 0x%08x%08x (%.17g) -> '%s'\n", numTests / kMillion, ( uint32 )( repr >> 32 ), ( uint32 )( repr & 0xffffffffull ), f, szPrintedDouble );
			fflush( pLogFile );
		}
	}

	DevMsg( "you looped 2^64 times, did the universe end?\n" );
	fprintf( pLogFile, "you looped 2^64 times, did the universe end?\n" );
	fclose( pLogFile );
}

void GrisuTest_DoublePrint_Benchmark()
{
	FILE* pLogFile = fopen( "d:\\dev\\csgo\\trunk\\double_bench.txt", "a" );

	fprintf( pLogFile, "\nstarting new batch of tests...\n" );
	fflush( pLogFile );

	DevMsg( "starting basically infinite test of double printing, kill the process when you are tired\n" );

	char szPrintedDouble[kVPrintDouble_BufLen];
	uint64 repr;
	double f;

	const uint64 incr = 0xcbf29ce484222325ull;	// fnv offset basis seems as good as any for mixing bits
	const uint64 kMillion = 1000000;
	uint64 numTests = 0;

	timespec ts_start;
	timespec_get( &ts_start, TIME_UTC );
	timespec ts_end;

	for ( repr = numTests * incr; ++numTests; repr += incr )
	{
		// skip infinity/nan
		if ( ( ( repr >> 52 ) & 0x7ff ) == 0x7ff )
			continue;

		// Write out
		memcpy( &f, &repr, sizeof( f ) );
		V_PrintDouble( f, szPrintedDouble, sizeof( szPrintedDouble ) );	// ~890 ns
		//sprintf( szPrintedDouble, "%.17g", f );						// for comparison, ~7650 ns

		// print status every 100 million tests
		if ( ( numTests % ( 10 * kMillion ) ) == 0 )
		{
			timespec_get( &ts_end, TIME_UTC );
			int64 s = ts_end.tv_sec - ts_start.tv_sec;
			int64 ns = ts_end.tv_nsec - ts_start.tv_nsec;
			ns += s * 1000000000i64;
			ns /= 10 * kMillion;

			DevMsg( "test %lldM: %lld ns\n", numTests / kMillion, ns );
			fprintf( pLogFile, "test %lldM: %lld ns\n", numTests / kMillion, ns );
			fflush( pLogFile );

			timespec_get( &ts_start, TIME_UTC );
		}
	}

	DevMsg( "you looped 2^64 times, did the universe end?\n" );
	fprintf( pLogFile, "you looped 2^64 times, did the universe end?\n" );
	fclose( pLogFile );

}

void Test_Grisu()
{
	bool gbTestFloatsExhaustively = false;
	bool gbTestDoublesForever = false;
	bool gbBenchmarkDoubles = false;

	GrisuTest_DiyFP();
	GrisuTest_FloatPrint_Basic();
	GrisuTest_FloatPrint_EdgeCases();
	GrisuTest_DoublePrint_Basic();
	if ( gbTestFloatsExhaustively )
		GrisuTest_FloatPrint_Exhaustive();

	if ( gbTestDoublesForever )
	{
		//GrisuTest_DoublePrint_Forever( 0 ); // start over
		GrisuTest_DoublePrint_Forever( 19480 ); // resume at 19.48 billion tests
	}

	if ( gbBenchmarkDoubles )
		GrisuTest_DoublePrint_Benchmark();
}
#endif // FLOATPRINT_TEST
