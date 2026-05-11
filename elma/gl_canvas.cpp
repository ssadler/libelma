
#include <cstdio>
#include <cstring>
#include <vector>

#include "EDITUJ.H"
#include "pic8.h"
#include "lgr.h"
#include "physics_init.h"
#include "level.h"
#include "polygon.h"
#include "gl_common.h"
#include "gl_canvas.h"
#include "gl_shaders.h"
#include "earcut.hpp"
#include "simulation.h"
#include "state.h"

#include <algorithm>


namespace mapbox {
namespace util {
template <> struct nth<0, vect2> { inline static auto get(const vect2 &t) { return t.x; }; };
template <> struct nth<1, vect2> { inline static auto get(const vect2 &t) { return t.y; }; };
} // namespace util
} // namespace mapbox

GLCanvas* GL_Canvas = nullptr;


void mergeAndSort(     
    const std::vector<std::vector<float>>& ground,
    const std::vector<std::vector<float>>& sky,
    std::vector<float>& verts_out,
    std::vector<unsigned int>& offsets_out
);


GLCanvas::GLCanvas() {

    for (int p = 0; p < MAX_POLYGONS; p++) {
        polygon* poly = Ptop->polygons[p];
        if (!poly) {
            break;
        }
        if (poly->is_grass) { continue; }

        bool is_sky = !poly->is_grass && poly->is_clockwise();

        std::vector<vect2> vpoly;

        vpoly.reserve(poly->vertex_count);
        for (auto i=0; i<poly->vertex_count; i++) {
          vpoly.push_back(poly->vertices[i]);
        }

        std::vector<std::vector<vect2>> points;
        points.push_back(vpoly);
        points.emplace_back();
        std::vector<uint32_t> r = mapbox::earcut<uint32_t>(points);
        std::vector<float> f;

        for (auto i : r) {
          auto v = poly->vertices[i];
          f.push_back(v.x);
          f.push_back(v.y);
        }

        if (is_sky) {
          sky.push_back(f);
          sky_n_verts += f.size() / 2;
        } else {
          ground.push_back(f);
          ground_n_verts += f.size() / 2;
        }
    }

    mergeAndSort(ground, sky, merged_verts, merged_offsets);
}

void GLCanvas::setup() {
  delete GL_Canvas;
  GL_Canvas = new GLCanvas;

  gl_canvas_init();
  gl_load_sprites();
  gl_init_grass();
  gl_init_tas();
}



void gl_canvas_render();
void gl_canvas_render_combined(int ground_mask, int sky_mask);

static GlRingBuffer* UBOBuffer;
static canvas_ubo0 CanvasUBO;
bool init = false;

void GLCanvas::render(double t, motorst* pmot, valtozok* metadata, vect2 corner, int x1, int y1, int x2, int y2, std::vector<Simulation*>& shadows) {

  if (!init) {
    init = true;
    UBOBuffer = new GlRingBuffer(GL_UNIFORM_BUFFER, sizeof(CanvasUBO), 1);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, UBOBuffer->vbo); 
  }


  { // Set shader globals

    CanvasUBO.pixels_to_meters = PixelsToMeters;

    CanvasUBO.frustrum[0] = corner.x;
    CanvasUBO.frustrum[1] = corner.y;
    CanvasUBO.frustrum[2] = corner.x + (x2-x1) / MetersToPixels;
    CanvasUBO.frustrum[3] = corner.y + (y2-y1) / MetersToPixels;

    // quantize
    for (auto i=0; i<4; i++) {
      CanvasUBO.frustrum[i] = floor(CanvasUBO.frustrum[i] * MetersToPixels) * PixelsToMeters;
    }

    UBOBuffer->push_data(1, &CanvasUBO);
  }

