
#include "EDITUJ.H"
#include "lgr.h"
#include "eol_settings.h"
#include "grass.h"
#include "main.h"
#include "level.h"
#include "physics_init.h"
#include "pic8.h"
#include "polygon.h"
#include "canvas.h"
#include "gl_canvas.h"
#include "gl_common.h"
#include "gl_shaders.h"
#include "segments.h"
#include <map>
#include <array>
#include <cstring>
#include <memory>


#define GRASS_PATCH_W 300
#define GRASS_PATCH_H 50
#define GRASS_PATCH_SIZE (GRASS_PATCH_W * GRASS_PATCH_H)
#define GRASS_TEX_NUM_PATCHES (GL_MAX_TEXTURE_SIZE / GRASS_PATCH_H)
#define GRASS_TEX_HEIGHT (GRASS_TEX_NUM_PATCHES * GRASS_PATCH_H)
//#define GRASS_TEX_NUM_PATCHES (GL_MAX_TEXTURE_SIZE - (GL_MAX_TEXTURE_SIZE % GRASS_PATCH_H))

//uint64_t HashInit = 14695981039346656037ull;
//uint64_t qupdown_hash = HashInit;
//void updateHash(uint64_t* hash, int c) {
//  *hash ^= c;
//  *hash *= 1099511628211ull;
//}

int iiii = 0;
using ulong = unsigned long;

struct GrassPoly {
  GLuint tex;
  GLuint vao;
  GLuint vbo;
  std::vector<float> verts;
};
static std::map<int, GrassPoly> GrassPolys;
static std::map<ulong, std::array<unsigned char, GRASS_PATCH_W * GRASS_PATCH_H>> GrassPatches;
//static size_t GrassNumPatchVerts;
static std::vector<GlTexture> TexPatches;

std::unique_ptr<GlManaged> GrassPatchesShader;
std::vector<GlManaged> PatchShaders;
std::vector<std::unique_ptr<GlManaged>> GrassPolysShader = {};

void draw_grass_polygons(vect2 origin);

