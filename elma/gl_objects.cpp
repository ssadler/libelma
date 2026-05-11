

#include "EDITUJ.H"
#include "gl_common.h"
#include "anim.h"
#include "gl_shaders.h"
#include "lgr.h"
#include "pic8.h"
#include "object.h"
#include "level.h"
#include "affine_pic.h"
#include "physics_init.h"
#include <cstdio>
#include <glad/glad.h>
#include <cmath>
#include <cstring>
#include <memory>


GLuint ObjectShaderProgram;
GLuint ObjectVAO, ObjectVBO;
GLuint AnimKiller, AnimExit, AnimFood;
GlManaged* FoodShader;
GlManaged* ExitShader;
GlManaged* KillerShader;


static GLuint upload_anim(anim* a);



int gl_init_objects() {

  if (AnimKiller == 0) {
    AnimKiller = upload_anim(Lgr->killer);
    AnimFood = upload_anim(Lgr->food[0]);
    AnimExit = upload_anim(Lgr->exit);
  }

  const char* vert = R"(
  #version 420 core
  layout(std140, binding = 0) uniform GlobalData {
    vec4 uFrustrum;
    float PixelsToMetersAtLoad;
    float PixelsToMeters;
  };
  layout (location = 0) in vec2 pos;
  layout (location = 1) in vec2 texCoord;
  layout (location = 2) in float frameCount;
  layout (location = 3) in float floatingPhase;
  out vec2 fragTexCoord;
  uniform float iTime;

  void main() {
    float frame = floor(iTime / 0.014);

    fragTexCoord = texCoord;
    fragTexCoord.y = 1.0 - fragTexCoord.y;
    fragTexCoord.y /= frameCount;
    fragTexCoord.y += (1.0/frameCount) * mod(frame, frameCount);

    float dy = pos.y + .05 * sin(iTime * 15.5 + floatingPhase);

    float x = (pos.x-uFrustrum.x)/(uFrustrum.z-uFrustrum.x);
    float y = (-dy-uFrustrum.y)/(uFrustrum.w-uFrustrum.y);
    
    vec2 ndc = vec2(-1.0 + x * 2.0, -1.0 + y * 2.0);

    gl_Position = vec4(ndc, 0.0, 1.0);
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
    FragColor = texture(PaletteTexture, index);
    if (index == texture(IndexTexture, vec2(0.0)).r) {
      FragColor = vec4(0.0);
    }
  }
  )";

  FoodShader = new GlManaged("objects");
  FoodShader->set_vertex_shader(vert);
  FoodShader->set_fragment_shader(frag);
  FoodShader->add_input_floats(2, GL_FALSE);
  FoodShader->add_input_floats(2, GL_FALSE);
  FoodShader->add_input_floats(1, GL_FALSE);
  FoodShader->add_input_floats(1, GL_FALSE);
  FoodShader->compile();
  FoodShader->persist_uniform1i("IndexTexture", 0);
  FoodShader->persist_uniform1i("PaletteTexture", 1);
  FoodShader->set_texture(GL_TEXTURE1, PaletteTexture);

  KillerShader = FoodShader->clone();
  ExitShader = FoodShader->clone();

  FoodShader->set_texture(GL_TEXTURE0, AnimFood);
  KillerShader->set_texture(GL_TEXTURE0, AnimKiller);
  ExitShader->set_texture(GL_TEXTURE0, AnimExit);




  auto buf = std::make_unique<std::array<float, 6>[]>(6 * MAX_OBJECTS);


  auto render = [&](object::Type ty, anim* a, GlManaged* shader) {

    float w = a->frames[0]->get_width() / MetersToPixels / 2.0f;
    float h = a->frames[0]->get_height() / MetersToPixels / 2.0f;

    int n = 0;

    for (int i = 0; i < MAX_OBJECTS; i++) {
      object* pker = Ptop->objects[i];
      if (!pker) { break; }
      if (pker->type != ty) { continue; }
      
      float fc = float(a->frame_count);
      float fp = float(pker->floating_phase);

      //float dy = (.05 * sin(t * 15.5 + pker->floating_phase));
      float x0 = pker->r.x + w;
      float y0 = pker->r.y + h;// + dy;
      float x1 = pker->r.x - w;
      float y1 = pker->r.y - h;// + dy;

      buf[n++] = { x0, y0, 0, 0, fc, fp };
      buf[n++] = { x1, y0, 1, 0, fc, fp };
      buf[n++] = { x1, y1, 1, 1, fc, fp };
      buf[n++] = { x0, y1, 0, 1, fc, fp };
      if (ty != object::Type::Food) {
      buf[n++] = { x0, y0, 0, 0, fc, fp };
      buf[n++] = { x1, y1, 1, 1, fc, fp };
      //} else {
      //buf[n++] = { x0, y0, 0, 0, fc, fp };
      //buf[n++] = { x1, y0, 1, 0, fc, fp };
      //buf[n++] = { x1, y1, 1, 1, fc, fp };
      //buf[n++] = { x0, y0, 0, 0, fc, fp };
      //buf[n++] = { x1, y1, 1, 1, fc, fp };
      //buf[n++] = { x0, y1, 0, 1, fc, fp };
      }
    }

    shader->buffer_data(n, buf.get(), GL_STATIC_DRAW);
  };

  render(object::Type::Food, Lgr->food[0], FoodShader);
  render(object::Type::Killer, Lgr->killer, KillerShader);
  render(object::Type::Exit, Lgr->exit, ExitShader);


  return 0;
}


static GLuint upload_anim(anim* a) {
  pic8* _0 = a->get_frame_by_index(0);
  int frame_size = _0->get_width() * _0->get_height();
  int size = frame_size * a->frame_count;
  unsigned char* pixels = new unsigned char[size];

  for (int i=0; i<a->frame_count; i++) {
    memcpy(&pixels[frame_size*i], a->get_frame_by_index(i)->pixels, frame_size);
  }

  GLuint tex_id;
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &tex_id);
  glBindTexture(GL_TEXTURE_2D, tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
               _0->get_width(), _0->get_height() * a->frame_count,
               0, GL_RED, GL_UNSIGNED_BYTE, pixels);
  return tex_id;
}


unsigned short foods[256*6*2];


GlRingBuffer* FoodIndicesBuffer = nullptr;

void gl_render_objects(float* frustrum, float t) {

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  ExitShader->use();
  ExitShader->uniform1f("iTime", t);
  ExitShader->draw();
  //KillerShader->uniform1f("iTime", t);
  KillerShader->draw();


  if (FoodIndicesBuffer == 0) {
    FoodIndicesBuffer = new GlRingBuffer(GL_ELEMENT_ARRAY_BUFFER, 2, 256*6);
  }

  // Make a list of food to render
  int n = 0;
  int tot = 0;

  for (int i = 0; i < MAX_OBJECTS; i++) {
    object* pker = Ptop->objects[i];
    if (!pker) { break; }
    if (pker->type != object::Type::Food) { continue; }
    if (pker->active) {
      foods[n++] = tot;
      foods[n++] = tot+1;
      foods[n++] = tot+2;
      foods[n++] = tot;
      foods[n++] = tot+2;
      foods[n++] = tot+3;
    }
    tot += 4;
  }

  FoodIndicesBuffer->push_data(n, foods);
  //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, FoodIndicesBuffer);
  //glBufferData(GL_ELEMENT_ARRAY_BUFFER, n, foods, GL_STREAM_DRAW);

  FoodShader->uniform1f("iTime", t);
  FoodShader->draw_indexed(n, GL_UNSIGNED_SHORT, foods);
}
