#include "MirrorNative.h"

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
int( *cross_secure_sprintf )(char *, size_t, const char *, ...) = sprintf_s;
#else 
// posix
int( *cross_secure_sprintf )(char *, size_t, const char *, ...) = snprintf;
#endif

#define FFPARAM_BOTTOM	(0)
#define FFPARAM_LEFT	(1)
#define FFPARAM_TOP		(2)
#define FFPARAM_RIGHT	(3)

#define STRINGIFY(A) #A

///Plugin Info
static CFFGLPluginInfo PluginInfo(
	MirrorNative::CreateInstance,		// Create method
	"MRNA",								// *** Plugin unique ID (4 chars) - this must be unique for each plugin
	"Mirror Native",					// *** Plugin name - make it different for each plugin 
	1,						   			// API major version number 													
	006,								// API minor version number	
	1,									// *** Plugin major version number
	000,								// *** Plugin minor version number
	FF_EFFECT,							// Plugin type can always be an effect
										// FF_SOURCE,						// or change this to FF_SOURCE for shaders that do not use a texture
	"Mirror for EDGE Nightclub Native Content",			// *** Plugin description - you can expand on this
	"by Daniel Goodnow (danielgoodnow@gmail.com)"		// *** About - use your own name and details
	);


// Common vertex shader code as per FreeFrame examples
char *vertexShaderCode = STRINGIFY(
	void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_FrontColor = gl_Color;

} );


// Important notes :

// The shader code is pasted into a section of the source file below
// which uses the Stringizing operator (#) (https://msdn.microsoft.com/en-us/library/7e3a913x.aspx)
// This converts the shader code into a string which is then used by the shader compiler on load of the plugin.
// There are some limitations of using the stringizing operator in this way because it depends on the "#" symbol,
// e.g. #( .. code ), Therefore there cannot be any # characters in the code itself.
//
// For example it is common to see :
//
//		#ifdef GL_ES
//		precision mediump float;
//		#endif
//
//	The #ifdef and #endif have to be removed.
//
//		// #ifdef GL_ES
//		// precision mediump float;
//		// #endif
//
// Compile the code as-is to start with and you should get a functioning plugin.
// Several examples can be used below. Only one can be compiled at a time.
//

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ++++++ COPY/PASTE YOUR GLSL SANDBOX OR SHADERTOY SHADER CODE HERE +++++
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
char *fragmentShaderCode = STRINGIFY(
	// ==================== PASTE WITHIN THESE LINES =======================

	uniform sampler2D tex0;

	// Red screen test shader
	void main(void) {
		gl_FragColor = texture2D( tex0, gl_TexCoord[0].st );
	}
);

