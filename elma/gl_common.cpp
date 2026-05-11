#include "gl_renderer.h"
#include "affine_pic.h"
#include "pic8.h"
#include "main.h"

#include <glad/glad.h>
#include <cstring>

//void (*gl_render_callback)() = nullptr;
//
//
//static const char* VertexShaderSource = R"(
//#version 410 core
//layout (location = 0) in vec2 position;
//layout (location = 1) in vec2 texCoord;
//out vec2 fragTexCoord;
//
//void main() {
//    gl_Position = vec4(position, 0.0, 1.0);
//    fragTexCoord = texCoord;
//}
//)";
//
//static const char* FragmentShaderSource = R"(
//#version 410 core
//in vec2 fragTexCoord;
//out vec4 FragColor;
//uniform sampler2D IndexTexture;
//uniform sampler1D PaletteTexture;
//
//void main() {
//    float index = texture(IndexTexture, fragTexCoord).r;
//    FragColor = texture(PaletteTexture, index);
//}
//)";
//
//static SDL_GLContext GLContext = nullptr;
//static int FrameWidth = 0;
//static int FrameHeight = 0;
//static GLuint VAO = 0;
//static GLuint VBO = 0;
//static GLuint IndexTexture = 0;
//static GLuint PaletteTexture = 0;
//static GLuint ShaderProgram = 0;
//static GLuint ShaderProgram2 = 0;
//static GLuint PBO = 0;
//static GLint IndexTexLoc = -1;
//static GLint PaletteTexLoc = -1;

static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        return 0;
    }
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        internal_error(std::string("Shader compilation failed:\n") + infoLog);
    }
    return shader;
}

GLuint gl_shader_program(const char* vert, const char* frag) {
    GLuint vertexShader = compile_shader(GL_VERTEX_SHADER, vert);
    GLuint fragmentShader = compile_shader(GL_FRAGMENT_SHADER, frag);
    if (!vertexShader || !fragmentShader) {
        return -1;
    }

    GLuint ShaderProgram = glCreateProgram();
    glAttachShader(ShaderProgram, vertexShader);
    glAttachShader(ShaderProgram, fragmentShader);
    glLinkProgram(ShaderProgram);

    GLint success;
    glGetProgramiv(ShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(ShaderProgram, 512, nullptr, infoLog);
        internal_error(std::string("Shader linking failed:\n") + infoLog);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return ShaderProgram;
}


GLuint upload_pcx8(unsigned char* pixels, int width, int height) {
  GLuint tex_id;
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &tex_id);
  glBindTexture(GL_TEXTURE_2D, tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
               width, height, 0,
               GL_RED, GL_UNSIGNED_BYTE,
               pixels);
  return tex_id;
}

GLuint upload_rgba(unsigned char* pixels, int width, int height) {

  GLuint tex_id;
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &tex_id);
  glBindTexture(GL_TEXTURE_2D, tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
               width, height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);
  return tex_id;
}

GLuint upload_affine_texture(affine_pic* pic) {
  return upload_pcx8(pic->pixels_orig, pic->width, pic->height-2);
}


GLuint upload_pic8_texture(pic8* pic) {
  return upload_pcx8(pic->pixels, pic->get_width(), pic->get_height());
}

