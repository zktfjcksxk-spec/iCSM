//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef STYLEPROPERTYFACTORY_H
#define STYLEPROPERTYFACTORY_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/validator.h"
#include "utlsymbol.h"
#include "utlvector.h"
#include "utlsortvector.h"
#include "utlstring.h"
#include "layout/stylesymbol.h"
#include "iuistylefactory.h"

namespace panorama
{

class CStyleProperty;

#ifndef DBGFLAG_VALIDATE
class CValidator { };
#define ValidateObj( obj ) (obj)
#endif

struct StylePropertyMemStats_t
{
	int m_nSizeInBytes;
	int m_nCount;
	int m_nPeakSizeInBytes;
	int m_nPeakCount;
};

//-----------------------------------------------------------------------------
// Purpose: Used by DECLARE_STYLE_PROPERTY* macros
//-----------------------------------------------------------------------------
typedef CStyleProperty *( *STYLEPROPERTYCREATEFUNC )();
typedef CStyleProperty *( *STYLEPROPERTYCOPYFUNC )( const CStyleProperty& );
typedef void ( *STYLEPROPERTYFREEFUNC)( CStyleProperty* );
typedef void ( *STYLEPROPERTYVALIDATEFUNC)( CValidator &validator );
typedef void ( *STYLEPROPERTYMEMSTATSFUNC)( StylePropertyMemStats_t &stats );
class CStylePropertyFactory
{
public:
	// used when defining a property
	CStylePropertyFactory( const char *pchName, STYLEPROPERTYCREATEFUNC pCreate, STYLEPROPERTYCOPYFUNC pCopy, STYLEPROPERTYFREEFUNC pFree, STYLEPROPERTYVALIDATEFUNC pValidate, STYLEPROPERTYMEMSTATSFUNC pMemStats, bool bCanInherit );

	// used when defining an alias
	CStylePropertyFactory( CStyleSymbol symProperty, STYLEPROPERTYCREATEFUNC pCreate, STYLEPROPERTYCOPYFUNC pCopy, STYLEPROPERTYFREEFUNC pFree, STYLEPROPERTYVALIDATEFUNC pValidate, STYLEPROPERTYMEMSTATSFUNC pMemStats, const char *pchAlias );

	static bool BRegisteredPropertyOrAlias( panorama::CStyleSymbol symName );
	static bool BRegisteredProperty( panorama::CStyleSymbol symName );
	static bool BRegisteredAlias( panorama::CStyleSymbol symName );
	static CStyleSymbol GetPropertyNameForAlias( panorama::CStyleSymbol symName );
	static CStyleProperty *CreateStyleProperty( panorama::CStyleSymbol symName );
	static CStyleProperty *CopyStyleProperty( const panorama::CStyleProperty &property );
	static void FreeStyleProperty( panorama::CStyleProperty *pProperty );
	static bool BCanInheritProperty( panorama::CStyleSymbol symName );
	static const CUtlVector< panorama::CStyleSymbol > &GetAllProperties();
	static const CUtlVector< panorama::CStyleSymbol > &GetInheritedProperties();
	static const CUtlVector< panorama::CStyleSymbol > &GetPropertiesAndAliases();
	static const CUtlVector< CUtlString > &GetSortedPropertyAndAliasNames();
	static void PrintPropertiesMemStats();

	// helper to make creating a property less verbose
	template< class T >
	static T *Create() { return (T*)CreateStyleProperty( T::symbol ); }

#ifdef DBGFLAG_VALIDATE
	static void ValidateStyleProperty( CStyleSymbol symName, CValidator &validator );
	static void ValidateStatics( CValidator &validator );
#endif

private:
	CStyleProperty *CreateStylePropertyInternal();
	CStyleProperty *CopyStylePropertyInternal( const panorama::CStyleProperty &property );
	void FreeStylePropertyInternal( CStyleProperty *pProperty );
	void ValidateMemPool( CValidator &validator );
	
	bool m_bAlias;			// if true, m_symbol and m_funcCreate will point to proprety that should be created
	bool m_bCanInherit;		// not set for property alias

	CStyleSymbol m_symbol;	// points to symbol of created property
	STYLEPROPERTYCREATEFUNC m_funcCreate;
	STYLEPROPERTYCOPYFUNC m_funcCopy;
	STYLEPROPERTYFREEFUNC m_funcFree;
	STYLEPROPERTYVALIDATEFUNC m_funcValidate;
	STYLEPROPERTYMEMSTATSFUNC m_funcMemStats;

