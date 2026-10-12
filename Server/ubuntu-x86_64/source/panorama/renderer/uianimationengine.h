
//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UIANIMATIONENGINE_NEW_H
#define UIANIMATIONENGINE_NEW_H
#pragma once

#include "tier1/utlvector.h"
#include "tier1/utllinkedlist.h"
#include "panorama/renderer/rendercommands.h"
#include "panorama/transformations.h"
#include "color.h"
#include "tier1/mempool.h"
#include "panorama/controls/panelptr.h"
#include "panorama/input/iuiinput.h"
#include "uirenderengine.h"
#include "vstdlib/random.h"


#if !defined( PANORAMA_USE_S1WRAPPER )
// Disable occlusion work done in the animation for CSGO
// Most panels on CSGO are transparent and not occluded by their children
#define PANORAMA_ANIMATION_ENGINE_OCCLUSION
#endif


namespace panorama
{

extern ConVar s_convarPanoramaAll2DTranslatesDontNeedCompositonLayer;
extern ConVar s_convarPanoramaMightScrollDontNeedCompositonLayer;
extern ConVar s_convarPanoramaAllTransformsDontNeedCompositonLayer;
extern ConVar s_convarPanoramaBoxShadowsDontNeedCompositonLayer;
extern ConVar s_convarPanoramaSimpleBordersDontNeedCompositonLayer;
#if !defined( SOURCE2_PANORAMA )
extern ConVar s_convarPanoramaTransformParentsNoLayerIfNoPerspective;
#endif
extern ConVar s_convarPanoramaTransformParentsNoLayerForPerspective;

struct AnimationParticleSystemKey_t
{
	uint64 ulPanelHandle;
	uint32 unBrushIndex;

	bool operator <( const AnimationParticleSystemKey_t &l ) const
	{
		if ( ulPanelHandle < l.ulPanelHandle )
			return true;
		else if ( ulPanelHandle > l.ulPanelHandle )
			return false;

		return unBrushIndex < l.unBrushIndex;
	}
};

// Forward decl
class CAnimationParticleSystem;
struct ScreenSpacePanelQuad_t;
class CRenderCommandList;

// Class to represent a single particle in a particle system on the animation thread
class CAnimationParticle
{
public:

	CAnimationParticle( Vector vecPosition, Vector vecVelocity, Color colorStart, Color colorEnd, float flParticleSize, float flParticleSharpness, float flFlicker, float flTimeNow, float flTimeExpire )
	{
		m_vecPosition = vecPosition;
		m_vecVelocity = vecVelocity;
		m_colorNow = colorStart;
		m_colorStart = colorStart;
		m_colorEnd = colorEnd;
		m_flParticleSize = flParticleSize;
		m_flParticleSharpness = flParticleSharpness;
		m_flFlicker = flFlicker;

		m_flTimeCreate = flTimeNow;
		m_flTimeExpire = flTimeExpire;
		m_flTimeFlickerOff = 0.0f;
		m_flTimeFlickerOn = 0.0f;

	}
private:

	friend class CAnimationParticleSystem;

	Vector m_vecPosition;
	Vector m_vecVelocity;

	Color m_colorNow;
	Color m_colorStart;
	Color m_colorEnd;

	float m_flParticleSize;
	float m_flParticleSharpness;
	float m_flFlicker;

	float m_flTimeCreate;
	float m_flTimeExpire;

	float m_flTimeFlickerOn;
	float m_flTimeFlickerOff;
};

// Class to represent particle system on animation thread
class CAnimationParticleSystem
{

public:

	CAnimationParticleSystem()
	{
		m_vecBasePosition.x = 0.0f;
		m_vecBasePosition.y = 0.0f;
		m_vecBasePosition.z = 0.0f;

		m_vecBasePositionVariance.x = 0.0f;
		m_vecBasePositionVariance.y = 0.0f;
		m_vecBasePositionVariance.z = 0.0f;

		m_flParticleSize = 0.0f;
		m_flParticleSizeVariance = 0.0f;

		m_flParticlesPerSecond = 0.0f;
		m_flParticlesPerSecondVariance = 0.0f;

		m_flParticleLifeSpanSeconds = 0.0f;
		m_flParticleLifeSpanSecondsVariance = 0.0f;

		m_vecParticleInitialVelocity.x = 0.0f;
		m_vecParticleInitialVelocity.y = 0.0f;
		m_vecParticleInitialVelocity.z = 0.0f;

		m_vecGravityAcceleration.x = 0.0f;
		m_vecGravityAcceleration.y = 0.0f;
		m_vecGravityAcceleration.z = 0.0f;

		m_flSharpness = 1.0f;
		m_flSharpnessVariance = 0.0f;

		m_flLastEmitTime = 0.0f;
		m_flLastRunTime = 0.0f;

	}

	~CAnimationParticleSystem()
	{
		m_vecParticles.PurgeAndDeleteElements();
	}

	void SetSystemValues( Vector vecBasePositon, Vector vecBasePositionVariance, float flParticleSize, float flParticleSizeVariance, 
						  float flParticlesPerSecond, float flParticlesPerSecondVariance, Vector vecInitialVelocity, Vector vecInitialVelocityVariance, Vector vecVelocityMin, Vector vecVelocityMax,
						  Vector vecGravityAcceleration, Vector vecGravityAccelerationParticleVariance, Color colorStart, Color colorStartVariance, Color colorEnd, Color colorEndVariance,
						  float flSharpness, float flSharpnessVariance, float flParticleLifespanSeconds, float flParticleLifespanSecondsVariance,
						  float flFlicker, float flFlickerVariance )
	{
		m_vecBasePosition = vecBasePositon;
		m_vecBasePositionVariance = vecBasePositionVariance;

		m_flParticleSize = flParticleSize;
		m_flParticleSizeVariance = flParticleSizeVariance;

		m_flParticlesPerSecond = flParticlesPerSecond;
		m_flParticlesPerSecondVariance = flParticlesPerSecondVariance;

		m_vecParticleInitialVelocity = vecInitialVelocity;
		m_vecParticleInitialVelocityVariance = vecInitialVelocityVariance;

		m_vecParticleVelocityMin = vecVelocityMin;
		m_vecParticleVelocityMax = vecVelocityMax;

		m_vecGravityAcceleration = vecGravityAcceleration;
		m_vecGravityAccelerationParticleVariance = vecGravityAccelerationParticleVariance;

		m_colorStartRGBA = colorStart;
		m_colorStartRGBAVariance = colorStartVariance;

		m_colorEndRGBA = colorEnd;
		m_colorEndRGBAVariance = colorEndVariance;

		m_flSharpness = flSharpness;
		m_flSharpnessVariance = flSharpnessVariance;

		m_flParticleLifeSpanSeconds = flParticleLifespanSeconds;
		m_flParticleLifeSpanSecondsVariance = flParticleLifespanSecondsVariance;

		m_flFlicker = flFlicker;
		m_flFlickerVariance = flFlickerVariance;
	}

	void SerializeParticles( ParticleSystem_t &out, CRenderCommandList &commandList )
	{
		VPROF_BUDGET( "CAnimationParticleSystem::SerializeParticles", VPROF_BUDGETGROUP_TENFOOT );

		FOR_EACH_VEC( m_vecParticles, i )
		{
			CAnimationParticle &particle = *( m_vecParticles[ i ] );

			if ( particle.m_flTimeFlickerOff > m_flLastRunTime )
				continue;

			Particle_t *pParticle = commandList.AllocType< Particle_t >();
			pParticle->particle_position.x = particle.m_vecPosition.x;
			pParticle->particle_position.y = particle.m_vecPosition.y;
			pParticle->particle_position.z = particle.m_vecPosition.z;

			pParticle->color_rgba = particle.m_colorNow.GetRawColor();
			pParticle->particle_sharpness = particle.m_flParticleSharpness;
			pParticle->particle_size = particle.m_flParticleSize;
		}
	}

