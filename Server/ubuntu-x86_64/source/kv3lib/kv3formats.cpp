//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
// This file contains the conversion routines between different KV3 formats.
//
// It is an INTENTIONAL GOAL that these routines should not depend on ANY external code apart from
// the underlying KV3 structures. Conversion routines should be written once, hard-code everything,
// and never change again. (If they referenced eg. enum values directly, then changing those enums
// would break conversion routines!)
//
//==================================================================================================

#include "kv3format_manager.h"
#include "kv3lib/kv3formats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//--------------------------------------------------------------------------------------------------
// Force kv3format_manager.cpp not to be elided by the linker
//--------------------------------------------------------------------------------------------------
extern int g_KV3Format_Manager_LinkerHook;
static int s_KV3Format_Manager = g_KV3Format_Manager_LinkerHook;
int g_KV3Formats_LinkerHook = 0;

//------------------------------------------------------------------------------
// Helpers that are stable across KV3 formats
//------------------------------------------------------------------------------
#define TRANSFER_CLASS_KEY "_class"

template< size_t TStringCount > static bool IsKV3InClassList( const KeyValues3 *pKV3, const char *(&pClassList)[ TStringCount ] ) 
{
	const char *pKV3Class = pKV3->GetMemberString( TRANSFER_CLASS_KEY );
	for ( int iClass = 0; iClass < TStringCount; ++iClass )
	{
		const char *pCheckClass = pClassList[ iClass ];
		if ( !V_strcmp( pKV3Class, pCheckClass ) )
		{
			return true;
		}
	}
	return false;
}

static bool IsKV3Class( const KeyValues3 *pKV3, const char *pCheckClass ) 
{
	const char *pKV3Class = pKV3->GetMemberString( TRANSFER_CLASS_KEY );
	return !V_strcmp( pKV3Class, pCheckClass );
}

static void SetKV3Class( KeyValues3 *pKV3, const char *pClass ) 
{
	pKV3->SetMemberString( TRANSFER_CLASS_KEY, pClass );
}

static int FindArrayElementOfClass( KeyValues3 *pArray, const char *pClass, bool bCreateIfNotFound )
{
	for ( int iElement = 0; iElement < pArray->GetArrayElementCount(); iElement++ )
	{
		KeyValues3 *pElement = pArray->GetArrayElement( iElement );
		if ( IsKV3Class( pElement, pClass ) )
		{
			return iElement;
		}
	}
	return -1;
}

#if 0 // $$$REI Remove Source2 ModelDoc format conversions from CSGO

//--------------------------------------------------------------------------------------------------
// ModelDoc V0: Remove names from singletons and rename GenericGameKeys -> RawGameKeys
//--------------------------------------------------------------------------------------------------
DECLARE_KV3_FORMAT_CONVERSION( KV3_FORMAT_GENERIC, KV3_FORMAT_MODELDOC0 )
{
	const char *SINGLETON_NODE_TYPES[] =
	{
		"RenderMeshList",
		"AnimationList",
		"HitboxSetList",
		"AttachmentList",
		"PhysicsShapeList",
		"MaterialGroupList",
		"BodyGroupList",
		"Skeleton",
		"GameDataList",
		"ChildDocumentList",
	};

	for ( CKeyValues3Iterator it( conversionCtx.m_pRoot ); it.IsValid(); it.Advance() )
	{
		// Strip names off of singletons
		if ( IsKV3InClassList( it.Get(), SINGLETON_NODE_TYPES ) )
		{
			it->RemoveMember( "name" );
		}

		// Rename GenericGameKeys -> RawGameKeys
		if ( IsKV3Class( it.Get(), "GenericGameKeys" ) )
		{
			it->SetMemberString( TRANSFER_CLASS_KEY, "RawGameKeys" );
		}
	}

	return KV3_FORMAT_CONVERSION_SUCCESS;
}


