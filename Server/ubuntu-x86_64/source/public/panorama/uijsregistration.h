
//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose:
//=============================================================================//

#ifndef UIJSREGISTRATION_H
#define UIJSREGISTRATION_H
#pragma once

#include "panorama/uievent.h"
#include "panorama/iuiengine.h"
#include "panorama/uimetaprogramming.h"

#if _GNUC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif // _GNUC

#ifdef None
#undef None // X11 defines None breaking the ability to use v8::None down below, so remove the preprocessor macro here
#endif

#pragma warning(push)
//warning C4700 : uninitialized local variable 'getcaster' used -- we'll do a lot of this, but its ok
#pragma warning( disable : 4700 )

#ifdef MEMBER_FUNCPTRS_MAXSIZE
#define V8_CALLBACK_ARRAY_SIZE 4
#else
#define V8_CALLBACK_ARRAY_SIZE 3
#endif

namespace panorama
{

	template<typename T> struct RegisterJSTypeDeducer_t { static RegisterJSType_t Type() { return k_ERegisterJSTypeUnknown; } };
#define PANORAMA_DECLARE_DEDUCE_JSTYPE( _JSType, _T ) template<> struct RegisterJSTypeDeducer_t<_T> { static RegisterJSType_t Type() { return k_ERegisterJSType##_JSType; } };
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Void, void )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Bool, bool )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Int8, int8 )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Uint8, uint8 )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Int16, int16 )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Uint16, uint16 )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Int32, int32 )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Uint32, uint32 )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Int64, int64 )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Uint64, uint64 )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Float, float )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( Double, double )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( ConstString, const char * )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( UtlString, CUtlString )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( PanoramaSymbol, CPanoramaSymbol )
	PANORAMA_DECLARE_DEDUCE_JSTYPE( RawV8Args, const v8::FunctionCallbackInfo<v8::Value>& )

	inline void JSCheckObjectValidity( const v8::FunctionCallbackInfo<v8::Value>& args )
	{
		// Don't use helper for getting panel, as it will throw exception, which we don't want in just this function
		v8::Local<v8::Object> obj = args.Holder();
		if( obj->InternalFieldCount() != 1 )
		{
			v8::Handle< v8::Boolean > value = v8::Boolean::New( args.GetIsolate(), false );
			args.GetReturnValue().Set( value );
		}

		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast( obj->GetInternalField( 0 ) );
		void *pPanel = (void*)wrap->Value();
		if( !pPanel )
		{
			v8::Handle< v8::Boolean > value = v8::Boolean::New( args.GetIsolate(), false );
			args.GetReturnValue().Set( value );
		}
		else
		{
			v8::Handle< v8::Boolean > value = v8::Boolean::New( args.GetIsolate(), true );
			args.GetReturnValue().Set( value );
		}
	}

	// Register special IsValid(), which makes the JS obj like a safe handle where you can use this
	// to check if the obj has been deleted in C++ and is gone.  You must still track all objects that are
	// created and set their internal value to 0 so they point at nothing to make this work.
	inline void RegisterJSIsValid()
	{
		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );

		v8::Local<v8::ObjectTemplate> objTemplate = UIEngine()->GetCurrentV8ObjectTemplateToSetup();
		v8::Local<v8::Signature> classSignature = UIEngine()->GetCurrentV8ClassToSetupSignature();
		objTemplate->Set( v8::String::NewFromUtf8( GetV8Isolate(), "IsValid" ).ToLocalChecked(), v8::FunctionTemplate::New( GetV8Isolate(), &JSCheckObjectValidity, v8::Undefined( GetV8Isolate() ), classSignature ) );
	}

	// Register a member to expose to JavaScript as read/write
	template< typename GetterDelegate, typename SetterDelegate >
	void RegisterJSAccessor( const char *jsMemberName, GetterDelegate delGet, SetterDelegate delSet, const char *pDesc = NULL );

	// Register a read-only member to expose to JavaScript
	template< typename GetterDelegate >
	void RegisterJSAccessorReadOnly( const char *jsMemberName, GetterDelegate delGet, const char *pDesc = NULL );

	template < typename Delegate >
	void RegisterJSMethod( const char *pchMethodName, Delegate del, const char *pDesc = NULL, const char *pArgNames = NULL );

	template <typename T>
	void RegisterJSConstantValue( const char *pchJSMemberName, T value, const char *pDesc = NULL );

	template < typename Delegate >
	void RegisterJSGlobalFunction( const char *pchJSFunctionName, Delegate del, bool bTrueGlobal = false, const char *pDesc = NULL, const char* pArgNames = NULL );


	//
	// Everything below here is internals, and should be ignored if you aren't adding additional type
	// support inside panorama itself.
	//



	template< typename ObjType> ObjType* GetThisPtrForJSCall( v8::Local<v8::Object> self )
	{
		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast( self->GetInternalField( 0 ) );

		// This unsafe cast *should* be safe because we are attaching signatures to all of our callbacks, so V8 should verify that
		// the panel being called is of the correct type before allowing the callback to happen.  See the documentation for
		// v8::FunctionTemplate and v8::Signature.
		ObjType *pPanel = (ObjType*)wrap->Value();

		if( pPanel && panorama_is_base_of< IUIPanelClient, ObjType >::value )
		{
			IUIPanel *pUIPanel = (IUIPanel*)(pPanel);
			pPanel = (ObjType*)(pUIPanel->ClientPtr());
		}

		if( !pPanel )
		{
			GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), "Underlying object is deleted!" ).ToLocalChecked() );
			return NULL;
		}
		return pPanel;
	}


	template <typename T>
	void RegisterJSConstantValue( const char *pchJSMemberName, T value, const char *pDesc )
	{
		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );

		v8::Handle<v8::ObjectTemplate> objTemplate = UIEngine()->GetCurrentV8ObjectTemplateToSetup();
		// don't need signature here as we aren't calling back into C++
		v8::Handle<v8::Value> val;

		PanoramaTypeToV8Param( value, &val );

		objTemplate->Set( v8::String::NewFromUtf8( GetV8Isolate(), pchJSMemberName ).ToLocalChecked(), val, v8::PropertyAttribute( v8::ReadOnly | v8::DontDelete ) );

		UIEngine()->NewRegisterJSEntry( pchJSMemberName, RegisterJSEntryInfo_t::k_EConstantValue, pDesc, RegisterJSTypeDeducer_t<T>::Type() );
	}


	//-----------------------------------------------------------------------------
	// Purpose: Regoster a member variable to expose to JavaScript, including a Getter
	//          and optional Setter
	//-----------------------------------------------------------------------------
	inline void RegisterJSAccessorInternal( const char *pchJSMemberName, v8::AccessorGetterCallback pGetCallback, v8::AccessorSetterCallback pSetCallback, const char *pDesc = NULL, RegisterJSType_t eDataType = k_ERegisterJSTypeUnknown )
	{
		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );

		v8::Handle<v8::ObjectTemplate> objTemplate = UIEngine()->GetCurrentV8ObjectTemplateToSetup();
		v8::Handle<v8::AccessorSignature> signature = UIEngine()->GetCurrentV8ClassToSetupAccessorSignature();
		objTemplate->SetAccessor( v8::String::NewFromUtf8( GetV8Isolate(), pchJSMemberName ).ToLocalChecked(), pGetCallback, pSetCallback, v8::Null( GetV8Isolate() ), v8::DEFAULT, v8::None, signature );

		UIEngine()->NewRegisterJSEntry( pchJSMemberName, pSetCallback ? RegisterJSEntryInfo_t::k_EAccessor : RegisterJSEntryInfo_t::k_EAccessorReadOnly, pDesc, eDataType );
	}

	template <typename GetterDelegate>
	void JSPropGetterCallback( v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info )
	{
		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );
		typename GetterDelegate::ObjectType *pPanel = GetThisPtrForJSCall<typename GetterDelegate::ObjectType>( info.Holder() );
		if ( !pPanel )
			return;

		v8::Handle< v8::Value > val;
		typename GetterDelegate::ReturnType nativeval = GetterDelegate::Apply( *pPanel );

		PanoramaTypeToV8Param( nativeval, &val );
		info.GetReturnValue().Set( val );
	}

	template <typename SetterDelegate>
	void JSPropSetterCallback( v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info )
	{
		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );
		typename SetterDelegate::ObjectType *pPanel = GetThisPtrForJSCall<typename SetterDelegate::ObjectType>( info.Holder() );
		if ( !pPanel )
			return;

		// we expect a single argument delegate here
		typename std::tuple_element<0, typename SetterDelegate::Arguments>::type nativeVal;
		V8ParamToPanoramaType( value, &nativeVal );
		SetterDelegate::Apply( *pPanel, panorama_move( nativeVal ) );

		FreeConvertedParam( &nativeVal );
	}


	//-----------------------------------------------------------------------------
	// Purpose: Register a float member to expose to JavaScript, can pass NULL for pSetFunc if this is only able to be read not written
	//-----------------------------------------------------------------------------
	template< typename GetterDelegate, typename SetterDelegate >
	void RegisterJSAccessor( const char *jsMemberName, GetterDelegate, SetterDelegate, const char *pDesc )
	{
		COMPILE_TIME_ASSERT( SetterDelegate::USE__PANORAMA_DELEGATE__MACRO_TO_CREATE_DELEGATES != 0 );
		COMPILE_TIME_ASSERT( SetterDelegate::kNumArgs == 1 );
		COMPILE_TIME_ASSERT( GetterDelegate::USE__PANORAMA_DELEGATE__MACRO_TO_CREATE_DELEGATES != 0 );
		COMPILE_TIME_ASSERT( GetterDelegate::kNumArgs == 0 );
		COMPILE_TIME_ASSERT( (std::is_same< typename GetterDelegate::ReturnType, typename std::tuple_element< 0, typename SetterDelegate::Arguments >::type >::value) );
		COMPILE_TIME_ASSERT( (std::is_same< typename SetterDelegate::ReturnType, void >::value) );
		RegisterJSAccessorInternal( jsMemberName, &JSPropGetterCallback<GetterDelegate>, &JSPropSetterCallback<SetterDelegate>, pDesc, RegisterJSTypeDeducer_t<typename GetterDelegate::ReturnType>::Type() );
	}

	// Register a read-only member to expose to JavaScript
	template< typename GetterDelegate >
	void RegisterJSAccessorReadOnly( const char *jsMemberName, GetterDelegate, const char *pDesc )
	{
		COMPILE_TIME_ASSERT( GetterDelegate::USE__PANORAMA_DELEGATE__MACRO_TO_CREATE_DELEGATES != 0 );
		COMPILE_TIME_ASSERT( GetterDelegate::kNumArgs == 0 );
		RegisterJSAccessorInternal( jsMemberName, &JSPropGetterCallback<GetterDelegate>, 0, pDesc, RegisterJSTypeDeducer_t<typename GetterDelegate::ReturnType>::Type() );
	}

	//-----------------------------------------------------------------------------
	// Purpose: Convert a v8 argument to native panorama type
	//-----------------------------------------------------------------------------
	template< typename T> bool BConvertV8ArgToPanorama( const v8::Handle<v8::Value> &val, T *pNativeVal )
	{
		v8::TryCatch try_catch( v8::Isolate::GetCurrent() );
		V8ParamToPanoramaType( val, pNativeVal );
		if( try_catch.HasCaught() )
		{
			UIEngine()->OutputJSExceptionToConsole( try_catch, UIEngine()->GetPanelForJavaScriptContext( *(GetV8Isolate()->GetCurrentContext()) ) );
			return false;
		}

		return true;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Handler for JS method call callbacks
	//-----------------------------------------------------------------------------

	// two JSMethodCallbackWrappers to handle void callbacks & callbacks with return values
	template< typename ReturnType, typename Delegate >
	struct JSMethodCallbackWrapper
	{
		static void Call( typename Delegate::ObjectType *pObject, const v8::FunctionCallbackInfo<v8::Value>& v8Args, typename Delegate::Arguments&& args )
		{
			ReturnType ret = Delegate::ApplyTuple( *pObject, panorama_move( args ) );

			v8::Handle< v8::Value > value;
			PanoramaTypeToV8Param( ret, &value );

			v8Args.GetReturnValue().Set( value );
		}
	};

	template< typename Delegate >
	struct JSMethodCallbackWrapper<void, Delegate>
	{
		static void Call( typename Delegate::ObjectType *pObject, const v8::FunctionCallbackInfo<v8::Value>& v8Args, typename Delegate::Arguments&& args )
		{
			Delegate::ApplyTuple( *pObject, panorama_move( args ) );
		}
	};

	template< typename ReturnType, typename Delegate >
	struct JSFunctionCallbackWrapper
	{
		static void Call( const v8::FunctionCallbackInfo<v8::Value>& v8Args, typename Delegate::Arguments&& args )
		{
			ReturnType ret = Delegate::ApplyTuple( panorama_move( args ) );

			v8::Handle< v8::Value > value;
			PanoramaTypeToV8Param( ret, &value );

			v8Args.GetReturnValue().Set( value );
		}
	};

	template< typename Delegate >
	struct JSFunctionCallbackWrapper<void, Delegate>
	{
		static void Call( const v8::FunctionCallbackInfo<v8::Value>& v8Args, typename Delegate::Arguments&& args )
		{
			Delegate::ApplyTuple( panorama_move( args ) );
		}
	};


	// Expands values in tuple into a single function call
	template< typename RetType, typename TupleType, class... Args, int... Indexes >
	void JSFunctionCallTuple_Helper( RetType( *pFunc )(Args...), const v8::FunctionCallbackInfo<v8::Value> &args, index_tuple< Indexes... >, TupleType &tup )
	{
		JSFunctionCallbackWrapper< RetType, Args... >::Call( pFunc, args, panorama_forward<Args>( std::get<Indexes>( tup ) )... );
	}

	// converts all v8 args into a tuple
	template< int Index, typename T >
	struct ConvertV8ArgsToTuple
	{
		static bool Call( T &tup, const v8::FunctionCallbackInfo<v8::Value> &args, bool *pbSucceeded )
		{
			bool bSucceeded = BConvertV8ArgToPanorama( args[Index - 1], &std::get< Index - 1>( tup ) );
			if ( pbSucceeded )
				pbSucceeded[Index - 1] = bSucceeded;

			if ( !bSucceeded )
				return false;

			return ConvertV8ArgsToTuple<Index - 1, T >::Call( tup, args, pbSucceeded );
		}
	};

	template< typename T >
	struct ConvertV8ArgsToTuple< 0, T >
	{
		static bool Call( T &tup, const v8::FunctionCallbackInfo<v8::Value> &args, bool *pbSucceeded )
		{
			return true;
		}
	};


	// frees all args in tuple
	template< int Index, typename T >
	struct FreeV8ArgsToTuple
	{
		static void Call( T &tup, bool *pbSucceeded )
		{
			if ( pbSucceeded && pbSucceeded[Index - 1] )
				FreeConvertedParam( &std::get< Index - 1 >( tup ) );

			FreeV8ArgsToTuple<Index - 1, T >::Call( tup, pbSucceeded );
		}
	};

	template< typename T >
	struct FreeV8ArgsToTuple < 0, T >
	{
		static void Call( T &tup, bool *pbSucceeded )
		{
		}
	};

	template< typename Delegate >
	void JSMethodCallTuple( const v8::FunctionCallbackInfo<v8::Value>& args )
	{
		v8::Isolate::Scope isolate_scope( args.GetIsolate() );
		v8::HandleScope handle_scope( args.GetIsolate() );

		if( args.Length() < Delegate::kNumArgs )
		{
			v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  args.Data()->ToString( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::String>() ) );
			GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), CFmtStr1024( "%s requires %d arguments; only %d given.", *str, (int)Delegate::kNumArgs, (int)args.Length() ).String() ).ToLocalChecked() );
			return;
		}

		typename Delegate::ObjectType *pPanel = GetThisPtrForJSCall<typename Delegate::ObjectType>( args.Holder() );
		if ( !pPanel )
			return;

		bool pbSucceeded[ Delegate::kNumArgsForArray ];
		V_memset( pbSucceeded, 0, Delegate::kNumArgs * sizeof( bool ) );

		typename Delegate::Arguments tupleArgs;
		if ( ConvertV8ArgsToTuple< Delegate::kNumArgs, typename Delegate::Arguments >::Call( tupleArgs, args, pbSucceeded ) )
			JSMethodCallbackWrapper< typename Delegate::ReturnType, Delegate >::Call( pPanel, args, panorama_move( tupleArgs ) );

		FreeV8ArgsToTuple< Delegate::kNumArgs, typename Delegate::Arguments >::Call( tupleArgs, pbSucceeded );
	}

	template< typename Delegate >
	void JSMethodCallRaw( const v8::FunctionCallbackInfo<v8::Value>& args )
	{
		v8::Isolate::Scope isolate_scope( args.GetIsolate() );
		v8::HandleScope handle_scope( args.GetIsolate() );

		typename Delegate::ObjectType *pPanel = GetThisPtrForJSCall<typename Delegate::ObjectType>( args.Holder() );
		if ( !pPanel )
			return;

		Delegate::Apply( *pPanel, args );
	}

	template< typename Delegate >
	void JSFunctionCallTuple( const v8::FunctionCallbackInfo<v8::Value>& args )
	{
		v8::Isolate::Scope isolate_scope( args.GetIsolate() );
		v8::HandleScope handle_scope( args.GetIsolate() );

		if( args.Length() < Delegate::kNumArgs )
		{
			v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  args.Data()->ToString( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::String>() ) );
			GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), CFmtStr1024( "%s requires %d arguments; only %d given.", *str, (int)Delegate::kNumArgs, (int)args.Length() ).String() ).ToLocalChecked() );
			return;
		}

		bool pbSucceeded[ Delegate::kNumArgsForArray];
		V_memset( pbSucceeded, 0, Delegate::kNumArgs * sizeof( bool ) );

		typename Delegate::Arguments tupleArgs;
		if ( ConvertV8ArgsToTuple< Delegate::kNumArgs, typename Delegate::Arguments>::Call( tupleArgs, args, pbSucceeded ) )
			JSFunctionCallbackWrapper< typename Delegate::ReturnType, Delegate>::Call( args, panorama_move( tupleArgs ) );

		FreeV8ArgsToTuple< Delegate::kNumArgs, typename Delegate::Arguments >::Call( tupleArgs, pbSucceeded );
	}

	template < typename Tuple, int index >
	struct RegisterJSTypesDeducerHelper
	{
		static void Call( RegisterJSType_t* pTypes )
		{
			pTypes[index-1] = RegisterJSTypeDeducer_t< typename std::tuple_element<index-1, Tuple>::type >::Type();
			RegisterJSTypesDeducerHelper<Tuple, index - 1>::Call( pTypes );
		}
	};

	template < typename Tuple >
	struct RegisterJSTypesDeducerHelper< Tuple, 0 >
	{
		static void Call( RegisterJSType_t* pTypes )
		{
			// Termination
		}
	};

	template< typename Tuple >
	struct RegisterJSTypesDeducer
	{
		
		static void Call( RegisterJSType_t (&pTypes)[ std::tuple_size<Tuple>::value ] )
		{
			RegisterJSTypesDeducerHelper<Tuple, std::tuple_size<Tuple>::value>::Call( pTypes );
		}

		// This overload should only get called for methods with 0 arguments
		static void Call( RegisterJSType_t( &pTypes )[std::tuple_size<Tuple>::value + 1] )
		{
			COMPILE_TIME_ASSERT( std::tuple_size<Tuple>::value == 0 );
		}
	};

	template <typename Delegate>
	void RegisterJSMethod( const char* pchMethodName, Delegate del, const char* pDesc, const char* pArgNames )
	{
		COMPILE_TIME_ASSERT( Delegate::USE__PANORAMA_DELEGATE__MACRO_TO_CREATE_DELEGATES != 0 );
		COMPILE_TIME_ASSERT( Delegate::kNumArgs <= RegisterJSEntryInfo_t::k_unMaxParams );

		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );

		v8::Handle<v8::ObjectTemplate> objTemplate = UIEngine()->GetCurrentV8ObjectTemplateToSetup();
		v8::Local<v8::Signature> classSignature = UIEngine()->GetCurrentV8ClassToSetupSignature();
		objTemplate->Set( v8::String::NewFromUtf8( GetV8Isolate(), pchMethodName ).ToLocalChecked(), v8::FunctionTemplate::New( GetV8Isolate(), &JSMethodCallTuple<Delegate>, v8::String::NewFromUtf8( GetV8Isolate(), pchMethodName ).ToLocalChecked(), classSignature ) );

		int nEntry = UIEngine()->NewRegisterJSEntry( pchMethodName, RegisterJSEntryInfo_t::k_EMethod, pDesc, RegisterJSTypeDeducer_t<typename Delegate::ReturnType>::Type() );

