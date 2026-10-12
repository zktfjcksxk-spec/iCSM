//////////////////////////////////////////////////////////////////////////
// hexes.h
//
// Implementation of functions for 'pointy hexes'
// from https://www.redblobgames.com/grids/hexagons/
//
// We use "pointy hexes" with this axis alignment:
//
//           -r
//       [4]  4  [5]      O: hex center
//    +s  _--^ ^--_  +q   0-5: corner indices
//       3         5      [0] - [5]: edge indices
//       |         |
//   [3] |    O    | [0]  ---->
//       |         |       +x
//       2         0
//    -q  ^--_ _--^  -s   +-qrs: cubical coordinate axis directions for corners
//       [2]  1  [1]       moving diagonally to the next hex in this direction is +2 of 
//           +r            the specified axis and -1 of the two other axes (or -2 / +1
//                         for the negative direction).
//            |   
//            | +y
//            v
//
// One difference between our code and the redblobgames page is that we use hex size = distance
// between hexes centers whereas they use hex size = radius of hex. So, functions that take a
// flHexSize paramater use that parameter as the distance between hex centers.
//
// I chose this representation because it means that hexes along the x axis have centers at
// exactly their hex coordinate. (e.g. the hex at [1,0] is centered at (1,0)).  It also turns
// out to simplify slightly the math for transforming 2d point <-> hex (largely because x needs
// no scaling).
//
// Both versions involve some factors of sqrt(3) in terms of figuring out how many hexes fit in
// a given 2d space.
//
// We assume the center of hex [0,0] is centered at 2d point coordinate (0,0).  You should scale/offset
// vector2d results / inputs by the actual size and position of hexes you are using.
//
// Offset coordinates are 'odd-right' [x,y].
//
// Offset coordinates are useful when storing hex data as a 2d array, but are difficult to manipulate.
//
// Convert offset coordinates to/from hex coordinates using CHexCoord::FromOffset() and
//    CHexCoord::ToOffset().
//
// OFFSET COORDINATES:
//
// [+0,+0]-[+1,+0]-[+2,+0]-[+3,+0]
//      \   /   \   /   \   /   \
//     [+0,+1]-[+1,+1]-[+2,+1]-[+3,+1]
//      /   \   /   \   /   \   /
// [+0,+2]-[+1,+2]-[+2,+2]-[+3,+2]
//      \   /   \   /   \   /   \
//     [+0,+3]-[+1,+3]-[+2,+3]-[+3,+3]
//      /   \   /   \   /   \   /
// [+0,+4]-[+1,+4]-[+2,+4]-[+3,+4]
//
// For most manipulations, you will use hex coordinates, which use 'axial' layout [q,r].
//
// AXIAL COORDINATES (CHexCoord):
//
// [+0,+0]-[+1,+0]-[+2,+0]-[+3,+0]
//      \   /   \   /   \   /   \
//     [+0,+1]-[+1,+1]-[+2,+1]-[+3,+1]
//      /   \   /   \   /   \   /
// [-1,+2]-[+0,+2]-[+1,+2]-[+2,+2]
//      \   /   \   /   \   /   \
//     [-1,+3]-[+0,+3]-[+1,+3]-[+2,+3]
//      /   \   /   \   /   \   /
// [-2,+4]-[-1,+4]-[+0,+4]-[+1,+4]
//
// Axial coordinates have the advantage that the neighbor hexes are easy to calculate:
//
//       [q,r-1]   [q+1,r-1]
//             \   /
//   [q-1,r] - [q,r] - [q+1,r]
//             /   \
//     [q-1,r+1]   [q,r+1]
//
//  In general, the horizontal axis changes r, the diagonal top-left to bottom-right axis changes q,
//  and the diagonal top-right to bottom-left axis changes q and r in opposite directions.
//
// 'Cube' coordinates [q,r,s] are just axial coordinates with s = -(q+r); that is, q+r+s = 0.
// In this system moving across any edge increments one coordinate and decrements another, leaving the third constant:
//
//      [q,r-1,s+1]    [q+1,r-1,s]
//                \   /
// [q-1,r,s+1] - [q,r,s] - [q+1,r,s-1]
//                /   \
//      [q-1,r+1,s]   [q,r+1,s-1]
//
// Cube coordinates are often the easiest to work with, but since each coordinate can be derived from
// the other two, we usually just drop s and use axial coordinates.  There are a few functions that use
// cube coordinates internally.
//
// For storing hex positions, use CHexCoord.  For storing points that might be inside of hexes that you
// want to manipulate, using hex-coord math, you can use CHexCoordF, which is a floating point version
// of axial coordinates.
//
// For example, the shortest distance between two Vector2D points when movement is restricted to
// the 6 valid hex directions can be calculated like this:
//     float hexLength = ( CHexCoordF(p1) - CHexCoordF(p2) ).HexLength()
// or
//     float hexLength = CHexCoordF( p1 - p2 ).HexLength()
//
//////////////////////////////////////////////////////////////////////////
//
// Thought for future work: we could get reduce of sqrt(3) inaccuracy in point data by postponing it
// to user code and making hexes 'stretched' in some way. For example, we could make the distance vertical
// hex rows be 7/8 which is 'close' to the correct answer and allow the user to correct it if they
// so desire.  Or we could stretch them slightly to 1 and get access to doubled coordinate indices
// 'for free'?
//
// The latter would make the distance between horizontal hex centers = 1, and uncorrected vertical hex
// centers = sqrt(5)/2 ~= 1.12.
//      