//--------------------------------------------------------------------------------------------------
// ModelDoc V1: Command procedures get moved to be a child of a new CommandProcedureList singleton
//--------------------------------------------------------------------------------------------------
DECLARE_KV3_FORMAT_CONVERSION( KV3_FORMAT_MODELDOC0, KV3_FORMAT_MODELDOC1 )
{
	KeyValues3 *pRootNode = conversionCtx.m_pRoot->FindMember( "rootNode" );
	KeyValues3 *pRootNodeChildren = pRootNode ? pRootNode->FindMember( "children" ) : nullptr;
	
	if ( !pRootNodeChildren )
		return KV3_FORMAT_CONVERSION_SUCCESS;

	CUtlVector< KeyValues3 > procedureCopies;
	for ( int iRootChild = 0; iRootChild < pRootNodeChildren->GetArrayElementCount(); )
	{
		KeyValues3 *pRootChild = pRootNodeChildren->GetArrayElement( iRootChild );
		if ( IsKV3Class( pRootChild, "CommandProcedure" ) )
		{
			procedureCopies.AddToTailGetPtr()->CopyFrom( pRootChild );
			pRootNodeChildren->ArrayRemoveMultiple( iRootChild, 1 );
		}
		else
		{
			iRootChild++;
		}
	}

	if ( procedureCopies.Count() == 0 )
		return KV3_FORMAT_CONVERSION_SUCCESS;

	// create the procedure list, with a children key
	KeyValues3 *pProcedureList = pRootNodeChildren->ArrayAddToTail();
	pProcedureList->SetMemberString( TRANSFER_CLASS_KEY, "CommandProcedureList" );
	KeyValues3 *pProcedureListChildren = pProcedureList->FindOrCreateMember( "children" );

	// copy each procedure into the list
	for ( const KeyValues3 &procedure: procedureCopies )
	{
		pProcedureListChildren->ArrayAddToTail()->CopyFrom( &procedure );
	}

	return KV3_FORMAT_CONVERSION_SUCCESS;
}


//--------------------------------------------------------------------------------------------------
// ModelDoc V2: Physics shapes get 'default' instead of empty string for surface_prop and collision_prop
//--------------------------------------------------------------------------------------------------
DECLARE_KV3_FORMAT_CONVERSION( KV3_FORMAT_MODELDOC1, KV3_FORMAT_MODELDOC2 )
{
	const char *PHYS_NODE_TYPES[] =
	{
		"PhysicsFileReference",
		"BasePhysicsShape",
		"PhysicsShapeCapsule",
		"PhysicsShapeHull",
		"PhysicsShapeMesh",
		"PhysicsShapeSphere",
	};

	for ( CKeyValues3Iterator it( conversionCtx.m_pRoot ); it.IsValid(); it.Advance() )
	{
		if ( !IsKV3InClassList( it.Get(), PHYS_NODE_TYPES ) )
			continue;

		if ( KeyValues3 *pSurfaceProp = it->FindMember( "surface_prop" ) )
		{
			if ( pSurfaceProp->EqualsValueFromString( "" ) )
			{
				pSurfaceProp->SetValueString( "default" );
			}
		}

		if ( KeyValues3 *pCollisionProp = it->FindMember( "collision_prop" ) )
		{
			if ( pCollisionProp )
			{
				pCollisionProp->SetValueString( "default" );
			}
		}
	}

	return KV3_FORMAT_CONVERSION_SUCCESS;
}

//--------------------------------------------------------------------------------------------------
// ModelDoc V3: CModelDocAnimation -> CModelDocAnimFile
//--------------------------------------------------------------------------------------------------
DECLARE_KV3_FORMAT_CONVERSION( KV3_FORMAT_MODELDOC2, KV3_FORMAT_MODELDOC3 )
{
	for ( CKeyValues3Iterator it( conversionCtx.m_pRoot ); it.IsValid(); it.Advance() )
	{
		if ( IsKV3Class( it.Get(), "Animation" ) )
		{
			it->SetMemberString( TRANSFER_CLASS_KEY, "AnimFile" );
			if ( KeyValues3 *pLoop = it->FindMember( "loop" ) )
			{
				if ( pLoop )
				{
					pLoop->SetValueString( "looping" );
				}
			}
		}
	}

	return KV3_FORMAT_CONVERSION_SUCCESS;
}

//--------------------------------------------------------------------------------------------------
// ModelDoc V4: PhysicsFileReference -> PhysicsFile
//--------------------------------------------------------------------------------------------------
DECLARE_KV3_FORMAT_CONVERSION( KV3_FORMAT_MODELDOC3, KV3_FORMAT_MODELDOC4 )
{
	for ( CKeyValues3Iterator it( conversionCtx.m_pRoot ); it.IsValid(); it.Advance() )
	{
		if ( IsKV3Class( it.Get(), "PhysicsFileReference" ) )
		{
			SetKV3Class( it.Get(), "PhysicsFile" );
		}
	}

	return KV3_FORMAT_CONVERSION_SUCCESS;
}


