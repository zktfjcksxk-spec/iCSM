//====== Copyright © 2018, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
// Template programming utilities
//
//=============================================================================//

#ifndef TIER1_TEMPLATE_UTILS_H
#define TIER1_TEMPLATE_UTILS_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h" // some C++11 utils

#if !VALVE_CPP11
#error "template_utils.h requires a C++11 compiler"
#endif

// TODO: Remove std:: usage here?
#include <type_traits>

//////////////////////////////////////////////////////////////////////////
// TEMPLATE_REQUIRE_CONVERTIBLE(X, Y):
// Usage: Include in a template list to cause the function to only exist if X can be constructed from Y implicitly.
//
// For example:
//
//    class CSomeBaseClass {};
//    class CDerivedA : public CSomeBaseClass {};
//    class CDerivedB : public CSomeBaseClass {};
//    class COther {};
//    class CUnrelated {};

//    template <typename Derived, TEMPLATE_REQUIRE_CONVERTIBLE(CSomeBaseClass*, Derived*)>
//    void f(const Derived& x) {
//        const CSomeBaseClass *p = &x; // this line will never cause a compile error
//    }
//    void f(const COther& x) {} // regular function overload for f()
//
// f( CSomeBaseClass() ); // calls f<CSomeBaseClass>( const Derived& )
// f( CDerivedA() );      // calls f<CDerivedA>( const Derived& )
// f( CDerivedB() );      // calls f<CDerivedB>( const Derived& )
// f( COther() );         // calls overloaded f( const COther& )
// f( CUrelated() );      // compile error, no matching overload for f()

#define TEMPLATE_MACRO_CONCAT_IMPL( X, Y ) X ## Y
#define TEMPLATE_MACRO_CONCAT( X, Y ) TEMPLATE_MACRO_CONCAT_IMPL( X, Y )
#define TEMPLATE_REQUIRE_CONVERTIBLE( X, Y ) typename TEMPLATE_MACRO_CONCAT( SFINAE_ , __COUNTER__ ) = typename std::enable_if< std::is_convertible< Y, X >::value >::type
#define TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( X, Y ) typename TEMPLATE_MACRO_CONCAT( SFINAE_, __COUNTER__ )

//////////////////////////////////////////////////////////////////////////
// Utilities for removing const/volatile/references from types

template <class T> struct C11RemoveConst { typedef T Type; };
template <class T> struct C11RemoveConst<const T> { typedef T Type; };

template <class T> struct C11RemoveVolatile { typedef T Type; };
template <class T> struct C11RemoveVolatile<volatile T> { typedef T type; };

template <class T> struct C11RemoveAllQualifiers {
private:
	typedef typename C11RemoveReference<T>::Type T1;
	typedef typename C11RemoveConst<T1>::Type T2;
public:
	typedef typename C11RemoveVolatile<T2>::Type Type;
};

//////////////////////////////////////////////////////////////////////////
// FORWARD_CONSTRUCT_TYPE / FORWARD_CONSTRUCT_ARG
//
// Usage: In some cases (particularly involving type erasure), you may have a proxy class that needs to construct
//        a target class and you want to support both move & copy construction of the target class.
//
// e.g.
//
//     struct IWhatever {
//         virtual ~IWhatever() {}
//         virtual void Whatever() = 0;
//     }
//
//     template <typename T> struct CDoTheThing : public IWhatever {
//         T mVal;
//         CDoTheThing( const T& copyFrom ) : mVal(copyFrom) {}
//         CDoTheThing( T&& moveFrom ) : mVal( Move(copyFrom) ) {}
//         virtual void Whatever() OVERRIDE { mVal.DoTheThing(); }
//     };
//
//     struct CWhateverUser {
//         IWhatever* mWhatever;
//
//         template <??> CWhateverUser( ?? ) : mWhatever ( new CDoTheThing( ?? ) ) {}
//         // ... more stuff ... 
//     };
//
// Here we want the user to be able to choose any T that has a DoTheThing() function and copy/move it into the
// embedded CDoTheThing.  But we don't know or care what T is since its type is hidden inside of IWhatever.
//
// The 'obvious' (and incorrect!) answer is this:
//    template <typename T> CWhateverUser( const T& val ) : mWhatever( new CDoTheThing<T>( val ) ) {}
//    template <typename T> CWhateverUser( T&& val ) : mWhatever( new CDoTheThing<T>( Move(val) ) ) {}
//
// Unfortunately this doesn't work.  The 2nd version of the template creates a so-called 'universal reference'
// which is ambiguous with the 1st version:
//     Someclass v;
//     CWhateverUser what( v );
// In this case we intend to call the 1st template with T = Someclass, but instead we can match
// the 2nd template with T = const Someclass &, because "const Someclass&" = "const Someclass & &&"
//
// A common workaround is to not care about how the object is constructed at all, and instead use
// perfect forwarding to construct a T using *all* the arguments:
//     template <typename T, typename... Args> void f( Args&&... args )
//     {  do_something_with ( T( Forward<Args>(args)... ) ); }
//
// This has 2 problems:
//     1. the caller has to specify T directly: f<T>( T() ) since type deduction can't be used to
//        figure out what T is.
//     2. because of (1), you can't use it on constructors at all since templated constructors
//        can only be used via type deduction.
//
// Instead, one solution is this pattern:
//
//     template <typename T> CWhateverUser( T&& val )
//         : mWhatever( new CDoTheThing< [T with const, volatile, &, && removed] >
//	           ( Forward<T>( val ) ) )
//     {}
//
// This handles both cases:
//     const Someclass& ->
//        T = const Someclass&
//        calls CDoTheThing<Someclass>( static_cast<const Someclass&>( val ) )
//
//     Someclass&& ->
//        T = Someclass
//        calls CDoTheThing<Someclass>( static_cast<Someclass&&>( val ) )
//
// To simplify this pattern, these macros can be used:
//      template <typename T> CWhateverUser( T&& val )
//          : mWhatever( new CDoTheThing<FORWARD_CONSTRUCT_TYPE(T)>( FORWARD_CONSTRUCT_ARG(T, val) ) )

#define FORWARD_CONSTRUCT_TYPE( T ) typename C11RemoveAllQualifiers<T>::Type
#define FORWARD_CONSTRUCT_ARG( T, v ) Forward<T>( v )



#endif // TIER1_TEMPLATE_UTILS_H