//glEnable( GL_LINE_SMOOTH );
//glEnable( GL_POLYGON_SMOOTH );
//glHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
//glHint( GL_POLYGON_SMOOTH_HINT, GL_NICEST );

  auto quality = State->high_quality;

  glDisable(GL_DEPTH_TEST);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


  glEnable(GL_STENCIL_TEST);
  glClearStencil(0);
  glClear(GL_STENCIL_BUFFER_BIT);

  // Enable stencil write
  glStencilMask(0xFF);
  glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);

  // Render bg and write 1
  glStencilFunc(GL_ALWAYS, 1, 0xFF);                    

  // render terrain verts in order
  gl_canvas_render_combined(
      1, // ground mask
      0  // sky mask
  );

  // stop write stencil
  glStencilMask(0x00);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  if (quality == 2) {
    // Render sprites where stencil == 0
    glStencilFunc(GL_EQUAL, 0, 0xFF);                      
    gl_render_sprites(CanvasUBO.frustrum, Clipping::Sky);
  }

  if (quality) {
    // Render grass where stencil == 1
    glStencilFunc(GL_EQUAL, 1, 0xFF);                      
    gl_render_grass(CanvasUBO.frustrum);
  }

  glDisable(GL_STENCIL_TEST);

  if (quality == 2) {
      gl_render_sprites(CanvasUBO.frustrum, Clipping::Unclipped);
  }

  // Render kuskis
  gl_render_objects(CanvasUBO.frustrum, t);
  for (auto shadow : shadows) {
    gl_render_kuski(CanvasUBO.frustrum, &shadow->motor, &shadow->valt, true);
  }
  gl_render_kuski(CanvasUBO.frustrum, pmot, metadata, false);

  //gl_render_extra(CanvasUBO.frustrum, t);

  gl_render_minimap(pmot, metadata);

  init = true;
}



static GLuint TexForeground = 0, TexBackground = 0;
static GlManaged* ShaderBack = nullptr;
static GlManaged* ShaderSky = nullptr;
static GlManaged* ShaderFront = nullptr;



int gl_canvas_init() {

    CanvasUBO.pixels_to_meters_at_load = PixelsToMeters;

    if (TexForeground) {
      glDeleteTextures(1, &TexForeground);
      glDeleteTextures(1, &TexBackground);
    }

    TexForeground = upload_pic8_texture(Lgr->foreground);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    TexBackground = upload_pic8_texture(Lgr->background);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);



    const char* vert = R"(
    #version 420 core
    layout(std140, binding = 0) uniform GlobalData {
      vec4 uFrustrum;
    };
    layout(location=0) in vec2 pos;
    out vec2 fragTexCoord;
    uniform vec2 texSize;
    
    void main() {

    
      float x = (pos.x-uFrustrum.x)/(uFrustrum.z-uFrustrum.x);
      float y = (-pos.y-uFrustrum.y)/(uFrustrum.w-uFrustrum.y);

      vec2 ndc;
      ndc.x = -1.0 + x * 2.0;
      ndc.y = -1.0 + y * 2.0;
    
      gl_Position = vec4(ndc, 0.0, 1.0);
      fragTexCoord = pos / texSize;
    }
    )";

    const char* frag = R"(
    #version 410 core
    in vec2 fragTexCoord;
    in vec2 rr;
    out vec4 FragColor;
    uniform sampler2D IndexTexture;
    uniform sampler1D PaletteTexture;

    void main() {
        float index = texture(IndexTexture, fragTexCoord).r;
        FragColor = texture(PaletteTexture, index);
        //if (index == texture(IndexTexture, vec2(0.0)).r) {
        //  FragColor = vec4(0.0);
        //}
    }
    )";


  delete ShaderBack;
  delete ShaderFront;
  delete ShaderSky;


  ShaderBack = new GlManaged("polygons");
  ShaderBack->set_vertex_shader(vert);
  ShaderBack->set_fragment_shader(frag);
  ShaderBack->add_input_floats(2, GL_FALSE);
  ShaderBack->compile();
  ShaderBack->persist_uniform1i("IndexTexture", 0);
  ShaderBack->persist_uniform1i("PaletteTexture", 1);
  ShaderBack->set_texture(GL_TEXTURE1, PaletteTexture);

  // take clones
  ShaderFront = ShaderBack->clone();
  ShaderSky = ShaderBack->clone();

  // shader back specific
  ShaderBack->enable_ring(6);
  ShaderBack->set_texture(GL_TEXTURE0, TexForeground);
  ShaderBack->persist_uniform2f(
    "texSize",
    Lgr->foreground->get_width() / MetersToPixels,
    Lgr->foreground->get_height() / MetersToPixels
  );


  auto& verts = GL_Canvas->merged_verts;

  ShaderFront->buffer_data(verts.size()>>1, verts.data(), GL_STATIC_DRAW);
  ShaderFront->set_texture(GL_TEXTURE0, TexForeground);
  ShaderFront->persist_uniform2f(
    "texSize",
    Lgr->foreground->get_width() / MetersToPixels,
    Lgr->foreground->get_height() / MetersToPixels
  );

  ShaderSky->buffer_data(verts.size()>>1, verts.data(), GL_STATIC_DRAW);
  ShaderSky->set_texture(GL_TEXTURE0, TexBackground);
  ShaderSky->persist_uniform2f(
    "texSize",
    Lgr->background->get_width() / MetersToPixels,
    Lgr->background->get_height() / MetersToPixels
  );


  gl_init_kuski();
  gl_init_sprite_system();
  gl_init_minimap();

  return gl_init_objects();
}



