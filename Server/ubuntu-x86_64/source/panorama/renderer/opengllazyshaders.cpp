//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose:
//=============================================================================//

#include "stdafx.h"

#include "opengllazyshaders.h"
#include "sdlopenglsurface.h"

static void CHECK_GL_ERRORS()
{
#ifdef _DEBUG
	int e = glGetError();
	if ( e )
	{
		Msg( "%s:%i: glGetError() = %i\n", __FILE__, __LINE__, e );
	}
#endif
}

static CCommandLineParam s_NvidiaBinaryShaders( "-nvidiabinaryshaders", "Attempt to use binary shaders on NVIDIA drivers too." );
static CCommandLineParam s_NoBinaryShaders( "-nobinaryshaders", "Do not load & save cached binary shaders." );
static CCommandLineParam s_NoLazyShaders( "-nolazyshaders", "Compile and link all possible shaders at startup, rather than using glfancyquadshaders.cfg." );

using namespace panorama;

CDynamicFunctionOpenGL< PFNGLGETPROGRAMBINARYPROC > g_glGetProgramBinary( "glGetProgramBinary" );
CDynamicFunctionOpenGL< PFNGLPROGRAMPARAMETERIPROC > g_glProgramParameteri( "glProgramParameteri" );
CDynamicFunctionOpenGL< PFNGLPROGRAMBINARYPROC > g_glProgramBinary( "glProgramBinary" );

CAbstractLazyShader::CAbstractLazyShader() :m_unShader( 0 ), m_bHasCompiled( false ), m_bHashValid( false )
{
	SetName( "<unnamed>" );
}

CAbstractLazyShader::~CAbstractLazyShader()
{
	if ( m_unShader > 0 )
		glDeleteShader( m_unShader );
}

//-----------------------------------------------------------------------------
// Purpose: Add the given string to the list of sources to be compiled.
//-----------------------------------------------------------------------------
void CAbstractLazyShader::AddSource( const char *pchSource )
{
	if ( m_bHasCompiled )
	{
		AssertMsg1( false, "Attempted to add source to compiled shader '%s'", GetName() );
		return;
	}

	m_bHashValid = false;
	m_vecShaderBuffers[ m_vecShaderBuffers.AddToTail() ].Put( pchSource, V_strlen( pchSource ) );
}

