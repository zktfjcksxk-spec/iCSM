//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: Decompose a transformation matrix into components (usually to interpolate two matrixes)
//-----------------------------------------------------------------------------
DecomposedMatrix_t DecomposeTransformMatrix( VMatrix matrix )
{
	DecomposedMatrix_t result;

	// Normalize the matrix.
	float divisor = matrix[3][3];
	if ( divisor == 0.0f )
		return result; 

	for ( int i = 0; i < 4; i++ )
	{
		for ( int j = 0; j < 4; j++ )
		{
			matrix[i][j] = matrix[i][j] / divisor;
		}
	}

	// Clear perspective, we don't need to get it out since we know it separately
	matrix.SetElement( 0, 3, 0.0f );
	matrix.SetElement( 1, 3, 0.0f );
	matrix.SetElement( 2, 3, 0.0f );
	matrix.SetElement( 3, 3, 1.0f );

	// Get translation x,y,z, then clear
	result.m_flTranslationXYZ[0] = matrix[0][3];
	result.m_flTranslationXYZ[1] = matrix[1][3];
	result.m_flTranslationXYZ[2] = matrix[2][3];
	matrix.SetElement( 3, 0, 0.0f );
	matrix.SetElement( 3, 1, 0.0f );
	matrix.SetElement( 3, 2, 0.0f );

	// Get scale out, then clear
	Vector v0( matrix[0][0], matrix[0][1], matrix[0][2] );
	Vector v1( matrix[1][0], matrix[1][1], matrix[1][2] );
	Vector v2( matrix[2][0], matrix[2][1], matrix[2][2] );

	// Get X Scale and normalize row
	result.m_flScaleXYZ[0] = VectorLength( v0 );
	VectorNormalize( v0 );

	// Compute XY shear factor and make 2nd row orthoganal to 1st
	result.m_flSkewXY = DotProduct( v0, v1 );
	VectorMA( v1, -result.m_flSkewXY, v0, v1 );

	// Now compute Y Scale and normalize 2nd row
	result.m_flScaleXYZ[1] = VectorLength( v1 );
	VectorNormalize( v1 );
	result.m_flSkewXY /= result.m_flScaleXYZ[1];

	// Compute XZ and YZ shears, orthogonalize 3rd row
	result.m_flSkewXZ = DotProduct( v0, v2 );
	VectorMA( v2, -result.m_flSkewXZ, v0, v2 );
	result.m_flSkewYZ = DotProduct( v1, v2 );
	VectorMA( v2, -result.m_flSkewYZ, v1, v2 );

	// Get Z Scale and normalize 3rd row
	result.m_flScaleXYZ[2] = VectorLength( v2 );
	VectorNormalize( v2 );
	result.m_flSkewXZ /= result.m_flScaleXYZ[2];
	result.m_flSkewYZ /= result.m_flScaleXYZ[2];

	// Matrix represented by v1,v2,v3 should be orthonormal now, check for coordinate system flip.
	Vector v = CrossProduct( v1, v2 );
	if ( DotProduct( v0, v ) < 0 )
	{
		for( int i=0; i<3; ++i )
			result.m_flScaleXYZ[i] *= -1;

		VectorScale( v0, -1.0f, v0 );
		VectorScale( v1, -1.0f, v1 );
		VectorScale( v2, -1.0f, v2 );
	}

	// Get out the rotation quaternion
	float s, t, x, y, z, w;
	t = v0[0] + v1[1] + v2[2] + 1.0f;
	if ( t > 1e-4 )
	{
		s = 0.5 / sqrt( t );
		w = 0.25 / s;
		x = (v2[1] - v1[2]) * s;
		y = (v0[2] - v2[0]) * s;
		z = (v1[0] - v0[1]) * s;
	}
	else if ( v0[0] > v1[1] && v0[0] > v2[2] )
	{
		s = sqrt( 1.0 + v0[0] - v1[1] - v2[2]) * 2.0;
		x = 0.25 *s;
		y = (v0[1] + v1[0]) / s;
		z = (v0[2] + v2[0]) / s;
		w = (v2[1] - v1[2]) / s;
	} 
	else if ( v1[1] > v2[2] )
	{
		s = sqrt( 1.0 + v1[1] - v0[0] - v2[2] ) * 2.0;
		x = (v0[1] + v1[0]) / s;
		y = 0.25 * s;
		z = (v1[2] + v2[1]) / s;
		w = (v0[2] - v2[0]) / s;
	}
	else
	{
		s = sqrt( 1.0 +v2[2] - v0[0] - v1[1] ) * 2.0;
		x = (v0[2] + v2[0]) / s;
		y = (v1[2] + v2[1]) / s;
		z = 0.25 *s;
		w = (v1[0] - v0[1]) / s;
	}

	result.m_quatTransform.x = x;
	result.m_quatTransform.y = y;
	result.m_quatTransform.z = z;
	result.m_quatTransform.w = w;

	return result;
}