void gl_canvas_render_combined(int ground_mask, int sky_mask) {

  glStencilFunc(GL_ALWAYS, ground_mask, 0xFF);                    

  float quad[] = {
     CanvasUBO.frustrum[0], -CanvasUBO.frustrum[1],
     CanvasUBO.frustrum[2], -CanvasUBO.frustrum[1],
     CanvasUBO.frustrum[2], -CanvasUBO.frustrum[3],
     CanvasUBO.frustrum[0], -CanvasUBO.frustrum[1],
     CanvasUBO.frustrum[2], -CanvasUBO.frustrum[3],
     CanvasUBO.frustrum[0], -CanvasUBO.frustrum[3]
  };

  ShaderBack->use();
  ShaderBack->push_data(6, quad);
  ShaderBack->draw(0, 6);



  int offset = 0;

  for (auto n : GL_Canvas->merged_offsets) {

    auto mask = ground_mask;
    auto shader = ShaderFront;

    if (n & 0x80000000) {
      shader = ShaderSky;
      mask = sky_mask;
      n &= 0x7fffffff;
    }

    glStencilFunc(GL_ALWAYS, mask, 0xFF);                    
    shader->use();
    shader->draw(offset, n);
    offset += n;
  }
}



struct Vec2 {
    float x, y;
};

static float triArea(const Vec2& a, const Vec2& b, const Vec2& c)
{
    return 0.5f * std::abs(
        (b.x - a.x) * (c.y - a.y) -
        (b.y - a.y) * (c.x - a.x)
    );
}

static float polygonAreaFromTriangles(const std::vector<float>& poly)
{
    float area = 0.0f;
    auto size = poly.size()-1;

    // 6 floats per triangle
    for (size_t i = 0; i + 5 < size; i += 6)
    {
        Vec2 a{poly[i],     poly[i + 1]};
        Vec2 b{poly[i + 2], poly[i + 3]};
        Vec2 c{poly[i + 4], poly[i + 5]};
        area += triArea(a, b, c);
    }

    return area;
}


// render 0..3


void mergeAndSort(
  const std::vector<std::vector<float>>& ground,
  const std::vector<std::vector<float>>& sky,
  std::vector<float>& verts_out,
  std::vector<unsigned int>& offsets_out
) {
    std::vector<std::vector<float>> all;
    all.reserve(ground.size() + sky.size());

    all.insert(all.end(), ground.begin(), ground.end());
    all.insert(all.end(), sky.begin(), sky.end());
    for (int i=0; i<all.size(); i++) {
      all[i].push_back(i < ground.size() ? i : i+1000000);
    }

    std::sort(all.begin(), all.end(),
        [](const std::vector<float>& a, const std::vector<float>& b)
        {
            return polygonAreaFromTriangles(a) > polygonAreaFromTriangles(b);
        });

    int last_is_sky = -1;
    int pushed = 0;

    int i = 0;
    std::vector<float>* v = nullptr;

    for (; i<all.size(); i++) {
      v = &all[i];

      bool is_sky = v->back() >= 1000000;
      if (last_is_sky == -1) {
        last_is_sky = is_sky;
      } else if (last_is_sky != is_sky) {
        offsets_out.push_back(pushed | (last_is_sky*0x80000000));
        last_is_sky = is_sky;
        pushed = 0;
      }

      verts_out.insert(verts_out.end(), v->begin(), v->end()-1);
      pushed += v->size()>>1;
    }
    
    if (pushed > 0) {
      offsets_out.push_back(pushed | (last_is_sky*0x80000000));
      verts_out.insert(verts_out.end(), v->begin(), v->end()-1);
    }
}
