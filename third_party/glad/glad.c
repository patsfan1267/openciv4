/*
 * OpenCiv4 OpenGL 3.3 Core Loader — Implementation
 * Loads all GL function pointers via the provided loader function.
 */
#include "glad.h"

/* ================================================================
 * Function pointer storage (all start as NULL)
 * ================================================================ */

/* GL 1.0 */
PFNGLCULLFACEPROC glad_glCullFace = 0;
PFNGLFRONTFACEPROC glad_glFrontFace = 0;
PFNGLLINEWIDTHPROC glad_glLineWidth = 0;
PFNGLPOLYGONMODEPROC glad_glPolygonMode = 0;
PFNGLSCISSORPROC glad_glScissor = 0;
PFNGLTEXPARAMETERFPROC glad_glTexParameterf = 0;
PFNGLTEXPARAMETERIPROC glad_glTexParameteri = 0;
PFNGLTEXIMAGE2DPROC glad_glTexImage2D = 0;
PFNGLCLEARPROC glad_glClear = 0;
PFNGLCLEARCOLORPROC glad_glClearColor = 0;
PFNGLDISABLEPROC glad_glDisable = 0;
PFNGLENABLEPROC glad_glEnable = 0;
PFNGLFLUSHPROC glad_glFlush = 0;
PFNGLFINISHPROC glad_glFinish = 0;
PFNGLBLENDFUNCPROC glad_glBlendFunc = 0;
PFNGLDEPTHFUNCPROC glad_glDepthFunc = 0;
PFNGLDEPTHMASKPROC glad_glDepthMask = 0;
PFNGLCOLORMASKPROC glad_glColorMask = 0;
PFNGLPIXELSTOREIPROC glad_glPixelStorei = 0;
PFNGLREADPIXELSPROC glad_glReadPixels = 0;
PFNGLGETERRORPROC glad_glGetError = 0;
PFNGLGETSTRINGPROC glad_glGetString = 0;
PFNGLGETINTEGERVPROC glad_glGetIntegerv = 0;
PFNGLGETFLOATVPROC glad_glGetFloatv = 0;
PFNGLVIEWPORTPROC glad_glViewport = 0;

/* GL 1.1 */
PFNGLDRAWARRAYSPROC glad_glDrawArrays = 0;
PFNGLDRAWELEMENTSPROC glad_glDrawElements = 0;
PFNGLGENTEXTURESPROC glad_glGenTextures = 0;
PFNGLDELETETEXTURESPROC glad_glDeleteTextures = 0;
PFNGLBINDTEXTUREPROC glad_glBindTexture = 0;
PFNGLTEXSUBIMAGE2DPROC glad_glTexSubImage2D = 0;

/* GL 1.3 */
PFNGLACTIVETEXTUREPROC glad_glActiveTexture = 0;

/* GL 1.4 */
PFNGLBLENDFUNCSEPARATEPROC glad_glBlendFuncSeparate = 0;

/* GL 1.5 */
PFNGLGENBUFFERSPROC glad_glGenBuffers = 0;
PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers = 0;
PFNGLBINDBUFFERPROC glad_glBindBuffer = 0;
PFNGLBUFFERDATAPROC glad_glBufferData = 0;
PFNGLBUFFERSUBDATAPROC glad_glBufferSubData = 0;

/* GL 2.0 */
PFNGLCREATESHADERPROC glad_glCreateShader = 0;
PFNGLDELETESHADERPROC glad_glDeleteShader = 0;
PFNGLSHADERSOURCEPROC glad_glShaderSource = 0;
PFNGLCOMPILESHADERPROC glad_glCompileShader = 0;
PFNGLGETSHADERIVPROC glad_glGetShaderiv = 0;
PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog = 0;
PFNGLCREATEPROGRAMPROC glad_glCreateProgram = 0;
PFNGLDELETEPROGRAMPROC glad_glDeleteProgram = 0;
PFNGLATTACHSHADERPROC glad_glAttachShader = 0;
PFNGLDETACHSHADERPROC glad_glDetachShader = 0;
PFNGLLINKPROGRAMPROC glad_glLinkProgram = 0;
PFNGLGETPROGRAMIVPROC glad_glGetProgramiv = 0;
PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog = 0;
PFNGLUSEPROGRAMPROC glad_glUseProgram = 0;
PFNGLVALIDATEPROGRAMPROC glad_glValidateProgram = 0;
PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation = 0;
PFNGLGETATTRIBLOCATIONPROC glad_glGetAttribLocation = 0;
PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer = 0;
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray = 0;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray = 0;
PFNGLUNIFORM1IPROC glad_glUniform1i = 0;
PFNGLUNIFORM1FPROC glad_glUniform1f = 0;
PFNGLUNIFORM2FPROC glad_glUniform2f = 0;
PFNGLUNIFORM3FPROC glad_glUniform3f = 0;
PFNGLUNIFORM4FPROC glad_glUniform4f = 0;
PFNGLUNIFORMMATRIX3FVPROC glad_glUniformMatrix3fv = 0;
PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv = 0;

