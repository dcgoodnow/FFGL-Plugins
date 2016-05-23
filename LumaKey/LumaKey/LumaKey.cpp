#include "LumaKey.h"

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
int( *cross_secure_sprintf )(char *, size_t, const char *, ...) = sprintf_s;
#else 
// posix
int( *cross_secure_sprintf )(char *, size_t, const char *, ...) = snprintf;
#endif

#define FFPARAM_THRESHOLD_BEGIN (0)
#define FFPARAM_THRESHOLD_END (1)

#define STRINGIFY(A) #A

///Plugin Info
static CFFGLPluginInfo PluginInfo(
	LumaKey::CreateInstance,		// Create method
	"LMKY",								// *** Plugin unique ID (4 chars) - this must be unique for each plugin
	"Luma Key",							// *** Plugin name - make it different for each plugin 
	1,						   			// API major version number 													
	006,								// API minor version number	
	1,									// *** Plugin major version number
	000,								// *** Plugin minor version number
	FF_EFFECT,							// Plugin type can always be an effect
										// FF_SOURCE,						// or change this to FF_SOURCE for shaders that do not use a texture
	"Luma Key: Filters out all pixels "					// *** Plugin description - you can expand on this
	"with a luma value below the threshold",			
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
	uniform float thresholdBegin;
	uniform float thresholdEnd;

// Red screen test shader
void main( void ) {
	vec4 color = texture2D( tex0, gl_TexCoord[0].st );
	if (thresholdEnd < .01)
		gl_FragColor = color;
	else
	{
		float luma = .2126 * color.x + .7152 * color.y + .0722 * color.z;
		float alpha = smoothstep( thresholdBegin + .0001, thresholdEnd + .0001, luma );
		gl_FragColor = vec4( color.xyz, alpha * color.w );
	}
}
);

LumaKey::LumaKey()
{
#ifdef DEBUG
	// Debug console window so printf works
	FILE* pCout; // should really be freed on exit
	AllocConsole();
	freopen_s( &pCout, "CONOUT$", "w", stdout );
	printf( "Shader Maker Vers 1.004\n" );
	printf( "GLSL version [%s]\n", glGetString( GL_SHADING_LANGUAGE_VERSION ) );
#endif

	SetMinInputs( 0 );
	SetMaxInputs( 1 );

	//Setup Parameters
	SetParamInfo( FFPARAM_THRESHOLD_BEGIN, "Threshold Begin", FF_TYPE_STANDARD, 0.0f );
	SetParamInfo( FFPARAM_THRESHOLD_END, "Threshold End", FF_TYPE_STANDARD, 0.0f );

	SetDefaults();

	bInitialized = false;
}

LumaKey::~LumaKey()
{
	//stub
}

void LumaKey::SetDefaults()
{
	m_thresholdEnd = 0.0;
	m_thresholdBegin = 0.0;
}

FFResult LumaKey::InitGL( const FFGLViewportStruct *vp )
{
	m_extensions.Initialize();
	if (m_extensions.multitexture == 0 || m_extensions.ARB_shader_objects == 0)
		return FF_FAIL;

	m_vpWidth = (float)vp->width;
	m_vpHeight = (float)vp->height;
	
	std::string shaderString = fragmentShaderCode;
	bInitialized = LoadShader( shaderString );
	return FF_SUCCESS;
}

FFResult LumaKey::DeInitGL()
{
	if (m_fbo)			m_extensions.glDeleteFramebuffersEXT( 1, &m_fbo );
	if (m_glTexture0)	glDeleteTextures( 1, &m_glTexture0 );

	m_glTexture0 = 0;
	m_fbo = 0;
	bInitialized = false;

	return FF_SUCCESS;
}

FFResult LumaKey::GetInputStatus( DWORD dwIndex )
{
	return FF_SUCCESS;
}

