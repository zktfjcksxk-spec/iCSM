//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: C++ metaprogramming utilities for panorama / client code
//=============================================================================//

#ifndef UIMETAPROGRAMMING_H
#define UIMETAPROGRAMMING_H
#pragma once

#include "panorama/panoramacxx.h"

namespace panorama {

// Create a delegator from a method pointer
#define PANORAMA_DELEGATE(method) (decltype(panorama::MkFunctionInfo(method))::Ptr<method>())

// If you are creating a delegate to an overloaded function, specify just enough of the arguments and return type
// to uniquely identify the overload you want to delegate to.
#define PANORAMA_DELEGATE_RESOLVE(method, ...) (decltype(panorama::MkFunctionInfo<__VA_ARGS__>(method))::Ptr<method>())

// Sometimes if overloads are too similar, DELEGATE_RESOLVE fails to be able to resolve to the correct method.
// In this case, specify exactly ReturnType, ObjectType, Arguments... and we will select exactly that method.
#define PANORAMA_DELEGATE_EXPLICIT(method, ...) (panorama::MethodInfo<__VA_ARGS__>::Ptr<method>())
#define PANORAMA_DELEGATE_EXPLICIT_CONST(method, ...) (panorama::MethodConstInfo<__VA_ARGS__>::Ptr<method>())

	// Empty struct that refers to another type
	template <typename T> struct TypeProxy {
		typedef T Proxy;
	};

	// helper to generate a sequences of index to iterate
	template<int...> struct index_tuple {};

	template<int I, typename IndexTuple, typename... Types>
	struct make_indexes_impl;

	template<int I, int... Indexes, typename T, typename ... Types>
	struct make_indexes_impl < I, index_tuple<Indexes...>, T, Types... >
	{
		typedef typename make_indexes_impl<I + 1, index_tuple<Indexes..., I>, Types...>::type type;
	};

	template<int I, int... Indexes>
	struct make_indexes_impl < I, index_tuple<Indexes...> >
	{
		typedef index_tuple<Indexes...> type;
	};

	template<typename ... Types>
	struct make_indexes : make_indexes_impl < 0, index_tuple<>, Types... >
	{
	};


	// Helpers to create delegators and apply them
	template <typename RetType, typename ObjType, typename... Args>
	struct MethodInfo
	{
		template <RetType( ObjType::*Method )( Args... )>
		struct Ptr
		{
			enum { USE__PANORAMA_DELEGATE__MACRO_TO_CREATE_DELEGATES = 1 };
			enum {
				kNumArgs = sizeof...( Args ),
				kNumArgsForArray = sizeof...( Args ) == 0 ? 1 : sizeof...( Args )
			};
			typedef RetType ReturnType;
			typedef ObjType ObjectType;
			typedef std::tuple<Args...> Arguments;

			template <typename... CallArgs>
			static RetType Apply( ObjType& obj, CallArgs&&... args )
			{
				return ( obj.*Method )( panorama_forward<CallArgs>( args )... );
			}

			template <int... Indices>
			static RetType ApplyTupleIndices( ObjType& obj, std::tuple<Args...>&& args, index_tuple<Indices...> indices )
			{
				return ( obj.*Method )( panorama_move( std::get<Indices>( args ) )... );
			}

			static RetType ApplyTuple( ObjType& obj, std::tuple<Args...>&& args )
			{
				return ApplyTupleIndices( obj, panorama_move( args ), typename make_indexes<Args...>::type() );
			}
		};
	};
	template <typename RetType, typename ObjType, typename... Args>
	struct MethodConstInfo
	{
		template <RetType( ObjType::*Method )( Args... ) const>
		struct Ptr
		{
			enum { USE__PANORAMA_DELEGATE__MACRO_TO_CREATE_DELEGATES = 1 };
			enum {
				kNumArgs = sizeof...( Args ),
				kNumArgsForArray = sizeof...( Args ) == 0 ? 1 : sizeof...( Args )
			};
			typedef RetType ReturnType;
			typedef const ObjType ObjectType;
			typedef std::tuple<Args...> Arguments;

			template <typename... CallArgs>
			static RetType Apply( const ObjType& obj, CallArgs&&... args )
			{
				return ( obj.*Method )( panorama_forward<CallArgs>( args )... );
			}

			template <int... Indices>
			static RetType ApplyTupleIndices( const ObjType& obj, std::tuple<Args...>&& args, index_tuple<Indices...> indices )
			{
				return ( obj.*Method )( panorama_move( std::get<Indices>( args ) )... );
			}

			static RetType ApplyTuple( const ObjType& obj, std::tuple<Args...>&& args )
			{
				return ApplyTupleIndices( obj, panorama_move( args ), typename make_indexes<Args...>::type() );
			}
		};
	};

	template <typename RetType, typename... Args>
	struct FunctionInfo
	{
		template <RetType( *Function )( Args... )>
		struct Ptr
		{
			enum { USE__PANORAMA_DELEGATE__MACRO_TO_CREATE_FUNCTIONS = 1 };
			enum {
				kNumArgs = sizeof...( Args ),
				kNumArgsForArray = sizeof...( Args ) == 0 ? 1 : sizeof...( Args )
			};
			typedef void ObjectType;
			typedef RetType ReturnType;
			typedef std::tuple<Args...> Arguments;

			template <typename... CallArgs>
			static RetType Apply( CallArgs&&... args )
			{
				return ( *Function )( panorama_forward<CallArgs>( args )... );
			}

			template <int... Indices>
			static RetType ApplyTupleIndices( std::tuple<Args...>&& args, index_tuple<Indices...> indices )
			{
				return ( *Function )( panorama_move( std::get<Indices>( args ) )... );
			}

			static RetType ApplyTuple( std::tuple<Args...>&& args )
			{
				return ApplyTupleIndices( panorama_move( args ), typename make_indexes<Args...>::type() );
			}
		};
	};

	template <typename... Args, typename RetType, typename ObjType>
	MethodInfo<RetType, ObjType, Args...> MkFunctionInfo( RetType( ObjType::*Method )( Args... ) )
	{
		return MethodInfo<RetType, ObjType, Args...>();
	}

	template <typename... Args, typename RetType, typename ObjType>
	MethodConstInfo<RetType, ObjType, Args...> MkFunctionInfo( RetType( ObjType::*Method )( Args... ) const )
	{
		return MethodConstInfo<RetType, ObjType, Args...>();
	}

	template <typename... Args, typename RetType>
	FunctionInfo<RetType, Args...> MkFunctionInfo( RetType( *Function )( Args... ) )
	{
		return FunctionInfo<RetType, Args...>();
	}

}

#endif // UIMETAPROGRAMMING_H