	void RunSystem( double flFrameTime )
	{
		VPROF_BUDGET( "CAnimationParticleSystem::RunSystem", VPROF_BUDGETGROUP_TENFOOT );

		// Very first frame do nothing, so we can have good baseline time
		if ( m_flLastRunTime == 0.0f )
		{
			m_flLastRunTime = flFrameTime;
			m_flLastEmitTime = flFrameTime;
			return;
		}

		// Limit the amount of time we can advance each frame, in case we've been paused in the debugger
		// or have severe frame rate stutter in the animation thread.  We don't want that to result in all
		// the particles moving way ahead and a burst of new ones.  If we are way too far behind > 2s, then we'll
		// advance time and skip old work, if we are less than 1s behind we'll just do a little catch up each frame and
		// the system will run slightly fast for a few frames.
		if ( flFrameTime > m_flLastRunTime + 1.0f )
		{
			FOR_EACH_VEC_BACK( m_vecParticles, i )
			{
				CAnimationParticle *pParticle = m_vecParticles[i];
				pParticle->m_flTimeExpire += flFrameTime - m_flLastRunTime;
			}
			m_flLastRunTime = flFrameTime - 0.2f;
			m_flLastEmitTime = flFrameTime - 0.2f;
		}

		if ( flFrameTime > m_flLastRunTime + 0.2f )
			flFrameTime = m_flLastRunTime + 0.2f;

		// Loop removing dead particles
		float flSecondsSinceUpdate = flFrameTime - m_flLastRunTime;
		FOR_EACH_VEC_BACK( m_vecParticles, i )
		{
			CAnimationParticle *pParticle = m_vecParticles[ i ];
			if ( pParticle->m_flTimeExpire <= flFrameTime )
			{
				delete pParticle;

				// Doesn't preserve order, which we don't care about, but does preserve order
				// in the section of the list ahead of us in iteration, which we do care about.
				// Guarantees just one item is moved, rather than a full shift.
				m_vecParticles.FastRemove( i );
			}
			else
			{

				float flLifespanProgress = (flFrameTime - pParticle->m_flTimeCreate ) / MAX( pParticle->m_flTimeExpire - pParticle->m_flTimeCreate, 0.000001f );
				flLifespanProgress = clamp( flLifespanProgress, 0.0f, 1.0f );

				pParticle->m_colorNow.SetColor( 
					Lerp( flLifespanProgress, pParticle->m_colorStart.r(), pParticle->m_colorEnd.r() ), 
					Lerp( flLifespanProgress, pParticle->m_colorStart.g(), pParticle->m_colorEnd.g() ), 
					Lerp( flLifespanProgress, pParticle->m_colorStart.b(), pParticle->m_colorEnd.b() ), 
					Lerp( flLifespanProgress, pParticle->m_colorStart.a(), pParticle->m_colorEnd.a() ) 					
				);

				// Apply change in position due to velocity of particle
				pParticle->m_vecPosition.x += pParticle->m_vecVelocity.x * flSecondsSinceUpdate;
				pParticle->m_vecPosition.y += pParticle->m_vecVelocity.y * flSecondsSinceUpdate;
				pParticle->m_vecPosition.z += pParticle->m_vecVelocity.z * flSecondsSinceUpdate;

				// Apply change in velocity due to system gravity
				float x = WeakRandomFloat( m_vecGravityAccelerationParticleVariance.x * -1.0f, m_vecGravityAccelerationParticleVariance.x );
				float y = WeakRandomFloat( m_vecGravityAccelerationParticleVariance.y * -1.0f, m_vecGravityAccelerationParticleVariance.y );
				float z = WeakRandomFloat( m_vecGravityAccelerationParticleVariance.z * -1.0f, m_vecGravityAccelerationParticleVariance.z );
				pParticle->m_vecVelocity.x += (m_vecGravityAcceleration.x + x ) * flSecondsSinceUpdate;
				pParticle->m_vecVelocity.y += (m_vecGravityAcceleration.y + y ) * flSecondsSinceUpdate;
				pParticle->m_vecVelocity.z += (m_vecGravityAcceleration.z + z ) * flSecondsSinceUpdate;

				pParticle->m_vecVelocity.x = clamp( pParticle->m_vecVelocity.x, m_vecParticleVelocityMin.x, m_vecParticleVelocityMax.x );
				pParticle->m_vecVelocity.y = clamp( pParticle->m_vecVelocity.y, m_vecParticleVelocityMin.y, m_vecParticleVelocityMax.y );
				pParticle->m_vecVelocity.z = clamp( pParticle->m_vecVelocity.z, m_vecParticleVelocityMin.z, m_vecParticleVelocityMax.z );

				if ( pParticle->m_flTimeFlickerOff < m_flLastRunTime && pParticle->m_flTimeFlickerOn < m_flLastRunTime ) 
				{
					float flRand = WeakRandomFloat( 0.0f, 1.0f );
					if ( flRand < pParticle->m_flFlicker )
					{
						pParticle->m_flTimeFlickerOff = flFrameTime + ( ( 1.1f - pParticle->m_flFlicker ) * 0.5f );

						float flFlicker = clamp( m_flFlicker + WeakRandomFloat( -1.0f * m_flFlickerVariance, m_flFlickerVariance ), 0.0f, 1.0f );
						pParticle->m_flTimeFlickerOn = pParticle->m_flTimeFlickerOff + ( ( 1.1f - flFlicker ) * 0.5 );
					}
				}
			}
		}

		// Loop emitting new particles
		float flTimeBetweenParticles;
		while ( 1 )
		{
			flTimeBetweenParticles = 1.0f / WeakRandomFloat( MAX( 0.0001f, m_flParticlesPerSecond - m_flParticlesPerSecondVariance ), m_flParticlesPerSecond + m_flParticlesPerSecondVariance );
			if ( m_flLastEmitTime + flTimeBetweenParticles <= flFrameTime )
			{
				// Increment last emit time by the interval we determined
				m_flLastEmitTime += flTimeBetweenParticles;

				// Emit a particle

				float flParticleSize = MAX( 0.0f, m_flParticleSize + WeakRandomFloat( -1.0f * m_flParticleSizeVariance, m_flParticleSizeVariance ) );

				// We let the system generate 0 sized particles, and they count as an emit, but no need to really create a particle in that case
				if ( flParticleSize <= 0.0f )
					continue;

				Vector vecPosition;
				vecPosition.x = m_vecBasePosition.x + WeakRandomFloat( -1.0f * m_vecBasePositionVariance.x, m_vecBasePositionVariance.x );
				vecPosition.y = m_vecBasePosition.y + WeakRandomFloat( -1.0f * m_vecBasePositionVariance.y, m_vecBasePositionVariance.y );
				vecPosition.z = m_vecBasePosition.z + WeakRandomFloat( -1.0f * m_vecBasePositionVariance.z, m_vecBasePositionVariance.z );

				Vector vecVelocity;
				vecVelocity.x = m_vecParticleInitialVelocity.x + WeakRandomFloat( -1.0f * m_vecParticleInitialVelocityVariance.x, m_vecParticleInitialVelocityVariance.x );
				vecVelocity.y = m_vecParticleInitialVelocity.y + WeakRandomFloat( -1.0f * m_vecParticleInitialVelocityVariance.y, m_vecParticleInitialVelocityVariance.y );
				vecVelocity.z = m_vecParticleInitialVelocity.z + WeakRandomFloat( -1.0f * m_vecParticleInitialVelocityVariance.z, m_vecParticleInitialVelocityVariance.z );

				Color colorStart;
				int r,g,b,a;
				r = m_colorStartRGBA.r() + WeakRandomInt( -1 * m_colorStartRGBAVariance.r(), m_colorStartRGBAVariance.r() );
				g = m_colorStartRGBA.g() + WeakRandomInt( -1 * m_colorStartRGBAVariance.g(), m_colorStartRGBAVariance.g() );
				b = m_colorStartRGBA.b() + WeakRandomInt( -1 * m_colorStartRGBAVariance.b(), m_colorStartRGBAVariance.b() );
				a = m_colorStartRGBA.a() + WeakRandomInt( -1 * m_colorStartRGBAVariance.a(), m_colorStartRGBAVariance.a() );

				colorStart.SetColor( 
					clamp( r, 0, 255 ),
					clamp( g, 0, 255 ),
					clamp( b, 0, 255 ),
					clamp( a, 0, 255 )
					);

				Color colorEnd;
				r = m_colorEndRGBA.r() + WeakRandomInt( -1 * m_colorEndRGBAVariance.r(), m_colorEndRGBAVariance.r() );
				g = m_colorEndRGBA.g() + WeakRandomInt( -1 * m_colorEndRGBAVariance.g(), m_colorEndRGBAVariance.g() );
				b = m_colorEndRGBA.b() + WeakRandomInt( -1 * m_colorEndRGBAVariance.b(), m_colorEndRGBAVariance.b() );
				a = m_colorEndRGBA.a() + WeakRandomInt( -1 * m_colorEndRGBAVariance.a(), m_colorEndRGBAVariance.a() );

				colorEnd.SetColor( 
					clamp( r, 0, 255 ),
					clamp( g, 0, 255 ),
					clamp( b, 0, 255 ),
					clamp( a, 0, 255 )
					);

				// We let the system generate transparent particles, but no need to really emit one in that case
				if ( colorStart.a() == 0 && colorEnd.a() == 0 )
					continue;

				float flParticleSharpness = m_flSharpness + WeakRandomFloat( -1.0f * m_flSharpnessVariance, m_flSharpnessVariance );
				flParticleSharpness = clamp( flParticleSharpness, 0.0f, 1.0f );

				float flFlicker = m_flFlicker + WeakRandomFloat( -1.0f * m_flFlickerVariance, m_flFlickerVariance );
				flFlicker = clamp( flFlicker, 0.0f, 1.0f );

				float flTimeExpire = flFrameTime + MAX( 0.0f, m_flParticleLifeSpanSeconds + WeakRandomFloat( -1.0f * m_flParticleLifeSpanSecondsVariance, m_flParticleLifeSpanSecondsVariance ) );

				CAnimationParticle *pParticle = new CAnimationParticle( vecPosition, vecVelocity, colorStart, colorEnd, flParticleSize, flParticleSharpness, flFlicker, m_flLastEmitTime, flTimeExpire );
				m_vecParticles.AddToTail( pParticle );				
			}
			else
			{
				break;
			}
		}

		m_flLastRunTime = flFrameTime;
	}


#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();

		ValidateObj( m_vecParticles );
		FOR_EACH_VEC( m_vecParticles, i )
		{
			validator.ClaimMemory( m_vecParticles[i] );
		}
	}
#endif


private:

	Vector m_vecBasePosition;
	Vector m_vecBasePositionVariance;

	float m_flParticleSize;
	float m_flParticleSizeVariance;

	float m_flParticlesPerSecond;
	float m_flParticlesPerSecondVariance;

	float m_flParticleLifeSpanSeconds;
	float m_flParticleLifeSpanSecondsVariance;

	Vector m_vecParticleInitialVelocity;
	Vector m_vecParticleInitialVelocityVariance;

	Vector m_vecParticleVelocityMin;
	Vector m_vecParticleVelocityMax;

