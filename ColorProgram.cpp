#include "ColorProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Load< ColorProgram > color_program(LoadTagEarly);

ColorProgram::ColorProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
#ifdef __ANDROID__
		"#version 320 es\n"
#else
		"#version 330\n"
#endif
		"uniform mat4 OBJECT_TO_CLIP;\n"
		"in vec4 Position;\n"
		"in vec4 Color;\n"
		"out vec4 color;\n"
		"void main() {\n"
		"	gl_Position = OBJECT_TO_CLIP * Position;\n"
		"	color = Color;\n"
		"}\n"
	,
		//fragment shader:
#ifdef __ANDROID__
		"#version 320 es\n"
		"precision highp int;\n" //overkill but saves re-writing the shader below
		"precision highp float;\n" //similar
#else
		"#version 330\n"
#endif
		"in vec4 color;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	fragColor = color;\n"
		"}\n"
	,
		"ColorProgram"
	);
	//As you can see above, adjacent strings in C/C++ are concatenated.
	// this is very useful for writing long shader programs inline.

	//look up the locations of vertex attributes:
	Position_vec4 = glGetAttribLocation(program, "Position");
	Color_vec4 = glGetAttribLocation(program, "Color");

	//look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(program, "OBJECT_TO_CLIP");
}

ColorProgram::~ColorProgram() {
	glDeleteProgram(program);
	program = 0;
}

