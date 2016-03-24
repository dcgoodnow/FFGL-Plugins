#pragma once
#include <stdio.h>
#include <string>
#include <vector>

#define GLEW_STATIC

#include <gl/glew.h>
#include "FFGL.h"
#include "FFGLLib.h"
#include "FFGLShader.h"
#include "FFGLPluginSDK.h"

#if (!(defined(WIN32) || defined(_WIN32) || defined(__WIN32__)))
// posix
typedef uint8_t  CHAR;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int8_t  BYTE;
typedef int16_t SHORT;
typedef int32_t LONG;
typedef LONG INT;
typedef INT BOOL;
typedef int64_t __int64;
typedef int64_t LARGE_INTEGER;
#include <ctime>
#include <chrono> // c++11 timer
#endif

#define GL_SHADING_LANGUAGE_VERSION	0x8B8C
#define GL_READ_FRAMEBUFFER_EXT		0x8CA8
#define GL_TEXTURE_WRAP_R			0x8072

struct Vertex
{
	GLfloat position[2];
	GLfloat color[4];
};

class EdgeTracer : public CFreeFrameGLPlugin
{

public:

	EdgeTracer();

	~EdgeTracer();

	FFResult SetFloatParameter( unsigned int index, float value );
	float GetFloatParameter( unsigned int index );
	FFResult ProcessOpenGL( ProcessOpenGLStruct* pGL );
	FFResult InitGL( const FFGLViewportStruct *vp );
	FFResult DeInitGL();
	FFResult GetInputStatus( DWORD dwIndex );
	char * GetParameterDisplay( DWORD dwIndex );

	bool LoadShader( std::string shaderCode );


	///////////////////////////////////////////////////
	// Factory method
	///////////////////////////////////////////////////
	static FFResult __stdcall CreateInstance( CFreeFrameGLPlugin **ppOutInstance )
	{
		*ppOutInstance = new EdgeTracer();
		if (*ppOutInstance != NULL)
			return FF_SUCCESS;
		return FF_FAIL;
	}

private:

	bool bInitialized;

	int m_initResources;
	FFGLExtensions m_extensions;
	FFGLShader m_shader;

	GLuint m_fbo;

	GLuint m_VertexLocation;
	GLuint m_ColorLocation;

	Vertex m_rect[4] = {
		{{-1.0, -1.0}, {1.0, 0.0, 0.0, 1.0}},
		{{-1.0, 1.0}, {1.0, 0.0, 0.0, 1.0}},
		{{1.0, 1.0}, {1.0, 0.0, 0.0, 0.0}},
		{{1.0, -1.0}, {1.0, 0.0, 0.0, 0.0}}
	};

	GLuint m_rectLocation;

	// Viewport
	float m_vpWidth;
	float m_vpHeight;
};