	Vector m_vecGravityAcceleration;
	Vector m_vecGravityAccelerationParticleVariance;

	Color m_colorStartRGBA;
	Color m_colorStartRGBAVariance;

	Color m_colorEndRGBA;
	Color m_colorEndRGBAVariance;

	float m_flSharpness;
	float m_flSharpnessVariance;

	float m_flFlicker;
	float m_flFlickerVariance;

	double m_flLastEmitTime;
	double m_flLastRunTime;
	
	CUtlVector<CAnimationParticle *> m_vecParticles;
};

inline bool IsVMatrix2DTranslateOnly( VMatrix const &m ) 
{
	return
		m[0][0] == 1.0f && m[0][1] == 0.0f && m[0][2] == 0.0f && /* translate x*/
		m[1][0] == 0.0f && m[1][1] == 1.0f && m[1][2] == 0.0f && /* translate y*/
		m[2][0] == 0.0f && m[2][1] == 0.0f && m[2][2] == 1.0f && m[2][3] == 0.0f &&
		m[3][0] == 0.0f && m[3][1] == 0.0f && m[3][2] == 0.0f && m[3][3] == 1.0f;
}


// Stack of transform/animation context currently in use for the active frame
class CAnimationAndTransformContext
{
public:
	CAnimationAndTransformContext()
	{
		m_ulContextID = k_ulInvalidPanelHandle64;
		m_TransformMatrix = VMatrix::GetIdentityMatrix();
		m_flPosX = m_flPosY = m_flPosZ = 0;
		m_flCurrentDrawingOffsetX = m_flCurrentDrawingOffsetY = m_flCurrentDrawingOffsetZ = 0.0f;
		m_bHasCompositionLayer = false;
		m_flPerspective = 1000.0f;
		m_flPerspectiveOriginX = 0.0f;
		m_flPerspectiveOriginY = 0.0f;
		m_flTransformOriginX = 0.0f;
		m_flTransformOriginY = 0.0f;
		m_flTransformOriginXUnoffset = 0.0f;
		m_flTransformOriginYUnoffset = 0.0f;
		m_flOpacity = 1.0f;
		m_flHueShift = 0.0f;
		m_flSaturation = 1.0f;
		m_flBrightness = 1.0f;
		m_flContrast = 1.0f;
		m_flOpacityMaskOpacity = 1.0f;
		m_compositionColor = Color( 255, 255, 255, 255 );
		m_bFastCompositionColor = false;
		m_bPassedHitTest = false;
		m_flWidth = 0;
		m_flHeight = 0;
		m_flContentScrollX = 0.0f;
		m_flContentScrollY = 0.0f;
		m_vMouse.x = 0.0f;
		m_vMouse.y = 0.0f;
		m_flBlurPasses = 0.0f;
		m_flBlurStdDevHor = 0.0f;
		m_flBlurStdDevVer = 0.0f;
		m_blurType = BT_NORMAL;
		m_bHasClipLayer = false;
		m_bBoxShadowInset = false;
		m_bBoxShadowFill = false;
		m_flBoxShadowHorOffset = 0.0f;
		m_flBoxShadowVerOffset = 0.0f;
		m_flBoxShadowBlurRadius = 0.0f;
		m_flBoxShadowSpreadDistance = 0.0f;
		m_bBoxShadowAnimating = false;
		m_flScale2DX = 1.0f;
		m_flScale2DY = 1.0f;
		m_flRotate2D = 0.0f;
		m_bBoxShadowSet = false;
		m_colBoxShadow.SetRawColor( 0x00000000 );
		m_bChildrenHave3DTransforms = false;
		m_bWantsHitTest = false;
		V_memset( m_rgBorderWidths, 0, sizeof( m_rgBorderWidths ) );
		m_bBorderWidthSet = false;
		V_memset( m_rgCornerRaddi, 0, sizeof( m_rgCornerRaddi ) );
		m_bCornerRaddiSet = false;
		m_bMightScroll = false;
		m_bHasPanelContextPushed = false;

		m_flTextShadowHorOffset = 0.0f;
		m_flTextShadowVerOffset = 0.0f;
		m_flTextShadowBlurRadius = 0.0f;
		m_bTextShadowSet = false;
		m_colTextShadow.SetRawColor( 0x00000000 );
		m_bTextShadowAnimating = false;

		m_flImageShadowHorOffset = 0.0f;
		m_flImageShadowVerOffset = 0.0f;
		m_flImageShadowBlurRadius = 0.0f;
		m_bImageShadowSet = false;
		m_colImageShadow.SetRawColor( 0x00000000 );
		m_bImageShadowAnimating = false;

		m_bHasExplicitClipRect = false;
		m_bHasRadialClip = false;
		m_eMixBlendMode = k_EMixBlendModeNormal;
		m_bHasOpaqueBackground = false;
		m_bNeedsIntermediateTexture = false;
		m_bNoClip = false;
		m_bHasChildPanels = false;
		m_bClipAfterTransform = false;
		m_bWantsScreenspaceQuadOutput = false;
		m_pScreenspaceQuad = NULL;
		m_bRequireCompositionLayer = false;
		m_bAlwaysCacheCompositionLayer = false;
		m_bForceNoCompositionLayer = false;
		m_bOffscreenCompositionLayer = false;
		m_eFractionalPixelPositions = k_EFractionalPixelPositionsDefault;
		m_bHasTransformMatrix = false;
	}

	~CAnimationAndTransformContext()
	{
		if ( m_pScreenspaceQuad )
		{
			delete[] m_pScreenspaceQuad;
			m_pScreenspaceQuad = NULL;
		}
	}

	void SetContext( uint64 ulContextID ) { m_ulContextID = ulContextID; }
	uint64 GetContext() { return m_ulContextID; }

	void SetNoClip( bool bNoClip ) { m_bNoClip = bNoClip; }
	bool GetNoClip() { return m_bNoClip; }

	void SetHasChildPanels( bool bHasChildPanels ) { m_bHasChildPanels = bHasChildPanels; }

	void SetMousePosition( Vector2D vMouse ) { m_vMouse = vMouse; }
	Vector2D GetMousePosition() { return m_vMouse; }

	void SetPassedHitTest( bool bPassed ) { m_bPassedHitTest = bPassed; }
	bool BPassedHitTest() { return m_bPassedHitTest; }

	void SetWantsHitTest( bool bWantsHitTest ) { m_bWantsHitTest = bWantsHitTest; }
	bool BWantsHitTest() { return m_bWantsHitTest; }

	void SetWantsScreenspaceQuadOutput( bool bWantsScreenspaceQuadOutput ) { m_bWantsScreenspaceQuadOutput = bWantsScreenspaceQuadOutput; }
	bool BWantsScreenspaceQuadOutput() { return m_bWantsScreenspaceQuadOutput; }
	void SetScreenspaceQuad(Vector2D *pQuad) 
	{ 
#if 0
		try
		{
			CPanelPtr< CUIPanel > ptr;
			ptr.SetFromUInt64( GetContext() );

			Msg( "Panel %s (%s) has screenspace quad: %1.2f,%1.2f, %1.2f,%1.2f, %1.2f,%1.2f, %1.2f,%1.2f\n", ptr->GetID(), ptr->GetPanelType().String(),
				pQuad[0].x, pQuad[0].y, pQuad[1].x, pQuad[1].y, pQuad[2].x, pQuad[2].y, pQuad[3].x, pQuad[3].y );
		}
		catch ( ... )
		{

		}
#endif

		m_pScreenspaceQuad = new Vector2D[4];
		V_memcpy( m_pScreenspaceQuad, pQuad, sizeof( Vector2D ) * 4 );
	}

	Vector2D *TakeOverScreenspaceQuadPtr()
	{
		Vector2D *pOut = m_pScreenspaceQuad;
		m_pScreenspaceQuad = NULL;

		return pOut;
	}


	void SetWantsHitTestChildren( bool bWantsHitTestChildren ) { m_bWantsHitTestChildren = bWantsHitTestChildren; }
	bool BWantsHitTestChildren() { return m_bWantsHitTestChildren; }

	bool BHasOpaqueBackground() { return m_bHasOpaqueBackground; }
	void SetHasOpaqueBackground( bool bOpaque ) { m_bHasOpaqueBackground = bOpaque; }

	bool BNeedsIntermediateTexture() { return m_bNeedsIntermediateTexture; }
	void SetNeedsIntermediateTexture( bool bNeedsIntermediateTexture ) { m_bNeedsIntermediateTexture = bNeedsIntermediateTexture; }

	void SetCompositionLayerTextureName( const char *pszCompositionLayerTextureName ) { m_symCompositionLayerTextureName = pszCompositionLayerTextureName; }
	const char *GetCompositionLayerTextureName() const { return m_symCompositionLayerTextureName.IsValid() ? m_symCompositionLayerTextureName.String() : nullptr; }

	bool BClipAfterTransform() { return m_bClipAfterTransform; }
	void SetClipAfterTransform( bool bClipAfterTransform ) { m_bClipAfterTransform = bClipAfterTransform; }

	void SetSize( float flWidth, float flHeight ) { m_flWidth = flWidth; m_flHeight = flHeight; }
	void GetSize( float &flWidth, float &flHeight ) { flWidth = m_flWidth; flHeight = m_flHeight; }

	void SetHasCompositionLayer( bool bHasLayer ) { m_bHasCompositionLayer = bHasLayer; }
	bool BHasCompositionLayer() { return m_bHasCompositionLayer; }

	void SetClipLayerPushed( bool bValue ) { m_bHasClipLayer = bValue; }
	bool BHasClipLayerPushed() { return m_bHasClipLayer; }