void gl_init_grass() {

  if (GrassPatchesShader == nullptr) {
    const char* vert = R"(
    #version 420 core
    layout(std140, binding = 0) uniform GlobalData { vec4 uFrustrum; };
    layout (location = 0) in vec2 pos;
    layout (location = 1) in vec2 texCoord;
    out vec2 fragTexCoord;
    //uniform vec4 uFrustrum;

    void main() {

      float x = (pos.x-uFrustrum.x)/(uFrustrum.z-uFrustrum.x);
      float y = (pos.y-uFrustrum.y)/(uFrustrum.w-uFrustrum.y);

      gl_Position = vec4(-1.0 + x * 2.0, -1.0 + y * 2.0, 0.0, 1.0);

      fragTexCoord = texCoord;
    }
    )";

    const char* frag = R"(
    #version 410 core
    in vec2 fragTexCoord;
    out vec4 FragColor;
    uniform sampler2D IndexTexture;
    uniform sampler1D PaletteTexture;

    void main() {
      float index = texture(IndexTexture, fragTexCoord).r;
      if (true || index > 1.0/256.0) {
        FragColor = texture(PaletteTexture, index);
        if (FragColor.rgb == vec3(0.0, 0.0, 0.0)) {
          FragColor = vec4(0.0);
        }
        //float o = FragColor.r + FragColor.g + FragColor.b;
        //FragColor.rgb = vec3(o / 1.05);
      } else {
        FragColor = vec4(0.0);
      }
    }
    )";

    GrassPatchesShader = std::make_unique<GlManaged>("grass");
    GrassPatchesShader->set_fragment_shader(frag);
    GrassPatchesShader->set_vertex_shader(vert);
    GrassPatchesShader->add_input_floats(2, GL_FALSE);
    GrassPatchesShader->add_input_floats(2, GL_FALSE);
    GrassPatchesShader->compile();

    GrassPatchesShader->persist_uniform1i("PatchTexture", 0);
    GrassPatchesShader->persist_uniform1i("PaletteTexture", 1);
    GrassPatchesShader->set_texture(GL_TEXTURE1, PaletteTexture);
  }


  GrassPatches.clear();
  GrassPolysShader.clear();
  TexPatches.clear();


  vect2 origin = CanvasFront->origin;
  //float om2p = MetersToPixels;
  //MetersToPixels = 100;
  //PixelsToMeters = 1 / MetersToPixels;
  draw_grass_polygons(origin);
  //MetersToPixels = om2p;
  //PixelsToMeters = 1 / MetersToPixels;

  { // Patches

    int n_patches = GrassPatches.size();

    std::vector<std::array<float, 4>> verts;
    verts.reserve(GrassPatches.size() * 6);

    int num_textures = (GrassPatches.size() * GRASS_PATCH_H + GRASS_TEX_HEIGHT - 1) / GRASS_TEX_HEIGHT;

    std::vector<unsigned char> pixels;
    pixels.reserve(num_textures * GRASS_PATCH_SIZE * GRASS_TEX_NUM_PATCHES);

    int patch_num = 0;

    float w_meters = GRASS_PATCH_W / MetersToPixels;
    float h_meters = GRASS_PATCH_H / MetersToPixels;

    for (auto const &[k, v] : GrassPatches) {

      std::memcpy(&pixels[patch_num * GRASS_PATCH_SIZE], v.data(), GRASS_PATCH_SIZE);

      float x0 = (k >> 32) * w_meters + origin.x;
      float y0 = (k & 0xffffffff) * h_meters + origin.y;
      float x1 = x0 + w_meters;
      float y1 = y0 + h_meters;

      int np = GRASS_TEX_NUM_PATCHES;
      float v0 = (1.0f / np) * (patch_num % np);
      float v1 = v0 + 1.0f / np;

      verts.push_back({ x0, y0, 0, v0 });
      verts.push_back({ x1, y0, 1, v0 });
      verts.push_back({ x1, y1, 1, v1 });
      verts.push_back({ x0, y0, 0, v0 });
      verts.push_back({ x1, y1, 1, v1 });
      verts.push_back({ x0, y1, 0, v1 });

      patch_num++;
    }

    GrassPatchesShader->buffer_data(6 * GrassPatches.size(), verts.data(), GL_STATIC_DRAW);

    // need to upload in chunks due to GL_MAX_TEXTURE_SIZE
    for (int i=0; i<num_textures; i++) {
      auto id = upload_pcx8(
        &pixels[i * GRASS_TEX_HEIGHT * GRASS_PATCH_W],
        GRASS_PATCH_W,
        GRASS_TEX_HEIGHT
      );
      TexPatches.emplace_back(id);
    }


    //TexPatches = GlTexture(upload_pcx8(pixels.data(), GRASS_PATCH_W, GRASS_PATCH_H * n_patches));
  }


  iiii = 0;

  { // Polys
    for (auto &[k, v] : GrassPolys) {
      GlManaged* s = GrassPatchesShader->clone();
      s->buffer_data(v.verts.size(), v.verts.data(), GL_STATIC_DRAW);
      GrassPolysShader.push_back(std::unique_ptr<GlManaged>(s));
    }
  }
}






void gl_render_grass(float* frustrum) {

  // Polys

  for (int i=0; i<GrassPolysShader.size(); i++) {
    auto v = &GrassPolys[i];
    glBindTexture(GL_TEXTURE_2D, v->tex);
    GrassPolysShader[i]->draw(0, v->verts.size());
  }

  // Patches

  int n_patches = GrassPatches.size();
  for (int i=0; i<TexPatches.size(); i++) {
    glBindTextureUnit(0, *TexPatches[i]);
    int np = std::min(n_patches - i * GRASS_TEX_NUM_PATCHES, GRASS_TEX_NUM_PATCHES);
    GrassPatchesShader->draw(6 * GRASS_TEX_NUM_PATCHES * i, 6 * np);
    np -= GRASS_TEX_NUM_PATCHES;
  }
}



// *BITROT*
void draw_grass_poly(vect2 origin, canvas_pixels source, int x, int y, int width, int height) {

  int id = source.to_texture();
  texture* tex = &Lgr->textures[id];

  GrassPoly* r = &GrassPolys[id];
  if (r->tex == 0) {
    r->tex = upload_pcx8(tex->pic->pixels, tex->pic->get_width(), tex->pic->get_height());
  }

  float w_meters = width * PixelsToMeters;
  float h_meters = height * PixelsToMeters;

  float x0 = x * PixelsToMeters + origin.x;
  float y0 = y * PixelsToMeters + origin.y;
  float x1 = x0 + w_meters;
  float y1 = y0 + h_meters;

  float v0 = 0; // 1.0f / n_patches * patch_num;
  float v1 = 1; // v0 + 1.0f / n_patches;

  float quad[] = {
    x0, y0, 0, v0,
    x1, y0, 1, v0,
    x1, y1, 1, v1,
    x0, y0, 0, v0,
    x1, y1, 1, v1,
    x0, y1, 0, v1
  };

  r->verts.insert(r->verts.end(), std::begin(quad), std::end(quad));
}

