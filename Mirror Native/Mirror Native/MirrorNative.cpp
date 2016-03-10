#include "MirrorNative.h"

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
int( *cross_secure_sprintf )(char *, size_t, const char *, ...) = sprintf_s;
#else 
// posix
int( *cross_secure_sprintf )(char *, size_t, const char *, ...) = snprintf;
#endif

#define FFPARAM_TEST	(0)

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


	// Red screen test shader
	void main(void) {
		gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
	}
);

MirrorNative::MirrorNative()
{
	// Debug console window so printf works
	FILE* pCout; // should really be freed on exit
	AllocConsole();
	freopen_s(&pCout, "CONOUT$", "w", stdout);
	printf("Shader Maker Vers 1.004\n");
	printf("GLSL version [%s]\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	
	SetMinInputs( 0 );
	SetMaxInputs( 2 );

	//Setup Parameters
	SetParamInfo( FFPARAM_TEST, "Test", FF_TYPE_STANDARD, 0.5f );

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
	FFGLTextureStruct Texture1;

	FFGLTexCoords maxCoords;
	time_t datime;
	struct tm tmbuff;

	if (bInitialized) 
	{
		float vpdim[4];
		glGetFloatv( GL_VIEWPORT, vpdim );
		m_vpWidth = vpdim[2];
		m_vpHeight = vpdim[3];

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

				CreateRectangleTexture( Texture0, maxCoords, m_glTexture0, GL_TEXTURE0, m_fbo, pGL->HostFBO );
			}
		}

		lastTime = elapsedTime;
		elapsedTime = GetCounter() / 1000.0;
		//m_time = m_time + (float)(elapsedTime - lastTime);

		m_shader.BindShader();

		if (m_inputTextureLocation >= 0 && Texture0.Handle > 0)
		{
			m_extensions.glUniform1iARB( m_inputTextureLocation, 0 );
		}

		//Bind all the variables!

		if (m_inputTextureLocation >= 0 && Texture0.Handle > 0)
		{
			m_extensions.glActiveTexture( GL_TEXTURE0 );

			if (m_glTexture0 > 0)
				glBindTexture( GL_TEXTURE_2D, m_glTexture0 );
			else
				glBindTexture( GL_TEXTURE_2D, Texture0.Handle);
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

		// unbind input texture 0
		if (m_inputTextureLocation >= 0 && Texture0.Handle > 0) {
			m_extensions.glActiveTexture( GL_TEXTURE0 );
			glBindTexture( GL_TEXTURE_2D, 0 );
		}

		m_shader.UnbindShader();
	}

	return FF_SUCCESS;
}

FFResult MirrorNative::SetFloatParameter( unsigned int index, float value )
{
	return FFResult();
}

float MirrorNative::GetFloatParameter( unsigned int index )
{
	return 0.0f;
}

FFResult MirrorNative::GetInputStatus( DWORD dwIndex )
{
	return FFResult();
}

char * MirrorNative::GetParameterDisplay( DWORD dwIndex )
{
	return nullptr;
}