//--------------------------------------------------------------------------------------------------
// ModelDoc V5: SequenceMarkupList:[SequenceMarkup] -> AnimationList:[AnimProxy]
//--------------------------------------------------------------------------------------------------
DECLARE_KV3_FORMAT_CONVERSION( KV3_FORMAT_MODELDOC4, KV3_FORMAT_MODELDOC5 )
{
	// Rename all sequence markup nodes to AnimProxy nodes
	for ( CKeyValues3Iterator it( conversionCtx.m_pRoot ); it.IsValid(); it.Advance() )
	{
		if ( IsKV3Class( it.Get(), "SequenceMarkup" ) )
		{
			SetKV3Class( it.Get(), "AnimProxy" );
		}
	}

	// Reparent them to AnimationList
	KeyValues3 *pRootNode = conversionCtx.m_pRoot->FindMember( "rootNode" );
	KeyValues3 *pRootNodeChildren = pRootNode ? pRootNode->FindMember( "children" ) : nullptr;
	if ( !pRootNodeChildren )
		return KV3_FORMAT_CONVERSION_SUCCESS;

	int nSequenceMarkupListIndex = FindArrayElementOfClass( pRootNodeChildren, "SequenceMarkupList", false );
	if ( nSequenceMarkupListIndex == -1 )
		return KV3_FORMAT_CONVERSION_SUCCESS;

	int nAnimationListIndex = FindArrayElementOfClass( pRootNodeChildren, "AnimationList", true );

	KeyValues3 *pSequenceMarkupListChildren = pRootNodeChildren->GetArrayElement( nSequenceMarkupListIndex )->FindOrCreateMember( "children" );
	KeyValues3 *pAnimationListChildren = pRootNodeChildren->GetArrayElement( nAnimationListIndex )->FindOrCreateMember( "children" );

	int nNumMarkup = pSequenceMarkupListChildren->GetArrayElementCount();
	for ( int iMarkup = 0; iMarkup < nNumMarkup; ++iMarkup )
	{
		KeyValues3 *pMarkup = pSequenceMarkupListChildren->GetArrayElement( iMarkup );
		pAnimationListChildren->ArrayAddToTail()->CopyFrom( pMarkup );
	}

	// Remove SequenceMarkupList
	pRootNodeChildren->ArrayRemoveMultiple( nSequenceMarkupListIndex, 1 );

	return KV3_FORMAT_CONVERSION_SUCCESS;
}

//--------------------------------------------------------------------------------------------------
// ModelDoc V6:
// AnimFile:[sequence_name] -> AnimParamBase:[anim_name] using node name if blank
// Anim:[sequence_name] -> Anim:[anim_name]	- Anim's were not being serialized at this version
// AnimProxy:[sequence_name] -> AnimProxy:[anim_name]
//--------------------------------------------------------------------------------------------------
DECLARE_KV3_FORMAT_CONVERSION( KV3_FORMAT_MODELDOC5, KV3_FORMAT_MODELDOC6 )
{
	// Rename all sequence markup nodes to AnimProxy nodes
	for ( CKeyValues3Iterator it( conversionCtx.m_pRoot ); it.IsValid(); it.Advance() )
	{
		if ( IsKV3Class( it.Get(), "AnimFile" ) || IsKV3Class( it.Get(), "Anim" ) )
		{
			if ( KeyValues3 *pSequenceName = it->FindMember( "sequence_name" ) )
			{
				if ( pSequenceName )
				{
					CUtlString sAnimName;
					pSequenceName->GetValueAsString( &sAnimName );
					if ( sAnimName.IsEmpty() )
					{
						it->GetMemberAsString( "name", &sAnimName );
					}
					it->SetMemberString( "anim_name", sAnimName.Get() );
					it->RemoveMember( pSequenceName );
				}
			}
		}
		else if ( IsKV3Class( it.Get(), "AnimProxy" ) )
		{
			if ( KeyValues3 *pSequenceName = it->FindMember( "sequence_name" ) )
			{
				if ( pSequenceName )
				{
					CUtlString sAnimName;
					pSequenceName->GetValueAsString( &sAnimName );
					it->SetMemberString( "anim_name", sAnimName.Get() );
					it->RemoveMember( pSequenceName );
				}
			}
		}
	}

	return KV3_FORMAT_CONVERSION_SUCCESS;
}