void draw_grass_pixels(canvas_pixels source, int source_dist, int x_left, int x_right, int y, Clipping clipping) {

  if (x_left < 0 || x_right < 0 || y < 0) {
    internal_error("gl_grass draw_grass_pixels: negative offset?");
  }

  texture* tex = nullptr;
  unsigned char* ptr;

  if (source.is_texture()) {
    tex = &Lgr->textures[source.to_texture()];
    ptr = tex->pic->get_row(y % tex->pic->get_height());
  } else {
    ptr = source.to_pointer();
  }

  int x = x_left;
  while (x <= x_right) {
    ulong key = x / GRASS_PATCH_W;
    //if (key == 8) return;
    key <<= 32;
    key += y / GRASS_PATCH_H;

    int mx = x % GRASS_PATCH_W;
    int my = y % GRASS_PATCH_H;

    int p;
    int w = GRASS_PATCH_W - mx; // dont write past grass patch

    if (tex) {
      p = x % tex->pic->get_width();
      int tex_w = tex->pic->get_width();
      w = std::min(w, tex_w - (x % tex_w)); // dont write past tex
    } else {
      p = x - x_left;
    }

    w = std::min(w, x_right - x + 1); // dont write past right
    std::memcpy(&GrassPatches[key][mx + my * GRASS_PATCH_W], &ptr[p], w);
    x += w;
  }
}






constexpr int GRASS_DISTANCE = 600;

static int consecutive_transparent_pixels(unsigned char* pic_row, int x, int width,
                                          unsigned char transparency) {
    int count = 0;
    for (int i = x; i < width; i++) {
        if (pic_row[i] != transparency) {
            return count;
        }
        count++;
    }
    return count;
}

static int consecutive_solid_pixels(unsigned char* pic_row, int x, int width,
                                    unsigned char transparency) {
    int count = 0;
    for (int i = x; i < width; i++) {
        if (pic_row[i] == transparency) {
            return count;
        }
        count++;
    }
    return count;
}


void draw_qgrass_texture(updown& qupdown, int x, int y, int qgrass_margin) {
    // Grab the QGRASS texture
    int distance = GRASS_DISTANCE;
    int texture_index = Lgr->get_texture_index("qgrass");
    if (texture_index < 0) {
        internal_error("draw_qgrass_texture texture_index < 0");
    }

    if (qupdown.msk.data == nullptr) {
        return;
    }

    int width = qupdown.pic->get_width();
    int height = qupdown.pic->get_height();

    //draw_grass_poly(canvas_pixels::texture(texture_index), x, y + 40, width, height);

    // Render mask (the green area)
    mask_element* data = qupdown.msk.data;
    int mask_height = qupdown.msk.height;
    int mask_y = y + height - 1;
    for (int i = 0; i < mask_height; i++) {
        int j = 0;
        while (true) {
            if (data->type == MaskEncoding::EndOfLine) {
                data++;
                break;
            }

            if (data->type == MaskEncoding::Solid) {
                draw_grass_pixels(canvas_pixels::texture(texture_index), distance, x + j,
                            x + j + data->length - 1, mask_y, Clipping::Ground);
            }

            j += data->length;
            data++;
        }

        mask_y--;
    }


    for (int i = 0; i < qgrass_margin; i++) {
        draw_grass_pixels(canvas_pixels::texture(texture_index), distance, x, x + width - 1,
                    y + height + i, Clipping::Ground);
    }
}

