
#include "EDITUJ.H"
#include "gl_canvas.h"
#include "gl_common.h"
#include "lgr.h"
#include "level.h"
#include "physics_init.h"
#include "main.h"
#include <glad/glad.h>
#include <cmath>
#include <cstring>


static constexpr int MASK_ROWS = 20;

static GLuint TexPictures[MAX_PICTURES] = {};
static size_t NumPictures = 0;
static GLuint TexMasks[MAX_MASKS] = {};
static size_t NumMasks = 0;

static size_t SpriteVertsOffset[MAX_SPRITES] = {};

static GLuint SpriteShaderProgram;
static GLuint SpriteVAO, SpriteVBO;
static GLuint SpriteUTransform = 0;

struct SpriteVertex {
  float x, y;     // 8 bytes
  float u, v;     // 8 bytes
  short mask_off; // 2 bytes
  short pad;      // padding
};
static_assert(sizeof(SpriteVertex) == 20);

struct RenderSprite {
  int sprite_id;
  int mask_id;
  int picture_id;
  int vert_off;
  int n_verts;
};
static std::vector<RenderSprite> Renderable = {};

static GLuint upload_picture(picture* p);
static GLuint upload_mask(mask* m);
static int get_mask_offset(mask* msk, int row);

static lgrfile* LastLgr = nullptr;



int gl_init_sprite_system() {

  /*
   * Shaders and GL objects
   */


  const char* vert = R"(
  #version 420 core
  layout(std140, binding = 0) uniform GlobalData { vec4 uFrustrum; };
  layout (location = 0) in vec2 pos;
  layout (location = 1) in vec2 texCoord;
  layout (location = 2) in int maskOff;
  out vec2 fragTexCoord;
  flat out int texMaskOff;

  void main() {

    float x = (pos.x-uFrustrum.x)/(uFrustrum.z-uFrustrum.x);
    float y = (pos.y-uFrustrum.y)/(uFrustrum.w-uFrustrum.y);

    gl_Position = vec4(-1.0 + x * 2.0, -1.0 + y * 2.0, 0.0, 1.0);

    fragTexCoord = texCoord;
    texMaskOff = maskOff;
  }
  )";

  const char* frag = R"(
  #version 410 core
  in vec2 fragTexCoord;
  out vec4 FragColor;
  uniform sampler2D IndexTexture;
  uniform sampler1D PaletteTexture;
  uniform sampler1D MaskTexture;
  uniform int tColor;


  void main() {
    FragColor = texture(IndexTexture, fragTexCoord);
    
    if (FragColor.a > 0.0) {

      //vec4 c = texture(IndexTexture, vec2(fragTexCoord.x, fragTexCoord.y - .05));
      //if (c.a == 0.0) {
      //  FragColor += vec4(.9);
      //}
      //float l = length(fragTexCoord);
      //float fl = floor(l);
      //float fc = fract(l);
      //float r = fract(sin(l * 43758.5453123));
      //float r1 = fract(sin((fl+1.0) * 43758.5453123));
      //float nn = mix(r, r1, fc);
      //FragColor.rgb += vec3(abs(nn));
    }
  }
  )";


  if ((SpriteShaderProgram = gl_shader_program(vert, frag)) == -1) {
      printf("failed to create SpriteShaderProgram\n");
      return -1;
  }

  SpriteUTransform = glGetUniformLocation(SpriteShaderProgram, "uTransform");

  glGenVertexArrays(1, &SpriteVAO);
  glGenBuffers(1, &SpriteVBO);

  glBindVertexArray(SpriteVAO);
  glBindBuffer(GL_ARRAY_BUFFER, SpriteVBO);


  size_t stride = sizeof(SpriteVertex);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);

  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2*sizeof(float)));

  glEnableVertexAttribArray(2);
  glVertexAttribIPointer(2, 1, GL_SHORT, stride, (void*)(4*sizeof(float)));

  return 0;
}

