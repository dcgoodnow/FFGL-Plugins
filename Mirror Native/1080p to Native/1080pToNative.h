#pragma once
#include <stdio.h>
#include <string>
#include <vector>
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

struct ROI
{
	float top;
	float left;
	float bottom;
	float right;
};

class C1080pToNative : public CFreeFrameGLPlugin
{

public:

	///
	///	Constructor.
	///

	C1080pToNative();

	///
	///	Destructor.
	///
	~C1080pToNative();

	///////////////////////////////////////////////////
	// FreeFrameGL plugin methods
	///////////////////////////////////////////////////
	FFResult SetFloatParameter( unsigned int index, float value );
	float GetFloatParameter( unsigned int index );
	FFResult ProcessOpenGL( ProcessOpenGLStruct* pGL );
	FFResult InitGL( const FFGLViewportStruct *vp );
	FFResult DeInitGL();
	FFResult GetInputStatus( DWORD dwIndex );
	char * GetParameterDisplay( DWORD dwIndex );

	///////////////////////////////////////////////////
	// Factory method
	///////////////////////////////////////////////////
	static FFResult __stdcall CreateInstance( CFreeFrameGLPlugin **ppOutInstance ) 
	{
		*ppOutInstance = new C1080pToNative();
		if (*ppOutInstance != NULL)
			return FF_SUCCESS;
		return FF_FAIL;
	}

protected:

	bool bInitialized;

	ROI m_Roi;

	std::vector<ROI> screens;

	// Local fbo and texture
	GLuint m_glTexture0;
	GLuint m_glTexture1;
	GLuint m_fbo;

	// Viewport
	float m_vpWidth;
	float m_vpHeight;

	int m_initResources;
	FFGLExtensions m_extensions;
	FFGLShader m_shader;

	GLint m_inputTextureLocation;



	void SetDefaults();
	void StartCounter();
	double GetCounter();
	bool LoadShader( std::string shaderString );
	void CreateRectangleTexture( FFGLTextureStruct texture, FFGLTexCoords maxCoords, GLuint &glTexture, GLenum texunit, GLuint &fbo, GLuint hostFbo );
	void CreateRectangleTexture( FFGLTextureStruct texture, FFGLTexCoords maxCoords, ROI roi, GLuint &glTexture, GLenum texunit, GLuint &fbo, GLuint hostFbo );
};
