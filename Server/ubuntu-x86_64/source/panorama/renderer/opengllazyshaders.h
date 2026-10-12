//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Manage lazily compiling & linking GLSL shaders.
//=============================================================================//

#ifndef OPENGLLAZYSHADERS_H
#define OPENGLLAZYSHADERS_H

#define GL_GLEXT_PROTOTYPES

#include "tier1/refcount.h"

#include <tier0/memdbgoff.h>
#include <SDL.h>
#ifndef SOURCE2_PANORAMA
#ifdef WIN32
#include "win32openglfuncs.h"
#endif
#include <SDL_opengl.h>
#endif
#include <tier0/memdbgon.h>

namespace panorama
{
	
//-----------------------------------------------------------------------------
// Purpose: GLSL shader wrapper that calls glCompile when you call GetShader(),
//          that is refcounted so that it will be disposed of when
//          CLazyShaderProgram no longer needs it.
//-----------------------------------------------------------------------------
class CAbstractLazyShader : public CRefCounted<>
{
	GLuint m_unShader;
	bool m_bHasCompiled;
	CUtlVector<CUtlBuffer> m_vecShaderBuffers;
	mutable char m_pchHashHex[k_cubSHA1Hash * 2 + 1];
	mutable bool m_bHashValid;
	CUtlString m_strShaderName;
	
public:
	CAbstractLazyShader();
	virtual ~CAbstractLazyShader();
	
	void AddSource( const char *pchSource );
	bool AddSourceFromFile( const char *pchFile );
	const char *GetHash() const;
	int GetShader();
	void SetName( const char *pchName ) { m_strShaderName.Set( pchName ); }
	const char *GetName() const { return m_strShaderName.String(); }
	
protected:
	virtual GLuint CreateShader() = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Concrete implementation of CLazyShader for GL_VERTEX_SHADER.
//-----------------------------------------------------------------------------
class CVertexShader : public CAbstractLazyShader
{
protected: GLuint CreateShader() OVERRIDE;
};
	
//-----------------------------------------------------------------------------
// Purpose: Concrete implementation of CLazyShader for GL_FRAGMENT_SHADER.
//-----------------------------------------------------------------------------
class CFragmentShader : public CAbstractLazyShader
{
protected: GLuint CreateShader() OVERRIDE;
};

//-----------------------------------------------------------------------------
// Purpose: GLSL program wrapper that supports binary shader caching and
//          glLinkProgram when you call GetProgram().
//-----------------------------------------------------------------------------
class CLazyShaderProgram
{
public:
     CLazyShaderProgram() :m_bHasLinked(false), m_pVertShader(NULL), m_pFragShader(NULL), m_bLastMatrixWasIdentity( false ) { Clear(); }
	~CLazyShaderProgram() { Clear(); }
	
	static bool BShouldPreloadShaders();
	static bool BTryingBinaryShaders();

	bool Attach( CVertexShader* pVertShader, CFragmentShader* pFragShader );
	bool Preload();
	bool BLinked() { return m_bHasLinked; }
	GLuint GetProgram();
	void Clear();

	// handles into shader variables
	GLint m_matTransformLoc;
	GLint m_viewportWidthLoc;
	GLint m_viewportHeightLoc;
	GLint m_textureLoc;
	GLint m_texture1Loc;
	GLint m_texture2Loc;
	GLint m_texture3Loc;
	GLint m_OpacityMaskOneBaseLoc;
	GLint m_OpacityMaskTwoBaseLoc;
	GLint m_OpacityMaskOpacityLoc;
	GLint m_SaturationLoc;
	GLint m_BrightnessLoc;
	GLint m_ContrastLoc;
	GLint m_HueShiftLoc;
	GLint m_blurStdDevLoc;
	GLint m_blurDirection;
	GLint m_outercornerradii0;
	GLint m_outercornerradii1;
	GLint m_innercornerradii0;
	GLint m_innercornerradii1;
	GLint m_bordercolor;
	GLint m_gradientradialoffset;
	GLint m_particleSharpness;
	GLint m_VRAttrib_P0;
	GLint m_VRAttrib_coef;
	GLint m_VRAttrib_invProj;
	GLint m_VRAttrib_rot;
	GLint m_VRAttrib_uvOffset;
	GLint m_VRAttrib_kernel;

	bool m_bLastMatrixWasIdentity;

	void SetName( const char *pchName ) { m_strShaderName.Set( pchName ); }
	const char *GetName() const { return m_strShaderName.String(); }
	
private:
	bool LoadBinary( const char *pchBaseDir );
	bool SaveBinary( const char *pchBaseDir );
	void ReleaseShaders();
	
	CUtlString GetBaseCachePath( const char *pchBaseDir );
	CUtlString GetDataCachePath( const char *pchBaseDir );
	CUtlString GetFormatCachePath( const char *pchBaseDir );

	CUtlString m_strShaderName;
	GLuint m_unProgram;
	CVertexShader* m_pVertShader;
	CFragmentShader* m_pFragShader;
	bool m_bHasLinked;
};

}

#endif // OPENGLLAZYSHADERS_H