//--------------------------------------------------------------------------------------------------
// ModelDoc V7: Move all children of BlendList to instead be children of AnimationList
//--------------------------------------------------------------------------------------------------
DECLARE_KV3_FORMAT_CONVERSION( KV3_FORMAT_MODELDOC6, KV3_FORMAT_MODELDOC7 )
{
	KeyValues3 *pRootNode = conversionCtx.m_pRoot->FindMember( "rootNode" );
	KeyValues3 *pRootNodeChildren = pRootNode ? pRootNode->FindMember( "children" ) : nullptr;
	if ( !pRootNodeChildren )
		return KV3_FORMAT_CONVERSION_SUCCESS;

	int nBlendListIndex = FindArrayElementOfClass( pRootNodeChildren, "BlendList", false );
	if ( nBlendListIndex == -1 )
		return KV3_FORMAT_CONVERSION_SUCCESS;

	KeyValues3 *pBlendListChildren = pRootNodeChildren->GetArrayElement( nBlendListIndex )->FindMember( "children" );
	if ( !pBlendListChildren )
		return KV3_FORMAT_CONVERSION_SUCCESS;

	int nAnimationListIndex = FindArrayElementOfClass( pRootNodeChildren, "AnimationList", true );
	KeyValues3 *pAnimationListChildren = pRootNodeChildren->GetArrayElement( nAnimationListIndex )->FindOrCreateMember( "children" );

	const int nBlendListChildCount = pBlendListChildren->GetArrayElementCount();
	for ( int nBlendListChild = 0; nBlendListChild < nBlendListChildCount; ++nBlendListChild )
	{
		KeyValues3 *pBlendListChild = pBlendListChildren->GetArrayElement( nBlendListChild );
		pAnimationListChildren->ArrayAddToTail()->CopyFrom( pBlendListChild );
	}

	pRootNodeChildren->ArrayRemoveMultiple( nBlendListIndex, 1 );

	return KV3_FORMAT_CONVERSION_SUCCESS;
}


//--------------------------------------------------------------------------------------------------
// ModelDoc V8: All anim_name keys 
//--------------------------------------------------------------------------------------------------
DECLARE_KV3_FORMAT_CONVERSION( KV3_FORMAT_MODELDOC7, KV3_FORMAT_MODELDOC8 )
{
	const char *ANIM_NODE_TYPES[] =
	{
		"AnimProxy",
		"1DBlend",
		"Anim",
		"AnimFile",
		"EmptyAnim",
	};

	for ( CKeyValues3Iterator it( conversionCtx.m_pRoot ); it.IsValid(); it.Advance() )
	{
		if ( IsKV3InClassList( it.Get(), ANIM_NODE_TYPES ) )
		{
			if ( KeyValues3 *pAnimName = it->FindMember( "anim_name" ) )
			{
				it->SetMemberString( "name", pAnimName->GetValueString() );
				it->RemoveMember( pAnimName );
			}
		}
	}

	return KV3_FORMAT_CONVERSION_SUCCESS;
}

//--------------------------------------------------------------------------------------------------
// ModelDoc V9: Handle AnimDelta nodes, convert to an AnimFile + Subtract modifier
//--------------------------------------------------------------------------------------------------
DECLARE_KV3_FORMAT_CONVERSION( KV3_FORMAT_MODELDOC8, KV3_FORMAT_MODELDOC9 )
{
	auto FindNodeByNmameAndClass = []( KeyValues3 *pKV3, const char *pszName, const char *pszClass )
	{
		KeyValues3 *pRet = nullptr;

		CUtlString sName;

		for ( CKeyValues3Iterator it( pKV3 ); it.IsValid(); it.Advance() )
		{
			if ( !IsKV3Class( it.Get(), pszClass ) )
				continue;

			it->GetMemberAsString( "name", &sName );
			if ( !V_stricmp_fast( sName.Get(), pszName ) )
			{
				pRet = it.Get();
				break;
			}
		}

		return pRet;
	};

	for ( CKeyValues3Iterator it( conversionCtx.m_pRoot ); it.IsValid(); it.Advance() )
	{
		if ( IsKV3Class( it.Get(), "AnimDelta" ) )
		{
			KeyValues3 *pNode = it.Get();

			SetKV3Class( pNode, "AnimFile" );

			KeyValues3 *pAnimA = FindNodeByNmameAndClass( conversionCtx.m_pRoot, pNode->GetMemberString( "anim_name_a" ), "AnimFile" );
			KeyValues3 *pAnimB = FindNodeByNmameAndClass( conversionCtx.m_pRoot, pNode->GetMemberString( "anim_name_b" ), "AnimFile" );

			pNode->SetMemberBool( "delta", pNode->GetMemberBool( "composite" ) );
			pNode->SetMemberString( "source_filename", pAnimA ? pAnimA->GetMemberString( "source_filename" ) : "" );

			KeyValues3 *pChildren = it->FindOrCreateMember( "children" );
			KeyValues3 *pSubtract = pChildren->ArrayAddToTail();
			SetKV3Class( pSubtract, "Subtract" );
			pSubtract->SetMemberString( "name", "" );
			pSubtract->SetMemberString( "anim_name", pAnimB ? pAnimB->GetMemberString( "name" ) : "" );
			pSubtract->SetMemberInt( "frame", 0 );

			pNode->RemoveMember( "anim_name_a" );
			pNode->RemoveMember( "anim_name_b" );
			pNode->RemoveMember( "composite" );
		}
	}

	return KV3_FORMAT_CONVERSION_SUCCESS;
}
#endif