	void SetPanelContextPushed( bool bValue ) { m_bHasPanelContextPushed = bValue; }
	bool BHasPanelContextPushedPushed() { return m_bHasPanelContextPushed; }

	const VMatrix &GetTransformMatrix() { return m_bHasTransformMatrix ? m_TransformMatrix : VMatrix::GetIdentityMatrix(); }
	void SetTransformMatrix( const VMatrix &matrix ) { m_TransformMatrix = matrix; m_bHasTransformMatrix = true; }
	bool BHasTransformMatrix() const { return m_bHasTransformMatrix; }

	void CheckTransformIs2DTranslateAndCombineWithPosition()
	{
		// Check if all we have is 2d translate then merge into position and clear transform
		if ( IsVMatrix2DTranslateOnly( m_TransformMatrix ) && s_convarPanoramaAll2DTranslatesDontNeedCompositonLayer.GetBool() )
		{
			m_flPosX += m_TransformMatrix[0][3];
			m_flPosY += m_TransformMatrix[1][3];
			m_TransformMatrix.SetElement( 3, 0, 0.0f );
			m_TransformMatrix.SetElement( 3, 1, 0.0f );
			m_bHasTransformMatrix = true;
		}
	}

	void SetOpacity( float flOpacity ) { m_flOpacity = flOpacity; }
	float GetOpacity() { return m_flOpacity; }

	void SetHueShift( float flHueShift ) { m_flHueShift = flHueShift; }
	float GetHueShift() { return m_flHueShift; }

	void SetSaturation( float flSaturation ) { m_flSaturation = flSaturation; }
	float GetSaturation() { return m_flSaturation; }

	void SetBrightness( float flBrightness ) { m_flBrightness = flBrightness; }
	float GetBrightness() { return m_flBrightness; }

	void SetContrast( float flContrast ) { m_flContrast = flContrast; }
	float GetContrast() { return m_flContrast; }

	void SetCompositionColor( Color color, bool bFast ) { m_compositionColor = color; m_bFastCompositionColor = bFast; }
	Color GetCompositionColor() { return m_compositionColor; }
	bool BIsFastCompositionColor() const { return m_bFastCompositionColor; }

	void SetBlurStdDeviationHor( float flStdDev ) { m_flBlurStdDevHor = flStdDev; }
	float GetBlurStdDeviationHor() { return m_flBlurStdDevHor; }

	void SetBlurStdDeviationVer( float flStdDev ) { m_flBlurStdDevVer = flStdDev; }
	float GetBlurStdDeviationVer() { return m_flBlurStdDevVer; }

	void SetBlurPasses( float flPasses ) { m_flBlurPasses = flPasses; }
	float GetBlurPasses() { return m_flBlurPasses; }

	void SetBlurType( BlurType_t blurType ) { m_blurType = blurType; }
	BlurType_t GetBlurType() { return m_blurType; }

	void SetPerspective( float flPerspective ) { m_flPerspective = flPerspective; }
	float GetPerspective() { return m_flPerspective; }

	void SetPerspectiveOrigin( float x, float y ) { m_flPerspectiveOriginX = x; m_flPerspectiveOriginY = y; }
	float GetPerspectiveOriginX() { return m_flPerspectiveOriginX; }
	float GetPerspectiveOriginY() { return m_flPerspectiveOriginY; }

	void SetTransformOrigin( float x, float y, float xParentCompOffset, float yParentCompOffset ) 
	{ 
		m_flTransformOriginX = x + xParentCompOffset;
		m_flTransformOriginY = y + yParentCompOffset;

		m_flTransformOriginXUnoffset = x;
		m_flTransformOriginYUnoffset = y;

	}
	float GetTransformOriginX() { return m_flTransformOriginX; }
	float GetTransformOriginY() { return m_flTransformOriginY; }
	float GetTransformOriginXUnoffset() { return m_flTransformOriginXUnoffset; }
	float GetTransformOriginYUnoffset() { return m_flTransformOriginYUnoffset; }

	void SetContentsScroll( float x, float y ) { m_flContentScrollX = x; m_flContentScrollY = y; m_bMightScroll = true; }
	void GetContentsScroll( float &x, float &y ) { x = m_flContentScrollX; y = m_flContentScrollY; }
	bool BMightScroll() { return m_bMightScroll; }

	void SetPosition( float x, float y, float z ) 
	{ 
		m_flPosX = x; 
		m_flPosY = y; 
		m_flPosZ = z; 

		Assert( IsFinite( m_flPosX ) );
		Assert( IsFinite( m_flPosY ) );
		Assert( IsFinite( m_flPosZ ) );
	}
	void GetPosition( float &x, float &y, float &z )
	{
		x = m_flPosX;
		y = m_flPosY;
		z = m_flPosZ;
	}

	void SetCurrentDrawingOffsets( float x, float y, float z )
	{
		m_flCurrentDrawingOffsetX = x;
		m_flCurrentDrawingOffsetY = y;
		m_flCurrentDrawingOffsetZ = z;
	}
	void GetCurrentDrawingOffset( float &x, float &y, float &z )
	{
		x = m_flCurrentDrawingOffsetX;
		y = m_flCurrentDrawingOffsetY;
		z = m_flCurrentDrawingOffsetZ;
	}

	void GetScale2DFactors( float &x, float &y ) { x = m_flScale2DX; y = m_flScale2DY; }
	void SetScale2DFactors( float x, float y ) { m_flScale2DX = x; m_flScale2DY = y; }

	void GetRotate2D( float &flDegrees ) { flDegrees = m_flRotate2D; }
	void SetRotate2D( float flDegrees ) { m_flRotate2D = flDegrees; }

	void SetChildrenHave3DTransforms( bool bChildrenHave3DTransforms ) { m_bChildrenHave3DTransforms = bChildrenHave3DTransforms; }

	void SetMixBlendMode( EMixBlendMode mode ) { m_eMixBlendMode = mode; }
	EMixBlendMode GetMixBlendMode() { return m_eMixBlendMode; }

	void SetOpacityMaskTexture( panorama::IUITexture *pTexture, float flOpacity )
	{
		m_pOpacityMaskTexture.SafeRelease();

		m_pOpacityMaskTexture = pTexture;
		m_flOpacityMaskOpacity = flOpacity;

		if ( m_pOpacityMaskTexture )
			m_pOpacityMaskTexture->AddRef();
	}

	panorama::IUITexture *GetOpacityMaskTexture() { return m_pOpacityMaskTexture; }
	float GetOpacityMaskOpacity() { return m_flOpacityMaskOpacity; }

	bool BBorderWidthSet() { return m_bBorderWidthSet; }

	void SetBorder( float topWidth, float rightWidth, float bottomWidth, float leftWidth, Color colTop, Color colRight, Color colBottom, Color colLeft )
	{
		m_bBorderWidthSet = true;
		m_rgBorderWidths[0] = topWidth;
		m_rgBorderWidths[1] = rightWidth;
		m_rgBorderWidths[2] = bottomWidth;
		m_rgBorderWidths[3] = leftWidth;

		m_rgBorderColors[0] = colTop;
		m_rgBorderColors[1] = colRight;
		m_rgBorderColors[2] = colBottom;
		m_rgBorderColors[3] = colLeft;
	}

	void GetBorder( float &topWidth, float &rightWidth, float &bottomWidth, float &leftWidth, Color &colTop, Color &colRight, Color &colBottom, Color &colLeft )
	{
		topWidth = m_rgBorderWidths[0];
		rightWidth = m_rgBorderWidths[1];
		bottomWidth = m_rgBorderWidths[2];
		leftWidth = m_rgBorderWidths[3];

		colTop = m_rgBorderColors[0];
		colRight = m_rgBorderColors[1];
		colBottom = m_rgBorderColors[2];
		colLeft = m_rgBorderColors[3];
	}

	bool BBorderRadiusSet() { return m_bCornerRaddiSet; }

	void SetBorderRadius( float flTopLeftHorizontal, float flTopLeftVertical, float flTopRightHorizontal, float flTopRightVertical,
		float flBottomRightHorizontal, float flBottomRightVertical, float flBottomLeftHorizontal, float flBottomLeftVertical )
	{
		m_rgCornerRaddi[0] = flTopLeftHorizontal;
		m_rgCornerRaddi[1] = flTopLeftVertical;
		m_rgCornerRaddi[2] = flTopRightHorizontal;
		m_rgCornerRaddi[3] = flTopRightVertical;
		m_rgCornerRaddi[4] = flBottomRightHorizontal;
		m_rgCornerRaddi[5] = flBottomRightVertical;
		m_rgCornerRaddi[6] = flBottomLeftHorizontal;
		m_rgCornerRaddi[7] = flBottomLeftVertical;

		for ( int i = 0; i < V_ARRAYSIZE( m_rgCornerRaddi ); ++i )
		{
			if ( m_rgCornerRaddi[i] != 0.0f )
			{
				m_bCornerRaddiSet = true;
				break;
			}
		}
	}


	void GetCornerRadius( float &flTopLeftHorizontal, float &flTopLeftVertical, float &flTopRightHorizontal, float &flTopRightVertical,
		float &flBottomRightHorizontal, float &flBottomRightVertical, float &flBottomLeftHorizontal, float &flBottomLeftVertical )
	{
		flTopLeftHorizontal = m_rgCornerRaddi[0];
		flTopLeftVertical = m_rgCornerRaddi[1];
		flTopRightHorizontal = m_rgCornerRaddi[2];
		flTopRightVertical = m_rgCornerRaddi[3];
		flBottomRightHorizontal = m_rgCornerRaddi[4];
		flBottomRightVertical = m_rgCornerRaddi[5];
		flBottomLeftHorizontal = m_rgCornerRaddi[6];
		flBottomLeftVertical = m_rgCornerRaddi[7];
	}

