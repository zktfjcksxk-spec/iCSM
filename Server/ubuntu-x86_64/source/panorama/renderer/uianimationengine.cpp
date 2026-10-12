//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uianimationengine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


using namespace panorama;

#if defined( SOURCE2_PANORAMA )
ConVar panorama::s_convarPanoramaAll2DTranslatesDontNeedCompositonLayer( "@panorama_2d_translate_no_comp_layer", "1", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar panorama::s_convarPanoramaMightScrollDontNeedCompositonLayer( "@panorama_might_scroll_no_comp_layer", "1", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar panorama::s_convarPanoramaBoxShadowsDontNeedCompositonLayer( "@panorama_box_shadow_no_comp_layer", "1", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar panorama::s_convarPanoramaSimpleBordersDontNeedCompositonLayer( "@panorama_simple_borders_no_comp_layer", "1", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );

// This works, but it means each operation in a panel is transformed and then clips to it's untransformed bounds, whereas if we transform after drawing a panel 
// the clipping is different.  So it's a behavior change and some panels could need "noclip, noclip" if they are transformed when they didn't before.
ConVar panorama::s_convarPanoramaAllTransformsDontNeedCompositonLayer( "@panorama_transforms_no_comp_layer", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );

// This allows parents of transformed children to not have layers just for transform origin... perspective impacting transforms still need a parent layer until further work
ConVar panorama::s_convarPanoramaTransformParentsNoLayerForPerspective( "@panorama_transform_parents_no_layer_for_perspective", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
#else

//
// To turn these on in Steam we need surface work to support these operations without composition layers!!!
//

ConVar panorama::s_convarPanoramaAll2DTranslatesDontNeedCompositonLayer( "@panorama_2d_translate_no_comp_layer", "1" );
ConVar panorama::s_convarPanoramaMightScrollDontNeedCompositonLayer( "@panorama_might_scroll_no_comp_layer", "0" );
ConVar panorama::s_convarPanoramaBoxShadowsDontNeedCompositonLayer( "@panorama_box_shadow_no_comp_layer", "0" );
ConVar panorama::s_convarPanoramaSimpleBordersDontNeedCompositonLayer( "@panorama_simple_borders_no_comp_layer", "0" );

// This works, but it means each operation in a panel is transformed and then clips to it's untransformed bounds, whereas if we transform after drawing a panel 
// the clipping is different.  So it's a behavior change and some panels could need "noclip, noclip" if they are transformed when they didn't before.
ConVar panorama::s_convarPanoramaAllTransformsDontNeedCompositonLayer( "@panorama_transforms_no_comp_layer", "0" );

// This allows parents of transformed children to not have layers just for transform origin... perspective impacting transforms still need a parent layer until further work
ConVar panorama::s_convarPanoramaTransformParentsNoLayerForPerspective( "@panorama_transform_parents_no_layer_for_perspective", "0" );
#endif

static ConVar s_convarPanoramaKeepPanelInLayerCache( "@panorama_keep_panel_in_layer_cache", "1" );
static ConVar s_convarPanoramaKeepTextInFontCache( "@panorama_keep_text_in_font_cache", "1" );
static ConVar s_convarPanoramaAnimForceSortChildOps( "@panorama_force_sort_child_ops", "0", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: This class takes a source 2d quadrilateral and a target 2d quadrilateral,
// it can then compute a transform matrix that converts the source into the target,
// and can be used to apply that matrix to any point, those converting between quadrilateral
// coordinate spaces.  
//
// Useful for getting relative mouse positions within untransformed quads when you have
// the mouse coords and transformed quad coords in screenspace.
//-----------------------------------------------------------------------------
namespace panorama
{
class CQuad2DWarper
{
public:

	CQuad2DWarper()
	{
		SetSourceQuad( Vector2D( 0.0f, 0.0f ), Vector2D( 1.0f, 0.0f ), Vector2D( 1.0f, 1.0f ), Vector2D( 0.0f, 1.0f ) );
		SetDestinationQuad( Vector2D( 0.0f, 0.0f ), Vector2D( 1.0f, 0.0f ), Vector2D( 1.0f, 1.0f ), Vector2D( 0.0f, 1.0f ) );
		m_WarpMatrix = VMatrix::GetIdentityMatrix();
		m_bDirty = false;
	}

	CQuad2DWarper( Vector2D srcTopLeft, Vector2D srcTopRight, Vector2D srcBottomRight, Vector2D srcBottomLeft,
		Vector2D dstTopLeft, Vector2D dstTopRight, Vector2D dstBottomRight, Vector2D dstBottomLeft )
	{
		SetSourceQuad( srcTopLeft, srcTopRight, srcBottomRight, srcBottomLeft );
		SetDestinationQuad( dstTopLeft, dstTopRight, dstBottomRight, dstBottomLeft );
		ComputeTransformMatrix();
	}

	void SetSourceQuad( Vector2D topLeft, Vector2D topRight, Vector2D bottomRight, Vector2D bottomLeft )
	{
		m_SrcQuad[0] = topLeft;
		m_SrcQuad[1] = topRight;
		m_SrcQuad[2] = bottomRight;
		m_SrcQuad[3] = bottomLeft;
		m_bDirty = true;
	}

	void SetDestinationQuad( Vector2D topLeft, Vector2D topRight, Vector2D bottomRight, Vector2D bottomLeft )
	{
		m_DestQuad[0] = topLeft;
		m_DestQuad[1] = topRight;
		m_DestQuad[2] = bottomRight;
		m_DestQuad[3] = bottomLeft;
		m_bDirty = true;
	}

	VMatrix GetTransformMatrix()
	{
		if ( m_bDirty )
			ComputeTransformMatrix();

		return m_WarpMatrix;
	}

	Vector2D TransformPoint( Vector2D point )
	{
		if ( m_bDirty )
			ComputeTransformMatrix();

		// We ignore Z here, as this is only a 2d quad warp, so matrix math is simplified, but we do
		// care about W for the perspective divide
		float result[3];
		result[0] = (point.x * m_WarpMatrix[0][0] + point.y*m_WarpMatrix[1][0] + m_WarpMatrix[3][0]);
		result[1] = (point.x * m_WarpMatrix[0][1] + point.y*m_WarpMatrix[1][1] + m_WarpMatrix[3][1]);
		result[2] = (point.x * m_WarpMatrix[0][3] + point.y*m_WarpMatrix[1][3] + m_WarpMatrix[3][3]);        
		return Vector2D( result[0]/result[2], result[1]/result[2] );
	}

private:

	void ComputeTransformMatrix()
	{
		ComputeQuadToSquare( m_SrcQuad[0], m_SrcQuad[1], m_SrcQuad[2], m_SrcQuad[3], m_SrcMatrix );
		ComputeSquareToQuad( m_DestQuad[0], m_DestQuad[1], m_DestQuad[2], m_DestQuad[3], m_DestMatrix );
		m_WarpMatrix = m_SrcMatrix * m_DestMatrix;
		m_bDirty = false;
	}

	void ComputeSquareToQuad( Vector2D topLeft, Vector2D topRight, Vector2D bottomRight, Vector2D bottomLeft, VMatrix &mat )
	{
		float dx1 = topRight.x - bottomLeft.x;
		float dy1 = topRight.y - bottomLeft.y;
		float dx2 = bottomRight.x - bottomLeft.x;
		float dy2 = bottomRight.y - bottomLeft.y;
		float sx = topLeft.x - topRight.x + bottomLeft.x - bottomRight.x;
		float sy = topLeft.y - topRight.y + bottomLeft.y - bottomRight.y;
		float g = (sx * dy2 - dx2 * sy) / (dx1 * dy2 - dx2 * dy1);
		float h = (dx1 * sy - sx * dy1) / (dx1 * dy2 - dx2 * dy1);
		float a = topRight.x - topLeft.x + g * topRight.x;
		float b = bottomRight.x - topLeft.x + h * bottomRight.x;
		float c = topLeft.x;
		float d = topRight.y - topLeft.y + g * topRight.y;
		float e = bottomRight.y - topLeft.y + h * bottomRight.y;
		float f = topLeft.y;

		mat[0][0] = a;	
		mat[0][1] = d;
		mat[0][2] = 0;
		mat[0][3] = g;

		mat[1][0] = b;
		mat[1][1] = e;
		mat[1][2] = 0;
		mat[1][3] = h;

		mat[2][0] = 0;
		mat[2][1] = 0;
		mat[2][2] = 1;
		mat[2][3] = 0;

		mat[3][0] = c;
		mat[3][1] = f;
		mat[3][2] = 0;
		mat[3][3] = 1;
	}

	void ComputeQuadToSquare( Vector2D topLeft, Vector2D topRight, Vector2D bottomRight, Vector2D bottomLeft, VMatrix &mat ) 
	{
		ComputeSquareToQuad( topLeft, topRight, bottomRight, bottomLeft, mat );
		VMatrix inverse;
		if ( !mat.InverseGeneral( inverse ) )
		{
			AssertMsg( false, "Unable to invert warper matrix\n" );
			inverse.Identity();
		}
		mat = inverse;
	}

	Vector2D m_SrcQuad[4];
	Vector2D m_DestQuad[4];
	bool m_bDirty;

	VMatrix m_SrcMatrix;
	VMatrix m_DestMatrix;
	VMatrix m_WarpMatrix;
};
} // namespace panorama

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUIAnimationEngine::CUIAnimationEngine( CUIRenderEngine *pRenderEngine ) : 
	m_pRenderEngine( pRenderEngine )
{
	m_unRenderCountThisFrame = 0;
	m_unSkipUntilContextPopCounter = 0;
	m_nLastFrameMillisecondsIndex = -1;
	for( int i=0; i < V_ARRAYSIZE( m_rgflMillisecondsFrame ); ++i )
	{
		m_rgflMillisecondsFrame[i] = FLT_MAX;
	}
	m_nFramesAnimated = 0;
	m_flAnimationFrameTime = 0.0f;

	m_flFinishedFrameGenerationTime = 0.0f;
	m_flCurrentFrameGenerationTime = 0.0f;
	m_flCurrentFrameAnimationTime = 0.0f;
	m_ulPanelHitTestPtrValue = k_ulInvalidPanelHandle64;
	m_PanelHitTestCoords.x = 0.0f;
	m_PanelHitTestCoords.y = 0.0f;
	m_unSurfaceWidth = 0;
	m_unSurfaceHeight = 0;
	m_flMouseX = 0.0f;
	m_flMouseY = 0.0f;
	m_flUIScaleFactor = 1.0f;
	m_pLastScreenSpaceQuadAdded = NULL;
	m_pScreenSpaceQuadOutput = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUIAnimationEngine::~CUIAnimationEngine()
{
	FOR_EACH_VEC( m_vecRenderOpsVectorPtrs, i )
	{
		delete m_vecRenderOpsVectorPtrs[i];
	}
	m_vecRenderOpsVectorPtrs.RemoveAll();

	FOR_EACH_MAP( m_mapParticleSystems, i )
	{
		delete m_mapParticleSystems[i];
	}
	m_mapParticleSystems.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Called on window resize, no thread safe, occurs on animation thread
//-----------------------------------------------------------------------------
void CUIAnimationEngine::OnWindowResize( uint32 unSurfaceWidth, uint32 unSurfaceHeight )
{
	m_unSurfaceWidth = unSurfaceWidth;
	m_unSurfaceHeight = unSurfaceHeight;
}



//-----------------------------------------------------------------------------
// Purpose: Get free render ops vector
//-----------------------------------------------------------------------------
CUtlVector< CUIAnimationEngine::RenderOperation_t *> * CUIAnimationEngine::GetFreeRenderOpsVector() 
{
	if ( m_vecRenderOpsVectorPtrs.Count() )
	{
		CUtlVector<CUIAnimationEngine::RenderOperation_t *> *pVec = m_vecRenderOpsVectorPtrs[m_vecRenderOpsVectorPtrs.Count() - 1];
		m_vecRenderOpsVectorPtrs.FastRemove( m_vecRenderOpsVectorPtrs.Count() - 1 );
		return pVec;
	}
	else
		return new CUtlVector<CUIAnimationEngine::RenderOperation_t *>();
}


//-----------------------------------------------------------------------------
// Purpose: Free render ops vector
//-----------------------------------------------------------------------------
void CUIAnimationEngine::FreeRenderOpsVector( CUtlVector< CUIAnimationEngine::RenderOperation_t *> * pVec )
{
	// Should already be empty...
	Assert( pVec->Count() == 0 );
	pVec->RemoveAll();
	m_vecRenderOpsVectorPtrs.AddToTail( pVec );
}



//-----------------------------------------------------------------------------
// Purpose: Called at the start of each frame, means we should reset state
//-----------------------------------------------------------------------------
void CUIAnimationEngine::BeginFrame( const BeginFrameRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::BeginFrame", VPROF_BUDGETGROUP_TENFOOT );
	Assert( m_vecRenderOperations.Count() == 0 );
	m_pCurrentOperationContext = NULL;
	m_vecRenderOperations.RemoveAll();

	Assert( m_unSkipUntilContextPopCounter == 0 );
	m_unSkipUntilContextPopCounter = 0;

	m_unSurfaceWidth = renderCommand.surface_width;
	m_unSurfaceHeight = renderCommand.surface_height;
	m_flUIScaleFactor = renderCommand.ui_scale_factor;

	FOR_EACH_LL( m_AnimationAndTransformStack, i )
	{
		delete m_AnimationAndTransformStack[i];
	}
	m_AnimationAndTransformStack.RemoveAll();

	m_flCurrentFrameGenerationTime = renderCommand.frame_paint_time;

	// Need to make sure we update this while not trying to stop a property on another thread
	{
		AUTO_LOCK( m_MutexStoppedTransitionData );
		m_flCurrentFrameAnimationTime = Plat_FloatTime();
	}

	// Create root noop render operation for the frame, to contain background drawing for root panels
	RenderOperation_t *pOperation = AllocPerFrameObject< RenderOperation_t >();
	m_vecRenderOperations.AddToTail( pOperation );
	pOperation->m_flAvgZPos = 0;
	pOperation->m_pRenderCommand = NULL;
	pOperation->m_pParent = NULL;
	pOperation->m_pVecChildOperations = GetFreeRenderOpsVector();
	m_pCurrentOperationContext = pOperation;

	renderCommand.PushCommandCopy( outputCommandList );
}


//-----------------------------------------------------------------------------
// Purpose: Sort function
//-----------------------------------------------------------------------------
bool CUIAnimationEngine::RenderOpSort( const RenderOpPtr &a, const RenderOpPtr &b )
{
	if( a->m_flAvgZPos != b->m_flAvgZPos )
		return a->m_flAvgZPos < b->m_flAvgZPos;

	return a->m_flZIndex < b->m_flZIndex;
}


//-----------------------------------------------------------------------------
// Purpose: Mark all child composition layers we know about as needing to free
//-----------------------------------------------------------------------------
void CUIAnimationEngine::FreeChildLayersRecursive( RenderOperation_t *pChild )
{
	if( pChild->m_bFreedChildLayers )
		return;

	pChild->m_bFreedChildLayers = true;

	if( pChild->m_bIsLayerPush )
	{
		FreeAndForceRepaintOfCompositingLayer( pChild->m_ulPanelPtrValue, false );
	}

	if( pChild->m_pVecChildOperations )
	{
		FOR_EACH_VEC( *(pChild->m_pVecChildOperations), i )
		{
			FreeChildLayersRecursive( pChild->m_pVecChildOperations->Element( i ) );
		}
	}

}

struct ParentOcclusionRegion_t
{
	float left;
	float top;
	float right;
	float bottom;
};


//-----------------------------------------------------------------------------
// Purpose: Sort and serialize render operations
//-----------------------------------------------------------------------------
void CUIAnimationEngine::SortAndSerializeRenderOperationsRecursive( RenderOperation_t *pRenderOp, bool bSuppressChildrenDrawing, CRenderCommandList &outputCommandList )
{
	// Check if we passed hit testing and keep track
	if ( pRenderOp->m_bHitTestPassed && pRenderOp->m_ulPanelPtrValue != k_ulInvalidPanelHandle64 && pRenderOp->m_bWantsHitTest )
	{
		m_ulPanelHitTestPtrValue = pRenderOp->m_ulPanelPtrValue;
		m_PanelHitTestCoords = pRenderOp->m_HitTestCoords;
	}
	
	// see if any of the listening panels want this movement
	if (   pRenderOp->m_ulPanelPtrValue != k_ulInvalidPanelHandle64 
		&& pRenderOp->m_bWantsHitTest
		&& m_vecTrackingMousePanels.Count() 
		&& m_vecTrackingMousePanels.Find( pRenderOp->m_ulPanelPtrValue ) != m_vecTrackingMousePanels.InvalidIndex() )
	{
		m_vecTrackingMouseResults.AddToTail( MouseTrackingResults_t( pRenderOp->m_ulPanelPtrValue, pRenderOp->m_HitTestCoords.x, pRenderOp->m_HitTestCoords.y ) );
	}

	bool bPopScreenSpaceQuads = false;
	if ( pRenderOp->m_ulPanelPtrValue != k_ulInvalidPanelHandle64 && pRenderOp->m_pScreenspaceQuad )
	{
		bPopScreenSpaceQuads = true;
		if ( !m_pScreenSpaceQuadOutput )
		{
			m_pLastScreenSpaceQuadAdded = m_pScreenSpaceQuadOutput = new ScreenSpacePanelQuad_t( NULL );
		}
		else if ( !m_pLastScreenSpaceQuadAdded )
		{
			ScreenSpacePanelQuad_t *pNextChild = m_pScreenSpaceQuadOutput;
			m_pLastScreenSpaceQuadAdded = m_pScreenSpaceQuadOutput = new ScreenSpacePanelQuad_t( NULL );
			m_pLastScreenSpaceQuadAdded->m_pNextPeer = pNextChild;
		}
		else if ( !m_pLastScreenSpaceQuadAdded->m_pFirstChild )
		{
			m_pLastScreenSpaceQuadAdded->m_pFirstChild = new ScreenSpacePanelQuad_t( m_pLastScreenSpaceQuadAdded );
			m_pLastScreenSpaceQuadAdded = m_pLastScreenSpaceQuadAdded->m_pFirstChild;
		}
		else if ( m_pLastScreenSpaceQuadAdded->m_pFirstChild )
		{
			ScreenSpacePanelQuad_t *pNextChild = m_pLastScreenSpaceQuadAdded->m_pFirstChild;
			
			m_pLastScreenSpaceQuadAdded = m_pLastScreenSpaceQuadAdded->m_pFirstChild = new ScreenSpacePanelQuad_t( m_pLastScreenSpaceQuadAdded );
			m_pLastScreenSpaceQuadAdded->m_pNextPeer = pNextChild;
		}
		else
		{
			Assert( false );
			m_pLastScreenSpaceQuadAdded = NULL; // crash below
		}

#if DEBUG_SCREENSPACE_QUAD_OUTPUT
		try
		{
			CPanelPtr< CUIPanel > ptr;
			ptr.SetFromUInt64( pRenderOp->m_ulPanelPtrValue );

			m_pLastScreenSpaceQuadAdded->m_strPanel = CFmtStr1024( "Panel %s (%s)", ptr->GetID(), ptr->GetPanelType().String() ).String();
		}
		catch ( ... )
		{
		}
#endif

		m_pLastScreenSpaceQuadAdded->m_ulPanelContextID = pRenderOp->m_ulPanelPtrValue;
		m_pLastScreenSpaceQuadAdded->m_pQuad = pRenderOp->m_pScreenspaceQuad;

		pRenderOp->m_pScreenspaceQuad = NULL;
	}

	if( pRenderOp->m_bDontDrawSelf )
		bSuppressChildrenDrawing = true;

	if( pRenderOp->m_bIsPanelContext && !bSuppressChildrenDrawing )
	{
		UIEngine()->IncrementPaintCountForPanel( pRenderOp->m_ulPanelPtrValue, pRenderOp->m_bIsLayerPush, UIEngine()->GetCurrentFrameTime() );
	}

	// Serialize our own command. This is done by just making a nested command that refers to the original command
	bool bSerialized = false;
	if ( pRenderOp->m_pRenderCommand )
	{
		bool bSerialized = false;
		if ( !bSuppressChildrenDrawing )
		{
			if ( pRenderOp->m_bIsLayerPush )
				m_vecLayersPushedThisFrame.AddToTail( pRenderOp->m_ulPanelPtrValue );

			NestedRenderCommand_t *pNestedCommand = outputCommandList.PushCommand< NestedRenderCommand_t >();
			pNestedCommand->command = pRenderOp->m_pRenderCommand;

			bSerialized = true;
		}

		if ( pRenderOp->m_bIsLayerPop && m_vecLayersPushedThisFrame.Count() && pRenderOp->m_ulPanelPtrValue == m_vecLayersPushedThisFrame[ m_vecLayersPushedThisFrame.Count() - 1 ] )
		{
			if ( !bSerialized )
			{
				NestedRenderCommand_t *pNestedCommand = outputCommandList.PushCommand< NestedRenderCommand_t >();
				pNestedCommand->command = pRenderOp->m_pRenderCommand;
			}

			m_vecLayersPushedThisFrame.Remove( m_vecLayersPushedThisFrame.Count() - 1 );
		}
	}


	// Ensure composition layers (associated with a panel) currently visible on screen don't get evicted from the cache
	// E.g. in the case of multiple child composition layers inside of a parent composition layer - currently child composition layers 
	// would get evicted from the cache if the parent composition layer decides it doesn't need to draw its children. 
	//	The call to PingCompositingLayer will ensure that we are touching the child composition layers in the cache.

	if ( !bSerialized && pRenderOp->m_bIsLayerPush && s_convarPanoramaKeepPanelInLayerCache.GetBool() )
	{
		// Make sure to keep the composition layer in the cache, even if it is not being drawn this frame - likely to be used in subsequent frames.
		m_pRenderEngine->Access3DSurface()->PingCompositingLayer( pRenderOp->m_ulPanelPtrValue, pRenderOp->m_flWidth, pRenderOp->m_flHeight );
	}

	// Similarly for text regions currently visible - do not evict them from the cache

	if ( !bSerialized && pRenderOp->m_pRenderCommand && ( pRenderOp->m_pRenderCommand->eCommandType == k_EDrawTextRegion ) && s_convarPanoramaKeepTextInFontCache.GetBool() )
	{
		m_pRenderEngine->Access3DSurface()->TouchTextRegionInCache( static_cast< RenderTextRegionCommand_t & >( *pRenderOp->m_pRenderCommand ) );
	}


	// Sort, then serialize children
	if ( pRenderOp->m_bSortChildOperations || s_convarPanoramaAnimForceSortChildOps.GetBool() )
	{
		VPROF_BUDGET_THREAD( "CUIAnimationEngine::SortAndSerializeRenderOperationsRecursive - Sort", VPROF_BUDGETGROUP_TENFOOT );
		std::stable_sort( pRenderOp->m_pVecChildOperations->Base(), pRenderOp->m_pVecChildOperations->Base()+pRenderOp->m_pVecChildOperations->Count(), RenderOpSort );
	}

#ifdef PANORAMA_ANIMATION_ENGINE_OCCLUSION
	CUtlVectorFixedGrowable<ParentOcclusionRegion_t, 256> vecParentOcclusionForChildren;
	if( pRenderOp->m_pVecChildOperations )
	{
		vecParentOcclusionForChildren.AddMultipleToTail( pRenderOp->m_pVecChildOperations->Count() );
	}

	// No need to do occlusion work if we already suppressed drawing all the children

	Vector2D vecOccludedParent[2];
	vecOccludedParent[0].Init( 0.0f, 0.0f );
	vecOccludedParent[1].Init( 0.0f, 0.0f );

	if( !bSuppressChildrenDrawing )
	{
		VPROF_BUDGET_THREAD( "CUIAnimationEngine::SortAndSerializeRenderOperationsRecursive - Occlusion", VPROF_BUDGETGROUP_TENFOOT );

		Vector2D vecOccludedRegion[2];
		vecOccludedRegion[0].Init( 0.0f, 0.0f );
		vecOccludedRegion[1].Init( 0.0f, 0.0f );

		if( pRenderOp->m_bIsPanelContext )
		{
			vecOccludedParent[0].x = pRenderOp->m_flOccludedLeft;
			vecOccludedParent[0].y = pRenderOp->m_flOccludedTop;
			vecOccludedParent[1].x = pRenderOp->m_flOccludedRight;
			vecOccludedParent[1].y = pRenderOp->m_flOccludedBottom;

#if 0
			try
			{
				CPanelPtr< CUIPanel > ptr;
				ptr.SetFromUInt64( pRenderOp->m_ulPanelPtrValue );

				Msg( "Parent %s (%s) (%p) is occluded in region: %1.2f,%1.2f %1.2f,%1.2f\n", ptr->GetID(), ptr->GetPanelType().String(), ptr->ClientPtr(), vecOccludedParent[0].x, vecOccludedParent[0].y, vecOccludedParent[1].x, vecOccludedParent[1].y );
			}
			catch( ... )
			{
			}
#endif
		}

		int nOpCount = 0;
		FOR_EACH_VEC_BACK( *(pRenderOp->m_pVecChildOperations), i )
		{
			++nOpCount;
			ParentOcclusionRegion_t &parentOccludedForChild = vecParentOcclusionForChildren[pRenderOp->m_pVecChildOperations->Count() - nOpCount];
			parentOccludedForChild.left = vecOccludedRegion[0].x;
			parentOccludedForChild.top = vecOccludedRegion[0].y;
			parentOccludedForChild.right = vecOccludedRegion[1].x;
			parentOccludedForChild.bottom = vecOccludedRegion[1].y;

			RenderOperation_t *pChild = pRenderOp->m_pVecChildOperations->Element( i );
			if ( pChild->m_bIsPanelContext )
			{
				PushCompositingLayerRenderCommand_t *pCmdOut = nullptr;
				if( pChild->m_bIsLayerPush )
					pCmdOut = static_cast< PushCompositingLayerRenderCommand_t *>( pChild->m_pRenderCommand );

				if( pChild->m_bUntransformed &&
						( 
						( pChild->m_flXOffsetInParentLayer + pChild->m_flWidth <= vecOccludedRegion[1].x && pChild->m_flXOffsetInParentLayer >= vecOccludedRegion[0].x
						&& pChild->m_flYOffsetInParentLayer >= vecOccludedRegion[0].y && pChild->m_flYOffsetInParentLayer + pChild->m_flHeight <= vecOccludedRegion[1].y) 
						||
						( pChild->m_flXOffsetInParentLayer + pChild->m_flWidth <= vecOccludedParent[1].x && pChild->m_flXOffsetInParentLayer >= vecOccludedParent[0].x
						&& pChild->m_flYOffsetInParentLayer >= vecOccludedParent[0].y && pChild->m_flYOffsetInParentLayer + pChild->m_flHeight <= vecOccludedParent[1].y)
						)
				)
				{
					// Since we are fully occluded we don't need to draw at all.
					pChild->m_bDontDrawSelf = true;

					// Since we are not drawing, we do need to mark the closest parent (us or parent above as not up-to-date in cached composition layers)
					RenderOperation_t *pParent = pChild;

					if( pParent && pParent->m_bIsLayerPush )
						FreeAndForceRepaintOfCompositingLayer( pParent->m_ulPanelPtrValue, false );

					pParent = pParent->m_pParent;
					while( pParent && !pParent->m_bIsLayerPush )
						pParent = pParent->m_pParent;

					if( pParent && pParent->m_bIsLayerPush )
						FreeAndForceRepaintOfCompositingLayer( pParent->m_ulPanelPtrValue, false );

					// We also need to mark any child layers that still exist as invalid, since we are also culling all their updating
					FreeChildLayersRecursive( pChild );
#if 0
					try
					{
						CPanelPtr< CUIPanel > ptr;
						ptr.SetFromUInt64( pChild->m_ulPanelPtrValue );

						Msg( "Panel %s (%s) (%p) is fully occluded and won't paint\n", ptr->GetID(), ptr->GetPanelType().String(), ptr->ClientPtr() );
					}
					catch( ... )
					{
					}
#endif
				}

				Vector2D vecChildOccluded[2];
				vecChildOccluded[0] = pChild->m_CoveredAxisAlignedRect[0];
				vecChildOccluded[1] = pChild->m_CoveredAxisAlignedRect[1];

				if ( pChild->m_pParent && pChild->m_pParent->m_bIsPanelContext && !pChild->m_pParent->m_bIsLayerPush )
				{
					for ( int j = 0; j < 2; ++j )
					{
						vecChildOccluded[j].x += pChild->m_pParent->m_flXOffsetInParentLayer;
						vecChildOccluded[j].y += pChild->m_pParent->m_flYOffsetInParentLayer;
					}

				}

				// If existing region is zero area then just replace
				float flAreaExisting = (vecOccludedRegion[1].x - vecOccludedRegion[0].x) * (vecOccludedRegion[1].y - vecOccludedRegion[0].y);

#if 0
				try
				{
					CPanelPtr< CUIPanel > ptr;
					ptr.SetFromUInt64( pChild->m_ulPanelPtrValue );

					Msg( "Panel %s (%s) (%p) is on top of the rest of it's peers\n", ptr->GetID(), ptr->GetPanelType().String(), ptr->ClientPtr() );
				}
				catch ( ... )
				{
				}
#endif

				// Some region was already occluded by a child (childs) above me... put that in my render message
				if ( flAreaExisting > 0.0001f )
				{
					if ( pChild->m_bIsPanelContext && pChild->m_bUntransformed )
					{							
						Vector2D vecIntersectingRect[2];
						if( pCmdOut )
						{
							vecIntersectingRect[0].x = Max( vecOccludedRegion[0].x, (float)pCmdOut->layer_quad_top_left.x );
							vecIntersectingRect[0].y = Max( vecOccludedRegion[0].y, (float)pCmdOut->layer_quad_top_left.y );
							vecIntersectingRect[1].x = Min( vecOccludedRegion[1].x, (float)pCmdOut->layer_quad_bottom_right.x );
							vecIntersectingRect[1].y = Min( vecOccludedRegion[1].y, (float)pCmdOut->layer_quad_bottom_right.y );
						}
						else
						{
							vecIntersectingRect[0].x = Max( vecOccludedRegion[0].x, pChild->m_flXOffsetInParentLayer );
							vecIntersectingRect[0].y = Max( vecOccludedRegion[0].y, pChild->m_flYOffsetInParentLayer );
							vecIntersectingRect[1].x = Min( vecOccludedRegion[1].x, pChild->m_flXOffsetInParentLayer + pChild->m_flWidth );
							vecIntersectingRect[1].y = Min( vecOccludedRegion[1].y, pChild->m_flYOffsetInParentLayer + pChild->m_flHeight );
						}

						if ( vecIntersectingRect[1].x > vecIntersectingRect[0].x && vecIntersectingRect[1].y > vecIntersectingRect[0].y )
						{
							if ( pCmdOut )
							{
								pCmdOut->occluded_left_edge = vecIntersectingRect[ 0 ].x;
								pCmdOut->occluded_top_edge = vecIntersectingRect[ 0 ].y;
								pCmdOut->occluded_right_edge = vecIntersectingRect[ 1 ].x;
								pCmdOut->occluded_bottom_edge = vecIntersectingRect[ 1 ].y;
							}
#if 0
							if ( pChild->m_pRenderMsg  )
							{
								try
								{
									CPanelPtr< CUIPanel > ptr;
									ptr.SetFromUInt64( pChild->m_ulPanelPtrValue );

									Msg( "Panel %s (%s) (%p) is occluded in region: %1.2f,%1.2f %1.2f,%1.2f\n", ptr->GetID(), ptr->GetPanelType().String(), ptr->ClientPtr(), vecIntersectingRect[0].x, vecIntersectingRect[0].y, vecIntersectingRect[1].x, vecIntersectingRect[1].y );
								}
								catch ( ... )
								{

								}
							}

#endif
						}

					}
				}

				if ( flAreaExisting < 0.0001f )
				{
					vecOccludedRegion[0] = vecChildOccluded[0];
					vecOccludedRegion[1] = vecChildOccluded[1];

#if 0
					try
					{
						CPanelPtr< CUIPanel > ptr;
						ptr.SetFromUInt64( pChild->m_ulPanelPtrValue );

						Msg( "Panel %s (%s) (%p) will occlude peers for region: %1.2f,%1.2f %1.2f,%1.2f (offset: %1.2f, %1.2f)\n", ptr->GetID(), ptr->GetPanelType().String(), ptr->ClientPtr(), vecOccludedRegion[0].x, vecOccludedRegion[0].y, vecOccludedRegion[1].x, vecOccludedRegion[1].y, pChild->m_flXOffsetInParentLayer, pChild->m_flYOffsetInParentLayer );
					}
					catch ( ... )
					{

					}
#endif
				}
				else
				{
					// See which has larger area, we'll use it as base, then expand by intersection of the two
					float flAreaNew = (vecChildOccluded[1].x - vecChildOccluded[0].x) * (vecChildOccluded[1].y - vecChildOccluded[0].y);

					Vector2D vecIntersectingRect[2];
					vecIntersectingRect[0].x = Max( vecChildOccluded[0].x, vecOccludedRegion[0].x );
					vecIntersectingRect[0].y = Max( vecChildOccluded[0].y, vecOccludedRegion[0].y );
					vecIntersectingRect[1].x = Min( vecChildOccluded[1].x, vecOccludedRegion[1].x );
					vecIntersectingRect[1].y = Min( vecChildOccluded[1].y, vecOccludedRegion[1].y );

					if ( vecIntersectingRect[1].x > vecIntersectingRect[0].x && vecIntersectingRect[1].y > vecIntersectingRect[0].y )
					{
						// Use new area as base to expand if it was larger
						if ( flAreaNew > flAreaExisting )
						{
							vecOccludedRegion[0] = vecChildOccluded[0];
							vecOccludedRegion[1] = vecChildOccluded[1];
						}

						vecOccludedRegion[0].x = Min( vecOccludedRegion[0].x, vecIntersectingRect[0].x );
						vecOccludedRegion[0].y = Min( vecOccludedRegion[0].y, vecIntersectingRect[0].y );
						vecOccludedRegion[1].x = Max( vecOccludedRegion[1].x, vecIntersectingRect[1].x );
						vecOccludedRegion[1].y = Max( vecOccludedRegion[1].y, vecIntersectingRect[1].y );
					}
					else if ( flAreaNew > flAreaExisting )
					{
						vecOccludedRegion[0] = vecChildOccluded[0];
						vecOccludedRegion[1] = vecChildOccluded[1];
					}
#if 0
					try
					{
						CPanelPtr< CUIPanel > ptr;
						ptr.SetFromUInt64( pChild->m_ulPanelPtrValue );

						Msg( "Expanded for %s (%s) (%p) will occlude peers for region: %1.2f,%1.2f %1.2f,%1.2f (offset: %1.2f, %1.2f)\n", ptr->GetID(), ptr->GetPanelType().String(), ptr->ClientPtr(), vecOccludedRegion[0].x, vecOccludedRegion[0].y, vecOccludedRegion[1].x, vecOccludedRegion[1].y, pChild->m_flXOffsetInParentLayer, pChild->m_flYOffsetInParentLayer );
					}
					catch ( ... )
					{
					}
#endif
				}
			}
		}
	}
#endif  // PANORAMA_ANIMATION_ENGINE_OCCLUSION

	FOR_EACH_VEC( *(pRenderOp->m_pVecChildOperations), i )
	{
		RenderOperation_t *pChild = pRenderOp->m_pVecChildOperations->Element( i );
#ifdef PANORAMA_ANIMATION_ENGINE_OCCLUSION
		if( vecParentOcclusionForChildren.Count() > i && pChild->m_bIsPanelContext )
		{
			ParentOcclusionRegion_t &parentOcclusion = vecParentOcclusionForChildren[i];

			// Decide whether to just use parents occluded region or if we had a larger region from peers we should use
			float flArea = (parentOcclusion.right - parentOcclusion.left) * (parentOcclusion.bottom - parentOcclusion.top);
			float flPriorParentArea = (vecOccludedParent[1].x - vecOccludedParent[0].x) * (vecOccludedParent[1].y - vecOccludedParent[0].y);

			if( pChild->m_bUntransformed )
			{
				if( flPriorParentArea > flArea )
				{
					pChild->m_flOccludedLeft = vecOccludedParent[0].x;
					pChild->m_flOccludedTop = vecOccludedParent[0].y;
					pChild->m_flOccludedRight = vecOccludedParent[1].x;
					pChild->m_flOccludedBottom = vecOccludedParent[1].y;
				}
				else
				{
					pChild->m_flOccludedLeft = parentOcclusion.left;
					pChild->m_flOccludedTop = parentOcclusion.top;
					pChild->m_flOccludedRight = parentOcclusion.right;
					pChild->m_flOccludedBottom = parentOcclusion.bottom;
				}

				// Offset if we are entering a new composition layer where child sizes will be reset relative to it
				if( pChild->m_bIsLayerPush )
				{
					pChild->m_flOccludedLeft -= pChild->m_flXOffsetInParentLayer;
					pChild->m_flOccludedTop -= pChild->m_flYOffsetInParentLayer;
					pChild->m_flOccludedRight -= pChild->m_flXOffsetInParentLayer;
					pChild->m_flOccludedBottom -= pChild->m_flYOffsetInParentLayer;
				}
			}
		}
#endif  // PANORAMA_ANIMATION_ENGINE_OCCLUSION
		// If this is a composition layer push with child drawing
		// disabled we need to make sure that there's cached
		// content for it, otherwise the rendering will end up
		// doing a layer push/pop but no actual rendering, dropping
		// what should be there.
		bool bSuppressChildRecurse = bSuppressChildrenDrawing;
		if ( !bSuppressChildRecurse && pRenderOp->m_bDontDrawChildren )
		{
			if ( pRenderOp->m_bIsLayerPush )
			{
				bSuppressChildRecurse = m_pRenderEngine->Access3DSurface()->PingCompositingLayer( pRenderOp->m_ulPanelPtrValue, pRenderOp->m_flWidth, pRenderOp->m_flHeight );
				if ( !bSuppressChildRecurse )
				{
					// There's no layer cache entry so turn drawing back on.
					pRenderOp->m_bDontDrawChildren = false;
					PushCompositingLayerRenderCommand_t *pLayerCmd = static_cast< PushCompositingLayerRenderCommand_t *>( pRenderOp->m_pRenderCommand );
					pLayerCmd->needs_clear = true;
				}
			}
			else
			{
				bSuppressChildRecurse = true;
			}
		}

		SortAndSerializeRenderOperationsRecursive( pChild, bSuppressChildRecurse, outputCommandList );
		FreePerFrameObject( pRenderOp->m_pVecChildOperations->Element( i ) );
	}

	if ( bPopScreenSpaceQuads )
		m_pLastScreenSpaceQuadAdded = m_pLastScreenSpaceQuadAdded->m_pParent;

	pRenderOp->m_pVecChildOperations->RemoveAll();
	FreeRenderOpsVector( pRenderOp->m_pVecChildOperations );
}


//-----------------------------------------------------------------------------
// Purpose: Called when there is a particle system no longer needed
//-----------------------------------------------------------------------------
void CUIAnimationEngine::DeleteParticleSystem( const DeleteParticleSystemRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList )
{
	AnimationParticleSystemKey_t key;
	key.ulPanelHandle = renderCommand.panel_handle;
	key.unBrushIndex = renderCommand.brush_index;

	int iMap = m_mapParticleSystems.Find( key );
	if ( iMap != m_mapParticleSystems.InvalidIndex() )
	{
		delete m_mapParticleSystems[iMap];
		m_mapParticleSystems.RemoveAt( iMap );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handle deleting a panel
//-----------------------------------------------------------------------------
void CUIAnimationEngine::DeletePanel( const DeletePanelRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList )
{
	m_mapPanelsToCompositionLayerNeeded.Remove( renderCommand.context_id );
	m_treePanelsThatNeededLayersThisFrame.Remove( renderCommand.context_id );
}


//-----------------------------------------------------------------------------
// Purpose: Dispose of old entries in the stopped transitions map.
//-----------------------------------------------------------------------------
void CUIAnimationEngine::CleanupStoppedTransitions()
{
	AUTO_LOCK( m_MutexStoppedTransitionData );

#if defined(CLEAR_ALL_STOPPED_TRANSITIONS)
	m_mapStoppedTransitions.PurgeAndDeleteElements();
#else
	FOR_EACH_MAP_FAST( m_mapStoppedTransitions, iMap )
	{
		CUtlVector< StoppedTransitionProperty_t > *pVec = m_mapStoppedTransitions[iMap];
		FOR_EACH_VEC_BACK( (*pVec), iVec )
		{
			if ( pVec->Element( iVec ).flLayoutFrameTime < m_flCurrentFrameGenerationTime )
			{
				// Msg( "Removing %llx, %u %u at %u\n", m_mapStoppedTransitions.Key(iMap), pVec->Element(iVec).unStyleSymbol, pVec->Element(iVec).nFrame, m_nFrameCounter );
				pVec->Remove( iVec );
			}
		}
		if ( pVec->Count() == 0 )
		{
			m_mapStoppedTransitions.RemoveAt( iMap );
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Tells animation thread to stop further interpolating property and get back the 
// time we stopped it at for layout thread to finish transition in matching manner.
//-----------------------------------------------------------------------------
float CUIAnimationEngine::StopAnimationOfPropertyUntilFrameUpdateAndGetStopTime( uint64 ulPanelContextID, uint32 hStyleSymbol )
{
	DbgAssert( hStyleSymbol != STYLE_SYMBOL_INVALID );
	AUTO_LOCK( m_MutexStoppedTransitionData );

	int iMap = m_mapStoppedTransitions.Find( ulPanelContextID );
	if ( iMap == m_mapStoppedTransitions.InvalidIndex() )
	{
		iMap = m_mapStoppedTransitions.Insert( ulPanelContextID, new CUtlVector< StoppedTransitionProperty_t > );
	}

	CUtlVector< StoppedTransitionProperty_t > *pVec = m_mapStoppedTransitions[iMap];
	FOR_EACH_VEC( (*pVec), iVec )
	{
		if ( pVec->Element( iVec ).unStyleSymbol == hStyleSymbol )
		{
			return pVec->Element( iVec ).flTransitionStopTime;
		}
	}

	StoppedTransitionProperty_t &prop = pVec->Element( pVec->AddToTail() );
	prop.flLayoutFrameTime = UIEngine()->GetCurrentFrameTime();
	prop.unStyleSymbol = hStyleSymbol;
	prop.flTransitionStopTime = m_flCurrentFrameAnimationTime;

	// Msg( "Stop %llx, %u at %u\n", ulPanelContextID, hStyleSymbol, prop.nFrame );

	return prop.flTransitionStopTime;
}


//-----------------------------------------------------------------------------
// Purpose: Debug code for screenspace quad output 
//-----------------------------------------------------------------------------
void OutputScreenSpaceQuadInfoNew( ScreenSpacePanelQuad_t *pPanelQuad, int cDepth, bool bTraversePeers )
{
#if DEBUG_SCREENSPACE_QUAD_OUTPUT
	if ( !pPanelQuad )
		return;

	if ( cDepth == 0 && bTraversePeers )
		Msg( "---------------------------------------------------------\n" );


	CUtlString strPanel( "- (unknown)" );
	try
	{
		CPanelPtr< CUIPanel > ptr;
		ptr.SetFromUInt64( pPanelQuad->m_ulPanelContextID );

		strPanel = CFmtStr1024( "%s (%s)", ptr->GetID(), ptr->GetPanelType().String() ).String();
	}
	catch ( ... )
	{

	}

	Msg( "%*s %s %1.2f,%1.2f %1.2f,%1.2f, %1.2f,%1.2f, %1.2f,%1.2f\n",
		cDepth, "", strPanel.String(),
		pPanelQuad->m_pQuad[0].x, pPanelQuad->m_pQuad[0].y,
		pPanelQuad->m_pQuad[1].x, pPanelQuad->m_pQuad[1].y,
		pPanelQuad->m_pQuad[2].x, pPanelQuad->m_pQuad[2].y,
		pPanelQuad->m_pQuad[3].x, pPanelQuad->m_pQuad[3].y );

	if ( pPanelQuad->m_pFirstChild )
	{
		OutputScreenSpaceQuadInfoNew( pPanelQuad->m_pFirstChild, cDepth + 3, true );
	}

	if ( bTraversePeers )
	{
		ScreenSpacePanelQuad_t *pPeer = pPanelQuad->m_pNextPeer;
		while ( pPeer )
		{
			OutputScreenSpaceQuadInfoNew( pPeer, cDepth, false );
			pPeer = pPeer->m_pNextPeer;
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Called at the end of each frame, means we should reset state
//-----------------------------------------------------------------------------
void CUIAnimationEngine::EndFrame( const EndFrameRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::EndFrame - Sort & Serialize", VPROF_BUDGETGROUP_TENFOOT );
	m_ulPanelHitTestPtrValue = k_ulInvalidPanelHandle64;

	Assert( m_vecLayersPushedThisFrame.Count() == 0 );
	m_vecLayersPushedThisFrame.RemoveAll();

	m_pLastScreenSpaceQuadAdded = NULL;
	
	if ( m_pLastScreenSpaceQuadAdded )
		delete m_pScreenSpaceQuadOutput;
	
	m_pScreenSpaceQuadOutput = NULL;

	// Clear last frames mouse results
	m_vecTrackingMouseResults.RemoveAll();

	{
		VPROF_BUDGET_THREAD( "CUIAnimationEngine::EndFrame - Sort", VPROF_BUDGETGROUP_TENFOOT );
		std::stable_sort( m_vecRenderOperations.Base(), m_vecRenderOperations.Base()+m_vecRenderOperations.Count(), RenderOpSort );
	}

	FOR_EACH_VEC( m_vecRenderOperations, i )
	{
		RenderOperation_t *pRenderOp = m_vecRenderOperations[i]; 
		SortAndSerializeRenderOperationsRecursive( pRenderOp, false, outputCommandList );
		FreePerFrameObject( pRenderOp );
	}
	m_vecRenderOperations.RemoveAll();
	m_pCurrentOperationContext = NULL;

	FOR_EACH_VEC( m_vecLayersToFree, i )
	{
		FreeCompositingLayerRenderCommand_t *pCommand = outputCommandList.PushCommand< FreeCompositingLayerRenderCommand_t >();
		pCommand->layer_id = m_vecLayersToFree[ i ];
	}
	m_vecLayersToFree.RemoveAll();

	m_flFinishedFrameGenerationTime = m_flCurrentFrameGenerationTime;
	
	renderCommand.PushCommandCopy( outputCommandList );

	OutputScreenSpaceQuadInfoNew( m_pScreenSpaceQuadOutput, 0, true );

	// Hand off the screenspace quad coordinates to render engine
	m_pRenderEngine->UpdatePanelScreenspaceQuadCoordinates( m_pScreenSpaceQuadOutput );
	m_pScreenSpaceQuadOutput = NULL;

	// Run particle systems so they are updated for next frame
	FOR_EACH_MAP_FAST( m_mapParticleSystems, i )
	{
		m_mapParticleSystems[i]->RunSystem( m_flCurrentFrameAnimationTime );
	}

	m_frameScratchMemory.FreeAll();

	m_FrameTimer.End();
	m_nLastFrameMillisecondsIndex++;
	if ( m_nLastFrameMillisecondsIndex >= V_ARRAYSIZE( m_rgflMillisecondsFrame ) )
	{
		for ( int i=0; i < V_ARRAYSIZE( m_rgflMillisecondsFrame ); ++i )
		{
			if ( m_rgflMillisecondsFrame[i] < 1000.0f && ( m_nFramesAnimated == 0 || Plat_IsInDebugSession() )  ) // ignore 1 second or longer frames, chances you were in the debugger or doing startup. If they really are running this slow all the time then the avg FPS measurement will tell us.
				m_flAnimationFrameTime +=  m_rgflMillisecondsFrame[i];
		}

		m_nFramesAnimated += m_nLastFrameMillisecondsIndex;
		m_nLastFrameMillisecondsIndex = 0;
	}

	m_rgflMillisecondsFrame[ m_nLastFrameMillisecondsIndex ] = m_FrameTimer.GetDuration().GetMillisecondsF();
	m_FrameTimer.Start();
}


//-----------------------------------------------------------------------------
// Purpose: Gets framerate average for the last few frames
//-----------------------------------------------------------------------------
float CUIAnimationEngine::GetFPSAverage()
{
	double flSum = 0.0f;
	int nDivisor = 0;
	for ( int i=0; i < V_ARRAYSIZE( m_rgflMillisecondsFrame ); ++i )
	{
		if ( m_rgflMillisecondsFrame[i] != FLT_MAX )
		{
			++nDivisor;
			flSum += m_rgflMillisecondsFrame[i];
		}
	}

	return 1000.0f / ((float)((double)flSum / (double)nDivisor));
}


//-----------------------------------------------------------------------------
// Purpose: Gets framerate average since creation 
//-----------------------------------------------------------------------------
float CUIAnimationEngine::GetSessionFPSAverages()
{
	return (m_nFramesAnimated*1000.0f)/m_flAnimationFrameTime;
}

//-----------------------------------------------------------------------------
// Purpose: Helper to compute transformed quad for current context, this is not offset by parent
// positions
//-----------------------------------------------------------------------------
void CUIAnimationEngine::TransformQuadForContext( Vector4D *pCorners, int iContextIndex )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::TransformedQuadForContext", VPROF_BUDGETGROUP_TENFOOT );


	//Msg( "Panel %f,%f, %f,%f, %f,%f, %f,%f\n", pCorners[0].x, pCorners[0].y, pCorners[1].x, pCorners[1].y, pCorners[2].x, pCorners[2].y, pCorners[3].x, pCorners[3].y );

	CAnimationAndTransformContext *pParent = NULL;
	CAnimationAndTransformContext *pContextStack = m_AnimationAndTransformStack[iContextIndex];

	int iPrev = m_AnimationAndTransformStack.Previous(iContextIndex);
	if ( iPrev != m_AnimationAndTransformStack.InvalidIndex() )
		pParent = m_AnimationAndTransformStack[iPrev];

	// Apply pre-transform 2d scaling right away before parent positioning or any other work that applies
	// within parent coordinate space like the real 3d transform
	float flScale2DX, flScale2DY;
	pContextStack->GetScale2DFactors( flScale2DX, flScale2DY );
	if ( flScale2DX != 1.0f || flScale2DY != 1.0f )
	{
		float flCurWidth, flCurHeight;
		pContextStack->GetSize( flCurWidth, flCurHeight );

		if ( flCurWidth > 0.01f && flCurHeight > 0.01f )
		{
			float flXCenter = flCurWidth/2.0f;
			float flYCenter = flCurHeight/2.0f;

			float flXOffset = ((flScale2DX*flCurWidth)-flCurWidth)/2.0f;
			float flYOffset = ((flScale2DY*flCurHeight)-flCurHeight)/2.0f;

			// Calculate distance from center of each corner, then lerp for scaling
			for( int iCorner=0; iCorner < 4; ++iCorner )
			{
				float xNormalized = ( pCorners[iCorner].x / ( flXCenter ) ) - 1.0f;
				float yNormalized = ( pCorners[iCorner].y / ( flYCenter ) ) - 1.0f;

				pCorners[iCorner].x += flXOffset * xNormalized;
				pCorners[iCorner].y += flYOffset * yNormalized;

				Assert( pCorners[iCorner].IsValid() );
			}
		}
	}

	// Apply pre-transform-rotate2d
	float flRotate2d;
	pContextStack->GetRotate2D( flRotate2d );
	if ( flRotate2d > 0.000001f || flRotate2d < -0.000001f )
	{
		float flCurWidth, flCurHeight;
		pContextStack->GetSize( flCurWidth, flCurHeight );
		if ( flCurWidth > 0.01f && flCurHeight > 0.01f )
		{
			float flSine;
			float flCosine;
			float flRadians = DEG2RAD( flRotate2d );
			SinCos( flRadians, &flSine, &flCosine );

			float flXTranslate = pCorners[0].x + ((pCorners[1].x - pCorners[0].x )/2.0f);
			float flYTranslate = pCorners[0].y + ((pCorners[3].y - pCorners[0].y )/2.0f);

			for( int iCorner=0; iCorner < 4; ++iCorner )
			{
				float x = ( pCorners[iCorner].x - flXTranslate );
				float y = ( pCorners[iCorner].y - flYTranslate );

				pCorners[iCorner].x  = x * flCosine - y * flSine;
				pCorners[iCorner].y = y * flCosine + x * flSine;

				pCorners[iCorner].x += flXTranslate;
				pCorners[iCorner].y += flYTranslate;

				Assert( pCorners[iCorner].IsValid() );
			}
		}
	}
	//Msg( "Panel after scaling %f,%f, %f,%f, %f,%f, %f,%f\n", pCorners[0].x, pCorners[0].y, pCorners[1].x, pCorners[1].y, pCorners[2].x, pCorners[2].y, pCorners[3].x, pCorners[3].y );

	float flParentWidth = (float)m_unSurfaceWidth;
	float flParentHeight = (float)m_unSurfaceHeight;
	float flParentPerspective = 1000.0f * m_flUIScaleFactor;
	float flParentPerspectiveX = 0.0f;
	float flParentPerspectiveY = 0.0f;

	float px = 0.0f;
	float py = 0.0f;
	float pz = 0.0f;
	pContextStack->GetPosition( px, py, pz );

	if ( pParent )
	{
		// Compute perspective based on parents settings
		pParent->GetSize( flParentWidth, flParentHeight );
		flParentPerspective = pParent->GetPerspective();
		flParentPerspectiveX = pParent->GetPerspectiveOriginX();
		flParentPerspectiveY = pParent->GetPerspectiveOriginY();

		// Add parents scroll in for hit testing 
		float xScroll, yScroll;
		pParent->GetContentsScroll( xScroll, yScroll );
		px += xScroll;
		py += yScroll;
	}
	for( int iCorner=0; iCorner < 4; ++iCorner )
	{
		pCorners[iCorner].x += px;
		pCorners[iCorner].y += py;
		pCorners[iCorner].z = pz;
		pCorners[iCorner].w = 1.0f;
		Assert( pCorners[iCorner].IsValid() );
	}

	//Msg( "Panel after parent offset %f,%f, %f,%f, %f,%f, %f,%f\n", pCorners[0].x, pCorners[0].y, pCorners[1].x, pCorners[1].y, pCorners[2].x, pCorners[2].y, pCorners[3].x, pCorners[3].y );

	// Note: Need to do this even if the matrix is the identify matrix, because we'll still
	// have to apply perspective in case of z position!  BHasCompositionLayer really is the right thing.
	ALIGN128 VMatrix mat = pContextStack->GetTransformMatrix();
	if ( pContextStack->BHasCompositionLayer() || !mat.IsIdentity() )
	{
		float xTrans = pContextStack->GetTransformOriginXUnoffset();
		float yTrans = pContextStack->GetTransformOriginYUnoffset();

		mat = SetupMatrixTranslation( Vector( xTrans, yTrans, 0 ) ) * mat * SetupMatrixTranslation( Vector( -xTrans, -yTrans, 0 ) );

		VMatrix matPerspective = SetupMatrixTranslation( Vector( flParentPerspectiveX, flParentPerspectiveY, 0 ) );

		VMatrix matTemp = VMatrix::GetIdentityMatrix();
		matTemp.SetElement( 2, 3, -1.0f/flParentPerspective );
		matPerspective = matTemp * matPerspective;
		matPerspective = SetupMatrixTranslation( Vector( -flParentPerspectiveX, -flParentPerspectiveY, 0 ) ) * matPerspective;

		mat = matPerspective * mat;

#ifdef PANORAMA_USE_S1WRAPPER

		// Old non simd code for now
		// TODO Add missing SIMD functions

		for ( int iCorner = 0; iCorner < 4; iCorner++ )
		{
			Vector4DMultiply( mat, pCorners[iCorner], pCorners[iCorner] );

			pCorners[iCorner].x = pCorners[iCorner].x / ( flParentWidth / 2.0f ) - 1;
			pCorners[iCorner].y = -( pCorners[iCorner].y / ( flParentHeight / 2.0f ) ) + 1;

			// Do the perspective divide by w
			if ( fabs( pCorners[iCorner].w ) > 0.000001f )
			{
				pCorners[iCorner].x /= pCorners[iCorner].w;
				pCorners[iCorner].y /= pCorners[iCorner].w;
			}

			pCorners[iCorner].x = ( pCorners[iCorner].x + 1.0f ) / 2.0f * flParentWidth;
			pCorners[iCorner].y = -( ( pCorners[iCorner].y + 1.0f ) / 2.0f * flParentHeight ) + flParentHeight;
			pCorners[iCorner].z = 0.0f;
			Assert( pCorners[iCorner].IsValid() );
		}

#else
		ALIGN128 Vector4D scalar( 1.0f / ( flParentWidth / 2.0f ), -1.0f / ( flParentHeight / 2.0f ), 1.0f, 1.0f );
		ALIGN128 Vector4D adder( -1.0, 1.0f, 0.0f, 0.0f );
		ALIGN128 Vector4D widthHeightScale( 0.5f * flParentWidth, -0.5f * flParentHeight, 1.0f, 1.0f );
		ALIGN128 Vector4D oneOneZeroOne( 1.0f, 1.0f, 0.0f, 1.0f );
		ALIGN128 Vector4D yParentHeight( 0.0f, flParentHeight, 0.0f, 0.0f );
		ALIGN128 Vector4D xyOne( 1.0f, 1.0f, 0.0f, 0.0f );

		const fltx4 adderSimd = LoadAlignedSIMD( &adder.x );
		const fltx4 scalarSimd = LoadAlignedSIMD( &scalar.x );
		const fltx4 widthHeightScalar = LoadAlignedSIMD( &widthHeightScale.x );
		const fltx4 oneSimd = ReplicateX4( 1.0f );
		const fltx4 absmask = CastToFltx4( ReplicateIX4( ~( 1 << 31 ) ) );
		const fltx4 nearZero = ReplicateX4( 0.00001f );
		const fltx4 vec1101 = LoadAlignedSIMD( &oneOneZeroOne.x );
		const fltx4 vecYParentHeight = LoadAlignedSIMD( &yParentHeight.x );
		const fltx4 oneXY = LoadAlignedSIMD(&xyOne.x);
		const fltx4 row1 = LoadAlignedSIMD( &mat[0][0] );
		const fltx4 row2 = LoadAlignedSIMD( &mat[1][0] );
		const fltx4 row4 = LoadAlignedSIMD( &mat[3][0] );

		for ( int iCorner = 0; iCorner < 4; iCorner++ )
		{
			const fltx4 vector = LoadUnalignedSIMD( &pCorners[iCorner].x );

			const fltx4 xSimd = Dot4SIMD( row1, vector );
			const fltx4 ySimd = Dot4SIMD( row2, vector );
			const fltx4 wSimd = Dot4SIMD( row4, vector );

			const fltx4 xyxySimd = InterleaveSIMDXY( xSimd, ySimd );
			const fltx4 xywwSimd = ShuffleSIMD<MM_SHUFFLE_REV( 0, 1, 0, 0 )>( xyxySimd, wSimd );

			const fltx4 scaled = MulSIMD( xywwSimd, scalarSimd );
			const fltx4 offset = AddSIMD( scaled, adderSimd );

			const fltx4 wAbs = AndSIMD( wSimd, absmask );  //wAbs = abs( w );
			const fltx4 wSimdMask = CmpGtSIMD( wAbs, nearZero );
			const fltx4 wReciprocalSimd = ReciprocalSIMD( wSimd );
			const fltx4 bitMaskedW = AndSIMD( wReciprocalSimd, wSimdMask );
			const fltx4 bitMaskedOne = AndNotSIMD( oneSimd, wSimdMask );
			const fltx4 adjustedDivisor = OrSIMD( bitMaskedW, bitMaskedOne ); //1/w or 1 depending on clamp

			const fltx4 normalizerSimd = ShuffleSIMD<MM_SHUFFLE_REV( 1, 0, 2, 3 )>( wReciprocalSimd, vec1101 );
			const fltx4 wScaledOffset = MulSIMD( offset, normalizerSimd ); //wScaledOffset = [x / w, y / w, 0.0, w]

			fltx4 tmp = AddSIMD( wScaledOffset, oneXY );
			tmp = MulSIMD( tmp, widthHeightScalar );
			tmp = AddSIMD( tmp, vecYParentHeight );

// Old non simd code + validation
// 			Vector4DMultiply( mat, pCorners[iCorner], pCorners[iCorner] );
// 			Vector4D tmp1 = pCorners[iCorner];
// 			tmp1.x = pCorners[iCorner].x / ( flParentWidth / 2.0f ) - 1;
// 			tmp1.y = -( pCorners[iCorner].y / ( flParentHeight / 2.0f ) ) + 1;
// 
// 			Vector4D tmp2 = tmp1;
// 			// Do the perspective divide by w
// 			if ( tmp2.w > 0.000001f || tmp2.w < -0.000001f )
// 			{
// 				tmp2.x /= tmp2.w;
// 				tmp2.y /= tmp2.w;
// 			}
// 
// 			Vector4D tmp3 = tmp2;
// 
// 			tmp3.x = ( tmp2.x + 1.0f ) * 0.5f * flParentWidth;
// 			tmp3.y = -( ( tmp2.y + 1.0f ) * 0.5f * flParentHeight ) + flParentHeight;
// 
// 			Vector4D tmpV4;
// 			StoreUnalignedSIMD( &tmpV4.x, tmp );
// 
// 			Assert( fabsf( tmp3.x - tmpV4.x ) < 0.01f );
// 			Assert( fabsf( tmp3.y - tmpV4.y ) < 0.01f );
// 			Assert( fabsf( tmp3.z - tmpV4.z ) < 0.01f );
// 			Assert( fabsf( tmp3.w - tmpV4.w ) < 0.01f );


			StoreUnalignedSIMD( &pCorners[iCorner].x, tmp );

			Assert( pCorners[iCorner].IsValid() );

		}
#endif	// PANORAMA_USE_S1WRAPPER
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper to check for intersection of two line segments, not that this
// assumes the lines are in 2D space even though it takes 3D vectors, would need
// to pass rather than compute the normal (v1Perpendicular) otherwise.
//-----------------------------------------------------------------------------
bool LineSegmentsIntersectNew( const Vector &v1a, const Vector &v1b, const Vector &v2a, const Vector &v2b, float flEpsilon, float *pTimeOfIntersection1, float *pTimeOfIntersection2 )
{
	Vector vDir1 = v1b - v1a;
	Vector vDir2 = v2b - v2a;
	Vector v1Perpendicular = Vector( vDir1.y, -vDir1.x, 0.0f );
	Vector vStartDiff = v1a - v2a;
	float flNumerator = vStartDiff.Dot( v1Perpendicular );
	float flDenominator = vDir2.Dot( v1Perpendicular );

	if ( fabsf( flDenominator ) < flEpsilon )
	{
		return false;
	}

	float t2 = flNumerator / flDenominator;
	if ( t2 >= -flEpsilon && t2 <= ( 1.0f + flEpsilon ) )
	{
		Vector vClosestPoint2 = t2 * vDir2 + v2a;
		float flLength1 = vDir1.NormalizeInPlace();
		
		// Can't be 0 otherwise flDenominator would have been 0
		float t1 = ( vClosestPoint2 - v1a ).Dot( vDir1 ) / flLength1;

		if ( t1 >= -flEpsilon && t1 <= ( 1.0f + flEpsilon ) )
		{
			*pTimeOfIntersection1 = t1;
			*pTimeOfIntersection2 = t2;
			return true;
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called to determine if current animation and transform context is fully clipped by parent
//-----------------------------------------------------------------------------
bool CUIAnimationEngine::BIsCurrentContextFullyClipped( Vector4D *pCorners )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::BIsCurrentContextFullyClipped", VPROF_BUDGETGROUP_TENFOOT );

	CAnimationAndTransformContext *pParent = NULL;

	int iCur = m_AnimationAndTransformStack.Tail();
	if ( iCur != m_AnimationAndTransformStack.InvalidIndex() )
	{
		int iPrev = m_AnimationAndTransformStack.Previous( iCur );
		if ( iPrev != m_AnimationAndTransformStack.InvalidIndex() )
			pParent = m_AnimationAndTransformStack[iPrev];
	}

	float flWide, flTall;
	if ( !pParent )
	{
		flWide = m_unSurfaceWidth;
		flTall = m_unSurfaceHeight;
	}
	else
	{
		pParent->GetSize( flWide, flTall );
	}

	Vector4D parentCorners[4];
	parentCorners[0].Init( 0, 0 );
	parentCorners[1].Init( flWide, 0 );
	parentCorners[2].Init( flWide, flTall );
	parentCorners[3].Init( 0, flTall );

	// Is the input quad axis aligned and ? Do check then.
	if ( pCorners[0].x == pCorners[3].x
		&& pCorners[0].y == pCorners[1].y
		&& pCorners[1].x == pCorners[2].x
		&& pCorners[2].y == pCorners[3].y )
	{
		// Note: Min/Max below needed because although the quad is axis aligned it could have a scaley( -1.0 ) or such,
		// Dota does this a bit...

		// Fully below parent
		if ( Min( pCorners[0].y, pCorners[2].y ) >= parentCorners[2].y )
			return true;

		// Fully to right of parent
		if ( Min( pCorners[0].x, pCorners[2].x ) >= parentCorners[2].x )
			return true;

		// Fully above parent
		if ( Max( pCorners[0].y, pCorners[2].y ) <= parentCorners[0].y )
			return true;

		// Fully to left of parent
		if ( Max( pCorners[0].x, pCorners[2].x ) <= parentCorners[0].x )
			return true;
			
		return false;
	}



	// If one polygon contains the corner of another we can be done in this fast path
	for( int i=0; i < 1; i++ )
	{
		if ( BPointInsideConvexQuad( pCorners[i].x, pCorners[i].y, parentCorners) )
			return false;

		if ( BPointInsideConvexQuad( parentCorners[i].x, parentCorners[i].y, pCorners ) )
			return false;
	}

	static const float LINE_INTERSECT_EPSILON = 1e-3f;

	float flTimeOfInt;
	float flTimeOfOtherInt;

	Vector pt1( 0.0f, 0.0f, 0.0f );
	Vector pt2( 0.0f, 0.0f, 0.0f );
	Vector pt3( 0.0f, 0.0f, 0.0f );
	Vector pt4( 0.0f, 0.0f, 0.0f );

	// Now we have to actually check if line segments intersect
	for ( int i=0, iPrev=3; i < 4; iPrev = i, ++i )
	{
		for ( int j=0, jPrev=3; j< 4; jPrev = j, ++j )
		{
			pt1.x = pCorners[iPrev].x; 
			pt1.y = pCorners[iPrev].y;
			pt2.x = pCorners[i].x;
			pt2.y = pCorners[i].y;
			pt3.x = parentCorners[jPrev].x;
			pt3.y = parentCorners[jPrev].y;
			pt4.x = parentCorners[j].x;
			pt4.y = parentCorners[j].y;

			if ( LineSegmentsIntersectNew( pt1, pt2, pt3, pt4, 0.0000001f, &flTimeOfInt, &flTimeOfOtherInt ) )
			{
				if ( flTimeOfInt >= -LINE_INTERSECT_EPSILON &&  flTimeOfInt <= 1.0f + LINE_INTERSECT_EPSILON && flTimeOfOtherInt >= -LINE_INTERSECT_EPSILON && flTimeOfOtherInt <= 1.0f + LINE_INTERSECT_EPSILON )
				{
					//Msg( "Line: (%1.2f,%1.2f) -> (%1.2f,%1.2f) intersects (%1.2f,%1.2f) -> (%1.2f,%1.2f) -- %1.2f %1.2f", p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, p4.x, p4.y, flTimeOfInt, flTimeOfOtherInt );
					return false;
				}
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called to hit test current animation and transform context
//-----------------------------------------------------------------------------
bool CUIAnimationEngine::BHitTestCurrentContext( Vector4D *pCorners, bool bForceMouseCoordinateCalculation, float &flMouseRelativeX, float &flMouseRelativeY, Vector2D *pScreenspaceQuadOut ) 
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::BHitTestCurrentContext", VPROF_BUDGETGROUP_TENFOOT );
	if ( m_AnimationAndTransformStack.Count() < 1 )
		return false;

	Vector4D cornersLocal[4];
	cornersLocal[0].Init( pCorners[0].x, pCorners[0].y, pCorners[0].z, pCorners[0].w );
	cornersLocal[1].Init( pCorners[1].x, pCorners[1].y, pCorners[1].z, pCorners[1].w );
	cornersLocal[2].Init( pCorners[2].x, pCorners[2].y, pCorners[2].z, pCorners[2].w );
	cornersLocal[3].Init( pCorners[3].x, pCorners[3].y, pCorners[3].z, pCorners[3].w );

	bool bSkip = true;
	FOR_EACH_LL_BACK( m_AnimationAndTransformStack, i )
	{
		if ( !bSkip )
		{
			TransformQuadForContext( cornersLocal, i );
		}
		bSkip = false;
	}

	if ( pScreenspaceQuadOut )
	{
		for ( int i = 0; i < 4; ++i )
		{
			pScreenspaceQuadOut[i].x = cornersLocal[i].x;
			pScreenspaceQuadOut[i].y = cornersLocal[i].y;
		}
	}

	// make a quick bounding box and see if specified point is inside the box
	float flMinX = cornersLocal[0].x;
	float flMinY = cornersLocal[0].y;
	float flMaxX = cornersLocal[0].x;
	float flMaxY = cornersLocal[0].y;

	for ( int i = 1; i < 4; i++ )
	{
		if ( cornersLocal[i].x < flMinX )
			flMinX = cornersLocal[i].x;
		if ( cornersLocal[i].x > flMaxX )
			flMaxX = cornersLocal[i].x;

		if ( cornersLocal[i].y < flMinY )
			flMinY = cornersLocal[i].y;
		if ( cornersLocal[i].y > flMaxY )
			flMaxY = cornersLocal[i].y;
	}

	bool bHitTestPassed = true;
	if ( m_flMouseX < flMinX || m_flMouseX > flMaxX )
		bHitTestPassed = false;

	if ( bHitTestPassed )
	{
		if ( m_flMouseY < flMinY || m_flMouseY > flMaxY )
			bHitTestPassed = false;
	}

	if ( bHitTestPassed )
	{
		bHitTestPassed = BPointInsideConvexQuad( m_flMouseX, m_flMouseY, cornersLocal );
	}

	if ( bHitTestPassed || bForceMouseCoordinateCalculation )
	{
		// Transform screen space mouse coordinates into panel relative coordinates.  We do this by using our
		// quad warper class to compute a transform matrix to transform any point relative to the screen space
		// panel coordinates to the layout space coordinates.
		// 
		// Also, to avoid the cost, we'll detect axis aligned quads and do them in a fast path
		if ( cornersLocal[0].x == cornersLocal[3].x && cornersLocal[1].x == cornersLocal[2].x
			&& cornersLocal[0].y == cornersLocal[1].y && cornersLocal[2].y == cornersLocal[3].y )
		{
			flMouseRelativeX = m_flMouseX - cornersLocal[0].x;
			flMouseRelativeY = m_flMouseY - cornersLocal[0].y;
		}
		else
		{
			VPROF_BUDGET_THREAD( "CUIAnimationEngine::BHitTestCurrentContext - CQuad2DWarper", VPROF_BUDGETGROUP_TENFOOT );
			float width, height;
			m_AnimationAndTransformStack[m_AnimationAndTransformStack.Tail()]->GetSize( width, height );

			CQuad2DWarper quadWarp( 
				cornersLocal[0].AsVector2D(), cornersLocal[1].AsVector2D(), cornersLocal[2].AsVector2D(), cornersLocal[3].AsVector2D(),
				Vector2D( 0.0f, 0.0f ), Vector2D( width, 0.0f ), Vector2D( width, height ), Vector2D( 0.0f, height) );

			Vector2D mouseTransformed = quadWarp.TransformPoint( Vector2D( m_flMouseX, m_flMouseY ) );
			flMouseRelativeX = mouseTransformed.x;
			flMouseRelativeY = mouseTransformed.y;
		}
	}

	return bHitTestPassed;
}


//-----------------------------------------------------------------------------
// Purpose: Helper to push all msg data into context, transition lerps happen at this time
//-----------------------------------------------------------------------------
void CUIAnimationEngine::PushDataAndStylesIntoContext( CAnimationAndTransformContext  *pContext, const PushAAndTContextRenderCommand_t &renderCommand, EAnimatingFlags *pAnimatingFlags )
{
	m_pVecStoppedTransitionsForCurrentPanel = NULL;
	int iMap = m_mapStoppedTransitions.Find( renderCommand.context_id );
	if ( iMap != m_mapStoppedTransitions.InvalidIndex() )
		m_pVecStoppedTransitionsForCurrentPanel = m_mapStoppedTransitions[iMap];


	EAnimatingFlags eAnimatingContext = k_EAnimatingFlag_NotAnimating;
	pContext->SetContext( renderCommand.context_id );

	pContext->SetSize( renderCommand.width, renderCommand.height );

	eAnimatingContext |= PushPanelPosition( pContext, renderCommand.panel_position );

	// Default perspective - might get overridden if something is in the optional commands
	pContext->SetPerspective( 1000.0f * m_flUIScaleFactor );

	for ( const void *pRawOptionalProperty : renderCommand.optional_properties )
	{
		EStyleOptionalProperty ePropertyType = reinterpret_cast< const OptionalProperty_t< void * > * >( pRawOptionalProperty )->property_type;
		switch ( ePropertyType )
		{
			// Note, important transform is pushed after PushPanelPosition so CheckTransformIs2DTranslateAndCombineWithPosition works
			case k_EStyleOptionalProperty_TransformMatrix:
				eAnimatingContext |= Push3DTransformMatrix( pContext, reinterpret_cast< const OptionalProperty_t< TransformMatrixWithTransition_t > * >( pRawOptionalProperty )->property_data );
				pContext->CheckTransformIs2DTranslateAndCombineWithPosition();
				break;

			case k_EStyleOptionalProperty_TransformOrigin:
				eAnimatingContext |= Push3DTransformOrigin( pContext, reinterpret_cast< const OptionalProperty_t< TransformOriginWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_TransformPerspective:
				eAnimatingContext |= Push3DTransformPerspective( pContext, reinterpret_cast< const OptionalProperty_t< TransformPerspectiveWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_TransformPerspectiveOrigin:
				eAnimatingContext |= Push3DTransformPerspectiveOrigin( pContext, reinterpret_cast< const OptionalProperty_t< TransformPerspectiveOriginWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_Opacity:
				eAnimatingContext |= PushOpacity( pContext, reinterpret_cast< const OptionalProperty_t< OpacityWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_Scale2D:
				eAnimatingContext |= Push2DScale( pContext, reinterpret_cast< const OptionalProperty_t< Scale2DWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_Rotate2D:
				eAnimatingContext |= Push2DRotate( pContext, reinterpret_cast< const OptionalProperty_t< Rotate2DWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_OpacityMask:
				eAnimatingContext |= PushOpacityMask( pContext, reinterpret_cast< const OptionalProperty_t< OpacityMaskWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_HueShift:
				eAnimatingContext |= PushHueShift( pContext, reinterpret_cast< const OptionalProperty_t< HueShiftWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_Saturation:
				eAnimatingContext |= PushSaturation( pContext, reinterpret_cast< const OptionalProperty_t< SaturationWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_Brightness:
				eAnimatingContext |= PushBrightness( pContext, reinterpret_cast< const OptionalProperty_t< BrightnessWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_Contrast:
				eAnimatingContext |= PushContrast( pContext, reinterpret_cast< const OptionalProperty_t< ContrastWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_WashColor:
				eAnimatingContext |= PushWashColor( pContext, reinterpret_cast< const OptionalProperty_t< WashColorWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_Border:
				eAnimatingContext |= PushBorder( pContext, reinterpret_cast< const OptionalProperty_t< BorderWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_BorderRadius:
				eAnimatingContext |= PushBorderRadius( pContext, reinterpret_cast< const OptionalProperty_t< BorderRadiusWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_TextShadow:
				eAnimatingContext |= PushTextShadow( pContext, reinterpret_cast< const OptionalProperty_t< TextShadowWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_ImageShadow:
				eAnimatingContext |= PushImageShadow( pContext, reinterpret_cast< const OptionalProperty_t< ImageShadowWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_BoxShadow:
				eAnimatingContext |= PushBoxShadow( pContext, reinterpret_cast< const OptionalProperty_t< BoxShadowWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_GaussianBlur:
				eAnimatingContext |= PushGaussianBlur( pContext, reinterpret_cast< const OptionalProperty_t< GaussianBlurWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;

			case k_EStyleOptionalProperty_Clip:
				eAnimatingContext |= PushClip( pContext, reinterpret_cast< const OptionalProperty_t< ClipWithTransition_t > * >( pRawOptionalProperty )->property_data );
				break;
		}
	}

	pContext->SetMixBlendMode( ( EMixBlendMode )renderCommand.mix_blend_mode );

	pContext->SetHasOpaqueBackground( renderCommand.opaque_background && pContext->GetOpacity() >= 0.99f && (pContext->GetOpacityMaskTexture() == nullptr || pContext->GetOpacityMaskOpacity() < 0.001f) );

	pContext->SetNeedsIntermediateTexture( renderCommand.needs_intermediate_texture );

	if ( !V_isempty( renderCommand.composition_layer_texture_name ) )
		pContext->SetCompositionLayerTextureName( renderCommand.composition_layer_texture_name );

	pContext->SetClipAfterTransform( renderCommand.clip_after_transform );

	pContext->SetChildrenHave3DTransforms( renderCommand.children_have_3dtransforms );

	pContext->SetNoClip( renderCommand.suppress_clip_to_bounds );

	pContext->SetHasChildPanels( renderCommand.has_children );

	pContext->SetRequireCompositionLayer( renderCommand.require_composition_layer );
	pContext->SetAlwaysCacheCompositionLayer( renderCommand.always_cache_composition_layer );
	pContext->SetForceNoCompositionLayer( renderCommand.force_no_composition_layer );
	pContext->SetOffscreenCompositionLayer( renderCommand.offscreen_composition_layer );
	pContext->SetFractionalPixelPositions( renderCommand.fractional_pixel_positions );

	// Save whether this context needs a composition layer now that we've set and interpolated all 
	// it's relevant properties
	pContext->SetHasCompositionLayer( pContext->BNeedsCompositionLayer() );

	// Set if this panel wants to be hit tested
	pContext->SetWantsHitTest( renderCommand.wants_hit_test );
	pContext->SetWantsHitTestChildren( renderCommand.wants_hit_test_children );

	pContext->SetWantsScreenspaceQuadOutput( renderCommand.wants_screenspace_quad_output );

	m_pVecStoppedTransitionsForCurrentPanel = NULL;
	if ( pAnimatingFlags )
	{
		*pAnimatingFlags |= eAnimatingContext;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called to push additional animation/transform state
//-----------------------------------------------------------------------------
void CUIAnimationEngine::PushAnimationAndTransformContext( const PushAAndTContextRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::PushAnimationAndTransformContext", VPROF_BUDGETGROUP_TENFOOT );
	
	if ( m_unSkipUntilContextPopCounter > 0 )
	{
		// Throw out any already cached composition layer, since we culled a drawing op it won't be safe to re-use anymore
		FreeAndForceRepaintOfCompositingLayer( renderCommand.context_id, false );

		++m_unSkipUntilContextPopCounter;
		return;
	}

	CAnimationAndTransformContext *pContext = AllocPerFrameObject< CAnimationAndTransformContext >();
	m_AnimationAndTransformStack.AddToTail( pContext );

	// Keep track of whether any properties are currently animating
	EAnimatingFlags eAnimatingContext = k_EAnimatingFlag_NotAnimating;

	// Scope for lock
	{
		AUTO_LOCK( m_MutexStoppedTransitionData );
		PushDataAndStylesIntoContext( pContext, renderCommand, &eAnimatingContext );
	}

	// Bail really early if opacity is actually 0, the layout thread culls things that are 0.0 opacity, but only
	// if there is no transform/animation, we can do better here as animations/transforms complete but the layout thread
	// is not yet aware.
	if ( pContext->GetOpacity() < 0.000001f )
	{

		// The below can be quite useful for perf debugging/debugging why panels get past
		// the layout thread simple heuristic cull, but it's not actually thread safe as the
		// panel can be deleted, hence the try catch as you will hit access violations on read, 
		// only use when you really need to debug this
#if 0
		try
		{
			CPanelPtr< CPanel2D > ptr;
			ptr.SetFromUInt64( pContext->GetContext() );

			Msg( "Culling panel transparent %s\n", ptr->GetID() );
		}
		catch ( ... )
		{

		}
#endif

		m_unSkipUntilContextPopCounter = 1;

		FreePerFrameObject( pContext );
		m_AnimationAndTransformStack.Remove( m_AnimationAndTransformStack.Tail() );
		return;
	}

	RenderCommand_t *pCommand = nullptr;

	float flWide, flTall;
	pContext->GetSize( flWide, flTall );

	Vector4D corners[4];
	corners[0].Init( 0, 0, 0, 1.0f );
	corners[1].Init( flWide, 0, 0, 1.0f );
	corners[2].Init( flWide, flTall, 0, 1.0f );
	corners[3].Init( 0, flTall, 0, 1.0f );

	TransformQuadForContext( corners, m_AnimationAndTransformStack.Tail() );

	bool bFullyClipped = BIsCurrentContextFullyClipped( corners );
	if ( bFullyClipped )
	{
		Vector4D cornersUnTransformed[4];
		float xPos, yPos, zPos;
		pContext->GetPosition( xPos, yPos, zPos );

		cornersUnTransformed[0].Init( xPos, yPos, 0, 1.0f );
		cornersUnTransformed[1].Init( xPos+flWide, yPos, 0, 1.0f );
		cornersUnTransformed[2].Init( xPos+flWide, yPos+flTall, 0, 1.0f );
		cornersUnTransformed[3].Init( xPos, yPos+flTall, 0, 1.0f );

		bool bUntransformedClipped = BIsCurrentContextFullyClipped( cornersUnTransformed );

		// The below can be quite useful for perf debugging/debugging why panels get past
		// the layout thread simple heuristic cull, but it's not actually thread safe as the
		// panel can be deleted, hence the try catch as you will hit access violations on read, 
		// only use when you really need to debug this
#if 0
		try
		{
			CPanelPtr< CUIPanel > ptr;
			ptr.SetFromUInt64( pContext->GetContext() );

			Msg( "Culling panel %1.2f x %1.2f %s %s (only clipped by transform: %s)\n", flWide, flTall, ptr->GetID(), ptr->GetPanelType().String(), bUntransformedClipped ? "false" : "true" );
		}
		catch ( ... ) 
		{

		}
#endif
		
		m_unSkipUntilContextPopCounter = 1;

		// Throw out any already cached composition layer, since we culled a drawing op it won't be safe to re-use anymore
		FreeAndForceRepaintOfCompositingLayer( pContext->GetContext(), bUntransformedClipped );

		FreePerFrameObject( pContext );
		m_AnimationAndTransformStack.Remove( m_AnimationAndTransformStack.Tail() );
		return;
	}


	float flPerspective = 1000.0f * m_flUIScaleFactor;
	float flPerspectiveX = 0.0f;
	float flPerspectiveY = 0.0f;

	Vector2D mouse( m_flMouseX, m_flMouseY );
	Vector vRenderPos( 0.0f, 0.0f, 0.0f );

	bool bParentsHitTestOK = true;
	bool bParentWantsHitTestChildren = true;
	{
		VPROF_BUDGET_THREAD( "AccumulateFromParents", VPROF_BUDGETGROUP_TENFOOT );

		float flParentCompLayerWidth = m_unSurfaceWidth;
		float flParentCompLayerHeight = m_unSurfaceHeight;
		float flImmediateParentWidth = m_unSurfaceWidth;
		float flImmediateParentHeight = m_unSurfaceHeight;

		FOR_EACH_LL( m_AnimationAndTransformStack, i )
		{
			// Don't process our own context in this loop
			if ( i  == m_AnimationAndTransformStack.Tail() )
				break;

			CAnimationAndTransformContext *pParentContext = m_AnimationAndTransformStack[i];

			if ( !pParentContext->BPassedHitTest() )
				bParentsHitTestOK = false;
				
			if ( !pParentContext->BWantsHitTestChildren() )
				bParentWantsHitTestChildren = false;

			pParentContext->GetSize( flImmediateParentWidth, flImmediateParentHeight );

			float scrollX = 0.0f;
			float scrollY = 0.0f;
			pParentContext->GetContentsScroll( scrollX, scrollY );

			// Accumulate position relative to parents, zeroing on each parent composition layer as they'll take 
			// care of positioning above them.
			if ( pParentContext->BHasCompositionLayer() )
			{
				vRenderPos.Init( 0.0f, 0.0f, 0.0f );
				vRenderPos.x += scrollX;
				vRenderPos.y += scrollY;
				pParentContext->GetSize( flParentCompLayerWidth, flParentCompLayerHeight );
			}
			else
			{
				float px, py, pz;
				pParentContext->GetPosition( px, py, pz );

				vRenderPos.x += px;
				vRenderPos.y += py;
				vRenderPos.z += pz;

				vRenderPos.x += scrollX;
				vRenderPos.y += scrollY;
			}

			if ( pParentContext->BPassedHitTest() )
				mouse = pParentContext->GetMousePosition();

			flPerspective = pParentContext->GetPerspective();
			flPerspectiveX = pParentContext->GetPerspectiveOriginX();
			flPerspectiveY = pParentContext->GetPerspectiveOriginY();
		}

		// Perspective comes in expected to be based on immediate parent size, adjust to account for not having a composition layer on immediate parent
		// if we've enabled that
		if ( s_convarPanoramaTransformParentsNoLayerForPerspective.GetBool() )
		{
			flPerspectiveX = -vRenderPos.x + flPerspectiveX + flParentCompLayerWidth / 2.0f - flImmediateParentWidth / 2.0f;
			flPerspectiveY = -vRenderPos.y + flPerspectiveY + flParentCompLayerHeight / 2.0f - flImmediateParentHeight / 2.0f;
		}
	}

	// Our position is always included
	float px, py, pz;
	px = py = pz = 0.0f;
	pContext->GetPosition( px, py, pz );

	vRenderPos.x += px;
	vRenderPos.y += py;
	vRenderPos.z += pz;


	// Set the current drawing offset on the current context
	// Values used in CUIAnimationEngine::GetCurrentDrawingOffsets
	if ( pContext->BHasCompositionLayer() )
	{
		float flCurrentScrollX, flCurrentScrollY;
		pContext->GetContentsScroll( flCurrentScrollX, flCurrentScrollY );
		pContext->SetCurrentDrawingOffsets( flCurrentScrollX, flCurrentScrollY, 0.0f );
	}
	else
	{
		float flCurrentScrollX, flCurrentScrollY;
		pContext->GetContentsScroll( flCurrentScrollX, flCurrentScrollY );
		pContext->SetCurrentDrawingOffsets( vRenderPos.x + flCurrentScrollX, vRenderPos.y + flCurrentScrollY, 0.0f );
	}

	// If our parents passed hit testing, then test ourselves as well, notice we don't check BWantsHitTest(), 
	// that's because even if we don't want hit test results our children may.   We'll throw out our hit result and let
	// someone else behind us return theirs though when bubbling out of the animation layer.
	pContext->SetPassedHitTest( false );
	bool bNeedsScreenspaceQuad = pContext->BWantsScreenspaceQuadOutput();
	if ( bParentWantsHitTestChildren && ( bNeedsScreenspaceQuad || bParentsHitTestOK || m_vecTrackingMousePanels.Count() ) )
	{
		int iVecTracking = m_vecTrackingMousePanels.Find( pContext->GetContext() );

		// if we our parent is inside the hit test area or we want to track this panel
		if ( bParentsHitTestOK || bNeedsScreenspaceQuad || iVecTracking != m_vecTrackingMousePanels.InvalidIndex() )
		{
			bool bForce = iVecTracking != m_vecTrackingMousePanels.InvalidIndex();

			Vector2D screenspaceQuad[4];
			Vector2D *pScreenspaceQuad = NULL;
			if ( bNeedsScreenspaceQuad )
				pScreenspaceQuad = screenspaceQuad;

			bool bPassedHitTest = BHitTestCurrentContext( corners, bForce, mouse.x, mouse.y, pScreenspaceQuad );

			if ( pScreenspaceQuad )
				pContext->SetScreenspaceQuad( pScreenspaceQuad );

			if ( bParentsHitTestOK && bPassedHitTest )
			{
				pContext->SetMousePosition( Vector2D( mouse.x, mouse.y ) );
				pContext->SetPassedHitTest( true );
			}
			else if ( bForce )
			{
				pContext->SetMousePosition( Vector2D( mouse.x, mouse.y ) );
			}
		}
	}

	bool bNeededLayerLastTimeWeDrew = false;

	if ( m_unRenderCountThisFrame == 1 )
	{
		int iMap = m_mapPanelsToCompositionLayerNeeded.Find( pContext->GetContext() );
		if ( iMap != m_mapPanelsToCompositionLayerNeeded.InvalidIndex() )
			bNeededLayerLastTimeWeDrew = m_mapPanelsToCompositionLayerNeeded[iMap];

		if ( bNeededLayerLastTimeWeDrew )
			m_treePanelsThatNeededLayersThisFrame.Insert( pContext->GetContext() );

		m_mapPanelsToCompositionLayerNeeded.InsertOrReplace( pContext->GetContext(), pContext->BHasCompositionLayer() );

	}
	else
	{
		if ( m_treePanelsThatNeededLayersThisFrame.Find( pContext->GetContext() ) != m_treePanelsThatNeededLayersThisFrame.InvalidIndex() )
			bNeededLayerLastTimeWeDrew = true;
	}

	bool bDontRedrawChildren = false;
	bool bIsLayerPush = false;
	bool bIsUntransformed = true;
	if ( pContext->BHasCompositionLayer() )
	{
		// Add the clip layer, since we didn't get our own composition layer that will clip for us
		PushCompositingLayerRenderCommand_t *pCmdOut = outputCommandList.AllocType< PushCompositingLayerRenderCommand_t >();
		bIsLayerPush = true;

		pCommand = pCmdOut;

		// If the last time we painted we didn't think we needed a layer, then we could have transitioned from layer->no_layer->layer 
		// again and have stale layer content cached, need to do a full repaint then regardless of what the layout thread thought.
		EPanelRepaint eNeedsRepaint = renderCommand.needs_full_repaint;
		if ( bNeededLayerLastTimeWeDrew != true )
			eNeedsRepaint = k_EPanelRepaintFull;

		bool bForceParentLayersRedraw = false;

		if ( eNeedsRepaint == k_EPanelRepaintComposition && m_pRenderEngine->Access3DSurface()->PingCompositingLayer( pContext->GetContext(), renderCommand.width, renderCommand.height ) )
		{
			// Don't draw our children at all unless we are doing a full repaint
			bDontRedrawChildren = true;
			bForceParentLayersRedraw = true;
		}
		else if( eNeedsRepaint == k_EPanelRepaintFull )
		{
			bForceParentLayersRedraw = true;
		}
		else if( eNeedsRepaint == k_EPanelRepaintNone )
		{
			bDontRedrawChildren = true;
		}

		if ( bForceParentLayersRedraw )
		{
#if 0
			try
			{
				CPanelPtr< CUIPanel > ptr;
				ptr.SetFromUInt64( pContext->GetContext() );

				Msg( " Forcing parents to redraw for layer %s (%s) (%p - %llu)\n", ptr->GetID(), ptr->GetPanelType().String(), ptr->ClientPtr(), pContext->GetContext() );

			}
			catch( ... ) {}
#endif

			RenderOperation_t *pParent = m_pCurrentOperationContext;
			while ( pParent && !pParent->m_bForcedParentDrawAlready )
			{
				pParent->m_bForcedParentDrawAlready = true;
				if ( pParent->m_bDontDrawChildren )
				{
					pParent->m_bDontDrawChildren = false;
					PushCompositingLayerRenderCommand_t *pParentCommand = ( PushCompositingLayerRenderCommand_t * )pParent->m_pRenderCommand;
					pParentCommand->needs_clear = true;
				}
				pParent = pParent->m_pParent;
			}
		}

		pCmdOut->needs_clear = !bDontRedrawChildren;
		pCmdOut->layer_id = pContext->GetContext();
		pCmdOut->width = renderCommand.width;
		pCmdOut->height = renderCommand.height;
		pCmdOut->perspective_depth = pContext->GetPerspective();
		pCmdOut->opacity = pContext->GetOpacity();
		pCmdOut->opacity_mask_texture.SetTexture( pContext->GetOpacityMaskTexture(), outputCommandList );
		pCmdOut->opacity_mask_opacity = pContext->GetOpacityMaskOpacity();
		pCmdOut->hue_shift = pContext->GetHueShift();
		pCmdOut->saturation = pContext->GetSaturation();
		pCmdOut->brightness = pContext->GetBrightness();
		pCmdOut->contrast = pContext->GetContrast();
		pCmdOut->composition_color = pContext->GetCompositionColor().GetRawColor();
		pCmdOut->gaussian_blur.passes = pContext->GetBlurPasses();
		pCmdOut->gaussian_blur.stddev_hor = pContext->GetBlurStdDeviationHor();
		pCmdOut->gaussian_blur.stddev_ver = pContext->GetBlurStdDeviationVer();
		pCmdOut->gaussian_blur.blurType = pContext->GetBlurType();
		pCmdOut->needs_intermediate_texture = pContext->BNeedsIntermediateTexture();
		pCmdOut->always_cache_composition_layer = pContext->BAlwaysCacheCompositionLayer();
		pCmdOut->offscreen_composition_layer = pContext->BOffscreenCompositionLayer();
		pCmdOut->fractional_pixel_positions = pContext->GetFractionalPixelPositions();

		if ( pContext->GetCompositionLayerTextureName() )
		{
			pCmdOut->composition_layer_texture_name = outputCommandList.CopyString( pContext->GetCompositionLayerTextureName() );
		}

		if ( pContext->GetMixBlendMode() != k_EMixBlendModeNormal )
			pCmdOut->mix_blend_mode = pContext->GetMixBlendMode();

		float xScale, yScale;
		pContext->GetScale2DFactors( xScale, yScale );
		pCmdOut->scale2d_factors.x = xScale;
		pCmdOut->scale2d_factors.y = yScale;

		if( xScale < 0.99999f || xScale > 1.00001f || yScale < 0.99999f || yScale > 1.00001f )
			bIsUntransformed = false;

		float flRotate;
		pContext->GetRotate2D( flRotate );
		pCmdOut->rotate_2d = flRotate;

		if( flRotate < -0.001f || flRotate > 0.001f )
			bIsUntransformed = false;

		if ( pContext->GetMixBlendMode() != k_EMixBlendModeNormal )
			pCmdOut->mix_blend_mode = pContext->GetMixBlendMode();

		VMatrix mat = pContext->GetTransformMatrix();
		if ( pContext->BHasTransformMatrix() && !mat.IsIdentity() )
		{
			float xTrans = pContext->GetTransformOriginX();
			float yTrans = pContext->GetTransformOriginY();

			mat = SetupMatrixTranslation( Vector( xTrans, yTrans, 0 ) ) * mat * SetupMatrixTranslation( Vector( -xTrans, -yTrans, 0 ) );

			bIsUntransformed = false;
		}

		// Add perspective into matrix
		VMatrix matPerspective = SetupMatrixTranslation( Vector( flPerspectiveX, flPerspectiveY, 0 ) );

		VMatrix matTemp = VMatrix::GetIdentityMatrix();
		matTemp.SetElement( 2, 3, -1.0f/flPerspective );
		matPerspective = matTemp * matPerspective;
		matPerspective = SetupMatrixTranslation( Vector( -flPerspectiveX, -flPerspectiveY, 0 ) ) * matPerspective;

		mat = matPerspective * mat;

		VMatrixToRenderMatrix( pCmdOut->transform, mat );
		
		pCmdOut->layer_quad_top_left.x = vRenderPos.x;
		pCmdOut->layer_quad_top_left.y = vRenderPos.y;
		pCmdOut->layer_quad_top_left.z = vRenderPos.z;

		pCmdOut->layer_quad_top_right.x = vRenderPos.x + renderCommand.width;
		pCmdOut->layer_quad_top_right.y = vRenderPos.y;
		pCmdOut->layer_quad_top_right.z = vRenderPos.z;

		pCmdOut->layer_quad_bottom_left.x = vRenderPos.x;
		pCmdOut->layer_quad_bottom_left.y = vRenderPos.y + renderCommand.height;
		pCmdOut->layer_quad_bottom_left.z = vRenderPos.z;

		pCmdOut->layer_quad_bottom_right.x = vRenderPos.x + renderCommand.width;
		pCmdOut->layer_quad_bottom_right.y = vRenderPos.y + renderCommand.height;
		pCmdOut->layer_quad_bottom_right.z = vRenderPos.z;

		if( vRenderPos.z != 0 )
			bIsUntransformed = false;

		CAnimationAndTransformContext *pParentContext = NULL;
		if( m_AnimationAndTransformStack.Count() > 1 )
			pParentContext = m_AnimationAndTransformStack[m_AnimationAndTransformStack.Count() - 2];

		if( pParentContext && pParentContext->BMightScroll() )
			bIsUntransformed = false;

		// Calculate average zposition of the panel, post transform, so we can use that to order drawing operations 
		// correctly.
		float zAvg = 0.0f;

		if ( pContext->BHasTransformMatrix() )
		{
			VMatrix transform = pContext->GetTransformMatrix();
			Vector vec( vRenderPos.x, vRenderPos.y, vRenderPos.z );
			transform.V3Mul( vec, vec );
			zAvg += vec.z;

			Vector vec1( vRenderPos.x + renderCommand.width, vRenderPos.y, vRenderPos.z );
			transform.V3Mul( vec1, vec1 );
			zAvg += vec.z;

			Vector vec2( vRenderPos.x, vRenderPos.y + renderCommand.height, vRenderPos.z );
			transform.V3Mul( vec2, vec2 );
			zAvg += vec.z;

			Vector vec3( vRenderPos.x + renderCommand.width, vRenderPos.y + renderCommand.height, vRenderPos.z );
			transform.V3Mul( vec3, vec3 );
			zAvg += vec.z;

			vRenderPos.z = zAvg / 4.0f;
		}

		if ( pContext->BBorderWidthSet() )
		{
			float rgBorderWidths[4];
			Color rgBorderColors[4];
			pContext->GetBorder( rgBorderWidths[0], rgBorderWidths[1], rgBorderWidths[2], rgBorderWidths[3], rgBorderColors[0], rgBorderColors[1], rgBorderColors[2], rgBorderColors[3] );

			pCmdOut->border = outputCommandList.AllocType< BorderData_t >();
			pCmdOut->border->top.style = k_EBorderStyleSolid;
			pCmdOut->border->top.width = rgBorderWidths[ 0 ];
			pCmdOut->border->top.color = rgBorderColors[ 0 ].GetRawColor();
			pCmdOut->border->right.style = k_EBorderStyleSolid;
			pCmdOut->border->right.width = rgBorderWidths[ 1 ];
			pCmdOut->border->right.color = rgBorderColors[ 1 ].GetRawColor();
			pCmdOut->border->bottom.style = k_EBorderStyleSolid;
			pCmdOut->border->bottom.width = rgBorderWidths[ 2 ];
			pCmdOut->border->bottom.color = rgBorderColors[ 2 ].GetRawColor();
			pCmdOut->border->left.style = k_EBorderStyleSolid;
			pCmdOut->border->left.width = rgBorderWidths[ 3 ];
			pCmdOut->border->left.color = rgBorderColors[ 3 ].GetRawColor();
		}

		if ( pContext->BBorderRadiusSet() )
		{
			float rgRadii[8];
			pContext->GetCornerRadius( rgRadii[0], rgRadii[1], rgRadii[2], rgRadii[3], rgRadii[4], rgRadii[5], rgRadii[6], rgRadii[7] );

			// Ensure that the curves do not overlap, per http://www.w3.org/TR/css3-background/ section 5.5, we do this here last after interpolation
			// and all other adjustments been done.  This algorithm decreases all raddi evenly so that ratios remain the same and circular curves 
			// do not turn elliptical or such (the algorithm is part of the css standard).
			float flOverlap = renderCommand.width / (rgRadii[0]+rgRadii[2]);
			flOverlap = MIN( flOverlap, renderCommand.width / (rgRadii[4]+rgRadii[6]) );
			flOverlap = MIN( flOverlap, renderCommand.height / (rgRadii[1]+rgRadii[7]) );
			flOverlap = MIN( flOverlap, renderCommand.height / (rgRadii[3]+rgRadii[5]) );
			if ( flOverlap < 1.0 )
			{
				for( int i=0; i<8; ++i )
				{
					rgRadii[i] *= flOverlap;
				}
			}

			pCmdOut->border_radius = outputCommandList.AllocType< RadiusData_t >();
			pCmdOut->border_radius->top_left.horizontal = rgRadii[0];
			pCmdOut->border_radius->top_left.vertical = rgRadii[ 1 ];
			pCmdOut->border_radius->top_right.horizontal = rgRadii[ 2 ];
			pCmdOut->border_radius->top_right.vertical = rgRadii[ 3 ];
			pCmdOut->border_radius->bottom_right.horizontal = rgRadii[ 4 ];
			pCmdOut->border_radius->bottom_right.vertical = rgRadii[ 5 ];
			pCmdOut->border_radius->bottom_left.horizontal = rgRadii[ 6 ];
			pCmdOut->border_radius->bottom_left.vertical = rgRadii[ 7 ];
		}

		if ( pContext->BBoxShadowSet() )
		{
			Color boxShadowColor;
			float flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance;
			bool bInset, bFill, bAnimating;
			pContext->GetBoxShadow( bInset, bFill, flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, boxShadowColor, bAnimating );

			pCmdOut->box_shadow = outputCommandList.AllocType< BoxShadowData_t >();
			pCmdOut->box_shadow->inset = bInset;
			pCmdOut->box_shadow->fill = bFill;
			pCmdOut->box_shadow->horizontal_offset = flHorOffset;
			pCmdOut->box_shadow->vertical_offset = flVerOffset;
			pCmdOut->box_shadow->blur_radius = flBlurRadius;
			pCmdOut->box_shadow->spread_distance = flSpreadDistance;
			pCmdOut->box_shadow->color = boxShadowColor.GetRawColor();
			pCmdOut->box_shadow->animating = bAnimating;
		}

		if ( pContext->BHasRadialClip() )
		{
			float flCenterX, flCenterY, flStartAngle, flSectorAngle;
			pContext->GetRadialClip( &flCenterX, &flCenterY, &flStartAngle, &flSectorAngle );

			pCmdOut->radial_clip = outputCommandList.AllocType< RadialClipData_t >();
			pCmdOut->radial_clip->center_x = flCenterX;
			pCmdOut->radial_clip->center_y = flCenterY;
			pCmdOut->radial_clip->start_angle = flStartAngle;
			pCmdOut->radial_clip->sector_angle = flSectorAngle;
		}
	}

	Assert( m_pCurrentOperationContext );
	if( m_pCurrentOperationContext )
	{
		// Add first operation, which is push for compositing layer, or just noop grouping layer if no compositing was needed
		RenderOperation_t *pOperation = AllocPerFrameObject< RenderOperation_t >();
		m_pCurrentOperationContext->m_pVecChildOperations->AddToTail( pOperation );
		pOperation->m_flAvgZPos = vRenderPos.z;
		pOperation->m_flZIndex = renderCommand.zindex;
		if ( pOperation->m_flAvgZPos != 0.0f || pOperation->m_flZIndex != 0.0f )
		{
			m_pCurrentOperationContext->m_bSortChildOperations = true;
		}
		pOperation->m_pRenderCommand = pCommand;
		pOperation->m_bIsLayerPush = bIsLayerPush;
		pOperation->m_bUntransformed = bIsUntransformed;
		pOperation->m_flWidth = renderCommand.width;
		pOperation->m_flHeight = renderCommand.height;
		
		if ( !bIsLayerPush )
		{
			float x, y, z;
			GetCurrentDrawingOffsets( x, y, z );
			pOperation->m_flXOffsetInParentLayer = x;
			pOperation->m_flYOffsetInParentLayer = y;
		}
		else
		{
			pOperation->m_flXOffsetInParentLayer = vRenderPos.x;
			pOperation->m_flYOffsetInParentLayer = vRenderPos.y;
		}
		pOperation->m_pParent = m_pCurrentOperationContext;
		pOperation->m_ulPanelPtrValue = pContext->GetContext();
		pOperation->m_bWantsHitTest = pContext->BWantsHitTest();
		pOperation->m_bHitTestPassed = pContext->BPassedHitTest();
		pOperation->m_HitTestCoords = pContext->GetMousePosition();
		pOperation->m_pScreenspaceQuad = pContext->TakeOverScreenspaceQuadPtr();
		pOperation->m_bDontDrawChildren = bDontRedrawChildren;
		pOperation->m_bIsPanelContext = true;
#ifdef PANORAMA_ANIMATION_ENGINE_OCCLUSION
		pOperation->m_CoveredAxisAlignedRect[0].x = pOperation->m_CoveredAxisAlignedRect[0].y = 0.0;
		pOperation->m_CoveredAxisAlignedRect[1].x = pOperation->m_CoveredAxisAlignedRect[1].y = 0.0;

		// If we have an opaque background and our transformed quad is still axis aligned than track our covered rect
		// to clip panels behind us from drawing within. 
		if ( pContext->BHasOpaqueBackground() 
			&& corners[0].x == corners[3].x 
			&& corners[1].x == corners[2].x
			&& corners[0].y == corners[1].y
			&& corners[2].y == corners[3].y )
		{
			VPROF_BUDGET_THREAD( "CUIAnimationEngine::PushAnimationAndTransformContext - bubble occlusion", VPROF_BUDGETGROUP_TENFOOT );
	
#if 0
			try
			{
				CPanelPtr< CUIPanel > ptr;
				ptr.SetFromUInt64( pContext->GetContext() );
				CUIPanel *pPanel = ptr.Get();
				Msg( "Panel %s (%s) (%p) is covering: %1.2f,%1.2f %1.2f,%1.2f\n", pPanel->GetID(), pPanel->GetPanelType().String(), ptr->ClientPtr(), corners[0].x, corners[0].y, corners[2].x, corners[2].y );
			}
			catch ( ... ) { }
#endif

			// If our corners are rounded we must select that from our covered area
			if ( pContext->BBorderRadiusSet() )
			{
				float rgRadii[8];
				pContext->GetCornerRadius( rgRadii[0], rgRadii[1], rgRadii[2], rgRadii[3], rgRadii[4], rgRadii[5], rgRadii[6], rgRadii[7] );

				//void GetCornerRadius( float &flTopLeftHorizontal, float &flTopLeftVertical, float &flTopRightHorizontal, float &flTopRightVertical,
				//	float &flBottomRightHorizontal, float &flBottomRightVertical, float &flBottomLeftHorizontal, float &flBottomLeftVertical )

				corners[0].x += Max( rgRadii[0], rgRadii[6] );
				corners[3].x += Max( rgRadii[0], rgRadii[6] );

				corners[0].y += Max( rgRadii[1], rgRadii[3] );
				corners[1].y += Max( rgRadii[1], rgRadii[3] );

				corners[1].x -= Max( rgRadii[2], rgRadii[4] );
				corners[2].x -= Max( rgRadii[2], rgRadii[4] );

				corners[2].y -= Max( rgRadii[5], rgRadii[7] );
				corners[3].y -= Max( rgRadii[5], rgRadii[7] );
			}

			// Make sure some actual area is covered now that we are fully updated
			if ( corners[2].x > corners[0].x && corners[2].y > corners[0].y )
			{
				pOperation->m_CoveredAxisAlignedRect[0].x = corners[0].x;
				pOperation->m_CoveredAxisAlignedRect[0].y = corners[0].y;
				pOperation->m_CoveredAxisAlignedRect[1].x = corners[2].x;
				pOperation->m_CoveredAxisAlignedRect[1].y = corners[2].y;

				// Expand into parents as well
				RenderOperation_t *pParent = pOperation->m_pParent;

				// First entry is for us
				int iStack = m_AnimationAndTransformStack.Tail();
			
				Vector4D cornersParent[4];
				cornersParent[0].Init( corners[0].x, corners[0].y, 0, 1.0f );
				cornersParent[1].Init( corners[1].x, corners[1].y, 0, 1.0f );
				cornersParent[2].Init( corners[2].x, corners[2].y, 0, 1.0f );
				cornersParent[3].Init( corners[3].x, corners[3].y, 0, 1.0f );

				while ( pParent )
				{
					CAnimationAndTransformContext *pThisContext = m_AnimationAndTransformStack[iStack];

					int iPrev = m_AnimationAndTransformStack.Previous( iStack );
					CAnimationAndTransformContext *pParentContext = NULL;
					if ( iPrev != m_AnimationAndTransformStack.InvalidIndex() )
						pParentContext = m_AnimationAndTransformStack[iPrev];

					// Can't consider as opaque inside parent unless parent has no opacity changes when compositing
					if( pParentContext && ( pParentContext->GetOpacity() < 1.0f || pParentContext->GetOpacityMaskTexture() != nullptr || pParentContext->BMightScroll() ) )
					{
#if 0
						if( pParentContext->BMightScroll() )
						{
							try
							{
								CPanelPtr< CUIPanel > ptr;
								ptr.SetFromUInt64( pParentContext->GetContext() );
								Msg( "  -- Parent panel %s (%s) preventing further bubbling of covering rect because it might scroll\n", ptr->GetID(), ptr->GetPanelType().String(), pParentContext->GetOpacity() );
							}
							catch( ... ) {}
						}

						if ( pParentContext->GetOpacity() > 0.0f && pParentContext->GetOpacity() < 1.0f )
						{
							try
							{
								CPanelPtr< CUIPanel > ptr;
								ptr.SetFromUInt64( pParentContext->GetContext() );
								Msg( "  -- Parent panel %s (%s) (opacity: %1.2f) preventing further bubbling of covering rect\n", ptr->GetID(), ptr->GetPanelType().String(), pParentContext->GetOpacity() );
							}
							catch ( ... ) {}
						}
#endif
						break;
					}

					
					float flW, flT;
					pThisContext->GetSize( flW, flT );

					Vector4D parentCorners[4];
					parentCorners[0].Init( 0, 0 );
					parentCorners[1].Init( flW, 0 );
					parentCorners[2].Init( flW, flT );
					parentCorners[3].Init( 0, flT );

					// If we are partially outside our parent, then we should stop now, as we don't want to occlude if we will actually be clipped and not draw!
					bool bOutsideParent = false;
					for ( int i = 0; i < 1; i++ )
					{
						if ( !BPointInsideConvexQuad( corners[i].x, corners[i].y, parentCorners ) )
						{
							bOutsideParent = true;
							break;
						}
					}

					if ( bOutsideParent )
						break;

					iStack = m_AnimationAndTransformStack.Previous( iStack );

					if ( iStack == m_AnimationAndTransformStack.InvalidIndex() )
					{
						break;
					}

					float flParentAreaAlreadyCovered = (pParent->m_CoveredAxisAlignedRect[1].x - pParent->m_CoveredAxisAlignedRect[0].x) * (pParent->m_CoveredAxisAlignedRect[1].y - pParent->m_CoveredAxisAlignedRect[0].y);
					
					TransformQuadForContext( cornersParent, iStack );

					// If not axis aligned can't use for our cheap scissoring occlusion work
					if ( cornersParent[0].x == cornersParent[3].x
						&& cornersParent[1].x == cornersParent[2].x
						&& cornersParent[0].y == cornersParent[1].y
						&& cornersParent[2].y == cornersParent[3].y )
					{
						float flNewAreaCovered = (cornersParent[2].x - cornersParent[0].x)*(cornersParent[2].y - cornersParent[0].y);
						if ( flParentAreaAlreadyCovered < 0.001f )
						{
							pParent->m_CoveredAxisAlignedRect[0].x = cornersParent[0].x;
							pParent->m_CoveredAxisAlignedRect[0].y = cornersParent[0].y;
							pParent->m_CoveredAxisAlignedRect[1].x = cornersParent[2].x;
							pParent->m_CoveredAxisAlignedRect[1].y = cornersParent[2].y;
						}
						else
						{
							Vector2D vecIntersectingRect[2];
							vecIntersectingRect[0].x = Max( pParent->m_CoveredAxisAlignedRect[0].x, cornersParent[0].x );
							vecIntersectingRect[0].y = Max( pParent->m_CoveredAxisAlignedRect[0].y, cornersParent[0].y );
							vecIntersectingRect[1].x = Min( pParent->m_CoveredAxisAlignedRect[1].x, cornersParent[2].x );
							vecIntersectingRect[1].y = Min( pParent->m_CoveredAxisAlignedRect[1].y, cornersParent[2].y );

							if ( vecIntersectingRect[1].x > vecIntersectingRect[0].x && vecIntersectingRect[1].y > vecIntersectingRect[0].y )
							{
								// Use new area as base to expand if it was larger
								if ( flNewAreaCovered > flParentAreaAlreadyCovered )
								{
									pParent->m_CoveredAxisAlignedRect[0].x = cornersParent[0].x;
									pParent->m_CoveredAxisAlignedRect[0].y = cornersParent[0].y;
									pParent->m_CoveredAxisAlignedRect[1].x = cornersParent[1].x;
									pParent->m_CoveredAxisAlignedRect[1].y = cornersParent[1].y;
								}

								pParent->m_CoveredAxisAlignedRect[0].x = Min( pParent->m_CoveredAxisAlignedRect[0].x, vecIntersectingRect[0].x );
								pParent->m_CoveredAxisAlignedRect[0].y = Min( pParent->m_CoveredAxisAlignedRect[0].y, vecIntersectingRect[0].y );
								pParent->m_CoveredAxisAlignedRect[1].x = Max( pParent->m_CoveredAxisAlignedRect[1].x, vecIntersectingRect[1].x );
								pParent->m_CoveredAxisAlignedRect[1].y = Max( pParent->m_CoveredAxisAlignedRect[1].y, vecIntersectingRect[1].y );
							}
							else if ( flNewAreaCovered > flParentAreaAlreadyCovered )
							{
								pParent->m_CoveredAxisAlignedRect[0].x = cornersParent[0].x;
								pParent->m_CoveredAxisAlignedRect[0].y = cornersParent[0].y;
								pParent->m_CoveredAxisAlignedRect[1].x = cornersParent[2].x;
								pParent->m_CoveredAxisAlignedRect[1].y = cornersParent[2].y;
							}
						}
					}
					else
					{
						break;
					}

#if 0
					try
					{
						CPanelPtr< CUIPanel > ptr;
						ptr.SetFromUInt64( pParent->m_ulPanelPtrValue );
						Msg( "  Parent panel %s (%s) is now covering: %1.2f,%1.2f %1.2f,%1.2f\n", ptr->GetID(), ptr->GetPanelType().String(), cornersParent[0].x, cornersParent[0].y, cornersParent[2].x, cornersParent[2].y );
					}
					catch ( ... ) {}
#endif
					
					pParent = pParent->m_pParent;
				}
			}
		}
#endif  // PANORAMA_ANIMATION_ENGINE_OCCLUSION

		pOperation->m_pVecChildOperations = GetFreeRenderOpsVector();
		m_pCurrentOperationContext = pOperation;

		if ( !pContext->BHasCompositionLayer() )
		{
			if ( pContext->BNeedsPanelContextIfNoCompositonLayer() )
			{
				pContext->SetPanelContextPushed( true );

				// Add the clip layer, since we didn't get our own composition layer that will clip for us
				PushPanelContextInLayerRenderCommand_t *pTransformPush = outputCommandList.AllocType< PushPanelContextInLayerRenderCommand_t >();

				pTransformPush->position.x = m_pCurrentOperationContext->m_flXOffsetInParentLayer;
				pTransformPush->position.y = m_pCurrentOperationContext->m_flYOffsetInParentLayer;
				pTransformPush->position.z = pz;

				pTransformPush->context_id = renderCommand.context_id;

				pTransformPush->width = m_pCurrentOperationContext->m_flWidth;
				pTransformPush->height = m_pCurrentOperationContext->m_flHeight;

				Color contextCompositionColor = pContext->GetCompositionColor();
				if ( contextCompositionColor.GetRawColor() != 0xffffffff && contextCompositionColor.a() != 0x00 && pContext->BIsFastCompositionColor() )
				{
					pTransformPush->composition_color = contextCompositionColor.GetRawColor();
				}
				else
				{
					pTransformPush->composition_color = 0xffffffff;
				}
				pTransformPush->mix_blend_mode = pContext->GetMixBlendMode();
				pTransformPush->fractional_pixel_positions = pContext->GetFractionalPixelPositions();

				float xScroll, yScroll;
				pContext->GetContentsScroll( xScroll, yScroll );
				pTransformPush->scroll_offset.x = xScroll;
				pTransformPush->scroll_offset.y = yScroll;

				// Add perspective into matrix
				if ( pContext->BHasTransformMatrix() )
				{
					VMatrix mat = pContext->GetTransformMatrix();
					if ( !mat.IsIdentity() )
					{
						float xTrans = pContext->GetTransformOriginX();
						float yTrans = pContext->GetTransformOriginY();

						mat = SetupMatrixTranslation( Vector( xTrans, yTrans, 0 ) ) * mat * SetupMatrixTranslation( Vector( -xTrans, -yTrans, 0 ) );

						m_pCurrentOperationContext->m_bUntransformed = false;

						VMatrix matPerspective = SetupMatrixTranslation( Vector( flPerspectiveX, flPerspectiveY, 0 ) );

						VMatrix matTemp = VMatrix::GetIdentityMatrix();
						matTemp.SetElement( 2, 3, -1.0f / flPerspective );
						matPerspective = matTemp * matPerspective;
						matPerspective = SetupMatrixTranslation( Vector( -flPerspectiveX, -flPerspectiveY, 0 ) ) * matPerspective;

						mat = matPerspective * mat;

						pTransformPush->transform = outputCommandList.AllocType< RenderMatrix4x4_t >();
						VMatrixToRenderMatrix( *pTransformPush->transform, mat );
					}
				}

				if ( pContext->BBorderWidthSet() )
				{
					// Push border data
					float rgBorderWidths[4];
					Color rgBorderColors[4];
					pContext->GetBorder( rgBorderWidths[0], rgBorderWidths[1], rgBorderWidths[2], rgBorderWidths[3], rgBorderColors[0], rgBorderColors[1], rgBorderColors[2], rgBorderColors[3] );

					pTransformPush->border = outputCommandList.AllocType< BorderData_t >();
					pTransformPush->border->top.style = k_EBorderStyleSolid;
					pTransformPush->border->top.width = rgBorderWidths[ 0 ];
					pTransformPush->border->top.color = rgBorderColors[ 0 ].GetRawColor();
					pTransformPush->border->right.style = k_EBorderStyleSolid;
					pTransformPush->border->right.width = rgBorderWidths[ 1 ];
					pTransformPush->border->right.color = rgBorderColors[ 1 ].GetRawColor();
					pTransformPush->border->bottom.style = k_EBorderStyleSolid;
					pTransformPush->border->bottom.width = rgBorderWidths[ 2 ];
					pTransformPush->border->bottom.color = rgBorderColors[ 2 ].GetRawColor();
					pTransformPush->border->left.style = k_EBorderStyleSolid;
					pTransformPush->border->left.width = rgBorderWidths[ 3 ];
					pTransformPush->border->left.color = rgBorderColors[ 3 ].GetRawColor();

					// If we had radius data we should be in the composition layer path
					Assert( !pContext->BBorderRadiusSet() );
				}

				if ( pContext->BBoxShadowSet() )
				{
					Color boxShadowColor;
					float flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance;
					bool bInset, bFill, bAnimating;
					pContext->GetBoxShadow( bInset, bFill, flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, boxShadowColor, bAnimating );

					pTransformPush->box_shadow = outputCommandList.AllocType< BoxShadowData_t >();
					pTransformPush->box_shadow->inset = bInset;
					pTransformPush->box_shadow->fill = bFill;
					pTransformPush->box_shadow->horizontal_offset = flHorOffset;
					pTransformPush->box_shadow->vertical_offset = flVerOffset;
					pTransformPush->box_shadow->blur_radius = flBlurRadius;
					pTransformPush->box_shadow->spread_distance = flSpreadDistance;
					pTransformPush->box_shadow->color = boxShadowColor.GetRawColor();
					pTransformPush->box_shadow->animating = bAnimating;
				}

				// Push with -max as zpos, so clip always occurs before any other drawing ops, and since our sort is stable and
				// we push right here at first push of a panels context, we know that will be true.
				RenderOperation_t *pClipOp = AllocPerFrameObject< RenderOperation_t >();
				pClipOp->m_flAvgZPos = -1.0 * FLT_MAX;
				pClipOp->m_pRenderCommand = pTransformPush;
				pClipOp->m_pParent = m_pCurrentOperationContext;
				pClipOp->m_pVecChildOperations = GetFreeRenderOpsVector();

				m_pCurrentOperationContext->m_pVecChildOperations->AddToTail( pClipOp );
			}
		}


		// Optionally, if we didn't push an actual compositing layer, then push a clip layer to clip within the compositing layer
		// for the current panels region/border radius.
		if ( pContext->BHasExplicitClipRect() )
		{
			pContext->SetClipLayerPushed( true );

			// Add the clip layer, since we didn't get our own composition layer that will clip for us
			PushClipLayerRenderCommand_t *pClipLayer = outputCommandList.AllocType< PushClipLayerRenderCommand_t >();

			pClipLayer->context_id = renderCommand.context_id;

			float flLeft, flTop, flRight, flBottom;
			pContext->GetExplicitClipRect( &flLeft, &flTop, &flRight, &flBottom );
			pClipLayer->top_left.x = vRenderPos.x + flLeft;
			pClipLayer->top_left.y = vRenderPos.y + flTop;
			pClipLayer->bottom_right.x = vRenderPos.x + flRight;
			pClipLayer->bottom_right.y = vRenderPos.y + flBottom;

			float rgRadii[8];
			pContext->GetCornerRadius( rgRadii[0], rgRadii[1], rgRadii[2], rgRadii[3], rgRadii[4], rgRadii[5], rgRadii[6], rgRadii[7] );

			pClipLayer->border_radius.top_left.horizontal = rgRadii[0];
			pClipLayer->border_radius.top_left.vertical = rgRadii[1];
			pClipLayer->border_radius.top_right.horizontal = rgRadii[2];
			pClipLayer->border_radius.top_right.vertical = rgRadii[3];
			pClipLayer->border_radius.bottom_right.horizontal = rgRadii[4];
			pClipLayer->border_radius.bottom_right.vertical = rgRadii[5];
			pClipLayer->border_radius.bottom_left.horizontal = rgRadii[6];
			pClipLayer->border_radius.bottom_left.vertical = rgRadii[7];

			// Push with -max as zpos, so clip always occurs before any other drawing ops, and since our sort is stable and
			// we push right here at first push of a panels context, we know that will be true.
			RenderOperation_t *pClipOp = AllocPerFrameObject< RenderOperation_t >();
			pClipOp->m_flAvgZPos = -1.0 * FLT_MAX;
			pClipOp->m_pRenderCommand = pClipLayer;
			pClipOp->m_pParent = m_pCurrentOperationContext;
			pClipOp->m_pVecChildOperations = GetFreeRenderOpsVector();

			m_pCurrentOperationContext->m_pVecChildOperations->AddToTail( pClipOp );
		}
		else if ( !pContext->BHasCompositionLayer() )
		{
			if ( !renderCommand.suppress_clip_to_bounds )
			{
				// Add the clip layer, since we didn't get our own composition layer that will clip for us
				pContext->SetClipLayerPushed( true );

				PushClipLayerRenderCommand_t *pClipLayer = outputCommandList.AllocType< PushClipLayerRenderCommand_t >();

				pClipLayer->context_id = renderCommand.context_id;

				pClipLayer->top_left.x = vRenderPos.x;
				pClipLayer->top_left.y = vRenderPos.y;
				pClipLayer->bottom_right.x = vRenderPos.x + renderCommand.width;
				pClipLayer->bottom_right.y = vRenderPos.y + renderCommand.height;

				float rgRadii[8];
				pContext->GetCornerRadius( rgRadii[0], rgRadii[1], rgRadii[2], rgRadii[3], rgRadii[4], rgRadii[5], rgRadii[6], rgRadii[7] );

				pClipLayer->border_radius.top_left.horizontal = rgRadii[0];
				pClipLayer->border_radius.top_left.vertical = rgRadii[1];
				pClipLayer->border_radius.top_right.horizontal = rgRadii[2];
				pClipLayer->border_radius.top_right.vertical = rgRadii[3];
				pClipLayer->border_radius.bottom_right.horizontal = rgRadii[4];
				pClipLayer->border_radius.bottom_right.vertical = rgRadii[5];
				pClipLayer->border_radius.bottom_left.horizontal = rgRadii[6];
				pClipLayer->border_radius.bottom_left.vertical = rgRadii[7];

				// Push with -max as zpos, so clip always occurs before any other drawing ops, and since our sort is stable and
				// we push right here at first push of a panels context, we know that will be true.
				RenderOperation_t *pClipOp = AllocPerFrameObject< RenderOperation_t >();
				pClipOp->m_flAvgZPos = -1.0 * FLT_MAX;
				pClipOp->m_pRenderCommand = pClipLayer;
				pClipOp->m_pParent = m_pCurrentOperationContext;
				pClipOp->m_pVecChildOperations = GetFreeRenderOpsVector();

				m_pCurrentOperationContext->m_pVecChildOperations->AddToTail( pClipOp );
			}
		}

		if ( eAnimatingContext != CUIAnimationEngine::k_EAnimatingFlag_NotAnimating )
		{
			MarkRenderOpAnimating( m_pCurrentOperationContext, (eAnimatingContext & k_EAnimatingFlag_CompositionOnly) == 0 );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called to indicate that this render operation is animating on this frame
//-----------------------------------------------------------------------------
void CUIAnimationEngine::MarkRenderOpAnimating( RenderOperation_t *pRenderOp, bool bCompositionOnly )
{
	// If we're animating a composition-only property, then skip marking ourselves as needing
	// redraw every frame. Only our parents will need to redraw every frame.
	if ( bCompositionOnly && pRenderOp )
	{
		pRenderOp = pRenderOp->m_pParent;
	}

	// Walk the parents looking for composition layers that need to be marked as redrawing every frame
	for ( ; pRenderOp; pRenderOp = pRenderOp->m_pParent )
	{
		if ( !pRenderOp->m_bIsLayerPush )
			continue;

		PushCompositingLayerRenderCommand_t *pPushLayer = static_cast< PushCompositingLayerRenderCommand_t * >( pRenderOp->m_pRenderCommand );

		// If the parent is already marked, assume we don't need to mark it or its ancestors again.
		if ( pPushLayer->needs_redraw_every_frame )
			break;

		pPushLayer->needs_redraw_every_frame = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called to pop previous animation/transform state
//-----------------------------------------------------------------------------
void CUIAnimationEngine::PopAnimationAndTransformContext( const PopAAndTContextRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::PopAnimationAndTransformContext", VPROF_BUDGETGROUP_TENFOOT );

	if ( m_unSkipUntilContextPopCounter > 0 )
	{
		m_unSkipUntilContextPopCounter--;
		return;
	}

	if ( m_AnimationAndTransformStack.Count() < 1 )
	{
		AssertMsg( false,  "Mismatched PopAnimationAndTransformContext, empty stack\n" );
		return;
	}

	CAnimationAndTransformContext *pContext = m_AnimationAndTransformStack[m_AnimationAndTransformStack.Tail()];
	if ( pContext->GetContext() != renderCommand.context_id )
	{
		AssertMsg( false,  "Mismatched PopAnimationAndTransformContext, non-matching context ids\n" );
		return;
	}

	Assert( m_pCurrentOperationContext );
	if ( m_pCurrentOperationContext )
	{
		// If the layer only had a layer push, then we don't need to push it at all, and instead of adding the pop we should just delete from parent
		// We also detect the case of a simple panel context that only has a clip context
		// pushed and finally a grouping operation that only has no-op children.
		bool bIsEmpty = false;
		if ( m_pCurrentOperationContext->m_bIsPanelContext )
		{
			int nEmptyCount = pContext->BHasClipLayerPushed() ? 1 : 0;
			bIsEmpty = m_pCurrentOperationContext->m_pVecChildOperations->Count() <= nEmptyCount;
			if ( !bIsEmpty )
			{
				bIsEmpty = true;
				FOR_EACH_VEC( *(m_pCurrentOperationContext->m_pVecChildOperations), i )
				{
					if ( !m_pCurrentOperationContext->m_pVecChildOperations->Element( i )->m_bDontDrawSelf )
					{
						bIsEmpty = false;
						break;
					}
				}
			}
		}
		if( bIsEmpty )
		{
			// The above comment isn't true though if the layer has a boxshadow, border, something else that would make it actually have contents
			if( !m_pCurrentOperationContext->m_bIsLayerPush )
			{
				m_pCurrentOperationContext->m_bDontDrawSelf = true;
			}
			else
			{
				PushCompositingLayerRenderCommand_t *pPushLayer = static_cast< PushCompositingLayerRenderCommand_t * >( m_pCurrentOperationContext->m_pRenderCommand );
				if( pPushLayer->box_shadow || pPushLayer->border || pPushLayer->offscreen_composition_layer )
				{
					// Still needs to draw as these attributes cause it to have actual contents
				}
				else
				{
					m_pCurrentOperationContext->m_bDontDrawSelf = true;
				}
			}
#if 0
			if( m_pCurrentOperationContext->m_bDontDrawSelf )
			{
				try
				{
					CPanelPtr< CUIPanel > ptr;
					ptr.SetFromUInt64( m_pCurrentOperationContext->m_ulPanelPtrValue );

					Msg( " Popping empty push/pop for %s %s\n", ptr->GetID(), ptr->GetPanelType().String() );

				}
				catch( ... ) {}
			}
#endif
		}

		if( pContext->BHasClipLayerPushed() )
		{
			// Pop the clip layer, which will have been pushed since we had no composition layer
			RenderOperation_t *pClip = AllocPerFrameObject< RenderOperation_t >();
			m_pCurrentOperationContext->m_pVecChildOperations->AddToTail( pClip );
			pClip->m_flAvgZPos = FLT_MAX;

			PopClipLayerRenderCommand_t *pPopClipLayer = outputCommandList.AllocType< PopClipLayerRenderCommand_t >();
			pClip->m_pRenderCommand = pPopClipLayer;
			pClip->m_pParent = m_pCurrentOperationContext;
			pClip->m_pVecChildOperations = GetFreeRenderOpsVector();
		}

		if ( pContext->BHasPanelContextPushedPushed() )
		{
			// Pop the transform matrix, which will have been pushed since we had no composition layer
			RenderOperation_t *pClip = AllocPerFrameObject< RenderOperation_t >();
			m_pCurrentOperationContext->m_pVecChildOperations->AddToTail( pClip );
			pClip->m_flAvgZPos = FLT_MAX;

			PopPanelContextInLayerRenderCommand_t *pPopPanelContext = outputCommandList.AllocType< PopPanelContextInLayerRenderCommand_t >();
			pClip->m_pRenderCommand = pPopPanelContext;
			pClip->m_pParent = m_pCurrentOperationContext;
			pClip->m_pVecChildOperations = GetFreeRenderOpsVector();
		}

		if( pContext->BHasCompositionLayer() )
		{
			// Push the pop for the compositing layer
			PopCompositingLayerRenderCommand_t *pPopCompositingLayer = outputCommandList.AllocType< PopCompositingLayerRenderCommand_t >();
			RenderOperation_t *pOperation = AllocPerFrameObject< RenderOperation_t >();
			m_pCurrentOperationContext->m_pVecChildOperations->AddToTail( pOperation );
			pOperation->m_flAvgZPos = FLT_MAX;
			pOperation->m_pRenderCommand = pPopCompositingLayer;
			pOperation->m_pParent = m_pCurrentOperationContext;
			pOperation->m_pVecChildOperations = GetFreeRenderOpsVector();
			pOperation->m_bIsLayerPop = true;
			pOperation->m_ulPanelPtrValue = pContext->GetContext();
		}

		// Reset ptr to current context to our parent, so new drawing happens above us
		Assert( m_pCurrentOperationContext->m_pParent );
		m_pCurrentOperationContext = m_pCurrentOperationContext->m_pParent;
	}

	FreePerFrameObject( pContext );
	m_AnimationAndTransformStack.Remove( m_AnimationAndTransformStack.Tail() );
}


//-----------------------------------------------------------------------------
// Purpose: Called at the start of paint background for each panel
//-----------------------------------------------------------------------------
void CUIAnimationEngine::BeginPaintBackground( const BeginPaintBackgroundRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList )
{
	if ( m_unSkipUntilContextPopCounter > 0 )
		return;

	Assert( m_pCurrentOperationContext );
	if( m_pCurrentOperationContext )
	{
		RenderOperation_t *pOperation = AllocPerFrameObject< RenderOperation_t >();;
		m_pCurrentOperationContext->m_pVecChildOperations->AddToTail( pOperation );
		pOperation->m_flAvgZPos = -1.0f * FLT_MAX;
		pOperation->m_pRenderCommand = NULL;
		pOperation->m_pParent = m_pCurrentOperationContext;
		pOperation->m_pVecChildOperations = GetFreeRenderOpsVector();
		m_pCurrentOperationContext = pOperation;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called at the end of paint background for each panel
//-----------------------------------------------------------------------------
void CUIAnimationEngine::EndPaintBackground( const EndPaintBackgroundRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList )
{
	if ( m_unSkipUntilContextPopCounter > 0 )
		return;

	Assert( m_pCurrentOperationContext );
	m_pCurrentOperationContext = m_pCurrentOperationContext->m_pParent;
}


//-----------------------------------------------------------------------------
// Purpose: Called at the start of paint last for each panel
//-----------------------------------------------------------------------------
void CUIAnimationEngine::BeginPaintLast( const BeginPaintLastRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList )
{
	if ( m_unSkipUntilContextPopCounter > 0 )
		return;

	Assert( m_pCurrentOperationContext );
	if( m_pCurrentOperationContext )
	{
		RenderOperation_t *pOperation = AllocPerFrameObject< RenderOperation_t >();;
		m_pCurrentOperationContext->m_pVecChildOperations->AddToTail( pOperation );
		pOperation->m_flAvgZPos = FLT_MAX;
		pOperation->m_pRenderCommand = NULL;
		pOperation->m_pParent = m_pCurrentOperationContext;
		pOperation->m_pVecChildOperations = GetFreeRenderOpsVector();
		m_pCurrentOperationContext = pOperation;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called at the end of paint last for each panel
//-----------------------------------------------------------------------------
void CUIAnimationEngine::EndPaintLast( const EndPaintLastRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList )
{
	if ( m_unSkipUntilContextPopCounter > 0 )
		return;

	Assert( m_pCurrentOperationContext );
	m_pCurrentOperationContext = m_pCurrentOperationContext->m_pParent;
}


//-----------------------------------------------------------------------------
// Purpose: Helper to get current time for property interpolation which may be now, or may be a stop time
//-----------------------------------------------------------------------------
float CUIAnimationEngine::GetFrameTimePropertyStoppedAt( uint32 hStyleSymbol )
{
	if ( !m_pVecStoppedTransitionsForCurrentPanel )
		return m_flCurrentFrameAnimationTime;

	FOR_EACH_VEC( (*m_pVecStoppedTransitionsForCurrentPanel), i )
	{
		StoppedTransitionProperty_t &prop = m_pVecStoppedTransitionsForCurrentPanel->Element( i );
		if ( prop.unStyleSymbol == hStyleSymbol )
			return prop.flTransitionStopTime;
	}

	return m_flCurrentFrameAnimationTime;
}


//-----------------------------------------------------------------------------
// Purpose: Perform spherical linear interpolation on quaternion data for animation
//-----------------------------------------------------------------------------
Quaternion CUIAnimationEngine::slerp( uint32 hStyleSymbol, const TransitionData_t &trans, const Quaternion &start, const Quaternion &end )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::slerp", VPROF_BUDGETGROUP_TENFOOT );

	float flNow = GetFrameTimePropertyStoppedAt( hStyleSymbol );

	Quaternion out = start;
	if ( trans.timing_func != k_EAnimationNone  )
	{
		if ( trans.duration_seconds < 0.0000001f )
		{
			out = end;
		}
		else
		{
			float flTimeProgress = ((flNow - trans.start_time) - trans.delay_seconds)
				/ trans.duration_seconds;
			flTimeProgress = clamp( flTimeProgress, 0.0f, 1.0f );

			Vector2D vec[4];
			if ( trans.timing_func != k_EAnimationCustomBezier )
				panorama::GetAnimationCurveControlPoints( trans.timing_func, vec );
			else
			{
				vec[0].x = 0.0f;
				vec[0].y = 0.0f;
				vec[1].x = trans.cubic_bezier_0;
				vec[1].y = trans.cubic_bezier_1;
				vec[2].x = trans.cubic_bezier_2;
				vec[2].y = trans.cubic_bezier_3;
				vec[3].x = 1.0f;
				vec[3].y = 1.0f;
			}

			CCubicBezierCurve< Vector2D > curve;
			curve.SetControlPoints( vec );

			flTimeProgress = GetProgressForTimingFunction( curve, flTimeProgress );

			QuaternionSlerp( start, end, flTimeProgress, out );
		}
	}

	return out;
}


//-----------------------------------------------------------------------------
// Purpose: Reverses the animation timing function value
//-----------------------------------------------------------------------------
EAnimationTimingFunction ReverseTimingFunctionNew( EAnimationTimingFunction eTimingFunction )
{
	if ( eTimingFunction == k_EAnimationEaseIn )
		return k_EAnimationEaseOut;
	else if ( eTimingFunction == k_EAnimationEaseOut )
		return k_EAnimationEaseIn;

	return eTimingFunction;
}


//-----------------------------------------------------------------------------
// Purpose: Determines if an animation is active. If active, returns the current frames and transition data for the animation
//-----------------------------------------------------------------------------
template < typename T >
bool BGetIndividualAnimationFrames( double flCurrentFrameTime, const typename PropertyWithTransition_t< T >::AnimationData_t &animation, TransitionData_t *pTransitionData, const T **ppPreviousFrameData, const T **ppCurrentFrameData )
{
	typedef typename PropertyWithTransition_t< T >::AnimationFrameData_t PropertyFrameData_t;

	VPROF_BUDGET_THREAD( "CUIAnimationEngine::BGetIndividualAnimationFrames", VPROF_BUDGETGROUP_TENFOOT );
	// check if we have animation data
	if ( animation.frames.IsEmpty() )
		return false;

	// calculate the total time for this animation	
	double flCurrent = flCurrentFrameTime;
	double flAnimationStart = animation.start_time + animation.delay_seconds;
	double flAnimationEnd = flAnimationStart + (animation.duration_seconds * animation.iteration);	// only valid if iteration != k_flFloatInfiniteIteration!!


	// check if the animation is still running

	if ( animation.fillMode == k_EAnimationFillModeNone )
	{
		if ( flCurrent < flAnimationStart || (animation.iteration != k_flFloatInfiniteIteration && flCurrent >= flAnimationEnd) )
			return false;
	}


	if ( (animation.fillMode == k_EAnimationFillModeBackwards) )
	{
		if ( animation.iteration != k_flFloatInfiniteIteration && flCurrent >= flAnimationEnd )
			return false;
	}

	if ( (animation.fillMode == k_EAnimationFillModeForwards) )
	{
		if ( flCurrent < flAnimationStart )
			return false;
	}



	if (animation.fillMode != k_EAnimationFillModeNone )
	{

		if ( ( animation.fillMode == k_EAnimationFillModeBoth ) || (animation.fillMode == k_EAnimationFillModeBackwards) )
		{
			if ( flCurrent < flAnimationStart )
			{
				flCurrent = flAnimationStart;
			}
		}
		
		if ( (animation.fillMode == k_EAnimationFillModeBoth) || (animation.fillMode == k_EAnimationFillModeForwards) )
		{
			if ( flCurrent > flAnimationEnd )
			{
				flCurrent = flAnimationEnd - 0.001;
			}
		}
	}
	
	// an animation is currently in progress. Determine which frame we should be in.
	float flIterations = (flCurrent - flAnimationStart) / animation.duration_seconds;
	float flIterationInt;
	float flIterationPercent = modff( flIterations, &flIterationInt );
	bool bEvenIteration = ((int)flIterationInt % 2) >= 1;		// >= because 0.* is iteration 1

	// if animation direction is alternate and we are on an even frame, we need to play the frame backwards
	EAnimationDirection eAnimationDirection = (EAnimationDirection)animation.direction;
	bool bReverse = false;
	if ( eAnimationDirection == k_EAnimationDirectionReverse )
		bReverse = true;
	else if ( bEvenIteration && eAnimationDirection == k_EAnimationDirectionAlternate )
		bReverse = true;
	else if ( !bEvenIteration && eAnimationDirection == k_EAnimationDirectionAlternateReverse )
		bReverse = true;

	if ( bReverse )
		flIterationPercent = 1.0f - flIterationPercent;

	// find frame data we are between. frames should already be sorted by percentage
	//Msg( "**ANIMATION: current=%f itpercent=%f iteration=%f\n", flCurrent, flIterationPercent, flIterationInt );

	const PropertyFrameData_t *pPreviousFrame = nullptr;
	const PropertyFrameData_t *pCurrentFrame = nullptr;

	const PropertyFrameData_t *pFirstFrame = animation.frames.GetFirst();
	const PropertyFrameData_t *pSecondFrame = animation.frames.GetNext( pFirstFrame );
	if ( pSecondFrame == nullptr )
	{
		// Only one frame
		pPreviousFrame = pFirstFrame;
		pCurrentFrame = pFirstFrame;
	}
	else
	{
		flIterationPercent *= 100.0f;	// bugbug cboyd - frame percents are 0-100.. change?

		pCurrentFrame = pFirstFrame;
		while ( pCurrentFrame )
		{
			float flFramePercent = pCurrentFrame->percent;
			//Msg( "**ANIMATION: reverse=%s itpercent=%f framepercent=%f\n", (bReverse ? "true" : "false"), flIterationPercent, flFramePercent );
			if ( flIterationPercent > flFramePercent )
			{
				pPreviousFrame = pCurrentFrame;
				pCurrentFrame = animation.frames.GetNext( pCurrentFrame );
				continue;
			}

			break;
		}

		if ( bReverse )
		{
			std::swap( pPreviousFrame, pCurrentFrame );
		}

		if ( pCurrentFrame == nullptr )
		{
			// Ran out of frames - just set everything to the final frame
			pCurrentFrame = pPreviousFrame;
		}
		else if ( pPreviousFrame == nullptr )
		{
			// Current frame is the first frame. So the previous frame is just the current frame
			pPreviousFrame = pCurrentFrame;
		}
	}

	Assert( pPreviousFrame != nullptr && pCurrentFrame != nullptr );

	// Get timing function. Each frame can optionally override
	EAnimationTimingFunction eTimingFunction = (EAnimationTimingFunction)animation.timing_func;
	if ( pCurrentFrame->timing_func != k_EAnimationUnset )
		eTimingFunction = pCurrentFrame->timing_func;

	// if necessary, invert timing function on reverse (ease-in becomes ease-out)
	if ( bReverse )
		eTimingFunction = ReverseTimingFunctionNew( eTimingFunction );

	// need transition data between frames
	float flStartPrevFrame = animation.duration_seconds * ( bReverse ? 100.0f - pPreviousFrame->percent : pPreviousFrame->percent ) / 100.0f;
	pTransitionData->start_time = flAnimationStart + ( flIterationInt * animation.duration_seconds ) + flStartPrevFrame;
	pTransitionData->delay_seconds = 0.0f;
	pTransitionData->duration_seconds = fabs( ( pCurrentFrame->percent - pPreviousFrame->percent ) / 100.0f * animation.duration_seconds );
	pTransitionData->timing_func = eTimingFunction;
	if ( eTimingFunction == k_EAnimationCustomBezier )
	{
		pTransitionData->cubic_bezier_0 = pCurrentFrame->cubic_bezier_0;
		pTransitionData->cubic_bezier_1 = pCurrentFrame->cubic_bezier_1;
		pTransitionData->cubic_bezier_2 = pCurrentFrame->cubic_bezier_2;
		pTransitionData->cubic_bezier_3 = pCurrentFrame->cubic_bezier_3;
	}

	*ppPreviousFrameData = &pPreviousFrame->data;
	*ppCurrentFrameData = &pCurrentFrame->data;

	//Msg( "**ANIMATION: icur=%d iprev=%d start=%f dur=%f etiming=%d\n", iCurrentFrame, iPreviousFrame, pTransitionData->start_time(), pTransitionData->duration_seconds(), eTimingFunction );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: For all provided animations, determines if an animation is active. 
// If active, returns the current frames and transition data for the animation
//-----------------------------------------------------------------------------
template < class T >
bool BGetAnimationFrames( double flCurrentFrameTime, const CRenderDataList< typename PropertyWithTransition_t< T >::AnimationData_t > &animations, TransitionData_t *pTransitionData, const T **ppPreviousFrameData, const T **ppCurrentFrameData )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::BGetAnimationFrames", VPROF_BUDGETGROUP_TENFOOT );
	// check if we have animation data
	if ( animations.IsEmpty() )
		return false;

	// loop each animation, searching for the last animation that is active. If multiple animations are active for this property, the last one wins
	for ( const typename PropertyWithTransition_t< T >::AnimationData_t *pAnimationData : animations )
	{
		const T *pPreviousFrameData = nullptr;
		const T *pCurrentFrameData = nullptr;
		if ( BGetIndividualAnimationFrames( flCurrentFrameTime, *pAnimationData, pTransitionData, &pPreviousFrameData, &pCurrentFrameData ) )
		{
			*ppPreviousFrameData = pPreviousFrameData;
			*ppCurrentFrameData = pCurrentFrameData;
		}
	}

	return *ppPreviousFrameData != nullptr && *ppCurrentFrameData != nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Push a 3D transform operation into the current context
//-----------------------------------------------------------------------------
template< class T >
void CUIAnimationEngine::GetRenderCmdFrameData( RenderCmdFrameData_t< T > *pData, const PropertyWithTransition_t< T > &renderCommand )
{
	pData->hStyleProperty = renderCommand.style_symbol;
	pData->base = nullptr;
	pData->transition = nullptr;
	pData->pTransitionData = NULL;

	// If there is an animation running, use that for calculation
	const T *pPreviousFrameData = nullptr;
	const T *pCurrentFrameData = nullptr;
	if ( BGetAnimationFrames( m_flCurrentFrameAnimationTime, renderCommand.animations, &pData->animationTransitionData, &pPreviousFrameData, &pCurrentFrameData ) )
	{
		// Clear hStyleProperty since we don't want to use to stop animations... hacky
		pData->hStyleProperty = STYLE_SYMBOL_INVALID;
		pData->pTransitionData = &pData->animationTransitionData;
		pData->base = pPreviousFrameData;
		pData->transition = pCurrentFrameData;
	}
	else
	{
		// no animation data
		pData->pTransitionData = renderCommand.transition_data;
		pData->base = &renderCommand.base;
		pData->transition = &renderCommand.transition;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Push a 3D transform operation into the current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::Push3DTransformMatrix( CAnimationAndTransformContext *pContext, const TransformMatrixWithTransition_t &command )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::Push3DTransformMatrix", VPROF_BUDGETGROUP_TENFOOT );
	
	RenderCmdFrameData_t< RenderMatrix4x4_t > data;
	GetRenderCmdFrameData( &data, command );
	
	// our pointers are set up
	VMatrix matrix;
	RenderMatrixToVMatrix( matrix, *data.base );

	// If there is a transition, we need to decompose and interpolate various matrix components...
	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		VMatrix matTransition;
		RenderMatrixToVMatrix( matTransition, *data.transition );

		float flTimeProgress = GetTimingTransitionProgress( data.hStyleProperty, data.pTransitionData );
		if ( flTimeProgress != 1.0f )
		{
			eAnimating = k_EAnimatingFlag_CompositionOnly;
		}
		VMatrix matrixRecomposed = InterpolateTransformMatrix( matrix, matTransition, flTimeProgress );
		pContext->SetTransformMatrix( matrixRecomposed );
	}
	else
	{
		pContext->SetTransformMatrix( matrix );
	}

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push a composition color into the current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::PushWashColor( CAnimationAndTransformContext *pContext, const WashColorWithTransition_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::PushWashColor", VPROF_BUDGETGROUP_TENFOOT );

	RenderCmdFrameData_t< WashColor_t > data;
	GetRenderCmdFrameData( &data, renderCommand );

	Color color;
	color.SetRawColor( data.base->m_color );
	
	float r = color.r();
	float g = color.g();
	float b = color.b();
	float a = color.a();

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		Color colorTransition;
		colorTransition.SetRawColor( data.transition->m_color );

		float rt = colorTransition.r();
		float gt = colorTransition.g();
		float bt = colorTransition.b();
		float at = colorTransition.a();

		color.SetColor( clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, r, rt, eAnimating, k_EAnimatingFlag_CompositionOnly ) ), 0, 255 ),
			clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, g, gt, eAnimating, k_EAnimatingFlag_CompositionOnly ) ), 0, 255 ),
			clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, b, bt, eAnimating, k_EAnimatingFlag_CompositionOnly ) ), 0, 255 ),
			clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, a, at, eAnimating, k_EAnimatingFlag_CompositionOnly ) ), 0, 255 ) );
	}

	pContext->SetCompositionColor( color, data.base->m_bFast );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push a centered 2D scaling transform into the current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::Push2DScale( CAnimationAndTransformContext *pContext, const Scale2DWithTransition_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::Push2DScale", VPROF_BUDGETGROUP_TENFOOT );

	RenderCmdFrameData_t< RenderPoint2D_t > data;
	GetRenderCmdFrameData( &data, renderCommand );

	float x, y;
	x = data.base->x;
	y = data.base->y;

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		x = std::max<float>( 0.0f, lerp( data.hStyleProperty, *data.pTransitionData, x, (float)data.transition->x, eAnimating, k_EAnimatingFlag_CompositionOnly ) );
		y = std::max<float>( 0.0f, lerp( data.hStyleProperty, *data.pTransitionData, y, (float)data.transition->y, eAnimating, k_EAnimatingFlag_CompositionOnly ) );
	}

	pContext->SetScale2DFactors( x, y );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push a centered 2D rotate transform into the current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::Push2DRotate( CAnimationAndTransformContext *pContext, const Rotate2DWithTransition_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::Push2DRotate", VPROF_BUDGETGROUP_TENFOOT );

	RenderCmdFrameData_t< float > data;
	GetRenderCmdFrameData( &data, renderCommand );

	float flDegrees = (float)(*data.base);

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		flDegrees = lerp( data.hStyleProperty, *data.pTransitionData, flDegrees, (float)(*data.transition), eAnimating, k_EAnimatingFlag_CompositionOnly );
	}

	pContext->SetRotate2D( flDegrees );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push a gaussian blur into the current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::PushGaussianBlur( CAnimationAndTransformContext *pContext, const GaussianBlurWithTransition_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::PushGaussianBlur", VPROF_BUDGETGROUP_TENFOOT );

	RenderCmdFrameData_t< GaussianValues_t > data;
	GetRenderCmdFrameData( &data, renderCommand );

	float flStdDevHor = data.base->stddev_hor;
	float flStdDevVer = data.base->stddev_ver;
	float flPasses = data.base->passes;
	BlurType_t blurType = data.base->blurType;

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		flStdDevHor = lerp( data.hStyleProperty, *data.pTransitionData, flStdDevHor, (float)data.transition->stddev_hor, eAnimating, k_EAnimatingFlag_CompositionOnly );
		flStdDevVer = lerp( data.hStyleProperty, *data.pTransitionData, flStdDevVer, (float)data.transition->stddev_ver, eAnimating, k_EAnimatingFlag_CompositionOnly );
		flPasses = clamp( lerp( data.hStyleProperty, *data.pTransitionData, flPasses, (float)data.transition->passes, eAnimating, k_EAnimatingFlag_CompositionOnly ), 0.0f, 100.0f );

		if ( blurType == BT_FASTANIM )
		{
			float progress;
			if ( data.base->passes <= data.transition->passes )
			{
				progress = (flPasses - data.base->passes) / (data.transition->passes - data.base->passes);
				if ( progress > 0.999f ) progress = 0.999f;
				flPasses = (data.transition->passes) + 1.0f - progress * 0.5f;
			}
			else
			{
				progress = (data.base->passes - flPasses) / (data.base->passes - data.transition->passes);
				if ( progress > 0.999f ) progress = 0.999f;
				flPasses = (data.base->passes) + 0.5f + progress * 0.5f;
			}
		}

	}

	pContext->SetBlurStdDeviationHor( flStdDevHor );
	pContext->SetBlurStdDeviationVer( flStdDevVer );
	pContext->SetBlurPasses( flPasses );
	pContext->SetBlurType( blurType );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push a hue shift into the current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::PushHueShift( CAnimationAndTransformContext *pContext, const HueShiftWithTransition_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::PushHueShift", VPROF_BUDGETGROUP_TENFOOT );
	
	RenderCmdFrameData_t< float > data;
	GetRenderCmdFrameData( &data, renderCommand );

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	float flHueShift = *data.base;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		flHueShift = lerp( data.hStyleProperty, *data.pTransitionData, *data.base, *data.transition, eAnimating, k_EAnimatingFlag_CompositionOnly );
	}
	pContext->SetHueShift( flHueShift );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push a saturation into the current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::PushSaturation( CAnimationAndTransformContext *pContext, const SaturationWithTransition_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::PushSaturation", VPROF_BUDGETGROUP_TENFOOT );
	
	RenderCmdFrameData_t< float > data;
	GetRenderCmdFrameData( &data, renderCommand );

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	float flSaturation = *data.base;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		flSaturation = lerp( data.hStyleProperty, *data.pTransitionData, *data.base, *data.transition, eAnimating, k_EAnimatingFlag_CompositionOnly );
	}
	pContext->SetSaturation( flSaturation );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push a brightness into the current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::PushBrightness( CAnimationAndTransformContext *pContext, const BrightnessWithTransition_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::PushBrightness", VPROF_BUDGETGROUP_TENFOOT );
	
	RenderCmdFrameData_t< float > data;
	GetRenderCmdFrameData( &data, renderCommand );

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	float flBrightness = *data.base;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		flBrightness = lerp( data.hStyleProperty, *data.pTransitionData, *data.base, *data.transition, eAnimating, k_EAnimatingFlag_CompositionOnly );
	}
	pContext->SetBrightness( flBrightness );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push a contrast into the current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::PushContrast( CAnimationAndTransformContext *pContext, const ContrastWithTransition_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::PushContrast", VPROF_BUDGETGROUP_TENFOOT );
	
	RenderCmdFrameData_t< float > data;
	GetRenderCmdFrameData( &data, renderCommand );

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	float flContrast = *data.base;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		flContrast = lerp( data.hStyleProperty, *data.pTransitionData, *data.base, *data.transition, eAnimating, k_EAnimatingFlag_CompositionOnly );
	}
	pContext->SetContrast( flContrast );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push opacity mask into the current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::PushOpacityMask( CAnimationAndTransformContext *pContext, const OpacityMaskWithTransition_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::PushOpacityMask", VPROF_BUDGETGROUP_TENFOOT );

	RenderCmdFrameData_t< OpacityMask_t > data;
	GetRenderCmdFrameData( &data, renderCommand );

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	float flOpacity = data.base->opacity_mask_opacity;
	IUITexture *pTexture = data.base->opacity_mask_texture.GetTexture();

	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		IUITexture *pTextureTransition = data.transition->opacity_mask_texture.GetTexture();

		if ( pTextureTransition == nullptr )
		{
			// Fade out
			flOpacity = lerp( data.hStyleProperty, *data.pTransitionData, flOpacity, (float)0.0f, eAnimating, k_EAnimatingFlag_CompositionOnly );
		}
		else if ( pTexture == nullptr )
		{
			// Fade in
			pTexture = pTextureTransition;
			flOpacity = clamp( lerp( data.hStyleProperty, *data.pTransitionData, 0.0f, (float)data.transition->opacity_mask_opacity, eAnimating, k_EAnimatingFlag_CompositionOnly ), 0.0f, 1.0f );
		}
		else 
		{
			// Fade between values, textures should really match as we don't support a cross fade of two at once
			pTexture = pTextureTransition;
			flOpacity = clamp( lerp( data.hStyleProperty, *data.pTransitionData, flOpacity, (float)data.transition->opacity_mask_opacity, eAnimating, k_EAnimatingFlag_CompositionOnly ), 0.0f, 1.0f );
		}
	}
	
	pContext->SetOpacityMaskTexture( pTexture, flOpacity );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push opacity into the current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::PushOpacity( CAnimationAndTransformContext *pContext, const OpacityWithTransition_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::PushOpacity", VPROF_BUDGETGROUP_TENFOOT );

	RenderCmdFrameData_t< float > data;
	GetRenderCmdFrameData( &data, renderCommand );

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	float flOpacity = *data.base;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		flOpacity = clamp( lerp( data.hStyleProperty, *data.pTransitionData, flOpacity, (float)*data.transition, eAnimating, k_EAnimatingFlag_CompositionOnly ), 0.0f, 1.0f );
	}
	pContext->SetOpacity( flOpacity );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push a 3D transform perspective operation into the current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::Push3DTransformPerspective( CAnimationAndTransformContext *pContext, const TransformPerspectiveWithTransition_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::Push3DTransformPerspective", VPROF_BUDGETGROUP_TENFOOT );
	
	RenderCmdFrameData_t< float > data;
	GetRenderCmdFrameData( &data, renderCommand );

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	float flPerspective = *data.base;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		flPerspective = lerp( data.hStyleProperty, *data.pTransitionData, flPerspective, (float)*data.transition, eAnimating, k_EAnimatingFlag_CompositionOnly );
	}
	pContext->SetPerspective( flPerspective );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push a 3D transform perspective origin operation into the current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::Push3DTransformPerspectiveOrigin( CAnimationAndTransformContext *pContext, const TransformPerspectiveOriginWithTransition_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::Push3DTransformPerspectiveOrigin", VPROF_BUDGETGROUP_TENFOOT );
	
	RenderCmdFrameData_t< RenderPoint_t > data;
	GetRenderCmdFrameData( &data, renderCommand );
	
	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	float x = data.base->x;
	float y = data.base->y;

	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		x = lerp( data.hStyleProperty, *data.pTransitionData, x, (float)data.transition->x, eAnimating, k_EAnimatingFlag_CompositionOnly );
		y = lerp( data.hStyleProperty, *data.pTransitionData, y, (float)data.transition->y, eAnimating, k_EAnimatingFlag_CompositionOnly );
	}
	
	pContext->SetPerspectiveOrigin( x, y );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push a 3D transform origin operation into the current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::Push3DTransformOrigin( CAnimationAndTransformContext *pContext, const TransformOriginWithTransition_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::Push3DTransformOrigin", VPROF_BUDGETGROUP_TENFOOT );

	RenderCmdFrameData_t< TransformOriginData_t > data;
	GetRenderCmdFrameData( &data, renderCommand );

	float flWidth, flHeight;
	pContext->GetSize( flWidth, flHeight );

	float xOffset, yOffset, zOffset;
	pContext->GetPosition( xOffset, yOffset, zOffset );

	float flParentWidth = flWidth;
	float flParentHeight = flHeight;

	if ( m_AnimationAndTransformStack.Count() > 1 )
		m_AnimationAndTransformStack[ m_AnimationAndTransformStack.Count() - 2 ]->GetSize( flParentWidth, flParentHeight );

	float x = data.base->x;
	float y = data.base->y;

	if ( data.base->x_is_percent )
	{
		if ( data.base->is_parent_relative )
			x = flParentWidth * x/100.0f;
		else
			x = flWidth * x/100.0f;
	}

	if ( data.base->y_is_percent )
	{
		if ( data.base->is_parent_relative )
			y = flParentHeight * y/100.0f;
		else
			y = flHeight * y/100.0f;
	}

	if ( !data.base->is_parent_relative )
	{
		x += xOffset;
		y += yOffset;
	}
	
	float xTransition = (float)data.transition->x;
	float yTransition = (float)data.transition->y;

	if ( data.transition->x_is_percent )
	{
		if ( data.transition->is_parent_relative )
			xTransition = flParentWidth * xTransition;
		else
			xTransition = flWidth * xTransition;
	}

	if ( data.transition->y_is_percent )
	{
		if ( data.transition->is_parent_relative )
			yTransition = flParentHeight * y;
		else
			yTransition = flHeight * y;
	}

	if ( !data.transition->is_parent_relative )
	{
		xTransition += xOffset;
		yTransition += yOffset;
	}

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		x = lerp( data.hStyleProperty, *data.pTransitionData, x, xTransition, eAnimating, k_EAnimatingFlag_CompositionOnly );
		y = lerp( data.hStyleProperty, *data.pTransitionData, y, yTransition, eAnimating, k_EAnimatingFlag_CompositionOnly );
	}

	float xOff, yOff, zOff;
	GetCurrentDrawingOffsets( xOff, yOff, zOff, true );

	pContext->SetTransformOrigin( x, y, xOff, yOff );

	return eAnimating;
}

//-----------------------------------------------------------------------------
// Purpose: Push border message into current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::PushBorder( CAnimationAndTransformContext *pContext, const BorderWithTransition_t &renderCommand )
{
	RenderCmdFrameData_t< BorderData_t > data;
	GetRenderCmdFrameData( &data, renderCommand );

	float rgWidths[4];
	Color rgColors[4];

	if ( (EBorderStyle)data.base->top.style == k_EBorderStyleSolid )
		rgWidths[0] = data.base->top.width;
	else
		rgWidths[0] = 0.0f;

	if ( (EBorderStyle)data.base->right.style == k_EBorderStyleSolid )
		rgWidths[1] = data.base->right.width;
	else
		rgWidths[1] = 0.0f;

	if ( (EBorderStyle)data.base->bottom.style == k_EBorderStyleSolid )
		rgWidths[2] = data.base->bottom.width;
	else
		rgWidths[2] = 0.0f;

	if ( (EBorderStyle)data.base->left.style == k_EBorderStyleSolid )
		rgWidths[3] = data.base->left.width;
	else
		rgWidths[3] = 0.0f;

	rgColors[0].SetRawColor( data.base->top.color );
	rgColors[1].SetRawColor( data.base->right.color );
	rgColors[2].SetRawColor( data.base->bottom.color );
	rgColors[3].SetRawColor( data.base->left.color );

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		float rgTargetWidths[4];
		Color rgTargetColors[4];

		if ( (EBorderStyle)data.transition->top.style == k_EBorderStyleSolid )
			rgTargetWidths[0] = data.transition->top.width;
		else
			rgTargetWidths[0] = 0.0f;

		if ( (EBorderStyle)data.transition->right.style == k_EBorderStyleSolid )
			rgTargetWidths[1] = data.transition->right.width;
		else
			rgTargetWidths[1] = 0.0f;

		if ( (EBorderStyle)data.transition->bottom.style == k_EBorderStyleSolid )
			rgTargetWidths[2] = data.transition->bottom.width;
		else
			rgTargetWidths[2] = 0.0f;

		if ( (EBorderStyle)data.transition->left.style == k_EBorderStyleSolid )
			rgTargetWidths[3] = data.transition->left.width;
		else
			rgTargetWidths[3] = 0.0f;

		rgTargetColors[0].SetRawColor( data.transition->top.color );
		rgTargetColors[1].SetRawColor( data.transition->right.color );
		rgTargetColors[2].SetRawColor( data.transition->bottom.color );
		rgTargetColors[3].SetRawColor( data.transition->left.color );
		

		for( int i=0; i < 4; ++i )
		{
			rgWidths[i] = MAX( 0.0f, lerp( data.hStyleProperty, *data.pTransitionData, rgWidths[i], rgTargetWidths[i], eAnimating, k_EAnimatingFlag_Repaint ) );

			float r,g,b,a, tr,tg,tb,ta;
			r = rgColors[i].r();
			g = rgColors[i].g();
			b = rgColors[i].b();
			a = rgColors[i].a();

			tr = rgTargetColors[i].r();
			tg = rgTargetColors[i].g();
			tb = rgTargetColors[i].b();
			ta = rgTargetColors[i].a();

			rgColors[i].SetColor(
				clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, r, tr, eAnimating, k_EAnimatingFlag_Repaint ) ), 0, 255 ),
				clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, g, tg, eAnimating, k_EAnimatingFlag_Repaint ) ), 0, 255 ),
				clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, b, tb, eAnimating, k_EAnimatingFlag_Repaint ) ), 0, 255 ),
				clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, a, ta, eAnimating, k_EAnimatingFlag_Repaint ) ), 0, 255 ) );
		}
	}

	pContext->SetBorder( rgWidths[0], rgWidths[1], rgWidths[2], rgWidths[3], rgColors[0], rgColors[1], rgColors[2], rgColors[3] );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push box shadow message into current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::PushBoxShadow( CAnimationAndTransformContext *pContext, const BoxShadowWithTransition_t &renderCommand )
{
	RenderCmdFrameData_t< BoxShadowData_t > data;
	GetRenderCmdFrameData( &data, renderCommand );

	bool bInset = data.base->inset;
	bool bFill = data.base->fill;
	float flHorOffset = data.base->horizontal_offset;
	float flVerOffset = data.base->vertical_offset;
	float flBlurRadius = data.base->blur_radius;
	float flSpreadDistance = data.base->spread_distance;
	Color col;
	col.SetRawColor( data.base->color );

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		if ( data.transition->inset != bInset || data.transition->fill != bFill )
		{
			// can't transition inset vs offset, or fill vs no fill
			bInset = data.transition->inset;
			flHorOffset = data.transition->horizontal_offset;
			flVerOffset = data.transition->vertical_offset;
			flBlurRadius = data.transition->blur_radius;
			flSpreadDistance = data.transition->spread_distance;
			col.SetRawColor( data.transition->color );
		}
		else
		{
			eAnimating = CUIAnimationEngine::k_EAnimatingFlag_CompositionOnly;

			Color colTarget;
			colTarget.SetRawColor( data.transition->color );

			flHorOffset = lerp( data.hStyleProperty, *data.pTransitionData, flHorOffset, (float)data.transition->horizontal_offset, eAnimating, k_EAnimatingFlag_CompositionOnly );
			flVerOffset = lerp( data.hStyleProperty, *data.pTransitionData, flVerOffset, (float)data.transition->vertical_offset, eAnimating, k_EAnimatingFlag_CompositionOnly );
			flBlurRadius = lerp( data.hStyleProperty, *data.pTransitionData, flBlurRadius, (float)data.transition->blur_radius, eAnimating, k_EAnimatingFlag_CompositionOnly );
			flSpreadDistance = lerp( data.hStyleProperty, *data.pTransitionData, flSpreadDistance, (float)data.transition->spread_distance, eAnimating, k_EAnimatingFlag_CompositionOnly );

			float r,g,b,a, tr,tg,tb,ta;
			r = col.r();
			g = col.g();
			b = col.b();
			a = col.a();

			tr = colTarget.r();
			tg = colTarget.g();
			tb = colTarget.b();
			ta = colTarget.a();

			col.SetColor(
				clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, r, tr, eAnimating, k_EAnimatingFlag_CompositionOnly ) ), 0, 255 ),
				clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, g, tg, eAnimating, k_EAnimatingFlag_CompositionOnly ) ), 0, 255 ),
				clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, b, tb, eAnimating, k_EAnimatingFlag_CompositionOnly ) ), 0, 255 ),
				clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, a, ta, eAnimating, k_EAnimatingFlag_CompositionOnly ) ), 0, 255 ) );
		}
	}

	pContext->SetBoxShadow( bInset, bFill, flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, col, eAnimating != CUIAnimationEngine::k_EAnimatingFlag_NotAnimating );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push text shadow message into current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::PushTextShadow( CAnimationAndTransformContext *pContext, const TextShadowWithTransition_t &renderCommand )
{
	RenderCmdFrameData_t< TextShadowData_t > data;
	GetRenderCmdFrameData( &data, renderCommand );

	float flHorOffset = data.base->horizontal_offset;
	float flVerOffset = data.base->vertical_offset;
	float flBlurRadius = data.base->blur_radius;
	float flStrength = data.base->strength;
	Color col;
	col.SetRawColor( data.base->color );

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		eAnimating = k_EAnimatingFlag_Repaint;

		Color colTarget;
		colTarget.SetRawColor( data.transition->color );

		flHorOffset = lerp( data.hStyleProperty, *data.pTransitionData, flHorOffset, (float)data.transition->horizontal_offset, eAnimating, k_EAnimatingFlag_Repaint );
		flVerOffset = lerp( data.hStyleProperty, *data.pTransitionData, flVerOffset, (float)data.transition->vertical_offset, eAnimating, k_EAnimatingFlag_Repaint );
		flBlurRadius = lerp( data.hStyleProperty, *data.pTransitionData, flBlurRadius, (float)data.transition->blur_radius, eAnimating, k_EAnimatingFlag_Repaint );
		flStrength = lerp( data.hStyleProperty, *data.pTransitionData, flStrength, (float)data.transition->strength, eAnimating, k_EAnimatingFlag_Repaint );

		float r, g, b, a, tr, tg, tb, ta;
		r = col.r();
		g = col.g();
		b = col.b();
		a = col.a();

		tr = colTarget.r();
		tg = colTarget.g();
		tb = colTarget.b();
		ta = colTarget.a();

		col.SetColor(
			clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, r, tr, eAnimating, k_EAnimatingFlag_Repaint ) ), 0, 255 ),
			clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, g, tg, eAnimating, k_EAnimatingFlag_Repaint ) ), 0, 255 ),
			clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, b, tb, eAnimating, k_EAnimatingFlag_Repaint ) ), 0, 255 ),
			clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, a, ta, eAnimating, k_EAnimatingFlag_Repaint ) ), 0, 255 ) );
	}

	pContext->SetTextShadow( flHorOffset, flVerOffset, flBlurRadius, flStrength, col, eAnimating != CUIAnimationEngine::k_EAnimatingFlag_NotAnimating );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push text shadow message into current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::PushImageShadow( CAnimationAndTransformContext *pContext, const ImageShadowWithTransition_t &renderCommand )
{
	RenderCmdFrameData_t< ImageShadowData_t > data;
	GetRenderCmdFrameData( &data, renderCommand );

	float flHorOffset = data.base->horizontal_offset;
	float flVerOffset = data.base->vertical_offset;
	float flBlurRadius = data.base->blur_radius;
	float flStrength = data.base->strength;
	Color col;
	col.SetRawColor( data.base->color );

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		eAnimating = k_EAnimatingFlag_Repaint;

		Color colTarget;
		colTarget.SetRawColor( data.transition->color );

		flHorOffset = lerp( data.hStyleProperty, *data.pTransitionData, flHorOffset, ( float )data.transition->horizontal_offset, eAnimating, k_EAnimatingFlag_Repaint );
		flVerOffset = lerp( data.hStyleProperty, *data.pTransitionData, flVerOffset, ( float )data.transition->vertical_offset, eAnimating, k_EAnimatingFlag_Repaint );
		flBlurRadius = lerp( data.hStyleProperty, *data.pTransitionData, flBlurRadius, ( float )data.transition->blur_radius, eAnimating, k_EAnimatingFlag_Repaint );
		flStrength = lerp( data.hStyleProperty, *data.pTransitionData, flStrength, ( float )data.transition->strength, eAnimating, k_EAnimatingFlag_Repaint );

		float r, g, b, a, tr, tg, tb, ta;
		r = col.r();
		g = col.g();
		b = col.b();
		a = col.a();

		tr = colTarget.r();
		tg = colTarget.g();
		tb = colTarget.b();
		ta = colTarget.a();

		col.SetColor(
			clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, r, tr, eAnimating, k_EAnimatingFlag_Repaint ) ), 0, 255 ),
			clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, g, tg, eAnimating, k_EAnimatingFlag_Repaint ) ), 0, 255 ),
			clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, b, tb, eAnimating, k_EAnimatingFlag_Repaint ) ), 0, 255 ),
			clamp( RoundFloatToInt( lerp( data.hStyleProperty, *data.pTransitionData, a, ta, eAnimating, k_EAnimatingFlag_Repaint ) ), 0, 255 ) );
	}

	pContext->SetImageShadow( flHorOffset, flVerOffset, flBlurRadius, flStrength, col, eAnimating != CUIAnimationEngine::k_EAnimatingFlag_NotAnimating );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push clipping rect/radial into the current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::PushClip( CAnimationAndTransformContext *pContext, const ClipWithTransition_t &renderCommand )
{
	RenderCmdFrameData_t< ClipData_t > data;
	GetRenderCmdFrameData( &data, renderCommand );

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;

	if ( data.base->rect_clip )
	{
		float flLeft = data.base->rect_clip->left;
		float flTop = data.base->rect_clip->top;
		float flRight = data.base->rect_clip->right;
		float flBottom = data.base->rect_clip->bottom;

		if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
		{
			if ( data.transition->rect_clip )
			{
				flLeft = lerp( data.hStyleProperty, *data.pTransitionData, flLeft, ( float )data.transition->rect_clip->left, eAnimating, k_EAnimatingFlag_Repaint );
				flTop = lerp( data.hStyleProperty, *data.pTransitionData, flTop, ( float )data.transition->rect_clip->top, eAnimating, k_EAnimatingFlag_Repaint );
				flRight = lerp( data.hStyleProperty, *data.pTransitionData, flRight, ( float )data.transition->rect_clip->right, eAnimating, k_EAnimatingFlag_Repaint );
				flBottom = lerp( data.hStyleProperty, *data.pTransitionData, flBottom, ( float )data.transition->rect_clip->bottom, eAnimating, k_EAnimatingFlag_Repaint );
				pContext->SetExplicitClipRect( flLeft, flTop, flRight, flBottom );
			}
			// NOTE: if transitioning to a state without any clip, don't apply clip for duration of transition
		}
		else
		{
			pContext->SetExplicitClipRect( flLeft, flTop, flRight, flBottom );
		}
	}

	if ( data.base->radial_clip )
	{
		const RadialClipData_t &radial = *data.base->radial_clip;
		float flRadialCenterX = radial.center_x;
		float flRadialCenterY = radial.center_y;
		float flRadialStartAngle = radial.start_angle;
		float flRadialSectorAngle = radial.sector_angle;

		if( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
		{
			if ( data.transition->radial_clip ) 
			{
				const RadialClipData_t &transitionRadial = *data.transition->radial_clip;
				flRadialCenterX = lerp( data.hStyleProperty, *data.pTransitionData, flRadialCenterX, ( float )transitionRadial.center_x, eAnimating, k_EAnimatingFlag_Repaint );
				flRadialCenterY = lerp( data.hStyleProperty, *data.pTransitionData, flRadialCenterY, ( float )transitionRadial.center_y, eAnimating, k_EAnimatingFlag_Repaint );
				flRadialStartAngle = lerp( data.hStyleProperty, *data.pTransitionData, flRadialStartAngle, ( float )transitionRadial.start_angle, eAnimating, k_EAnimatingFlag_Repaint );
				flRadialSectorAngle = lerp( data.hStyleProperty, *data.pTransitionData, flRadialSectorAngle, ( float )transitionRadial.sector_angle, eAnimating, k_EAnimatingFlag_Repaint );
				pContext->SetRadialClip( flRadialCenterX, flRadialCenterY, flRadialStartAngle, flRadialSectorAngle );
			}
			// NOTE: if transitioning to a state without any clip, don't apply clip for duration of transition
		}
		else
		{
			pContext->SetRadialClip( flRadialCenterX, flRadialCenterY, flRadialStartAngle, flRadialSectorAngle );
		}
	}

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push border radius message into current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::PushBorderRadius( CAnimationAndTransformContext *pContext, const BorderRadiusWithTransition_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::PushBorderRadius", VPROF_BUDGETGROUP_TENFOOT );
	
	RenderCmdFrameData_t< RadiusData_t > data;
	GetRenderCmdFrameData( &data, renderCommand );

	float rgCornerRadii[8] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	float rgCornerRadiiTarget[8] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

	rgCornerRadii[0] = data.base->top_left.horizontal;
	rgCornerRadii[1] = data.base->top_left.vertical;
	rgCornerRadii[2] = data.base->top_right.horizontal;
	rgCornerRadii[3] = data.base->top_right.vertical;
	rgCornerRadii[4] = data.base->bottom_right.horizontal;
	rgCornerRadii[5] = data.base->bottom_right.vertical;
	rgCornerRadii[6] = data.base->bottom_left.horizontal;
	rgCornerRadii[7] = data.base->bottom_left.vertical;

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		rgCornerRadiiTarget[0] = data.transition->top_left.horizontal;
		rgCornerRadiiTarget[1] = data.transition->top_left.vertical;
		rgCornerRadiiTarget[2] = data.transition->top_right.horizontal;
		rgCornerRadiiTarget[3] = data.transition->top_right.vertical;
		rgCornerRadiiTarget[4] = data.transition->bottom_right.horizontal;
		rgCornerRadiiTarget[5] = data.transition->bottom_right.vertical;
		rgCornerRadiiTarget[6] = data.transition->bottom_left.horizontal;
		rgCornerRadiiTarget[7] = data.transition->bottom_left.vertical;	

		for( int i=0; i < 8; ++i )
			rgCornerRadii[i] = lerp( data.hStyleProperty, *data.pTransitionData, rgCornerRadii[i], rgCornerRadiiTarget[i], eAnimating, k_EAnimatingFlag_CompositionOnly );
	}

	pContext->SetBorderRadius( rgCornerRadii[0], rgCornerRadii[1], rgCornerRadii[2], rgCornerRadii[3],
		rgCornerRadii[4], rgCornerRadii[5], rgCornerRadii[6], rgCornerRadii[7] );

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Push panel position operation into the current context
//-----------------------------------------------------------------------------
CUIAnimationEngine::EAnimatingFlags CUIAnimationEngine::PushPanelPosition( CAnimationAndTransformContext *pContext, const PanelPositionWithTransition_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::PushPanelPosition", VPROF_BUDGETGROUP_TENFOOT );
	
	RenderCmdFrameData_t< RenderPoint_t > data;
	GetRenderCmdFrameData( &data, renderCommand );

	float x = data.base->x;
	float y = data.base->y;
	float z = data.base->z;

	EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
	if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
	{
		x = lerp( data.hStyleProperty, *data.pTransitionData, x, (float)data.transition->x, eAnimating, k_EAnimatingFlag_CompositionOnly );
		y = lerp( data.hStyleProperty, *data.pTransitionData, y, (float)data.transition->y, eAnimating, k_EAnimatingFlag_CompositionOnly );
		z = lerp( data.hStyleProperty, *data.pTransitionData, z, (float)data.transition->z, eAnimating, k_EAnimatingFlag_CompositionOnly );
	}

	pContext->SetPosition( x, y, z );

	if ( renderCommand.scroll_offset )
	{
		float xScroll = renderCommand.scroll_offset->x;
		float yScroll = renderCommand.scroll_offset->y;

		if ( renderCommand.scroll_transition_x && renderCommand.scroll_transition_x->timing_func != k_EAnimationNone )
		{
			xScroll = lerp( STYLE_SYMBOL_SCROLL, *renderCommand.scroll_transition_x, xScroll, (float)renderCommand.scroll_offset_target->x, eAnimating, k_EAnimatingFlag_CompositionOnly );
		}

		if ( renderCommand.scroll_transition_y && renderCommand.scroll_transition_y->timing_func != k_EAnimationNone )
		{
			yScroll = lerp( STYLE_SYMBOL_SCROLL, *renderCommand.scroll_transition_y, yScroll, (float)renderCommand.scroll_offset_target->y, eAnimating, k_EAnimatingFlag_CompositionOnly );
		}

		pContext->SetContentsScroll( xScroll, yScroll );
	}

	return eAnimating;
}


//-----------------------------------------------------------------------------
// Purpose: Get the correct drawing offsets for a drawing op based on the current context
//-----------------------------------------------------------------------------
void CUIAnimationEngine::GetCurrentDrawingOffsets( float &x, float &y, float &z, bool bSkipCurrentPanel /* = false */ )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::GetCurrentDrawingOffsets", VPROF_BUDGETGROUP_TENFOOT );
	x = y = z = 0.0f;

	int iCur = m_AnimationAndTransformStack.Tail();
	if ( iCur != m_AnimationAndTransformStack.InvalidIndex() )
	{
		if ( !bSkipCurrentPanel )
		{
			CAnimationAndTransformContext *pContext = m_AnimationAndTransformStack[iCur];
			pContext->GetCurrentDrawingOffset( x, y, z );
		}
		else
		{
			int iPrev = m_AnimationAndTransformStack.Previous( iCur );
			if ( iPrev != m_AnimationAndTransformStack.InvalidIndex() )
			{
				CAnimationAndTransformContext *pContext = m_AnimationAndTransformStack[iPrev];
				pContext->GetCurrentDrawingOffset( x, y, z );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Push free of a compositing layer
//-----------------------------------------------------------------------------
void CUIAnimationEngine::FreeAndForceRepaintOfCompositingLayer( uint64 ulLayerID, bool bForceRepaint )
{
	// Don't ever skip this for m_unSkipUntilContextPopCounter! It's because we are skipping normal updates to a layer 
	// that we must push this!

	// This is a list to tell the render thread about this frame
	m_vecLayersToFree.AddToTail( ulLayerID );

	// This is a tree of layers for the layout thread to notice when it gets ready to repaint
	if ( bForceRepaint )
		UIEngine()->MarkLayerToRepaintThreadSafe( ulLayerID );

	m_mapPanelsToCompositionLayerNeeded.Remove( ulLayerID );
	m_treePanelsThatNeededLayersThisFrame.Remove( ulLayerID );
}


//-----------------------------------------------------------------------------
// Purpose: Handle animation/transform on quad
//-----------------------------------------------------------------------------
void CUIAnimationEngine::DrawTexturedRect( const DrawTexturedRectRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::DrawTexturedRect", VPROF_BUDGETGROUP_TENFOOT );

	if ( m_unSkipUntilContextPopCounter > 0 )
	{
		if ( renderCommand.texture_serial )
		{
			// Need to still push a fake draw to increment the drawn serial and avoid deadlocks in html code that uses this syncronization stuff
			LockTextureRenderCommand_t *pTransformedCommand = outputCommandList.AllocType< LockTextureRenderCommand_t >();

			pTransformedCommand->texture.CopyFrom( renderCommand.texture, outputCommandList );
			pTransformedCommand->texture_serial = renderCommand.texture_serial;

			Assert( m_pCurrentOperationContext );
			if ( m_pCurrentOperationContext )
			{
				RenderOperation_t *pOperation = AllocPerFrameObject< RenderOperation_t >();;
				m_pCurrentOperationContext->m_pVecChildOperations->AddToTail( pOperation );
				pOperation->m_flAvgZPos = 0.0f;
				pOperation->m_pRenderCommand = pTransformedCommand;
				pOperation->m_pParent = m_pCurrentOperationContext;
				pOperation->m_pVecChildOperations = GetFreeRenderOpsVector();
			}
		}
		return;
	}

	// Perform geometry adjustments
	RenderTexturedRectRenderCommand_t *pTransformedCommand = outputCommandList.AllocType< RenderTexturedRectRenderCommand_t >();
	pTransformedCommand->texture_top_left.CopyFrom( renderCommand.texture_top_left, outputCommandList );
	pTransformedCommand->texture_bottom_right.CopyFrom( renderCommand.texture_bottom_right, outputCommandList );

	pTransformedCommand->texture.CopyFrom( renderCommand.texture, outputCommandList );
	pTransformedCommand->texture_serial = renderCommand.texture_serial;
	pTransformedCommand->texture_sample_mode = renderCommand.texture_sample_mode;

	if ( renderCommand.texture_opacity )
	{
		RenderCmdFrameData_t< float > data;
		GetRenderCmdFrameData( &data, *renderCommand.texture_opacity );

		float flOpacity = 1.0f;
		if ( data.pTransitionData && data.pTransitionData->timing_func != k_EAnimationNone )
		{
			EAnimatingFlags eAnimating = k_EAnimatingFlag_NotAnimating;
			flOpacity = lerp( data.hStyleProperty, *data.pTransitionData, *data.base, *data.transition, eAnimating, k_EAnimatingFlag_CompositionOnly );
		}
		else
		{
			// We must have supplied background-img-opacity with no transition or animation
			flOpacity = *data.base;
		}

		pTransformedCommand->texture_opacity = clamp( flOpacity, 0.0f, 1.0f );
	}

	float x,y,z;
	GetCurrentDrawingOffsets( x, y, z );

	pTransformedCommand->top_left.x = renderCommand.top_left.x + x;
	pTransformedCommand->top_left.y = renderCommand.top_left.y + y;
	pTransformedCommand->bottom_right.x = renderCommand.bottom_right.x + x;
	pTransformedCommand->bottom_right.y = renderCommand.bottom_right.y + y;

	// Add shadow data

	if ( m_AnimationAndTransformStack.Count() )
	{
		CAnimationAndTransformContext *pContext = m_AnimationAndTransformStack[ m_AnimationAndTransformStack.Tail() ];
		if ( pContext->BImageShadowSet() )
		{
			float flHorizontalOffset, flVerticalOffset, flBlurRadius, flStrength;
			Color colShadow;
			bool bAnimating;
			pContext->GetImageShadow( flHorizontalOffset, flVerticalOffset, flBlurRadius, flStrength, colShadow, bAnimating );

			pTransformedCommand->img_shadow = outputCommandList.AllocType< ImageShadowData_t >();
			pTransformedCommand->img_shadow->horizontal_offset = flHorizontalOffset;
			pTransformedCommand->img_shadow->vertical_offset = flVerticalOffset;
			pTransformedCommand->img_shadow->blur_radius = flBlurRadius;
			pTransformedCommand->img_shadow->strength = flStrength;
			pTransformedCommand->img_shadow->color = colShadow.GetRawColor();
			pTransformedCommand->img_shadow->animating = bAnimating;
		}
		else
		{
			pTransformedCommand->img_shadow = nullptr;
		}
	}
	

	Assert( m_pCurrentOperationContext );
	if ( m_pCurrentOperationContext )
	{
		RenderOperation_t *pOperation = AllocPerFrameObject< RenderOperation_t >();;
		m_pCurrentOperationContext->m_pVecChildOperations->AddToTail( pOperation );
		pOperation->m_flAvgZPos = z;
		if ( z != 0.0f )
		{
			m_pCurrentOperationContext->m_bSortChildOperations = true;
		}
		pOperation->m_flXOffsetInParentLayer = pTransformedCommand->top_left.x;
		pOperation->m_flXOffsetInParentLayer = pTransformedCommand->top_left.y;
		pOperation->m_pRenderCommand = pTransformedCommand;
		pOperation->m_pParent = m_pCurrentOperationContext;
		pOperation->m_pVecChildOperations = GetFreeRenderOpsVector();
	}
}

void InterpolateColorStops( float flTransitionProgress,
	const CRenderDataList< ColorStop_t > *pStartStops, uint32 unStartColor,
	const CRenderDataList< ColorStop_t > *pEndStops, uint32 unEndColor,
	CRenderDataList< ColorStop_t > &outputStops, CRenderCommandList &commandList )
{
	Assert( pStartStops || pEndStops );

	CRenderDataListBuilder< ColorStop_t > outputStopsBuilder( outputStops, &commandList );

	bool bFirstStop = true;
	const ColorStop_t *pStartStop = pStartStops ? pStartStops->GetFirst() : nullptr;
	const ColorStop_t *pEndStop = pEndStops ? pEndStops->GetFirst() : nullptr;
	while ( pStartStop || pEndStop )
	{
		ColorStop_t *pOutStop = outputStopsBuilder.AddToTail();

		Color ca, cb;
		ca.SetRawColor( pStartStop ? pStartStop->color_rgba : unStartColor );
		cb.SetRawColor( pEndStop ? pEndStop->color_rgba : unEndColor );

		Color cOut( clamp( Lerp( flTransitionProgress, ca.r(), cb.r() ), 0, 255 ),
			clamp( Lerp( flTransitionProgress, ca.g(), cb.g() ), 0, 255 ),
			clamp( Lerp( flTransitionProgress, ca.b(), cb.b() ), 0, 255 ),
			clamp( Lerp( flTransitionProgress, ca.a(), cb.a() ), 0, 255 ) );
		pOutStop->color_rgba = cOut.GetRawColor();

		double fStartPosition = pStartStop ? pStartStop->position : ( bFirstStop ? 0.0 : 1.0 );
		double fEndPosition = pEndStop ? pEndStop->position : ( bFirstStop ? 0.0 : 1.0 );
		pOutStop->position = Lerp( flTransitionProgress, fStartPosition, fEndPosition );

		if ( pStartStop )
		{
			unStartColor = pStartStop->color_rgba;
			pStartStop = pStartStops->GetNext( pStartStop );
		}

		if ( pEndStop )
		{
			unEndColor = pEndStop->color_rgba;
			pEndStop = pEndStops->GetNext( pEndStop );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Helper for running particle systems, this takes a fill brush collection as 
// input, and finds any particle systems present in it, then it creates/updates the 
// system locally and updates the message to contain the data for each particle for
// rendering on the render thread.
//-----------------------------------------------------------------------------
void CUIAnimationEngine::UpdateParticleSystems( FillBrushCollection_t &fillBrushCollection, CRenderCommandList &outputCommandList )
{
	for ( FillBrush_t *pFillBrush : fillBrushCollection.fill_brush )
	{
		if ( pFillBrush->eFillBrushType == k_EFillBrushType_ParticleSystem )
		{
			UpdateParticleSystem( *pFillBrush->particle_system, outputCommandList );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper for updating individual particle system
//-----------------------------------------------------------------------------
void CUIAnimationEngine::UpdateParticleSystem( ParticleSystem_t &particleSystem, CRenderCommandList &outputCommandList )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::UpdateParticleSystem", VPROF_BUDGETGROUP_TENFOOT );

	AnimationParticleSystemKey_t key;
	key.ulPanelHandle = particleSystem.parent_panel_handle;
	key.unBrushIndex = particleSystem.parent_brush_index;
	
	CAnimationParticleSystem *pSystem = NULL;

	int iMap = m_mapParticleSystems.Find( key );
	if ( iMap == m_mapParticleSystems.InvalidIndex() )
	{
		pSystem = new CAnimationParticleSystem();
		m_mapParticleSystems.Insert( key, pSystem );
	}
	else
	{
		pSystem = m_mapParticleSystems[iMap];
	}

	Vector vecBasePositon;
	RenderPointToVector( vecBasePositon, particleSystem.base_position );

	Vector vecBasePositionVariance;
	RenderPointToVector( vecBasePositionVariance, particleSystem.base_position_variance );

	Vector vecInitialVelocity;
	RenderPointToVector( vecInitialVelocity, particleSystem.particle_initial_velocity );

	Vector vecInitialVelocityVariance;
	RenderPointToVector( vecInitialVelocityVariance, particleSystem.particle_initial_velocity_variance );

	Vector vecVelocityMin;
	RenderPointToVector( vecVelocityMin, particleSystem.particle_velocity_min );

	Vector vecVelocityMax;
	RenderPointToVector( vecVelocityMax, particleSystem.particle_velocity_max );

	Vector vecGravityAcceleration;
	RenderPointToVector( vecGravityAcceleration, particleSystem.gravity_acceleration );

	Vector vecGravityAccelerationParticleVariance;
	RenderPointToVector( vecGravityAccelerationParticleVariance, particleSystem.gravity_acceleration_particle_variance );

	Color colorStart, colorStartVariance, colorEnd, colorEndVariance;
	colorStart.SetRawColor( particleSystem.color_start_rgba );
	colorStartVariance.SetRawColor( particleSystem.color_start_rgba_variance );
	colorEnd.SetRawColor( particleSystem.color_end_rgba );
	colorEndVariance.SetRawColor( particleSystem.color_end_rgba_variance );

	pSystem->SetSystemValues( vecBasePositon, vecBasePositionVariance, particleSystem.particle_size, particleSystem.particle_size_variance,
		particleSystem.particles_per_second, particleSystem.particles_per_second_variance, vecInitialVelocity, vecInitialVelocityVariance, vecVelocityMin, vecVelocityMax,
		vecGravityAcceleration, vecGravityAccelerationParticleVariance, colorStart, colorStartVariance, colorEnd, colorEndVariance, particleSystem.particle_sharpness, particleSystem.particle_sharpness_variance,
		particleSystem.particle_lifespan_seconds, particleSystem.particle_lifespan_seconds_variance, particleSystem.particle_flicker, particleSystem.particle_flicker_variance );

	// Now that we've updated the system so it will use the new values next time it runs a frame, we also need to 
	// serialize the particles that presently exist into the message for rendering
	pSystem->SerializeParticles( particleSystem, outputCommandList );
}


//-----------------------------------------------------------------------------
// Purpose: Helper for interpolating fill brushes
//-----------------------------------------------------------------------------
void CUIAnimationEngine::InterpolateFillBrush( const FillBrushCollectionWithTransition_t &inputCollection, FillBrushCollection_t *pOutputCollection, CRenderCommandList &outputCommandList )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::InterpolateFillBrush", VPROF_BUDGETGROUP_TENFOOT );

	RenderCmdFrameData_t< FillBrushCollection_t > data;
	GetRenderCmdFrameData( &data, inputCollection );

	float flTransitionProgress = GetTimingTransitionProgress( STYLE_SYMBOL_INVALID, data.pTransitionData );
	
	FillBrush_t k_emptyBrush;
	memset( &k_emptyBrush, 0, sizeof( k_emptyBrush ) );
	k_emptyBrush.eFillBrushType = k_EFillBrushType_Color;

	const FillBrush_t *pCurrentBaseBrush = data.base->fill_brush.GetFirst();
	const FillBrush_t *pCurrentTransitionBrush = data.transition->fill_brush.GetFirst();

	const FillBrush_t *pStartBrush = &k_emptyBrush;
	const FillBrush_t *pEndBrush = &k_emptyBrush;

	CRenderDataListBuilder< FillBrush_t > outputFillBrushes( pOutputCollection->fill_brush, &outputCommandList );
	while ( pCurrentBaseBrush || pCurrentTransitionBrush )
	{
		if ( pCurrentBaseBrush )
			pStartBrush = pCurrentBaseBrush;
		if ( pCurrentTransitionBrush )
			pEndBrush = pCurrentTransitionBrush;

		if ( CloseEnough( flTransitionProgress, 0.0f ) )
		{
			// Fast case if there is no transition
			FillBrush_t *pBrushOut = outputFillBrushes.AddToTail();
			pBrushOut->CopyFrom( *pStartBrush, outputCommandList );
		}
		else if ( flTransitionProgress >= 1.0f )
		{
			// Fast case if the transition is actually done...
			FillBrush_t *pBrushOut = outputFillBrushes.AddToTail();
			pBrushOut->CopyFrom( *pEndBrush, outputCommandList );
		}
		else if ( pStartBrush->eFillBrushType == k_EFillBrushType_Color && pEndBrush->eFillBrushType == k_EFillBrushType_Color )
		{
			// Are they both simple fill colors?  That's awesome basic color lerp case.
			Color ca, cb;
			ca.SetRawColor( pStartBrush->color_rgba );
			cb.SetRawColor( pEndBrush->color_rgba );

			Color cOut( clamp( Lerp( flTransitionProgress, ca.r(), cb.r() ), 0, 255 ),
				clamp( Lerp( flTransitionProgress, ca.g(), cb.g() ), 0, 255 ),
				clamp( Lerp( flTransitionProgress, ca.b(), cb.b() ), 0, 255 ),
				clamp( Lerp( flTransitionProgress, ca.a(), cb.a() ), 0, 255 ) );

			FillBrush_t *pBrushOut = outputFillBrushes.AddToTail();
			pBrushOut->eFillBrushType = k_EFillBrushType_Color;
			pBrushOut->color_rgba = cOut.GetRawColor();
			pBrushOut->opacity = clamp( Lerp( flTransitionProgress, pStartBrush->opacity, pEndBrush->opacity ), 0.0f, 1.0f );
		}
		else if ( ( pStartBrush->eFillBrushType == k_EFillBrushType_LinearGradient || pStartBrush->eFillBrushType == k_EFillBrushType_Color ) && 
			( pEndBrush->eFillBrushType == k_EFillBrushType_LinearGradient || pEndBrush->eFillBrushType == k_EFillBrushType_Color ) )
		{
			// If we have a linear gradient and a basic rgba color, or two linear gradients, then we can normalize them into linear
			// gradients with the same number of color stops and interpolate the gradients.
			FillBrush_t *pBrushOut = outputFillBrushes.AddToTail();
			pBrushOut->opacity = Lerp( flTransitionProgress, pStartBrush->opacity, pEndBrush->opacity );

			const LinearGradient_t *pStartGradient = pStartBrush->eFillBrushType == k_EFillBrushType_LinearGradient ? pStartBrush->linear_gradient : nullptr;
			const LinearGradient_t *pEndGradient = pEndBrush->eFillBrushType == k_EFillBrushType_LinearGradient ? pEndBrush->linear_gradient : nullptr;

			uint32 unStartColor = pStartBrush->eFillBrushType == k_EFillBrushType_Color ? pStartBrush->color_rgba : 0;
			uint32 unEndColor = pEndBrush->eFillBrushType == k_EFillBrushType_Color ? pEndBrush->color_rgba : 0;

			pBrushOut->eFillBrushType = k_EFillBrushType_LinearGradient;
			pBrushOut->linear_gradient = outputCommandList.AllocType< LinearGradient_t >();
			pBrushOut->linear_gradient->start_position.x = 
				Lerp( flTransitionProgress, pStartGradient ? pStartGradient->start_position.x : 0.0f , pEndGradient ? pEndGradient->start_position.x : 0.0f );
			pBrushOut->linear_gradient->start_position.y =
				Lerp( flTransitionProgress, pStartGradient ? pStartGradient->start_position.y : 0.0f, pEndGradient ? pEndGradient->start_position.y : 0.0f );
			pBrushOut->linear_gradient->end_position.x =
				Lerp( flTransitionProgress, pStartGradient ? pStartGradient->end_position.x : 0.0f, pEndGradient ? pEndGradient->end_position.x : 0.0f );
			pBrushOut->linear_gradient->end_position.y =
				Lerp( flTransitionProgress, pStartGradient ? pStartGradient->end_position.y : 0.0f, pEndGradient ? pEndGradient->end_position.y : 0.0f );

			InterpolateColorStops( flTransitionProgress,
				pStartGradient ? &pStartGradient->color_stop : nullptr, unStartColor,
				pEndGradient ? &pEndGradient->color_stop : nullptr, unEndColor,
				pBrushOut->linear_gradient->color_stop, outputCommandList );
		}
		else if ( ( pStartBrush->eFillBrushType == k_EFillBrushType_RadialGradient || pStartBrush->eFillBrushType == k_EFillBrushType_Color ) &&
			( pEndBrush->eFillBrushType == k_EFillBrushType_RadialGradient || pEndBrush->eFillBrushType == k_EFillBrushType_Color ) )
		{
			FillBrush_t *pBrushOut = outputFillBrushes.AddToTail();
			pBrushOut->opacity = Lerp( flTransitionProgress, pStartBrush->opacity, pEndBrush->opacity );

			const RadialGradient_t *pStartGradient = pStartBrush->eFillBrushType == k_EFillBrushType_RadialGradient ? pStartBrush->radial_gradient : nullptr;
			const RadialGradient_t *pEndGradient = pEndBrush->eFillBrushType == k_EFillBrushType_RadialGradient ? pEndBrush->radial_gradient : nullptr;

			uint32 unStartColor = pStartBrush->eFillBrushType == k_EFillBrushType_Color ? pStartBrush->color_rgba : 0;
			uint32 unEndColor = pEndBrush->eFillBrushType == k_EFillBrushType_Color ? pEndBrush->color_rgba : 0;

			pBrushOut->eFillBrushType = k_EFillBrushType_RadialGradient;
			pBrushOut->radial_gradient = outputCommandList.AllocType< RadialGradient_t >();
			pBrushOut->radial_gradient->center_position.x = 
				Lerp( flTransitionProgress, pStartGradient ? pStartGradient->center_position.x : 0.0f, pEndGradient ? pEndGradient->center_position.x : 0.0f );
			pBrushOut->radial_gradient->center_position.y = 
				Lerp( flTransitionProgress, pStartGradient ? pStartGradient->center_position.y : 0.0f, pEndGradient ? pEndGradient->center_position.y : 0.0f );
			pBrushOut->radial_gradient->offset_distance.x = 
				Lerp( flTransitionProgress, pStartGradient ? pStartGradient->offset_distance.x : 0.0f, pEndGradient ? pEndGradient->offset_distance.x : 0.0f );
			pBrushOut->radial_gradient->offset_distance.y =
				Lerp( flTransitionProgress, pStartGradient ? pStartGradient->offset_distance.y : 0.0f, pEndGradient ? pEndGradient->offset_distance.y : 0.0f );
			pBrushOut->radial_gradient->radii.x =
				Lerp( flTransitionProgress, pStartGradient ? pStartGradient->radii.x : 0.0f, pEndGradient ? pEndGradient->radii.x : 0.0f );
			pBrushOut->radial_gradient->radii.y =
				Lerp( flTransitionProgress, pStartGradient ? pStartGradient->radii.y : 0.0f, pEndGradient ? pEndGradient->radii.y : 0.0f );

			InterpolateColorStops( flTransitionProgress,
				pStartGradient ? &pStartGradient->color_stop : nullptr, unStartColor,
				pEndGradient ? &pEndGradient->color_stop : nullptr, unEndColor,
				pBrushOut->radial_gradient->color_stop, outputCommandList );
		}
		else if ( pStartBrush->eFillBrushType == k_EFillBrushType_ParticleSystem && pEndBrush->eFillBrushType == k_EFillBrushType_ParticleSystem )
		{
			FillBrush_t *pBrushOut = outputFillBrushes.AddToTail();
			pBrushOut->opacity = Lerp( flTransitionProgress, pStartBrush->opacity, pEndBrush->opacity );

			const ParticleSystem_t *pStart = pStartBrush->particle_system;
			const ParticleSystem_t *pEnd = pEndBrush->particle_system;

			pBrushOut->eFillBrushType = k_EFillBrushType_ParticleSystem;
			pBrushOut->particle_system = outputCommandList.AllocType< ParticleSystem_t >();
			ParticleSystem_t *pOut = pBrushOut->particle_system;

			pOut->base_position.x = Lerp( flTransitionProgress, pStart->base_position.x, pEnd->base_position.x );
			pOut->base_position.y = Lerp( flTransitionProgress, pStart->base_position.y, pEnd->base_position.y );
			pOut->base_position.z = Lerp( flTransitionProgress, pStart->base_position.z, pEnd->base_position.z );

			pOut->base_position_variance.x = Lerp( flTransitionProgress, pStart->base_position_variance.x, pEnd->base_position_variance.x );
			pOut->base_position_variance.y = Lerp( flTransitionProgress, pStart->base_position_variance.y, pEnd->base_position_variance.y );
			pOut->base_position_variance.z = Lerp( flTransitionProgress, pStart->base_position_variance.z, pEnd->base_position_variance.z );

			pOut->particle_size = Lerp( flTransitionProgress, pStart->particle_size, pEnd->particle_size );
			pOut->particle_size_variance = Lerp( flTransitionProgress, pStart->particle_size_variance, pEnd->particle_size_variance );

			pOut->particles_per_second = Lerp( flTransitionProgress, pStart->particles_per_second, pEnd->particles_per_second );
			pOut->particles_per_second_variance = Lerp( flTransitionProgress, pStart->particles_per_second_variance, pEnd->particles_per_second_variance );

			pOut->particle_lifespan_seconds = Lerp( flTransitionProgress, pStart->particle_lifespan_seconds, pEnd->particle_lifespan_seconds );
			pOut->particle_lifespan_seconds_variance = Lerp( flTransitionProgress, pStart->particle_lifespan_seconds_variance, pEnd->particle_lifespan_seconds_variance );

			pOut->particle_initial_velocity.x = Lerp( flTransitionProgress, pStart->particle_initial_velocity.x, pEnd->particle_initial_velocity.x );
			pOut->particle_initial_velocity.y = Lerp( flTransitionProgress, pStart->particle_initial_velocity.y, pEnd->particle_initial_velocity.y );
			pOut->particle_initial_velocity.z = Lerp( flTransitionProgress, pStart->particle_initial_velocity.z, pEnd->particle_initial_velocity.z );

			pOut->particle_initial_velocity_variance.x = Lerp( flTransitionProgress, pStart->particle_initial_velocity_variance.x, pEnd->particle_initial_velocity_variance.x );
			pOut->particle_initial_velocity_variance.y = Lerp( flTransitionProgress, pStart->particle_initial_velocity_variance.y, pEnd->particle_initial_velocity_variance.y );
			pOut->particle_initial_velocity_variance.z = Lerp( flTransitionProgress, pStart->particle_initial_velocity_variance.z, pEnd->particle_initial_velocity_variance.z );

			pOut->particle_velocity_min.x = Lerp( flTransitionProgress, pStart->particle_velocity_min.x, pEnd->particle_velocity_min.x );
			pOut->particle_velocity_min.y = Lerp( flTransitionProgress, pStart->particle_velocity_min.y, pEnd->particle_velocity_min.y );
			pOut->particle_velocity_min.z = Lerp( flTransitionProgress, pStart->particle_velocity_min.z, pEnd->particle_velocity_min.z );

			pOut->particle_velocity_max.x = Lerp( flTransitionProgress, pStart->particle_velocity_max.x, pEnd->particle_velocity_max.x );
			pOut->particle_velocity_max.y = Lerp( flTransitionProgress, pStart->particle_velocity_max.y, pEnd->particle_velocity_max.y );
			pOut->particle_velocity_max.z = Lerp( flTransitionProgress, pStart->particle_velocity_max.z, pEnd->particle_velocity_max.z );

			pOut->gravity_acceleration.x = Lerp( flTransitionProgress, pStart->gravity_acceleration.x, pEnd->gravity_acceleration.x );
			pOut->gravity_acceleration.y = Lerp( flTransitionProgress, pStart->gravity_acceleration.y, pEnd->gravity_acceleration.y );
			pOut->gravity_acceleration.z = Lerp( flTransitionProgress, pStart->gravity_acceleration.z, pEnd->gravity_acceleration.z );

			pOut->gravity_acceleration_particle_variance.x = Lerp( flTransitionProgress, pStart->gravity_acceleration_particle_variance.x, pEnd->gravity_acceleration_particle_variance.x );
			pOut->gravity_acceleration_particle_variance.y = Lerp( flTransitionProgress, pStart->gravity_acceleration_particle_variance.y, pEnd->gravity_acceleration_particle_variance.y );
			pOut->gravity_acceleration_particle_variance.z = Lerp( flTransitionProgress, pStart->gravity_acceleration_particle_variance.z, pEnd->gravity_acceleration_particle_variance.z );

			Color ca, cb;
			ca.SetRawColor( pStart->color_start_rgba );
			cb.SetRawColor( pEnd->color_start_rgba );

			Color cOut( clamp( Lerp( flTransitionProgress, ca.r(), cb.r() ), 0, 255 ),
				clamp( Lerp( flTransitionProgress, ca.g(), cb.g() ), 0, 255 ),
				clamp( Lerp( flTransitionProgress, ca.b(), cb.b() ), 0, 255 ),
				clamp( Lerp( flTransitionProgress, ca.a(), cb.a() ), 0, 255 ) );

			pOut->color_start_rgba = cOut.GetRawColor();

			ca.SetRawColor( pStart->color_start_rgba_variance );
			cb.SetRawColor( pEnd->color_start_rgba_variance );

			cOut.SetColor( clamp( Lerp( flTransitionProgress, ca.r(), cb.r() ), 0, 255 ),
				clamp( Lerp( flTransitionProgress, ca.g(), cb.g() ), 0, 255 ),
				clamp( Lerp( flTransitionProgress, ca.b(), cb.b() ), 0, 255 ),
				clamp( Lerp( flTransitionProgress, ca.a(), cb.a() ), 0, 255 ) );

			pOut->color_start_rgba_variance = cOut.GetRawColor();

			ca.SetRawColor( pStart->color_end_rgba );
			cb.SetRawColor( pEnd->color_end_rgba );

			cOut.SetColor( Lerp( flTransitionProgress, ca.r(), cb.r() ),
				Lerp( flTransitionProgress, ca.g(), cb.g() ),
				Lerp( flTransitionProgress, ca.b(), cb.b() ),
				Lerp( flTransitionProgress, ca.a(), cb.a() ) );

			pOut->color_end_rgba = cOut.GetRawColor();

			ca.SetRawColor( pStart->color_end_rgba_variance );
			cb.SetRawColor( pEnd->color_end_rgba_variance );

			cOut.SetColor( Lerp( flTransitionProgress, ca.r(), cb.r() ),
				Lerp( flTransitionProgress, ca.g(), cb.g() ),
				Lerp( flTransitionProgress, ca.b(), cb.b() ),
				Lerp( flTransitionProgress, ca.a(), cb.a() ) );

			pOut->color_end_rgba_variance = cOut.GetRawColor();

			Assert( pStart->parent_panel_handle == pEnd->parent_panel_handle );
			Assert( pStart->parent_brush_index == pEnd->parent_brush_index );
			pOut->parent_panel_handle = pEnd->parent_panel_handle;
			pOut->parent_brush_index = pEnd->parent_brush_index;

			pOut->particle_sharpness = Lerp( flTransitionProgress, pStart->particle_sharpness, pEnd->particle_sharpness );
			pOut->particle_sharpness_variance = Lerp( flTransitionProgress, pStart->particle_sharpness_variance, pEnd->particle_sharpness_variance );

			pOut->particle_flicker = Lerp( flTransitionProgress, pStart->particle_flicker, pEnd->particle_flicker );
			pOut->particle_flicker_variance = Lerp( flTransitionProgress, pStart->particle_flicker_variance, pEnd->particle_flicker_variance );
		}
		else
		{
			// We have two incompatible types that can't do smart interpolation (say a linear gradient and a radial gradient), we
			// will just crossfade between the two.
			FillBrush_t *pBrushOut = outputFillBrushes.AddToTail();
			pBrushOut->CopyFrom( *pStartBrush, outputCommandList );
			pBrushOut->opacity = pStartBrush->opacity * ( 1.0f - flTransitionProgress );

			FillBrush_t *pBrushOutTwo = outputFillBrushes.AddToTail();
			pBrushOutTwo->CopyFrom( *pEndBrush, outputCommandList );
			pBrushOutTwo->opacity = pEndBrush->opacity * flTransitionProgress;
		}

		if ( pCurrentBaseBrush )
			pCurrentBaseBrush = data.base->fill_brush.GetNext( pCurrentBaseBrush );
		if ( pCurrentTransitionBrush )
			pCurrentTransitionBrush = data.transition->fill_brush.GetNext( pCurrentTransitionBrush );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handle animation/transform on quad
//-----------------------------------------------------------------------------
void CUIAnimationEngine::DrawFilledRect( const DrawFilledRectRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList )
{
	// Perform geometry adjustments
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::DrawFilledRect", VPROF_BUDGETGROUP_TENFOOT );

	if ( m_unSkipUntilContextPopCounter > 0 )
		return;

	RenderFilledRectRenderCommand_t *pTransformedCommand = outputCommandList.AllocType< RenderFilledRectRenderCommand_t >();
	pTransformedCommand->antialiasing = renderCommand.antialiasing;

	pTransformedCommand->context_id = renderCommand.context_id;

	InterpolateFillBrush( renderCommand.fill_brush_collection, &pTransformedCommand->fill_brush_collection, outputCommandList );
	UpdateParticleSystems( pTransformedCommand->fill_brush_collection, outputCommandList );

	float x,y,z;
	GetCurrentDrawingOffsets( x, y, z );

	pTransformedCommand->top_left.x = renderCommand.top_left.x + x;
	pTransformedCommand->top_left.y = renderCommand.top_left.y + y;
	pTransformedCommand->bottom_right.x = renderCommand.bottom_right.x + x;
	pTransformedCommand->bottom_right.y = renderCommand.bottom_right.y + y;

	Assert( m_pCurrentOperationContext );
	if ( m_pCurrentOperationContext )
	{
		RenderOperation_t *pOperation = AllocPerFrameObject< RenderOperation_t >();;
		m_pCurrentOperationContext->m_pVecChildOperations->AddToTail( pOperation );
		pOperation->m_flAvgZPos = z;
		if ( z != 0.0f )
		{
			m_pCurrentOperationContext->m_bSortChildOperations = true;
		}
		pOperation->m_pRenderCommand = pTransformedCommand;
		pOperation->m_pParent = m_pCurrentOperationContext;
		pOperation->m_pVecChildOperations = GetFreeRenderOpsVector();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handle animation/transform on render system callback request
//-----------------------------------------------------------------------------
void CUIAnimationEngine::RequestRenderCallback( const RequestRenderCallbackCommand_t &renderCommand, CRenderCommandList &outputCommandList )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::RequestRenderCallback", VPROF_BUDGETGROUP_TENFOOT );

	if ( m_unSkipUntilContextPopCounter > 0 )
		return;

	// Perform geometry adjustments
	RequestRenderCallbackCommand_t *pTransformedCommand = outputCommandList.AllocType< RequestRenderCallbackCommand_t >();
	pTransformedCommand->pCallbackObj = renderCommand.pCallbackObj;
	pTransformedCommand->flags = renderCommand.flags;
	pTransformedCommand->panelRT.CopyFrom( renderCommand.panelRT, outputCommandList );

	// Get drawing offsets incase we are drawing within a parent
	float x, y, z;
	GetCurrentDrawingOffsets( x, y, z );

	pTransformedCommand->top_left.x = x + renderCommand.top_left_padding.x;
	pTransformedCommand->top_left.y = y + renderCommand.top_left_padding.y;
	pTransformedCommand->bottom_right.x = renderCommand.bottom_right.x - renderCommand.top_left.x + x - renderCommand.bottom_right_padding.x;
	pTransformedCommand->bottom_right.y = renderCommand.bottom_right.y - renderCommand.top_left.y + y - renderCommand.bottom_right_padding.y;

	Assert( m_pCurrentOperationContext );
	if ( m_pCurrentOperationContext )
	{
		RenderOperation_t *pOperation = AllocPerFrameObject< RenderOperation_t >();
		m_pCurrentOperationContext->m_pVecChildOperations->AddToTail( pOperation );
		pOperation->m_flAvgZPos = z;
		if ( z != 0.0f )
		{
			m_pCurrentOperationContext->m_bSortChildOperations = true;
		}
		pOperation->m_pRenderCommand = pTransformedCommand;
		pOperation->m_pParent = m_pCurrentOperationContext;
		pOperation->m_pVecChildOperations = GetFreeRenderOpsVector();

		// Find containing composition layer push and flag as needing depth buffer, also flag as being 'dynamic'
		// content that will repaint every frame if we know thats the situation
		RenderOperation_t *pContext = m_pCurrentOperationContext;
		while ( pContext && !pContext->m_bIsLayerPush )
		{
			pContext = pContext->m_pParent;
		}
		
		// We may not find a layer if the back buffer is the direct parent.  Back buffer just always needs depth and always redraws anyway.
		if ( pContext && pContext->m_bIsLayerPush )
		{
			if ( ( renderCommand.flags & k_ERenderCallbackFlagsNeedsDepth ) != 0 )
			{
				PushCompositingLayerRenderCommand_t *pPushLayer = static_cast<PushCompositingLayerRenderCommand_t *>( pContext->m_pRenderCommand );
				pPushLayer->needs_depth = true;
			}
			if ( ( renderCommand.flags & k_ERenderCallbackFlagsAlwaysRepaint ) != 0 )
			{
				MarkRenderOpAnimating( pContext, false );
			}
		}
	}
}


void CUIAnimationEngine::InterpolateTextFormat( const TextFormat_t &input, RenderTextFormat_t &output, CRenderCommandList &outputCommandList )
{
	output.font_name = outputCommandList.CopyString( input.font_name );
	output.font_size = input.font_size;
	output.font_weight = input.font_weight;
	output.font_style = input.font_style;
	output.text_decoration = input.text_decoration;
	output.letter_spacing = input.letter_spacing;
	InterpolateFillBrush( input.fill_brush_collection, &output.fill_brush_collection, outputCommandList );
	CopyRenderDataPointer( output.inline_object, input.inline_object, outputCommandList );
}


//-----------------------------------------------------------------------------
// Purpose: Handle animation/transform on text region
//-----------------------------------------------------------------------------
void CUIAnimationEngine::DrawTextRegion( const DrawTextRegionRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList )
{
	VPROF_BUDGET_THREAD( "CUIAnimationEngine::DrawTextRegion", VPROF_BUDGETGROUP_TENFOOT );

	if ( m_unSkipUntilContextPopCounter > 0 )
		return;

	// Perform geometry adjustments
	RenderTextRegionCommand_t *pCommand = outputCommandList.AllocType< RenderTextRegionCommand_t >();

	if ( renderCommand.raw_text && renderCommand.raw_text_bytes > 0 )
	{
		pCommand->raw_text = outputCommandList.Alloc( renderCommand.raw_text_bytes );
		V_memcpy( pCommand->raw_text, renderCommand.raw_text, renderCommand.raw_text_bytes );
		pCommand->raw_text_bytes = renderCommand.raw_text_bytes;
	}

	pCommand->text_chars = renderCommand.text_chars;
	pCommand->text_encoding = renderCommand.text_encoding;

	// copy defaults
	InterpolateTextFormat( renderCommand.default_format, pCommand->default_format, outputCommandList );

	// assert that the ranges are monotonically increasing
#ifdef _DEBUG
	uint iCharCur = 0;
	uint iEndCharCur = 0;
#endif

	// copy range formats
	CRenderDataListBuilder< RenderTextRangeFormat_t > outputRangeFormats( pCommand->range_formats, &outputCommandList );
	for ( const TextRangeFormatData_t *pTextRangeFormat : renderCommand.range_formats )
	{
		RenderTextRangeFormat_t *pRangeOut = outputRangeFormats.AddToTail();

		// range should not start before the end of the previous range
		DbgAssert( pTextRangeFormat->start_index >= iEndCharCur );

		DbgAssert( pTextRangeFormat->start_index >= iCharCur );
#ifdef _DEBUG
		iCharCur = pTextRangeFormat->start_index;
#endif
		pRangeOut->start_index = pTextRangeFormat->start_index;

		DbgAssert( pTextRangeFormat->end_index >= iEndCharCur );
#ifdef _DEBUG
		iEndCharCur = pTextRangeFormat->end_index;
#endif
		pRangeOut->end_index = pTextRangeFormat->end_index;

		InterpolateTextFormat( pTextRangeFormat->format, pRangeOut->format, outputCommandList );
	}
	
	pCommand->text_align = renderCommand.text_align;
	pCommand->wrapping = renderCommand.wrapping;
	pCommand->ellipsis = renderCommand.ellipsis;
	pCommand->line_height = renderCommand.line_height;

	float x,y,z;
	GetCurrentDrawingOffsets( x, y, z );

	pCommand->top_left.x = renderCommand.top_left.x + x;
	pCommand->top_left.y = renderCommand.top_left.y + y;
	pCommand->bottom_right.x = renderCommand.bottom_right.x + x;
	pCommand->bottom_right.y = renderCommand.bottom_right.y + y;

	// Add text shadow if applicable
	if ( m_AnimationAndTransformStack.Count() )
	{
		CAnimationAndTransformContext *pContext = m_AnimationAndTransformStack[m_AnimationAndTransformStack.Tail()];
		if ( pContext->BTextShadowSet() )
		{
			float flHorizontalOffset, flVerticalOffset, flBlurRadius, flStrength;
			Color colShadow;
			bool bAnimating;
			pContext->GetTextShadow( flHorizontalOffset, flVerticalOffset, flBlurRadius, flStrength, colShadow, bAnimating );

			pCommand->text_shadow = outputCommandList.AllocType< TextShadowData_t >();
			pCommand->text_shadow->horizontal_offset = flHorizontalOffset;
			pCommand->text_shadow->vertical_offset = flVerticalOffset;
			pCommand->text_shadow->blur_radius = flBlurRadius;
			pCommand->text_shadow->strength = flStrength;
			pCommand->text_shadow->color = colShadow.GetRawColor();
			pCommand->text_shadow->animating = bAnimating;
		}
	}
	else
	{
		AssertMsg( false, "m_AnimationAndTransformStack empty during DrawTextRegion - should never happen, require a PushAnimationAndTransformContext before any drawing" );
	}
	
	Assert( m_pCurrentOperationContext );
	if ( m_pCurrentOperationContext )
	{
		RenderOperation_t *pOperation = AllocPerFrameObject< RenderOperation_t >();
		m_pCurrentOperationContext->m_pVecChildOperations->AddToTail( pOperation );
		pOperation->m_flAvgZPos = z;
		if ( z != 0.0f )
		{
			m_pCurrentOperationContext->m_bSortChildOperations = true;
		}
		pOperation->m_pRenderCommand = pCommand;
		pOperation->m_pParent = m_pCurrentOperationContext;
		pOperation->m_pVecChildOperations = GetFreeRenderOpsVector();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper for computing transition/animation progress
//-----------------------------------------------------------------------------
float CUIAnimationEngine::GetTimingTransitionProgress( uint32 hStyleProperty, const TransitionData_t *pTransitionData )
{
	float flTimeProgress = 0.0f;
	if ( pTransitionData && pTransitionData->timing_func != k_EAnimationNone )
	{
		if ( pTransitionData->duration_seconds < 0.0000001f )
		{
			flTimeProgress = 1.0f;
		}
		else
		{
			flTimeProgress = ( ( GetFrameTimePropertyStoppedAt( hStyleProperty ) - pTransitionData->start_time ) - pTransitionData->delay_seconds ) / pTransitionData->duration_seconds;
			flTimeProgress = clamp( flTimeProgress, 0.0f, 1.0f );

			Vector2D vec[4];
			if ( pTransitionData->timing_func != k_EAnimationCustomBezier )
				panorama::GetAnimationCurveControlPoints( pTransitionData->timing_func, vec );
			else
			{
				vec[ 0 ].x = 0.0f;
				vec[ 0 ].y = 0.0f;
				vec[ 1 ].x = pTransitionData->cubic_bezier_0;
				vec[ 1 ].y = pTransitionData->cubic_bezier_1;
				vec[ 2 ].x = pTransitionData->cubic_bezier_2;
				vec[ 2 ].y = pTransitionData->cubic_bezier_3;
				vec[ 3 ].x = 1.0f;
				vec[ 3 ].y = 1.0f;
			}

			CCubicBezierCurve< Vector2D > curve;
			curve.SetControlPoints( vec );

			flTimeProgress = GetProgressForTimingFunction( curve, flTimeProgress );
		}
	}

	return flTimeProgress;
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CUIAnimationEngine::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	ValidateObj( m_AnimationAndTransformStack );
	FOR_EACH_LL( m_AnimationAndTransformStack, i )
	{
		validator.ClaimMemory( m_AnimationAndTransformStack[i] );
	}

	ValidateObj( m_vecRenderOpsVectorPtrs );
	FOR_EACH_VEC( m_vecRenderOpsVectorPtrs, i )
	{
		ValidatePtr( m_vecRenderOpsVectorPtrs[i] );
	}

	ValidateObj( m_vecLayersPushedThisFrame );

	// don't need to validate m_pCurrentOperationContext, its from our render ops below
	ValidateObj( m_vecRenderOperations );
	FOR_EACH_VEC( m_vecRenderOperations, i )
	{
		ValidatePtr( m_vecRenderOperations[i] );
	}

	ValidateObj( m_mapParticleSystems );
	FOR_EACH_MAP( m_mapParticleSystems, i )
	{
		ValidatePtr( m_mapParticleSystems[i] );
	}

	ValidateObj( m_vecTrackingMousePanels );
	ValidateObj( m_vecTrackingMouseResults );
	
	ValidateObj( m_mapPanelsToCompositionLayerNeeded );
	ValidateObj( m_treePanelsThatNeededLayersThisFrame );

	ValidateObj( m_vecLayersToFree );
}
#endif