//-----------------------------------------------------------------------------
// Purpose: Load the given file into the list of sources to be compiled.
//-----------------------------------------------------------------------------
bool CAbstractLazyShader::AddSourceFromFile( const char *pchFile )
{
	if ( m_bHasCompiled )
	{
		AssertMsg1( false, "Attempted to add source to compiled shader '%s'", GetName() );
		return false;
	}

	m_bHashValid = false;
	m_strShaderName.Set( pchFile );
	
	CUtlString strUTF8 = UIEngine()->GetLocalPathForNamedPath( "{shaders}" );
	strUTF8 += pchFile;
	
	int idxBuf = m_vecShaderBuffers.AddToTail();
	CUtlBuffer &bufFile = m_vecShaderBuffers[ idxBuf ];
	LoadFileIntoBuffer( strUTF8.String(), bufFile, false );
	
	if ( bufFile.TellPut() == 0 )
	{
		m_vecShaderBuffers.Remove( idxBuf );
		Msg( "Failed to read fragment file %s", strUTF8.String() );
		return false;
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Get the program object for this shader, compiling if needed.
//-----------------------------------------------------------------------------
int CAbstractLazyShader::GetShader()
{
	if ( m_bHasCompiled )
		return m_unShader;
	
	m_bHasCompiled = true;
	m_unShader = CreateShader();
	GLchar **buffers = new GLchar*[ m_vecShaderBuffers.Count() ];
	FOR_EACH_VEC( m_vecShaderBuffers, i )
	{
		int bufSize = m_vecShaderBuffers[i].TellPut() + 1;
		buffers[i] = new GLchar[ bufSize ];
		memcpy( buffers[i], m_vecShaderBuffers[i].Base(), bufSize - 1 );
		buffers[i][bufSize - 1] = 0;
	}
	
	glShaderSource( m_unShader, m_vecShaderBuffers.Count(), (const GLchar **)buffers, NULL );
	glCompileShader( m_unShader );
	
	FOR_EACH_VEC( m_vecShaderBuffers, i )
	{
		delete [] buffers[i];
	}
	delete [] buffers;
	
	GLint bIsCompiled;
	glGetShaderiv( m_unShader, GL_COMPILE_STATUS, &bIsCompiled );
	
	// forestw: we should print out the info log if it contains any warnings, not just if the compile failed
	int nLogLength = 0;
	glGetShaderiv( m_unShader, GL_INFO_LOG_LENGTH, &nLogLength );
	
	// The maxLength includes the NULL character
	char *pchShaderProgramInfoLog = ( char * )malloc( nLogLength );
	glGetShaderInfoLog( m_unShader, nLogLength, &nLogLength, pchShaderProgramInfoLog );
	if ( !bIsCompiled )
		Msg( "Failed to compile shader - shader info log for %s : %s\n", m_strShaderName.String(), pchShaderProgramInfoLog );
	else if ( (IsLinux() && V_stristr( pchShaderProgramInfoLog, "warning" )) || V_stristr( pchShaderProgramInfoLog, "error" ) )
		Msg( "Shader info log for %s : %s\n", m_strShaderName.String(), pchShaderProgramInfoLog );
	
	free( pchShaderProgramInfoLog );
	
	if ( bIsCompiled == false )
	{
		glDeleteShader( m_unShader );
		m_unShader = 0;
	}
	
	return m_unShader;
}

//-----------------------------------------------------------------------------
// Purpose: Get hash of the concatenation of all the sources in this shader.
//-----------------------------------------------------------------------------
const char *CAbstractLazyShader::GetHash() const
{
	if ( !m_bHashValid )
	{
		CSHA1_CLASS_NAME sha1;
		FOR_EACH_VEC( m_vecShaderBuffers, i )
		{
			const CUtlBuffer& buffer = m_vecShaderBuffers[i];
			sha1.Update( (const uint8*)buffer.PeekGet(), buffer.TellPut() );
		}
		sha1.Final();
		
		sha1.GetHashHex( m_pchHashHex, sizeof( m_pchHashHex ) );
	}
	
	return m_pchHashHex;
}

GLuint CVertexShader::CreateShader() { return glCreateShader( GL_VERTEX_SHADER ); }
GLuint CFragmentShader::CreateShader() { return glCreateShader( GL_FRAGMENT_SHADER ); }

//-----------------------------------------------------------------------------
// Purpose: Return true if the COpenGLSurface should honor glfancyquadshaders.cfg.
//-----------------------------------------------------------------------------
bool CLazyShaderProgram::BShouldPreloadShaders()
{
	return !CommandLine()->FindParm( s_NoLazyShaders.GetHParam() ) && !CommandLine()->FindParm( g_DumpUsedShaders.GetHParam() );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if we should attempt to save & load binary shaders.
//-----------------------------------------------------------------------------
bool CLazyShaderProgram::BTryingBinaryShaders()
{
	if ( CommandLine()->FindParm( s_NoBinaryShaders.GetHParam() ) )
		return false;

	if ( !g_glProgramParameteri || !g_glProgramBinary || !g_glGetProgramBinary )
		return false;

	// 2014-01-07, John McDonald <jmcdonald@nvidia.com> said:
	// > it's still recommended that you leverage the shader cache on us instead of program binaries.
	// > they should be as fast or faster in all cases.
	//
	// Because of this, we disable the binaries on NVIDIA hardware.
	const char *pszVendor = ( const char * )glGetString( GL_VENDOR );
	if ( V_stricmp( pszVendor, "NVIDIA Corporation" ) == 0 && !CommandLine()->FindParm( s_NvidiaBinaryShaders.GetHParam() ) )
		return false;

	return true;
}

void CLazyShaderProgram::Clear()
{
	if ( m_bHasLinked && m_unProgram )
	{
		CHECK_GL_ERRORS();
		glDeleteProgram( m_unProgram );
		CHECK_GL_ERRORS();
	}

	m_unProgram = 0;
	m_bHasLinked = false;
	m_matTransformLoc = 0;
	m_viewportWidthLoc = 0;
	m_viewportHeightLoc = 0;
	m_textureLoc = 0;
	m_texture1Loc = 0;
	m_texture2Loc = 0;
	m_texture3Loc = 0;
	m_OpacityMaskOneBaseLoc = 0;
	m_OpacityMaskTwoBaseLoc = 0;
	m_SaturationLoc = 0;
	m_BrightnessLoc = 0;
	m_ContrastLoc = 0;
	m_HueShiftLoc = 0;
	m_blurStdDevLoc = 0;
	m_blurDirection = 0;
	m_outercornerradii0 = 0;
	m_outercornerradii1 = 0;
	m_innercornerradii0 = 0;
	m_innercornerradii1 = 0;
	m_bordercolor = 0;
	m_gradientradialoffset = 0;
	m_VRAttrib_P0 = 0;
	m_VRAttrib_coef = 0;
	m_VRAttrib_invProj = 0;
	m_VRAttrib_rot = 0;
	m_VRAttrib_uvOffset = 0;
	m_VRAttrib_kernel = 0;
	ReleaseShaders();
}

//-----------------------------------------------------------------------------
// Purpose: Associate this program with the given shaders.
//-----------------------------------------------------------------------------
bool CLazyShaderProgram::Attach( CVertexShader* pVertShader, CFragmentShader* pFragShader )
{
	m_bHasLinked = false;
	m_pVertShader = InlineAddRef( pVertShader );
	m_pFragShader = InlineAddRef( pFragShader );
	
	SetName( CFmtStr( "%s/%s", m_pVertShader->GetName(), m_pFragShader->GetName() ).String() );
	
	if ( CommandLine()->FindParm( s_NoLazyShaders.GetHParam() ) )
		return Preload();
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Cause the program to be loaded from or binary cache, or if
//          that fails, link it.
//-----------------------------------------------------------------------------
bool CLazyShaderProgram::Preload()
{
	if ( m_bHasLinked )
		return ( m_unProgram != 0 );
	
	CUtlString strShaderCachePath = UIEngine()->GetLocalPathForNamedPath( "{config}" );
	strShaderCachePath += "shadercache";

	m_bHasLinked = true;
	
	Assert( m_pVertShader && m_pFragShader );
	if ( !m_pVertShader || !m_pFragShader )
		return false;
	
	m_unProgram = glCreateProgram();
	if ( !LoadBinary( strShaderCachePath.String() ) )
	{
		CHECK_GL_ERRORS();
		glAttachShader( m_unProgram, m_pVertShader->GetShader() );
		CHECK_GL_ERRORS();
		glAttachShader( m_unProgram, m_pFragShader->GetShader() );
		CHECK_GL_ERRORS();
		
		glLinkProgram( m_unProgram );
		CHECK_GL_ERRORS();
		
		SaveBinary( strShaderCachePath.String() );
	}
	
	GLint bIsLinked;
	glGetProgramiv( m_unProgram, GL_LINK_STATUS, &bIsLinked );
	CHECK_GL_ERRORS();
	
	// forestw: we should print out the info log if it contains any warnings, not just if the link failed
	int nLogLength = 0;
	glGetProgramiv( m_unProgram, GL_INFO_LOG_LENGTH, &nLogLength );
	CHECK_GL_ERRORS();
	
	// The maxLength includes the NULL character
	char *pchProgramInfoLog = ( char * )malloc( nLogLength );
	
	
	// Notice that glGetProgramInfoLog, not glGetShaderInfoLog.
	glGetProgramInfoLog( m_unProgram, nLogLength, &nLogLength, pchProgramInfoLog );
	CHECK_GL_ERRORS();
	
	if ( !bIsLinked )
		Msg( "Failed to link shader (%s & %s) - program info log : %s\n", m_pVertShader->GetName(), m_pFragShader->GetName(), pchProgramInfoLog );
	else if ( ( IsLinux() && V_stristr( pchProgramInfoLog, "warning" ) ) || V_stristr( pchProgramInfoLog, "error" ) )
		Msg( "Program info log : %s\n", pchProgramInfoLog );
	
	free( pchProgramInfoLog );
	
	if ( !bIsLinked )
		Clear();
	else
	{
		CHECK_GL_ERRORS();
		m_matTransformLoc = glGetUniformLocation ( m_unProgram, "g_MatTransform" );
		m_viewportWidthLoc = glGetUniformLocation ( m_unProgram, "g_viewportWidth" );
		m_viewportHeightLoc = glGetUniformLocation ( m_unProgram, "g_viewportHeight" );
		m_textureLoc = glGetUniformLocation ( m_unProgram, "Texture0" );
		m_texture1Loc = glGetUniformLocation ( m_unProgram, "Texture1" );
		m_texture2Loc = glGetUniformLocation ( m_unProgram, "Texture2" );
		m_texture3Loc = glGetUniformLocation ( m_unProgram, "Texture3" );
		m_OpacityMaskOneBaseLoc = glGetUniformLocation ( m_unProgram, "g_OpacityMaskOneBase" );
		m_OpacityMaskTwoBaseLoc = glGetUniformLocation ( m_unProgram, "g_OpacityMaskTwoBase" );
		m_OpacityMaskOpacityLoc = glGetUniformLocation ( m_unProgram, "g_OpacityMaskOpacity" );
		m_SaturationLoc = glGetUniformLocation ( m_unProgram, "g_Saturation" );
		m_BrightnessLoc = glGetUniformLocation( m_unProgram, "g_Brightness" );
		m_ContrastLoc = glGetUniformLocation( m_unProgram, "g_Contrast" );
		m_HueShiftLoc = glGetUniformLocation( m_unProgram, "g_HueShift" );
		m_blurStdDevLoc = glGetUniformLocation ( m_unProgram, "blurSigma" );
		m_blurDirection = glGetUniformLocation ( m_unProgram, "blurMultiplyVec" );
		m_outercornerradii0 = glGetUniformLocation ( m_unProgram, "outercornerradii0" );
		m_outercornerradii1 = glGetUniformLocation ( m_unProgram, "outercornerradii1" );
		m_innercornerradii0 = glGetUniformLocation ( m_unProgram, "innercornerradii0" );
		m_innercornerradii1 = glGetUniformLocation ( m_unProgram, "innercornerradii1" );
		m_bordercolor = glGetUniformLocation ( m_unProgram, "bordercolor" );
		m_gradientradialoffset = glGetUniformLocation ( m_unProgram, "gradientradialoffset" );
		m_particleSharpness = glGetUniformLocation ( m_unProgram, "particleSharpness" );
		m_VRAttrib_P0 = glGetUniformLocation( m_unProgram, "P0" );
		m_VRAttrib_coef = glGetUniformLocation( m_unProgram, "coef" );
		m_VRAttrib_invProj = glGetUniformLocation( m_unProgram, "invProj" );
		m_VRAttrib_rot = glGetUniformLocation( m_unProgram, "rot" );
		m_VRAttrib_uvOffset = glGetUniformLocation( m_unProgram, "uvOffset" );
		m_VRAttrib_kernel = glGetUniformLocation( m_unProgram, "kernel" );

		CHECK_GL_ERRORS();
	}

	ReleaseShaders();

	return m_unProgram ? true :  false;
}

//-----------------------------------------------------------------------------
// Purpose: Get the program object, assert & linking if it hasn't been linked already.
//-----------------------------------------------------------------------------
GLuint CLazyShaderProgram::GetProgram()
{
	if ( !m_bHasLinked )
	{
		AssertMsg1( CommandLine()->FindParm( g_DumpUsedShaders.GetHParam() ), "Using shader that wasn't preloaded: %s", GetName() );
		Preload();
	}
	
	return m_unProgram;
}

//-----------------------------------------------------------------------------
// Purpose: Attempt to load the program from the binary shader cache.
//-----------------------------------------------------------------------------
bool CLazyShaderProgram::LoadBinary( const char *pchBaseDir )
{
	if ( !BTryingBinaryShaders() )
		return false;
	
	g_glProgramParameteri( m_unProgram, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE );
	
	CUtlBuffer bufFormat;
	LoadFileIntoBuffer( GetFormatCachePath( pchBaseDir ).String(), bufFormat, false );
	if ( bufFormat.TellPut() == 0 )
	{
		return false;
	}
	
	CUtlBuffer bufData;
	LoadFileIntoBuffer( GetDataCachePath( pchBaseDir ).String(), bufData, false );
	if ( bufData.TellPut() == 0 )
	{
		Msg( "CLazyShaderProgram::LoadBinary: Unable to load shader cache data from %s\n", GetDataCachePath( pchBaseDir ).String() );
		return false;
	}
	
	CHECK_GL_ERRORS();
	g_glProgramBinary( m_unProgram, bufFormat.GetInt(), bufData.PeekGet(), bufData.TellPut() );
	
	GLint bIsLinked;
	glGetProgramiv( m_unProgram, GL_LINK_STATUS, &bIsLinked );
	CHECK_GL_ERRORS();
	
	if (!bIsLinked)
	{
		Msg( "CLazyShaderProgram::LoadBinary: Unable to load binary shader (loaded from %s)\n", GetDataCachePath( pchBaseDir ).String() );
	}
	
	return bIsLinked ? true :  false;
}

//-----------------------------------------------------------------------------
// Purpose: Attempt to save the program to the binary shader cache.
//-----------------------------------------------------------------------------
bool CLazyShaderProgram::SaveBinary( const char *pchBaseDir )
{
	if ( !BTryingBinaryShaders() )
		return false;
	
	glUseProgram( m_unProgram );
	GLsizei binaryLength;
	glGetProgramiv( m_unProgram, GL_PROGRAM_BINARY_LENGTH, &binaryLength );
	
	if ( binaryLength > 0 )
	{
		CUtlBuffer buffer( 0, binaryLength );
		CUtlBuffer formatBuffer( 0, sizeof( GLenum ) );
		g_glGetProgramBinary( m_unProgram, binaryLength, &binaryLength, (GLenum*)formatBuffer.Base(), buffer.Base() );
		
		formatBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, sizeof( GLenum ) );
		buffer.SeekPut( CUtlBuffer::SEEK_HEAD, binaryLength );
		
		SaveBufferToFile( formatBuffer, GetFormatCachePath( pchBaseDir ).String() );
		SaveBufferToFile( buffer, GetDataCachePath( pchBaseDir ).String() );
		
		return true;
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Remove our reference to the vert & frag shaders
//-----------------------------------------------------------------------------
void CLazyShaderProgram::ReleaseShaders()
{
	if ( m_pVertShader )
		m_pVertShader->Release();
	m_pVertShader = NULL;
	if ( m_pFragShader )
		m_pFragShader->Release();
	m_pFragShader = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Get the directory in which we should store this programs
//          binary shader format & data.
//-----------------------------------------------------------------------------
CUtlString CLazyShaderProgram::GetBaseCachePath( const char *pchBaseDir )
{
	CFmtStr basePath( "%s/cache/%s/%s", pchBaseDir, m_pVertShader->GetHash(), m_pFragShader->GetHash() );
	return basePath.String();
}

//-----------------------------------------------------------------------------
// Purpose: Get the path for this program's binary shader format.
//-----------------------------------------------------------------------------
CUtlString CLazyShaderProgram::GetFormatCachePath( const char *pchBaseDir )
{
	CUtlString strDataPath = GetBaseCachePath( pchBaseDir );
	strDataPath.Append( "/format" );
	return strDataPath;
}

//-----------------------------------------------------------------------------
// Purpose: Get the path for this program's binary shader data.
//-----------------------------------------------------------------------------
CUtlString CLazyShaderProgram::GetDataCachePath( const char *pchBaseDir )
{
	CUtlString strDataPath = GetBaseCachePath( pchBaseDir );
	strDataPath.Append( "/data" );
	return strDataPath;
}