//-----------------------------------------------------------------------------
// Purpose: Turn decomposed transform data back into VMatrix
//-----------------------------------------------------------------------------
VMatrix RecomposeTransformMatrix( const DecomposedMatrix_t &decomposed )
{
	VMatrix result = VMatrix::GetIdentityMatrix();

	// Set translation into matrix
	result.SetTranslation( Vector( decomposed.m_flTranslationXYZ[0], decomposed.m_flTranslationXYZ[1], decomposed.m_flTranslationXYZ[2] ) );

	// Multiply in rotation
	matrix3x4_t mat;
	SetIdentityMatrix( mat );
	QuaternionMatrix( decomposed.m_quatTransform, mat );
	result = result * VMatrix( mat );

	// Multiply in skews if set
	if ( decomposed.m_flSkewYZ > 0.00001f || decomposed.m_flSkewYZ < -0.00001f )
	{
		matrix3x4_t matSkew;
		SetIdentityMatrix( matSkew );
		matSkew[2][1] = decomposed.m_flSkewYZ;
		result = result * VMatrix( matSkew );
	}

	if ( decomposed.m_flSkewXZ > 0.00001f || decomposed.m_flSkewXZ < -0.00001f )
	{
		matrix3x4_t matSkew;
		SetIdentityMatrix( matSkew );
		matSkew[2][0] = decomposed.m_flSkewXZ;
		result = result * VMatrix( matSkew );
	}

	if ( decomposed.m_flSkewXY > 0.00001f || decomposed.m_flSkewXY < -0.00001f )
	{
		matrix3x4_t matSkew;
		SetIdentityMatrix( matSkew );
		matSkew[1][0] = decomposed.m_flSkewYZ;
		result = result * VMatrix( matSkew );
	}

	// Finally, add scale
	if ( decomposed.m_flScaleXYZ[0] > 1.00001f || decomposed.m_flScaleXYZ[0] < 0.99999f
		|| decomposed.m_flScaleXYZ[1] > 1.00001f || decomposed.m_flScaleXYZ[1] < 0.99999f 
		|| decomposed.m_flScaleXYZ[2] > 1.00001f || decomposed.m_flScaleXYZ[2] < 0.99999f )
	{
		VMatrix scale;
		MatrixBuildScale( scale, decomposed.m_flScaleXYZ[0], decomposed.m_flScaleXYZ[1], decomposed.m_flScaleXYZ[2] );
		result = result * scale;
	}

	return result;
}


//-----------------------------------------------------------------------------
// Purpose: Returns interpolated matrix between from and to
//-----------------------------------------------------------------------------
VMatrix InterpolateTransformMatrix( VMatrix matCurrent, VMatrix matTarget, float flTimeProgress )
{
	DecomposedMatrix_t current = DecomposeTransformMatrix( matCurrent );
	DecomposedMatrix_t target = DecomposeTransformMatrix( matTarget );

	for( int i=0; i < 3; ++i )
	{		
		current.m_flTranslationXYZ[i] = Lerp( flTimeProgress, current.m_flTranslationXYZ[i], target.m_flTranslationXYZ[i] );
		current.m_flScaleXYZ[i] = Lerp( flTimeProgress, current.m_flScaleXYZ[i], target.m_flScaleXYZ[i] );
	}

	current.m_flSkewXY = Lerp( flTimeProgress, current.m_flSkewXY, target.m_flSkewXY );
	current.m_flSkewXZ = Lerp( flTimeProgress, current.m_flSkewXZ, target.m_flSkewXZ );
	current.m_flSkewYZ = Lerp( flTimeProgress, current.m_flSkewYZ, target.m_flSkewYZ );
	QuaternionSlerp( current.m_quatTransform, target.m_quatTransform, flTimeProgress, current.m_quatTransform );

	return RecomposeTransformMatrix( current );
}

} // namespace panorama