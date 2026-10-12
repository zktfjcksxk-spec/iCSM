//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#define DEFINE_VARS
#include "win32openglfuncs.h"

#define GET_FUNC( func_defn, var )  m##var = (func_defn)wglGetProcAddress( #var );

void InitGLFunctionPointers()
{
	static bool bInitialized = false;

	if ( bInitialized )
		return;

	bInitialized = true;

	GET_FUNC( PFNGLFRAMEBUFFERTEXTURE2DEXTPROC, glFramebufferTexture2DEXT );
	GET_FUNC( PFNGLACTIVETEXTUREPROC, glActiveTexture );
	GET_FUNC( PFNGLACTIVETEXTUREARBPROC, glActiveTextureARB );
	GET_FUNC( PFNGLCREATESHADERPROC, glCreateShader );
	GET_FUNC( PFNGLUSEPROGRAMPROC, glUseProgram );
	GET_FUNC( PFNGLMULTITEXCOORD2FPROC, glMultiTexCoord2f );
	GET_FUNC( PFNGLMULTITEXCOORD4FVPROC, glMultiTexCoord4fv );
	GET_FUNC( PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate );
	GET_FUNC( PFNGLUNIFORM1FPROC, glUniform1f );
	GET_FUNC( PFNGLUNIFORM2FPROC, glUniform2f );
	GET_FUNC( PFNGLUNIFORM4FPROC, glUniform4f );
	GET_FUNC( PFNGLUNIFORM1IPROC, glUniform1i );
	GET_FUNC( PFNGLUNIFORM4FVPROC, glUniform4fv );
	GET_FUNC( PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv );
	GET_FUNC( PFNGLBINDBUFFERPROC, glBindBuffer );
	GET_FUNC( PFNGLDELETEBUFFERSPROC, glDeleteBuffers );
	GET_FUNC( PFNGLGENBUFFERSPROC, glGenBuffers );
	GET_FUNC( PFNGLBUFFERDATAPROC, glBufferData );
	GET_FUNC( PFNGLMAPBUFFERPROC, glMapBuffer );
	GET_FUNC( PFNGLUNMAPBUFFERPROC, glUnmapBuffer );
	GET_FUNC( PFNGLBINDFRAMEBUFFEREXTPROC, glBindFramebufferEXT );
	GET_FUNC( PFNGLDELETEFRAMEBUFFERSEXTPROC, glDeleteFramebuffersEXT );
	GET_FUNC( PFNGLGENFRAMEBUFFERSEXTPROC, glGenFramebuffersEXT );
	GET_FUNC( PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC, glCheckFramebufferStatusEXT );

	GET_FUNC( PFNGLATTACHSHADERPROC, glAttachShader );
	GET_FUNC( PFNGLCOMPILESHADERPROC, glCompileShader );
	GET_FUNC( PFNGLCREATEPROGRAMPROC, glCreateProgram );
	GET_FUNC( PFNGLDELETEPROGRAMPROC, glDeleteProgram );
	GET_FUNC( PFNGLDELETESHADERPROC, glDeleteShader );
	GET_FUNC( PFNGLGETPROGRAMIVPROC, glGetProgramiv );
	GET_FUNC( PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog );
	GET_FUNC( PFNGLGETSHADERIVPROC, glGetShaderiv );
	GET_FUNC( PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog );
	GET_FUNC( PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation );
	GET_FUNC( PFNGLLINKPROGRAMPROC, glLinkProgram );
	GET_FUNC( PFNGLSHADERSOURCEPROC, glShaderSource );
}