MirrorNative::MirrorNative()
{
#ifdef DEBUG
	// Debug console window so printf works
	FILE* pCout; // should really be freed on exit
	AllocConsole();
	freopen_s(&pCout, "CONOUT$", "w", stdout);
	printf("Shader Maker Vers 1.004\n");
	printf("GLSL version [%s]\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
#endif
	
	SetMinInputs( 0 );
	SetMaxInputs( 2 );

	//Setup Parameters
	SetParamInfo( FFPARAM_BOTTOM,	"Bottom Bound"	, FF_TYPE_STANDARD, 0.0f );
	SetParamInfo( FFPARAM_LEFT,		"Left Bound"	, FF_TYPE_STANDARD, 0.0f );
	SetParamInfo( FFPARAM_TOP,		"Top Bound"		, FF_TYPE_STANDARD, 1.0f );
	SetParamInfo( FFPARAM_RIGHT,	"Right Bound"	, FF_TYPE_STANDARD, 1.0f );

	m_Roi = { 0.0f, 0.0f, 1.0f, 1.0f };

	SetDefaults();

	bInitialized = false;
}

MirrorNative::~MirrorNative()
{
	//stub
}

FFResult MirrorNative::InitGL( const FFGLViewportStruct * vp )
{
	m_extensions.Initialize();
	if (m_extensions.multitexture == 0 || m_extensions.ARB_shader_objects == 0)
		return FF_FAIL;

	m_vpWidth = (float)vp->width;
	m_vpHeight = (float)vp->height;


	StartCounter();

	std::string shaderString = fragmentShaderCode;
	bInitialized = LoadShader( shaderString );
	return FF_SUCCESS;
}

FFResult MirrorNative::DeInitGL()
{
	if (m_fbo)	m_extensions.glDeleteFramebuffersEXT( 1, &m_fbo );
	if (m_glTexture0)	glDeleteTextures( 1, &m_glTexture0 );
	if (m_glTexture1)	glDeleteTextures( 1, &m_glTexture1 );
	if (m_glTexture2)	glDeleteTextures( 1, &m_glTexture2 );
	if (m_glTexture3)	glDeleteTextures( 1, &m_glTexture3 );

	m_glTexture0 = 0;
	m_glTexture1 = 0;
	m_glTexture2 = 0;
	m_glTexture3 = 0;
	m_fbo = 0;
	bInitialized = false;

	return FF_SUCCESS;
}

FFResult MirrorNative::ProcessOpenGL( ProcessOpenGLStruct * pGL )
{
	FFGLTextureStruct Texture0;

	FFGLTexCoords maxCoords;

	if (bInitialized) 
	{
		float vpdim[4];
		glGetFloatv( GL_VIEWPORT, vpdim );
		m_vpWidth = vpdim[2];
		m_vpHeight = vpdim[3];

		for (ROI screen : this->screens)
		{
			if (m_inputTextureLocation >= 0 || m_inputTextureLocation1 >= 0)
			{
				if (m_inputTextureLocation >= 0 && pGL->numInputTextures > 0 && pGL->inputTextures[0] != NULL)
				{
					Texture0 = *(pGL->inputTextures[0]);
					maxCoords = GetMaxGLTexCoords( Texture0 );

					if ((int)m_channelResolution[0][0] != Texture0.Width
						|| (int)m_channelResolution[0][1] != Texture0.Height)
					{
						if (m_glTexture0 > 0)
						{
							glDeleteTextures( 1, &m_glTexture0 );
							m_glTexture0 = 0;
						}
					}

					m_channelResolution[0][0] = (float)Texture0.Width;
					m_channelResolution[0][1] = (float)Texture0.Height;

					//CreateRectangleTexture( Texture0, maxCoords, m_glTexture0, GL_TEXTURE0, m_fbo, pGL->HostFBO );

					CreateRectangleTexture( Texture0, maxCoords, screen, m_glTexture0, GL_TEXTURE0, m_fbo, pGL->HostFBO );
				}
			}

			lastTime = elapsedTime;
			elapsedTime = GetCounter() / 1000.0;
			//m_time = m_time + (float)(elapsedTime - lastTime);

			m_shader.BindShader();


			//Bind all the variables!
			if (m_inputTextureLocation >= 0 && Texture0.Handle > 0)
			{
				m_extensions.glUniform1iARB( m_inputTextureLocation, 0 );
			}

			if (m_inputTextureLocation >= 0 && Texture0.Handle > 0)
			{
				m_extensions.glActiveTexture( GL_TEXTURE0 );

				if (m_glTexture0 > 0)
					glBindTexture( GL_TEXTURE_2D, m_glTexture0 );
				else
					glBindTexture( GL_TEXTURE_2D, Texture0.Handle );
			}

			ROI normalizedRoi = screen;
			normalizedRoi.bottom = normalizedRoi.bottom * 2 - 1;
			normalizedRoi.left = normalizedRoi.left * 2 - 1;
			normalizedRoi.top = normalizedRoi.top * 2 - 1;
			normalizedRoi.right = normalizedRoi.right * 2 - 1;

			glEnable( GL_TEXTURE_2D );
			glBegin( GL_QUADS );
			glTexCoord2f( 0.0, 0.0 );
			glVertex2f( normalizedRoi.left, normalizedRoi.bottom );
			glTexCoord2f( 0.0, 1.0 );
			glVertex2f( normalizedRoi.left, normalizedRoi.top );
			glTexCoord2f( 1.0, 1.0 );
			glVertex2f( (normalizedRoi.right - normalizedRoi.left) / 2 + normalizedRoi.left, normalizedRoi.top );
			glTexCoord2f( 1.0, 0.0 );
			glVertex2f( (normalizedRoi.right - normalizedRoi.left) / 2 + normalizedRoi.left, normalizedRoi.bottom );

			//mirror
			glTexCoord2f( 1.0, 0.0 );
			glVertex2f( (normalizedRoi.right - normalizedRoi.left) / 2 + normalizedRoi.left, normalizedRoi.bottom );
			glTexCoord2f( 1.0, 1.0 );
			glVertex2f( (normalizedRoi.right - normalizedRoi.left) / 2 + normalizedRoi.left, normalizedRoi.top );
			glTexCoord2f( 0.0, 1.0 );
			glVertex2f( normalizedRoi.right, normalizedRoi.top );
			glTexCoord2f( 0.0, 0.0 );
			glVertex2f( normalizedRoi.right, normalizedRoi.bottom );
			glEnd();
			glDisable( GL_TEXTURE_2D );

			// unbind input texture 0
			if (m_inputTextureLocation >= 0 && Texture0.Handle > 0) {
				m_extensions.glActiveTexture( GL_TEXTURE0 );
				glBindTexture( GL_TEXTURE_2D, 0 );
			}

			m_shader.UnbindShader();

		}

	}

	return FF_SUCCESS;
}

FFResult MirrorNative::SetFloatParameter( unsigned int index, float value )
{
	switch (index)
	{
	case FFPARAM_BOTTOM:
		m_Roi.bottom = value;
		break;

	case FFPARAM_LEFT:
		m_Roi.left = value;
		break;

	case FFPARAM_TOP:
		m_Roi.top = value;
		break;

	case FFPARAM_RIGHT:
		m_Roi.right = value;
		break;

	default:
		return FF_FAIL;
	}
	return FF_SUCCESS;
}

float MirrorNative::GetFloatParameter( unsigned int index )
{
	switch (index)
	{
	case FFPARAM_BOTTOM:
		return m_Roi.bottom;
		break;

	case FFPARAM_LEFT:
		return m_Roi.left;
		break;

	case FFPARAM_TOP:
		return m_Roi.top;
		break;

	case FFPARAM_RIGHT:
		return m_Roi.right;
		break;

	default:
		return FF_FAIL;
	}
}

FFResult MirrorNative::GetInputStatus( DWORD dwIndex )
{
	return FF_SUCCESS;
}

char * MirrorNative::GetParameterDisplay( DWORD dwIndex )
{
	return "1";
}

void MirrorNative::SetDefaults()
{
	elapsedTime = 0.0;
	lastTime = 0.0;
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
	PCFreq = 0.0;
	CounterStart = 0;
#else
	start = std::chrono::steady_clock::now();
#endif

	m_glTexture0 = 0;
	m_glTexture1 = 0;
	m_fbo = 0;

	//set screen ROI's

	ROI screen;
	
	//back wall
	screen.bottom = 1.0f - (512.0f / 1080.0f);
	screen.top = 1.0f;
	screen.left = 0.0f;
	screen.right = .5f;
	this->screens.push_back( screen );

	//dj booth
	screen.bottom = 1.0f - ((512.0f + 375.0f) / 1080.0f);
	screen.top = 1.0f - (512.0f / 1080.0f);
	screen.left = 649.0 / 4096.0;
	screen.right = screen.left + (750.0f / 4096.0f);
	this->screens.push_back( screen );

	//screen near bar
	screen.bottom = 1.0f - (384.0f / 1080.0f);
	screen.top = 1.0f;
	screen.left = (2432.0f / 4096.0f);
	screen.right = (3712.0f / 4096.0f);
	this->screens.push_back( screen );

	//outer wall L
	screen.bottom = 0.0f;
	screen.top = 384.0f / 1080.0f;
	screen.left = 1792.0f / 4096.0f;
	screen.right = screen.left + (640.0f / 4096.0f);
	this->screens.push_back( screen );

	//south wall l
	screen.left = screen.right;
	screen.right += 512.0f / 4096.0f;
	this->screens.push_back( screen );

	//south wall r
	screen.left = screen.right;
	screen.right += 512.0f / 4096.0f;
	this->screens.push_back( screen );

	//outer wall r
	screen.left = screen.right;
	screen.right += 640.0f / 4096.0f;
	this->screens.push_back( screen );
}

void MirrorNative::StartCounter()
{
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
	LARGE_INTEGER li;
	// Find frequency
	QueryPerformanceFrequency( &li );
	PCFreq = double( li.QuadPart ) / 1000.0;
	// Second call needed
	QueryPerformanceCounter( &li );
	CounterStart = li.QuadPart;
#else
	// posix c++11
	start = std::chrono::steady_clock::now();
#endif
}
	FFGLTextureStruct Texture1;

double MirrorNative::GetCounter()
{
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
	LARGE_INTEGER li;
	QueryPerformanceCounter( &li );
	return double( li.QuadPart - CounterStart ) / PCFreq;
#else
	// posix c++11
	end = std::chrono::steady_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.;
#endif
	return 0;

}

bool MirrorNative::LoadShader( std::string shaderString )
{
	m_shader.SetExtensions( &m_extensions );
	if (!m_shader.Compile( vertexShaderCode, shaderString.c_str() ))
	{
		printf( "Shader failed to compile." );
		return false;
	}
	else
	{
		bool success = false;
		if (m_shader.IsReady())
		{
			if (m_shader.BindShader())
				success = true;
		}
		if (!success)
			return false;
		else
		{
			//get uniform locations here using m_shader.FindUniform("string")
			m_inputTextureLocation = -1;

			if (m_inputTextureLocation < 0)
				m_inputTextureLocation = m_shader.FindUniform( "tex0" );

			m_shader.UnbindShader();
			if (m_glTexture0 > 0) glDeleteTextures( 1, &m_glTexture0 );
			if (m_glTexture1 > 0) glDeleteTextures( 1, &m_glTexture1 );
			m_glTexture0 = 0;
			m_glTexture1 = 0;

			StartCounter();

			return true;
		}
	}
	return false;
}

void MirrorNative::CreateRectangleTexture( FFGLTextureStruct texture, FFGLTexCoords maxCoords, GLuint & glTexture, GLenum texunit, GLuint & fbo, GLuint hostFbo )
{
	if (fbo == 0)
	{
		m_extensions.glGenFramebuffersEXT( 1, &fbo );
	}

	if (glTexture == 0)
	{
		glGenTextures( 1, &glTexture );
		m_extensions.glActiveTexture( texunit );
		glBindTexture( GL_TEXTURE_2D, glTexture );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, texture.Width, texture.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
		glBindTexture( GL_TEXTURE_2D, 0 );
		m_extensions.glActiveTexture( GL_TEXTURE0 );
	}
	m_extensions.glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, fbo );
	m_extensions.glFramebufferTexture2DEXT( GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, glTexture, 0 );

	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, texture.Handle );
	glBegin( GL_QUADS );

	//lower left
	glTexCoord2f( 0.0, 0.0 );
	glVertex2f( -1.0, -1.0 );
	//upper left
	glTexCoord2f( 0.0, (float)maxCoords.t );
	glVertex2f( -1.0, 1.0 );
	// upper right
	glTexCoord2f( (float)maxCoords.s, (float)maxCoords.t );
	glVertex2f( 1.0, 1.0 );
	//lower right
	glTexCoord2f( (float)maxCoords.s, 0.0 );
	glVertex2f( 1.0, -1.0 );
	glEnd();
	glDisable( GL_TEXTURE_2D );

	glBindTexture( GL_TEXTURE_2D , 0);

	if (hostFbo > 0)
		m_extensions.glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, hostFbo );
	else
		m_extensions.glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
}

