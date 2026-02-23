/*
 * OpenCiv4 OpenGL 3.3 Core Loader
 * Minimal glad-compatible GL function loader.
 * Only includes functions used by the engine.
 */
#ifndef GLAD_GL_H_
#define GLAD_GL_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Calling convention ---- */
#ifndef APIENTRY
  #ifdef _WIN32
    #define APIENTRY __stdcall
  #else
    #define APIENTRY
  #endif
#endif
#define APIENTRYP APIENTRY *

/* ================================================================
 * GL type definitions
 * ================================================================ */
typedef void GLvoid;
typedef unsigned int GLenum;
typedef float GLfloat;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLbitfield;
typedef double GLdouble;
typedef unsigned int GLuint;
typedef unsigned char GLboolean;
typedef unsigned char GLubyte;
typedef char GLchar;
typedef short GLshort;
typedef signed char GLbyte;
typedef unsigned short GLushort;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef float GLclampf;
typedef double GLclampd;

/* ================================================================
 * GL constants
 * ================================================================ */

/* Boolean */
#define GL_FALSE                    0
#define GL_TRUE                     1
#define GL_NONE                     0

/* Errors */
#define GL_NO_ERROR                 0
#define GL_INVALID_ENUM             0x0500
#define GL_INVALID_VALUE            0x0501
#define GL_INVALID_OPERATION        0x0502
#define GL_OUT_OF_MEMORY            0x0505

/* Clear buffer bits */
#define GL_DEPTH_BUFFER_BIT         0x00000100
#define GL_STENCIL_BUFFER_BIT       0x00000400
#define GL_COLOR_BUFFER_BIT         0x00004000

/* Enable/Disable caps */
#define GL_DEPTH_TEST               0x0B71
#define GL_BLEND                    0x0BE2
#define GL_CULL_FACE                0x0B44
#define GL_SCISSOR_TEST             0x0C11
#define GL_LINE_SMOOTH              0x0B20

/* Blend factors */
#define GL_ZERO                     0
#define GL_ONE                      1
#define GL_SRC_COLOR                0x0300
#define GL_ONE_MINUS_SRC_COLOR      0x0301
#define GL_SRC_ALPHA                0x0302
#define GL_ONE_MINUS_SRC_ALPHA      0x0303
#define GL_DST_ALPHA                0x0304
#define GL_ONE_MINUS_DST_ALPHA      0x0305
#define GL_DST_COLOR                0x0306
#define GL_ONE_MINUS_DST_COLOR      0x0307

/* Depth functions */
#define GL_NEVER                    0x0200
#define GL_LESS                     0x0201
#define GL_EQUAL                    0x0202
#define GL_LEQUAL                   0x0203
#define GL_GREATER                  0x0204
#define GL_NOTEQUAL                 0x0205
#define GL_GEQUAL                   0x0206
#define GL_ALWAYS                   0x0207

/* Face culling */
#define GL_FRONT                    0x0404
#define GL_BACK                     0x0405
#define GL_FRONT_AND_BACK           0x0408
#define GL_CW                       0x0900
#define GL_CCW                      0x0901

/* Polygon modes */
#define GL_POINT                    0x1B00
#define GL_LINE                     0x1B01
#define GL_FILL                     0x1B02

/* Data types */
#define GL_BYTE                     0x1400
#define GL_UNSIGNED_BYTE            0x1401
#define GL_SHORT                    0x1402
#define GL_UNSIGNED_SHORT           0x1403
#define GL_INT                      0x1404
#define GL_UNSIGNED_INT             0x1405
#define GL_FLOAT                    0x1406

/* Primitive types */
#define GL_POINTS                   0x0000
#define GL_LINES                    0x0001
#define GL_LINE_LOOP                0x0002
#define GL_LINE_STRIP               0x0003
#define GL_TRIANGLES                0x0004
#define GL_TRIANGLE_STRIP           0x0005
#define GL_TRIANGLE_FAN             0x0006

/* Texture targets */
#define GL_TEXTURE_2D               0x0DE1

/* Texture parameters */
#define GL_TEXTURE_MAG_FILTER       0x2800
#define GL_TEXTURE_MIN_FILTER       0x2801
#define GL_TEXTURE_WRAP_S           0x2802
#define GL_TEXTURE_WRAP_T           0x2803

/* Texture filter modes */
#define GL_NEAREST                  0x2600
#define GL_LINEAR                   0x2601
#define GL_NEAREST_MIPMAP_NEAREST   0x2700
#define GL_LINEAR_MIPMAP_NEAREST    0x2701
#define GL_NEAREST_MIPMAP_LINEAR    0x2702
#define GL_LINEAR_MIPMAP_LINEAR     0x2703

