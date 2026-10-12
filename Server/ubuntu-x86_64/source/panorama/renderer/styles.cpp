//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "styles.h"
#include "mathlib/beziercurve.h"
#include "mathlib/vector.h"
#include "panorama/panoramacurves.h"
#include "panorama/iuisoundsystem.h"
#include "panorama/uijsregistration.h"
#include "tier1/utldelegate.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

#define NO_DIRECT_INDEX 0xff

ConVar s_convarPanoramaSpewFullInvalidates( "@panorama_spew_layout_invalidates", "0" );
ConVar s_convarTransitionTimeFactor( "@panorama_transition_time_factor", "1.0", 0, "A float representing a scale factor for transitions. 1.0 is normal, 2.0 would be twice as fast as normal, 0.5 half as fast" );

CUtlVector< StyleEntry_t > CPanelStyle::s_EmptyPropertyVec;


//-----------------------------------------------------------------------------
// Purpose: helper function to print out a friendly name of a panel (used to debug transition issues)
//-----------------------------------------------------------------------------
extern void AppendStyleFlagsToString( CFmtStr1024 *pfmt, uint unStyleFlags );
static bool GetDebugPanelName( char *pchBuffer, uint cubBuffer, IUIPanel *pPanel )
{
	CFmtStr1024 fmt( "%s", pPanel->ClientPtr()->GetPanelType().String() );
	if ( pPanel->GetID()[0] != '\0' )
		fmt.AppendFormat( "#%s", pPanel->GetID() );

	FOR_EACH_VEC( pPanel->GetClasses(), iVec )
	{
		fmt.AppendFormat( ".%s", pPanel->GetClasses()[iVec].String() );
	}

	// append style flags but skip inspect
	uint unStyleFlags = pPanel->GetStyleFlags();
	unStyleFlags &= ~k_EStyleFlagInspect;
	AppendStyleFlagsToString( &fmt, unStyleFlags );

	// always try and copy something
	V_strncpy( pchBuffer, fmt.Access(), cubBuffer );
	return (cubBuffer < (uint)fmt.Length() + 1);
}


//-----------------------------------------------------------------------------
// Purpose: Converts straight time value into time value for transition timing func
//-----------------------------------------------------------------------------
float panorama::GetProgressForTimingFunction( const CCubicBezierCurve< Vector2D > &cubicBezier, float flPctComplete /* 0.0 -> 1.0 */ )
{
	// bugbug jmccaskey - could precompute tables for the standard functions.  Optimize if needed later.
	Vector2D vecReturn;
	cubicBezier.Evaluate( flPctComplete, vecReturn );
	return vecReturn.y;
}


//-----------------------------------------------------------------------------
// Purpose: Returns interpolated matrix between from and to
//-----------------------------------------------------------------------------
float panorama::GetTimeProgress( const CCubicBezierCurve< Vector2D > &cubicBezier, double flStart, double flCurrent, float flDelay, float flDuration )
{
	if ( flDuration < 0.0000001f )
		return 1.0f;

	float flTimeProgress = ( ( flCurrent - flStart ) -  flDelay ) / flDuration;
	flTimeProgress = clamp( flTimeProgress, 0.0f, 1.0f );

	return GetProgressForTimingFunction( cubicBezier, flTimeProgress );
}


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CPanelStyle::CPanelStyle( IUIPanel *pPanel ) :
	m_pPanel( pPanel ),
	m_vecProperties( 0, 12 ),
	m_ScheduledTransitionCleanup( MAKE_SCHEDULED_FUNC( CPanelStyle::CleanupTransitionsAndAnimations ) )
{
	m_flLastTransitionCleanup = 0;
	m_flNextTransitionCleanup = FLT_MAX;
	m_pVecElementProperties = NULL;
	m_pvecActiveAnimations = NULL;
	V_memset( m_rgPropsPresent, 0, sizeof( m_rgPropsPresent ) );
	for ( int i = 0; i < V_ARRAYSIZE( m_rgDirectPropIndex ); i++ )
	{
		m_rgDirectPropIndex[i] = NO_DIRECT_INDEX;
	}
}


//-----------------------------------------------------------------------------
// Purpose: deconstructor
//-----------------------------------------------------------------------------
CPanelStyle::~CPanelStyle()
{	
	Clear();
}