	static CUtlVector< CStyleSymbol > s_vecAllProperties;
	static CUtlVector< CStyleSymbol > s_vecInheritedProperties;
	static CUtlVector< CStyleSymbol > s_vecPropertiesAndAliases;
	static CUtlVector< CUtlString > s_vecSortedPropertyAndAliasNames;
};


//-----------------------------------------------------------------------------
// Purpose: Wrapper to expose parts of static style factory to client code
//-----------------------------------------------------------------------------
class CStyleFactoryWrapper : public panorama::IUIStyleFactory
{
public:
	virtual CStyleSymbol GetPropertyNameForAlias( panorama::CStyleSymbol symName ) OVERRIDE 
	{ 
		return CStylePropertyFactory::GetPropertyNameForAlias( symName ); 
	}

	virtual CStyleProperty *CreateStyleProperty( panorama::CStyleSymbol symName ) OVERRIDE
	{
		return CStylePropertyFactory::CreateStyleProperty( symName );
	}

	virtual CStyleProperty *CopyStyleProperty( const panorama::CStyleProperty &property ) OVERRIDE
	{
		return CStylePropertyFactory::CopyStyleProperty( property );
	}

	virtual void FreeStyleProperty( panorama::CStyleProperty *pProperty ) OVERRIDE
	{
		return CStylePropertyFactory::FreeStyleProperty( pProperty );
	}

	virtual const CUtlVector< CUtlString > &GetSortedPropertyAndAliasNames() OVERRIDE
	{
		return CStylePropertyFactory::GetSortedPropertyAndAliasNames();
	}

	virtual bool BRegisteredProperty( panorama::CStyleSymbol symName ) OVERRIDE
	{
		return CStylePropertyFactory::BRegisteredProperty( symName );
	}