//--------------------------------------------------------------------------------------------------
//
// KV3 Test format conversions:
//                  A
//                  |
//                  V
//      Generic --> B --> C --> D
//                        |
//                        V
//                        E
//
//--------------------------------------------------------------------------------------------------

#if 0 // $$$REI Temporarily remove all DECLARE_KV3_FORMAT_CONVERSION due to C++ Static Initialization Order Fiasco
//--------------------------------------------------------------------------------------------------
// Test Conversion: A to B is a no-op
//--------------------------------------------------------------------------------------------------
DECLARE_KV3_FORMAT_CONVERSION( KV3_FORMAT_TEST_A, KV3_FORMAT_TEST_B )
{
	int nCounter = conversionCtx.m_pRoot->GetMemberInt( "_test_conversion_counter", 0 ) + 1;
	conversionCtx.m_pRoot->SetMemberInt( "A->B", nCounter );
	conversionCtx.m_pRoot->SetMemberInt( "_test_conversion_counter", nCounter );
	return KV3_FORMAT_CONVERSION_SUCCESS;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
DECLARE_KV3_FORMAT_CONVERSION( KV3_FORMAT_GENERIC, KV3_FORMAT_TEST_B )
{
	int nCounter = conversionCtx.m_pRoot->GetMemberInt( "_test_conversion_counter", 0 ) + 1;
	conversionCtx.m_pRoot->SetMemberInt( "G->B", nCounter );
	conversionCtx.m_pRoot->SetMemberInt( "_test_conversion_counter", nCounter );
	return KV3_FORMAT_CONVERSION_SUCCESS;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
DECLARE_KV3_FORMAT_CONVERSION( KV3_FORMAT_TEST_B, KV3_FORMAT_TEST_C )
{
	int nCounter = conversionCtx.m_pRoot->GetMemberInt( "_test_conversion_counter", 0 ) + 1;
	conversionCtx.m_pRoot->SetMemberInt( "B->C", nCounter );
	conversionCtx.m_pRoot->SetMemberInt( "_test_conversion_counter", nCounter );
	return KV3_FORMAT_CONVERSION_SUCCESS;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
DECLARE_KV3_FORMAT_CONVERSION( KV3_FORMAT_TEST_C, KV3_FORMAT_TEST_D )
{
	int nCounter = conversionCtx.m_pRoot->GetMemberInt( "_test_conversion_counter", 0 ) + 1;
	conversionCtx.m_pRoot->SetMemberInt( "C->D", nCounter );
	conversionCtx.m_pRoot->SetMemberInt( "_test_conversion_counter", nCounter );
	return KV3_FORMAT_CONVERSION_SUCCESS;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
DECLARE_KV3_FORMAT_CONVERSION( KV3_FORMAT_TEST_C, KV3_FORMAT_TEST_E )
{
	int nCounter = conversionCtx.m_pRoot->GetMemberInt( "_test_conversion_counter", 0 ) + 1;
	conversionCtx.m_pRoot->SetMemberInt( "C->E", nCounter );
	conversionCtx.m_pRoot->SetMemberInt( "_test_conversion_counter", nCounter );
	return KV3_FORMAT_CONVERSION_SUCCESS;
}

#endif // $$$REI