//-----------------------------------------------------------------------------
// Purpose: Set a new UI scale factor from the one we constructed with
//-----------------------------------------------------------------------------
void CPanelStyle::UpdateUIScaleFactor( const Vector &vOldScaleFactor, const Vector &vNewScaleFactor, const Vector &vOldParentScaleFactor, const Vector &vNewParentScaleFactor )
{
	Vector vScaleFactor = vNewScaleFactor / vOldScaleFactor;
	Vector vParentScaleFactor = vNewParentScaleFactor / vOldParentScaleFactor;

	FOR_EACH_VEC( m_vecProperties, i )
	{
		m_vecProperties[ i ].m_pStyleProperty->ApplyUIScaleFactor( vScaleFactor, vParentScaleFactor );
	}

	FOR_EACH_RBTREE_FAST( m_treePropertiesInTransition, i )
	{
		m_treePropertiesInTransition.Element( i )->m_pStyleProperty->ApplyUIScaleFactor( vScaleFactor, vParentScaleFactor );
	}

	if ( m_pvecActiveAnimations )
	{
		CUtlVector< CActiveAnimation * > &vec = *m_pvecActiveAnimations;
		FOR_EACH_VEC( vec, iAnim )
		{
			CActiveAnimation *pAnim = vec[ iAnim ];
			pAnim->ApplyUIScaleFactor( vScaleFactor, vParentScaleFactor );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the ui-scale property
//-----------------------------------------------------------------------------
void CPanelStyle::SetUIScale( const Vector &vUIScale )
{
	CStylePropertyUIScale *pValue = CStylePropertyFactory::Create< CStylePropertyUIScale >();
	pValue->m_vUIScale = vUIScale;
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Get the ui-scale property value
//-----------------------------------------------------------------------------
Vector CPanelStyle::GetUIScale()
{
	CStylePropertyUIScale *pValue = ( CStylePropertyUIScale* )FindProperty( CStylePropertyUIScale::symbol );
	return pValue ? pValue->m_vUIScale : CStylePropertyUIScale::GetDefault();
}


//-----------------------------------------------------------------------------
// Purpose: Get the interpolated ui-scale value
//-----------------------------------------------------------------------------
Vector CPanelStyle::GetInterpolatedUIScale( bool bFinal )
{

	if ( BHasTransition( CStylePropertyUIScale::symbol ) )
	{
		CStylePropertyUIScale *pProperty = ( CStylePropertyUIScale* )GetInterpolatedProperty( CStylePropertyUIScale::symbol, bFinal );
		if ( pProperty )
		{
			Vector vOut = pProperty->m_vUIScale;

			CStylePropertyFactory::FreeStyleProperty( pProperty );

			return vOut;
		}
		else
		{
			return CStylePropertyUIScale::GetDefault();
		}
	}
	else
	{
		return GetUIScale();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the current ui scale for the panel that might include a 
// propagated ui-scale from some ancestor
//-----------------------------------------------------------------------------
Vector CPanelStyle::GetActualUIScale()
{
	if ( m_pPanel )
		return m_pPanel->GetActualUIScale();

	CStylePropertyUIScale *pValue = ( CStylePropertyUIScale* )FindProperty( CStylePropertyUIScale::symbol );
	if ( pValue )
		return pValue->m_vUIScale;

	return CStylePropertyUIScale::GetDefault();
}


//-----------------------------------------------------------------------------
// Purpose: Get the current ui scale for the panel's parent that might include a 
// propagated ui-scale from some ancestor
//-----------------------------------------------------------------------------
Vector CPanelStyle::GetParentActualUIScale()
{
	if ( m_pPanel )
		return m_pPanel->GetParentActualUIScale();

	CStylePropertyUIScale *pValue = ( CStylePropertyUIScale* )FindProperty( CStylePropertyUIScale::symbol );
	if ( pValue )
		return pValue->m_vUIScale;

	return CStylePropertyUIScale::GetDefault();
}


//-----------------------------------------------------------------------------
// Purpose: Get the current ui scale x for the panel that might include a 
// propagated ui-scale from some ancestor
//-----------------------------------------------------------------------------
float CPanelStyle::GetActualUIScaleX()
{
	if ( m_pPanel )
		return m_pPanel->GetActualUIScaleX();

	CStylePropertyUIScale *pValue = ( CStylePropertyUIScale* )FindProperty( CStylePropertyUIScale::symbol );
	if ( pValue )
		return pValue->m_vUIScale.x;

	return CStylePropertyUIScale::GetDefaultX();
}


//-----------------------------------------------------------------------------
// Purpose: Get the current ui scale y for the panel that might include a 
// propagated ui-scale from some ancestor
//-----------------------------------------------------------------------------
float CPanelStyle::GetActualUIScaleY()
{
	if ( m_pPanel )
		return m_pPanel->GetActualUIScaleY();

	CStylePropertyUIScale *pValue = ( CStylePropertyUIScale* )FindProperty( CStylePropertyUIScale::symbol );
	if ( pValue )
		return pValue->m_vUIScale.y;

	return CStylePropertyUIScale::GetDefaultY();
}


//-----------------------------------------------------------------------------
// Purpose: Get the current ui scale z for the panel that might include a 
// propagated ui-scale from some ancestor
//-----------------------------------------------------------------------------
float CPanelStyle::GetActualUIScaleZ()
{
	if ( m_pPanel )
		return m_pPanel->GetActualUIScaleZ();

	CStylePropertyUIScale *pValue = ( CStylePropertyUIScale* )FindProperty( CStylePropertyUIScale::symbol );
	if ( pValue )
		return pValue->m_vUIScale.z;

	return CStylePropertyUIScale::GetDefaultZ();
}


//-----------------------------------------------------------------------------
// Purpose: clears all properties, animations, and transitions
//-----------------------------------------------------------------------------
void CPanelStyle::Clear( bool bIncludeClearingElementStyles /* = true */ )
{
	m_ScheduledTransitionCleanup.Cancel();
	FOR_EACH_VEC( m_vecProperties, i )
	{
		if( m_vecProperties[i].m_StyleSymbol == CStylePropertyBackgroundColor::symbol )
		{
			CStylePropertyBackgroundColor *pBackground = (CStylePropertyBackgroundColor*)m_vecProperties[i].m_pStyleProperty;
			for( int iInner = 0; iInner < pBackground->m_FillBrushCollection.GetNumParticleSystems(); ++iInner )
			{
				CTopLevelWindow *pWindow = (CTopLevelWindow*)m_pPanel->GetParentWindow();
				pWindow->GetUIRenderEngine()->QueueParticleSystemDelete( CPanelPtr<IUIPanel>( m_pPanel ).GetHandleAsUInt64(), iInner );
			}
		}
		CStylePropertyFactory::FreeStyleProperty( m_vecProperties[i].m_pStyleProperty );
	}
	m_vecProperties.RemoveAll();
	V_memset( m_rgPropsPresent, 0, sizeof( m_rgPropsPresent ) );
	for ( int i = 0; i < V_ARRAYSIZE( m_rgDirectPropIndex ); i++ )
	{
		m_rgDirectPropIndex[i] = NO_DIRECT_INDEX;
	}

	FOR_EACH_RBTREE_FAST( m_treePropertiesInTransition, i )
	{
		SAFE_DELETE( m_treePropertiesInTransition.Element( i ) );
	}
	m_treePropertiesInTransition.RemoveAll();

	if( m_pvecActiveAnimations )
	{
		m_pvecActiveAnimations->PurgeAndDeleteElements();
		SAFE_DELETE( m_pvecActiveAnimations );
	}

	if ( bIncludeClearingElementStyles && m_pVecElementProperties )
	{
		CUtlVector<StyleEntry_t> &vec = *m_pVecElementProperties;
		FOR_EACH_VEC( vec, i )
		{
			CStylePropertyFactory::FreeStyleProperty( vec[i].m_pStyleProperty );
		}
		SAFE_DELETE( m_pVecElementProperties );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Check if a property is present
//-----------------------------------------------------------------------------
bool CPanelStyle::BPropertyBitFlagSet( CStyleSymbol hSymbol )
{
	uint64 ulBitFlag = 0x1;
	if ( (int)hSymbol.GetID() < 64 )
	{
		ulBitFlag = ulBitFlag << (int)hSymbol.GetID();
		if ( (m_rgPropsPresent[0] & ulBitFlag) == 0 )
			return false;
	}
	else if ( (int)hSymbol.GetID() < 128 )
	{
		ulBitFlag = ulBitFlag << ((int)hSymbol.GetID() - 64);
		if ( (m_rgPropsPresent[1] & ulBitFlag) == 0 )
			return false;
	}
	else
	{
		AssertMsg( false, "Need to add bitflag space for new properties" );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Clear a property as present
//-----------------------------------------------------------------------------
void CPanelStyle::ClearHasProperty( CStyleSymbol hSymbol, int iIndex )
{
	uint64 ulBitFlag = 0x1;
	if ( (int)hSymbol.GetID() < 64 )
	{
		ulBitFlag = ulBitFlag << (int)hSymbol.GetID();
		m_rgPropsPresent[0] &= ~(ulBitFlag);
	}
	else if ( (int)hSymbol.GetID() < 128 )
	{
		ulBitFlag = ulBitFlag << ((int)hSymbol.GetID() - 64);
		m_rgPropsPresent[1] &= ~(ulBitFlag);
	}
	else
	{
		AssertMsg( false, "Need to add bitflag space for new properties" );
	}

	// Clear the slot for this symbol.
	if ( hSymbol.GetID() < V_ARRAYSIZE( m_rgDirectPropIndex ) )
	{
		m_rgDirectPropIndex[hSymbol.GetID()] = NO_DIRECT_INDEX;
	}
	// Adjust any indices that are higher than the
	// index being removed since the vector Remove will pack down.
	for ( int i = 0; i < V_ARRAYSIZE( m_rgDirectPropIndex ); i++ )
	{
		if ( m_rgDirectPropIndex[i] != NO_DIRECT_INDEX &&
			 (int)m_rgDirectPropIndex[i] > iIndex )
		{
			m_rgDirectPropIndex[i]--;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set a property as present
//-----------------------------------------------------------------------------
void CPanelStyle::SetHasProperty( CStyleSymbol hSymbol, int iIndex )
{
	uint64 ulBitFlag = 0x1;
	if ( (int)hSymbol.GetID() < 64 )
	{
		ulBitFlag = ulBitFlag << (int)hSymbol.GetID();
		m_rgPropsPresent[0] |= ulBitFlag;
	}
	else if ( (int)hSymbol.GetID() < 128 )
	{
		ulBitFlag = ulBitFlag << ((int)hSymbol.GetID() - 64);
		m_rgPropsPresent[1] |= ulBitFlag;
	}
	else
	{
		AssertMsg( false, "Need to add bitflag space for new properties" );
	}

	if ( hSymbol.GetID() < V_ARRAYSIZE( m_rgDirectPropIndex ) )
	{
		m_rgDirectPropIndex[hSymbol.GetID()] = (uint8)iIndex;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Find a property by name
//-----------------------------------------------------------------------------
CStyleProperty * CPanelStyle::FindProperty( CStyleSymbol hSymbol, int *pIndex )
{
	if ( pIndex )
		*pIndex = -1;

	if ( !BPropertyBitFlagSet( hSymbol) )
		return NULL;

	if ( hSymbol.GetID() < V_ARRAYSIZE( m_rgDirectPropIndex ) )
	{
		int iIndex = (int)m_rgDirectPropIndex[hSymbol.GetID()];
		AssertDbg( iIndex != NO_DIRECT_INDEX );
		if ( pIndex )
			*pIndex = iIndex;
		return m_vecProperties[iIndex].m_pStyleProperty;
	}

	FOR_EACH_VEC( m_vecProperties, i )
	{
		if( m_vecProperties[i].m_StyleSymbol == hSymbol )
		{
			if( pIndex )
				*pIndex = i;
			return m_vecProperties[i].m_pStyleProperty;
		}
	}

	// Shouldn't reach, the bitflag checks above should always catch unset cases
	Assert( false );
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Find property data for a possibly inherited property
//-----------------------------------------------------------------------------
void CPanelStyle::FindPropertyInfo( CStyleSymbol hSymbol, CStyleProperty **ppProperty, PropertyInTransition_t **ppTransitionData, CUtlVector< CActiveAnimation * > *pvecAnimations )
{
	bool bInherit = CStylePropertyFactory::BCanInheritProperty( hSymbol );

	Assert( ppProperty );
	*ppProperty = NULL;
	if ( ppTransitionData )
		*ppTransitionData = NULL;

	// below we loop checking panel style instead of just looping through panels because m_pPanel could be NULL and we still
	// want to check if the property exists in this panel style

	// find the first panel which has the requested property defined. Search both property and transition data.	
	CPanelStyle *pStyle = this;
	CPanelStyle *pStyleWithProperty = NULL;
	while ( pStyle )
	{
		*ppProperty = pStyle->FindProperty( hSymbol );

		if ( ppTransitionData )
		{
			short iTrans = -1;
			pStyle->FindPropertyInTransition( hSymbol, &iTrans );
			if ( iTrans != pStyle->m_treePropertiesInTransition.InvalidIndex() )
				*ppTransitionData = pStyle->m_treePropertiesInTransition.Element( iTrans );
		}
		
		// check if we found property data
		if ( *ppProperty != NULL || (ppTransitionData && *ppTransitionData != NULL) )
		{
			pStyleWithProperty = pStyle;
			break;
		}

		// if property can't inherit, we are done
		if ( !bInherit )
			break;

		pStyle = ( pStyle->m_pPanel && pStyle->m_pPanel->GetParent() ) ? ((CUIPanel*)pStyle->m_pPanel->GetParent())->AccessStyle() : NULL;
	}	

	// can exit early if caller does not want animation data
	if ( !pvecAnimations )
		return;

	// look again for animation data, but if inheriting the property, do not go past the panel where we found transition data
	pStyle = this;
	while ( pStyle )
	{
		bool bFound = false;
		if( pStyle->m_pvecActiveAnimations )
		{
			CUtlVector< CActiveAnimation * > &vec = *(pStyle->m_pvecActiveAnimations);
			pvecAnimations->EnsureCapacity( vec.Count() );
			FOR_EACH_VEC( vec, i )
			{
				if( !vec[i]->GetFrameData( hSymbol ) )
					continue;

				bFound = true;
				pvecAnimations->AddToTail( vec[i] );

				// keep looping through all animations on this panel
			}
		}

		if ( bFound )
			break;

		if ( !bInherit || pStyle == pStyleWithProperty )
			break;

		pStyle = ( pStyle->m_pPanel && pStyle->m_pPanel->GetParent() ) ? ((CUIPanel*)pStyle->m_pPanel->GetParent())->AccessStyle() : NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Find a property transition in progress by name
//-----------------------------------------------------------------------------
CStyleProperty * CPanelStyle::FindPropertyInTransition( CStyleSymbol hSymbol, short *pIndex )
{
	CStylePropertySearch findStyle( hSymbol );
	PropertyInTransition_t find;
	find.m_pStyleProperty = &findStyle;

	short ret = m_treePropertiesInTransition.Find( &find );

	if( pIndex )
		*pIndex = ret;

	// need to clear pointer to property so PropertyInTransition_t destructor doesn't try to free the property on stack
	find.m_pStyleProperty = NULL;

	if( ret == m_treePropertiesInTransition.InvalidIndex() )
		return NULL;
	
	return m_treePropertiesInTransition.Element( ret )->m_pStyleProperty;
}


//-----------------------------------------------------------------------------
// Purpose: Find transition requirements for a property by name
//-----------------------------------------------------------------------------
TransitionProperty_t *CPanelStyle::FindTransitionData( CStyleSymbol hSymbol )
{
	CStylePropertyTransitionProperties *p = (CStylePropertyTransitionProperties*)FindProperty( CStylePropertyTransitionProperties::symbol );
	if( p )
		return p->GetTransitionData( hSymbol );

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Return currently running animations.
//-----------------------------------------------------------------------------
void CPanelStyle::GetActiveAnimations( CUtlVector<CActiveAnimation *> *pvecAnimations )
{
	if ( m_pvecActiveAnimations )
	{
		CUtlVector< CActiveAnimation * > &vec = *( m_pvecActiveAnimations );
		pvecAnimations->EnsureCapacity( vec.Count() );
		FOR_EACH_VEC( vec, i )
		{
			pvecAnimations->AddToTail( vec[i] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Access a specific animation by name.
//-----------------------------------------------------------------------------
CActiveAnimation* CPanelStyle::FindActiveAnimation( CPanoramaSymbol animName )
{
	if ( m_pvecActiveAnimations )
	{
		CUtlVector<CActiveAnimation*> &vec = *m_pvecActiveAnimations;
		FOR_EACH_VEC( vec, i )
		{
			if ( vec[i]->GetAnimationData().m_symName == animName )
			{
				return vec[i];
			}
		}
	}

	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Check if any properties are transitioning
//-----------------------------------------------------------------------------
bool CPanelStyle::BHasAnyTransition()
{
	return (m_treePropertiesInTransition.Count() > 0);
}


//-----------------------------------------------------------------------------
// Purpose: Determine if the property has any transition
//-----------------------------------------------------------------------------
bool CPanelStyle::BHasTransition( CStyleSymbol hSymbolProperty )
{
	CStyleProperty *pProp = FindPropertyInTransition( hSymbolProperty );
	return pProp != NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Determine if the property has any transition or animation
//-----------------------------------------------------------------------------
bool CPanelStyle::BHasTransitionOrAnimation( CStyleSymbol hSymbolProperty )
{
	if ( BHasTransition( hSymbolProperty ) )
		return true;

	if( m_pvecActiveAnimations )
	{
		CUtlVector< CActiveAnimation * > &vec = *m_pvecActiveAnimations;
		FOR_EACH_VEC( vec, i )
		{
			if( vec[i]->BHasFrameDataForProperty( hSymbolProperty ) )
				return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Determine if the property has any transition or animation
//-----------------------------------------------------------------------------
bool CPanelStyle::BHasAnyTransitionOrAnimation( bool bExcludeStylesImpactingOnlyCompositing )
{
	if ( BHasAnimatingBackground() )
		return true;

	if ( !bExcludeStylesImpactingOnlyCompositing )
	{
		if ( m_treePropertiesInTransition.Count() )
			return true;

		if ( m_pvecActiveAnimations && m_pvecActiveAnimations->Count() )
			return true;

		return false;
	}
	else
	{
		FOR_EACH_RBTREE_FAST( m_treePropertiesInTransition, i )
		{
			if ( !m_treePropertiesInTransition[i]->m_pStyleProperty->BAffectsCompositionOnly() )
			{
				return true;
			}
		}

		if( m_pvecActiveAnimations )
		{
			CUtlVector< CActiveAnimation * > &vec = *m_pvecActiveAnimations;
			FOR_EACH_VEC( vec, i )
			{
				if( !vec[i]->BAffectsCompositionOnly() )
					return true;
			}
		}

		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Determine if panel has an animating background (movie)
//-----------------------------------------------------------------------------
bool CPanelStyle::BHasAnimatingBackground()
{
	CStylePropertyBackgroundImage *pProperty = (CStylePropertyBackgroundImage*)FindProperty( CStylePropertyBackgroundImage::symbol );
	if ( pProperty )
	{
		if ( pProperty->BHasActiveMovie() )
			return true;
	}

	CStylePropertyBackgroundColor *pColor = (CStylePropertyBackgroundColor*)FindProperty( CStylePropertyBackgroundColor::symbol );
	if( pColor )
	{
		if( pColor->BHasAnyParticleSystems() )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Stops any background movies which are animating
//-----------------------------------------------------------------------------
void CPanelStyle::EnableBackgroundMovies( bool bEnabled )
{
	CStylePropertyBackgroundImage *pProperty = (CStylePropertyBackgroundImage*)FindProperty( CStylePropertyBackgroundImage::symbol );
	if( pProperty )
	{
		pProperty->EnableBackgroundMovies( bEnabled );		
	}
}


//-----------------------------------------------------------------------------
// Purpose: Checks if the panel is completely transparent, and has no current animation/transition of opacity.  
// If this is true it means we don't need to draw the panel.
//-----------------------------------------------------------------------------
bool CPanelStyle::BIsTransparentWithNoOpacityTransition()
{
	float flCurOpacity = 0.0f;
	GetOpacity( flCurOpacity );

	if ( flCurOpacity < 0.00001f )
	{
		if ( !BHasTransitionOrAnimation( CStylePropertyOpacity::symbol ) )
		{
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Determine if a style property has any data at all (base, transition, or animation)
//-----------------------------------------------------------------------------
bool CPanelStyle::BHasAnyStyleDataForProperty( CStyleSymbol hSymbolProperty )
{
	bool bInherit = CStylePropertyFactory::BCanInheritProperty( hSymbolProperty );

	// below we loop checking panel style instead of just looping through panels because m_pPanel could be NULL and we still
	// want to check if the property exists in this panel style

	CPanelStyle *pStyle = this;
	while ( pStyle )
	{
		if ( pStyle->BPropertyBitFlagSet( hSymbolProperty ) || pStyle->BHasTransitionOrAnimation( hSymbolProperty ) )
		{
			return true;
		}

		// if property can't inherit, we are done
		if ( !bInherit )
			break;

		pStyle = ( pStyle->m_pPanel && pStyle->m_pPanel->GetParent() ) ? ( (CUIPanel*)pStyle->m_pPanel->GetParent() )->AccessStyle() : NULL;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Figure out what the interpolated value of a property is right now
//-----------------------------------------------------------------------------
CStyleProperty *CPanelStyle::GetInterpolatedProperty( CStyleSymbol hSymbol, bool bFinal )
{
	CStyleProperty *pProperty = CStylePropertyFactory::CreateStyleProperty( hSymbol );

	// bugbug jmccaskey/cboyd - this doesn't actually work for animations... it should?

	short iInProgress = -1;
	FindPropertyInTransition( hSymbol, &iInProgress );
	CStyleProperty *pCurrent = FindProperty( hSymbol );

	if ( iInProgress != m_treePropertiesInTransition.InvalidIndex() )
	{
		PropertyInTransition_t *pTransition = m_treePropertiesInTransition[iInProgress];

		// if only interested in the final value, can exit early
		if ( bFinal )
		{
			pTransition->m_pStyleProperty->MergeTo( pProperty );
			return pProperty;
		}

		if( pCurrent )
		{
			// Copy data out to our new instance so we can interpolate on it without modifying source data
			pCurrent->MergeTo( pProperty );
		}

		double flNow = UIEngine()->GetCurrentFrameTime();
		double flTimeProgress = 1.0f;
		
		if ( pTransition->m_transitionData.m_flTransitionSeconds > 0.0f )
		{
			flTimeProgress = ( (flNow - pTransition->m_flTransitionStartTime) - GetScaledTransitionTime( pTransition->m_transitionData.m_flTransitionDelaySeconds ) )
				/ GetScaledTransitionTime( pTransition->m_transitionData.m_flTransitionSeconds );
			flTimeProgress = clamp( flTimeProgress, 0.0f, 1.0f );
		}
		else
		{
			char szPanelName[1024];
			GetDebugPanelName( szPanelName, sizeof( szPanelName ), m_pPanel );
			Msg( "Invalid transition time set for %s\n", szPanelName );			
		}

		// If we appear to be done animating, then complete now
		if ( flTimeProgress >= 1.0f )
		{
			pProperty->Interpolate( m_pPanel, *pTransition->m_pStyleProperty, flTimeProgress );
			CompletePropertyTransitionNow( hSymbol, true );
		}
		else
		{
			flTimeProgress = GetProgressForTimingFunction( pTransition->m_transitionData.m_CubicBezier, flTimeProgress );
			pProperty->Interpolate( m_pPanel, *pTransition->m_pStyleProperty, flTimeProgress );
		}		
	}
	else
	{
		if( pCurrent )
		{
			// Copy data out to our new instance so we can return it
			pCurrent->MergeTo( pProperty );
		}
		else
		{
			// If we had no property in transition, and we had no base property, then return defaults, rather than invalid data... but we shoulnd't hit this
			AssertMsg( false, "Called GetInterpolatedProperty but there is no transition or base property value..." );
			pProperty->ResolveDefaultValues();
		}
	}

	return pProperty;
}


//-----------------------------------------------------------------------------
// Purpose: Updates the property immediately, removing active transition.  Will 
// interpolate to current time, will not force updating all the way to end of transition.
//-----------------------------------------------------------------------------
CStyleProperty *CPanelStyle::CompletePropertyTransitionNow( CStyleSymbol hSymbol, bool bDeleteTargetProperty )
{
	// Figure out if any transition is occurring on the specified property, then perform
	// interpolation up to now and clear the transition in progress.

	CStyleProperty *pRet = NULL;

	short iInProgress = -1;
	FindPropertyInTransition( hSymbol, &iInProgress );
	if ( iInProgress != m_treePropertiesInTransition.InvalidIndex() )
	{
		CStyleProperty *pCurrent = FindProperty( hSymbol );
		if( pCurrent == NULL )
		{
			// Add the default, so we can transition from it since we have a target value but no current
			int iVec = m_vecProperties.AddToTail();
			m_vecProperties[iVec].m_StyleSymbol = hSymbol;
			pCurrent = m_vecProperties[iVec].m_pStyleProperty = CStylePropertyFactory::CreateStyleProperty( hSymbol );
			
			pCurrent->ResolveDefaultValues();
			pCurrent->ApplyUIScaleFactor( GetActualUIScale(), GetParentActualUIScale() );

			SetHasProperty( hSymbol, iVec );
		}

		PropertyInTransition_t *pTransition = m_treePropertiesInTransition[iInProgress];
		double flNow = UIEngine()->GetCurrentFrameTime();

		if ( m_pPanel )
		{
			flNow = m_pPanel->StopAnimationOfPropertyUntilFrameUpdateAndGetStopTime( hSymbol.GetID() );
		}
					
		float flTimeProgress = GetTimeProgress( pTransition->m_transitionData.m_CubicBezier, pTransition->m_flTransitionStartTime, flNow, 
			GetScaledTransitionTime ( pTransition->m_transitionData.m_flTransitionDelaySeconds ), GetScaledTransitionTime( pTransition->m_transitionData.m_flTransitionSeconds ) );

		//Msg( "****** %f CompletePropertyTransitionNow %s start=%f progress=%f\n", flNow, hSymbol.String(), pTransition->m_flTransitionStartTime, flTimeProgress );
		pCurrent->Interpolate( m_pPanel, *m_treePropertiesInTransition[iInProgress]->m_pStyleProperty, flTimeProgress );

		if ( !bDeleteTargetProperty )
		{
			pRet = m_treePropertiesInTransition[iInProgress]->m_pStyleProperty;
			m_treePropertiesInTransition[iInProgress]->m_pStyleProperty = NULL;
		}

		delete m_treePropertiesInTransition[iInProgress];
		m_treePropertiesInTransition.RemoveAt( iInProgress );
	}

	return pRet;
}


//-----------------------------------------------------------------------------
// Purpose: Setter on arbitrary base property from code
//-----------------------------------------------------------------------------
void CPanelStyle::SetProperty( CStyleProperty *pProperty, bool bAllowTransition )
{
	if ( !m_pVecElementProperties )
		m_pVecElementProperties = new CUtlVector<StyleEntry_t>();

	// if there is a current property set into the element properties map just clobber it
	CUtlVector<StyleEntry_t> &vec = *m_pVecElementProperties;
	FOR_EACH_VEC( vec, i )
	{
		if ( vec[ i ].m_StyleSymbol == pProperty->GetPropertySymbol() )
		{
			// If old and new property match free the new one and early out so we don't dirty the styles and force re-apply
			CStyleProperty *pOld = vec[ i ].m_pStyleProperty;
			if ( *pOld == *pProperty && pOld->BDisallowTransition() == pProperty->BDisallowTransition() )
			{
				CStylePropertyFactory::FreeStyleProperty( pProperty );
				return;
			}

			// merge old into new preserving what it set
			pOld->MergeTo( pProperty );

			CStylePropertyFactory::FreeStyleProperty( pOld );
			vec.Remove( i );
		}
	}

	// Set disallow transition correctly after merge (could have taken old value in merge)
	pProperty->SetDisallowTransition( !bAllowTransition );

	// record that this property has been set by code so styles do not override the setting
	int iNew = vec.AddToTail();
	vec[ iNew ].m_StyleSymbol = pProperty->GetPropertySymbol();
	vec[ iNew ].m_pStyleProperty = pProperty;

	// above set element style property which will be merged with the style properties from CSS files next apply styles pass
	// if already dirty, just let that happen next pass
	if ( !m_pPanel || m_pPanel->BStylesDirty() )
		return;

	// for perf reasons, we don't mark the panel as having to reapply styles. Instead, just apply the new style now so large grids, carousels and flowing
	// layouts don't have to look up style matches
	CStyleProperty *pToApply = CStylePropertyFactory::CreateStyleProperty( pProperty->GetPropertySymbol() );
	pProperty->MergeTo( pToApply );

	CStyleProperty *pExistingProperty = FindPropertyInTransition( pProperty->GetPropertySymbol() );
	if ( !pExistingProperty )
		pExistingProperty = FindProperty( pProperty->GetPropertySymbol() );

	// as we merged the old and new property values from code above, any new data that is merged in from the call below should be parts of a partial property
	// that came from style files
	if ( pExistingProperty )
		pExistingProperty->MergeTo( pToApply );
	

	// really slam (old property could have had allow transition
	pToApply->SetDisallowTransition( !bAllowTransition );
	if ( SetPropertyFromStyle( pToApply ) && AccessPanel() )
	{
		EStyleRepaint eRepaint = pToApply->BAffectsCompositionOnly() ? k_EStyleRepaintComposition : k_EStyleRepaintFull;
		bool bInheritablePropertiesChanged = CStylePropertyFactory::BCanInheritProperty( pToApply->GetPropertySymbol() );

		// Hack: we early out OnContentSizeTraverse on panels with opacity = 0, so if opacity is changing
		// we need to re-run they layout traverse on that panel.
		if ( ( pToApply->GetPropertySymbol() == CStylePropertyOpacity::symbol ) && !AccessPanel()->BInvalidateSizeAndPositionOnOpacityChangeDisabled() )
		{
			AccessPanel()->InvalidateSizeAndPosition();
		}

		// We early out in the "apply style traverse" if a panel is not visible or is transparent, so if the visibility or opacity 
		// is changing we need to re-run the "apply style traverse" if styles are dirty for any of the panel's children.
		// (Ideally we only need to re-run the "apply style traverse" if the panel become visible or the opacity is changing
		// from 0 to any other value)
		if ( ( pToApply->GetPropertySymbol() == CStylePropertyVisible::symbol ) || ( pToApply->GetPropertySymbol() == CStylePropertyOpacity::symbol ) )
		{
			if ( AccessPanel()->BChildStylesDirty() )
			{
				AccessPanel()->MarkChildStylesDirtyOnParents();
			}
		}

		AccessPanel()->AfterStylesApplied( true, eRepaint, bInheritablePropertiesChanged, false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Merge-based setter on arbitrary base property from code
//-----------------------------------------------------------------------------
void CPanelStyle::MergeProperty( CStyleProperty *pProperty, bool bAllowTransition )
{
	if ( !m_pVecElementProperties )
		m_pVecElementProperties = new CUtlVector<StyleEntry_t>();

	// Look for an existing property for this symbol. If we find it, merge in
	// those values into our new property (it will only write over unset values
	// on our new property) and set it as our new value.
	CUtlVector<StyleEntry_t> &vec = *m_pVecElementProperties;
	FOR_EACH_VEC( vec, i )
	{
		if ( vec[ i ].m_StyleSymbol != pProperty->GetPropertySymbol() )
			continue;

		CStyleProperty *pExistingProperty = vec[ i ].m_pStyleProperty;
		pExistingProperty->MergeTo( pProperty );
		break;
	}

	return SetProperty( pProperty, bAllowTransition );
}


//-----------------------------------------------------------------------------
// Purpose: Setter on arbitrary base property
//			Does not update symbol
// Returns: true if property changed.. false otherwise
//-----------------------------------------------------------------------------
bool CPanelStyle::SetPropertyFromStyle( CStyleProperty *pProperty )
{
	VPROF_BUDGET_DETAILED( "CPanelStyle::SetPropertyFromStyle", VPROF_BUDGETGROUP_TENFOOT );

	// let the property know it is being assigned to a panel so it can finalize any unset values, as well as apply any scaling
	pProperty->ResolveDefaultValues();

	// let the property know about any scaling the panel is applying, so it can set it's data correctly
	pProperty->ApplyUIScaleFactor( GetActualUIScale(), GetParentActualUIScale() );

	// check if the new property values match the property values already set. If so, do not reapply, as that would cause
	// the property to transition (causes events to refire, etc.)
	CStyleProperty *pCurrentProperty = FindProperty( pProperty->GetPropertySymbol() );
	
	// May also (or alternatively) already have the property but set as a transition, transitions are the real
	// current values more so than the ones in the hash which are possibly stale from before transitions starting
	CStyleProperty *pTransition = FindPropertyInTransition( pProperty->GetPropertySymbol() );
	if( pTransition )
		pCurrentProperty = pTransition;
	
	if ( pCurrentProperty && *pCurrentProperty == *pProperty && pCurrentProperty->BDisallowTransition() == pProperty->BDisallowTransition() )
	{
		CStylePropertyFactory::FreeStyleProperty( pProperty );
		return false;
	}

	// check if previous style had transition position, so we can update panel if necessary later
	bool bTransitionPositionPreviouslySet = false;
	if ( pProperty->GetPropertySymbol() == CStylePropertyTransitionProperties::symbol && pCurrentProperty )
	{
		CStylePropertyTransitionProperties *pOldTransProp = (CStylePropertyTransitionProperties *)pCurrentProperty;
		bTransitionPositionPreviouslySet = (pOldTransProp->GetTransitionData( CStylePropertyPosition::symbol ) != NULL);
	}

	// Force completion of previous transition up until this time, can't have multiple in flight
	// on a single property, new position will transition from where the old left off, if appropriate.
	CompletePropertyTransitionNow( pProperty->GetPropertySymbol(), true );

	// See if there is transition data defining a new transition, for properties that support that.
	TransitionProperty_t *pTransitionData = NULL;
	if ( pProperty->BCanTransition() )
	{
		if ( !pProperty->BDisallowTransition() )
		{
			pTransitionData = FindTransitionData( pProperty->GetPropertySymbol() );
		}
		else
		{
			// remove any active transition for this property
			short iInProgress = -1;
			FindPropertyInTransition( pProperty->GetPropertySymbol(), &iInProgress );
			if ( iInProgress != m_treePropertiesInTransition.InvalidIndex() )
			{
				delete m_treePropertiesInTransition[iInProgress];
				m_treePropertiesInTransition.RemoveAt( iInProgress );

				if ( pProperty->BInvalidatesPosition( NULL ) || pProperty->BInvalidatesSizeAndPosition( NULL ) )
					SetPanelLayoutFlagsForTransitionAnimation();

				if ( m_pPanel )
					DispatchEvent( PropertyTransitionEnd(), m_pPanel, pProperty->GetPropertySymbol() );
			}
		}
	}

	CStyleProperty *pPriorStyleProperty = NULL;
	bool bDeletePrior = false;
	if ( pTransitionData && pTransitionData->m_flTransitionSeconds > 0.0f )
	{
		// Rather than setting the new property, set a transition in progress
		PropertyInTransition_t *pInTransition = new PropertyInTransition_t;
		pInTransition->m_flTransitionStartTime = UIEngine()->GetCurrentFrameTime();
		pInTransition->m_pStyleProperty = pProperty;
		pInTransition->m_transitionData = *pTransitionData;

		m_treePropertiesInTransition.Insert( pInTransition );
		ScheduleTransitionCleanup( pInTransition->m_flTransitionStartTime + GetScaledTransitionTime( pInTransition->m_transitionData.m_flTransitionDelaySeconds ) );

		pPriorStyleProperty = FindProperty( pProperty->GetPropertySymbol() );

		pProperty->OnStartingTransition( pPriorStyleProperty );
	}
	else
	{
		// Not setup to transition, so just set the new property, deleting the old one if present.
		int iIndex = -1;
		pPriorStyleProperty = FindProperty( pProperty->GetPropertySymbol(), &iIndex );
		if( pPriorStyleProperty )
		{
			bDeletePrior = true;
		}
		else
		{
			iIndex = m_vecProperties.AddToTail();
			m_vecProperties[iIndex].m_StyleSymbol = pProperty->GetPropertySymbol();
			SetHasProperty( pProperty->GetPropertySymbol(), iIndex );
		}

		m_vecProperties[iIndex].m_pStyleProperty = pProperty;
	}

	SetPropertyFromStyleHelper( pProperty, pPriorStyleProperty, bDeletePrior );

	// special handling for transition property
	if ( pProperty->GetPropertySymbol() == CStylePropertyTransitionProperties::symbol )
		OnNewTransitionPropertySet( (CStylePropertyTransitionProperties *)pProperty, bTransitionPositionPreviouslySet );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Setter on arbitrary base property
//			Does not update symbol
//			Simplified version of CPanelStyle::SetPropertyFromStyle called from 
//			CStyleFileSet::ApplyMatchedStylesToPanelStyle for CPanelStyle with 
//			no transition data. (only valid if CPanelStyle has no transition properties and 
//			CStyleFileSet::ApplyMatchedStylesToPanelStyle will not apply any transition properties
// Returns: true if property changed.. false otherwise
//-----------------------------------------------------------------------------
bool CPanelStyle::SetPropertyFromStyleSimple( CStyleProperty *pProperty )
{
	VPROF_BUDGET_DETAILED( "CPanelStyle::SetPropertyFromStyleSimple", VPROF_BUDGETGROUP_TENFOOT );

	// let the property know it is being assigned to a panel so it can finalize any unset values, as well as apply any scaling
	pProperty->ResolveDefaultValues();

	// let the property know about any scaling the panel is applying, so it can set it's data correctly
	pProperty->ApplyUIScaleFactor( GetActualUIScale(), GetParentActualUIScale() );

	// check if the new property values match the property values already set. If so, do not reapply, as that would cause
	// the property to transition (causes events to refire, etc.)
	int iPriorPropertyIndex = -1;
	CStyleProperty *pPriorStyleProperty = FindProperty( pProperty->GetPropertySymbol(), &iPriorPropertyIndex );

	if ( pPriorStyleProperty && *pPriorStyleProperty == *pProperty && pPriorStyleProperty->BDisallowTransition() == pProperty->BDisallowTransition() )
	{
		CStylePropertyFactory::FreeStyleProperty( pProperty );
		return false;
	}
	
	// Not setup to transition, so just set the new property, deleting the old one if present.
	if ( !pPriorStyleProperty )
	{
		iPriorPropertyIndex = m_vecProperties.AddToTail();
		m_vecProperties[iPriorPropertyIndex].m_StyleSymbol = pProperty->GetPropertySymbol();
		SetHasProperty( pProperty->GetPropertySymbol(), iPriorPropertyIndex );
	}

	m_vecProperties[iPriorPropertyIndex].m_pStyleProperty = pProperty;

	SetPropertyFromStyleHelper( pProperty, pPriorStyleProperty, ( pPriorStyleProperty != nullptr ) );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPanelStyle::SetPropertyFromStyleHelper( CStyleProperty *pProperty, CStyleProperty *pPriorStyleProperty, bool bDeletePrior )
{
	pProperty->OnAppliedToPanel( m_pPanel );

	// if necessary, invalidate layout
	if ( m_pPanel && pProperty->BInvalidatesSizeAndPosition( pPriorStyleProperty ) )
	{
		if ( s_convarPanoramaSpewFullInvalidates.GetBool() )
		{
			const char *pchPanel = m_pPanel->GetID();
			CFmtStr1024 strName;
			if ( pchPanel == NULL || pchPanel[0] == '\0' )
			{
				if ( m_pPanel->GetParent() && m_pPanel->GetParent()->GetID() )
				{
					strName.AppendFormat( "(unset) w/parent: %s", m_pPanel->GetParent()->GetID() );
					pchPanel = strName.Access();
				}
				else
				{
					pchPanel = "(unset)";
				}
			}

			Msg( "Invalidating fully because style %s changed on panel %s\n", pProperty->GetPropertySymbol().String(), pchPanel );
		}
		m_pPanel->InvalidateSizeAndPosition();
	}

	if ( m_pPanel && pProperty->BInvalidatesPosition( pPriorStyleProperty ) )
		m_pPanel->InvalidatePosition();

	// special handling for background-color to see if a particle system has now been removed completely
	if ( pPriorStyleProperty && pProperty->GetPropertySymbol() == CStylePropertyBackgroundColor::symbol && m_pPanel )
	{
		CStylePropertyBackgroundColor *pOld = (CStylePropertyBackgroundColor*)pPriorStyleProperty;
		CStylePropertyBackgroundColor *pNew = (CStylePropertyBackgroundColor*)pProperty;

		int nNumOld = pOld->m_FillBrushCollection.GetNumParticleSystems();
		int nNumNew = pNew->m_FillBrushCollection.GetNumParticleSystems();

		if ( nNumNew < nNumOld )
		{
			while ( nNumOld > nNumNew )
			{
				((CTopLevelWindow*)m_pPanel->GetParentWindow())->GetUIRenderEngine()->QueueParticleSystemDelete( CPanelPtr<IUIPanel>( m_pPanel ).GetHandleAsUInt64(), nNumOld - 1 );
				--nNumOld;
			}
		}
	}

	if ( pPriorStyleProperty && bDeletePrior )
		CStylePropertyFactory::FreeStyleProperty( pPriorStyleProperty );

	// special handling for animation property
	if ( pProperty->GetPropertySymbol() == CStylePropertyAnimationProperties::symbol )
		SetAnimationProperties( (CStylePropertyAnimationProperties *)pProperty );

	if ( pProperty->GetPropertySymbol() == CStylePropertyEntranceSound::symbol )
	{
		CStylePropertySound *pSoundProperty = ( CStylePropertySound * )pProperty;
		pSoundProperty->PlaySoundOnPanel( m_pPanel );
	}
	else if(pProperty->GetPropertySymbol() == CStylePropertyTransitionSound::symbol)
	{
		CStylePropertySound *pSoundProperty = (CStylePropertySound *)pProperty;
		pSoundProperty->PlaySoundOnPanel( m_pPanel, pPriorStyleProperty );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when new transition property values have been set
//-----------------------------------------------------------------------------
void CPanelStyle::OnNewTransitionPropertySet( CStylePropertyTransitionProperties *pProperty, bool bTransitionPositionPreviouslySet )
{
	// check if adding position
	if ( !bTransitionPositionPreviouslySet )
	{
		if ( pProperty->GetTransitionData( CStylePropertyPosition::symbol ) != NULL && m_pPanel )
			m_pPanel->TransitionPositionApplied( pProperty->m_bImmediate );
	}

	// if not immediate, nothing more to do
	if ( !pProperty->m_bImmediate )
		return;

	CUtlVector< PropertyInTransition_t* > vecNewPropertiesInTransition;
	FOR_EACH_RBTREE_FAST( m_treePropertiesInTransition, i )
	{
		PropertyInTransition_t *pPropertyInTransition = m_treePropertiesInTransition[i];
		CStyleSymbol symProperty = pPropertyInTransition->m_pStyleProperty->GetPropertySymbol();
		TransitionProperty_t *pTransitionData = pProperty->GetTransitionData( symProperty );

		if ( !pTransitionData )
		{
			// property is no longer in transition, stop active transition
			pPropertyInTransition->m_transitionData.m_flTransitionDelaySeconds = 0.0f;
			pPropertyInTransition->m_transitionData.m_flTransitionSeconds = 0.0f;
			ScheduleTransitionCleanup( UIEngine()->GetCurrentFrameTime() );
		}
		else
		{
			// recalculate new
			CStyleProperty *pTarget = CompletePropertyTransitionNow( symProperty, false );

			// Rather than setting the new property, set a transition in progress
			PropertyInTransition_t *pInTransition = new PropertyInTransition_t;
			pInTransition->m_flTransitionStartTime = UIEngine()->GetCurrentFrameTime();
			pInTransition->m_pStyleProperty = pTarget;
			pInTransition->m_transitionData = *pTransitionData;

			vecNewPropertiesInTransition.AddToTail( pInTransition );
		}
	}

	FOR_EACH_VEC( vecNewPropertiesInTransition, i )
	{
		PropertyInTransition_t *pInTransition = vecNewPropertiesInTransition[i];
		m_treePropertiesInTransition.Insert( pInTransition );
		ScheduleTransitionCleanup( pInTransition->m_flTransitionStartTime + GetScaledTransitionTime( pInTransition->m_transitionData.m_flTransitionDelaySeconds ) );
	}	
}


//-----------------------------------------------------------------------------
// Purpose: Checks if specified animation is already active on this panel
//-----------------------------------------------------------------------------
int CPanelStyle::GetActiveAnimation( CPanoramaSymbol symAnimation )
{
	if( m_pvecActiveAnimations )
	{
		CUtlVector< CActiveAnimation * > &vec = *m_pvecActiveAnimations;
		FOR_EACH_VEC( vec, i )
		{
			if( vec[i]->GetName() == symAnimation )
				return i;
		}
	}
	
	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: Clears current animation properties and sets new ones
//-----------------------------------------------------------------------------
void CPanelStyle::SetAnimationProperties( CStylePropertyAnimationProperties *pAnimationProperties )
{
	if ( !m_pPanel )
		return;

	LayoutFilePtr_t pLayoutFile = UIEngineInternal()->UILayoutManagerInternal()->GetCLayoutFile( m_pPanel->GetLayoutFile() );
	if ( !pLayoutFile )
	{
		AssertMsg( false, "Couldn't find layout file" );
		return;
	}

	// The calling code has already verified that something about the animation property has changed, so we are free
	// to nuke our current animations and restart all. Build a temp array of active animations which will be swapped
	// with the panel's at the end of this function.
	CUtlVector< AnimationProperty_t > &vecAnimations = pAnimationProperties->m_vecAnimationProperties;
	CUtlVector< CActiveAnimation * > vecTempAnimations( 0, vecAnimations.Count() );

	// add each animation. Important to keep the animation order set by the property
	FOR_EACH_VEC( vecAnimations, iAnim )
	{
		AnimationProperty_t &animation = vecAnimations[iAnim];

		const CStyleAnimation *pStyleAnimation = pLayoutFile->GetAnimation( animation.m_symName );
		if ( !pStyleAnimation )
		{
			LogLayoutParsingError( pLayoutFile->GetLayoutFileSymbol(), 0, "Trying to apply missing animation: %s", animation.m_symName.String() );
			continue;
		}

		const VecKeyFrames_t &vecKeyFrames = pStyleAnimation->GetFrames();
		AddNewActiveAnimationToVec( vecTempAnimations, animation, vecKeyFrames );

		// process next animation
	}

	SetActiveAnimations( vecTempAnimations );
}

//-----------------------------------------------------------------------------
// Purpose: Sets a single animation from given keyframes
//-----------------------------------------------------------------------------
void CPanelStyle::SetSingleAnimation( AnimationProperty_t animation, VecKeyFrames_t &vecKeyframes )
{
	if ( !m_pPanel )
		return;

	CUtlVector< CActiveAnimation * > vecTempAnimations( 0, 1 );
	AddNewActiveAnimationToVec( vecTempAnimations, animation, vecKeyframes );
	SetActiveAnimations( vecTempAnimations );
}

//-----------------------------------------------------------------------------
// Purpose: Helper to add new entry to given animations vec
//-----------------------------------------------------------------------------
void CPanelStyle::AddNewActiveAnimationToVec(  CUtlVector<CActiveAnimation*> &vec, AnimationProperty_t &animation, const VecKeyFrames_t &vecKeyframes )
{
	// new animation
	CActiveAnimation *pActiveAnimation = new CActiveAnimation( UIEngine()->GetCurrentFrameTime(), animation );
	vec.AddToTail( pActiveAnimation );

	// Break key frames into per property key frames
	FOR_EACH_VEC( vecKeyframes, iFrame )
	{
		const CStyleKeyFrame *pFrame = vecKeyframes.Element( iFrame );

		// check if this frame has overridden the default timing function
		StylePropertyHash_t *pProperties = pFrame->GetProperties();
		FOR_EACH_HASHMAP( *pProperties, iProperty )
		{
			pActiveAnimation->AddFrameData( pFrame->GetPercent(), pFrame->GetTimingFunction(), pFrame->GetCubicBezier(), pProperties->Element( iProperty ), GetActualUIScale(), GetParentActualUIScale() );
		}

		// process next frame
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets current animation properties from input vec. Input vec is 
// reset on return
//-----------------------------------------------------------------------------
void CPanelStyle::SetActiveAnimations( CUtlVector<CActiveAnimation *> &vecAnimations )
{
	// swap in active, ordered animations
	if( vecAnimations.Count() )
	{
		if( !m_pvecActiveAnimations )
			m_pvecActiveAnimations = new CUtlVector < CActiveAnimation * > ;

		m_pvecActiveAnimations->Swap( vecAnimations );
	}
	else if ( m_pvecActiveAnimations )
	{
		// vecAnimations is empty, but we still need to dispatch an AnimationEnd() event for our current animations
		m_pvecActiveAnimations->Swap( vecAnimations );

		// delete empty vector
		Assert( m_pvecActiveAnimations->Count() == 0 );
		m_pvecActiveAnimations->PurgeAndDeleteElements();
		delete m_pvecActiveAnimations;
		m_pvecActiveAnimations = NULL;
	}

	// for each animation which no longer exists, dispatch an event
	FOR_EACH_VEC( vecAnimations, i )
	{
		CPanoramaSymbol symAnimation = vecAnimations[i]->GetName();
		int iVecActive = GetActiveAnimation( symAnimation );
		if ( m_pPanel && iVecActive == -1 )
			DispatchEventAsync( 0.0f, AnimationEnd(), m_pPanel, symAnimation );
	}

	// vecAnimations now contains only animations which are no longer valid
	vecAnimations.PurgeAndDeleteElements();

	CleanupTransitionsAndAnimations();
}


//-----------------------------------------------------------------------------
// Purpose: Clears set property data
//-----------------------------------------------------------------------------
bool CPanelStyle::RemoveProperty( CStyleSymbol symProperty )
{
	bool bChanged = false;
	TransitionProperty_t *pTransitionData = FindTransitionData( symProperty );
	if ( pTransitionData )
	{
		// inherited properties need to inherit their parent's value
		CStyleProperty *pInherited = NULL;
		if ( CStylePropertyFactory::BCanInheritProperty( symProperty ) )
		{
			// check parents, starting with parent style
			CPanelStyle *pParentStyle = NULL;
			if ( m_pPanel && m_pPanel->GetParent() )
				pParentStyle = ((CUIPanel*)m_pPanel->GetParent())->AccessStyle();

			if ( pParentStyle )
			{
				CStyleProperty *pBase = NULL;
				PropertyInTransition_t *pTransition = NULL;
				pParentStyle->FindPropertyInfo( symProperty, &pBase, &pTransition, NULL );

				if ( pTransition )
					pInherited = pTransition->m_pStyleProperty;
				else
					pInherited = pBase;
			}
			
		}

		// as this property can transition, we need to transition back to the default
		CStyleProperty *pProperty = CStylePropertyFactory::CreateStyleProperty( symProperty );
		if ( pInherited )
			pInherited->MergeTo( pProperty );
		
		bChanged = SetPropertyFromStyle( pProperty );
		pProperty = NULL;
		
		// SetPropertyFromStyle handles invalidating size and position on panel
	}
	else
	{
		bool bInvalidateSizeAndPosition = false;
		bool bInvalidatePosition = false;

		// no transition. Just remove all related to this property
		int iIndex = -1;
		CStyleProperty *pProperty = FindProperty( symProperty, &iIndex );
		if( pProperty )
		{
			// Special case to play the exit sound before destroying the property
			if ( pProperty->GetPropertySymbol() == CStylePropertyExitSound::symbol )
			{
				CStylePropertySound *pSoundProperty = ( CStylePropertySound * )pProperty;
				pSoundProperty->PlaySoundOnPanel( m_pPanel );
			}

			bInvalidateSizeAndPosition = pProperty->BInvalidatesSizeAndPosition( NULL );
			bInvalidatePosition = pProperty->BInvalidatesPosition( NULL );

			ClearHasProperty( pProperty->GetPropertySymbol(), iIndex );
			CStylePropertyFactory::FreeStyleProperty( pProperty );
			m_vecProperties.Remove( iIndex );

			bChanged = true;
		}

		short iProp = -1;
		FindPropertyInTransition( symProperty, &iProp );
		if ( iProp != m_treePropertiesInTransition.InvalidIndex() )
		{
			PropertyInTransition_t *pPropertyInTrans = m_treePropertiesInTransition.Element( iProp );
			if( pPropertyInTrans->m_pStyleProperty->BInvalidatesSizeAndPosition( NULL ) )
				bInvalidateSizeAndPosition = true;
			if( pPropertyInTrans->m_pStyleProperty->BInvalidatesPosition( NULL ) )
				bInvalidatePosition = true;

			SAFE_DELETE( pPropertyInTrans );
			m_treePropertiesInTransition.RemoveAt( iProp );

			bChanged = true;
		}

		// if necessary, invalidate layout
		if ( m_pPanel && bInvalidateSizeAndPosition )
			m_pPanel->InvalidateSizeAndPosition();
		else if ( m_pPanel && bInvalidatePosition )
			m_pPanel->InvalidatePosition();
	}	

	// special handling for animation property
	if ( symProperty == CStylePropertyAnimationProperties::symbol )
	{
		bool bAffectsLayoutFlags = false;
		if( m_pvecActiveAnimations )
		{
			CUtlVector< CActiveAnimation * > &vec = *m_pvecActiveAnimations;
			FOR_EACH_VEC_BACK( vec, i )
			{
				CActiveAnimation *pAnimation = vec[i];
				vec.Remove( i );

				// don't know if we already raised an AnimationStart event (not cached)... could if necessary
				if( m_pPanel )
					DispatchEventAsync( 0.0f, AnimationEnd(), m_pPanel, pAnimation->GetName() );

				if( pAnimation->BAffectsPanelLayoutFlags( this ) )
					bAffectsLayoutFlags = true;

				delete pAnimation;

				bChanged = true;
			}

			SAFE_DELETE( m_pvecActiveAnimations );
		}

		if ( m_pPanel && bAffectsLayoutFlags )
		{
			m_pPanel->InvalidateSizeAndPosition();
			SetPanelLayoutFlagsForTransitionAnimation();
		}
	}

	return bChanged;
}


//-----------------------------------------------------------------------------
// Purpose: Helper for gradient message building for brushes
//-----------------------------------------------------------------------------
void CPanelStyle::PopulateRadialGradientRenderData( CFillBrush *pBrush, RadialGradient_t &gradient, CRenderCommandList &commandList, float flRenderWidth, float flRenderHeight )
{
	CUILength centerX, centerY, offsetX, offsetY, radiusX, radiusY;
	pBrush->GetRadialGradientValues( centerX, centerY, offsetX, offsetY, radiusX, radiusY );

	gradient.center_position.x = centerX.GetValueAsLength( flRenderWidth );
	gradient.center_position.y = centerY.GetValueAsLength( flRenderHeight );
	gradient.offset_distance.x = offsetX.GetValueAsLength( flRenderWidth );
	gradient.offset_distance.y = offsetY.GetValueAsLength( flRenderHeight );
	gradient.radii.x = radiusX.GetValueAsLength( flRenderWidth );
	gradient.radii.y = radiusY.GetValueAsLength( flRenderHeight );

	CRenderDataListBuilder< ColorStop_t > listBuilder( gradient.color_stop, &commandList );
	for ( const CGradientColorStop &stop : pBrush->AccessStopColors() )
	{
		ColorStop_t *pStop = listBuilder.AddToTail();
		pStop->position = stop.GetPosition();
		pStop->color_rgba = stop.GetColor().GetRawColor();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper for gradient message building for brushes
//-----------------------------------------------------------------------------
void CPanelStyle::PopulateLinearGradientRenderData( CFillBrush *pBrush, LinearGradient_t &gradient, CRenderCommandList &commandList, float flRenderWidth, float flRenderHeight )
{
	CUILength startX, startY, endX, endY;
	pBrush->GetStartAndEndPoints( startX, startY, endX, endY );

	gradient.start_position.x = startX.GetValueAsLength( flRenderWidth );
	gradient.start_position.y = startY.GetValueAsLength( flRenderHeight );
	gradient.end_position.x = endX.GetValueAsLength( flRenderWidth );
	gradient.end_position.y = endY.GetValueAsLength( flRenderHeight );

	CRenderDataListBuilder< ColorStop_t > listBuilder( gradient.color_stop, &commandList );
	for ( const CGradientColorStop &stop : pBrush->AccessStopColors() )
	{
		ColorStop_t *pStop = listBuilder.AddToTail();
		pStop->position = stop.GetPosition();
		pStop->color_rgba = stop.GetColor().GetRawColor();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper for particle system message building for brushes
//-----------------------------------------------------------------------------
void CPanelStyle::PopulateParticleSystemRenderData( CFillBrush *pBrush, ParticleSystem_t &particleSystem, CRenderCommandList &commandList, float flRenderWidth, float flRenderHeight, uint64 ulParentPanelHandle, int iBrushIndex )
{
	CParticleSystem *pParticleSystem = pBrush->AccessParticleSystem();

	VectorToRenderPoint( particleSystem.base_position, pParticleSystem->GetBasePosition() );
	VectorToRenderPoint( particleSystem.base_position_variance, pParticleSystem->GetBasePositionVariance() );

	particleSystem.particle_size = pParticleSystem->GetParticleSize();
	particleSystem.particle_size_variance = pParticleSystem->GetParticleSizeVariance();

	particleSystem.particles_per_second = pParticleSystem->GetParticlesPerSecond();
	particleSystem.particles_per_second_variance = pParticleSystem->GetParticlesPerSecondVariance();

	particleSystem.particle_lifespan_seconds = pParticleSystem->GetParticleLifeSpanSeconds();
	particleSystem.particle_lifespan_seconds_variance = pParticleSystem->GetParticleLifeSpanSecondsVariance();

	VectorToRenderPoint( particleSystem.gravity_acceleration, pParticleSystem->GetGravityAcceleration() );
	VectorToRenderPoint( particleSystem.gravity_acceleration_particle_variance, pParticleSystem->GetGravityAccelerationParticleVariance() );

	VectorToRenderPoint( particleSystem.particle_velocity_min, pParticleSystem->GetParticleVelocityMin() );
	VectorToRenderPoint( particleSystem.particle_velocity_max, pParticleSystem->GetParticleVelocityMax() );

	particleSystem.color_start_rgba = pParticleSystem->GetStartColor().GetRawColor();
	particleSystem.color_start_rgba_variance = pParticleSystem->GetStartColorVariance().GetRawColor();

	particleSystem.color_end_rgba = pParticleSystem->GetEndColor().GetRawColor();
	particleSystem.color_end_rgba_variance = pParticleSystem->GetEndColorVariance().GetRawColor();

	particleSystem.parent_panel_handle = ulParentPanelHandle;
	particleSystem.parent_brush_index = iBrushIndex;

	particleSystem.particle_sharpness = pParticleSystem->GetSharpness();
	particleSystem.particle_sharpness_variance = pParticleSystem->GetSharpnessVariance();

	particleSystem.particle_flicker = pParticleSystem->GetFlicker();
	particleSystem.particle_flicker_variance = pParticleSystem->GetFlickerVariance();
}


//-----------------------------------------------------------------------------
// Purpose: Data needed when filling in a position message
//-----------------------------------------------------------------------------
struct GetFillBrushCollectionContext_t
{
	float m_flRenderWidth;
	float m_flRenderHeight;
	CRenderCommandList *pCommandList;
};


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( FillBrushCollection_t &data, CStylePropertyBackgroundColor *pProperty, void *pContext )
{
	GetFillBrushCollectionContext_t *pBrushContext = ( GetFillBrushCollectionContext_t* )pContext;
	if ( !pProperty )
	{
		CFillBrushCollection c;
		AddBrushesToRenderData( c.AccessBrushes(), data, *pBrushContext->pCommandList, pBrushContext->m_flRenderWidth, pBrushContext->m_flRenderHeight );
		return;
	}

	AddBrushesToRenderData( pProperty->m_FillBrushCollection.AccessBrushes(), data, *pBrushContext->pCommandList, pBrushContext->m_flRenderWidth, pBrushContext->m_flRenderHeight );
}


//-----------------------------------------------------------------------------
// Purpose: Get color command for background color
//-----------------------------------------------------------------------------
void CPanelStyle::GetBackgroundFillBrushCollectionData( FillBrushCollectionWithTransition_t &data, CRenderCommandList &commandList )
{
	GetFillBrushCollectionContext_t context;
	context.m_flRenderWidth = m_pPanel->GetActualRenderWidth();
	context.m_flRenderHeight = m_pPanel->GetActualRenderHeight();
	context.pCommandList = &commandList;

	FillRenderData< CStylePropertyBackgroundColor >( data, commandList, &context );
}


//-----------------------------------------------------------------------------
// Purpose: Get blend mode
//-----------------------------------------------------------------------------
EMixBlendMode CPanelStyle::GetMixBlendMode()
{
	CStylePropertyMixBlendMode *pValue = (CStylePropertyMixBlendMode*)FindProperty( CStylePropertyMixBlendMode::symbol );
	if( pValue )
	{
		return pValue->m_eMixBlendMode;
	}
	else
	{
		return k_EMixBlendModeNormal;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get texture sampling mode
//-----------------------------------------------------------------------------
ETextureSampleMode CPanelStyle::GetTexturesSampleMode()
{
	CStylePropertyTextureSampleMode *pValue = (CStylePropertyTextureSampleMode*)FindProperty( CStylePropertyTextureSampleMode::symbol );
	if ( pValue )
	{
		return pValue->m_eTextureSampleMode;
	}
	else
	{
		return k_ETextureSampleModeNormal;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get composition color
//-----------------------------------------------------------------------------
void CPanelStyle::GetWashColor( Color &c )
{
	CStylePropertyWashColor *pValue = (CStylePropertyWashColor*)FindProperty( CStylePropertyWashColor::symbol );
	if( pValue )
	{
		c = pValue->GetColor();	
	}
	else
	{
		// White default, so rendering is unaffected
		c = CStylePropertyWashColor::GetDefault();			
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set composition color
//-----------------------------------------------------------------------------
void CPanelStyle::SetWashColor( const char *pchColor )
{
	CStylePropertyWashColor *pValue = CStylePropertyFactory::Create< CStylePropertyWashColor >();
	if ( !pValue->BSetFromString( "wash-color", pchColor ) )
	{
		CStylePropertyFactory::FreeStyleProperty( pValue );
		return;
	}
	SetProperty( pValue );
}

//-----------------------------------------------------------------------------
// Purpose: Set wash color to a simple RGBA
//-----------------------------------------------------------------------------
void CPanelStyle::SetSimpleWashColor( const Color &c, bool bFast )
{
	CStylePropertyWashColor *pValue = CStylePropertyFactory::Create< CStylePropertyWashColor >();
	pValue->SetColor( c );
	pValue->SetFast( bFast );
	SetProperty( pValue );
}

//-----------------------------------------------------------------------------
// Purpose: Set background color
//-----------------------------------------------------------------------------
void CPanelStyle::SetBackgroundColor( const char *pchColor )
{
	CStylePropertyBackgroundColor *pValue = CStylePropertyFactory::Create< CStylePropertyBackgroundColor >();
	if ( !pValue->BSetFromString( "background-color", pchColor ) )
	{
		CStylePropertyFactory::FreeStyleProperty( pValue );
		return;
	}
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Set background color to a simple RGBA
//-----------------------------------------------------------------------------
void CPanelStyle::SetSimpleBackgroundColor( const Color &c )
{
	CStylePropertyBackgroundColor *pValue = CStylePropertyFactory::Create< CStylePropertyBackgroundColor >();
	CFillBrushCollection::FillBrush_t brush = { CFillBrush( c.r(), c.g(), c.b(), c.a() ), 1.0f };
	pValue->m_FillBrushCollection.AccessBrushes().AddToTail( brush );
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Get background color if it is a simple RGBA
//-----------------------------------------------------------------------------
bool CPanelStyle::GetSimpleBackgroundColor( Color &c )
{
	CStylePropertyBackgroundColor *pValue = ( CStylePropertyBackgroundColor* )FindProperty( CStylePropertyBackgroundColor::symbol );
	if ( !pValue )
		return false;

	if ( pValue->m_FillBrushCollection.GetBrushCount() != 1 )
		return false;

	const CFillBrush &fillBrush = pValue->m_FillBrushCollection.AccessBrushes()[ 0 ].m_Brush;
	if ( fillBrush.GetType() != CFillBrush::k_EStrokeTypeFillColor )
		return false;

	c = fillBrush.GetFillColor();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Set foreground color
//-----------------------------------------------------------------------------
void CPanelStyle::SetForegroundColor( const char *pchColor )
{
	CStylePropertyForegroundColor *pValue = CStylePropertyFactory::Create< CStylePropertyForegroundColor >();
	if ( !pValue->BSetFromString( "color", pchColor ) )
	{
		CStylePropertyFactory::FreeStyleProperty( pValue );
		return;
	}
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Set foreground color to a simple RGBA
//-----------------------------------------------------------------------------
void CPanelStyle::SetSimpleForegroundColor( const Color &c )
{
	CStylePropertyForegroundColor *pValue = CStylePropertyFactory::Create< CStylePropertyForegroundColor >();
	CFillBrushCollection::FillBrush_t brush = { CFillBrush( c.r(), c.g(), c.b(), c.a() ), 1.0f };
	pValue->m_FillBrushCollection.AccessBrushes().AddToTail( brush );
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Get background color if it is a simple RGBA
//-----------------------------------------------------------------------------
bool CPanelStyle::GetSimpleForegroundColor( Color &c )
{
	CStylePropertyForegroundColor *pValue = ( CStylePropertyForegroundColor* )FindProperty( CStylePropertyForegroundColor::symbol );
	if ( !pValue )
		return false;

	if ( pValue->m_FillBrushCollection.GetBrushCount() != 1 )
		return false;

	const CFillBrush &fillBrush = pValue->m_FillBrushCollection.AccessBrushes()[ 0 ].m_Brush;
	if ( fillBrush.GetType() != CFillBrush::k_EStrokeTypeFillColor )
		return false;

	c = fillBrush.GetFillColor();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: set border color
//-----------------------------------------------------------------------------
void CPanelStyle::SetBorderColor( const Color &c )
{
	CStylePropertyBorder *pValue = CStylePropertyFactory::Create< CStylePropertyBorder >();

	if ( !pValue->BSetBorderColor( c ) )
	{
		CStylePropertyFactory::FreeStyleProperty( pValue );
		return;
	}
	SetProperty( pValue );
}

//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( WashColor_t &data, CStylePropertyWashColor *pProperty, void *pContext )
{
	if ( pProperty )
	{
		data.m_color = pProperty->GetColor().GetRawColor();
		data.m_bFast = pProperty->BIsFast();
	}
	else
	{
		data.m_color = CStylePropertyWashColor::GetDefault().GetRawColor();
		data.m_bFast = false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get color command for composition
//-----------------------------------------------------------------------------
void CPanelStyle::GetWashColorData( WashColorWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyWashColor >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPanelStyle::AddBrushesToRenderData( CFillBrushCollection::BrushVec_t &vecBrushes, FillBrushCollection_t &collection, CRenderCommandList &commandList, float flWidth, float flHeight )
{
	int nParticleSystems = 0;
	CRenderDataListBuilder< FillBrush_t > listBuilder( collection.fill_brush, &commandList );
	for ( CFillBrushCollection::FillBrush_t &fillBrush : vecBrushes )
	{
		CFillBrush &brush = fillBrush.m_Brush;

		FillBrush_t *pFillBrushData = listBuilder.AddToTail();
		pFillBrushData->opacity = fillBrush.m_Opacity;

		if ( brush.GetType() == CFillBrush::k_EStrokeTypeFillColor )
		{
			pFillBrushData->eFillBrushType = k_EFillBrushType_Color;
			pFillBrushData->color_rgba = brush.GetFillColor().GetRawColor();
		}
		else if ( brush.GetType() == CFillBrush::k_EStrokeTypeLinearGradient )
		{
			pFillBrushData->eFillBrushType = k_EFillBrushType_LinearGradient;
			pFillBrushData->linear_gradient = commandList.AllocType< LinearGradient_t >();
			PopulateLinearGradientRenderData( &brush, *pFillBrushData->linear_gradient, commandList, flWidth, flHeight );
		}
		else if ( brush.GetType() == CFillBrush::k_EStrokeTypeRadialGradient )
		{
			pFillBrushData->eFillBrushType = k_EFillBrushType_RadialGradient;
			pFillBrushData->radial_gradient = commandList.AllocType< RadialGradient_t >();
			PopulateRadialGradientRenderData( &brush, *pFillBrushData->radial_gradient, commandList, flWidth, flHeight );
		}
		else if ( brush.GetType() == CFillBrush::k_EStrokeTypeParticleSystem )
		{
			CPanelPtr< CPanel2D > panel = m_pPanel;
			pFillBrushData->eFillBrushType = k_EFillBrushType_ParticleSystem;
			pFillBrushData->particle_system = commandList.AllocType< ParticleSystem_t >();
			PopulateParticleSystemRenderData( &brush, *pFillBrushData->particle_system, commandList, flWidth, flHeight, panel.GetHandleAsUInt64(), nParticleSystems );
			++nParticleSystems;
		}
		else
		{
			AssertMsg( false, "Unsupported stroke type" );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( FillBrushCollection_t &data, CStylePropertyForegroundColor *pProperty, void *pContext )
{
	GetFillBrushCollectionContext_t *pBrushContext = ( GetFillBrushCollectionContext_t* )pContext;
	if ( !pProperty )
	{
		// default foreground color to red so we can find and fix missing styles
		CFillBrushCollection c;
		c.AddFillBrush( CFillBrush( 0xff, 0, 0, 0xff ) );
		AddBrushesToRenderData( c.AccessBrushes(), data, *pBrushContext->pCommandList, pBrushContext->m_flRenderWidth, pBrushContext->m_flRenderHeight );
		return;
	}

	AddBrushesToRenderData( pProperty->m_FillBrushCollection.AccessBrushes(), data, *pBrushContext->pCommandList, pBrushContext->m_flRenderWidth, pBrushContext->m_flRenderHeight );
}


//-----------------------------------------------------------------------------
// Purpose: Get color command for text color
//-----------------------------------------------------------------------------
void CPanelStyle::GetForegroundFillBrushCollectionData( FillBrushCollectionWithTransition_t &data, CRenderCommandList &commandList, float flRenderWidth, float flRenderHeight )
{
	GetFillBrushCollectionContext_t context;
	context.m_flRenderWidth = flRenderWidth;
	context.m_flRenderHeight = flRenderHeight;
	context.pCommandList = &commandList;

	FillRenderData< CStylePropertyForegroundColor >( data, commandList, &context );
}


//-----------------------------------------------------------------------------
// Purpose: Data needed when filling in a position message
//-----------------------------------------------------------------------------
struct GetPositionMsgContext_t
{
	float m_flParentWidth;
	float m_flParentHeight;
	float m_flParentPerspective;
	float m_flXOffset;
	float m_flYOffset;
};


//-----------------------------------------------------------------------------
// Purpose: Get render command data for position
//-----------------------------------------------------------------------------
void CPanelStyle::GetPositionRenderData( PanelPositionWithTransition_t &positionData, CRenderCommandList &commandList )
{
	EOverflowValue eHorizontal, eVertical;
	GetOverflow( eHorizontal, eVertical );

	if ( eHorizontal == k_EOverflowScroll || eVertical == k_EOverflowScroll )
	{
		positionData.scroll_offset = commandList.AllocType< RenderPoint2D_t >();
		positionData.scroll_offset->x = m_pPanel->GetContentsXScrollOffset();
		positionData.scroll_offset->y = m_pPanel->GetContentsYScrollOffset();

		positionData.scroll_offset_target = commandList.AllocType< RenderPoint2D_t >();
		positionData.scroll_offset_target->x = m_pPanel->GetContentsXScrollOffsetTarget();
		positionData.scroll_offset_target->y = m_pPanel->GetContentsYScrollOffsetTarget();

		if ( m_pPanel->GetContentsXScrollOffsetTarget() != FLT_MAX )
		{
			positionData.scroll_transition_x = commandList.AllocType< TransitionData_t >();
			positionData.scroll_transition_x->start_time = m_pPanel->GetContentsXScrollTransitionStart();
			positionData.scroll_transition_x->delay_seconds = 0.0;
			positionData.scroll_transition_x->duration_seconds = m_pPanel->GetContentsXScrollTransitionTime();
			positionData.scroll_transition_x->timing_func = m_pPanel->GetContentsXScrollTransitionTimingFunction();
			if ( m_pPanel->GetContentsXScrollTransitionTimingFunction() == k_EAnimationCustomBezier )
			{
				Vector2D vecPoints[ 4 ];
				m_pPanel->GetContextXScrollTransitionControlPoints( vecPoints );
				positionData.scroll_transition_x->cubic_bezier_0 = vecPoints[ 1 ].x;
				positionData.scroll_transition_x->cubic_bezier_1 = vecPoints[ 1 ].y;
				positionData.scroll_transition_x->cubic_bezier_2 = vecPoints[ 2 ].x;
				positionData.scroll_transition_x->cubic_bezier_3 = vecPoints[ 2 ].y;
			}
		}

		if ( m_pPanel->GetContentsYScrollOffsetTarget() != FLT_MAX )
		{
			positionData.scroll_transition_y = commandList.AllocType< TransitionData_t >();
			positionData.scroll_transition_y->start_time = m_pPanel->GetContentsYScrollTransitionStart();
			positionData.scroll_transition_y->delay_seconds = 0.0;
			positionData.scroll_transition_y->duration_seconds = m_pPanel->GetContentsYScrollTransitionTime();
			positionData.scroll_transition_y->timing_func = m_pPanel->GetContentsYScrollTransitionTimingFunction();
			if ( m_pPanel->GetContentsYScrollTransitionTimingFunction() == k_EAnimationCustomBezier )
			{
				Vector2D vecPoints[ 4 ];
				m_pPanel->GetContextYScrollTransitionControlPoints( vecPoints );
				positionData.scroll_transition_y->cubic_bezier_0 = vecPoints[ 1 ].x;
				positionData.scroll_transition_y->cubic_bezier_1 = vecPoints[ 1 ].y;
				positionData.scroll_transition_y->cubic_bezier_2 = vecPoints[ 2 ].x;
				positionData.scroll_transition_y->cubic_bezier_3 = vecPoints[ 2 ].y;
			}
		}
	}

	// pass layout thread offset here (parent border/padding, child margin & alignment). Flowing offsets are passed through position later so they can transition
	GetPositionMsgContext_t context;
	context.m_flXOffset = m_pPanel->GetRawActualXOffset();
	context.m_flYOffset = m_pPanel->GetRawActualYOffset();

	CTopLevelWindow *pWindow = ( CTopLevelWindow* )m_pPanel->GetParentWindow();
	context.m_flParentWidth = pWindow->GetSurfaceWidth();
	context.m_flParentHeight = pWindow->GetSurfaceHeight();
	context.m_flParentPerspective = 1000.0 * GetActualUIScaleZ();

	// Parent padding values
	IUIPanel *pParent = m_pPanel->GetParent();
	if ( pParent )
	{
		IUIPanelStyle *pParentStyle = pParent->AccessIUIStyle();

		context.m_flParentWidth = pParent->GetActualRenderWidth();
		context.m_flParentHeight = pParent->GetActualRenderHeight();

		pParentStyle->GetPerspective( context.m_flParentPerspective );

		float flLeft, flTop, flRight, flBottom;
		pParentStyle->GetContentInset( context.m_flParentWidth, context.m_flParentHeight, false, flLeft, flTop, flRight, flBottom );
		context.m_flParentWidth -= flLeft - flRight;
		context.m_flParentHeight -= flTop - flBottom;
	}

	FillRenderData< CStylePropertyPosition >( positionData, commandList, &context );
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( RenderPoint_t &data, CStylePropertyPosition *pProperty, void *pContext )
{
	GetPositionMsgContext_t	*pMsgContext = ( GetPositionMsgContext_t * )pContext;

	float x, y, z;
	if ( !pProperty )
	{
		x = 0.0f + pMsgContext->m_flXOffset;
		y = 0.0f + pMsgContext->m_flYOffset;
		z = 0.0f;
	}
	else
	{
		x = pProperty->x.GetValueAsLength( pMsgContext->m_flParentWidth ) + pMsgContext->m_flXOffset;
		y = pProperty->y.GetValueAsLength( pMsgContext->m_flParentHeight ) + pMsgContext->m_flYOffset;
		z = pProperty->z.GetValueAsLength( pMsgContext->m_flParentPerspective );
	}

	// Round to 3 decimal points. This fixes cases where position values were like 99.9999992f
	// and being drawn incorrectly rather than just 100.0f.
	x = RoundFloatToInt( x * 1000.0f ) / 1000.0f;
	y = RoundFloatToInt( y * 1000.0f ) / 1000.0f;
	z = RoundFloatToInt( z * 1000.0f ) / 1000.0f;

	data.x = x;
	data.y = y;
	data.z = z;

	Assert( IsFinite( data.x ) );
	Assert( IsFinite( data.y ) );
	Assert( IsFinite( data.z ) );
}


//-----------------------------------------------------------------------------
// Purpose: Get position
//-----------------------------------------------------------------------------
void CPanelStyle::GetPosition( CUILength &x, CUILength &y, CUILength &z, bool bIncludeUIScaleFactor /* = true  */ )
{
	CStylePropertyPosition *pos = (CStylePropertyPosition*)FindProperty( CStylePropertyPosition::symbol );
	if( pos )
	{
		x = pos->x;
		y = pos->y;
		z = pos->z;

		// Reverse scaling if requested to return raw style value not scaled value
		if( !bIncludeUIScaleFactor )
		{
			x.ScaleLengthValue( 1.0 / GetActualUIScaleX() );
			y.ScaleLengthValue( 1.0 / GetActualUIScaleY() );
			z.ScaleLengthValue( 1.0 / GetActualUIScaleZ() );
		}
	}
	else
	{
		x.SetLength( 0.0f );
		y.SetLength( 0.0f );
		z.SetLength( 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get interpolated scale2d property values
//-----------------------------------------------------------------------------
void CPanelStyle::GetInterpolatedScale2DCentered( float &flX, float &flY ) 
{
	VPROF_BUDGET( "CPanelStyle::GetInterpolatedPosition", VPROF_BUDGETGROUP_TENFOOT );

	if ( BHasTransition( CStylePropertyScale2DCentered::symbol ) )
	{
		CStylePropertyScale2DCentered *pStyle = (CStylePropertyScale2DCentered*)GetInterpolatedProperty( CStylePropertyScale2DCentered::symbol, false );
		if ( pStyle )
		{
			flX = pStyle->m_flX;;
			flY = pStyle->m_flY;
			CStylePropertyFactory::FreeStyleProperty( pStyle );
		}
		else
		{
			// Set to default
			flX = 1.0f;
			flY = 1.0f;
		}
	}
	else
	{
		CStylePropertyScale2DCentered *pStyle = (CStylePropertyScale2DCentered*)FindProperty( CStylePropertyScale2DCentered::symbol );
		if ( pStyle )
		{
			flX = pStyle->m_flX;;
			flY = pStyle->m_flY;
		}
		else
		{
			// Set to default
			flX = 1.0f;
			flY = 1.0f;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get interpolated position property
//-----------------------------------------------------------------------------
void CPanelStyle::GetInterpolatedPosition( CUILength &x, CUILength &y, CUILength &z, bool bFinal, bool bIncludeUIScaleFactor /*= true */ )
{
	if ( BHasTransition( CStylePropertyPosition::symbol ) )
	{
		CStylePropertyPosition *pStyle = (CStylePropertyPosition*)GetInterpolatedProperty( CStylePropertyPosition::symbol, bFinal );
		if ( pStyle )
		{
			x = pStyle->x;
			y = pStyle->y;
			z = pStyle->z;
			CStylePropertyFactory::FreeStyleProperty( pStyle );

			// Reverse scaling if requested to return raw style value not scaled value
			if( !bIncludeUIScaleFactor )
			{
				x.ScaleLengthValue( 1.0 / GetActualUIScaleX() );
				y.ScaleLengthValue( 1.0 / GetActualUIScaleY() );
				z.ScaleLengthValue( 1.0 / GetActualUIScaleZ() );
			}
		}
		else
		{
			// Set to default
			x.SetLength( 0.0f );
			y.SetLength( 0.0f );
			z.SetLength( 0.0f );
		}
	}
	else
	{
		GetPosition( x, y, z );

		// Reverse scaling if requested to return raw style value not scaled value
		if( !bIncludeUIScaleFactor )
		{
			x.ScaleLengthValue( 1.0 / GetActualUIScaleX() );
			y.ScaleLengthValue( 1.0 / GetActualUIScaleY() );
			z.ScaleLengthValue( 1.0 / GetActualUIScaleZ() );
		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: Set position
//-----------------------------------------------------------------------------
void CPanelStyle::SetPosition( CUILength x, CUILength y, CUILength z, bool bPreScaledByUIScaleFactor /*= false */ )
{
	CStylePropertyPosition *pPosition = CStylePropertyFactory::Create< CStylePropertyPosition >();
	pPosition->x = x;
	pPosition->y = y;
	pPosition->z = z;

	// Reverse scaling if needed as it will be re-applied when we merge styles and apply to the result
	if ( bPreScaledByUIScaleFactor && m_pPanel )
	{
		pPosition->x.ScaleLengthValue( 1.0 / GetActualUIScaleX() );
		pPosition->y.ScaleLengthValue( 1.0 / GetActualUIScaleY() );
		pPosition->z.ScaleLengthValue( 1.0 / GetActualUIScaleZ() );
	}

	SetProperty( pPosition, true );
}


//-----------------------------------------------------------------------------
// Purpose: Sets panel position but does not allow for panel transition
//-----------------------------------------------------------------------------
void CPanelStyle::SetPositionWithoutTransition( CUILength x, CUILength y, CUILength z, bool bPreScaledByUIScaleFactor /*= false */ )
{
	CStylePropertyPosition *pPosition = CStylePropertyFactory::Create< CStylePropertyPosition >();
	pPosition->x = x;
	pPosition->y = y;
	pPosition->z = z;

	// Reverse scaling if needed as it will be re-applied when we merge styles and apply to the result
	if( bPreScaledByUIScaleFactor && m_pPanel )
	{
		pPosition->x.ScaleLengthValue( 1.0 / GetActualUIScaleX() );
		pPosition->y.ScaleLengthValue( 1.0 / GetActualUIScaleY() );
		pPosition->z.ScaleLengthValue( 1.0 / GetActualUIScaleZ() );
	}

	SetProperty( pPosition, false );
}


//-----------------------------------------------------------------------------
// Purpose: Set perspective origin for 3d transforms
//-----------------------------------------------------------------------------
void CPanelStyle::SetPerspectiveOrigin( CUILength &x, CUILength &y, bool &bInvert )
{
	CStylePropertyPerspectiveOrigin *pValue = CStylePropertyFactory::Create< CStylePropertyPerspectiveOrigin >();
	pValue->x = x;
	pValue->y = y;
	pValue->m_bInvert = bInvert;

	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Get perspective origin for 3d transforms
//-----------------------------------------------------------------------------
void CPanelStyle::GetPerspectiveOrigin( CUILength &x, CUILength &y, bool &bInvert )
{
	CStylePropertyPerspectiveOrigin *pValue = (CStylePropertyPerspectiveOrigin*)FindProperty( CStylePropertyPerspectiveOrigin::symbol );
	if( pValue )
	{
		x = pValue->x;
		y = pValue->y;
	}
	else
	{
		CStylePropertyPerspectiveOrigin::GetDefault( &x, &y, &bInvert );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set transform origin for 3d transforms
//-----------------------------------------------------------------------------
void CPanelStyle::SetTransformOrigin( CUILength &x, CUILength &y, bool bParentLayerRelative )
{
	CStylePropertyTransformOrigin *pValue = CStylePropertyFactory::Create< CStylePropertyTransformOrigin >();
	pValue->x = x;
	pValue->y = y;
	pValue->m_bParentRelative = bParentLayerRelative;

	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Get transform origin for 3d transforms
//-----------------------------------------------------------------------------
void CPanelStyle::GetTransformOrigin( CUILength &x, CUILength &y, bool &bParentLayerRelative )
{
	CStylePropertyTransformOrigin *pValue = (CStylePropertyTransformOrigin*)FindProperty( CStylePropertyTransformOrigin::symbol );
	if( pValue )
	{
		x = pValue->x;
		y = pValue->y;
		bParentLayerRelative = pValue->m_bParentRelative;
	}
	else
	{
		CStylePropertyTransformOrigin::GetDefault( &x, &y, &bParentLayerRelative );
	}
}



//-----------------------------------------------------------------------------
// Purpose: Get z-index for panel
//-----------------------------------------------------------------------------
void CPanelStyle::GetZIndex( float &zindex )
{
	CStylePropertyZIndex *pValue = (CStylePropertyZIndex*)FindProperty( CStylePropertyZIndex::symbol );
	if( pValue )
	{
		zindex = pValue->zindex;
	}
	else
	{
		zindex = CStylePropertyZIndex::GetDefault();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set z-index for panel
//-----------------------------------------------------------------------------
void CPanelStyle::SetZIndex( float zIndex )
{
	CStylePropertyZIndex *pValue = CStylePropertyFactory::Create< CStylePropertyZIndex >();
	pValue->zindex = zIndex;

	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Set perspective for 3d transforms
//-----------------------------------------------------------------------------
void CPanelStyle::SetPerspective( float perspective )
{
	CStylePropertyPerspective*pValue = CStylePropertyFactory::Create< CStylePropertyPerspective >();
	pValue->perspective = perspective;
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Get perspective for 3d transforms
//-----------------------------------------------------------------------------
void CPanelStyle::GetPerspective( float &perspective )
{
	CStylePropertyPerspective *pValue = (CStylePropertyPerspective*)FindProperty( CStylePropertyPerspective::symbol );
	if( pValue )
	{
		perspective = pValue->perspective;
	}
	else
	{
		perspective = CStylePropertyPerspective::GetDefault() * m_pPanel->GetActualUIScaleZ();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get overflow behavior
//-----------------------------------------------------------------------------
void CPanelStyle::GetOverflow( EOverflowValue &eHorizontal, EOverflowValue &eVertical )
{
	CStylePropertyOverflow *pValue = (CStylePropertyOverflow*)FindProperty( CStylePropertyOverflow::symbol );
	if( pValue )
	{
		eHorizontal = pValue->m_eHorizontal;
		eVertical = pValue->m_eVertical;
	}
	else
	{
		eHorizontal = k_EOverflowSquish;
		eVertical = k_EOverflowSquish;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get overflow behavior
//-----------------------------------------------------------------------------
void CPanelStyle::SetOverflow( const EOverflowValue eHorizontal, const EOverflowValue eVertical )
{
	CStylePropertyOverflow *pValue = CStylePropertyFactory::Create< CStylePropertyOverflow >();
	pValue->m_eHorizontal = eHorizontal;
	pValue->m_eVertical = eVertical;
	pValue->m_bSet = true;
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Get hue shift
//-----------------------------------------------------------------------------
void CPanelStyle::GetHueShift( float &flHueShift )
{
	CStylePropertyHueShift *pValue = (CStylePropertyHueShift *)FindProperty( CStylePropertyHueShift::symbol );
	if ( pValue )
	{
		flHueShift = pValue->hueShift;
	}
	else
	{
		flHueShift = CStylePropertyHueShift::GetDefault();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set hue shift
//-----------------------------------------------------------------------------
void CPanelStyle::SetHueShift( float flHueShift )
{
	CStylePropertyHueShift *pValue = CStylePropertyFactory::Create< CStylePropertyHueShift >();
	pValue->hueShift = flHueShift;
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( float &data, CStylePropertyHueShift *pProperty, void *pContext )
{
	data = pProperty ? pProperty->hueShift : CStylePropertyHueShift::GetDefault();
}


//-----------------------------------------------------------------------------
// Purpose: Get saturation
//-----------------------------------------------------------------------------
void CPanelStyle::GetSaturation( float &saturation )
{
	CStylePropertySaturation *pValue = (CStylePropertySaturation *)FindProperty( CStylePropertySaturation::symbol );
	if ( pValue )
	{
		saturation = pValue->saturation;
	}
	else
	{
		saturation = CStylePropertySaturation::GetDefault();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set saturation
//-----------------------------------------------------------------------------
void CPanelStyle::SetSaturation( float saturation )
{
	CStylePropertySaturation *pValue = CStylePropertyFactory::Create< CStylePropertySaturation >();
	pValue->saturation = saturation;
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( float &data, CStylePropertySaturation *pProperty, void *pContext )
{
	data = pProperty ? pProperty->saturation : CStylePropertySaturation::GetDefault();
}


//-----------------------------------------------------------------------------
// Purpose: Get brightness
//-----------------------------------------------------------------------------
void CPanelStyle::GetBrightness( float &flBrightness )
{
	CStylePropertyBrightness *pValue = (CStylePropertyBrightness *)FindProperty( CStylePropertyBrightness::symbol );
	if ( pValue )
	{
		flBrightness = pValue->brightness;
	}
	else
	{
		flBrightness = CStylePropertyBrightness::GetDefault();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set brightness
//-----------------------------------------------------------------------------
void CPanelStyle::SetBrightness( float flBrightness )
{
	CStylePropertyBrightness *pValue = CStylePropertyFactory::Create< CStylePropertyBrightness >();
	pValue->brightness = flBrightness;
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( float &data, CStylePropertyBrightness *pProperty, void *pContext )
{
	data = pProperty ? pProperty->brightness : CStylePropertyBrightness::GetDefault();
}


//-----------------------------------------------------------------------------
// Purpose: Get contrast
//-----------------------------------------------------------------------------
void CPanelStyle::GetContrast( float &flContrast )
{
	CStylePropertyContrast *pValue = (CStylePropertyContrast *)FindProperty( CStylePropertyContrast::symbol );
	if ( pValue )
	{
		flContrast = pValue->contrast;
	}
	else
	{
		flContrast = CStylePropertyContrast::GetDefault();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set contrast
//-----------------------------------------------------------------------------
void CPanelStyle::SetContrast( float flContrast )
{
	CStylePropertyContrast *pValue = CStylePropertyFactory::Create< CStylePropertyContrast >();
	pValue->contrast = flContrast;
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( float &data, CStylePropertyContrast *pProperty, void *pContext )
{
	data = pProperty ? pProperty->contrast : CStylePropertyContrast::GetDefault();
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( GaussianValues_t &data, CStylePropertyBlur *pProperty, void *pContext )
{
	if ( !pProperty )
	{
		data.passes = CStylePropertyBlur::GetDefaultPasses();
		data.stddev_hor = CStylePropertyBlur::GetDefaultStdDev();
		data.stddev_ver = CStylePropertyBlur::GetDefaultStdDev();
		data.blurType = CStylePropertyBlur::GetDefaultBlurType();;
	}
	else
	{
		data.passes = pProperty->passes;
		data.stddev_hor = pProperty->stddevhor;
		data.stddev_ver = pProperty->stddevver;
		data.blurType = pProperty->blurType;
	}

}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( OpacityMask_t &data, CStylePropertyOpacityMask *pProperty, void *pContext )
{
	if ( pProperty )
	{
		CRenderCommandList *pCommandList = reinterpret_cast< CRenderCommandList * >( pContext );

		if ( pProperty->m_pImageUpDown && m_pPanel->BCanScrollUp() && m_pPanel->BCanScrollDown() )
		{
			data.opacity_mask_texture.SetTexture( pProperty->m_pImageUpDown->GetTexture(), *pCommandList );
			data.opacity_mask_opacity = pProperty->m_flOpacityMaskOpacityUpDown;
		}
		else if ( pProperty->m_pImageUp && m_pPanel->BCanScrollUp() )
		{
			data.opacity_mask_texture.SetTexture( pProperty->m_pImageUp->GetTexture(), *pCommandList );
			data.opacity_mask_opacity = pProperty->m_flOpacityMaskOpacityUp;
		}
		else if ( pProperty->m_pImageDown && ( m_pPanel->BCanScrollDown() || ( !m_pPanel->BCanScrollDown() && m_pPanel->GetContentHeight() > m_pPanel->GetActualLayoutHeight() ) ) )
		{
			data.opacity_mask_texture.SetTexture( pProperty->m_pImageDown->GetTexture(), *pCommandList );
			data.opacity_mask_opacity = pProperty->m_flOpacityMaskOpacityDown;
		}
		else if ( pProperty->m_pImage )
		{
			data.opacity_mask_texture.SetTexture( pProperty->m_pImage->GetTexture(), *pCommandList );
			data.opacity_mask_opacity = pProperty->m_flOpacityMaskOpacity;
		}
		else
		{
			data.opacity_mask_texture.SetTexture( nullptr, *pCommandList );
			data.opacity_mask_opacity = ( 1.0f );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get hue shift in msg form
//-----------------------------------------------------------------------------
void CPanelStyle::GetHueShiftData( HueShiftWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyHueShift >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Get saturation in msg form
//-----------------------------------------------------------------------------
void CPanelStyle::GetSaturationData( SaturationWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertySaturation >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Get brightness in msg form
//-----------------------------------------------------------------------------
void CPanelStyle::GetBrightnessData( BrightnessWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyBrightness >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Get contrast in msg form
//-----------------------------------------------------------------------------
void CPanelStyle::GetContrastData( ContrastWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyContrast >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Get gaussian blur
//-----------------------------------------------------------------------------
void CPanelStyle::GetGaussianBlur( BlurType_t &blurType, float &passes, float &stddevhor, float &stddevver )
{
	CStylePropertyBlur *pValue = (CStylePropertyBlur*)FindProperty( CStylePropertyBlur::symbol );
	if( pValue )
	{
		passes = pValue->passes;
		stddevhor = pValue->stddevhor;
		stddevver = pValue->stddevver;
		blurType = pValue->blurType;
	}
	else
	{
		stddevhor = CStylePropertyBlur::GetDefaultStdDev();
		stddevver = CStylePropertyBlur::GetDefaultStdDev();
		passes = CStylePropertyBlur::GetDefaultPasses();
		blurType = CStylePropertyBlur::GetDefaultBlurType();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set gaussian blur
//-----------------------------------------------------------------------------
void CPanelStyle::SetGaussianBlur( BlurType_t blurType, float passes, float stddevhor, float stddevver )
{
	CStylePropertyBlur *pValue = CStylePropertyFactory::Create< CStylePropertyBlur >();
	pValue->passes = passes;
	pValue->stddevhor = stddevhor;
	pValue->stddevver = stddevver;
	pValue->blurType = blurType;
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Get blur in message form
//-----------------------------------------------------------------------------
void CPanelStyle::GetGaussianBlurData( GaussianBlurWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyBlur >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Get opacity 
//-----------------------------------------------------------------------------
void CPanelStyle::GetOpacity( float &opacity )
{
	CStylePropertyOpacity *pValue = (CStylePropertyOpacity*)FindProperty( CStylePropertyOpacity::symbol );
	if( pValue )
	{
		opacity = pValue->opacity;
	}
	else
	{
		opacity = CStylePropertyOpacity::GetDefault();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set opacity 
//-----------------------------------------------------------------------------
void CPanelStyle::SetOpacity( float opacity )
{
	CStylePropertyOpacity *pValue = CStylePropertyFactory::Create< CStylePropertyOpacity >();
	pValue->opacity = opacity;
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Simplified version of SetOpacity that will call a simplified version of SetProperty
//			ie bypassing updating the corresponding panel - it is the responsibility of the calling
//			code to update the corresponding UIPanel (such as calling InvalidateSizeAndPosition, AfterStyleApplied, ...).
//			Also assuming no animation / transition set for transform (via CSS or code)
//			Returns true if style has been updated with the new value, false otherwise.
//-----------------------------------------------------------------------------
bool CPanelStyle::SetOpacitySimple( float opacity )
{
	//
	// Apply transform property to style
	//

	CStylePropertyOpacity *pToApply = CStylePropertyFactory::Create< CStylePropertyOpacity >();
	pToApply->opacity = opacity;

	if ( SetPropertyFromStyleSimple( pToApply ) )
	{
		//
		// Update list of properties set from code
		//

		CStylePropertyOpacity *pOpacity = CStylePropertyFactory::Create< CStylePropertyOpacity >();
		pToApply->MergeTo( pOpacity );

		if ( !m_pVecElementProperties )
			m_pVecElementProperties = new CUtlVector<StyleEntry_t>();

		// if there is a current property set into the element properties map just clobber it
		CUtlVector<StyleEntry_t> &vec = *m_pVecElementProperties;
		FOR_EACH_VEC( vec, i )
		{
			if ( vec[i].m_StyleSymbol == CStylePropertyOpacity::symbol )
			{
				CStyleProperty *pOld = vec[i].m_pStyleProperty;
				CStylePropertyFactory::FreeStyleProperty( pOld );
				vec.FastRemove( i );

				break;
			}
		}

		// record that this property has been set by code so styles do not override the setting
		int iNew = vec.AddToTail();
		vec[iNew].m_StyleSymbol = CStylePropertyOpacity::symbol;
		vec[iNew].m_pStyleProperty = pOpacity;

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( float &data, CStylePropertyOpacity *pProperty, void *pContext )
{
	data = pProperty ? pProperty->opacity : CStylePropertyOpacity::GetDefault();
}


//-----------------------------------------------------------------------------
// Purpose: Get opacity in msg form for animation thread
//-----------------------------------------------------------------------------
void CPanelStyle::GetOpacityData( OpacityWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyOpacity >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Get BackgroundImgOpacity 
//-----------------------------------------------------------------------------
void CPanelStyle::GetBackgroundImgOpacity( float &BackgroundImgOpacity )
{
	CStylePropertyBackgroundImgOpacity *pValue = (CStylePropertyBackgroundImgOpacity*)FindProperty( CStylePropertyBackgroundImgOpacity::symbol );
	if ( pValue )
	{
		BackgroundImgOpacity = pValue->backgroundImgOpacity;
	}
	else
	{
		BackgroundImgOpacity = CStylePropertyBackgroundImgOpacity::GetDefault();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set BackgroundImgOpacity 
//-----------------------------------------------------------------------------
void CPanelStyle::SetBackgroundImgOpacity( float backgroundImgOpacity )
{
	CStylePropertyBackgroundImgOpacity *pValue = CStylePropertyFactory::Create< CStylePropertyBackgroundImgOpacity >();
	pValue->backgroundImgOpacity = backgroundImgOpacity;
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( float &data, CStylePropertyBackgroundImgOpacity *pProperty, void *pContext )
{
	data = pProperty ? pProperty->backgroundImgOpacity : CStylePropertyBackgroundImgOpacity::GetDefault();
}


//-----------------------------------------------------------------------------
// Purpose: Get BackgroundImgOpacity in msg form for animation thread
//-----------------------------------------------------------------------------
void CPanelStyle::GetBackgroundImgOpacityData( BackgroundImgOpacityWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyBackgroundImgOpacity >( data, commandList );
}






//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( RenderPoint2D_t &data, CStylePropertyScale2DCentered *pProperty, void *pContext )
{
	data.x = pProperty ? pProperty->m_flX : CStylePropertyScale2DCentered::GetDefaultX();
	data.y = pProperty ? pProperty->m_flY : CStylePropertyScale2DCentered::GetDefaultY();
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( float &data, CStylePropertyRotate2DCentered *pProperty, void *pContext )
{
	data = pProperty ? pProperty->m_flDegrees : CStylePropertyRotate2DCentered::GetDefault();
}

//-----------------------------------------------------------------------------
// Purpose: Sets scale2d centered value
//-----------------------------------------------------------------------------
void CPanelStyle::SetScale2DCentered( float flX, float flY )
{
	CStylePropertyScale2DCentered *pValue = CStylePropertyFactory::Create< CStylePropertyScale2DCentered >();
	pValue->m_flX = flX;
	pValue->m_flY = flY;	
	SetProperty( pValue );
}

//-----------------------------------------------------------------------------
// Purpose: Gets scale2d centered value
//-----------------------------------------------------------------------------
void CPanelStyle::GetScale2DCentered( float &flX, float &flY )
{
	CStylePropertyScale2DCentered *pValue = ( CStylePropertyScale2DCentered* )FindProperty( CStylePropertyScale2DCentered::symbol );
	if ( pValue )
	{
		flX = pValue->m_flX;
		flY = pValue->m_flY;
	}
	else
	{
		flX = CStylePropertyScale2DCentered::GetDefaultX();
		flY = CStylePropertyScale2DCentered::GetDefaultY();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get scale 2d message for animation thread
//-----------------------------------------------------------------------------
void CPanelStyle::GetScale2DCenteredData( Scale2DWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyScale2DCentered >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Sets rotate2d centered value
//-----------------------------------------------------------------------------
void CPanelStyle::SetRotate2DCentered( float flDegrees )
{
	CStylePropertyRotate2DCentered *pValue = CStylePropertyFactory::Create< CStylePropertyRotate2DCentered >();
	pValue->m_flDegrees = flDegrees;
	SetProperty( pValue );
}

//-----------------------------------------------------------------------------
// Purpose: Gets rotate2d centered value
//-----------------------------------------------------------------------------
void CPanelStyle::GetRotate2DCentered( float &flDegrees )
{
	CStylePropertyRotate2DCentered *pValue = ( CStylePropertyRotate2DCentered* )FindProperty( CStylePropertyRotate2DCentered::symbol );
	if ( pValue )
	{
		flDegrees = pValue->m_flDegrees;
	}
	else
	{
		flDegrees = CStylePropertyRotate2DCentered::GetDefault();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get rotate2d message for animation thread
//-----------------------------------------------------------------------------
void CPanelStyle::GetRotate2DCenteredData( Rotate2DWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyRotate2DCentered >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( RadiusData_t &data, CStylePropertyBorderRadius *pProperty, void *pContext )
{
	// if no property, use defaults
	if ( !pProperty )
		return;

	float flWidth = m_pPanel->GetActualRenderWidth();
	float flHeight = m_pPanel->GetActualRenderHeight();

	data.top_left.horizontal = pProperty->m_rgCornerRaddi[ CStylePropertyBorderRadius::k_ECornerTopLeft ].m_HorizontalRadii.GetValueAsLength( flWidth );
	data.top_left.vertical = pProperty->m_rgCornerRaddi[ CStylePropertyBorderRadius::k_ECornerTopLeft ].m_VerticalRadii.GetValueAsLength( flHeight );

	data.top_right.horizontal = pProperty->m_rgCornerRaddi[ CStylePropertyBorderRadius::k_ECornerTopRight ].m_HorizontalRadii.GetValueAsLength( flWidth );
	data.top_right.vertical = pProperty->m_rgCornerRaddi[ CStylePropertyBorderRadius::k_ECornerTopRight ].m_VerticalRadii.GetValueAsLength( flHeight );

	data.bottom_right.horizontal = pProperty->m_rgCornerRaddi[ CStylePropertyBorderRadius::k_ECornerBottomRight ].m_HorizontalRadii.GetValueAsLength( flWidth );
	data.bottom_right.vertical = pProperty->m_rgCornerRaddi[ CStylePropertyBorderRadius::k_ECornerBottomRight ].m_VerticalRadii.GetValueAsLength( flHeight );

	data.bottom_left.horizontal = pProperty->m_rgCornerRaddi[ CStylePropertyBorderRadius::k_ECornerBottomLeft ].m_HorizontalRadii.GetValueAsLength( flWidth );
	data.bottom_left.vertical = pProperty->m_rgCornerRaddi[ CStylePropertyBorderRadius::k_ECornerBottomLeft ].m_VerticalRadii.GetValueAsLength( flHeight );
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( BorderData_t &data, CStylePropertyBorder *pProperty, void *pContext )
{
	// if no property, use defaults
	if ( !pProperty )
		return;

	float flWidth = m_pPanel->GetActualRenderWidth();
	float flHeight = m_pPanel->GetActualRenderHeight();

	data.top.style = ( uint32 )pProperty->m_rgBorderStyle[ 0 ];
	data.top.width = pProperty->m_rgBorderWidth[ 0 ].GetValueAsLength( flWidth );
	data.top.color = pProperty->m_rgBorderColor[ 0 ].GetRawColor();

	data.right.style = ( uint32 )pProperty->m_rgBorderStyle[ 1 ];
	data.right.width = pProperty->m_rgBorderWidth[ 1 ].GetValueAsLength( flHeight );
	data.right.color = pProperty->m_rgBorderColor[ 1 ].GetRawColor();

	data.bottom.style = ( uint32 )pProperty->m_rgBorderStyle[ 2 ];
	data.bottom.width = pProperty->m_rgBorderWidth[ 2 ].GetValueAsLength( flWidth );
	data.bottom.color = pProperty->m_rgBorderColor[ 2 ].GetRawColor();

	data.left.style = ( uint32 )pProperty->m_rgBorderStyle[ 3 ];
	data.left.width = pProperty->m_rgBorderWidth[ 3 ].GetValueAsLength( flHeight );
	data.left.color = pProperty->m_rgBorderColor[ 3 ].GetRawColor();
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( BoxShadowData_t &data, CStylePropertyBoxShadow *pProperty, void *pContext )
{
	// if no property, use defaults
	if ( !pProperty )
		return;

	float flWidth = m_pPanel->GetActualRenderWidth();
	float flHeight = m_pPanel->GetActualRenderHeight();

	data.inset = pProperty->m_bInset;
	data.fill = pProperty->m_bFill;
	data.horizontal_offset = pProperty->m_HorizontalOffset.GetValueAsLength( flWidth );
	data.vertical_offset = pProperty->m_VerticalOffset.GetValueAsLength( flHeight );
	data.blur_radius = pProperty->m_BlurRadius.GetValueAsLength( ( flHeight + flWidth ) / 2.0f );
	data.spread_distance = pProperty->m_SpreadDistance.GetValueAsLength( ( flHeight + flWidth ) / 2.0f );
	data.color = pProperty->m_ShadowColor.GetRawColor();
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( TextShadowData_t &data, CStylePropertyTextShadow *pProperty, void *pContext )
{
	// if no property, use defaults
	if ( !pProperty )
		return;

	float flWidth = m_pPanel->GetActualRenderWidth();
	float flHeight = m_pPanel->GetActualRenderHeight();

	data.horizontal_offset = pProperty->m_HorizontalOffset.GetValueAsLength( flWidth );
	data.vertical_offset = pProperty->m_VerticalOffset.GetValueAsLength( flHeight );
	data.blur_radius = pProperty->m_BlurRadius.GetValueAsLength( ( flHeight + flWidth ) / 2.0f );
	data.strength = pProperty->m_flStrength;
	data.color = pProperty->m_ShadowColor.GetRawColor();
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( ImageShadowData_t &data, CStylePropertyImageShadow *pProperty, void *pContext )
{
	// if no property, use defaults
	if ( !pProperty )
		return;

	float flWidth = m_pPanel->GetActualRenderWidth();
	float flHeight = m_pPanel->GetActualRenderHeight();

	data.horizontal_offset = pProperty->m_HorizontalOffset.GetValueAsLength( flWidth );
	data.vertical_offset = pProperty->m_VerticalOffset.GetValueAsLength( flHeight );
	data.blur_radius = pProperty->m_BlurRadius.GetValueAsLength( ( flHeight + flWidth ) / 2.0f );
	data.strength = pProperty->m_flStrength;
	data.color = pProperty->m_ShadowColor.GetRawColor();
}


//-----------------------------------------------------------------------------
void CPanelStyle::SetRadialClip( bool bRadialClip, const panorama::CUILength &x, const panorama::CUILength &y, float flStartAngle, float flSectorAngle )
{
	CStylePropertyClip *pValue = CStylePropertyFactory::Create< CStylePropertyClip >();
	pValue->m_bRadialClip = bRadialClip;
	pValue->m_RadialCenterX = x;
	pValue->m_RadialCenterY = y;
	pValue->m_flRadialStartAngle = flStartAngle;
	pValue->m_flRadialSectorAngle = flSectorAngle;
	SetProperty( pValue );
}

//-----------------------------------------------------------------------------
void CPanelStyle::GetRadialClip( bool &bRadialClip, panorama::CUILength &x, panorama::CUILength &y, float &flStartAngle, float &flSectorAngle )
{
	CStylePropertyClip *pValue = (CStylePropertyClip *)FindProperty( CStylePropertyClip::symbol );
	if ( pValue )
	{
		x = pValue->m_RadialCenterX;
		y = pValue->m_RadialCenterY;
		flStartAngle = pValue->m_flRadialStartAngle;
		flSectorAngle = pValue->m_flRadialSectorAngle;
	}
	else
	{
		bRadialClip = false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( ClipData_t &data, CStylePropertyClip *pProperty, void *pContext )
{
	// if no property, use defaults
	if ( !pProperty )
		return;

	CRenderCommandList *pCommandList = reinterpret_cast< CRenderCommandList * >( pContext );

	float flWidth = m_pPanel->GetActualRenderWidth();
	float flHeight = m_pPanel->GetActualRenderHeight();

	if ( pProperty->m_bRectClip )
	{
		data.rect_clip = pCommandList->AllocType< RectClipData_t >();
		data.rect_clip->left = pProperty->m_Left.GetValueAsLength( flWidth );
		data.rect_clip->top = pProperty->m_Top.GetValueAsLength( flHeight );
		data.rect_clip->right = pProperty->m_Right.GetValueAsLength( flWidth );
		data.rect_clip->bottom = pProperty->m_Bottom.GetValueAsLength( flHeight );
	}

	if ( pProperty->m_bRadialClip )
	{
		data.radial_clip = pCommandList->AllocType< RadialClipData_t >();
		data.radial_clip->center_x = pProperty->m_RadialCenterX.GetValueAsLength( flWidth );
		data.radial_clip->center_y = pProperty->m_RadialCenterY.GetValueAsLength( flHeight );
		data.radial_clip->start_angle = pProperty->m_flRadialStartAngle;
		data.radial_clip->sector_angle = pProperty->m_flRadialSectorAngle;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get border into msg form
//-----------------------------------------------------------------------------
void CPanelStyle::GetBorderData( BorderWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyBorder >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Get border radius into msg form
//-----------------------------------------------------------------------------
void CPanelStyle::GetBorderRadiusData( BorderRadiusWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyBorderRadius >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Get box shadow into msg form
//-----------------------------------------------------------------------------
void CPanelStyle::GetBoxShadowData( BoxShadowWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyBoxShadow >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Get text shadow into msg form
//-----------------------------------------------------------------------------
void CPanelStyle::GetTextShadowData( TextShadowWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyTextShadow >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Get text shadow into msg form
//-----------------------------------------------------------------------------
void CPanelStyle::GetImageShadowData( ImageShadowWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyImageShadow >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Get clip into msg form
//-----------------------------------------------------------------------------
void CPanelStyle::GetClipData( ClipWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyClip >( data, commandList, &commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the font style
//-----------------------------------------------------------------------------
void CPanelStyle::SetFontStyle( const char *pchFontFamily, float flSize, EFontStyle style, EFontWeight weight )
{
	CStylePropertyFont *pValue = CStylePropertyFactory::Create< CStylePropertyFont >();
	pValue->m_strFontFamily = pchFontFamily;
	pValue->m_flFontSize = flSize;
	pValue->m_eFontStyle = style;
	pValue->m_eFontWeight = weight;
	SetProperty( pValue );
}

//-----------------------------------------------------------------------------
// Purpose: Updates font style values if not set using only this panel style's values
//-----------------------------------------------------------------------------
void CPanelStyle::UpdateFontStyleNoInherit( const char **pchFontFamily, float &flSize, EFontStyle &style, EFontWeight &weight )
{
	CStylePropertyFont *pValue = (CStylePropertyFont*)FindProperty( CStylePropertyFont::symbol );
	if( !pValue )
		return;

	if ( *pchFontFamily == NULL && !pValue->m_strFontFamily.IsEmpty() )
		*pchFontFamily = pValue->m_strFontFamily.String();

	if ( flSize == k_flFloatNotSet )
		flSize = pValue->m_flFontSize;

	if ( style == k_EFontStyleUnset )
		style = pValue->m_eFontStyle;

	if ( weight == k_EFontWeightUnset )
		weight = pValue->m_eFontWeight;
}


//-----------------------------------------------------------------------------
// Purpose: Get font family
//-----------------------------------------------------------------------------
void CPanelStyle::GetFontStyleNoDefaults( const char **pchFontFamily, float &flSize, EFontStyle &style, EFontWeight &weight )
{	
	flSize = k_flFloatNotSet;
	style = k_EFontStyleUnset;
	weight = k_EFontWeightUnset;
	*pchFontFamily = NULL;

	// set values from this style
	UpdateFontStyleNoInherit( pchFontFamily, flSize, style, weight );

	// each of the font values should inherit, so if unset, retrieve parent's value
	IUIPanel *pParent = m_pPanel ? m_pPanel->GetParent() : NULL;
	for( IUIPanel *pPanel = pParent; pPanel != NULL; pPanel = pPanel->GetParent() )
	{
		// stop when all properties are set
		if ( pchFontFamily && flSize != k_flFloatNotSet && style != k_EFontStyleUnset && weight != k_EFontWeightUnset )
			break;

		((CUIPanel*)pPanel)->AccessStyle()->UpdateFontStyleNoInherit( pchFontFamily, flSize, style, weight );		
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get font info. Will return default values if unset.
//-----------------------------------------------------------------------------
void CPanelStyle::GetFontStyle( const char **pchFontFamily, float &flSize, EFontStyle &style, EFontWeight &weight )
{
	GetFontStyleNoDefaults( pchFontFamily, flSize, style, weight );

	if ( !*pchFontFamily )
		*pchFontFamily = "Arial";

	if ( flSize == k_flFloatNotSet )
		flSize = 12;

	if ( style == k_EFontStyleUnset )
		style = k_EFontStyleNormal;

	if ( weight == k_EFontWeightUnset )
		weight = k_EFontWeightNormal;
}


//-----------------------------------------------------------------------------
// Purpose: Get line height
//-----------------------------------------------------------------------------
void CPanelStyle::GetLineHeight( float &flLineHeight )
{
	CStyleProperty *pProperty = NULL;
	FindPropertyInfo( CStylePropertyLineHeight::symbol, &pProperty, NULL, NULL );
	if ( pProperty )
	{
		CStylePropertyLineHeight *pValue = (CStylePropertyLineHeight*)pProperty;
		flLineHeight = pValue->m_flLineHeight;
	}
	else
	{
		flLineHeight = k_flFloatNotSet;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get text align
//-----------------------------------------------------------------------------
void CPanelStyle::GetTextAlign( ETextAlign &align )
{
	CStyleProperty *pProperty = NULL;
	FindPropertyInfo( CStylePropertyTextAlign::symbol, &pProperty, NULL, NULL );
	if ( pProperty )
	{
		CStylePropertyTextAlign *pValue = (CStylePropertyTextAlign*)pProperty;
		align = pValue->m_eAlign;
	}
	else
	{
		align = k_ETextAlignLeft;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get text letter spacing
//-----------------------------------------------------------------------------
void CPanelStyle::GetTextLetterSpacing( int &spacing )
{
	CStyleProperty *pProperty = NULL;
	FindPropertyInfo( CStylePropertyTextLetterSpacing::symbol, &pProperty, NULL, NULL );
	if ( pProperty )
	{
		CStylePropertyTextLetterSpacing *pValue = (CStylePropertyTextLetterSpacing*)pProperty;
		spacing = pValue->m_nLetterSpacing;
	}
	else
	{
		spacing = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gets text decoration
//-----------------------------------------------------------------------------
void CPanelStyle::GetTextDecoration( ETextDecoration &decoration )
{
	CStyleProperty *pProperty = NULL;
	FindPropertyInfo( CStylePropertyTextDecoration::symbol, &pProperty, NULL, NULL );
	if ( pProperty )
	{
		CStylePropertyTextDecoration *pValue = (CStylePropertyTextDecoration*)pProperty;
		decoration = pValue->m_eDecoration;
	}
	else
	{
		decoration = CStylePropertyTextDecoration::GetDefault();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gets text transform
//-----------------------------------------------------------------------------
void CPanelStyle::GetTextTransform( ETextTransform &transform )
{
	CStyleProperty *pProperty = NULL;
	FindPropertyInfo( CStylePropertyTextTransform::symbol, &pProperty, NULL, NULL );
	if ( pProperty )
	{
		CStylePropertyTextTransform *pValue = (CStylePropertyTextTransform*)pProperty;
		transform = pValue->m_eTransform;
	}
	else
	{
		transform = CStylePropertyTextTransform::GetDefault();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get foreground color
//-----------------------------------------------------------------------------
void CPanelStyle::GetForegroundFillBrushCollection( CFillBrushCollection &c )
{
	CStyleProperty *pProperty = NULL ;
	FindPropertyInfo( CStylePropertyForegroundColor::symbol, &pProperty, NULL, NULL );
	if ( pProperty )
	{
		CStylePropertyForegroundColor *pValue = (CStylePropertyForegroundColor*)pProperty;
		c = pValue->m_FillBrushCollection;
	}
	else
	{
		// default foreground color to red so we can find and fix missing styles
		c.AddFillBrush( CFillBrush( 0xff, 0, 0, 0xff ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Determine if the style has transforms set that affect 3d/depth values
//-----------------------------------------------------------------------------
bool CPanelStyle::BHasNon2DTransforms()
{
	CStylePropertyTransform3D *transform = (CStylePropertyTransform3D*)FindProperty( CStylePropertyTransform3D::symbol );
	if( transform )
	{
		if( transform->BHasNon2DTransforms() )
			return true;
	}

	// Now check for transition/animation
	short iInProgress = -1;
	FindPropertyInTransition( CStylePropertyTransform3D::symbol, &iInProgress );
	if( iInProgress != m_treePropertiesInTransition.InvalidIndex() )
	{
		PropertyInTransition_t *pTransition = m_treePropertiesInTransition[iInProgress];

		CStylePropertyTransform3D *pTransitionTransform = (CStylePropertyTransform3D*)pTransition->m_pStyleProperty;
		if( pTransitionTransform->BHasNon2DTransforms() )
			return true;
	}

	if( m_pvecActiveAnimations )
	{
		CUtlVector< CActiveAnimation * > &vec = *m_pvecActiveAnimations;
		FOR_EACH_VEC( vec, i )
		{
			const CActiveAnimation::VecPropertyFrameData_t *pFrameData = vec[i]->GetFrameData( CStylePropertyTransform3D::symbol );
			if( pFrameData )
			{
				FOR_EACH_VEC( *pFrameData, iFrame )
				{
					CStylePropertyTransform3D *pFrameTransform = (CStylePropertyTransform3D*)pFrameData->Element( iFrame ).m_pStyleProperty;
					if( pFrameTransform->BHasNon2DTransforms() )
						return true;
				}
			}
		}
	}


	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Set all the 3d transform attributes
//-----------------------------------------------------------------------------
void CPanelStyle::SetTransform3D( const CUtlVector<CTransform3D *> &vecTransforms )
{
	CStylePropertyTransform3D *pTransform = CStylePropertyFactory::Create< CStylePropertyTransform3D >();
	FOR_EACH_VEC( vecTransforms, i )
	{
		Assert( vecTransforms[i] != NULL );
		pTransform->AddTransform3D( vecTransforms[i] );
	}

	pTransform->MarkSet();
	SetProperty( pTransform );
}


//-----------------------------------------------------------------------------
// Purpose: Set all the 3d transform attributes immediately
//-----------------------------------------------------------------------------
void CPanelStyle::SetTransform3DWithoutTransition( const CUtlVector<CTransform3D *> &vecTransforms )
{
	CStylePropertyTransform3D *pTransform = CStylePropertyFactory::Create< CStylePropertyTransform3D >();
	FOR_EACH_VEC( vecTransforms, i )
	{
		Assert( vecTransforms[i] != NULL );
		pTransform->AddTransform3D( vecTransforms[i] );
	}

	pTransform->MarkSet();
	SetProperty( pTransform, false );
}


//-----------------------------------------------------------------------------
// Purpose: Simplified version of SetTransform3D that will call a simplified version of SetProperty
//			ie bypassing updating the corresponding panel - it is the responsibility of the calling
//			code to update the corresponding UIPanel (such as calling InvalidateSizeAndPosition, AfterStyleApplied, ...).
//			Also assuming no animation / transition set for transform (via CSS or code)
//			Returns true if style has been updated with the new value, false otherwise.
//-----------------------------------------------------------------------------
bool CPanelStyle::SetTransform3DSimple( const CUtlVector<CTransform3D *> &vecTransforms )
{
	//
	// Apply transform property to style
	//

	CStylePropertyTransform3D *pToApply = CStylePropertyFactory::Create< CStylePropertyTransform3D >();
	FOR_EACH_VEC( vecTransforms, i )
	{
		Assert( vecTransforms[i] != NULL );
		pToApply->AddTransform3D( vecTransforms[i] );
	}
	pToApply->MarkSet();

	if ( SetPropertyFromStyleSimple( pToApply ) )
	{
		//
		// Update list of properties set from code
		//

		CStylePropertyTransform3D *pTransform = CStylePropertyFactory::Create< CStylePropertyTransform3D >();
		pToApply->MergeTo( pTransform );

		if ( !m_pVecElementProperties )
			m_pVecElementProperties = new CUtlVector<StyleEntry_t>();

		// if there is a current property set into the element properties map just clobber it
		CUtlVector<StyleEntry_t> &vec = *m_pVecElementProperties;
		FOR_EACH_VEC( vec, i )
		{
			if ( vec[i].m_StyleSymbol == CStylePropertyTransform3D::symbol )
			{
				CStyleProperty *pOld = vec[i].m_pStyleProperty;
				CStylePropertyFactory::FreeStyleProperty( pOld );
				vec.FastRemove( i );

				break;
			}
		}

		// record that this property has been set by code so styles do not override the setting
		int iNew = vec.AddToTail();
		vec[iNew].m_StyleSymbol = CStylePropertyTransform3D::symbol;
		vec[iNew].m_pStyleProperty = pTransform;

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Get 3D transform matrix
//-----------------------------------------------------------------------------
VMatrix CPanelStyle::GetTransform3DMatrix()
{
	CStylePropertyTransform3D *transform = (CStylePropertyTransform3D*)FindProperty( CStylePropertyTransform3D::symbol );
	if( transform )
	{
		return transform->GetTransformMatrix( GetParentActualRenderWidth(), GetParentActualRenderHeight() );
	}
	else
	{
		return CStylePropertyTransform3D::GetDefault();
	}
}

void CPanelStyle::GetTransforms( CUtlVector<CTransform3D*>& inTransforms )
{
	CStylePropertyTransform3D *transform = (CStylePropertyTransform3D*) FindProperty( CStylePropertyTransform3D::symbol );
	if ( transform )
	{
		transform->GetTransforms( inTransforms );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Will the transform be the identity matrix regardless of parent size,
// we need this vs just calling GetTransform3DMatrix() and checking for identity
// because % based sizes may break things otherwise
//-----------------------------------------------------------------------------
bool CPanelStyle::BTransformIsIdentityRegardlessOfParentSize()
{
	CStylePropertyTransform3D *transform = (CStylePropertyTransform3D*)FindProperty( CStylePropertyTransform3D::symbol );
	if( !transform )
		return true;
	else
	{
		// Don't allow zero width/height to be used here, since it just means we don't know yet, and 
		// that means we shouldn't cache off a matrix or use it with those values to decide we have an
		// identity transform since we don't know that until we have the correct values.

		float flParentWidth = GetParentActualRenderWidth();
		float flParentHeight = GetParentActualRenderHeight();

		if( flParentWidth > -0.001f && flParentWidth < 0.001f )
			flParentWidth = 10.0f;

		if( flParentHeight > -0.001f && flParentHeight < 0.001f )
			flParentHeight = 10.0f;

		return transform->GetTransformMatrix( flParentWidth, flParentHeight ).IsIdentity();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( float &data, CStylePropertyPerspective *pProperty, void *pContext )
{
	data = pProperty ? pProperty->perspective : CStylePropertyPerspective::GetDefault();
}


//-----------------------------------------------------------------------------
// Purpose: Get 3D transform perspective cmd data
//-----------------------------------------------------------------------------
void CPanelStyle::GetTransformationPerspectiveData( TransformPerspectiveWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyPerspective >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( RenderPoint_t &data, CStylePropertyPerspectiveOrigin *pProperty, void *pContext )
{
	CUILength x, y;
	bool bInvert;
	if ( pProperty )
	{
		x = pProperty->x;
		y = pProperty->y;
		bInvert = pProperty->m_bInvert;
	}
	else
	{
		CStylePropertyPerspectiveOrigin::GetDefault( &x, &y, &bInvert );
	}

	x.ConvertToLength( m_pPanel->GetActualRenderWidth() );
	y.ConvertToLength( m_pPanel->GetActualRenderHeight() );
	if ( !bInvert )
	{
		data.x = -( x.GetValue() - m_pPanel->GetActualRenderWidth() / 2.0f );
		data.y = -( y.GetValue() - m_pPanel->GetActualRenderHeight() / 2.0f );
	}
	else
	{
		data.x = x.GetValue() - m_pPanel->GetActualRenderWidth() / 2.0f;
		data.y = y.GetValue() - m_pPanel->GetActualRenderHeight() / 2.0f;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get 3D transform perspective origin cmd data
//-----------------------------------------------------------------------------
void CPanelStyle::GetTransformationPerspectiveOriginData( TransformPerspectiveOriginWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyPerspectiveOrigin >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( TransformOriginData_t &data, CStylePropertyTransformOrigin *pProperty, void *pContext )
{
	CUILength x, y;
	bool bParentRelative = false;
	if ( pProperty )
	{
		x = pProperty->x;
		y = pProperty->y;
		bParentRelative = pProperty->m_bParentRelative;
	}
	else
	{
		CStylePropertyTransformOrigin::GetDefault( &x, &y, &bParentRelative );
	}

	Assert( x.IsPercent() || x.IsLength() );
	Assert( y.IsPercent() || y.IsLength() );

	data.x = x.GetValue();
	data.x_is_percent = x.IsPercent();

	data.y = y.GetValue();
	data.y_is_percent = y.IsPercent();

	data.is_parent_relative = bParentRelative;
}


//-----------------------------------------------------------------------------
// Purpose: Get 3D transform origin cmd data
//-----------------------------------------------------------------------------
void CPanelStyle::GetTransformationOriginData( TransformOriginWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyTransformOrigin >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose: Helper to fill in transition data in render commands
//-----------------------------------------------------------------------------
void CPanelStyle::FillInTransitionData( TransitionData_t *pData, PropertyInTransition_t *pTransition )
{
	pData->start_time = pTransition->m_flTransitionStartTime;
	pData->timing_func = pTransition->m_transitionData.m_eTimingFunction;
	if ( pTransition->m_transitionData.m_eTimingFunction == k_EAnimationCustomBezier )
	{
		pData->cubic_bezier_0 = pTransition->m_transitionData.m_CubicBezier.ControlPoint( 1 ).x;
		pData->cubic_bezier_1 = pTransition->m_transitionData.m_CubicBezier.ControlPoint( 1 ).y;
		pData->cubic_bezier_2 = pTransition->m_transitionData.m_CubicBezier.ControlPoint( 2 ).x;
		pData->cubic_bezier_3 = pTransition->m_transitionData.m_CubicBezier.ControlPoint( 2 ).y;
	}
	pData->delay_seconds = GetScaledTransitionTime( pTransition->m_transitionData.m_flTransitionDelaySeconds );
	pData->duration_seconds = GetScaledTransitionTime( pTransition->m_transitionData.m_flTransitionSeconds );
}


//-----------------------------------------------------------------------------
// Purpose: Helper to fill in animation data in render commands
//-----------------------------------------------------------------------------
void CPanelStyle::FillInAnimationData( BaseAnimationData_t *pData, const CActiveAnimation *pAnimation )
{
	const AnimationProperty_t &data = pAnimation->GetAnimationData();

	pData->start_time = pAnimation->GetStartTime();
	pData->duration_seconds = data.m_flDuration;
	pData->delay_seconds = data.m_flDelay;
	pData->timing_func = data.m_eTimingFunction;
	if ( data.m_eTimingFunction == k_EAnimationCustomBezier )
	{
		pData->cubic_bezier_0 = data.m_CubicBezier.ControlPoint( 1 ).x;
		pData->cubic_bezier_1 = data.m_CubicBezier.ControlPoint( 1 ).y;
		pData->cubic_bezier_2 = data.m_CubicBezier.ControlPoint( 2 ).x;
		pData->cubic_bezier_3 = data.m_CubicBezier.ControlPoint( 2 ).y;
	}
	pData->fillMode = data.m_eAnimationFillMode;
	pData->direction = data.m_eAnimationDirection;
	pData->iteration = data.m_flIteration;
}


//-----------------------------------------------------------------------------
// Purpose: Helper to fill in animation frame data in render commands
//-----------------------------------------------------------------------------
void CPanelStyle::FillInAnimationFrameData( BaseAnimationFrameData_t *pFrameData, const CActiveAnimation::PropertyFrameData_t &frameDataToCopy )
{
	pFrameData->percent = frameDataToCopy.m_flPercent;
	if ( frameDataToCopy.m_eTimingFunction != k_EAnimationUnset )
	{
		pFrameData->timing_func = frameDataToCopy.m_eTimingFunction;
		if ( frameDataToCopy.m_eTimingFunction == k_EAnimationCustomBezier )
		{
			pFrameData->cubic_bezier_0 = frameDataToCopy.m_CubicBezier.ControlPoint( 1 ).x;
			pFrameData->cubic_bezier_1 = frameDataToCopy.m_CubicBezier.ControlPoint( 1 ).y;
			pFrameData->cubic_bezier_2 = frameDataToCopy.m_CubicBezier.ControlPoint( 2 ).x;
			pFrameData->cubic_bezier_3 = frameDataToCopy.m_CubicBezier.ControlPoint( 2 ).y;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Overload for setting render message data
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( RenderMatrix4x4_t &renderMatrix, CStylePropertyTransform3D *pProperty, void *pContext )
{
	VMatrix matrix = pProperty ? pProperty->GetTransformMatrix( GetParentActualRenderWidth(), GetParentActualRenderHeight() ) : CStylePropertyTransform3D::GetDefault();
	VMatrixToRenderMatrix( renderMatrix, matrix );
}


//-----------------------------------------------------------------------------
// Purpose: Helper which can fill property, transition, and animation data for standard render command formats
//
// The data must have the following fields: base, transition, transition_data, animation_data, animation_frames
// You must also declare a SetRenderMsgData() to convert a property to the protobuf message data type (base, transition, and data in animation frame's type)
// - Your SetRenderMsgData() must handle receiving a NULL property pointer, which will be the default base value
//-----------------------------------------------------------------------------
template < class PROPERTY_TYPE, class DATA_TYPE >
void CPanelStyle::FillRenderData( DATA_TYPE &dataParam, CRenderCommandList &commandList, void *pContext )
{
	typedef typename DATA_TYPE::AnimationData_t PropertyAnimationData_t;
	typedef typename DATA_TYPE::AnimationFrameData_t PropertyAnimationFrameData_t;

	CStyleProperty *pBase = NULL;
	PropertyInTransition_t *pTransition = NULL;
	CUtlVector< CActiveAnimation * > vecAnimations;
	FindPropertyInfo( PROPERTY_TYPE::symbol, &pBase, &pTransition, &vecAnimations );

	dataParam.style_symbol = PROPERTY_TYPE::symbol.GetID();

	SetRenderData( dataParam.base, ( PROPERTY_TYPE * )pBase, pContext );

	// Determine if transition on related properties is in progress
	if ( pTransition )
	{
		dataParam.transition_data = commandList.AllocType< TransitionData_t >();
		FillInTransitionData( dataParam.transition_data, pTransition );
		SetRenderData( dataParam.transition, ( PROPERTY_TYPE* )pTransition->m_pStyleProperty, pContext );
	}

	//// add animation data if necessary
	if ( vecAnimations.Count() > 0 )
	{
		CRenderDataListBuilder< PropertyAnimationData_t > outputAnimations( dataParam.animations, &commandList );
		FOR_EACH_VEC( vecAnimations, iAnimation )
		{
			CActiveAnimation *pAnimation = vecAnimations[ iAnimation ];
			PropertyAnimationData_t *pAnimationData = outputAnimations.AddToTail();
			FillInAnimationData( pAnimationData, pAnimation );

			// add frames to variable data section
			const CActiveAnimation::VecPropertyFrameData_t *pvecFrameData = pAnimation->GetFrameData( PROPERTY_TYPE::symbol );
			if ( !pvecFrameData || pvecFrameData->Count() == 0 )
				continue;

			CRenderDataListBuilder< PropertyAnimationFrameData_t > outputFrames( pAnimationData->frames, &commandList );

			// check if a 0% frame was specified. If not, use the start value
			const CActiveAnimation::PropertyFrameData_t &firstFrame = pvecFrameData->Element( 0 );
			if ( firstFrame.m_flPercent != 0.0f )
			{
				CActiveAnimation::PropertyFrameData_t dataInner;
				dataInner.m_eTimingFunction = k_EAnimationUnset;

				Vector2D vec[ 4 ];
				panorama::GetAnimationCurveControlPoints( k_EAnimationUnset, vec );
				dataInner.m_CubicBezier.SetControlPoints( vec );

				dataInner.m_flPercent = 0.0f;
				dataInner.m_pStyleProperty = pTransition ? pTransition->m_pStyleProperty : pBase;

				PropertyAnimationFrameData_t *pFrameData = outputFrames.AddToTail();
				FillInAnimationFrameData( pFrameData, dataInner );

				SetRenderData( pFrameData->data, ( PROPERTY_TYPE* )dataInner.m_pStyleProperty, pContext );
			}

			FOR_EACH_VEC( *pvecFrameData, iFrames )
			{
				const CActiveAnimation::PropertyFrameData_t &frameDataToCopy = pvecFrameData->Element( iFrames );

				PropertyAnimationFrameData_t *pFrameData = outputFrames.AddToTail();
				FillInAnimationFrameData( pFrameData, frameDataToCopy );

				SetRenderData( pFrameData->data, ( PROPERTY_TYPE* )frameDataToCopy.m_pStyleProperty, pContext );
			}

			// check if a 100% frame was specified. If not, use the start value (this is all undefined by the spec. Maybe it is better to calculate the interpolated value when the animation ends?)
			const CActiveAnimation::PropertyFrameData_t &lastFrame = pvecFrameData->Element( pvecFrameData->Count() - 1 );
			if ( lastFrame.m_flPercent != 100.0f )
			{
				CActiveAnimation::PropertyFrameData_t dataInner;
				dataInner.m_eTimingFunction = k_EAnimationUnset;

				Vector2D vec[ 4 ];
				panorama::GetAnimationCurveControlPoints( k_EAnimationUnset, vec );
				dataInner.m_CubicBezier.SetControlPoints( vec );

				dataInner.m_flPercent = 100.0f;
				dataInner.m_pStyleProperty = pTransition ? pTransition->m_pStyleProperty : pBase;

				PropertyAnimationFrameData_t *pFrameData = outputFrames.AddToTail();
				FillInAnimationFrameData( pFrameData, dataInner );

				SetRenderData( pFrameData->data, ( PROPERTY_TYPE* )dataInner.m_pStyleProperty, pContext );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get 3D transform matrix cmd data
//-----------------------------------------------------------------------------
void CPanelStyle::GetTransformationData( TransformMatrixWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyTransform3D >( data, commandList );
}


//-----------------------------------------------------------------------------
// Purpose:  Get animation control curve points
//-----------------------------------------------------------------------------
void CPanelStyle::GetAnimationCurveControlPoints( EAnimationTimingFunction eTransitionEffect, Vector2D vecPoints[4] )
{
	return panorama::GetAnimationCurveControlPoints( eTransitionEffect, vecPoints );
}

//-----------------------------------------------------------------------------
// Purpose: Set set of transition properties
//-----------------------------------------------------------------------------
void CPanelStyle::SetTransitionProperties( const CUtlVector< TransitionProperty_t > &vecTransitionProperties )
{
	CStylePropertyTransitionProperties *pValue = CStylePropertyFactory::Create< CStylePropertyTransitionProperties >();
	pValue->m_vecTransitionProperties.AddVectorToTail( vecTransitionProperties );
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Set set of transition properties
//-----------------------------------------------------------------------------
bool CPanelStyle::BHasConstantOpaqueBackground()
{
	// We don't try to detect if there is a transition or animation as it may change on animation thread
	if ( BHasTransitionOrAnimation( CStylePropertyBackgroundColor::symbol ) )
		return false;

	const CFillBrushCollection *pCollection = GetBackgroundFillBrushCollection();
	if ( !pCollection || pCollection->GetBrushCount() < 1 )
		return false;

	for ( uint32 iBrush = 0; iBrush < pCollection->GetBrushCount(); ++iBrush )
	{
		const CFillBrushCollection::FillBrush_t &brush = pCollection->AccessBrushes()[ iBrush ];
		if ( brush.m_Opacity > 0.9999f )
		{
			if ( brush.m_Brush.GetType() == CFillBrush::k_EStrokeTypeFillColor && brush.m_Brush.GetFillColor().a() >= 255 )
				return true;

			if ( brush.m_Brush.GetType() == CFillBrush::k_EStrokeTypeLinearGradient || brush.m_Brush.GetType() == CFillBrush::k_EStrokeTypeRadialGradient )
			{
				const CUtlVector<CGradientColorStop> &stops = brush.m_Brush.AccessStopColors();
				bool bAllOpaque = true;
				FOR_EACH_VEC( stops, i )
				{
					if ( stops[i].GetColor().a() < 255 )
					{
						bAllOpaque = false;
						break;
					}
				}

				if ( bAllOpaque )
					return true;
			}

		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Early out check if the panel has any possible current background color
//-----------------------------------------------------------------------------
bool CPanelStyle::BHasPossibleBackgroundColor()
{
	if ( BHasTransitionOrAnimation( CStylePropertyBackgroundColor::symbol ) )
		return true;
	else 
	{
		const CFillBrushCollection *pCollection = GetBackgroundFillBrushCollection();
		if ( !pCollection )
			return false;

		if ( pCollection->GetBrushCount() > 1 )
			return true;

		if ( pCollection->GetBrushCount() == 1 )
		{
			const CFillBrushCollection::FillBrush_t &brush = pCollection->AccessBrushes()[ 0 ];
			if ( brush.m_Opacity > 0.0f && 
				( brush.m_Brush.GetType() != CFillBrush::k_EStrokeTypeFillColor || brush.m_Brush.GetFillColor().a() > 0.0f ) )
			{
				return true;
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Gets the background color
//-----------------------------------------------------------------------------
const CFillBrushCollection *CPanelStyle::GetBackgroundFillBrushCollection()
{
	CStylePropertyBackgroundColor *pValue = (CStylePropertyBackgroundColor*)FindProperty( CStylePropertyBackgroundColor::symbol );
	return pValue ? &pValue->m_FillBrushCollection : nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Set background color
//-----------------------------------------------------------------------------
void CPanelStyle::SetBackgroundFillBrushCollection( CFillBrushCollection &c )
{
	CStylePropertyBackgroundColor *pValue = CStylePropertyFactory::Create< CStylePropertyBackgroundColor >();
	pValue->m_FillBrushCollection = c;
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Get width property
//-----------------------------------------------------------------------------
void CPanelStyle::GetInterpolatedWidth( CUILength &width, bool bFinal )
{
	VPROF_BUDGET_DETAILED( "CPanelStyle::GetInterpolatedWidth", VPROF_BUDGETGROUP_TENFOOT );

	if ( BHasTransition( CStylePropertyWidth::symbol ) )
	{
		CStylePropertyWidth *pStyle = (CStylePropertyWidth*)GetInterpolatedProperty( CStylePropertyWidth::symbol, bFinal );
		if ( pStyle )
		{
			width = pStyle->m_Length;
			CStylePropertyFactory::FreeStyleProperty( pStyle );
		}
		else
		{
			// Set to default
			width.SetFitChildren();
		}
	}
	else
	{
		GetWidth( width );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get height property
//-----------------------------------------------------------------------------
void CPanelStyle::GetInterpolatedHeight( CUILength &height, bool bFinal )
{
	VPROF_BUDGET_DETAILED( "CPanelStyle::GetInterpolatedHeight", VPROF_BUDGETGROUP_TENFOOT );
	if ( BHasTransition( CStylePropertyHeight::symbol ) )
	{
		CStylePropertyHeight *pStyle = (CStylePropertyHeight*)GetInterpolatedProperty( CStylePropertyHeight::symbol, bFinal );
		if ( pStyle )
		{
			height = pStyle->m_Height;
			CStylePropertyFactory::FreeStyleProperty( pStyle );
		}
		else
		{
			// Set to default
			height.SetFitChildren();
		}
	}
	else
	{
		GetHeight( height );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get white-space wrap property
//-----------------------------------------------------------------------------
void CPanelStyle::GetWhitespaceWrap( bool &bWrap )
{
	CStylePropertyWhiteSpace *pValue = (CStylePropertyWhiteSpace*)FindProperty( CStylePropertyWhiteSpace::symbol );
	if( pValue )
	{
		bWrap = pValue->m_bWrap;
	}
	else
	{
		bWrap = true;
	}
}



//-----------------------------------------------------------------------------
// Purpose: Get text-overflow property
//-----------------------------------------------------------------------------
void CPanelStyle::GetTextOverflow( ETextOverflow &eTextOverflow )
{
	CStylePropertyTextOverflow *pValue = (CStylePropertyTextOverflow*)FindProperty( CStylePropertyTextOverflow::symbol );
	if( pValue )
	{
		eTextOverflow = pValue->m_eTextOverflow;
	}
	else
	{
		eTextOverflow = CStylePropertyTextOverflow::GetDefault();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get width property
//-----------------------------------------------------------------------------
void CPanelStyle::GetWidth( CUILength &width )
{
	CStylePropertyWidth *pValue = (CStylePropertyWidth*)FindProperty( CStylePropertyWidth::symbol );
	if( pValue )
	{
		width = pValue->m_Length;
	}
	else
	{
		width.SetFitChildren();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set width property
//-----------------------------------------------------------------------------
void CPanelStyle::SetWidth( CUILength width )
{
	CStylePropertyWidth *pValue = CStylePropertyFactory::Create< CStylePropertyWidth >();
	pValue->m_Length = width;
	
	if ( pValue->m_Length.GetType() == CUILength::k_EUILengthLength )
		AssertMsg( IsFinite( pValue->m_Length.GetValue() ), "Setting invalid width" );
	
	SetProperty( pValue );
}

//-----------------------------------------------------------------------------
// Purpose: Set width property
//-----------------------------------------------------------------------------
void CPanelStyle::SetWidthWithoutTransition( CUILength width )
{
	CStylePropertyWidth *pValue = CStylePropertyFactory::Create< CStylePropertyWidth >();
	pValue->m_Length = width;

	if ( pValue->m_Length.GetType() == CUILength::k_EUILengthLength )
		AssertMsg( IsFinite( pValue->m_Length.GetValue() ), "Setting invalid width" );

	SetProperty( pValue, false );
}


//-----------------------------------------------------------------------------
// Purpose: Get height property
//-----------------------------------------------------------------------------
void CPanelStyle::GetHeight( CUILength &height )
{
	CStylePropertyHeight *pValue = (CStylePropertyHeight*)FindProperty( CStylePropertyHeight::symbol );
	if( pValue )
	{
		height = pValue->m_Height;
	}
	else
	{
		height.SetFitChildren();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set height property
//-----------------------------------------------------------------------------
void CPanelStyle::SetHeight( CUILength height )
{
	CStylePropertyHeight *pValue = CStylePropertyFactory::Create< CStylePropertyHeight >();
	pValue->m_Height = height;
	if ( pValue->m_Height.GetType() == CUILength::k_EUILengthLength )
		AssertMsg( IsFinite( pValue->m_Height.GetValue() ), "Setting invalid height" );

	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Set height property
//-----------------------------------------------------------------------------
void CPanelStyle::SetHeightWithoutTransition( CUILength height )
{
	CStylePropertyHeight *pValue = CStylePropertyFactory::Create< CStylePropertyHeight >();
	pValue->m_Height = height;
	if ( pValue->m_Height.GetType() == CUILength::k_EUILengthLength )
		AssertMsg( IsFinite( pValue->m_Height.GetValue() ), "Setting invalid height" );

	SetProperty( pValue, false );
}


//-----------------------------------------------------------------------------
// Purpose: Get min-width property
//-----------------------------------------------------------------------------
void CPanelStyle::GetMinWidth( CUILength &minWidth )
{
	CStylePropertyMinWidth *pValue = (CStylePropertyMinWidth*)FindProperty( CStylePropertyMinWidth::symbol );
	if( pValue )
	{
		minWidth = pValue->GetMinWidth();
	}
	else
	{
		minWidth.Set( 0, CUILength::k_EUILengthUnset );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set min-width property
//-----------------------------------------------------------------------------
void CPanelStyle::SetMinWidth( CUILength minWidth )
{
	CStylePropertyMinWidth *pValue = CStylePropertyFactory::Create< CStylePropertyMinWidth >();
	pValue->SetMinWidth( minWidth );
	if ( pValue->GetMinWidth().GetType() == CUILength::k_EUILengthLength )
		AssertMsg( IsFinite( pValue->GetMinWidth().GetValue() ), "Setting invalid min-width" );

	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Get min-height property
//-----------------------------------------------------------------------------
void CPanelStyle::GetMinHeight( CUILength &minHeight )
{
	CStylePropertyMinHeight *pValue = (CStylePropertyMinHeight*)FindProperty( CStylePropertyMinHeight::symbol );
	if( pValue )
	{
		minHeight = pValue->GetMinHeight();
	}
	else
	{
		minHeight.Set( 0, CUILength::k_EUILengthUnset );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set min-height property
//-----------------------------------------------------------------------------
void CPanelStyle::SetMinHeight( CUILength minHeight )
{
	CStylePropertyMinHeight *pValue = CStylePropertyFactory::Create< CStylePropertyMinHeight >();
	pValue->SetMinHeight( minHeight );
	if ( pValue->GetMinHeight().GetType() == CUILength::k_EUILengthLength )
		AssertMsg( IsFinite( pValue->GetMinHeight().GetValue() ), "Setting invalid min-height" );

	SetProperty( pValue );
}

//-----------------------------------------------------------------------------
// Purpose: Get max-width property
//-----------------------------------------------------------------------------
void CPanelStyle::GetInterpolatedMaxWidth( CUILength &width, bool bFinal )
{
//	VPROF_BUDGET( "CPanelStyle::GetInterpolatedMaxWidth", VPROF_BUDGETGROUP_TENFOOT );

	if ( BHasTransition( CStylePropertyMaxWidth::symbol ) )
	{
		CStylePropertyMaxWidth *pStyle = (CStylePropertyMaxWidth*)GetInterpolatedProperty( CStylePropertyMaxWidth::symbol, bFinal );
		if ( pStyle )
		{
			width = pStyle->GetMaxWidth();
			CStylePropertyFactory::FreeStyleProperty( pStyle );
		}
		else
		{
			// Set to default
			width.SetFitChildren();
		}
	}
	else
	{
		GetMaxWidth( width );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get max-height property
//-----------------------------------------------------------------------------
void CPanelStyle::GetInterpolatedMaxHeight( CUILength &height, bool bFinal )
{
	VPROF_BUDGET_DETAILED( "CPanelStyle::GetInterpoloatedMaxHeight", VPROF_BUDGETGROUP_TENFOOT );
	if ( BHasTransition( CStylePropertyMaxHeight::symbol ) )
	{
		CStylePropertyMaxHeight *pStyle = (CStylePropertyMaxHeight*)GetInterpolatedProperty( CStylePropertyMaxHeight::symbol, bFinal );
		if ( pStyle )
		{
			height = pStyle->GetMaxHeight();
			CStylePropertyFactory::FreeStyleProperty( pStyle );
		}
		else
		{
			// Set to default
			height.SetFitChildren();
		}
	}
	else
	{
		GetMaxHeight( height );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get max-width property
//-----------------------------------------------------------------------------
void CPanelStyle::GetMaxWidth( CUILength &maxWidth )
{
	CStylePropertyMaxWidth *pValue = (CStylePropertyMaxWidth*)FindProperty( CStylePropertyMaxWidth::symbol );
	if( pValue )
	{
		maxWidth = pValue->GetMaxWidth();
	}
	else
	{
		maxWidth.Set( 0, CUILength::k_EUILengthUnset );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set max-width property
//-----------------------------------------------------------------------------
void CPanelStyle::SetMaxWidth( CUILength maxWidth )
{
	CStylePropertyMaxWidth *pValue = CStylePropertyFactory::Create< CStylePropertyMaxWidth >();
	pValue->SetMaxWidth( maxWidth );
	if ( pValue->GetMaxWidth().GetType() == CUILength::k_EUILengthLength )
		AssertMsg( IsFinite( pValue->GetMaxWidth().GetValue() ), "Setting invalid min-height" );

	SetProperty( pValue );
}

//-----------------------------------------------------------------------------
// Purpose: Get max-height property
//-----------------------------------------------------------------------------
void CPanelStyle::GetMaxHeight( CUILength &maxHeight )
{
	CStylePropertyMaxHeight *pValue = (CStylePropertyMaxHeight*)FindProperty( CStylePropertyMaxHeight::symbol );
	if( pValue )
	{
		maxHeight = pValue->GetMaxHeight();
	}
	else
	{
		maxHeight.Set( 0, CUILength::k_EUILengthUnset );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set max-height property
//-----------------------------------------------------------------------------
void CPanelStyle::SetMaxHeight( CUILength maxHeight )
{
	CStylePropertyMaxHeight *pValue = CStylePropertyFactory::Create< CStylePropertyMaxHeight >();
	pValue->SetMaxHeight( maxHeight );
	if ( pValue->GetMaxHeight().GetType() == CUILength::k_EUILengthLength )
		AssertMsg( IsFinite( pValue->GetMaxHeight().GetValue() ), "Setting invalid max-height" );

	SetProperty( pValue );
}

//-----------------------------------------------------------------------------
// Purpose: Get visibility property
//-----------------------------------------------------------------------------
void CPanelStyle::GetVisibility( bool &bVisible )
{
	CStylePropertyVisible *pValue = (CStylePropertyVisible*)FindProperty( CStylePropertyVisible::symbol );
	if( pValue )
	{
		bVisible = pValue->GetVisibility();
	}
	else
	{
		bVisible = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set visibility property
//-----------------------------------------------------------------------------
void CPanelStyle::SetVisibility( bool bVisible )
{
	CStylePropertyVisible *pValue = CStylePropertyFactory::Create< CStylePropertyVisible >();
	pValue->SetVisibility( bVisible );
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Get flow direction
//-----------------------------------------------------------------------------
void CPanelStyle::GetFlowChildren( EFlowDirection &eFlowDirection )
{
	CStylePropertyFlowChildren *pValue = (CStylePropertyFlowChildren*)FindProperty( CStylePropertyFlowChildren::symbol );
	if( pValue )
	{
		eFlowDirection = pValue->m_eFlowDirection;
	}
	else
	{
		eFlowDirection = k_EFlowNone;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set flow direction
//-----------------------------------------------------------------------------
void CPanelStyle::SetFlowChildren( EFlowDirection eFlowDirection )
{
	CStylePropertyFlowChildren *pValue = CStylePropertyFactory::Create< CStylePropertyFlowChildren >();
	pValue->m_eFlowDirection = eFlowDirection;
	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: get the opacity mask image 
//-----------------------------------------------------------------------------
void CPanelStyle::GetOpacityMaskImage( IImageSource *& pImage, float *pflOpacityMaskOpacity )
{
	CStylePropertyOpacityMask *pValue = (CStylePropertyOpacityMask*)FindProperty( CStylePropertyOpacityMask::symbol );
	if( pValue )
	{
		pImage = pValue->m_pImage;
		if ( pflOpacityMaskOpacity )
			*pflOpacityMaskOpacity = pValue->m_flOpacityMaskOpacity;
	}
	else
	{
		pImage = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: get the opacity mask msg with transition data and all 
//-----------------------------------------------------------------------------
void CPanelStyle::GetOpacityMaskData( OpacityMaskWithTransition_t &data, CRenderCommandList &commandList )
{
	FillRenderData< CStylePropertyOpacityMask >( data, commandList, &commandList );
}



//-----------------------------------------------------------------------------
// Purpose: get the image 
//-----------------------------------------------------------------------------
CUtlVector< CBackgroundImageLayer * > *CPanelStyle::GetBackgroundImages()
{
	CStylePropertyBackgroundImage *pValue = ( CStylePropertyBackgroundImage* )FindPropertyInTransition( CStylePropertyBackgroundImage::symbol );
	if ( pValue )
		return &pValue->AccessLayers();

	pValue = ( CStylePropertyBackgroundImage* )FindProperty( CStylePropertyBackgroundImage::symbol );
	return pValue ? &pValue->AccessLayers() : nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Sets background images for this style
//-----------------------------------------------------------------------------
void CPanelStyle::SetBackgroundImages( const CUtlVector< CBackgroundImageLayer * > &vecLayers )
{
	CStylePropertyBackgroundImage *pValue = CStylePropertyFactory::Create< CStylePropertyBackgroundImage >();
	pValue->Set( vecLayers );

	SetProperty( pValue );
}


//-----------------------------------------------------------------------------
// Purpose: Get the opacity data for a given background image layer. Note that this we be
// allocated as part of the given command list, so you shouldn't hold onto pointers to this data.
//-----------------------------------------------------------------------------
OpacityWithTransition_t *CPanelStyle::GetBackgroundImageLayerOpacityData( CBackgroundImageLayer *pLayer, CRenderCommandList &commandList )
{
	if ( !pLayer )
		return nullptr;

	// If we applied the style "background-img-opacity", then we want to pass on it's value as opacity.
	// rather than process the transition/animation between two different images.
	// We couold seprate these two properties if using both on a background becomes a requirement
	CStylePropertyBackgroundImgOpacity*  pBkgndImgOpacityValue = (CStylePropertyBackgroundImgOpacity*)FindProperty( CStylePropertyBackgroundImgOpacity::symbol );
	if ( pBkgndImgOpacityValue )
	{
		BackgroundImgOpacityWithTransition_t *pBkgndImgOpacityData = commandList.AllocType< BackgroundImgOpacityWithTransition_t >();
		CPanelStyle::FillRenderData< CStylePropertyBackgroundImgOpacity >( *pBkgndImgOpacityData, commandList, pLayer );

		OpacityWithTransition_t *pOpacityData = commandList.AllocType< OpacityWithTransition_t >();
		CPanelStyle::FillRenderData< CStylePropertyBackgroundImage >( *pOpacityData, commandList, pLayer );
		pOpacityData->base = pBkgndImgOpacityData->base;
		pOpacityData->animations = pBkgndImgOpacityData->animations;
		pOpacityData->transition = pBkgndImgOpacityData->transition;
		pOpacityData->transition_data = pBkgndImgOpacityData->transition_data;

		return pOpacityData;
	}

	// Here we a swapping between images using animated opacity
	CStylePropertyBackgroundImage *pValue = ( CStylePropertyBackgroundImage* )FindProperty( CStylePropertyBackgroundImage::symbol );
	if ( !pValue )
		return nullptr;

	OpacityWithTransition_t *pOpacityData = commandList.AllocType< OpacityWithTransition_t >();
	CPanelStyle::FillRenderData< CStylePropertyBackgroundImage >( *pOpacityData, commandList, pLayer );

	// Only return if we actually need to animate the opacity
	return pOpacityData->transition_data || !pOpacityData->animations.IsEmpty() ? pOpacityData : nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Set the opacity for a specific background image layer
//-----------------------------------------------------------------------------
void CPanelStyle::SetRenderData( float &data, CStylePropertyBackgroundImage *pProperty, void *pContext )
{
	if ( !pProperty )
	{
		// default is 1.0
		data = 1.0;
		return;
	}

	CBackgroundImageLayer *pQueryLayer = reinterpret_cast< CBackgroundImageLayer * >( pContext );

	// Look through all the layers. If we find one that matches, then our desired opacity for this frame is the layer's opacity.
	CUtlVector< CBackgroundImageLayer * > &vecLayers = pProperty->AccessLayers();
	for ( int i = 0; i < vecLayers.Count(); ++i )
	{
		CBackgroundImageLayer *pLayer = vecLayers[ i ];
		if ( *pLayer == *pQueryLayer )
		{
			data = pLayer->GetOpacity();
			return;
		}
	}

	// Didn't find a matching layer
	data = 0.0;
}


//-----------------------------------------------------------------------------
// Purpose: Gets border widths
//-----------------------------------------------------------------------------
void CPanelStyle::GetInterpolatedBorderWidth( CUILength &left, CUILength &top, CUILength &right, CUILength &bottom, bool bFinal )
{
	if ( BHasTransition( CStylePropertyBorder::symbol ) )
	{
		CStylePropertyBorder *pStyle = (CStylePropertyBorder*)GetInterpolatedProperty( CStylePropertyBorder::symbol, bFinal );		
		if ( pStyle )
		{
			top = pStyle->m_rgBorderWidth[0];
			right = pStyle->m_rgBorderWidth[1];
			bottom = pStyle->m_rgBorderWidth[2];
			left = pStyle->m_rgBorderWidth[3];
			CStylePropertyFactory::FreeStyleProperty( pStyle );
		}
		else
		{
			left.SetLength( 0 );
			top.SetLength( 0 );
			bottom.SetLength( 0 );
			right.SetLength( 0 );	
		}
	}
	else
	{
		CStylePropertyBorder *pValue = (CStylePropertyBorder*)FindProperty( CStylePropertyBorder::symbol );
		if( pValue )
		{
			top = pValue->m_rgBorderWidth[0];
			right = pValue->m_rgBorderWidth[1];
			bottom = pValue->m_rgBorderWidth[2];
			left = pValue->m_rgBorderWidth[3];
		}
		else
		{
			left.SetLength( 0 );
			top.SetLength( 0 );
			bottom.SetLength( 0 );
			right.SetLength( 0 );		
		}
	}

}


//-----------------------------------------------------------------------------
// Purpose: Gets total content inset distance which is border-width+padding for each side
// of the box, flBoxWidth and flBoxHeight are needed to deal with % values
//-----------------------------------------------------------------------------
void CPanelStyle::GetContentInset( float flBoxWidth, float flBoxHeight, bool bFinalDimensions, float &left, float &top, float &right, float &bottom )
{
	left = 0.0;
	top = 0.0;
	right = 0.0;
	bottom = 0.0;

	if ( BHasAnyStyleDataForProperty( CStylePropertyBorder::symbol ) )
	{
		CUILength borderLeft, borderTop, borderRight, borderBottom;
		GetInterpolatedBorderWidth( borderLeft, borderTop, borderRight, borderBottom, bFinalDimensions );

		left += borderLeft.GetValueAsLength( flBoxWidth );
		top += borderTop.GetValueAsLength( flBoxHeight );
		right += borderRight.GetValueAsLength( flBoxWidth );
		bottom += borderBottom.GetValueAsLength( flBoxHeight );
	}

	if ( BPropertyBitFlagSet( CStylePropertyPadding::symbol ) )
	{
		CUILength paddingLeft, paddingTop, paddingRight, paddingBottom;
		GetPadding( paddingLeft, paddingTop, paddingRight, paddingBottom );

		left += paddingLeft.GetValueAsLength( flBoxWidth );
		top += paddingTop.GetValueAsLength( flBoxHeight );
		right += paddingRight.GetValueAsLength( flBoxWidth );
		bottom += paddingBottom.GetValueAsLength( flBoxHeight );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check if "content inset" (border-width+padding) property is transitioning
//-----------------------------------------------------------------------------
bool CPanelStyle::BHasContentInsetTransition()
{
	return BHasTransition( CStylePropertyBorder::symbol );
}


//-----------------------------------------------------------------------------
// Purpose: Gets padding for the panel
//-----------------------------------------------------------------------------
void CPanelStyle::GetPadding( CUILength &left, CUILength &top, CUILength &right, CUILength &bottom )
{
	CStylePropertyPadding *pValue = (CStylePropertyPadding*)FindProperty( CStylePropertyPadding::symbol );
	if( pValue )
	{
		left = pValue->m_left;
		top = pValue->m_top;
		bottom = pValue->m_bottom;
		right = pValue->m_right;
	}
	else
	{
		left.SetLength( 0 );
		top.SetLength( 0 );
		bottom.SetLength( 0 );
		right.SetLength( 0 );		
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gets margin for the panel
//-----------------------------------------------------------------------------
void CPanelStyle::GetMargin( CUILength &left, CUILength &top, CUILength &right, CUILength &bottom )
{
	CStylePropertyMargin *pValue = (CStylePropertyMargin*)FindProperty( CStylePropertyMargin::symbol );
	if( pValue )
	{
		left = pValue->m_left;
		top = pValue->m_top;
		bottom = pValue->m_bottom;
		right = pValue->m_right;
	}
	else
	{
		left.SetLength( 0 );
		top.SetLength( 0 );
		bottom.SetLength( 0 );
		right.SetLength( 0 );		
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gets margin for the panel in pixels. flBoxWidth and flBoxHeight are 
//			needed to deal with % values
//-----------------------------------------------------------------------------
void CPanelStyle::GetMargin( float flBoxWidth, float flBoxHeight, float &left, float &top, float &right, float &bottom )
{
	CUILength marginLeft, marginTop, marginRight, marginBottom;
	GetMargin( marginLeft, marginTop, marginRight, marginBottom );

	left = marginLeft.GetValueAsLength( flBoxWidth );
	top = marginTop.GetValueAsLength( flBoxHeight );
	right = marginRight.GetValueAsLength( flBoxWidth );
	bottom = marginBottom.GetValueAsLength( flBoxHeight );
}


//-----------------------------------------------------------------------------
// Purpose: Sets margin for the panel
//-----------------------------------------------------------------------------
void CPanelStyle::SetMargin( CUILength &left, CUILength &top, CUILength &right, CUILength &bottom )
{
	CStylePropertyMargin *pMargin = CStylePropertyFactory::Create< CStylePropertyMargin >();
	pMargin->m_left = left;
	pMargin->m_top = top;
	pMargin->m_right = right;
	pMargin->m_bottom = bottom;

	SetProperty( pMargin );
}


//-----------------------------------------------------------------------------
// Purpose: Gets the horizontal and vertical alignment for a panel
//-----------------------------------------------------------------------------
void CPanelStyle::GetAlignment( EHorizontalAlignment &eHorizontalAlignment, EVerticalAlignment &eVerticalAlignment )
{
	CStylePropertyAlign *pValue = (CStylePropertyAlign*)FindProperty( CStylePropertyAlign::symbol );
	if( pValue )
	{
		eHorizontalAlignment = pValue->m_eHorizontalAlignment;
		eVerticalAlignment = pValue->m_eVerticalAlignment;		
	}
	else
	{
		eHorizontalAlignment = k_EHorizontalAlignmentLeft;
		eVerticalAlignment = k_EVerticalAlignmentTop;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the tooltip positions for a panel
//-----------------------------------------------------------------------------
void CPanelStyle::SetTooltipPositions( const EContextUIPosition( &eTooltipPositions )[ 4 ] )
{
	CStylePropertyTooltipPosition *pTooltipPosition = CStylePropertyFactory::Create< CStylePropertyTooltipPosition >();

	for ( int i = 0; i < V_ARRAYSIZE( eTooltipPositions ); ++i )
	{
		pTooltipPosition->m_ePositions[ i ] = eTooltipPositions[ i ];
	}

	SetProperty( pTooltipPosition );
}


//-----------------------------------------------------------------------------
// Purpose: Gets the tooltip positions for a panel
//-----------------------------------------------------------------------------
void CPanelStyle::GetTooltipPositions( EContextUIPosition( &eTooltipPositions )[ 4 ] )
{
	CStylePropertyTooltipPosition *pValue = ( CStylePropertyTooltipPosition * )FindProperty( CStylePropertyTooltipPosition::symbol );
	if ( pValue )
	{
		for ( int i = 0; i < V_ARRAYSIZE( eTooltipPositions ); ++i )
		{
			eTooltipPositions[ i ] = pValue->m_ePositions[ i ];
		}
	}
	else
	{
		for ( EContextUIPosition &eTooltipPosition : eTooltipPositions )
		{
			eTooltipPosition = k_EContextUIPositionUnset;
		}

		CStylePropertyTooltipPosition::ResolveDefaultPositions( eTooltipPositions );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the tooltip body position
//-----------------------------------------------------------------------------
void CPanelStyle::SetTooltipBodyPosition( const panorama::CUILength &horizontalPosition, const panorama::CUILength &verticalPosition )
{
	CStylePropertyTooltipBodyPosition *pBodyPosition = CStylePropertyFactory::Create< CStylePropertyTooltipBodyPosition >();

	pBodyPosition->m_HorizontalPosition = horizontalPosition;
	pBodyPosition->m_VerticalPosition = verticalPosition;

	SetProperty( pBodyPosition );
}


//-----------------------------------------------------------------------------
// Purpose: Gets the tooltip body position
//-----------------------------------------------------------------------------
void CPanelStyle::GetTooltipBodyPosition( panorama::CUILength &horizontalPosition, panorama::CUILength &verticalPosition )
{
	CStylePropertyTooltipBodyPosition *pValue = ( CStylePropertyTooltipBodyPosition * )FindProperty( CStylePropertyTooltipBodyPosition::symbol );
	if ( pValue )
	{
		horizontalPosition = pValue->m_HorizontalPosition;
		verticalPosition = pValue->m_VerticalPosition;
	}
	else
	{
		CStylePropertyTooltipBodyPosition::ResolveDefaultBodyPosition( horizontalPosition, verticalPosition );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the tooltip arrow position
//-----------------------------------------------------------------------------
void CPanelStyle::SetTooltipArrowPosition( const panorama::CUILength &horizontalPosition, const panorama::CUILength &verticalPosition )
{
	CStylePropertyTooltipArrowPosition *pArrowPosition = CStylePropertyFactory::Create< CStylePropertyTooltipArrowPosition >();

	pArrowPosition->m_HorizontalPosition = horizontalPosition;
	pArrowPosition->m_VerticalPosition = verticalPosition;

	SetProperty( pArrowPosition );
}


//-----------------------------------------------------------------------------
// Purpose: Gets the tooltip arrow position
//-----------------------------------------------------------------------------
void CPanelStyle::GetTooltipArrowPosition( panorama::CUILength &horizontalPosition, panorama::CUILength &verticalPosition )
{
	CStylePropertyTooltipArrowPosition *pValue = ( CStylePropertyTooltipArrowPosition * )FindProperty( CStylePropertyTooltipArrowPosition::symbol );
	if ( pValue )
	{
		horizontalPosition = pValue->m_HorizontalPosition;
		verticalPosition = pValue->m_VerticalPosition;
	}
	else
	{
		CStylePropertyTooltipArrowPosition::ResolveDefaultArrowPosition( horizontalPosition, verticalPosition );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the context menu positions for a panel
//-----------------------------------------------------------------------------
void CPanelStyle::SetContextMenuPositions( const EContextUIPosition( &eContextMenuPositions )[ 4 ] )
{
	CStylePropertyContextMenuPosition *pContextMenuPosition = CStylePropertyFactory::Create< CStylePropertyContextMenuPosition >();

	for ( int i = 0; i < V_ARRAYSIZE( eContextMenuPositions ); ++i )
	{
		pContextMenuPosition->m_ePositions[ i ] = eContextMenuPositions[ i ];
	}

	SetProperty( pContextMenuPosition );
}


//-----------------------------------------------------------------------------
// Purpose: Gets the context menu positions for a panel
//-----------------------------------------------------------------------------
void CPanelStyle::GetContextMenuPositions( EContextUIPosition( &eContextMenuPositions )[ 4 ] )
{
	CStylePropertyContextMenuPosition *pValue = ( CStylePropertyContextMenuPosition * )FindProperty( CStylePropertyContextMenuPosition::symbol );
	if ( pValue )
	{
		for ( int i = 0; i < V_ARRAYSIZE( eContextMenuPositions ); ++i )
		{
			eContextMenuPositions[ i ] = pValue->m_ePositions[ i ];
		}
	}
	else
	{
		for ( EContextUIPosition &eContextMenuPosition : eContextMenuPositions )
		{
			eContextMenuPosition = k_EContextUIPositionUnset;
		}

		CStylePropertyContextMenuPosition::ResolveDefaultPositions( eContextMenuPositions );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the context menu body position
//-----------------------------------------------------------------------------
void CPanelStyle::SetContextMenuBodyPosition( const panorama::CUILength &horizontalPosition, const panorama::CUILength &verticalPosition )
{
	CStylePropertyContextMenuBodyPosition *pBodyPosition = CStylePropertyFactory::Create< CStylePropertyContextMenuBodyPosition >();

	pBodyPosition->m_HorizontalPosition = horizontalPosition;
	pBodyPosition->m_VerticalPosition = verticalPosition;

	SetProperty( pBodyPosition );
}


//-----------------------------------------------------------------------------
// Purpose: Gets the context menu body position
//-----------------------------------------------------------------------------
void CPanelStyle::GetContextMenuBodyPosition( panorama::CUILength &horizontalPosition, panorama::CUILength &verticalPosition )
{
	CStylePropertyContextMenuBodyPosition *pValue = ( CStylePropertyContextMenuBodyPosition * )FindProperty( CStylePropertyContextMenuBodyPosition::symbol );
	if ( pValue )
	{
		horizontalPosition = pValue->m_HorizontalPosition;
		verticalPosition = pValue->m_VerticalPosition;
	}
	else
	{
		CStylePropertyContextMenuBodyPosition::ResolveDefaultBodyPosition( horizontalPosition, verticalPosition );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the context menu arrow position
//-----------------------------------------------------------------------------
void CPanelStyle::SetContextMenuArrowPosition( const panorama::CUILength &horizontalPosition, const panorama::CUILength &verticalPosition )
{
	CStylePropertyContextMenuArrowPosition *pArrowPosition = CStylePropertyFactory::Create< CStylePropertyContextMenuArrowPosition >();

	pArrowPosition->m_HorizontalPosition = horizontalPosition;
	pArrowPosition->m_VerticalPosition = verticalPosition;

	SetProperty( pArrowPosition );
}


//-----------------------------------------------------------------------------
// Purpose: Gets the context menu arrow position
//-----------------------------------------------------------------------------
void CPanelStyle::GetContextMenuArrowPosition( panorama::CUILength &horizontalPosition, panorama::CUILength &verticalPosition )
{
	CStylePropertyContextMenuArrowPosition *pValue = ( CStylePropertyContextMenuArrowPosition * )FindProperty( CStylePropertyContextMenuArrowPosition::symbol );
	if ( pValue )
	{
		horizontalPosition = pValue->m_HorizontalPosition;
		verticalPosition = pValue->m_VerticalPosition;
	}
	else
	{
		CStylePropertyContextMenuArrowPosition::ResolveDefaultArrowPosition( horizontalPosition, verticalPosition );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gets the actual parent width, needed for some % to px conversions
//-----------------------------------------------------------------------------
float CPanelStyle::GetParentActualRenderWidth()
{
	if ( m_pPanel && m_pPanel->GetParent() )
	{
		return m_pPanel->GetParent()->GetActualRenderWidth();
	}
	return 0.0f;
}


//-----------------------------------------------------------------------------
// Purpose: Gets the actual parent height, needed for some % to px conversions
//-----------------------------------------------------------------------------
float CPanelStyle::GetParentActualRenderHeight()
{
	if ( m_pPanel && m_pPanel->GetParent() )
	{
		return m_pPanel->GetParent()->GetActualRenderHeight();
	}
	return 0.0f;
}


//-----------------------------------------------------------------------------
// Purpose: Reset all active animations
//-----------------------------------------------------------------------------
void CPanelStyle::ResetAnimations()
{
	if( m_pvecActiveAnimations )
	{
		CUtlVector< CActiveAnimation * > &vec = *m_pvecActiveAnimations;
		FOR_EACH_VEC( vec, i )
		{
			vec[i]->Reset();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Force any non-looping animations to their final state
//-----------------------------------------------------------------------------
void CPanelStyle::SkipAnimations()
{
	if ( m_pvecActiveAnimations )
	{
		CUtlVector< CActiveAnimation * > vecNewAnimations;
		CUtlVector< CActiveAnimation * > &vecCurrentAnimations = *m_pvecActiveAnimations;

		// Check if we need to do anything
		bool bAnyToSkip = false;
		FOR_EACH_VEC( vecCurrentAnimations, i )
		{
			CActiveAnimation* pActiveAnim = vecCurrentAnimations[i];
			if ( pActiveAnim->GetAnimationData().m_flIteration != k_flFloatInfiniteIteration )
			{
				bAnyToSkip = true;
				break;
			}
		}

		if ( !bAnyToSkip )
			return;

		// We have some animations to skip, kill them
		FOR_EACH_VEC( vecCurrentAnimations, i )
		{
			CActiveAnimation* pActiveAnim = vecCurrentAnimations[i];
			if ( pActiveAnim->GetAnimationData().m_flIteration == k_flFloatInfiniteIteration )
			{
				// This is a looping animation, we need to preserve it.
				CActiveAnimation* pAnimClone = new CActiveAnimation( pActiveAnim->GetStartTime(), pActiveAnim->GetAnimationData() );
				
				// We are just going to delete the old animation data, and SetActiveAnimations() doesn't use the frame data (at the moment, at least!)
				pActiveAnim->MoveFrameData( pAnimClone );

				// Add it to the list
				vecNewAnimations.AddToTail( pAnimClone );
			}
		}

		// Set new active animations
		SetActiveAnimations( vecNewAnimations );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the specified property if set directly on this panel, or NULL if not set
//-----------------------------------------------------------------------------
const CStyleProperty *CPanelStyle::GetPropertyNoInherit( CStyleSymbol symProperty )
{
	// check if transitioning.. return that first
	CStyleProperty *pProperty = FindPropertyInTransition( symProperty );
	if( pProperty )
		return pProperty;
	
	// not transitioning
	return FindProperty( symProperty );
}


//-----------------------------------------------------------------------------
// Purpose: Schedules a transition clean up
//-----------------------------------------------------------------------------
void CPanelStyle::ScheduleTransitionCleanup( double flTime )
{
	if ( flTime > m_flNextTransitionCleanup )
		return;

	m_flNextTransitionCleanup = flTime;
	
	if( m_ScheduledTransitionCleanup.BScheduled() )
		m_ScheduledTransitionCleanup.Cancel();

	m_ScheduledTransitionCleanup.Schedule( m_flNextTransitionCleanup - UIEngine()->GetCurrentFrameTime() );
}


//-----------------------------------------------------------------------------
// Purpose: Cleans up animation and transition properties,
//			and schedules a future call to clean up running transitions and animations
//-----------------------------------------------------------------------------
void CPanelStyle::CleanupTransitionsAndAnimations()
{
	double flNow = UIEngine()->GetCurrentFrameTime();
	double flNextToExpire = FLT_MAX;
	bool bCalculateLayoutFlags = false;
	m_flNextTransitionCleanup = FLT_MAX;
	m_ScheduledTransitionCleanup.Cancel();

	// clean up transitions
	FOR_EACH_RBTREE_FAST( m_treePropertiesInTransition, i )
	{
		PropertyInTransition_t *pTransition = m_treePropertiesInTransition.Element( i );
		CStyleSymbol symProperty = pTransition->m_pStyleProperty->GetPropertySymbol();

		bool bDeleteOriginal = false;
		int iOldProperty = -1;
		CStyleProperty *pOriginalProperty = FindProperty( symProperty, &iOldProperty );
		
		CStyleProperty *pProperty = pTransition->m_pStyleProperty;
		double flStartWithDelay = pTransition->m_flTransitionStartTime + GetScaledTransitionTime( pTransition->m_transitionData.m_flTransitionDelaySeconds );
		double flEnd = flStartWithDelay + GetScaledTransitionTime( pTransition->m_transitionData.m_flTransitionSeconds );

		// if transition started, need to update panel flags
		if ( m_flLastTransitionCleanup < flStartWithDelay && flStartWithDelay <= flNow )
		{
			if ( pProperty->BInvalidatesSizeAndPosition( pOriginalProperty ) || pProperty->BInvalidatesPosition( pOriginalProperty ) || pProperty->BCanTransition() )
				bCalculateLayoutFlags = true;
		}

		// Is the transition completed?
		if ( flEnd <= flNow )
		{
			// remove old property
			if ( iOldProperty != m_vecProperties.InvalidIndex() )
			{
				bDeleteOriginal = true;
			}

			if ( pProperty->BInvalidatesSizeAndPosition( pOriginalProperty ) )
			{
				bCalculateLayoutFlags = true;
				m_pPanel->InvalidateSizeAndPosition();
			}
			else if ( pProperty->BInvalidatesPosition( pOriginalProperty ) )
			{
				bCalculateLayoutFlags = true;
				m_pPanel->InvalidatePosition();
			}

			if( pProperty->BCanTransition() )
				bCalculateLayoutFlags = true;

			// move property in transition to current property
			if( iOldProperty == m_vecProperties.InvalidIndex() )
			{
				iOldProperty = m_vecProperties.AddToTail();
				m_vecProperties[iOldProperty].m_StyleSymbol = pProperty->GetPropertySymbol();
			}

			m_vecProperties[iOldProperty].m_pStyleProperty = pProperty;
			pTransition->m_pStyleProperty = NULL;
			SetHasProperty( pProperty->GetPropertySymbol(), iOldProperty );

			// should be ok to delete old entry and remove.. iterating fast
			delete m_treePropertiesInTransition[i];
			m_treePropertiesInTransition.RemoveAt( i );

			if ( bDeleteOriginal )
				CStylePropertyFactory::FreeStyleProperty( pOriginalProperty );

			pProperty->OnFinishedTransition();

			DispatchEvent( PropertyTransitionEnd(), m_pPanel, symProperty );
			continue;
		}

		// still transition still going
		flNextToExpire = MIN( flNextToExpire, flEnd );
	}

	// clean up animations
	if( m_pvecActiveAnimations )
	{
		CUtlVector< CActiveAnimation * > &vec = *m_pvecActiveAnimations;
		FOR_EACH_VEC_BACK( vec, i )
		{
			CActiveAnimation *pAnimation = vec[i];
			CPanoramaSymbol symAnimation = pAnimation->GetName();
			double flEnd = pAnimation->CalculateAnimationEndTime();

			// if an animation started, fire event
			double flStartWithDelay = pAnimation->GetStartTimeWithDelay();
			if( m_flLastTransitionCleanup < flStartWithDelay && flStartWithDelay <= flNow )
			{
				DispatchEvent( AnimationStart(), m_pPanel, symAnimation );

				if( pAnimation->BAffectsPanelLayoutFlags( this ) )
					bCalculateLayoutFlags = true;
			}

			// check if an animation has ended

			if ( ( pAnimation->GetAnimationData().m_eAnimationFillMode != k_EAnimationFillModeForwards )
				&& (pAnimation->GetAnimationData().m_eAnimationFillMode != k_EAnimationFillModeBoth) )
			{
				if( flEnd <= flNow )
				{
					if( pAnimation->BAffectsPanelLayoutFlags( this ) )
					{
						bCalculateLayoutFlags = true;
						m_pPanel->InvalidateSizeAndPosition();
					}

					vec.Remove( i );
					delete pAnimation;

					DispatchEventAsync( 0.0f, AnimationEnd(), m_pPanel, symAnimation );
					continue;
				}
			}
			else
			{
				if ( (m_flLastTransitionCleanup < flEnd) && ( flEnd <= flNow ) )
				{
					DispatchEventAsync( 0.0f, AnimationEnd(), m_pPanel, symAnimation );
					flEnd = FLT_MAX;
				}
			}

			// if transition has not yet started, schedule for start
			if( flStartWithDelay > flNow )
			{
				flNextToExpire = MIN( flNextToExpire, flStartWithDelay );
			}
			else
			{
				// schedule for expire
				flNextToExpire = MIN( flNextToExpire, flEnd );
			}
		}
		if( vec.Count() == 0 )
			SAFE_DELETE( m_pvecActiveAnimations );
	}

	// need to update panel layout
	if ( bCalculateLayoutFlags )
		SetPanelLayoutFlagsForTransitionAnimation();

	// reschedule
	if ( flNextToExpire != FLT_MAX )
		ScheduleTransitionCleanup( flNextToExpire );	

	m_flLastTransitionCleanup = flNow;	

	if( m_pPanel )
		m_pPanel->OnStyleTransitionsCleanup();
}


//-----------------------------------------------------------------------------
// Purpose: Looks through transitions and animations and sets the appropriate layout flags on a panel
//-----------------------------------------------------------------------------
void CPanelStyle::SetPanelLayoutFlagsForTransitionAnimation()
{
	VPROF_BUDGET( "CPanelStyle::SetPanelLayoutFlagsForTransitionAnimation()", VPROF_BUDGETGROUP_TENFOOT );
	bool bAffectsSize = false;
	bool bAffectsPosition = false;

	// check transitions
	double flNow = UIEngine()->GetCurrentFrameTime();
	FOR_EACH_RBTREE_FAST( m_treePropertiesInTransition, i )
	{
		PropertyInTransition_t *pTransition = m_treePropertiesInTransition.Element( i );
		double flStart = pTransition->m_flTransitionStartTime + GetScaledTransitionTime( pTransition->m_transitionData.m_flTransitionDelaySeconds );
		if( flStart > flNow )
			continue;

		double flEnd = pTransition->m_flTransitionStartTime + GetScaledTransitionTime( pTransition->m_transitionData.m_flTransitionDelaySeconds ) + GetScaledTransitionTime( pTransition->m_transitionData.m_flTransitionSeconds );
		if( flNow > flEnd )
			continue;

		CStyleProperty *pProperty = pTransition->m_pStyleProperty;

		CStyleProperty *pBase = FindProperty( pProperty->GetPropertySymbol() );
		if( pProperty->BInvalidatesSizeAndPosition( pBase ) )
		{
			bAffectsSize = true;
			bAffectsPosition = true;
			break;
		}

		if( pProperty->BInvalidatesPosition( pBase ) )
			bAffectsPosition = true;
	}

	// check animations
	if( !bAffectsSize )
	{
		if( m_pvecActiveAnimations )
		{
			CUtlVector< CActiveAnimation * > &vec = *m_pvecActiveAnimations;
			FOR_EACH_VEC( vec, i )
			{
				bool bAnimationAffectsSize;
				bool bAnimationsAffectsPosition;
				vec[i]->GetAffectedPanelLayoutFlags( this, &bAnimationAffectsSize, &bAnimationsAffectsPosition );

				if( bAnimationAffectsSize )
				{
					bAffectsSize = true;
					bAffectsPosition = true;
					break;
				}

				if( bAnimationsAffectsPosition )
					bAffectsPosition = true;
			}
		}
	}

	if( bAffectsSize )
	{
		m_pPanel->SetActiveSizeAndPositionTransition();
	}
	else if( bAffectsPosition )
	{
		((CUIPanel*)m_pPanel)->ClearLayoutTransitionFlagsBubble( CUIPanel::k_EPanelLayoutSizeTransitionActive );
		m_pPanel->SetActivePositionTransition();
	}
	else
	{
		((CUIPanel*)m_pPanel)->ClearLayoutTransitionFlagsBubble( CUIPanel::k_EPanelLayoutSizeTransitionActive | CUIPanel::k_EPanelLayoutPositionTransitionActive );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Fills in a vector with property symbols that do not exist in the specified style property tree, but are set on this panel style
//-----------------------------------------------------------------------------
void CPanelStyle::BuildListOfExistingPropsNotInTree( CStyleProperty **rgToCheck, bool *rgPropertiesToRemove )
{
	// check current
	FOR_EACH_VEC( m_vecProperties, i )
	{
		uint8 iSym = m_vecProperties[i].m_StyleSymbol.GetID();
		if ( rgToCheck[iSym] == NULL )
			rgPropertiesToRemove[iSym] = true;
	}

	// check in transition
	FOR_EACH_RBTREE_FAST( m_treePropertiesInTransition, i )
	{
		uint8 iSym = m_treePropertiesInTransition[i]->m_pStyleProperty->GetPropertySymbol().GetID();
		if ( rgToCheck[iSym] == NULL )
			rgPropertiesToRemove[iSym] = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns symbols for animations (active and complete)
//-----------------------------------------------------------------------------
void CPanelStyle::GetAnimationNames( CUtlVector< CPanoramaSymbol > *pvecAnimations )
{
	pvecAnimations->RemoveAll();

	CStylePropertyAnimationProperties *pProperty = (CStylePropertyAnimationProperties*)FindProperty( CStylePropertyAnimationProperties::symbol );
	if( !pProperty )
		return;
	
	pvecAnimations->EnsureCapacity( pProperty->m_vecAnimationProperties.Count() );
	FOR_EACH_VEC( pProperty->m_vecAnimationProperties, i )
	{
		pvecAnimations->AddToTail( pProperty->m_vecAnimationProperties[i].m_symName );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Check if property is set from element styles
//-----------------------------------------------------------------------------
bool CPanelStyle::BPropertySetFromElement( CStyleSymbol symProperty ) const
{ 
	return GetPropertyFromElementStyle( symProperty ) != NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Clear property set from code. Once a panel styles are reloaded, properties from CSS will apply.
//			Caller should invalidate panel styles
//-----------------------------------------------------------------------------
void CPanelStyle::ClearPropertySetFromElement( CStyleSymbol symProperty )
{
	if( m_pVecElementProperties )
	{
		CUtlVector<StyleEntry_t> &vec = *m_pVecElementProperties;
		FOR_EACH_VEC( vec, i )
		{
			if( vec[i].m_StyleSymbol == symProperty )
			{
				CStylePropertyFactory::FreeStyleProperty( vec[i].m_pStyleProperty );
				vec.Remove( i );
				RemoveProperty( symProperty );
				m_pPanel->MarkStylesDirty( true );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the property as set by element style
//-----------------------------------------------------------------------------
CStyleProperty *CPanelStyle::GetPropertyFromElementStyle( CStyleSymbol symProperty ) const
{
	if( !m_pVecElementProperties )
		return NULL;

	CUtlVector<StyleEntry_t> &vec = *m_pVecElementProperties;
	FOR_EACH_VEC( vec, i )
	{
		if( vec[i].m_StyleSymbol == symProperty )
			return vec[i].m_pStyleProperty;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Return a transition time scaled to the time factor convar
//-----------------------------------------------------------------------------
float CPanelStyle::GetScaledTransitionTime( float flTime )
{
	return flTime / s_convarTransitionTimeFactor.GetFloat();
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Validate
//-----------------------------------------------------------------------------
void PropertyInTransition_t::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	// Don't validate style property, it's allocated from a self-validating pool.  Do have
	// it validate it's own members.
	m_pStyleProperty->Validate( validator, "CStyleProperty" );
}


//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CPanelStyle::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_vecProperties );
	
	// Don't validate elements, they are allocated from a pool and those pools will be validated separately.  Do have
	// them validate their own members.
	FOR_EACH_VEC( m_vecProperties, i )
	{
		m_vecProperties[i].m_pStyleProperty->Validate( validator, "CStyleProperty" );
	}
	
	
	ValidateObj( m_treePropertiesInTransition );
	FOR_EACH_RBTREE_FAST( m_treePropertiesInTransition, i )
	{
		ValidatePtr( m_treePropertiesInTransition[i] );
	}

	if( m_pvecActiveAnimations )
	{
		ValidatePtr( m_pvecActiveAnimations );
		CUtlVector< CActiveAnimation * > &vec = *m_pvecActiveAnimations;
		FOR_EACH_VEC( vec, i )
		{
			ValidatePtr( vec[i] );
		}
	}

	// Don't validate elements, they are allocated from a pool and those pools will be validated separately.  Do have
	// them validate their own members.
	ValidatePtr( m_pVecElementProperties );
	if( m_pVecElementProperties )
	{
		FOR_EACH_VEC( *m_pVecElementProperties, i )
		{
			m_pVecElementProperties->Element( i ).m_pStyleProperty->Validate( validator, "CStyleProperty" );
		}
	}

	ValidateObj( m_ScheduledTransitionCleanup );
}
#endif

//-----------------------------------------------------------------------------
// CJSKeyframesObject constructor
//-----------------------------------------------------------------------------
CJSKeyframesObject::CJSKeyframesObject( CSmartPtr< CLayoutFile> pLayoutFile, const char *pchKeyframesName )
{
	// See if this type is registered with UIEngine
	if ( !UIEngine()->IsObjectTypeExposedToJavaScript( GetJSTypeName() ) )
	{
		CUtlAbstractDelegate del = UtlMakeDelegate( this, &CJSKeyframesObject::SetupJavaScriptObjectTemplate ).GetAbstractDelegate();
		UIEngine()->ExposeObjectTypeToJavaScript( GetJSTypeName(), del );
	}

	// Find keyframes in the style set
	const CStyleAnimation *pStyleAnimation = pLayoutFile->GetAnimation( CPanoramaSymbol( pchKeyframesName ) );
	if ( pStyleAnimation != nullptr )
	{
		m_pKeyframes = pStyleAnimation->CopyKeyframes();
	}
	else
	{
		Msg("Failed to find keyframes %s in layout file %s\n", pchKeyframesName, pLayoutFile->GetLayoutFileSymbol().String() );
	}
};

//-----------------------------------------------------------------------------
// CJSKeyframesObject destructor
//-----------------------------------------------------------------------------
CJSKeyframesObject::~CJSKeyframesObject()
{
	if ( m_pKeyframes )
	{
		int nNumKeyframes = m_pKeyframes->Count();
		for ( int i = 0; i < nNumKeyframes; i++ )
		{
			delete (*m_pKeyframes)[i];
		}

		delete m_pKeyframes;
	}
}

//-----------------------------------------------------------------------------
// CJSKeyframesObject::FindClosestKeyframe
//-----------------------------------------------------------------------------
int CJSKeyframesObject::FindClosestKeyframe( float flPercent )
{
	int nResult = m_pKeyframes->InvalidIndex();

	if ( m_pKeyframes )
	{
		float flMinDistance = FLT_MAX;
		int nCount = m_pKeyframes->Count();
		for ( int i = 0 ; i < nCount; i++ )
		{
			CStyleKeyFrame *pKeyframe = (*m_pKeyframes)[ i ];
			float flDistance = fabsf( pKeyframe->GetPercent() - flPercent );
			if ( flDistance < flMinDistance )
			{
				nResult = i;
				flMinDistance = flDistance;
			}
		}
	}

	return nResult;
}

//-----------------------------------------------------------------------------
// CJSKeyframesObject::IsKeyframeValid
//-----------------------------------------------------------------------------
bool CJSKeyframesObject::IsKeyframeValid( int nKeyframeIndex )
{
	return m_pKeyframes->IsValidIndex( nKeyframeIndex );
}

//-----------------------------------------------------------------------------
// CJSKeyframesObject::IsKeyframeValid
//-----------------------------------------------------------------------------
int CJSKeyframesObject::InsertCopyOfKeyframe( float flDstPercent, int nSrcKeyframeIndex )
{
	int nResult = m_pKeyframes->InvalidIndex();

	if ( m_pKeyframes->IsValidIndex( nSrcKeyframeIndex ) )
	{
		CStyleKeyFrame *pSrcKeyframe = (*m_pKeyframes)[ nSrcKeyframeIndex ];
		CStyleKeyFrame *pDstKeyframe = pSrcKeyframe->Copy();
		pDstKeyframe->SetPercent( flDstPercent );
		nResult = InsertAnimationFrame( m_pKeyframes, pDstKeyframe );
	}

	return nResult;
}


//-----------------------------------------------------------------------------
// CJSKeyframesObject::SetProperty
//-----------------------------------------------------------------------------
void CJSKeyframesObject::SetKeyframeProperty( int nKeyframeIndex, const char *pszStyleProperty )
{
	if ( m_pKeyframes->IsValidIndex( nKeyframeIndex ) )
	{
		CStyleKeyFrame *pKeyFrame = (*m_pKeyframes)[ nKeyframeIndex ];
		
		StylePropertyHash_t *pKeyFrameProperties = pKeyFrame->GetProperties();
		
		CUtlBuffer buff;
		buff.CopyBuffer( pszStyleProperty, V_strlen( pszStyleProperty ) );
		CUtlVector< StyleFilePtr_t > styleFile;
		BParseStyleBody( buff, pKeyFrameProperties, CPanoramaSymbol( "CJSKeyframesObject" ), nullptr, styleFile, true );
	}
}

//-----------------------------------------------------------------------------
// CJSKeyframesObject::SetupJavaScriptObjectTemplate
//-----------------------------------------------------------------------------
void CJSKeyframesObject::SetupJavaScriptObjectTemplate()
{
	RegisterJSMethod( "FindClosestKeyframe", PANORAMA_DELEGATE( &CJSKeyframesObject::FindClosestKeyframe ), 
		"Return handle H to animation keyframe closest to for given %. Returns invalid handle if not found. Check with call to IsKeyframeValid(H)." );
	
	RegisterJSMethod( "IsKeyframeValid",  PANORAMA_DELEGATE( &CJSKeyframesObject::IsKeyframeValid ),
		"Check if handle returned by FindKeyframe is valid.");
	
	RegisterJSMethod( "SetKeyframeProperty",  PANORAMA_DELEGATE( &CJSKeyframesObject::SetKeyframeProperty ),
		"Set individual properties in keyframe. Example input: 'x:200px;'");

	RegisterJSMethod( "InsertCopyOfKeyframe",  PANORAMA_DELEGATE( &CJSKeyframesObject::InsertCopyOfKeyframe ),
		"InsertCopyOfKeyframe.");
}