/* Texture wrap modes */
#define GL_REPEAT                   0x2901
#define GL_CLAMP_TO_EDGE            0x812F
#define GL_MIRRORED_REPEAT          0x8370

/* Pixel store */
#define GL_UNPACK_ALIGNMENT         0x0CF5
#define GL_PACK_ALIGNMENT           0x0D05

/* Texture internal formats */
#define GL_RED                      0x1903
#define GL_RG                       0x8227
#define GL_RGB                      0x1907
#define GL_RGBA                     0x1908
#define GL_R8                       0x8229
#define GL_RG8                      0x822B
#define GL_RGB8                     0x8051
#define GL_RGBA8                    0x8058
#define GL_SRGB8_ALPHA8             0x8C43

/* Texture units */
#define GL_TEXTURE0                 0x84C0
#define GL_TEXTURE1                 0x84C1
#define GL_TEXTURE2                 0x84C2
#define GL_TEXTURE3                 0x84C3

/* Buffer targets */
#define GL_ARRAY_BUFFER             0x8892
#define GL_ELEMENT_ARRAY_BUFFER     0x8893

/* Buffer usage */
#define GL_STREAM_DRAW              0x88E0
#define GL_STATIC_DRAW              0x88E4
#define GL_DYNAMIC_DRAW             0x88E8

/* Shader types */
#define GL_VERTEX_SHADER            0x8B31
#define GL_FRAGMENT_SHADER          0x8B30
#define GL_GEOMETRY_SHADER          0x8DD9

/* Shader/program query */
#define GL_COMPILE_STATUS           0x8B81
#define GL_LINK_STATUS              0x8B82
#define GL_VALIDATE_STATUS          0x8B83
#define GL_INFO_LOG_LENGTH          0x8B84
#define GL_ACTIVE_UNIFORMS          0x8B86
#define GL_ACTIVE_ATTRIBUTES        0x8B89

/* Framebuffer */
#define GL_FRAMEBUFFER              0x8D40
#define GL_READ_FRAMEBUFFER         0x8C3A
#define GL_DRAW_FRAMEBUFFER         0x8CA9
#define GL_COLOR_ATTACHMENT0        0x8CE0
#define GL_DEPTH_ATTACHMENT         0x8D00
#define GL_STENCIL_ATTACHMENT       0x8D20
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_FRAMEBUFFER_COMPLETE     0x8CD5

/* String/limit queries */
#define GL_VENDOR                   0x1F00
#define GL_RENDERER                 0x1F01
#define GL_VERSION                  0x1F02
#define GL_EXTENSIONS               0x1F03
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_NUM_EXTENSIONS           0x821D
#define GL_MAX_TEXTURE_SIZE         0x0D33
#define GL_MAX_VERTEX_ATTRIBS       0x8869

/* ================================================================
 * GL function pointer type definitions
 * ================================================================ */

/* -- GL 1.0 -- */
typedef void (APIENTRYP PFNGLCULLFACEPROC)(GLenum mode);
typedef void (APIENTRYP PFNGLFRONTFACEPROC)(GLenum mode);
typedef void (APIENTRYP PFNGLLINEWIDTHPROC)(GLfloat width);
typedef void (APIENTRYP PFNGLPOLYGONMODEPROC)(GLenum face, GLenum mode);
typedef void (APIENTRYP PFNGLSCISSORPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLTEXPARAMETERFPROC)(GLenum target, GLenum pname, GLfloat param);
typedef void (APIENTRYP PFNGLTEXPARAMETERIPROC)(GLenum target, GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLTEXIMAGE2DPROC)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
typedef void (APIENTRYP PFNGLCLEARPROC)(GLbitfield mask);
typedef void (APIENTRYP PFNGLCLEARCOLORPROC)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (APIENTRYP PFNGLDISABLEPROC)(GLenum cap);
typedef void (APIENTRYP PFNGLENABLEPROC)(GLenum cap);
typedef void (APIENTRYP PFNGLFLUSHPROC)(void);
typedef void (APIENTRYP PFNGLFINISHPROC)(void);
typedef void (APIENTRYP PFNGLBLENDFUNCPROC)(GLenum sfactor, GLenum dfactor);
typedef void (APIENTRYP PFNGLDEPTHFUNCPROC)(GLenum func);
typedef void (APIENTRYP PFNGLDEPTHMASKPROC)(GLboolean flag);
typedef void (APIENTRYP PFNGLCOLORMASKPROC)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
typedef void (APIENTRYP PFNGLPIXELSTOREIPROC)(GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLREADPIXELSPROC)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
typedef GLenum (APIENTRYP PFNGLGETERRORPROC)(void);
typedef const GLubyte* (APIENTRYP PFNGLGETSTRINGPROC)(GLenum name);
typedef void (APIENTRYP PFNGLGETINTEGERVPROC)(GLenum pname, GLint* data);
typedef void (APIENTRYP PFNGLGETFLOATVPROC)(GLenum pname, GLfloat* data);
typedef void (APIENTRYP PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei width, GLsizei height);

