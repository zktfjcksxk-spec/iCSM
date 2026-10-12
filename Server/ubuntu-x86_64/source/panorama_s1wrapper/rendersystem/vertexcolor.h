//==== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======//
//
// Purpose: A color format which works on 360 + PC
//
//===========================================================================//

#ifndef VERTEXCOLOR_H
#define VERTEXCOLOR_H
#pragma once

#include "tier0/platform.h"
#include "color.h"
#include "mathlib/vector4d.h"


//-----------------------------------------------------------------------------
// The challenge here is to make a color struct that works both on the 360
// and PC, since the 360 is big-endian vs the PC which is little endian.
//-----------------------------------------------------------------------------

// To silence the non-standard extension warning.
#pragma warning( disable : 4201 )

struct VertexColor_t
{
	// NOTE: This constructor is explicitly here to disallow initializers
	// with initializer lists i.e. 
	//     VertexColor_t color = { 255,   0,   0, 255 };
	// which will totally fail on the 360.
	VertexColor_t() {};
	VertexColor_t( const VertexColor_t &src );
	VertexColor_t( uint8 ir, uint8 ig, uint8 ib, uint8 ia=255 );
	VertexColor_t( const Color &src );

	// convert from a Vector. maps 0..1 to 0..255
	VertexColor_t( Vector const &vecColor );
	VertexColor_t( Vector4D const &vecColor );
	VertexColor_t( Vector const &vecColor, float flAlpha );

	void Init( uint8 ir, uint8 ig, uint8 ib, uint8 ia=255 );

	void InitFromPackedARGB( uint32 nARGB )
	{
		a = ( ( nARGB >> 24 ) & 0xff );
		r = ( ( nARGB >> 16 ) & 0xff );
		g = ( ( nARGB >> 8 ) & 0xff );
		b = ( ( nARGB >> 0 ) & 0xff );
	}
	
	// assign and copy by using the whole register rather than byte-by-byte copy. 
	// (No, the compiler is not smart enough to do this for you. /FAcs if you 
	// don't believe me.)
	uint32 AsUint32() const; 
	uint32 *AsUint32Ptr();
	const uint32 *AsUint32Ptr() const; 

	uint8 &operator[]( int i );
	const uint8 &operator[]( int i ) const;

	// assignment
	VertexColor_t &operator=( const VertexColor_t &src );
	VertexColor_t &operator=( const color32 &src );
	VertexColor_t &operator=( const Color &src );

	// comparison
	bool operator==( const VertexColor_t &src ) const;
	bool operator!=( const VertexColor_t &src ) const;

	// NOTE: Interestingly, VertexColor_t is not 4-byte aligned when inserted
	// into structures like vertex definition structures without this union.
	// If you just list 4 uint8s, it will actually try to pack them in next to
	// adjacent uint8-sized things. This is terrible for directX, since it
	// requires each field to be 4-byte aligned. Therefore, U added the uint32
	// to force 4-byte alignment.
	union
	{
		uint32 color;
		struct
		{
#ifdef PLATFORM_X360
			// 360 is little endian
			uint8 a, b, g, r;
#else
			uint8 r, g, b, a;
#endif
		};
	};
};

#pragma warning( default : 4201 )

//-----------------------------------------------------------------------------
// Constructors
//-----------------------------------------------------------------------------
inline VertexColor_t::VertexColor_t( const VertexColor_t &src )
{
	*AsUint32Ptr() = src.AsUint32();
}

inline VertexColor_t::VertexColor_t( uint8 ir, uint8 ig, uint8 ib, uint8 ia )
{
	 r = ir;
	 g = ig;
	 b = ib;
	 a = ia;
}

inline VertexColor_t::VertexColor_t( const Color &src )
{
	r = src.r();
	g = src.g();
	b = src.b();
	a = src.a();
}


inline void VertexColor_t::Init( uint8 ir, uint8 ig, uint8 ib, uint8 ia )
{
	r = ir;
	g = ig;
	b = ib;
	a = ia;
}

/// convert from Vector
inline VertexColor_t::VertexColor_t( Vector const &vecColor )
{
	r = (uint8)clamp( 255.0 * vecColor.x, 0, 255 );
	g = (uint8)clamp( 255.0 * vecColor.y, 0, 255 );
	b = (uint8)clamp( 255.0 * vecColor.z, 0, 255 );
	a = 255;
}

inline VertexColor_t::VertexColor_t( Vector4D const &vecColor )
{
	r = (uint8)clamp( 255.0 * vecColor.x, 0, 255 );
	g = (uint8)clamp( 255.0 * vecColor.y, 0, 255 );
	b = (uint8)clamp( 255.0 * vecColor.z, 0, 255 );
	a = (uint8)clamp( 255.0 * vecColor.w, 0, 255 );
}

inline VertexColor_t::VertexColor_t( Vector const &vecColor, float flAlpha )
{
	r = (uint8)clamp( 255.0 * vecColor.x, 0, 255 );
	g = (uint8)clamp( 255.0 * vecColor.y, 0, 255 );
	b = (uint8)clamp( 255.0 * vecColor.z, 0, 255 );
	a = (uint8)clamp( 255.0 * flAlpha, 0, 255 );
}

inline uint8 &VertexColor_t::operator[]( int i )
{
	AssertDbg( i >= 0 && i <= 3 );
	return ((uint8*)(&color))[i];
}

inline const uint8 &VertexColor_t::operator[]( int i ) const
{
	AssertDbg( i >= 0 && i <= 3 );
	return ((const uint8*)(&color))[i];
}

//-----------------------------------------------------------------------------
// Cast to int
//-----------------------------------------------------------------------------
inline uint32 VertexColor_t::AsUint32() const
{ 
	return *reinterpret_cast<const uint32*>( this ); 
}

inline uint32 *VertexColor_t::AsUint32Ptr() 
{ 
	return reinterpret_cast<uint32*>( this ); 
}

inline const uint32 *VertexColor_t::AsUint32Ptr() const 
{ 
	return reinterpret_cast<const uint32*>( this ); 
} 


//-----------------------------------------------------------------------------
// assignment
//-----------------------------------------------------------------------------
inline VertexColor_t &VertexColor_t::operator=( const VertexColor_t &src )
{
	*AsUint32Ptr() = src.AsUint32();
	return *this;
}

inline VertexColor_t &VertexColor_t::operator=( const color32 &src )
{
	r = src.r;
	g = src.g;
	b = src.b;
	a = src.a;
	return *this;
}

inline VertexColor_t &VertexColor_t::operator=( const Color &src )
{
	r = src.r();
	g = src.g();
	b = src.b();
	a = src.a();
	return *this;
}


//-----------------------------------------------------------------------------
// comparison
//-----------------------------------------------------------------------------
inline bool VertexColor_t::operator==( const VertexColor_t &src ) const
{
	return AsUint32() == src.AsUint32();
}

inline bool VertexColor_t::operator!=( const VertexColor_t &src ) const
{
	return AsUint32() != src.AsUint32();
}



#endif // VERTEXCOLOR_H