	bool BBoxShadowSet() { return m_bBoxShadowSet; }

	void SetBoxShadow( bool bInset, bool bFill, float flHorizontalOffset, float flVerticalOffset, float flBlurRadius, float flSpreadDistance, Color colShadow, bool bAnimating )
	{
		m_bBoxShadowSet = true;
		m_bBoxShadowInset = bInset;
		m_bBoxShadowFill = bFill;
		m_flBoxShadowHorOffset = flHorizontalOffset;
		m_flBoxShadowVerOffset = flVerticalOffset;
		m_flBoxShadowBlurRadius = flBlurRadius;
		m_flBoxShadowSpreadDistance = flSpreadDistance;
		m_colBoxShadow = colShadow;
		m_bBoxShadowAnimating = bAnimating;
	}

	void GetBoxShadow( bool &bInset, bool &bFill, float &flHorizontalOffset, float &flVerticalOffset, float &flBlurRadius, float &flSpreadDistance, Color &colShadow, bool &bAnimating )
	{
		bInset = m_bBoxShadowInset;
		bFill = m_bBoxShadowFill;
		flHorizontalOffset = m_flBoxShadowHorOffset;
		flVerticalOffset = m_flBoxShadowVerOffset;
		flBlurRadius = m_flBoxShadowBlurRadius;
		flSpreadDistance = m_flBoxShadowSpreadDistance;
		colShadow = m_colBoxShadow;
		bAnimating = m_bBoxShadowAnimating;
	}

	bool BTextShadowSet() { return m_bTextShadowSet; }

	void SetTextShadow( float flHorizontalOffset, float flVerticalOffset, float flBlurRadius, float flStrength, Color colShadow, bool bAnimating )
	{
		m_bTextShadowSet = true;
		m_flTextShadowHorOffset = flHorizontalOffset;
		m_flTextShadowVerOffset = flVerticalOffset;
		m_flTextShadowBlurRadius = flBlurRadius;
		m_flTextShadowStrength = flStrength;
		m_colTextShadow = colShadow;
		m_bTextShadowAnimating = bAnimating;
	}

	void GetTextShadow( float &flHorizontalOffset, float &flVerticalOffset, float &flBlurRadius, float &flStrength, Color &colShadow, bool &bAnimating )
	{
		flHorizontalOffset = m_flTextShadowHorOffset;
		flVerticalOffset = m_flTextShadowVerOffset;
		flBlurRadius = m_flTextShadowBlurRadius;
		flStrength = m_flTextShadowStrength;
		colShadow = m_colTextShadow;
		bAnimating = m_bTextShadowAnimating;
	}

	bool BImageShadowSet() { return m_bImageShadowSet; }

	void SetImageShadow( float flHorizontalOffset, float flVerticalOffset, float flBlurRadius, float flStrength, Color colShadow, bool bAnimating )
	{
		m_bImageShadowSet = true;
		m_flImageShadowHorOffset = flHorizontalOffset;
		m_flImageShadowVerOffset = flVerticalOffset;
		m_flImageShadowBlurRadius = flBlurRadius;
		m_flImageShadowStrength = flStrength;
		m_colImageShadow = colShadow;
		m_bImageShadowAnimating = bAnimating;
	}

	void GetImageShadow( float &flHorizontalOffset, float &flVerticalOffset, float &flBlurRadius, float &flStrength, Color &colShadow, bool &bAnimating )
	{
		flHorizontalOffset = m_flImageShadowHorOffset;
		flVerticalOffset = m_flImageShadowVerOffset;
		flBlurRadius = m_flImageShadowBlurRadius;
		flStrength = m_flImageShadowStrength;
		colShadow = m_colImageShadow;
		bAnimating = m_bImageShadowAnimating;
	}

	bool BHasExplicitClipRect()
	{
		return m_bHasExplicitClipRect;
	}

	void SetExplicitClipRect( float flLeft, float flTop, float flRight, float flBottom )
	{
		m_bHasExplicitClipRect = true;
		m_rgflClipRect[0] = flLeft;
		m_rgflClipRect[1] = flTop;
		m_rgflClipRect[2] = flRight;
		m_rgflClipRect[3] = flBottom;
	}

	void GetExplicitClipRect( float *pflLeft, float *pflTop, float *pflRight, float *pflBottom )
	{
		Assert( m_bHasExplicitClipRect );
		*pflLeft = m_rgflClipRect[0];
		*pflTop = m_rgflClipRect[1];
		*pflRight = m_rgflClipRect[2];
		*pflBottom = m_rgflClipRect[3];
	}

	void SetRadialClip( float x, float y, float flStartAngle, float flSectorAngle )
	{
		m_bHasRadialClip = true;
		m_flRadialClipCenterX = x;
		m_flRadialClipCenterY = y;
		m_flRadialClipStartAngle = flStartAngle;
		m_flRadialClipSectorAngle = flSectorAngle;
	}

	bool BHasRadialClip()
	{
		return m_bHasRadialClip;
	}

	void GetRadialClip( float *pflX, float *pflY, float *pflStartAngle, float *pflSectorAngle )
	{
		Assert( m_bHasRadialClip );
		*pflX = m_flRadialClipCenterX;
		*pflY = m_flRadialClipCenterY;
		*pflStartAngle = m_flRadialClipStartAngle;
		*pflSectorAngle = m_flRadialClipSectorAngle;
	}

	bool BRequireCompositionLayer() const
	{
		return m_bRequireCompositionLayer;
	}
	void SetRequireCompositionLayer( bool bValue )
	{
		m_bRequireCompositionLayer = bValue;
	}

	bool BAlwaysCacheCompositionLayer() const
	{
		return m_bAlwaysCacheCompositionLayer;
	}
	void SetAlwaysCacheCompositionLayer( bool bValue )
	{
		m_bAlwaysCacheCompositionLayer = bValue;
	}

	bool BForceNoCompositionLayer() const
	{
		return m_bForceNoCompositionLayer;
	}
	void SetForceNoCompositionLayer( bool bValue )
	{
		m_bForceNoCompositionLayer = bValue;
	}

	bool BOffscreenCompositionLayer() const
	{
		return m_bOffscreenCompositionLayer;
	}
	void SetOffscreenCompositionLayer( bool bValue )
	{
		m_bOffscreenCompositionLayer = bValue;
	}

	EFractionalPixelPositions GetFractionalPixelPositions() const
	{
		return m_eFractionalPixelPositions;
	}
	void SetFractionalPixelPositions( EFractionalPixelPositions eFrationalPixelPositions )
	{
		m_eFractionalPixelPositions = eFrationalPixelPositions;
	}

	void LogCompositionLayerReason( const char *pchReason )
	{
		REFERENCE( pchReason );
#if 0
		try
		{
			CPanelPtr< CUIPanel > ptr;
			ptr.SetFromUInt64( GetContext() );

			Msg( "Panel %s (%s) needs composition layer because: %s\n", ptr->GetID(), ptr->GetPanelType().String(), pchReason );
		}
		catch( ... )
		{

		}
#endif
	}

	// If we don't decide we need a composition layer, then do we need to push a lighter weight panel context to 
	// draw some extra stuff or accumulate transforms, etc?
	bool BNeedsPanelContextIfNoCompositonLayer()
	{
		if ( m_bHasTransformMatrix && !m_TransformMatrix.IsIdentity() )
			return true;

		if ( m_colBoxShadow.GetRawColor() != 0x00000000 )
			return true;

		if ( BBorderWidthSet() )
		{
			for ( int i = 0; i < V_ARRAYSIZE( m_rgBorderWidths ); ++i )
			{
				if ( m_rgBorderWidths[i] > 0.0f )
				{
					return true;
				}
			}
		}

		if ( m_bChildrenHave3DTransforms )
			return true;

		if ( m_compositionColor.GetRawColor() != 0xffffffff && m_compositionColor.a() != 0x00 && m_bFastCompositionColor )
		{
			return true;
		}

		if ( m_eMixBlendMode != k_EMixBlendModeNormal )
		{
			return true;
		}

		if ( m_eFractionalPixelPositions != k_EFractionalPixelPositionsDefault )
		{
			return true;
		}

		return false;
	}


