#include "gl_renderer.h"
#include "gl_common.h"
#include "gl_tas.h"
#include "gl_canvas.h"
#include "main.h"

#include <glad/glad.h>
#include <cstring>

void (*gl_render_callback)() = nullptr;


static const char* VertexShaderSource = R"(
#version 410 core
layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texCoord;
out vec2 fragTexCoord;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    fragTexCoord = texCoord;
}
)";

static const char* FragmentShaderSource = R"(
#version 410 core
in vec2 fragTexCoord;
out vec4 FragColor;
uniform sampler2D IndexTexture;
uniform sampler1D PaletteTexture;

void main() {
    float index = texture(IndexTexture, fragTexCoord).r;
    FragColor = texture(PaletteTexture, index);
}
)";

int FrameWidth = 0;
int FrameHeight = 0;

static SDL_GLContext GLContext = nullptr;
static GLuint VAO = 0;
static GLuint VBO = 0;
static GLuint IndexTexture = 0;
GLuint PaletteTexture = 0;
static GLuint ShaderProgram = 0;
static GLuint PBO = 0;
static GLint IndexTexLoc = -1;
static GLint PaletteTexLoc = -1;



static int init_shaders() {
    ShaderProgram = gl_shader_program(VertexShaderSource, FragmentShaderSource);
    if (ShaderProgram == -1) {
        return -1;
    }

    glUseProgram(ShaderProgram);
    IndexTexLoc = glGetUniformLocation(ShaderProgram, "IndexTexture");
    PaletteTexLoc = glGetUniformLocation(ShaderProgram, "PaletteTexture");
    glUniform1i(IndexTexLoc, 0);
    glUniform1i(PaletteTexLoc, 1);

    return 0;
}


static void setup_textures(int width, int height) {
    // Create index texture (R8)
    glGenTextures(1, &IndexTexture);
    glBindTexture(GL_TEXTURE_2D, IndexTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

    // Create palette texture (1D, 256 entries)
    glGenTextures(1, &PaletteTexture);
    glBindTexture(GL_TEXTURE_1D, PaletteTexture);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

static void setup_render_state() {
    glUseProgram(ShaderProgram);
    glBindVertexArray(VAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, IndexTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_1D, PaletteTexture);
}

static void setup_PBO(int pitch, int height) {
    glGenBuffers(1, &PBO);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, pitch * height, nullptr, GL_STREAM_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

static void setup_vertex_data() {
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    float vertices[] = {
        // positions  texCoords
        -1.0f, 1.0f,  0.0f, 0.0f, // top-left
        -1.0f, -1.0f, 0.0f, 1.0f, // bottom-left
        1.0f,  -1.0f, 1.0f, 1.0f, // bottom-right
        -1.0f, 1.0f,  0.0f, 0.0f, // top-left
        1.0f,  -1.0f, 1.0f, 1.0f, // bottom-right
        1.0f,  1.0f,  1.0f, 0.0f  // top-right
    };

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}

int gl_init(SDL_Window* sdl_window, int width, int height, int pitch) {
    FrameWidth = width;
    FrameHeight = height;

    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

    GLContext = SDL_GL_CreateContext(sdl_window);

    if (!GLContext) {
        internal_error(std::string("Failed to create OpenGL context:\n") + SDL_GetError());
    }


    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        internal_error("Failed to initialize GLAD");
    }

    const GLubyte* v = glGetString(GL_VERSION);
    printf("OpenGL %s\n", v);

    // Disable unnecessary GL features
    glDisable(GL_DEPTH_TEST);
    //glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DITHER);

    glViewport(0, 0, width, height);

    // Disable VSync
    SDL_GL_SetSwapInterval(0);
    setup_textures(width, height);

    return 0;
}

void gl_upload_frame(const unsigned char* indices, int pitch) {
    const unsigned long long buffer_size = pitch * FrameHeight;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
    void* ptr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, buffer_size,
                                 GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
    if (!ptr) {
        internal_error("Could not map PBO!");
    }

    memcpy(ptr, indices, buffer_size);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    glActiveTexture(GL_TEXTURE0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, FrameWidth, FrameHeight, GL_RED, GL_UNSIGNED_BYTE,
                    nullptr);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void gl_update_palette(const void* palette) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_1D, PaletteTexture);
    glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 256, GL_RGBA, GL_UNSIGNED_BYTE, palette);
}



void gl_present() {

    // Indexed image:
    //glDisable(GL_BLEND);
    //setup_render_state();
    //glDrawArrays(GL_TRIANGLES, 0, 6);

    //gl_canvas_render();

    if (gl_render_callback != nullptr) {
      IsRenderCallback = true;
      gl_render_callback();
      IsRenderCallback = false;
    }
}

int gl_resize(int width, int height, int pitch) {
    FrameWidth = width;
    FrameHeight = height;
    glViewport(0, 0, width, height);

    // Resize index texture
    //glActiveTexture(GL_TEXTURE0);
    //glBindTexture(GL_TEXTURE_2D, IndexTexture);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

    // Resize PBO
    //glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
    //glBufferData(GL_PIXEL_UNPACK_BUFFER, pitch * height, nullptr, GL_STREAM_DRAW);
    //glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return 0;
}

void gl_cleanup() {
    if (VBO) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (PBO) {
        glDeleteBuffers(1, &PBO);
        PBO = 0;
    }
    if (IndexTexture) {
        glDeleteTextures(1, &IndexTexture);
        IndexTexture = 0;
    }
    if (PaletteTexture) {
        glDeleteTextures(1, &PaletteTexture);
        PaletteTexture = 0;
    }
    if (VAO) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (ShaderProgram) {
        glDeleteProgram(ShaderProgram);
        ShaderProgram = 0;
    }
    if (GLContext) {
        SDL_GL_DeleteContext(GLContext);
        GLContext = nullptr;
    }
}
