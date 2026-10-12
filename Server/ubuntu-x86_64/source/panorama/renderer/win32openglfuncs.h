//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include <Gl/gl.h>
#include <Gl/GLext.h>

extern void InitGLFunctionPointers();

#define GL_NUM_PROGRAM_BINARY_FORMATS 0x87FE
#define GL_PROGRAM_BINARY_RETRIEVABLE_HINT 0x8257
#define GL_PROGRAM_BINARY_LENGTH          0x8741

typedef void ( APIENTRYP PFNGLGETPROGRAMBINARYPROC ) ( GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary );
typedef void (APIENTRYP PFNGLPROGRAMPARAMETERIPROC) (GLuint program, GLenum pname, GLint value);
typedef void (APIENTRYP PFNGLPROGRAMBINARYPROC) (GLuint program, GLenum binaryFormat, const void *binary, GLsizei length);
typedef void (APIENTRYP PFNGLMULTITEXCOORD4FVPROC) (GLenum target, const GLfloat *v);
typedef void (APIENTRYP PFNGLMULTITEXCOORD2FPROC) (GLenum target, GLfloat s, GLfloat t);

#ifdef DEFINE_VARS
// in this mode define the variable below in the include
#define FUNC_TYPE 
#else
// otherwise just extern the defn to the one define we had
#define FUNC_TYPE extern
#endif

extern "C"
{
	FUNC_TYPE PFNGLFRAMEBUFFERTEXTURE2DEXTPROC mglFramebufferTexture2DEXT;
#define glFramebufferTexture2DEXT mglFramebufferTexture2DEXT
	FUNC_TYPE PFNGLACTIVETEXTUREPROC mglActiveTexture;
#define glActiveTexture mglActiveTexture
	FUNC_TYPE PFNGLACTIVETEXTUREARBPROC mglActiveTextureARB;
#define glActiveTextureARB mglActiveTextureARB
	FUNC_TYPE PFNGLCREATESHADERPROC mglCreateShader;
#define glCreateShader mglCreateShader
	FUNC_TYPE PFNGLUSEPROGRAMPROC mglUseProgram;
#define glUseProgram mglUseProgram
	FUNC_TYPE PFNGLMULTITEXCOORD4FVPROC mglMultiTexCoord4fv;
#define glMultiTexCoord4fv mglMultiTexCoord4fv
	FUNC_TYPE PFNGLMULTITEXCOORD2FPROC mglMultiTexCoord2f;
#define glMultiTexCoord2f mglMultiTexCoord2f
	FUNC_TYPE PFNGLBLENDFUNCSEPARATEPROC mglBlendFuncSeparate;
#define glBlendFuncSeparate mglBlendFuncSeparate
	FUNC_TYPE PFNGLUNIFORM1FPROC mglUniform1f;
#define glUniform1f mglUniform1f
	FUNC_TYPE PFNGLUNIFORM2FPROC mglUniform2f;
#define glUniform2f mglUniform2f
	FUNC_TYPE PFNGLUNIFORM4FPROC mglUniform4f;
#define glUniform4f mglUniform4f
	FUNC_TYPE PFNGLUNIFORM1IPROC mglUniform1i;
#define glUniform1i mglUniform1i
	FUNC_TYPE PFNGLUNIFORM4FVPROC mglUniform4fv;
#define glUniform4fv mglUniform4fv
	FUNC_TYPE PFNGLUNIFORMMATRIX4FVPROC mglUniformMatrix4fv;
#define glUniformMatrix4fv mglUniformMatrix4fv

	FUNC_TYPE PFNGLBINDBUFFERPROC mglBindBuffer;
#define glBindBuffer mglBindBuffer
	FUNC_TYPE PFNGLDELETEBUFFERSPROC mglDeleteBuffers;
#define glDeleteBuffers mglDeleteBuffers
	FUNC_TYPE PFNGLGENBUFFERSPROC mglGenBuffers;
#define glGenBuffers mglGenBuffers
	FUNC_TYPE PFNGLBUFFERDATAPROC mglBufferData;
#define glBufferData mglBufferData
	FUNC_TYPE PFNGLMAPBUFFERPROC mglMapBuffer;
#define glMapBuffer mglMapBuffer
	FUNC_TYPE PFNGLUNMAPBUFFERPROC mglUnmapBuffer;
#define glUnmapBuffer mglUnmapBuffer
	FUNC_TYPE PFNGLBINDFRAMEBUFFEREXTPROC mglBindFramebufferEXT;
#define glBindFramebufferEXT mglBindFramebufferEXT
	FUNC_TYPE PFNGLDELETEFRAMEBUFFERSEXTPROC mglDeleteFramebuffersEXT;
#define glDeleteFramebuffersEXT mglDeleteFramebuffersEXT
	FUNC_TYPE PFNGLGENFRAMEBUFFERSEXTPROC mglGenFramebuffersEXT;
#define glGenFramebuffersEXT mglGenFramebuffersEXT
	FUNC_TYPE PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC mglCheckFramebufferStatusEXT;
#define glCheckFramebufferStatusEXT mglCheckFramebufferStatusEXT
	FUNC_TYPE PFNGLATTACHSHADERPROC mglAttachShader;
#define glAttachShader mglAttachShader
	FUNC_TYPE PFNGLCOMPILESHADERPROC mglCompileShader;
#define glCompileShader mglCompileShader
	FUNC_TYPE PFNGLCREATEPROGRAMPROC mglCreateProgram;
#define glCreateProgram mglCreateProgram
	FUNC_TYPE PFNGLDELETEPROGRAMPROC mglDeleteProgram;
#define glDeleteProgram mglDeleteProgram
	FUNC_TYPE PFNGLDELETESHADERPROC mglDeleteShader;
#define glDeleteShader mglDeleteShader
	FUNC_TYPE PFNGLGETPROGRAMIVPROC mglGetProgramiv;
#define glGetProgramiv mglGetProgramiv
	FUNC_TYPE PFNGLGETPROGRAMINFOLOGPROC mglGetProgramInfoLog;
#define glGetProgramInfoLog mglGetProgramInfoLog
	FUNC_TYPE PFNGLGETSHADERIVPROC mglGetShaderiv;
#define glGetShaderiv mglGetShaderiv
	FUNC_TYPE PFNGLGETSHADERINFOLOGPROC mglGetShaderInfoLog;
#define glGetShaderInfoLog mglGetShaderInfoLog
	FUNC_TYPE PFNGLGETUNIFORMLOCATIONPROC mglGetUniformLocation;
#define glGetUniformLocation mglGetUniformLocation
	FUNC_TYPE PFNGLLINKPROGRAMPROC mglLinkProgram;
#define glLinkProgram mglLinkProgram
	FUNC_TYPE PFNGLSHADERSOURCEPROC mglShaderSource;
#define glShaderSource mglShaderSource
}