	// Do we need an offscreen texture for composition of this panel context in the render layer?
	bool BNeedsCompositionLayer()
	{
		if ( m_bForceNoCompositionLayer )
			return false;

		if ( m_flPosZ != 0.00 )
		{
			LogCompositionLayerReason( "m_flPosZ" );
			return true;
		}

		if ( m_flOpacity != 1.0f )
		{
			LogCompositionLayerReason( "m_flOpacity" );
			return true;
		}

		if ( m_flHueShift != 0.0f )
		{
			LogCompositionLayerReason( "m_flHueShift" );
			return true;
		}

		if ( m_flSaturation != 1.0f )
		{
			LogCompositionLayerReason( "m_flSaturation" );
			return true;
		}

		if ( m_flBrightness != 1.0f )
		{
			LogCompositionLayerReason( "m_flBrightness" );
			return true;
		}

		if ( m_flContrast != 1.0f )
		{
			LogCompositionLayerReason( "m_flContrast" );
			return true;
		}

		if ( m_bRequireCompositionLayer )
		{
			LogCompositionLayerReason( "m_bRequireCompositionLayer" );
			return true;
		}
		
		if ( m_bMightScroll && !s_convarPanoramaMightScrollDontNeedCompositonLayer.GetBool() )
		{
			LogCompositionLayerReason( "m_bMightScroll" );
			return true;
		}

		if ( m_flContentScrollX != 0.0f || m_flContentScrollY != 0.0f )
		{
			LogCompositionLayerReason( "Is scrolled" );
			return true;
		}

		if ( m_bHasTransformMatrix && !m_TransformMatrix.IsIdentity() )
		{
			if ( !s_convarPanoramaAllTransformsDontNeedCompositonLayer.GetBool() )
			{
				LogCompositionLayerReason( "m_TransformMatrix" );
				return true;
			}
			else if ( !m_bClipAfterTransform )
			{
				// If the panel has a transform it needs a layer still mostly because of clipping needing to occur pre-transform, then transform later and clip to parent
				// However, if the panel has noclip and it also has no children (who might need clipping) then we can skip the layer.
				if ( !m_bNoClip || m_bHasChildPanels )
				{
					LogCompositionLayerReason( "m_TransformMatrix" );
					return true;
				}
			}
		}

		if( m_pOpacityMaskTexture && m_flOpacityMaskOpacity > 0.0f )
		{
			LogCompositionLayerReason( "m_pOpacityMaskTexture" );
			return true;
		}

		if( m_compositionColor.GetRawColor() != 0xffffffff && m_compositionColor.a() != 0x00 && !m_bFastCompositionColor )
		{
			LogCompositionLayerReason( "m_compositionColor" );
			return true;
		}

		if( m_flBlurPasses > 0.0f && (m_flBlurStdDevHor > 0.0f || m_flBlurStdDevVer > 0.0f) )
		{
			LogCompositionLayerReason( "m_flBlurPasses" );
			return true;
		}

		if( m_colBoxShadow.GetRawColor() != 0x00000000 && !s_convarPanoramaBoxShadowsDontNeedCompositonLayer.GetBool() )
		{
			LogCompositionLayerReason( "m_colBoxShadow" );
			return true;
		}

		if ( m_bChildrenHave3DTransforms && !s_convarPanoramaTransformParentsNoLayerForPerspective.GetBool() )
		{
#if !defined( SOURCE2_PANORAMA )
			if ( s_convarPanoramaTransformParentsNoLayerIfNoPerspective.GetBool() )
				LogCompositionLayerReason( "m_bChildrenHave3DTransformsImpactingZPerspective" );
			else
				LogCompositionLayerReason( "m_bChildrenHave3DTransforms" );
#else
			LogCompositionLayerReason( "m_bChildrenHave3DTransformsImpactingZPerspective" );
#endif

			return true;
		}

		if( m_flScale2DX != 1.0f || m_flScale2DY != 1.0f )
		{
			LogCompositionLayerReason( "m_flScale2DX/Y" );
			return true;
		}

		if( m_flRotate2D > 0.00001f || m_flRotate2D < -0.00001f )
		{
			LogCompositionLayerReason( "m_flRotate2D" );
			return true;
		}

		for( int i=0; i < V_ARRAYSIZE( m_rgCornerRaddi ); ++i )
		{
			if( m_rgCornerRaddi[i] != 0.0f )
			{
				LogCompositionLayerReason( "m_rgCornerRaddi" );
				return true;
			}
		}

		if ( !s_convarPanoramaSimpleBordersDontNeedCompositonLayer.GetBool() )
		{
			for ( int i = 0; i < V_ARRAYSIZE( m_rgBorderWidths ); ++i )
			{
				if ( m_rgBorderWidths[i] > 0.0f )
				{
					LogCompositionLayerReason( "m_rgBorderWidths" );
					return true;
				}
			}
		}

		// Skip composition layer if the panel doesn't have any children
		// and use a cheaper panel context instead
		if( m_eMixBlendMode != k_EMixBlendModeNormal && m_bHasChildPanels )
		{
			LogCompositionLayerReason( "m_eMixBlendMode & panel with children" );
			return true;
		}

		if ( m_bNeedsIntermediateTexture )
		{
			LogCompositionLayerReason( "m_bNeedsIntermediateTexture" );
			return true;
		}

		if ( m_symCompositionLayerTextureName.IsValid() )
		{
			LogCompositionLayerReason( "m_symCompositionLayerTextureName" );
			return true;
		}

		if ( m_bHasRadialClip )
		{
			LogCompositionLayerReason( "m_bHasRadialClip" );
			return true;
		}

		return false;
	}

private:

	float m_flPosX, m_flPosY, m_flPosZ;
	float m_flContentScrollX, m_flContentScrollY;
	float m_flCurrentDrawingOffsetX, m_flCurrentDrawingOffsetY, m_flCurrentDrawingOffsetZ;
	Color m_compositionColor;
	bool m_bFastCompositionColor;
	float m_flOpacity;
	float m_flHueShift;
	float m_flSaturation;
	float m_flBrightness;
	float m_flContrast;
	float m_flBlurPasses;
	float m_flBlurStdDevHor;
	float m_flBlurStdDevVer;
	BlurType_t  m_blurType;
	uint64 m_ulContextID;
	EMixBlendMode m_eMixBlendMode;
	EFractionalPixelPositions m_eFractionalPixelPositions;
	VMatrix m_TransformMatrix;
	float m_flPerspective;
	CRefPtr< panorama::IUITexture > m_pOpacityMaskTexture;
	float m_flOpacityMaskOpacity;
	float m_flPerspectiveOriginX, m_flPerspectiveOriginY;
	float m_flTransformOriginX, m_flTransformOriginY;
	float m_flTransformOriginXUnoffset, m_flTransformOriginYUnoffset;
	bool m_bChildrenHave3DTransforms;
	bool m_bHasCompositionLayer;
	bool m_bHasClipLayer;
	float m_rgBorderWidths[4];
	Color m_rgBorderColors[4];
	float m_rgCornerRaddi[8];
	bool m_bBoxShadowInset;
	bool m_bBoxShadowFill;
	float m_flBoxShadowHorOffset;
	float m_flBoxShadowVerOffset;
	float m_flBoxShadowBlurRadius;
	float m_flBoxShadowSpreadDistance;
	Color m_colBoxShadow;
	bool m_bBoxShadowAnimating;
	bool m_bPassedHitTest;
	float m_flWidth;
	float m_flHeight;
	Vector2D m_vMouse;
	float m_flScale2DX;
	float m_flScale2DY;
	float m_flRotate2D;
	bool m_bWantsHitTest;
	bool m_bWantsHitTestChildren;
	bool m_bMightScroll;
	bool m_bHasOpaqueBackground;
	bool m_bNeedsIntermediateTexture;
	bool m_bClipAfterTransform;
	bool m_bHasTransformMatrix;
	CPanoramaSymbol m_symCompositionLayerTextureName;

	bool m_bHasChildPanels;
	bool m_bNoClip;
	bool m_bHasExplicitClipRect;
	float m_rgflClipRect[4];

	bool m_bHasRadialClip;
	float m_flRadialClipCenterX;
	float m_flRadialClipCenterY;
	float m_flRadialClipStartAngle;
	float m_flRadialClipSectorAngle;

	bool m_bBoxShadowSet;
	bool m_bBorderWidthSet;
	bool m_bCornerRaddiSet;

	float m_flTextShadowHorOffset;
	float m_flTextShadowVerOffset;
	float m_flTextShadowBlurRadius;
	float m_flTextShadowStrength;
	bool m_bTextShadowSet;
	Color m_colTextShadow;
	bool m_bTextShadowAnimating;

	float m_flImageShadowHorOffset;
	float m_flImageShadowVerOffset;
	float m_flImageShadowBlurRadius;
	float m_flImageShadowStrength;
	bool m_bImageShadowSet;
	Color m_colImageShadow;
	bool m_bImageShadowAnimating;

	bool m_bRequireCompositionLayer;
	bool m_bAlwaysCacheCompositionLayer;
	bool m_bForceNoCompositionLayer;
	bool m_bOffscreenCompositionLayer;

	bool m_bHasPanelContextPushed;
	bool m_bWantsScreenspaceQuadOutput;
	Vector2D *m_pScreenspaceQuad;
};


class CUIAnimationEngine
{
public:

	// Constructor
	CUIAnimationEngine( CUIRenderEngine *pRenderEngine );

	// Destructor
	~CUIAnimationEngine();

	// Get framerate average for the animation engine
	float GetFPSAverage();
	float GetSessionFPSAverages();

	// Set the number of times we've render the frame we are currently animating
	void SetRenderCountThisFrame( uint32 unRenderCountThisFrame ) 
	{ 
		m_unRenderCountThisFrame = unRenderCountThisFrame; 
		if ( m_unRenderCountThisFrame == 1 )
		{
			m_treePanelsThatNeededLayersThisFrame.RemoveAll();

			CleanupStoppedTransitions();
		}
	}

	// Called on window resize, no thread safe, occurs on animation thread
	void OnWindowResize( uint32 unSurfaceWidth, uint32 unSurfaceHeight );

	// Set current mouse position into animation engine
	void SetMousePosition( float x, float y ) 
	{ 
		m_flMouseX = x; 
		m_flMouseY = y; 
	}

	// set the vector of panel handles we want to track mouse move for
	void SetTrackingMousePanels( CCopyableUtlVector<uint64> vec )
	{
		m_vecTrackingMousePanels = vec;
		m_vecTrackingMouseResults.RemoveAll(); // clear our results from last time
	}

