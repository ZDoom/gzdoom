#ifndef R_RENDER
#define R_RENDER

#if defined(__APPLE__)
#define APIENTRY
#define APIENTRYP *
#endif // __APPLE__

#include "basictypes.h"
#ifndef _WIN32
typedef bool BOOL;
#endif

#ifndef PFNGLMULTITEXCOORD2FPROC
typedef void (APIENTRYP PFNGLMULTITEXCOORD2FPROC) (GLenum target, GLfloat s, GLfloat t);
#endif
#ifndef PFNGLMULTITEXCOORD2FVPROC
typedef void (APIENTRYP PFNGLMULTITEXCOORD2FVPROC) (GLenum target, const GLfloat *v);
#endif

enum RenderFlags
{
	RFL_NPOT_TEXTURE=1,
	RFL_NOSTENCIL=2,
	RFL_OCCLUSION_QUERY=4,
	// [BB] Added texture compression flags.
	RFL_TEXTURE_COMPRESSION=8,
	RFL_TEXTURE_COMPRESSION_S3TC=16,

	RFL_VBO = 32,
	RFL_MAP_BUFFER_RANGE = 64,
	RFL_FRAMEBUFFER = 128,
	RFL_TEXTUREBUFFER = 256,
	RFL_NVIDIA = 512,
	RFL_ATI = 1024,


	RFL_GL_20 = 0x10000000,
	RFL_GL_21 = 0x20000000,
	RFL_GL_30 = 0x40000000,
};

enum TexMode
{
	TMF_MASKBIT = 1,
	TMF_OPAQUEBIT = 2,
	TMF_INVERTBIT = 4,

	TM_MODULATE = 0,
	TM_MASK = TMF_MASKBIT,
	TM_OPAQUE = TMF_OPAQUEBIT,
	TM_INVERT = TMF_INVERTBIT,
	//TM_INVERTMASK = TMF_MASKBIT | TMF_INVERTBIT
	TM_INVERTOPAQUE = TMF_INVERTBIT | TMF_OPAQUEBIT,
};

struct RenderContext
{
	unsigned int flags;
	unsigned int shadermodel;
	unsigned int maxuniforms;
	int max_texturesize;
	char * vendorstring;

	int MaxLights() const
	{
		return maxuniforms>=2048? 128:64;
	}

	void (APIENTRY * LoadExtensions) ();
	void (APIENTRY * SetTextureMode) (int type);
	void (APIENTRY * PrintStartupLog) ();
	BOOL (APIENTRY * SetVSync) (int on);
#if !defined (unix) && !defined (__APPLE__)
	bool (APIENTRY * InitHardware) (HWND, bool allowsoftware, bool nostencil, int multisample);
	void (APIENTRY * Shutdown) ();
#else
	bool (APIENTRY * InitHardware) (bool allowsoftware, bool nostencil, int multisample);
#endif
	void (APIENTRY * SwapBuffers) ();
	bool (APIENTRY * SetFullscreen) (const char *devicename, int w, int h, int bits, int hz);


	void (APIENTRY * Begin) (GLenum mode);
	void (APIENTRY * End) (void);
	void (APIENTRY * DrawArrays) (GLenum mode, GLint first, GLsizei count);

	void (APIENTRY * TexCoord2f) (GLfloat s, GLfloat t);
	void (APIENTRY * TexCoord2fv) (const GLfloat *v);

	void (APIENTRY * Vertex2f) (GLfloat x, GLfloat y);
	void (APIENTRY * Vertex2i) (GLint x, GLint y);
	void (APIENTRY * Vertex3f) (GLfloat x, GLfloat y, GLfloat z);
	void (APIENTRY * Vertex3d) (GLdouble x, GLdouble y, GLdouble z);
	void (APIENTRY * Vertex3fv) (const GLfloat *v);

	void (APIENTRY * Color4f) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
	void (APIENTRY * Color4fv) (const GLfloat *v);
	void (APIENTRY * Color3f) (GLfloat red, GLfloat green, GLfloat blue);
	void (APIENTRY * Color3ub) (GLubyte red, GLubyte green, GLubyte blue);
	void (APIENTRY * Color4ub) (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);