#ifndef MATHLIB_HEXES_H
#define MATHLIB_HEXES_H

#include "mathlib/vector2d.h"

template <typename T>
struct MathUtils // Some minor type-independent math functions
{
	static T Abs( T val ) { return ( val < 0 ) ? ( -val ) : val; }

	// These implementations of Max/Min return their leftmost argument when the arguments are equal
	static T Max( T a, T b ) { return ( a < b ) ? b : a; }
	static T Min( T a, T b ) { return ( b < a ) ? b : a; }
};

// useful constants
constexpr float kSqrt3F = 1.732050807f;
constexpr float kHexRadius = kSqrt3F / 3.0f;
constexpr float kHexHeight = kSqrt3F / 2.0f;	// distance between hex centers along Y axis

// Inflate constants can be used when calculating hex size to force the grid to fit entirely within a given space
constexpr float kHexInflateX = ( 1.0f / 2.0f ); // offset row takes up one half extra hex
constexpr float kHexInflateY = ( 1.0f / 3.0f );	// 'pointy top/bottom' of hex take 1/3 of an extra hex height

// hex directions in clockwise order
enum EHexEdge {
	EHexEdge_East,			// +x
	EHexEdge_Southeast,		// +x, +y
	EHexEdge_Southwest,		// -x, +y
	EHexEdge_West,			// -x
	EHexEdge_Northwest,		// -x, -y
	EHexEdge_Northeast,		// +x, -y

	EHexEdge_Count
};

//////////////////////////////////////////////////////////////////////////
// CHexCoordBase: implementation of axial coordinate system for hexes
template <typename Coord>
class CHexCoordBase
{
public:
	Coord q;
	Coord r;

	constexpr CHexCoordBase() : q( 0 ), r( 0 ) {}
	constexpr CHexCoordBase( Coord q_, Coord r_ ) : q( q_ ), r( r_ ) {}

	// Calculate s for cube coordinates
	Coord s() const { return ( -q ) + ( -r ); }

	// Like "manhattan length" for vectors, this determines the shortest length 
	// (in hexes) from the origin to this point moving only in hex-aligned
	// directions.
	Coord HexLength() const;

	// Converts hex coordinate to point assuming hexes have diameter 1
	// (multiply by hex size to get actual point)
	Vector2D ToPoint() const;

	// Converts hex coordinate to point
	Vector2D ToPoint( float flHexSize ) const;

	// Tells us the edge this coordinate is pointing towards from the origin
	// (or -1 if at the origin)
	int NearestEdgeIndex() const;

protected:
	typedef MathUtils<Coord> Utils;
};

class CHexCoord;
class CHexCoordF;

// CHexCoord
// Hex centers / indices
class CHexCoord : public CHexCoordBase<int>
{
public:
	constexpr CHexCoord() : CHexCoordBase<int>() {}
	constexpr CHexCoord( int q, int r ) : CHexCoordBase<int>( q, r ) {}

	// Finds nearest hex to 2d point
	static CHexCoord RoundFromPoint( const Vector2D& point );

	// Finds nearest hex to 2d point
	static CHexCoord RoundFromPoint( const Vector2D& point, float flHexSize );

	// Converts from odd-r offset coordinates to axial coordinates
	// with offset(0,0) == axial(0,0)
	static CHexCoord FromOffset( int col, int row );

	// Converts to odd-r offset coordinates
	// with offset(0,0) == axial(0,0)
	void ToOffset( int& col, int& row ) const;

	// Get a neighbor for this hex (clockwise from east
	CHexCoord Neighbor( int edgeIndex ) const;

	// vector-style operators
	CHexCoord operator-() const { return CHexCoord( -q, -r ); }
	CHexCoord operator+( const CHexCoord& rhs ) const { return CHexCoord( q + rhs.q, r + rhs.r ); }
	CHexCoord operator-( const CHexCoord& rhs ) const { return CHexCoord( q - rhs.q, r - rhs.r ); }
	CHexCoord operator*( int dist ) const { return CHexCoord( q * dist, r * dist ); }
	CHexCoord& operator+=( const CHexCoord& rhs ) { q += rhs.q; r += rhs.r; return *this; }
	CHexCoord& operator-=( const CHexCoord& rhs ) { q -= rhs.q; r -= rhs.r; return *this; }
	CHexCoord& operator*=( int dist ) { q *= dist; r *= dist; return *this; }
};

// arbitrary points in 2d space in axial coordinates
class CHexCoordF : public CHexCoordBase<float>
{
public:
	constexpr CHexCoordF() : CHexCoordBase<float>() {}
	constexpr CHexCoordF( float q, float r ) : CHexCoordBase<float>( q, r ) {}
	constexpr CHexCoordF( const CHexCoord& hex ) : CHexCoordBase<float>( hex.q, hex.r ) {}