	// get the vector of mouse move info we accumulated
	CCopyableUtlVector<MouseTrackingResults_t> GetMouseTrackingResults()
	{
		 return m_vecTrackingMouseResults;
	}

	// Get the time of the frame we last finished, this is the generation time from the layout thread
	float GetLastFinishedFrameTime() { return (float)m_flFinishedFrameGenerationTime; }

	// Get the last panel that passed hit testing for the last completed frame
	uint64 GetLastHitTestPanelPtr() { return m_ulPanelHitTestPtrValue; }

	// Get the position within the last panel that passed hit testing that was specifically hit
	Vector2D GetLastHitTestPanelCoords() { return m_PanelHitTestCoords; }

	// Called at the start of each frame, means we should reset state
	void BeginFrame( const BeginFrameRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList );

	// Called at the end of each frame, means we should reset state
	void EndFrame( const EndFrameRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList );

	// Called to push additional animation/transform state
	void PushAnimationAndTransformContext( const PushAAndTContextRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList );

	// Called to pop previous animation/transform state
	void PopAnimationAndTransformContext( const PopAAndTContextRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList );

	// Called at the start of paint background for each panel
	void BeginPaintBackground( const BeginPaintBackgroundRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList );

	// Called at the end of paint background for each panel
	void EndPaintBackground( const EndPaintBackgroundRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList );

	// Called at the start of paint last for each panel
	void BeginPaintLast( const BeginPaintLastRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList );

	// Called at the end of paint last for each panel
	void EndPaintLast( const EndPaintLastRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList );

	// Handle animation/transform on quad
	void DrawTexturedRect( const DrawTexturedRectRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList );

	// Handle animation/transform on quad
	void DrawFilledRect( const DrawFilledRectRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList );

	// Handle animation/transform on text region
	void DrawTextRegion( const DrawTextRegionRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList );

	// Handle animation/transform on render callback request (basically may just cull)
	void RequestRenderCallback( const RequestRenderCallbackCommand_t &renderCommand, CRenderCommandList &outputCommandList );

	// Handle deleting a particle system
	void DeleteParticleSystem( const DeleteParticleSystemRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList );

	// Handle deleting a panel
	void DeletePanel( const DeletePanelRenderCommand_t &renderCommand, CRenderCommandList &outputCommandList );

	// Tells animation thread to stop further interpolating property and get back the time we stopped it at for layout 
	// thread to finish transition in matching manner.
	float StopAnimationOfPropertyUntilFrameUpdateAndGetStopTime( uint64 ulPanelContextID, uint32 hStyleSymbol );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

	// Flags to indicate what needs to repaint when a property is animating. The decision behind using Repaint or
	// CompositionOnly should match that from CStyleProperty::BAffectsCompositionOnly
	enum EAnimatingFlags
	{
		k_EAnimatingFlag_NotAnimating = 0x00,
		k_EAnimatingFlag_Repaint = 1 << 0,
		k_EAnimatingFlag_CompositionOnly = 1 << 1,
	};

private:

	// 
	// Helpers for pushing AAndTContext data
	//

	void PushDataAndStylesIntoContext( CAnimationAndTransformContext  *pContext, const PushAAndTContextRenderCommand_t &renderCommand, EAnimatingFlags *pAnimatingFlags );

	// Push a 3D transform operation into the current context
	EAnimatingFlags Push3DTransformMatrix( CAnimationAndTransformContext *pContext, const TransformMatrixWithTransition_t &command );

	// Push a 3D transform perspective operation into the current context
	EAnimatingFlags Push3DTransformPerspective( CAnimationAndTransformContext *pContext, const TransformPerspectiveWithTransition_t &command );

	// Push a 3D transform perspective origin operation into the current context
	EAnimatingFlags Push3DTransformPerspectiveOrigin( CAnimationAndTransformContext *pContext, const TransformPerspectiveOriginWithTransition_t &command );

	// Push a 3D transform perspective origin operation into the current context
	EAnimatingFlags Push3DTransformOrigin( CAnimationAndTransformContext *pContext, const TransformOriginWithTransition_t &command );

	// Push an opacity into the current context
	EAnimatingFlags PushOpacity( CAnimationAndTransformContext *pContext, const OpacityWithTransition_t &command );

	// Push an opacity mask texture into the current context
	EAnimatingFlags PushOpacityMask( CAnimationAndTransformContext *pContext, const OpacityMaskWithTransition_t &command );

	// Push a hue shift into the current context
	EAnimatingFlags PushHueShift( CAnimationAndTransformContext *pContext, const HueShiftWithTransition_t &command );

	// Push a saturation into the current context
	EAnimatingFlags PushSaturation( CAnimationAndTransformContext *pContext, const SaturationWithTransition_t &command );

	// Push a brightness into the current context
	EAnimatingFlags PushBrightness( CAnimationAndTransformContext *pContext, const BrightnessWithTransition_t &command );

	// Push a contrast into the current context
	EAnimatingFlags PushContrast( CAnimationAndTransformContext *pContext, const ContrastWithTransition_t &command );

	// Push a blur into the current context
	EAnimatingFlags PushGaussianBlur( CAnimationAndTransformContext *pContext, const GaussianBlurWithTransition_t &command );

	// Push a 2D scaling operation into the current context
	EAnimatingFlags Push2DScale( CAnimationAndTransformContext *pContext, const Scale2DWithTransition_t &command );

	// Push a 2D rotation operation into the current context
	EAnimatingFlags Push2DRotate( CAnimationAndTransformContext *pContext, const Rotate2DWithTransition_t &renderCommand );

	// Push a composition color into the current context
	EAnimatingFlags PushWashColor( CAnimationAndTransformContext *pContext, const WashColorWithTransition_t &renderCommand );

	// Push panel position operation into the current context
	EAnimatingFlags PushPanelPosition( CAnimationAndTransformContext *pContext, const PanelPositionWithTransition_t &renderCommand );

	// Push border radius message into current context
	EAnimatingFlags PushBorderRadius( CAnimationAndTransformContext *pContext, const BorderRadiusWithTransition_t &renderCommand );

	// Push border message into current context
	EAnimatingFlags PushBorder( CAnimationAndTransformContext *pContext, const BorderWithTransition_t &renderCommand );

	// Push box shadow message into current context
	EAnimatingFlags PushBoxShadow( CAnimationAndTransformContext *pContext, const BoxShadowWithTransition_t &renderCommand );

	// Push text shadow message into current context
	EAnimatingFlags PushTextShadow( CAnimationAndTransformContext *pContext, const TextShadowWithTransition_t &renderCommand );

	// Push image shadow message into current context
	EAnimatingFlags PushImageShadow( CAnimationAndTransformContext *pContext, const ImageShadowWithTransition_t &renderCommand );

	// Push clip message into current context
	EAnimatingFlags PushClip( CAnimationAndTransformContext *pContext, const ClipWithTransition_t &renderCommand );
	
	// Helpers for interpolating
	void InterpolateFillBrush( const FillBrushCollectionWithTransition_t &inputCollection, FillBrushCollection_t *pOutputCollection, CRenderCommandList &outputCommandList );
	void InterpolateTextFormat( const TextFormat_t &input, RenderTextFormat_t &output, CRenderCommandList &outputCommandList );

	// Free compositing layer
	void FreeAndForceRepaintOfCompositingLayer( uint64 ulLayerID, bool bForceRepaint );

	// Helper for updating particle systems, this takes a fill brush collection as input, and finds any particle systems present
	// in it, then it creates/updates the system locally and updates the message to contain the data for each particle for rendering 
	// on the render thread.
	void UpdateParticleSystems( FillBrushCollection_t &fillBrushCollection, CRenderCommandList &outputCommandList );
	void UpdateParticleSystem( ParticleSystem_t &particleSystem, CRenderCommandList &outputCommandList );

	float GetTimingTransitionProgress( uint32 hStyleProperty, const TransitionData_t *pTransitionData );

	float GetFrameTimePropertyStoppedAt( uint32 hStyleSymbol );

	template <class T> T lerp( uint32 hStyleSymbol, const TransitionData_t &trans, T flValue, T flTarget, EAnimatingFlags &eAnimating, EAnimatingFlags eFlagsIfAnimating );

	Quaternion slerp( uint32 hStyleSymbol, const TransitionData_t &trans, const Quaternion &start, const Quaternion &end );

	// Helper for getting offsets to apply to drawing operations as they occur based on context stack
	void GetCurrentDrawingOffsets( float &x, float &y, float &z, bool bSkipCurrentPanel = false );

	template< class T >
	struct RenderCmdFrameData_t
	{
		uint32 hStyleProperty;
		const T *base;
		const T *transition;
		const TransitionData_t *pTransitionData;

		// Use pTransitionData instead of this member. Here as we need some place to allocate animation transition data.
		TransitionData_t animationTransitionData;
	};

	template< class T > void GetRenderCmdFrameData( RenderCmdFrameData_t< T > *pData, const PropertyWithTransition_t< T > &renderCommand );

	CUtlLinkedList< CAnimationAndTransformContext * > m_AnimationAndTransformStack;

	// Render tree, which we build up as we animate, then serialize to render thread when done with the
	// frame.  This is needed so we can sort before rendering.