/* -- GL 1.1 -- */
typedef void (APIENTRYP PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void (APIENTRYP PFNGLDRAWELEMENTSPROC)(GLenum mode, GLsizei count, GLenum type, const void* indices);
typedef void (APIENTRYP PFNGLGENTEXTURESPROC)(GLsizei n, GLuint* textures);
typedef void (APIENTRYP PFNGLDELETETEXTURESPROC)(GLsizei n, const GLuint* textures);
typedef void (APIENTRYP PFNGLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void (APIENTRYP PFNGLTEXSUBIMAGE2DPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);

/* -- GL 1.3 -- */
typedef void (APIENTRYP PFNGLACTIVETEXTUREPROC)(GLenum texture);

/* -- GL 1.4 -- */
typedef void (APIENTRYP PFNGLBLENDFUNCSEPARATEPROC)(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);

/* -- GL 1.5 -- */
typedef void (APIENTRYP PFNGLGENBUFFERSPROC)(GLsizei n, GLuint* buffers);
typedef void (APIENTRYP PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint* buffers);
typedef void (APIENTRYP PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (APIENTRYP PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void (APIENTRYP PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);

/* -- GL 2.0 -- */
typedef GLuint (APIENTRYP PFNGLCREATESHADERPROC)(GLenum type);
typedef void (APIENTRYP PFNGLDELETESHADERPROC)(GLuint shader);
typedef void (APIENTRYP PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
typedef void (APIENTRYP PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void (APIENTRYP PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint* params);
typedef void (APIENTRYP PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef GLuint (APIENTRYP PFNGLCREATEPROGRAMPROC)(void);
typedef void (APIENTRYP PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void (APIENTRYP PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (APIENTRYP PFNGLDETACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (APIENTRYP PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (APIENTRYP PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint* params);
typedef void (APIENTRYP PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void (APIENTRYP PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void (APIENTRYP PFNGLVALIDATEPROGRAMPROC)(GLuint program);
typedef GLint (APIENTRYP PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar* name);
typedef GLint (APIENTRYP PFNGLGETATTRIBLOCATIONPROC)(GLuint program, const GLchar* name);
typedef void (APIENTRYP PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
typedef void (APIENTRYP PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (APIENTRYP PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (APIENTRYP PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void (APIENTRYP PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void (APIENTRYP PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRYP PFNGLUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRYP PFNGLUNIFORM4FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRYP PFNGLUNIFORMMATRIX3FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (APIENTRYP PFNGLUNIFORMMATRIX4FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);

/* -- GL 3.0 -- */
typedef void (APIENTRYP PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint* arrays);
typedef void (APIENTRYP PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint* arrays);
typedef void (APIENTRYP PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void (APIENTRYP PFNGLGENERATEMIPMAPPROC)(GLenum target);
typedef const GLubyte* (APIENTRYP PFNGLGETSTRINGIPROC)(GLenum name, GLuint index);
typedef void (APIENTRYP PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint* framebuffers);
typedef void (APIENTRYP PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint* framebuffers);
typedef void (APIENTRYP PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void (APIENTRYP PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLenum (APIENTRYP PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);

/* -- GL 3.1 -- */
typedef void (APIENTRYP PFNGLDRAWARRAYSINSTANCEDPROC)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
typedef void (APIENTRYP PFNGLDRAWELEMENTSINSTANCEDPROC)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount);

/* ================================================================
 * GL function pointer declarations + #define aliases
 * ================================================================ */

/* GL 1.0 */
extern PFNGLCULLFACEPROC glad_glCullFace;
extern PFNGLFRONTFACEPROC glad_glFrontFace;
extern PFNGLLINEWIDTHPROC glad_glLineWidth;
extern PFNGLPOLYGONMODEPROC glad_glPolygonMode;
extern PFNGLSCISSORPROC glad_glScissor;
extern PFNGLTEXPARAMETERFPROC glad_glTexParameterf;
extern PFNGLTEXPARAMETERIPROC glad_glTexParameteri;
extern PFNGLTEXIMAGE2DPROC glad_glTexImage2D;
extern PFNGLCLEARPROC glad_glClear;
extern PFNGLCLEARCOLORPROC glad_glClearColor;
extern PFNGLDISABLEPROC glad_glDisable;
extern PFNGLENABLEPROC glad_glEnable;
extern PFNGLFLUSHPROC glad_glFlush;
extern PFNGLFINISHPROC glad_glFinish;
extern PFNGLBLENDFUNCPROC glad_glBlendFunc;
extern PFNGLDEPTHFUNCPROC glad_glDepthFunc;
extern PFNGLDEPTHMASKPROC glad_glDepthMask;
extern PFNGLCOLORMASKPROC glad_glColorMask;
extern PFNGLPIXELSTOREIPROC glad_glPixelStorei;
extern PFNGLREADPIXELSPROC glad_glReadPixels;
extern PFNGLGETERRORPROC glad_glGetError;
extern PFNGLGETSTRINGPROC glad_glGetString;
extern PFNGLGETINTEGERVPROC glad_glGetIntegerv;
extern PFNGLGETFLOATVPROC glad_glGetFloatv;
extern PFNGLVIEWPORTPROC glad_glViewport;

/* GL 1.1 */
extern PFNGLDRAWARRAYSPROC glad_glDrawArrays;
extern PFNGLDRAWELEMENTSPROC glad_glDrawElements;
extern PFNGLGENTEXTURESPROC glad_glGenTextures;
extern PFNGLDELETETEXTURESPROC glad_glDeleteTextures;
extern PFNGLBINDTEXTUREPROC glad_glBindTexture;
extern PFNGLTEXSUBIMAGE2DPROC glad_glTexSubImage2D;

/* GL 1.3 */
extern PFNGLACTIVETEXTUREPROC glad_glActiveTexture;

/* GL 1.4 */
extern PFNGLBLENDFUNCSEPARATEPROC glad_glBlendFuncSeparate;

/* GL 1.5 */
extern PFNGLGENBUFFERSPROC glad_glGenBuffers;
extern PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers;
extern PFNGLBINDBUFFERPROC glad_glBindBuffer;
extern PFNGLBUFFERDATAPROC glad_glBufferData;
extern PFNGLBUFFERSUBDATAPROC glad_glBufferSubData;

/* GL 2.0 */
extern PFNGLCREATESHADERPROC glad_glCreateShader;
extern PFNGLDELETESHADERPROC glad_glDeleteShader;
extern PFNGLSHADERSOURCEPROC glad_glShaderSource;
extern PFNGLCOMPILESHADERPROC glad_glCompileShader;
extern PFNGLGETSHADERIVPROC glad_glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog;
extern PFNGLCREATEPROGRAMPROC glad_glCreateProgram;
extern PFNGLDELETEPROGRAMPROC glad_glDeleteProgram;
extern PFNGLATTACHSHADERPROC glad_glAttachShader;
extern PFNGLDETACHSHADERPROC glad_glDetachShader;
extern PFNGLLINKPROGRAMPROC glad_glLinkProgram;
extern PFNGLGETPROGRAMIVPROC glad_glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog;
extern PFNGLUSEPROGRAMPROC glad_glUseProgram;
extern PFNGLVALIDATEPROGRAMPROC glad_glValidateProgram;
extern PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation;
extern PFNGLGETATTRIBLOCATIONPROC glad_glGetAttribLocation;
extern PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray;
extern PFNGLUNIFORM1IPROC glad_glUniform1i;
extern PFNGLUNIFORM1FPROC glad_glUniform1f;
extern PFNGLUNIFORM2FPROC glad_glUniform2f;
extern PFNGLUNIFORM3FPROC glad_glUniform3f;
extern PFNGLUNIFORM4FPROC glad_glUniform4f;
extern PFNGLUNIFORMMATRIX3FVPROC glad_glUniformMatrix3fv;
extern PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv;

/* GL 3.0 */
extern PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays;
extern PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray;
extern PFNGLGENERATEMIPMAPPROC glad_glGenerateMipmap;
extern PFNGLGETSTRINGIPROC glad_glGetStringi;
extern PFNGLGENFRAMEBUFFERSPROC glad_glGenFramebuffers;
extern PFNGLDELETEFRAMEBUFFERSPROC glad_glDeleteFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC glad_glBindFramebuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC glad_glFramebufferTexture2D;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus;

/* GL 3.1 */
extern PFNGLDRAWARRAYSINSTANCEDPROC glad_glDrawArraysInstanced;
extern PFNGLDRAWELEMENTSINSTANCEDPROC glad_glDrawElementsInstanced;

/* ---- Name aliases (so code writes glClear not glad_glClear) ---- */
#define glCullFace              glad_glCullFace
#define glFrontFace             glad_glFrontFace
#define glLineWidth             glad_glLineWidth
#define glPolygonMode           glad_glPolygonMode
#define glScissor               glad_glScissor
#define glTexParameterf         glad_glTexParameterf
#define glTexParameteri         glad_glTexParameteri
#define glTexImage2D            glad_glTexImage2D
#define glClear                 glad_glClear
#define glClearColor            glad_glClearColor
#define glDisable               glad_glDisable
#define glEnable                glad_glEnable
#define glFlush                 glad_glFlush
#define glFinish                glad_glFinish
#define glBlendFunc             glad_glBlendFunc
#define glDepthFunc             glad_glDepthFunc
#define glDepthMask             glad_glDepthMask
#define glColorMask             glad_glColorMask
#define glPixelStorei           glad_glPixelStorei
#define glReadPixels            glad_glReadPixels
#define glGetError              glad_glGetError
#define glGetString             glad_glGetString
#define glGetIntegerv           glad_glGetIntegerv
#define glGetFloatv             glad_glGetFloatv
#define glViewport              glad_glViewport
#define glDrawArrays            glad_glDrawArrays
#define glDrawElements          glad_glDrawElements
#define glGenTextures           glad_glGenTextures
#define glDeleteTextures        glad_glDeleteTextures
#define glBindTexture           glad_glBindTexture
#define glTexSubImage2D         glad_glTexSubImage2D
#define glActiveTexture         glad_glActiveTexture
#define glBlendFuncSeparate     glad_glBlendFuncSeparate
#define glGenBuffers            glad_glGenBuffers
#define glDeleteBuffers         glad_glDeleteBuffers
#define glBindBuffer            glad_glBindBuffer
#define glBufferData            glad_glBufferData
#define glBufferSubData         glad_glBufferSubData
#define glCreateShader          glad_glCreateShader
#define glDeleteShader          glad_glDeleteShader
#define glShaderSource          glad_glShaderSource
#define glCompileShader         glad_glCompileShader
#define glGetShaderiv           glad_glGetShaderiv
#define glGetShaderInfoLog      glad_glGetShaderInfoLog
#define glCreateProgram         glad_glCreateProgram
#define glDeleteProgram         glad_glDeleteProgram
#define glAttachShader          glad_glAttachShader
#define glDetachShader          glad_glDetachShader
#define glLinkProgram           glad_glLinkProgram
#define glGetProgramiv          glad_glGetProgramiv
#define glGetProgramInfoLog     glad_glGetProgramInfoLog
#define glUseProgram            glad_glUseProgram
#define glValidateProgram       glad_glValidateProgram
#define glGetUniformLocation    glad_glGetUniformLocation
#define glGetAttribLocation     glad_glGetAttribLocation
#define glVertexAttribPointer   glad_glVertexAttribPointer
#define glEnableVertexAttribArray   glad_glEnableVertexAttribArray
#define glDisableVertexAttribArray  glad_glDisableVertexAttribArray
#define glUniform1i             glad_glUniform1i
#define glUniform1f             glad_glUniform1f
#define glUniform2f             glad_glUniform2f
#define glUniform3f             glad_glUniform3f
#define glUniform4f             glad_glUniform4f
#define glUniformMatrix3fv      glad_glUniformMatrix3fv
#define glUniformMatrix4fv      glad_glUniformMatrix4fv
#define glGenVertexArrays       glad_glGenVertexArrays
#define glDeleteVertexArrays    glad_glDeleteVertexArrays
#define glBindVertexArray       glad_glBindVertexArray
#define glGenerateMipmap        glad_glGenerateMipmap
#define glGetStringi            glad_glGetStringi
#define glGenFramebuffers       glad_glGenFramebuffers
#define glDeleteFramebuffers    glad_glDeleteFramebuffers
#define glBindFramebuffer       glad_glBindFramebuffer
#define glFramebufferTexture2D  glad_glFramebufferTexture2D
#define glCheckFramebufferStatus glad_glCheckFramebufferStatus
#define glDrawArraysInstanced   glad_glDrawArraysInstanced
#define glDrawElementsInstanced glad_glDrawElementsInstanced

/* ================================================================
 * Loader API
 * ================================================================ */
typedef void* (*GLADloadproc)(const char* name);
int gladLoadGLLoader(GLADloadproc load);

#ifdef __cplusplus
}
#endif

#endif /* GLAD_GL_H_ */
