//========== Copyright © 2018, Valve Corporation, All rights reserved. ========
//
// Purpose: Simple macros to defer execution of some code until the end
//          of the current scope
//
// Examples:
//	void Example1()
//	{
//		// open a file
//		FILE* f = fopen("config.txt", "r");
//		if(!f) return;
//		DEFER( { fclose(f); } );
//		
//		if ( !ReadHeader(f) )
//		     return; // f will be closed here
//		
//		DoSomething( f );
//	} // f will also be closed here
//    
//	Obj* Example2()
//  {
//		Obj* p = new Obj;
//		DEFER_NAMED( deleteP, { delete p; } );
//
//      if( !Init(p) )
//			return nullptr;		// p will be deleted here
//
//      if( !AnotherInit(p) )
//			return nullptr;		// p will also be deleted here
//
//		deleteP.Disable();		// disable deferred code
//		return p;				// p will *not* be deleted here
//	}
//
//=============================================================================

#ifndef TIER1_DEFER_H
#define TIER1_DEFER_H

#if defined( _WIN32 )
#pragma once
#endif

#include  "tier0/platform.h"

#if !VALVE_CPP11
#error "defer relies on c++11 features (lambda, move)"
#endif

#define DEFER_CONCAT2( x, y ) x ## y
#define DEFER_CONCAT( x, y ) DEFER_CONCAT2( x, y )

//////////////////////////////////////////////////////////////////////////
// DEFER and DEFER_NAMED will execute the attached block when the current scope ends.
//
// Usage:
//    DEFER( { code } );
// or
//    DEFER_NAMED( varName, { code } );
//
// With DEFER_NAMED, you can use
//    varName.Disable();
// to stop the code from executing when the scope ends.
//
// See the examples at the top of this file for more info.
//
#define DEFER_NAMED( name, ... )								\
	const Deferrer& name = make_deferred([&]() __VA_ARGS__);	\
	/* suppress warning about unused variable */				\
	do { (void)name; } while(0)

#define DEFER( ... ) DEFER_NAMED( DEFER_CONCAT( defer, __COUNTER__ ) , __VA_ARGS__ )

//////////////////////////////////////////////////////////////////////////
// Implementation
class Deferrer {
protected:
	Deferrer();
	~Deferrer() = default;

public:
	bool m_bEnabled;
	bool SetEnabled( bool bEnabled );
	bool Enable();
	bool Disable();
};

template <class Code>
class DeferredCode : public Deferrer
{
public:
	DeferredCode() = delete;
	explicit DeferredCode( Code&& code );
	~DeferredCode();

private:
	Code m_code;
};

FORCEINLINE bool Deferrer::SetEnabled( bool bEnabled )
{
	bool bCur = m_bEnabled;
	m_bEnabled = bEnabled;
	return bCur;
}

FORCEINLINE bool Deferrer::Enable()
{
	return SetEnabled( true );
}

FORCEINLINE bool Deferrer::Disable()
{
	return SetEnabled( false );
}

FORCEINLINE Deferrer::Deferrer()
	: m_bEnabled( true )
{
}

template <typename Code>
FORCEINLINE DeferredCode<Code>::DeferredCode( Code&& code )
	: m_code( Move( code ) )
{
}

template <typename Code>
FORCEINLINE DeferredCode<Code>::~DeferredCode()
{
	if ( m_bEnabled )
		m_code();
}

template <class Code>
FORCEINLINE DeferredCode<Code> make_deferred( Code&& code )
{
	return DeferredCode<Code>( Move( code ) );
}

#endif // TIER1_DEFER_H
