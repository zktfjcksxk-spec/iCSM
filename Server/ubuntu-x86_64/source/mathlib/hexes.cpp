//////////////////////////////////////////////////////////////////////////
// hexes.cpp
//

#include "mathlib/hexes.h"
#include "mathlib/mathlib.h" // SinCos

// Calculate the position of a hex corner on a hex of size 1 given the corner index
// TODO: MAKE CONSTEXPR ARRAY
Vector2D HexCorner( int iCorner )
{
	float angle = DEG2RAD( 60 * iCorner + 30 );
	float s, c;

	SinCos( angle, &s, &c );
	return Vector2D( c * kHexRadius, s * kHexRadius );
}

// Round to center of nearest hex
CHexCoord CHexCoordF::Round() const
{
	float ql = q;
	float rl = r;
	float sl = -( ql + rl );
	
	float qr = floorf( ql + 0.5f );
	float rr = floorf( rl + 0.5f );
	float sr = floorf( sl + 0.5f );

	float qd = fabsf( ql - qr );
	float rd = fabsf( rl - rr );
	float sd = fabsf( sl - sr );

	int qi = ( int )qr;
	int ri = ( int )rr;
	int si = ( int )sr;

	if ( qd > rd && qd > sd )
		return CHexCoord( -ri - si, ri );
	else if ( rd > sd )
		return CHexCoord( qi, -qi - si );
	else
		return CHexCoord( qi, ri );
}
