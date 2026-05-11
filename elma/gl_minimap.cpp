
#include "EDITUJ.H"
#include "gl_canvas.h"
#include "gl_shaders.h"

GlManaged* MinimapRenderer = nullptr;
GlManaged* MinimapPresenter = nullptr;

void gl_init_minimap() {


    const char* vert = R"(
    #version 420 core
    uniform vec4 uFrustrum;
    layout (location = 0) in vec2 pos;

    void main() {
      float x = (pos.x-uFrustrum.x)/(uFrustrum.z-uFrustrum.x);
      float y = (-pos.y-uFrustrum.y)/(uFrustrum.w-uFrustrum.y);

      vec2 ndc;
      ndc.x = -1.0 + x * 2.0;
      ndc.y = -1.0 + y * 2.0;
      //ndc.x /=5.0;
      //ndc.y /=5.0;
    
      gl_Position = vec4(ndc, 0.0, 1.0);

    }
    )";

    const char* frag = R"(
    #version 410 core
    out vec4 FragColor;
    uniform vec4 color;

    void main() {
      FragColor = color;
    }
    )";

    MinimapRenderer = new GlManaged("minimapRender");
    MinimapRenderer->set_vertex_shader(vert);
    MinimapRenderer->set_fragment_shader(frag);
    MinimapRenderer->add_input_floats(2, GL_FALSE);
    MinimapRenderer->compile();

    std::vector<float> verts;
    for (auto v : GL_Canvas->sky) {
      for (auto f : v) { verts.push_back(f); }
    }
    int sky = verts.size() / 2;
    for (auto v : GL_Canvas->ground) {
      for (auto f : v) { verts.push_back(f); }
    }
    int ground = verts.size() / 2 - sky;
    MinimapRenderer->buffer_data(verts.size()>>1, verts.data(), GL_STATIC_DRAW);


// Assumes you already have:
// - A valid OpenGL context
// - Compiled shaders
// - VAO/VBO setup
// - A renderTexture GLuint to render into
// - A fullscreen quad or scene draw call

GLuint fbo = 0;
GLuint colorTex = 0;
GLint previousFbo = 0;
GLint previousViewport[4];

glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
glGetIntegerv(GL_VIEWPORT, previousViewport);

double x0, y0, x1, y1;
Ptop->get_boundaries(&x0, &y0, &x1, &y1, false);

const int width  = 1000;
const int height = 600;

// -----------------------------------------------------------------------------
// Create color texture
// -----------------------------------------------------------------------------
glGenTextures(1, &colorTex);
glBindTexture(GL_TEXTURE_2D, colorTex);

glTexImage2D(
    GL_TEXTURE_2D,
    0,
    GL_RGBA8,
    width,
    height,
    0,
    GL_RGBA,
    GL_UNSIGNED_BYTE,
    nullptr
);

glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

glGenFramebuffers(1, &fbo);
glBindFramebuffer(GL_FRAMEBUFFER, fbo);

// Attach color texture
glFramebufferTexture2D(
    GL_FRAMEBUFFER,
    GL_COLOR_ATTACHMENT0,
    GL_TEXTURE_2D,
    colorTex,
    0
);

// Check completeness
if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
{
    printf("we have framebuffer error");
    int* i = 0;
    *i = 1;
}


// -----------------------------------------------------------------------------
// Render to offscreen framebuffer
// -----------------------------------------------------------------------------
glBindFramebuffer(GL_FRAMEBUFFER, fbo);
glViewport(0, 0, width, height);
glClearColor(1.0f, 1.0f, 1.0f, 0.7f);
glClear(GL_COLOR_BUFFER_BIT);
glEnable(GL_BLEND);
// Replace destination alpha with source alpha
MinimapRenderer->use();
MinimapRenderer->uniform4f("uFrustrum", x0, y0, x1, y1);
MinimapRenderer->persist_uniform4f("color", 1.0f, 1.0f, 1.0f, 0.7f);
glBlendFunc(GL_ZERO, GL_ZERO);
MinimapRenderer->draw(0, sky);
glBlendFunc(GL_ONE, GL_ZERO);
MinimapRenderer->draw(sky, ground);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
glBindFramebuffer(GL_FRAMEBUFFER, previousFbo);
glDeleteFramebuffers(1, &fbo);
glViewport(
    previousViewport[0],
    previousViewport[1],
    previousViewport[2],
    previousViewport[3]
);



    const char* vert2 = R"(
    #version 420 core
    layout(std140, binding = 0) uniform GlobalData {
      vec4 uFrustrum;
      float PixelsToMetersAtLoad;
      float PixelsToMeters;
    };
    layout (location = 0) in vec2 pos;
    layout (location = 1) in vec2 texCoord;
    out vec2 fragTexCoord;

    void main() {
      fragTexCoord = texCoord;
      gl_Position = vec4(pos/2.0+vec2(.483, -.980), 0.0, 1.0);
    }
    )";

    const char* frag2 = R"(
    #version 410 core
    in vec2 fragTexCoord;
    out vec4 FragColor;
    uniform sampler2D Minimap;
    uniform vec4 boundary;
    uniform float zoom;
    uniform vec2 origin; 

    void main() {
      FragColor = texture(Minimap, fragTexCoord);
      //FragColor += vec4(.2);
    }
    )";

    MinimapPresenter = new GlManaged("minimap");
    MinimapPresenter->set_vertex_shader(vert2);
    MinimapPresenter->set_fragment_shader(frag2);
    MinimapPresenter->add_input_floats(2, GL_FALSE);
    MinimapPresenter->add_input_floats(2, GL_FALSE);
    MinimapPresenter->compile();
    MinimapPresenter->set_texture(GL_TEXTURE0, colorTex);
    MinimapPresenter->persist_uniform1i("Minimap", 0);
    MinimapPresenter->persist_uniform4f("boundary", x0, y0, x1, y1);

    float quadUnit[24] = {
      0, 0, 0, 0,
      1, 0, 1, 0,
      1, 1, 1, 1,
      0, 0, 0, 0,
      1, 1, 1, 1,
      0, 1, 0, 1
    };
    MinimapPresenter->buffer_data(6, quadUnit, GL_STATIC_DRAW);
  }

void gl_render_minimap(motorst* mot, valtozok* metadata) {
  MinimapPresenter->use();
  MinimapPresenter->uniform2f("origin", mot->body_r.x, mot->body_r.y);
  MinimapPresenter->uniform1f("zoom", 1);
  MinimapPresenter->draw();
}