FFResult LumaKey::ProcessOpenGL( ProcessOpenGLStruct * pGL )
{
	FFGLTextureStruct Texture0;

	FFGLTexCoords maxCoords;

	if (bInitialized)
	{
		float vpdim[4];
		glGetFloatv( GL_VIEWPORT, vpdim );
		m_vpWidth = vpdim[2];
		m_vpHeight = vpdim[3];

		if (m_inputTextureLocation >= 0)
		{
			if (m_inputTextureLocation >= 0 && pGL->numInputTextures > 0 && pGL->inputTextures[0] != NULL)
			{
				Texture0 = *(pGL->inputTextures[0]);
				maxCoords = GetMaxGLTexCoords( Texture0 );
				if ((int)m_resolution[0] != Texture0.Width || (int)m_resolution[1] != Texture0.Height)
				{
					if (m_glTexture0 > 0)
					{
						glDeleteTextures( 1, &m_glTexture0 );
						m_glTexture0 = 0;
					}
				}

				m_resolution[0] = (float)Texture0.Width;
				m_resolution[1] = (float)Texture0.Height;

				CreateRectangleTexture( Texture0, maxCoords, m_glTexture0, GL_TEXTURE0, m_fbo, pGL->HostFBO );

			}
		}

		m_shader.BindShader();

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

		if (m_thresholdEndLocation >= 0)
		{
			m_extensions.glUniform1fARB( m_thresholdEndLocation, m_thresholdEnd );
		}

		if (m_thresholdBeginLocation >= 0)
		{
			m_extensions.glUniform1fARB( m_thresholdBeginLocation, m_thresholdBegin );
		}

		glEnable( GL_TEXTURE_2D );
		glBegin( GL_QUADS );
		glTexCoord2f( 0.0, 0.0 );
		glVertex2f( -1.0, -1.0 );
		glTexCoord2f( 0.0, 1.0 );
		glVertex2f( -1.0, 1.0 );
		glTexCoord2f( 1.0, 1.0 );
		glVertex2f( 1.0, 1.0 );
		glTexCoord2f( 1.0, 0.0 );
		glVertex2f( 1.0, -1.0 );
		glEnd();
		glDisable( GL_TEXTURE_2D );

		if (m_inputTextureLocation >= 0 && Texture0.Handle > 0) {
			m_extensions.glActiveTexture( GL_TEXTURE0 );
			glBindTexture( GL_TEXTURE_2D, 0 );
		}

		m_shader.UnbindShader();

	}

	return FF_SUCCESS;
}

FFResult LumaKey::SetFloatParameter( unsigned int index, float value )
{
	switch (index)
	{
	case FFPARAM_THRESHOLD_END:
		m_thresholdEnd = value;
		break;

	case FFPARAM_THRESHOLD_BEGIN:
		if(value <= m_thresholdEnd)
			m_thresholdBegin = value;
		break;

	default:
		return FF_FAIL;
		break;
	}
	return FF_SUCCESS;
}

float LumaKey::GetFloatParameter( unsigned int index )
{
	switch (index)
	{
	case FFPARAM_THRESHOLD_END:
		return m_thresholdEnd;
		break;

	case FFPARAM_THRESHOLD_BEGIN:
		return m_thresholdBegin;
		break;

	default:
		return 0.0f;
	}
}

char * LumaKey::GetParameterDisplay( DWORD dwIndex )
{
	return "1";
}

bool LumaKey::LoadShader( std::string shaderString )
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

			m_thresholdEndLocation = -1;
			m_thresholdEndLocation = m_shader.FindUniform( "thresholdEnd" );

			m_thresholdBeginLocation = -1;
			m_thresholdBeginLocation = m_shader.FindUniform( "thresholdBegin" );

			m_shader.UnbindShader();
			if (m_glTexture0 > 0) glDeleteTextures( 1, &m_glTexture0 );
			m_glTexture0 = 0;

			return true;
		}
	}
	return false;
}

void LumaKey::CreateRectangleTexture( FFGLTextureStruct texture, FFGLTexCoords maxCoords, GLuint & glTexture, GLenum texunit, GLuint & fbo, GLuint hostFbo )
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

	glBindTexture( GL_TEXTURE_2D, 0 );

	if (hostFbo > 0)
		m_extensions.glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, hostFbo );
	else
		m_extensions.glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
}