	// Converts point to hex coordinate assuming hexes of diameter 1 (radius 0.5)
	explicit CHexCoordF( const Vector2D& point );
	CHexCoordF( const Vector2D& point, float flHexSize );

	// Move to nearest hex center
	CHexCoord Round() const;

	// vector-style operators
	CHexCoordF operator+( const CHexCoordF& rhs ) const { return CHexCoordF( q + rhs.q, r + rhs.r ); }
	CHexCoordF operator-( const CHexCoordF& rhs ) const { return CHexCoordF( q - rhs.q, r - rhs.r ); }
	CHexCoordF operator-() const { return CHexCoordF( -q, -r ); }
	CHexCoordF operator*( float dist ) const { return CHexCoordF( q * dist, r * dist ); }
	CHexCoordF operator/( float dist ) const { return CHexCoordF( q / dist, r / dist ); }
	CHexCoordF& operator+=( const CHexCoordF& rhs ) { q += rhs.q; r += rhs.r; return *this; }
	CHexCoordF& operator-=( const CHexCoordF& rhs ) { q -= rhs.q; r -= rhs.r; return *this; }
	CHexCoordF& operator*=( float dist ) { q *= dist; r *= dist; return *this; }
	CHexCoordF& operator/=( float dist ) { q /= dist; r /= dist; return *this; }
};

constexpr CHexCoord kHexDirection[EHexEdge_Count] = {
	CHexCoord( 1, 0 ),  CHexCoord( 0,1 ),  CHexCoord( -1,1 ), 
	CHexCoord( -1, 0 ), CHexCoord( 0,-1 ), CHexCoord( 1,-1 ), 
};

// Return the position of a particular corner for the hex of diameter 1 centered at 0,0
Vector2D HexCorner( int iCorner );

//////////////////////////////////////////////////////////////////////////
// inline implementations

template <typename Coord>
inline Coord CHexCoordBase<Coord>::HexLength() const 
{
	Coord q_len = Utils::Abs( q );
	Coord r_len = Utils::Abs( r );
	Coord s_len = Utils::Abs( q + r ); // same as Abs(s()) = Abs(-(q+r))
	return Utils::Max( Utils::Max( q_len, r_len ), s_len );
}

template <typename Coord>
inline Vector2D CHexCoordBase<Coord>::ToPoint() const
{
	float x = q + 0.5f * r;
	float y = ( 0.5f * kSqrt3F ) * r;

	return Vector2D( x, y );
}

template <typename Coord>
inline Vector2D CHexCoordBase<Coord>::ToPoint( float flHexSize ) const
{
	return ToPoint() * flHexSize;
}

// what direction is closest to the target
template <typename Coord>
int CHexCoordBase<Coord>::NearestEdgeIndex() const
{
	// choose minimum cubical coordinate
	Coord qd = Utils::Abs( q );
	Coord rd = Utils::Abs( r );
	Coord sd = Utils::Abs( q + r ); // = Abs(-s()) = Abs(s())

	if ( qd < rd && qd < sd )		// q is closest to 0
		return ( r < 0 ) ? EHexEdge_Northwest : EHexEdge_Southeast;
	else if ( rd < sd )				// r is closest to 0
		return ( q < 0 ) ? EHexEdge_West : EHexEdge_East;
	else                            // s is closest to 0
		return ( q < 0 ) ? EHexEdge_Southwest : EHexEdge_Northeast;
}


inline CHexCoordF::CHexCoordF( const Vector2D& point )
{
	q = point.x - ( kSqrt3F / 3.0f ) * point.y;
	r = ( kSqrt3F * 2.0f / 3.0f ) * point.y;
}

inline CHexCoordF::CHexCoordF( const Vector2D& point, float flHexSize )
{
	q = ( point.x - ( kSqrt3F / 3.0f ) * point.y ) / flHexSize;
	r = ( ( kSqrt3F * 2.0f / 3.0f ) * point.y ) / flHexSize;
}

inline /*static*/ CHexCoord CHexCoord::RoundFromPoint( const Vector2D& point )
{
	return CHexCoordF( point ).Round();
}

inline /*static*/ CHexCoord CHexCoord::RoundFromPoint( const Vector2D& point, float flHexSize )
{
	return CHexCoordF( point, flHexSize ).Round();
}

inline /*static*/ CHexCoord CHexCoord::FromOffset( int col, int row )
{
	int q = col - ( row - ( row & 1 ) ) / 2;
	int r = row;

	return CHexCoord( q, r );
}

inline CHexCoord CHexCoord::Neighbor( int neighborIdx ) const
{
	Assert( neighborIdx >= 0 && neighborIdx < EHexEdge_Count );
	return ( *this ) + kHexDirection[neighborIdx];
}

inline void CHexCoord::ToOffset( int& outCol, int& outRow ) const
{
	int col = q + ( r - ( r & 1 ) ) / 2;
	int row = r;

	outCol = col;
	outRow = row;
}

#endif // MATHLIB_HEXES_H