void MirrorNative::CreateRectangleTexture( FFGLTextureStruct texture, FFGLTexCoords maxCoords, ROI roi, GLuint & glTexture, GLenum texunit, GLuint & fbo, GLuint hostFbo )
{
	if (fbo == 0)
	{
		m_extensions.glGenFramebuffersEXT( 1, &fbo );
	}

	float texWidth = (float)maxCoords.s;
	float texHeight = (float)maxCoords.t;

	roi.left *= texWidth;
	roi.right *= texWidth;
	roi.bottom *= texHeight;
	roi.top *= texHeight;

	if (glTexture == 0)
	{
		glGenTextures( 1, &glTexture );
		m_extensions.glActiveTexture( texunit );
		glBindTexture( GL_TEXTURE_2D, glTexture );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, texture.Width, texture.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
		glBindTexture( GL_TEXTURE_2D, 0 );
		m_extensions.glActiveTexture( GL_TEXTURE0 );
	}
	m_extensions.glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, fbo );
	m_extensions.glFramebufferTexture2DEXT( GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, glTexture, 0 );

	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, texture.Handle );
	glBegin( GL_QUADS );

	//lower left
	glTexCoord2f( roi.left, roi.bottom);
	glVertex2f( -1.0, -1.0 );
	//upper left
	glTexCoord2f( roi.left, roi.top);
	glVertex2f( -1.0, 1.0 );
	// upper right
	glTexCoord2f( roi.right, roi.top);
	glVertex2f( 1.0, 1.0 );
	//lower right
	glTexCoord2f( roi.right, roi.bottom);
	glVertex2f( 1.0, -1.0 );
	glEnd();
	glDisable( GL_TEXTURE_2D );

	glBindTexture( GL_TEXTURE_2D , 0);

	if (hostFbo > 0)
		m_extensions.glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, hostFbo );
	else
		m_extensions.glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
}