void gl_load_sprites() {

  /*
   * Upload textures and masks
   */

  if (Lgr != LastLgr) {
    glDeleteTextures(NumPictures, TexPictures);
    NumPictures = 0;
    glDeleteTextures(NumMasks, TexMasks);
    NumMasks = 0;

    for (int i=0; i<Lgr->picture_count; i++) {
      TexPictures[NumPictures++] = upload_picture(&Lgr->pictures[i]);
    }

    for (int i=0; i<Lgr->mask_count; i++) {
      // TODO
      //TexMasks[NumMasks++] = upload_mask(&Lgr->masks[i]);
    }

    LastLgr = Lgr;
  }

  Renderable.clear();

  /*
   * Create sprite vertices
   */

  std::vector<SpriteVertex> verts = {};

  for (int sprite_index = 0; sprite_index < MAX_SPRITES; sprite_index++) {
    sprite* spr = Ptop->sprites[sprite_index];
    if (!spr) { break; }

    if (!spr->picture_name[0]) { // || !spr->mask_name[0]) {
      continue; // TODO
    }

    //printf("distance: %i, clipping: %i\n", spr->distance, spr->clipping);

    int picture_index = Lgr->get_picture_index(spr->picture_name);
    if (picture_index < 0) {
        internal_error("draw_sprites picture_index < 0");
    }
    picture* pict = &Lgr->pictures[picture_index];

    mask* msk = nullptr;
    int mask_index = -1;
    if (spr->mask_name[0]) {
      printf("mask name: %s\n", spr->mask_name);
      mask_index = Lgr->get_mask_index(spr->mask_name);
      if (mask_index < 0) {
          internal_error("gl_init_sprites mask_index < 0");
      }
      msk = &Lgr->masks[mask_index];
    }




    float w = pict->width * PixelsToMeters;
    float h = pict->height * PixelsToMeters;

    int n_slabs = (pict->height+9) / MASK_ROWS;
    //printf("n_slabs: %i, wireframe_height: %f\n", n_slabs, spr->wireframe_height);

    n_slabs = 1;
    Renderable.push_back({ sprite_index, mask_index, picture_index, (int)verts.size(), n_slabs * 6 });

    float x0 = spr->r.x;
    float y0 = spr->r.y;
    float x1 = x0 + w;
    float y1 = y0 + h; // / n_slabs;
    //printf("x0: %f, y0: %f, x1: %f, y1: %f\n", x0, y0, x1, y1);

    float u0 = 0;
    float v0 = 0;
    float u1 = 1;
    float v1 = 1;

    short off = -1; // msk ? get_mask_offset(msk, i*MASK_ROWS) : -1;

    verts.push_back({ x0, -y0, u0, v0, off, 0 });
    verts.push_back({ x1, -y0, u1, v0, off, 0 });
    verts.push_back({ x1, -y1, u1, v1, off, 0 });
    verts.push_back({ x0, -y0, u0, v0, off, 0 });
    verts.push_back({ x1, -y1, u1, v1, off, 0 });
    verts.push_back({ x0, -y1, u0, v1, off, 0 });


    //for (int i=0; i<n_slabs; i++) {

    //  float x0 = spr->r.x;
    //  float y0 = spr->r.y + h / n_slabs * i;
    //  float x1 = x0 + w;
    //  float y1 = y0 + h / n_slabs;
    //  printf("x0: %f, y0: %f, x1: %f, y1: %f\n", x0, y0, x1, y1);

    //  float u0 = 0;
    //  float v0 = 1 / n_slabs * i;
    //  float u1 = 1;
    //  float v1 = v0 + 1 / n_slabs;

    //  short off = msk ? get_mask_offset(msk, i*MASK_ROWS) : -1;

    //  verts.push_back({ x0, -y0, u0, v0, off, 0 });
    //  verts.push_back({ x1, -y0, u1, v0, off, 0 });
    //  verts.push_back({ x1, -y1, u1, v1, off, 0 });
    //  verts.push_back({ x0, -y0, u0, v0, off, 0 });
    //  verts.push_back({ x1, -y1, u1, v1, off, 0 });
    //  verts.push_back({ x0, -y1, u0, v1, off, 0 });
    //}
  }


  glBindVertexArray(SpriteVAO);
  glBindBuffer(GL_ARRAY_BUFFER, SpriteVBO);

  size_t verts_size = sizeof(SpriteVertex) * verts.size();
  glBufferData(GL_ARRAY_BUFFER, verts_size, verts.data(), GL_STATIC_DRAW);
}




void gl_render_sprites(float* frustrum, Clipping clipping) {


  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUseProgram(SpriteShaderProgram);
  glUniform1i(glGetUniformLocation(SpriteShaderProgram, "PictureTexture"), 0);
  glUniform1i(glGetUniformLocation(SpriteShaderProgram, "PaletteTexture"), 1);
  glUniform1i(glGetUniformLocation(SpriteShaderProgram, "MaskTexture"), 2);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_1D, PaletteTexture);

  glBindVertexArray(SpriteVAO);
  glBindBuffer(GL_ARRAY_BUFFER, SpriteVBO);


  for (auto r : Renderable) {

    sprite* sprite = Ptop->sprites[r.sprite_id];

    if (sprite->clipping != clipping) continue;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, TexPictures[r.picture_id]);

    //glActiveTexture(GL_TEXTURE2);
    //glBindTexture(GL_TEXTURE_1D, TexMasks[r.mask_id]);


    glDrawArrays(GL_TRIANGLES, r.vert_off, r.n_verts);
    //printf("off: %i, n: %i\n", r.vert_off, r.n_verts);

  }
}



static int get_mask_offset(mask* msk, int row) {
  size_t off = 0;
  int line;

  mask_element* data = msk->data;

  while (true) {
    if (line == row) {
      return off * 4;
    }
    if (data->type == MaskEncoding::EndOfLine) {
      if (++line == msk->height) {
        internal_error("get_mask_offset not found");
      }
    }
    data++;
  }
}


static GLuint upload_picture(picture* p) {
  std::vector<unsigned char> img(p->width * p->height * 4, 0);

  int offset = 0;
  for (int i = 0; i<p->height; i++) {
      int j = 0;
      while (true) {
          int skip = read_varint(p->data, offset);
          if (skip == -1) {
              // leave transparent
              break;
          }
          j += skip;

          int count = read_varint(p->data, offset);

          for (int c=0; c<count; c++) {
            auto img_off = (i * p->width + j + c) * 4;
            memcpy(&img[img_off], &Lgr->palette_data[p->data[offset + c] * 3], 3);
            img[img_off+3] = 0xFF;
          }

          j += count;
          offset += count;
      }
  }

  return upload_rgba(img.data(), p->width, p->height);
}

static GLuint upload_mask(mask* m) {

  // Mask is stored as an array of shorts: {type}{length}

  unsigned char mdata[1024*256];
  size_t off = 0;
  int eol = 0;

  mask_element* data = m->data;

  while (true) {
    mdata[off++] = (unsigned char) data->type;
    off++;
    mdata[off++] = data->length & 255;
    mdata[off++] = data->length >> 8;

    if (data->type == MaskEncoding::EndOfLine) {
      if (++eol == m->height) break;
    }

    data++;
  }

  GLuint tex_id;
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &tex_id);
  glBindTexture(GL_TEXTURE_2D, tex_id);
  glTexImage1D(
      GL_TEXTURE_2D, 0, GL_RG16, off>>2, 0, GL_RG_INTEGER, GL_UNSIGNED_SHORT, mdata
  );
  return tex_id;
}