	void (APIENTRY * BlendEquation) (GLenum);
	void (APIENTRY * ColorMask) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);

	void (APIENTRY * DepthFunc) (GLenum func);
	void (APIENTRY * DepthMask) (GLboolean flag);
	void (APIENTRY * DepthRange) (GLclampd zNear, GLclampd zFar);

	void (APIENTRY * StencilFunc) (GLenum func, GLint ref, GLuint mask);
	void (APIENTRY * StencilMask) (GLuint mask);
	void (APIENTRY * StencilOp) (GLenum fail, GLenum zfail, GLenum zpass);

	void (APIENTRY * MatrixMode) (GLenum mode);
	void (APIENTRY * PushMatrix) (void);
	void (APIENTRY * PopMatrix) (void);
	void (APIENTRY * LoadIdentity) (void);
	void (APIENTRY * MultMatrixd) (const GLdouble *m);
	void (APIENTRY * Translatef) (GLfloat x, GLfloat y, GLfloat z);
	void (APIENTRY * Ortho) (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
	void (APIENTRY * Scalef) (GLfloat x, GLfloat y, GLfloat z);
	void (APIENTRY * Rotatef) (GLfloat angle, GLfloat x, GLfloat y, GLfloat z);

	void (APIENTRY * Viewport) (GLint x, GLint y, GLsizei width, GLsizei height);
	void (APIENTRY * Scissor) (GLint x, GLint y, GLsizei width, GLsizei height);

	void (APIENTRY * Clear) (GLbitfield mask);
	void (APIENTRY * ClearColor) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
	void (APIENTRY * ClearDepth) (GLclampd depth);
	void (APIENTRY * ShadeModel) (GLenum mode);
	void (APIENTRY * Hint) (GLenum target, GLenum mode);

	void (APIENTRY * DisableClientState) (GLenum array);
	void (APIENTRY * EnableClientState) (GLenum array);

	void (APIENTRY * Fogf) (GLenum pname, GLfloat param);
	void (APIENTRY * Fogi) (GLenum pname, GLint param);
	void (APIENTRY * Fogfv) (GLenum pname, const GLfloat *params);

	void (APIENTRY * Enable) (GLenum cap);
	GLboolean (APIENTRY * IsEnabled) (GLenum cap);
	void (APIENTRY * Disable) (GLenum cap);

	void (APIENTRY * TexGeni) (GLenum coord, GLenum pname, GLint param);
	void (APIENTRY * DeleteTextures) (GLsizei n, const GLuint *textures);
	void (APIENTRY * GenTextures) (GLsizei n, GLuint *textures);
	void (APIENTRY * BindTexture) (GLenum target, GLuint texture);
	void (APIENTRY * TexImage2D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
	void (APIENTRY * TexParameterf) (GLenum target, GLenum pname, GLfloat param);
	void (APIENTRY * TexParameteri) (GLenum target, GLenum pname, GLint param);
	void (APIENTRY * CopyTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);

	void (APIENTRY * ReadPixels) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
	void (APIENTRY * PolygonOffset) (GLfloat factor, GLfloat units);
	void (APIENTRY * ClipPlane) (GLenum which, const GLdouble *);

	void (APIENTRY * Finish) (void);
	void (APIENTRY * Flush) (void);

	// ARB_SHADER_OBJECTS
	PFNGLDELETESHADERPROC DeleteShader;
	PFNGLDELETEPROGRAMPROC DeleteProgram;
	PFNGLDETACHSHADERPROC DetachShader;
	PFNGLCREATESHADERPROC CreateShader;
	PFNGLSHADERSOURCEPROC ShaderSource;
	PFNGLCOMPILESHADERPROC CompileShader;
	PFNGLCREATEPROGRAMPROC CreateProgram;
	PFNGLATTACHSHADERPROC AttachShader;
	PFNGLLINKPROGRAMPROC LinkProgram;
	PFNGLUSEPROGRAMPROC UseProgram;
	PFNGLVALIDATEPROGRAMPROC ValidateProgram;
	PFNGLVERTEXATTRIB1FPROC VertexAttrib1f;
	PFNGLVERTEXATTRIB2FPROC VertexAttrib2f;
	PFNGLVERTEXATTRIB4FPROC VertexAttrib4f;
	PFNGLVERTEXATTRIB2FVPROC VertexAttrib2fv;
	PFNGLVERTEXATTRIB3FVPROC VertexAttrib3fv;
	PFNGLVERTEXATTRIB4FVPROC VertexAttrib4fv;
	PFNGLVERTEXATTRIB4UBVPROC VertexAttrib4ubv;
	PFNGLGETATTRIBLOCATIONPROC GetAttribLocation;
	PFNGLBINDATTRIBLOCATIONPROC BindAttribLocation;

	PFNGLUNIFORM1FPROC Uniform1f;
	PFNGLUNIFORM2FPROC Uniform2f;
	PFNGLUNIFORM3FPROC Uniform3f;
	PFNGLUNIFORM4FPROC Uniform4f;
	PFNGLUNIFORM1IPROC Uniform1i;
	PFNGLUNIFORM2IPROC Uniform2i;
	PFNGLUNIFORM3IPROC Uniform3i;
	PFNGLUNIFORM4IPROC Uniform4i;
	PFNGLUNIFORM1FVPROC Uniform1fv;
	PFNGLUNIFORM2FVPROC Uniform2fv;
	PFNGLUNIFORM3FVPROC Uniform3fv;
	PFNGLUNIFORM4FVPROC Uniform4fv;
	PFNGLUNIFORM1IVPROC Uniform1iv;
	PFNGLUNIFORM2IVPROC Uniform2iv;
	PFNGLUNIFORM3IVPROC Uniform3iv;
	PFNGLUNIFORM4IVPROC Uniform4iv;

	PFNGLUNIFORMMATRIX2FVPROC UniformMatrix2fv;
	PFNGLUNIFORMMATRIX3FVPROC UniformMatrix3fv;
	PFNGLUNIFORMMATRIX4FVPROC UniformMatrix4fv;

	PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;
	PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog;
	PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation;
	PFNGLGETACTIVEUNIFORMPROC GetActiveUniform;
	PFNGLGETUNIFORMFVPROC GetUniformfv;
	PFNGLGETUNIFORMIVPROC GetUniformiv;
	PFNGLGETSHADERSOURCEPROC GetShaderSource;
	PFNGLGETOBJECTPARAMETERIVARBPROC GetObjectParameteriv;
	
	PFNGLGENQUERIESARBPROC GenQueries;
	PFNGLDELETEQUERIESARBPROC DeleteQueries;
	PFNGLBEGINQUERYARBPROC BeginQuery;
	PFNGLENDQUERYARBPROC EndQuery;
	PFNGLGETQUERYOBJECTUIVARBPROC GetQueryObjectuiv;

	PFNGLACTIVETEXTUREPROC ActiveTexture;
	PFNGLMULTITEXCOORD2FPROC MultiTexCoord2f;
	PFNGLMULTITEXCOORD2FVPROC MultiTexCoord2fv;

	PFNGLBINDBUFFERPROC BindBuffer;
	PFNGLDELETEBUFFERSPROC DeleteBuffers;
	PFNGLGENBUFFERSPROC GenBuffers;
	PFNGLBUFFERDATAPROC BufferData;
	PFNGLBUFFERSUBDATAPROC BufferSubData;
	PFNGLMAPBUFFERPROC MapBuffer;
	PFNGLUNMAPBUFFERPROC UnmapBuffer;
	PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;
	PFNGLDISABLEVERTEXATTRIBARRAYPROC DisableVertexAttribArray;
	PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer;

	PFNGLMAPBUFFERRANGEPROC MapBufferRange;
	PFNGLFLUSHMAPPEDBUFFERRANGEPROC FlushMappedBufferRange;

	PFNGLGENFRAMEBUFFERSPROC GenFramebuffers;
	PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers;
	PFNGLBINDFRAMEBUFFERPROC BindFramebuffer;
	PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D;
	PFNGLGENRENDERBUFFERSPROC GenRenderbuffers;
	PFNGLDELETERENDERBUFFERSPROC DeleteRenderbuffers;
	PFNGLBINDRENDERBUFFERPROC BindRenderbuffer;
	PFNGLRENDERBUFFERSTORAGEPROC RenderbufferStorage;
	PFNGLFRAMEBUFFERRENDERBUFFERPROC FramebufferRenderbuffer;

	PFNGLTEXBUFFERARBPROC TexBufferARB;
};


typedef void (APIENTRY * GetContextProc)(RenderContext & gl);

void APIENTRY GetContext(RenderContext & gl);

#endif