//#ifdef SOURCE2_PANORAMA // $$$REI Test this stuff
		RegisterJSType_t paramTypes[Delegate::kNumArgsForArray ];
		RegisterJSTypesDeducer<typename Delegate::Arguments>::Call( paramTypes );
		UIEngine()->SetRegisterJSEntryParams( nEntry, Delegate::kNumArgs, paramTypes, pArgNames );
//#else
//		REFERENCE( nEntry );
//#endif
	}

	template < typename Delegate >
	void RegisterJSGlobalFunction( const char *pchJSFunctionName, Delegate del, bool bTrueGlobal, const char *pDesc, const char *pArgNames )
	{
		COMPILE_TIME_ASSERT( Delegate::USE__PANORAMA_DELEGATE__MACRO_TO_CREATE_FUNCTIONS != 0 );
		COMPILE_TIME_ASSERT( Delegate::kNumArgs <= RegisterJSEntryInfo_t::k_unMaxParams );
		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );

		v8::Handle< v8::FunctionTemplate > funcTempl = v8::FunctionTemplate::New( GetV8Isolate(), &JSFunctionCallTuple<Delegate>, v8::String::NewFromUtf8( GetV8Isolate(), pchJSFunctionName ).ToLocalChecked() );
		UIEngine()->AddGlobalV8FunctionTemplate( pchJSFunctionName, &funcTempl, bTrueGlobal );

		int nEntry = UIEngine()->NewRegisterJSEntry( pchJSFunctionName, RegisterJSEntryInfo_t::k_EGlobalFunction, pDesc, RegisterJSTypeDeducer_t<typename Delegate::ReturnType>::Type() );
		RegisterJSType_t paramTypes[Delegate::kNumArgsForArray];
		RegisterJSTypesDeducer<typename Delegate::Arguments>::Call( paramTypes );
		UIEngine()->SetRegisterJSEntryParams( nEntry, Delegate::kNumArgs, paramTypes, pArgNames );
	}

	inline void RegisterJSGlobalFunctionRaw( const char *pchJSFunctionName, void (*pCallbackFunc)( const v8::FunctionCallbackInfo< v8::Value > &callbackInfo ), bool bTrueGlobal, const char *pDesc = NULL )
	{
		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );

		v8::Handle< v8::FunctionTemplate > funcTempl = v8::FunctionTemplate::New( GetV8Isolate(), pCallbackFunc );
		UIEngine()->AddGlobalV8FunctionTemplate( pchJSFunctionName, &funcTempl, bTrueGlobal );

		int nEntry = UIEngine()->NewRegisterJSEntry( pchJSFunctionName, RegisterJSEntryInfo_t::k_EGlobalFunction, pDesc, k_ERegisterJSTypeVoid );
		RegisterJSType_t pParamTypes[1] = { k_ERegisterJSTypeRawV8Args };
		UIEngine()->SetRegisterJSEntryParams( nEntry, 1, pParamTypes, NULL );
	}

	template < typename Delegate >
	void RegisterJSMethodRaw( const char *pchMethodName, Delegate rawDel, const char *pDesc = NULL )
	{
		COMPILE_TIME_ASSERT( Delegate::USE__PANORAMA_DELEGATE__MACRO_TO_CREATE_DELEGATES != 0 );
		COMPILE_TIME_ASSERT( Delegate::kNumArgs == 1 );
		COMPILE_TIME_ASSERT( ( std::is_same< typename std::tuple_element<0, typename Delegate::Arguments>::type, const v8::FunctionCallbackInfo<v8::Value>&>::value ) );
		COMPILE_TIME_ASSERT( ( std::is_same< typename Delegate::ReturnType, void>::value ) );

		v8::Isolate::Scope isolate_scope( GetV8Isolate() );
		v8::HandleScope handle_scope( GetV8Isolate() );

		v8::Handle<v8::ObjectTemplate> objTemplate = UIEngine()->GetCurrentV8ObjectTemplateToSetup();
		v8::Local<v8::Signature> classSignature = UIEngine()->GetCurrentV8ClassToSetupSignature();
		objTemplate->Set( v8::String::NewFromUtf8( GetV8Isolate(), pchMethodName ).ToLocalChecked(), v8::FunctionTemplate::New( GetV8Isolate(), &JSMethodCallRaw<Delegate>, v8::Null( GetV8Isolate() ), classSignature ) );

		int nEntry = UIEngine()->NewRegisterJSEntry( pchMethodName, RegisterJSEntryInfo_t::k_EMethod, pDesc, k_ERegisterJSTypeVoid );
		RegisterJSType_t pParamTypes[1] = { k_ERegisterJSTypeRawV8Args };
		UIEngine()->SetRegisterJSEntryParams( nEntry, 1, pParamTypes, NULL );
	}

	inline v8::Local< v8::String > JSObjectToJSON( v8::Isolate *pIsolate, v8::Local< v8::Value > object )
	{
		auto context = pIsolate->GetCurrentContext();
		v8::Local<v8::Value> json, stringify, result;
		if ( !context->Global()->Get( context, v8::String::NewFromUtf8Literal( pIsolate, "JSON" ) ).ToLocal( &json ) || !json->IsObject() ) return {};
		auto globalJSON = json.As<v8::Object>();
		if ( !globalJSON->Get( context, v8::String::NewFromUtf8Literal( pIsolate, "stringify" ) ).ToLocal( &stringify ) || !stringify->IsFunction() ) return {};
		v8::Local<v8::Value> args[] = { object };
		if ( !stringify.As<v8::Function>()->Call( context, globalJSON, 1, args ).ToLocal( &result ) || !result->IsString() ) return {};
		return result.As<v8::String>();
	}
}

#pragma warning(pop)

#if _GNUC
#pragma GCC diagnostic pop
#endif

#endif // _WIN32
