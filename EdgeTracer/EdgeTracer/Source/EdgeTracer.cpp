#include "EdgeTracer.h"


#if (defined( WIN32 ) || defined( _WIN32 ) || defined( __WIN32__ ))
int( *cross_secure_sprintf )(char *, size_t, const char *, ...) = sprintf_s;
#else 
// posix
int( *cross_secure_sprintf )(char *, size_t, const char *, ...) = snprintf;
#endif

#define FFPARAM_BOTTOM	(0)

#define STRINGIFY(A) #A


///Plugin Info
static CFFGLPluginInfo PluginInfo(
	EdgeTracer::CreateInstance,			// Create method
	"EGTR",								// *** Plugin unique ID (4 chars) - this must be unique for each plugin
	"Edge Tracer",						// *** Plugin name - make it different for each plugin 
	1,						   			// API major version number 													
	006,								// API minor version number	
	1,									// *** Plugin major version number
	000,								// *** Plugin minor version number
	FF_EFFECT,							// Plugin type can always be an effect
										// FF_SOURCE,						// or change this to FF_SOURCE for shaders that do not use a texture
	"Tracer effect",					// *** Plugin description - you can expand on this
	"by Daniel Goodnow (danielgoodnow@gmail.com)"		// *** About - use your own name and details
	);


// Common vertex shader code as per FreeFrame examples
char *vertexShaderCode = STRINGIFY(

in layout(location = 0) vec2 vtex;
in layout(location = 1) vec4 color;

out vec4 o_color;

void main()
{
	gl_Position = vtex;
	o_color = color;
} );

char *fragmentShaderCode = STRINGIFY(
	// ==================== PASTE WITHIN THESE LINES =======================

in vec4 o_color;

// Red screen test shader
void main( void ) {
	//gl_FragColor = o_color;
	gl_FragColor = o_color;
}
);


EdgeTracer::EdgeTracer()
{
#ifdef DEBUG
	// Debug console window so printf works
	FILE* pCout; // should really be freed on exit
	AllocConsole();
	freopen_s( &pCout, "CONOUT$", "w", stdout );
	printf( "Edge Tracer Vers 1.00\n" );
	printf( "GLSL version [%s]\n", glGetString( GL_SHADING_LANGUAGE_VERSION ) );
#endif

	SetMinInputs( 0 );
	SetMaxInputs( 0 );

	//SetParamInfo( Param_id, param_name, param_type, default_value);

	//Setup Defaults;

	GLenum status = glewInit();
	if (status != GLEW_OK)
	{
		printf( "Glew not initialized...\n" );
		printf( (char*)glewGetErrorString( status ));
		printf( "\n" );
	}
	
	bInitialized = false;
}

EdgeTracer::~EdgeTracer()
{
	//stub
}

FFResult EdgeTracer::InitGL( const FFGLViewportStruct * vp )
{
	m_extensions.Initialize();
	if (m_extensions.multitexture == 0 || m_extensions.ARB_shader_objects == 0)
	{
		return FF_FAIL;
	}

	m_vpWidth = (float)vp->width;
	m_vpHeight = (float)vp->height;

	std::string shaderString = fragmentShaderCode;
	bInitialized = LoadShader( shaderString );
	if (bInitialized)
	{
		glGenBuffers( 1, &m_rectLocation );
		glBindBuffer( GL_ARRAY_BUFFER, m_rectLocation );
		glBufferData( GL_ARRAY_BUFFER, sizeof( m_rect ), m_rect, GL_STATIC_DRAW);


		return FF_SUCCESS;
	}
	return FF_FAIL;
}

FFResult EdgeTracer::DeInitGL()
{
	if (m_fbo) m_extensions.glDeleteFramebuffersEXT( 1, &m_fbo );

	m_fbo = 0;

	bInitialized = false;

	return FF_SUCCESS;
}

FFResult EdgeTracer::ProcessOpenGL( ProcessOpenGLStruct *pGl )
{
	FFGLTextureStruct Texture0;

	FFGLTexCoords maxCoords;

	m_shader.BindShader();

	glEnableVertexAttribArray( m_VertexLocation );
	glEnableVertexAttribArray( m_ColorLocation );
	glBindBuffer( GL_ARRAY_BUFFER, m_rectLocation );

	glVertexAttribPointer( m_VertexLocation,
		2,
		GL_FLOAT,
		GL_FALSE,
		sizeof( Vertex ),
		0 );

	glVertexAttribPointer( m_ColorLocation,
		4,
		GL_FLOAT,
		GL_FALSE,
		sizeof( Vertex ),
		(void*)offsetof( Vertex, color ) );

	glDrawArrays( GL_QUADS, 0, 4 );

	glDisableVertexAttribArray( m_VertexLocation );
	glDisableVertexAttribArray( m_ColorLocation );

	m_shader.UnbindShader();

	return FF_SUCCESS;
}


FFResult EdgeTracer::SetFloatParameter( unsigned int index, float value )
{
	switch (index)
	{
		//deal with cases here

	default:
		return FF_FAIL;
	}
	return FF_SUCCESS;
}

float EdgeTracer::GetFloatParameter( unsigned int index )
{
	switch (index)
	{
		//deal with cases here

	default:
		return FF_FAIL;
	}
}

FFResult EdgeTracer::GetInputStatus( DWORD dwIndex )
{
	return FF_SUCCESS;
}

char * EdgeTracer::GetParameterDisplay( DWORD dwIndex )
{
	return "fixme";
}

bool EdgeTracer::LoadShader( std::string shaderCode )
{
	m_shader.SetExtensions( &m_extensions );
	if (!m_shader.Compile( vertexShaderCode, shaderCode.c_str() ))
	{
		printf( "Shader failed to compile" );
		GLint isCompiled = 0;
		glGetShaderiv( m_shader.GetVertexShaderID(), GL_COMPILE_STATUS, &isCompiled );
		if (isCompiled == GL_FALSE)
		{
			GLint length = 0;

			glGetShaderiv( m_shader.GetVertexShaderID(), GL_INFO_LOG_LENGTH, &length );

			std::vector<char> errorLog( length );
			glGetShaderInfoLog( m_shader.GetVertexShaderID(), length, &length, &errorLog[0] );

			printf( &errorLog[0] );
		}
		glGetShaderiv( m_shader.GetFragmentShaderID(), GL_COMPILE_STATUS, &isCompiled );
		if (isCompiled == GL_FALSE)
		{
			GLint length = 0;

			glGetShaderiv( m_shader.GetFragmentShaderID(), GL_INFO_LOG_LENGTH, &length );

			std::vector<char> errorLog( length );
			glGetShaderInfoLog( m_shader.GetFragmentShaderID(), length, &length, &errorLog[0] );

			printf( &errorLog[0] );
		}

		return false;
	}

	else
	{
		bool success = false;
		if (m_shader.IsReady())
		{
			if (m_shader.BindShader())
			{
				success = true;
			}
		}
		if (!success)
		{
			return success;
		}

		else
		{
			//get uniform locations here using m_shader.FindUniform("string")
			
			m_shader.BindShader();
			m_VertexLocation = glGetAttribLocationARB( m_shader.GetShaderID(), "vtex" );

			
			m_ColorLocation = glGetAttribLocationARB( m_shader.GetShaderID(), "color" );

			return true;
		}
	}
	return false;
}