/* GL 3.0 */
PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays = 0;
PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays = 0;
PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray = 0;
PFNGLGENERATEMIPMAPPROC glad_glGenerateMipmap = 0;
PFNGLGETSTRINGIPROC glad_glGetStringi = 0;
PFNGLGENFRAMEBUFFERSPROC glad_glGenFramebuffers = 0;
PFNGLDELETEFRAMEBUFFERSPROC glad_glDeleteFramebuffers = 0;
PFNGLBINDFRAMEBUFFERPROC glad_glBindFramebuffer = 0;
PFNGLFRAMEBUFFERTEXTURE2DPROC glad_glFramebufferTexture2D = 0;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus = 0;

/* GL 3.1 */
PFNGLDRAWARRAYSINSTANCEDPROC glad_glDrawArraysInstanced = 0;
PFNGLDRAWELEMENTSINSTANCEDPROC glad_glDrawElementsInstanced = 0;

/* ================================================================
 * Loader implementation
 * ================================================================ */
int gladLoadGLLoader(GLADloadproc load) {
    if (!load) return 0;

    /* GL 1.0 */
    glad_glCullFace = (PFNGLCULLFACEPROC)load("glCullFace");
    glad_glFrontFace = (PFNGLFRONTFACEPROC)load("glFrontFace");
    glad_glLineWidth = (PFNGLLINEWIDTHPROC)load("glLineWidth");
    glad_glPolygonMode = (PFNGLPOLYGONMODEPROC)load("glPolygonMode");
    glad_glScissor = (PFNGLSCISSORPROC)load("glScissor");
    glad_glTexParameterf = (PFNGLTEXPARAMETERFPROC)load("glTexParameterf");
    glad_glTexParameteri = (PFNGLTEXPARAMETERIPROC)load("glTexParameteri");
    glad_glTexImage2D = (PFNGLTEXIMAGE2DPROC)load("glTexImage2D");
    glad_glClear = (PFNGLCLEARPROC)load("glClear");
    glad_glClearColor = (PFNGLCLEARCOLORPROC)load("glClearColor");
    glad_glDisable = (PFNGLDISABLEPROC)load("glDisable");
    glad_glEnable = (PFNGLENABLEPROC)load("glEnable");
    glad_glFlush = (PFNGLFLUSHPROC)load("glFlush");
    glad_glFinish = (PFNGLFINISHPROC)load("glFinish");
    glad_glBlendFunc = (PFNGLBLENDFUNCPROC)load("glBlendFunc");
    glad_glDepthFunc = (PFNGLDEPTHFUNCPROC)load("glDepthFunc");
    glad_glDepthMask = (PFNGLDEPTHMASKPROC)load("glDepthMask");
    glad_glColorMask = (PFNGLCOLORMASKPROC)load("glColorMask");
    glad_glPixelStorei = (PFNGLPIXELSTOREIPROC)load("glPixelStorei");
    glad_glReadPixels = (PFNGLREADPIXELSPROC)load("glReadPixels");
    glad_glGetError = (PFNGLGETERRORPROC)load("glGetError");
    glad_glGetString = (PFNGLGETSTRINGPROC)load("glGetString");
    glad_glGetIntegerv = (PFNGLGETINTEGERVPROC)load("glGetIntegerv");
    glad_glGetFloatv = (PFNGLGETFLOATVPROC)load("glGetFloatv");
    glad_glViewport = (PFNGLVIEWPORTPROC)load("glViewport");

    /* GL 1.1 */
    glad_glDrawArrays = (PFNGLDRAWARRAYSPROC)load("glDrawArrays");
    glad_glDrawElements = (PFNGLDRAWELEMENTSPROC)load("glDrawElements");
    glad_glGenTextures = (PFNGLGENTEXTURESPROC)load("glGenTextures");
    glad_glDeleteTextures = (PFNGLDELETETEXTURESPROC)load("glDeleteTextures");
    glad_glBindTexture = (PFNGLBINDTEXTUREPROC)load("glBindTexture");
    glad_glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC)load("glTexSubImage2D");

    /* GL 1.3 */
    glad_glActiveTexture = (PFNGLACTIVETEXTUREPROC)load("glActiveTexture");

    /* GL 1.4 */
    glad_glBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC)load("glBlendFuncSeparate");

    /* GL 1.5 */
    glad_glGenBuffers = (PFNGLGENBUFFERSPROC)load("glGenBuffers");
    glad_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)load("glDeleteBuffers");
    glad_glBindBuffer = (PFNGLBINDBUFFERPROC)load("glBindBuffer");
    glad_glBufferData = (PFNGLBUFFERDATAPROC)load("glBufferData");
    glad_glBufferSubData = (PFNGLBUFFERSUBDATAPROC)load("glBufferSubData");

    /* GL 2.0 */
    glad_glCreateShader = (PFNGLCREATESHADERPROC)load("glCreateShader");
    glad_glDeleteShader = (PFNGLDELETESHADERPROC)load("glDeleteShader");
    glad_glShaderSource = (PFNGLSHADERSOURCEPROC)load("glShaderSource");
    glad_glCompileShader = (PFNGLCOMPILESHADERPROC)load("glCompileShader");
    glad_glGetShaderiv = (PFNGLGETSHADERIVPROC)load("glGetShaderiv");
    glad_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)load("glGetShaderInfoLog");
    glad_glCreateProgram = (PFNGLCREATEPROGRAMPROC)load("glCreateProgram");
    glad_glDeleteProgram = (PFNGLDELETEPROGRAMPROC)load("glDeleteProgram");
    glad_glAttachShader = (PFNGLATTACHSHADERPROC)load("glAttachShader");
    glad_glDetachShader = (PFNGLDETACHSHADERPROC)load("glDetachShader");
    glad_glLinkProgram = (PFNGLLINKPROGRAMPROC)load("glLinkProgram");
    glad_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)load("glGetProgramiv");
    glad_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)load("glGetProgramInfoLog");
    glad_glUseProgram = (PFNGLUSEPROGRAMPROC)load("glUseProgram");
    glad_glValidateProgram = (PFNGLVALIDATEPROGRAMPROC)load("glValidateProgram");
    glad_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)load("glGetUniformLocation");
    glad_glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)load("glGetAttribLocation");
    glad_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)load("glVertexAttribPointer");
    glad_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)load("glEnableVertexAttribArray");
    glad_glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)load("glDisableVertexAttribArray");
    glad_glUniform1i = (PFNGLUNIFORM1IPROC)load("glUniform1i");
    glad_glUniform1f = (PFNGLUNIFORM1FPROC)load("glUniform1f");
    glad_glUniform2f = (PFNGLUNIFORM2FPROC)load("glUniform2f");
    glad_glUniform3f = (PFNGLUNIFORM3FPROC)load("glUniform3f");
    glad_glUniform4f = (PFNGLUNIFORM4FPROC)load("glUniform4f");
    glad_glUniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC)load("glUniformMatrix3fv");
    glad_glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)load("glUniformMatrix4fv");

    /* GL 3.0 */
    glad_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)load("glGenVertexArrays");
    glad_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)load("glDeleteVertexArrays");
    glad_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)load("glBindVertexArray");
    glad_glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)load("glGenerateMipmap");
    glad_glGetStringi = (PFNGLGETSTRINGIPROC)load("glGetStringi");
    glad_glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)load("glGenFramebuffers");
    glad_glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)load("glDeleteFramebuffers");
    glad_glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)load("glBindFramebuffer");
    glad_glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)load("glFramebufferTexture2D");
    glad_glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)load("glCheckFramebufferStatus");

    /* GL 3.1 */
    glad_glDrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC)load("glDrawArraysInstanced");
    glad_glDrawElementsInstanced = (PFNGLDRAWELEMENTSINSTANCEDPROC)load("glDrawElementsInstanced");

    /* Verify critical functions loaded */
    if (!glad_glClear || !glad_glCreateShader || !glad_glGenVertexArrays)
        return 0;

    return 1;
}