	struct RenderOperation_t
	{
		RenderOperation_t()
		{
			m_pRenderCommand = nullptr;
			m_flAvgZPos = 0.0f;
			m_flZIndex = 0.0f;
			m_flWidth = 0.0f;
			m_flHeight = 0.0f;
			m_pParent = NULL;
			m_bHitTestPassed = false;
			m_bWantsHitTest = false;
			m_ulPanelPtrValue = k_ulInvalidPanelHandle64;
			m_bDontDrawChildren = false;
			m_bDontDrawSelf = false;
			m_bIsLayerPop = false;
			m_bIsLayerPush = false;
			m_HitTestCoords.x = 0.0f;
			m_HitTestCoords.y = 0.0f;
			m_bForcedParentDrawAlready = false;
			m_bIsPanelContext = false;
#ifdef PANORAMA_ANIMATION_ENGINE_OCCLUSION
			m_CoveredAxisAlignedRect[0].x = m_CoveredAxisAlignedRect[0].y = 0.0f;
			m_CoveredAxisAlignedRect[1].x = m_CoveredAxisAlignedRect[1].y = 0.0f;
			m_flOccludedLeft = 0.0f;
			m_flOccludedTop = 0.0f;
			m_flOccludedRight = 0.0f;
			m_flOccludedBottom = 0.0f;
#endif
			m_flXOffsetInParentLayer = 0.0f;
			m_flYOffsetInParentLayer = 0.0f;
			m_bFreedChildLayers = false;
			m_pScreenspaceQuad = NULL;
			m_bSortChildOperations = false;
		}

		~RenderOperation_t()
		{
			if ( m_pScreenspaceQuad )
			{
				delete[] m_pScreenspaceQuad;
				m_pScreenspaceQuad = NULL;
			}
		}

		RenderCommand_t *m_pRenderCommand;
		float m_flAvgZPos;
		float m_flZIndex;
		float m_flXOffsetInParentLayer;
		float m_flYOffsetInParentLayer;
#ifdef PANORAMA_ANIMATION_ENGINE_OCCLUSION
		float m_flOccludedLeft;
		float m_flOccludedTop;
		float m_flOccludedRight;
		float m_flOccludedBottom;
		Vector2D m_CoveredAxisAlignedRect[2];
#endif
		float m_flWidth;
		float m_flHeight;
		bool m_bHitTestPassed : 1;
		bool m_bWantsHitTest : 1;
		bool m_bDontDrawChildren : 1;
		bool m_bDontDrawSelf : 1;
		bool m_bIsLayerPop : 1;
		bool m_bIsLayerPush : 1;
		bool m_bUntransformed : 1; // true for non layer push panel contexts, and for layers that are untransformed/have no z
		bool m_bForcedParentDrawAlready : 1;
		bool m_bIsPanelContext : 1;
		bool m_bFreedChildLayers : 1;
		bool m_bSortChildOperations : 1;
		Vector2D m_HitTestCoords;
		uint64 m_ulPanelPtrValue;
		RenderOperation_t *m_pParent;
		Vector2D *m_pScreenspaceQuad;

		CUtlVector< RenderOperation_t *> * m_pVecChildOperations;

#ifdef DBGFLAG_VALIDATE
		virtual void Validate( CValidator &validator, const tchar *pchName )
		{
			VALIDATE_SCOPE();
			ValidatePtrIfNeeded( m_pVecChildOperations );
			FOR_EACH_VEC( *m_pVecChildOperations, i )
			{
				ValidatePtr( (*m_pVecChildOperations)[i] );
			}
		}
#endif
	};

	CUtlVector< CUIAnimationEngine::RenderOperation_t *> * GetFreeRenderOpsVector() ;
	void FreeRenderOpsVector( CUtlVector< CUIAnimationEngine::RenderOperation_t *> * pVec );

	// Sort function
	typedef RenderOperation_t * RenderOpPtr;
	static bool RenderOpSort( const RenderOpPtr &a, const RenderOpPtr &b );

	// Helper to mark a render operation and its parents as animating on this frame
	void MarkRenderOpAnimating( RenderOperation_t *pRenderOp, bool bCompositionOnly );

	// Helper for hit testing panels
	bool BHitTestCurrentContext( Vector4D *pCorners, bool bForceMouseCoordinateCalculation, float &flMouseRelativeX, float &flMouseRelativeY, Vector2D *pScreenspaceQuadOut );

	// Helper for calculating whether panels are fully clipped post animation/transforms/everything
	bool BIsCurrentContextFullyClipped( Vector4D *pCorners );

	// Helper for computing quad for current context
	void TransformQuadForContext( Vector4D *pCorners, int iContextIndex );

	// Clean up old entries in the stopped transitions map
	void CleanupStoppedTransitions();

	template < typename T > T *AllocPerFrameObject()
	{
		T *pObject = m_frameScratchMemory.AllocType< T >( 1 );
		new ( pObject ) T();
		return pObject;
	}

	template < typename T > void FreePerFrameObject( T *pObject )
	{
		// Don't actually remove the object from the scratch memory - just call its destructor.
		// The entire scratch memory will be cleared at the end of the frame anyways.
		pObject->~T();
	}

	CUtlScratchMemoryPoolFixedGrowable< 128 * 1024 > m_frameScratchMemory;

	// BUG? ... changed to 32-bit index, otherwise 16-bit overflows on movie play
	CUtlVector< CUtlVector< RenderOperation_t *> * > m_vecRenderOpsVectorPtrs;

	RenderOperation_t *m_pCurrentOperationContext;
	CUtlVector< RenderOperation_t * > m_vecRenderOperations;

	void SortAndSerializeRenderOperationsRecursive( RenderOperation_t *pRenderOp, bool bSupressChildrenDrawing, CRenderCommandList &outputCommandList );

	void FreeChildLayersRecursive( RenderOperation_t *pChild );

	// Time the frame we last finished with was generated
	double m_flFinishedFrameGenerationTime;

	// Time the current frames paint data was generated
	double m_flCurrentFrameGenerationTime;

	// Time we started processing the current frame for animation
	double m_flCurrentFrameAnimationTime;

	// FPS average data
	CFastTimer m_FrameTimer;
	int m_nLastFrameMillisecondsIndex;
	float m_rgflMillisecondsFrame[PANORAMA_FRAMES_FOR_FPS_AVERAGES];

	// Map of active particle systems
	CUtlMap< AnimationParticleSystemKey_t, CAnimationParticleSystem *, int, CDefLess< AnimationParticleSystemKey_t > > m_mapParticleSystems;

	// List of context ids for layers we pushed (serialized) this frame
	CUtlVector<uint64> m_vecLayersPushedThisFrame;

	// Mouse position for the frame
	float m_flMouseX;
	float m_flMouseY;

	// Last panel hit test
	uint64 m_ulPanelHitTestPtrValue;
	Vector2D m_PanelHitTestCoords;

	// Surface size
	uint32 m_unSurfaceWidth;
	uint32 m_unSurfaceHeight;

	// vector of panels that always want mouse move data on and its results
	CUtlVector<uint64> m_vecTrackingMousePanels;
	CCopyableUtlVector<MouseTrackingResults_t> m_vecTrackingMouseResults;

	// Queue of layers to push free operations for at end of frame
	CUtlVector<uint64> m_vecLayersToFree;

	// Use the render engine with caution, mostly not save for use on animation thread
	CUIRenderEngine *m_pRenderEngine;

	uint32 m_unSkipUntilContextPopCounter;

	float m_flUIScaleFactor;

	int m_nFramesAnimated; // number of frames we have drawn since construction
	double m_flAnimationFrameTime; // accumulated number of msec taken drawing

	uint32 m_unRenderCountThisFrame;

	ScreenSpacePanelQuad_t *m_pScreenSpaceQuadOutput;
	ScreenSpacePanelQuad_t *m_pLastScreenSpaceQuadAdded;

	struct StoppedTransitionProperty_t
	{
		float flLayoutFrameTime;
		uint32 unStyleSymbol;
		float flTransitionStopTime;
	};

	CThreadMutex m_MutexStoppedTransitionData;
	CUtlMap< uint64, CUtlVector< StoppedTransitionProperty_t > *, int, CDefLess< uint64 > > m_mapStoppedTransitions;
	CUtlVector< StoppedTransitionProperty_t > *m_pVecStoppedTransitionsForCurrentPanel;

	CUtlMap< uint64, bool, int, CDefLess< uint64 > > m_mapPanelsToCompositionLayerNeeded;
	CUtlRBTree< uint64, int, CDefLess< uint64 > > m_treePanelsThatNeededLayersThisFrame;
};


// Convenience overloaded operators
inline CUIAnimationEngine::EAnimatingFlags operator |( CUIAnimationEngine::EAnimatingFlags a, CUIAnimationEngine::EAnimatingFlags b )
{
	return ( CUIAnimationEngine::EAnimatingFlags )( ( uint32 )a | ( uint32 )b );
}
inline CUIAnimationEngine::EAnimatingFlags &operator |=( CUIAnimationEngine::EAnimatingFlags &a, CUIAnimationEngine::EAnimatingFlags b )
{
	a = a | b;
	return a;
}

template <class T> T CUIAnimationEngine::lerp( uint32 hStyleProperty, const TransitionData_t &trans, T flValue, T flTarget, EAnimatingFlags &eAnimating, EAnimatingFlags eFlagsIfAnimating )
{
	float flOut = flValue;
	if ( trans.timing_func != k_EAnimationNone )
	{
		float flTimeProgress;
		if ( trans.duration_seconds < 0.0000001f )
			flTimeProgress = 1.0f;
		else
			flTimeProgress = GetTimingTransitionProgress( hStyleProperty, &trans );

		if ( flTimeProgress < 1.0f )
		{
			eAnimating |= eFlagsIfAnimating;
		}

		flOut = ( float )flValue + ( ( float )flTarget - ( float )flValue ) * flTimeProgress;
	}

	return ( T )flOut;
}


} // namespace panorama

#endif // UIANIMATIONENGINE_H


