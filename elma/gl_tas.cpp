
#include <cstdio>
#include <cstring>
#include "gl_common.h"
#include "gl_canvas.h"
#include "fonts.h"
#include "gl_shaders.h"
#include "main.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "include/stb_image.h"


#define MAX_CHARS 1024l


bool IsRenderCallback = false;

GLuint BoxShaderProgram = 0;
GLuint triangleVAO, triangleVBO;

GLuint FontShaderProgram = 0;
GLuint textVAO, textVBO;

GlRingBuffer* TextBuf = nullptr;


int gl_init_tas_shader() {

    const char* vert2 = R"(
    // vertex
    #version 410 core
    layout (location = 2) in vec2 position;
    uniform vec4 Pos;

    void main() {
      vec2 p = position * Pos.zw + Pos.xy;
      p = vec2(p.x * 2.0 - 1.0, 1.0 - p.y * 2.0);
      gl_Position = vec4(p, 0.0, 1.0);
    }
    )";

    const char* frag2 = R"(
    #version 410 core
    out vec4 FragColor;
    uniform vec4 Color;

    void main() {
        FragColor = Color;
    }
    )";


    if ((BoxShaderProgram = gl_shader_program(vert2, frag2)) == -1) {
        printf("failed to create BoxShaderProgram\n");
        return -1;
    }

    glGenVertexArrays(1, &triangleVAO);
    glGenBuffers(1, &triangleVBO);

    glBindVertexArray(triangleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, triangleVBO);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    float quad[] = { 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1 };
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, quad, GL_STATIC_DRAW);

    return 0;
}

static GLuint fontTexture;


void get_uv(char c, float &u0, float &v0, float &u1, float &v1) {

  if (c >= 'A' && c <= 'Z') {
    c -= 'A';
  } else if (c >= 'a' && c <= 'z') {
    c -= 'a'; c += 26;
  } else if (c >= '0' && c <= '9') {
    c -= '0'; c += 52;
  } else {
    std::string s = "+-=()[]{}<>/?:#%!?.,'\"@&$_";
    int pos = s.find(c);
    if (pos != std::string::npos) {
      c = 62 + pos;
    }
  }


    int x = c % 13;
    int y = c / 13;

    u0 = 1.0f / 13.0f * x++;
    v0 = 1.0f / 7.0f * y++;
    u1 = 1.0f / 13.0f * x;
    v1 = 1.0f / 7.0f * y;
}

static std::array<float, 4> TasText[MAX_CHARS * 6];

void draw_text(float x, float y, float w, float h, const char* text) {

  if (strlen(text) > MAX_CHARS) {
    char tmp[30];
    sprintf(tmp, "draw_text: max %li", MAX_CHARS);
    external_error(tmp);
  }

  size_t off = 0;

  for (int i = 0; !(i > 0 && !text[i]); i++) {

    char c = text[i];

    if (c != ' ') {
      float u0, v0, u1, v1;
      get_uv(c, u0, v0, u1, v1);

      //h = h / FrameHeight * FrameWidth;

      //               pos       uv
      TasText[off++] = { x,   y,   u0, v0};
      TasText[off++] = { x+w, y,   u1, v0};
      TasText[off++] = { x+w, y-h, u1, v1};
      TasText[off++] = { x,   y,   u0, v0};
      TasText[off++] = { x+w, y-h, u1, v1};
      TasText[off++] = { x,   y-h, u0, v1};
    }
    x += w;
  }

  TextBuf->push_data(off, TasText);
  glDrawArrays(GL_TRIANGLES, 0, off);
}

static int gl_init_tas_font() {

  glActiveTexture(GL_TEXTURE2);
  glGenTextures(1, &fontTexture);
  glBindTexture(GL_TEXTURE_2D, fontTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


  // Verified image loading correctly
  int FontWidth, FontHeight, channels;
  stbi_uc* pixels = stbi_load_from_memory(
      font_atlas_minogram_6x10_png, 
      font_atlas_minogram_6x10_png_len,
      &FontWidth, &FontHeight, &channels, 4
  );


  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
               FontWidth, FontHeight, 0,
               GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);

  stbi_image_free(pixels);

    const char* vert = R"(
    #version 410 core
    layout(location=0) in vec2 pos;
    layout(location=1) in vec2 uv;

    out vec2 vUV;

    void main() {
        gl_Position = vec4(pos, 0.0, 1.0);
        vUV = uv;
    }
    )";

    const char* frag = R"(
    #version 410 core
    in vec2 vUV;
    out vec4 FragColor;
    
    uniform sampler2D fontTex;
    uniform vec4 color;
    
    void main() {
        float a = texture(fontTex, vUV).a;
        FragColor = vec4(color.rgb, a * color.a);
    }
    )";

    if ((FontShaderProgram = gl_shader_program(vert, frag)) == -1) {
        printf("failed to create FontShaderProgram\n");
        return -1;
    }



    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);

    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);

    //glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24 * MAX_CHARS, nullptr, GL_STREAM_DRAW);
    TextBuf = new GlRingBuffer(GL_ARRAY_BUFFER, 4*sizeof(float), MAX_CHARS * 6);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));

    return 0;
}

int gl_init_tas() {
  gl_init_tas_shader();
  gl_init_tas_font();
  return 0;
}

static int char_width = 12;
static int char_height = 20;

void gl_render_box(float r, float g, float b, float a, int x, int y, int rows, int cols, int paddingX, int paddingY) {
  if (!IsRenderCallback) {
    external_error(std::string("gl_render_box: can only render in callback\n"));
  }

  int w = cols * char_width + paddingX * 2;
  int h = rows * char_height + paddingY * 2;
  if (cols) { w -= char_width / 6; }
  if (rows) { h -= char_height / 3; }

  if (x < 0) {
    x = FrameWidth - w + x;
  }

  float x0 = -1.0 + (((float)x) / FrameWidth * 2.0);
  float y0 = 1.0 - (((float)y) / FrameHeight * 2.0);
  float x1 = -1.0 + (((float)x+w) / FrameWidth * 2.0);
  float y1 = 1.0 - (((float)y+h) / FrameHeight * 2.0);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUseProgram(BoxShaderProgram);
  glUniform4f(glGetUniformLocation(BoxShaderProgram, "Color"), r, g, b, a);
  glUniform4f(
    glGetUniformLocation(BoxShaderProgram, "Pos"),
    (float)x / FrameWidth, (float)y / FrameHeight, (float)w / FrameWidth, (float)h / FrameHeight
  );

  glBindVertexArray(triangleVAO);

  glDrawArrays(GL_TRIANGLES, 0, 6);
}

void gl_render_text(float r, float g, float b, float a, int x, int y, int colOff, int rowOff, const char* text) {
  if (!IsRenderCallback) {
    external_error(std::string("gl_render_text: can only render in callback\n"));
  }
  glUseProgram(FontShaderProgram);
  glUniform4f(glGetUniformLocation(FontShaderProgram, "color"), r, g, b, a);
  glUniform1i(glGetUniformLocation(FontShaderProgram, "fontTex"), 2);

  glBindVertexArray(textVAO);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, fontTexture);

  if (x < 0) {
    int w = char_width * strlen(text);
    x = FrameWidth - w + x;
    x -= colOff * char_width;
  } else {
    x += colOff * char_width;
  }
  y = y + rowOff * char_height;
  float fx = -1.0 + (((float)x) / FrameWidth * 2.0);
  float fy = 1.0 - (((float)y) / FrameHeight * 2.0);

  draw_text(fx, fy, char_width*2.0f / FrameWidth, char_height*2.0f / FrameHeight, text);
}


