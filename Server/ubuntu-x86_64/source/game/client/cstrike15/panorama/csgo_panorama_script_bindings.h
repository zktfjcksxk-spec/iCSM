//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#pragma once

// Helpers for exposing C side objects to javascript

// KV wrapper just like autodelete but allows an empty default constructor, required by panorama binding templates
class JSObjectAsKeyValues
{
public:
	inline JSObjectAsKeyValues() : m_pKeyValues( nullptr ) {}
	inline explicit JSObjectAsKeyValues( KeyValues* pKV ) : m_pKeyValues( pKV ) {}

	// FIXME: Expensive, but currently required for loading screen panorama event
	JSObjectAsKeyValues( const JSObjectAsKeyValues& src )
	{
		m_pKeyValues = src->MakeCopy();
	};
	JSObjectAsKeyValues& operator= ( const JSObjectAsKeyValues& src )
	{
		// Careful ordering to make sure self-assignment works
		KeyValues* myKv = src->MakeCopy();
		if ( m_pKeyValues )
			delete m_pKeyValues;
		m_pKeyValues = myKv;
		return *this;
	};

	JSObjectAsKeyValues( JSObjectAsKeyValues&& src )
	{
		m_pKeyValues = src.m_pKeyValues;
		src.m_pKeyValues = nullptr;
	}
	JSObjectAsKeyValues& operator=( JSObjectAsKeyValues&& src )
	{
		// Swap so that destructor of moved-from clears state
		KeyValues* myKv = m_pKeyValues;
		m_pKeyValues = src.m_pKeyValues;
		src.m_pKeyValues = myKv;
		return *this;
	}

	// Don't construct from AutoDelete; either construct using new KeyValues, or, if you don't own the relevant KV, use MakeCopy()
	JSObjectAsKeyValues( const KeyValues::AutoDelete& ) = delete;

	inline ~JSObjectAsKeyValues( void )
	{
		if ( m_pKeyValues )
		{
			delete m_pKeyValues;
			m_pKeyValues = nullptr;
		}
	}
	KeyValues *operator->() { return m_pKeyValues; }
	KeyValues *operator->()	const { return m_pKeyValues; }
	operator KeyValues *( ) { return m_pKeyValues; }
	operator KeyValues *( ) const { return m_pKeyValues; }
protected:
	KeyValues *m_pKeyValues;
private:
};

#if defined ( PANORAMA_ENABLE )

#include "panorama/uijsregistration.h"
#include "panorama/iuiengine.h"
#include "keyvalues.h"

v8::Local< v8::Object > JSObjectFromKeyValues( v8::Isolate *pIsolate, KeyValues *pKeyValues );


// Note: the &*memberName idiom here is so that we work correctly with smart pointers which
// (intentionally) don't implicitly cast to KeyValues*.  We know we are just borrowing the
// pointer here so it's safe to pass this way.
#define PANORAMA_DELCARE_EXPOSED_KEYVALUE_MEMBER( memberName )			\
	v8::Persistent< v8::Object > m_jsObject_##memberName;				\
	v8::Local< v8::Object > FetchKeyValuesAsJSObject_##memberName()		\
	{																	\
		v8::Isolate* pIsolate = panorama::GetV8Isolate();				\
		if( m_jsObject_##memberName.IsEmpty() )							\
		{																\
			m_jsObject_##memberName.Reset( pIsolate, JSObjectFromKeyValues( pIsolate, &*memberName ) ); \
		}																\
		return m_jsObject_##memberName.Get( pIsolate );					\
	}																	\
	void GetKeyValuesAsJSObject_##memberName( const v8::FunctionCallbackInfo<v8::Value>& info ) \
	{ \
		info.GetReturnValue().Set( FetchKeyValuesAsJSObject_##memberName() ); \
	}

#define PANORAMA_FETCH_EXPOSED_KEYVALUE_MEMBER( memberName ) \
	FetchKeyValuesAsJSObject_##memberName()
	
#define PANORAMA_CLEAR_EXPOSED_KEYVALUES_MEMBER( memberName ) \
	do { m_jsObject_##memberName.Reset(); } while(0)

#define PANORAMA_EXPOSE_KEYVALUES_MEMBER( jsFuncName, className, memberName, description ) \
	panorama::RegisterJSMethodRaw( #jsFuncName, PANORAMA_DELEGATE( &className::GetKeyValuesAsJSObject_##memberName ), description );

void V8ParamToPanoramaType( const v8::Handle<v8::Value> &pValueIn, JSObjectAsKeyValues* out );
void PanoramaTypeToV8Param( JSObjectAsKeyValues &pIn, v8::Handle<v8::Value> *pValueOut );


// Helper functions for setting JSO data
//
//

v8::Local< v8::Object > CreateJSOSubObject( v8::Local< v8::Object > pObject, const char *key );
void CreateJSOEntry_Number( v8::Local< v8::Object > pObject, const char *key, int value );
void CreateJSOEntry_String( v8::Local< v8::Object > pObject, const char *key, const char * value );
void CreateJSOEntry_Bool( v8::Local< v8::Object > pObject, const char *key, bool value );

#else // PANORAMA_ENABLE

// STUBS FOR WIN64
// Platforms that don't have v8 get stubbed macros/functions to keep a bunch of ifdef boilerplate out of the code

#define PANORAMA_DELCARE_EXPOSED_KEYVALUE_MEMBER( memberName )
#define PANORAMA_CLEAR_EXPOSED_KEYVALUES_MEMBER( memberName )
#define PANORAMA_EXPOSE_KEYVALUES_MEMBER( jsFuncName, className, memberName, description )

#endif