void draw_qupdown(updown& qupdown, int x, int y, int qupdown_margin, int qgrass_margin) {

    int distance = GRASS_DISTANCE;

    // Grab the QUP/QDOWN image
    int width = qupdown.pic->get_width();
    int height = qupdown.pic->get_height();
    unsigned char transparency = qupdown.pic->gpixel(0, 0);

    // Slide image down by top of image buffer
    y -= qupdown_margin;
    if (!qupdown.is_up) {
        // Slide image down again by the slope
        y += qupdown.slope;
    }

    // Check out of bounds
    //int y2 = y + height + qgrass_margin;
    //int x2 = x + width;
    //if (x < CANVAS_SAFETY_LEFT || x2 >= pixel_width - CANVAS_SAFETY_RIGHT ||
    //    y < CANVAS_SAFETY_TOP || y2 >= pixel_height - CANVAS_SAFETY_BOTTOM) {
    //    return;
    //}
    //


    // Draw upside down
    for (int i = 0; i < height; i++) {
        int j = 0;
        unsigned char* sor = qupdown.pic->get_row(height - 1 - i);

        // cheat (could send rect here)
        //draw_grass_pixels(canvas_pixels::pointer(&sor[0]), distance, x, x + width, y + i, Clipping::Ground);
        
        while (j < width) {
            j += consecutive_transparent_pixels(sor, j, width, transparency);
            if (j >= width) {
                break;
            }
            int count = consecutive_solid_pixels(sor, j, width, transparency);
            if (count <= 0) {
                internal_error("draw_qupdown count <= 0");
            }
            draw_grass_pixels(canvas_pixels::pointer(&sor[j]), distance, x + j, x + j + count - 1, y + i,
                        Clipping::Ground);
            j += count;
        }
    }

    // Draw the QGRASS image on top of the QUP/QDOWN
    draw_qgrass_texture(qupdown, x, y, qgrass_margin);
}

void draw_grass_polygon(grass* gr, int* heightmap, int heightmap_length, int x0,
                                int qupdown_margin, int qgrass_margin) {
    if (heightmap_length < 1) {
        internal_error("draw_grass_polygon should always have a length of at least 1!");
    }
    int x = x0;
    int y = heightmap[0];
    while (true) {
        if (x >= x0 + heightmap_length) {
            return;
        }

        // Let's pick the best matching QUP/QDOWN
        int best_score = INT_MAX;
        updown* best_qupdown = nullptr;
        int best_slope = 0;
        for (updown& qupdown : gr->elements) {
            // Check each image and choose the one with the smallest y-offset from desired
            // height
            int target_x = x + qupdown.pic->get_width();
            int target_y;
            if (target_x >= x0 + heightmap_length) {
                target_y = heightmap[heightmap_length - 1];
            } else {
                target_y = heightmap[target_x - x0];
            }

            int score = abs(y + qupdown.slope - target_y);
            if (score < best_score) {
                best_score = score;
                best_qupdown = &qupdown;
                best_slope = qupdown.slope;
            }
        }

        // Draw the best matching QUP/QDOWN
        if (!best_qupdown) {
            internal_error("draw_grass_polygon no qupdown identified!");
        }

        draw_qupdown(*best_qupdown, x, y, qupdown_margin, qgrass_margin);
        x += best_qupdown->pic->get_width();
        y += best_slope;
    }
}


void draw_grass_polygons(vect2 origin) {

    // Only if the lgr has the right assets
    if (!Lgr->has_grass) {
        return;
    }

    // Calculate zoom-adjusted grass margin
    double zoom = EolSettings->zoom();
    int qupdown_margin = (int)(QUPDOWN_MARGIN * (EolSettings->zoom_grass() ? zoom : 1.0));
    int qgrass_margin = (int)(QGRASS_MARGIN * zoom) - qupdown_margin;

    constexpr int HEIGHTMAP_LENGTH = 10000;
    int max_heightmap_length = zoom * HEIGHTMAP_LENGTH;
    int* heightmap = new int[max_heightmap_length];

    for (int i = 0; i < MAX_POLYGONS; i++) {
        // Make sure we are drawing a grass polygon onto the ground
        polygon* poly = Ptop->polygons[i];
        if (!poly) {
            break;
        }
        if (!poly->is_grass) {
            continue;
        }

        grass* gr = Lgr->grass_pics;

        // Calculate the grass heightmap
        int heightmap_length = 0;
        int x0 = 0;
        if (!create_grass_polygon_heightmap(poly, heightmap, &heightmap_length, &x0,
                                            max_heightmap_length, &origin)) {
            continue;
        }
        if (heightmap_length > max_heightmap_length) {
            internal_error("draw_grass_polygons heightmap_length > max_heightmap_length");
        }

        // Draw the grass polygon
        draw_grass_polygon(gr, heightmap, heightmap_length, x0, qupdown_margin, qgrass_margin);
    }

    delete[] heightmap;
}