	virtual bool BRegisteredAlias( panorama::CStyleSymbol symName ) OVERRIDE
	{
		return CStylePropertyFactory::BRegisteredAlias( symName );
	}
};


// Macro for declaring a style property & its symbol
#if !defined( SOURCE2_PANORAMA ) || defined( PANORAMA_USE_S1WRAPPER )

#if defined( PANORAMA_USE_S1WRAPPER )
#define STYLE_PROPERTY_MEMPOOL_GROWMODE CUtlMemoryPool::GROW_SLOW
#else
#define STYLE_PROPERTY_MEMPOOL_GROWMODE CUtlMemoryPool::GROW_FAST
#endif

#define DECLARE_STYLE_PROPERTY( className, cssSymbol )												\
	const CStyleSymbol className::symbol( #cssSymbol, true );										\
	static CClassMemoryPool< className > &GetPropertyMemPool_##className()							\
	{																								\
		static CClassMemoryPool< className > s_pool( 100, STYLE_PROPERTY_MEMPOOL_GROWMODE, VALIGNOF_TEMPLATE_SAFE(className) );		\
		return s_pool;																				\
	}																								\
	static CStyleProperty *Create_##className()														\
	{																								\
		return GetPropertyMemPool_##className().Alloc();											\
	};																								\
	static CStyleProperty *Copy_##className( const CStyleProperty &property )						\
	{																								\
		className *pNewProperty = (className*)Create_##className();									\
		return CopyConstruct<className>( pNewProperty, (const className&)property );									\
	};																								\
	static void Free_##className( CStyleProperty *pDelete )											\
	{																								\
		GetPropertyMemPool_##className().Free( (className*)pDelete );								\
	}																								\
	static void ValidateMemPool_##className( CValidator &validator )								\
	{																								\
		ValidateObj( GetPropertyMemPool_##className() );											\
	}																								\
	static void MemStats_##className( StylePropertyMemStats_t &stats )								\
	{																								\
		stats.m_nSizeInBytes = GetPropertyMemPool_##className().Size();								\
		stats.m_nCount = GetPropertyMemPool_##className().Count();									\
		stats.m_nPeakSizeInBytes = GetPropertyMemPool_##className().PeakCount() * GetPropertyMemPool_##className().BlockSize();		\
		stats.m_nPeakCount = GetPropertyMemPool_##className().PeakCount();							\
	}																								\
	static CStylePropertyFactory g_##className##_Helper( #cssSymbol, Create_##className, Copy_##className, Free_##className, ValidateMemPool_##className, MemStats_##className, false );	\
	className *g_##className##LinkerHack = NULL;


// Macro for declaring a style property & its symbol
#define DECLARE_STYLE_PROPERTY_INHERIT( className, cssSymbol )										\
	const CStyleSymbol className::symbol( #cssSymbol, true );										\
	static CClassMemoryPool< className > &GetPropertyMemPool_##className()							\
	{																								\
		static CClassMemoryPool< className > s_pool( 100, STYLE_PROPERTY_MEMPOOL_GROWMODE, VALIGNOF_TEMPLATE_SAFE(className) );		\
		return s_pool;																				\
	}																								\
	static CStyleProperty *Create_##className()														\
	{																								\
		return GetPropertyMemPool_##className().Alloc();											\
	};																								\
	static CStyleProperty *Copy_##className( const CStyleProperty &property )						\
	{																								\
		className *pNewProperty = (className*)Create_##className();									\
		return CopyConstruct<className>( pNewProperty, (const className&)property );									\
	};																								\
	static void Free_##className( CStyleProperty *pDelete )											\
	{																								\
		GetPropertyMemPool_##className().Free( (className*)pDelete );								\
	}																								\
	static void ValidateMemPool_##className( CValidator &validator )								\
	{																								\
		ValidateObj( GetPropertyMemPool_##className() );											\
	}																								\
	static void MemStats_##className( StylePropertyMemStats_t &stats )								\
	{																								\
		stats.m_nSizeInBytes = GetPropertyMemPool_##className().Size();								\
		stats.m_nCount = GetPropertyMemPool_##className().Count();									\
		stats.m_nPeakSizeInBytes = GetPropertyMemPool_##className().PeakCount() * GetPropertyMemPool_##className().BlockSize();		\
		stats.m_nPeakCount = GetPropertyMemPool_##className().PeakCount();							\
	}																								\
	static CStylePropertyFactory g_##className##_Helper( #cssSymbol, Create_##className, Copy_##className, Free_##className, ValidateMemPool_##className, MemStats_##className, true );	\
	className *g_##className##LinkerHack = NULL;

#else

#define DECLARE_STYLE_PROPERTY( className, cssSymbol )												\
	const CStyleSymbol className::symbol( #cssSymbol, true );										\
	static CStyleProperty *Create_##className()														\
	{																								\
		return new className;																		\
	};																								\
	static void Free_##className( CStyleProperty *pDelete )											\
	{																								\
		delete (className*)pDelete;																	\
	}																								\
	static void ValidateMemPool_##className( CValidator &validator )								\
	{																								\
	}																								\
	static void MemStats_##className( StylePropertyMemStats_t &stats )								\
	{																								\
	}																								\
	static CStylePropertyFactory g_##className##_Helper( #cssSymbol, Create_##className, Free_##className, ValidateMemPool_##className, MemStats_##className, false );	\
	className *g_##className##LinkerHack = NULL;


// Macro for declaring a style property & its symbol
#define DECLARE_STYLE_PROPERTY_INHERIT( className, cssSymbol )										\
	const CStyleSymbol className::symbol( #cssSymbol, true );										\
	static CStyleProperty *Create_##className()														\
	{																								\
		return new className;																		\
	};																								\
	static void Free_##className( CStyleProperty *pDelete )											\
	{																								\
		delete (className*)pDelete;																	\
	}																								\
	static void ValidateMemPool_##className( CValidator &validator )								\
	{																								\
	}																								\
	static void MemStats_##className( StylePropertyMemStats_t &stats )								\
	{																								\
	}																								\
	static CStylePropertyFactory g_##className##_Helper( #cssSymbol, Create_##className, Free_##className, ValidateMemPool_##className, MemStats_##className, true );	\
	className *g_##className##LinkerHack = NULL;

#endif

// Macro for declaring another css name for a property
// MUST HAVE ALREADY USED AFTER DECLARE_STYLE_PROPERTY FOR THIS CLASS
#define DECLARE_STYLE_PROPERTY_ALIAS( className, cssSymbol, classStatic )							\
	const CStyleSymbol className::classStatic( #cssSymbol, true );											\
	static CStylePropertyFactory g_##className##_##classStatic( className::symbol, Create_##className, Copy_##className, Free_##className, ValidateMemPool_##className, MemStats_##className, #cssSymbol );

} // namespace panorama

#endif // STYLEPROPERTYFACTORY_H
