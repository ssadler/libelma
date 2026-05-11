

#include "gl_canvas.h"
#include "gl_shaders.h"
#include <cstring>



static GlManaged* Shader = nullptr;


void gl_render_extra(float* frustrum, float t) {

  //if (Shader == nullptr) {
  //  const char* vert = R"(
  //  #version 410 core
  //  layout (location = 0) in vec2 pos;

  //  void main() {
  //  
  //    vec2 ndc;
  //    ndc.x = -1.0 + pos.x * 2.0;
  //    ndc.y = 1.0 - pos.y * 2.0;
  //  
  //    gl_Position = vec4(ndc, 0.0, 1.0);
  //  }
  //  )";

  //printf("before init extra: %i\n", glGetError());

  //  Shader = new GlManaged("extra");
  //  Shader->set_vertex_shader(vert);
  //  Shader->set_fragment_shader_from_file("...")
  //  Shader->add_vao_floats(2, GL_FALSE);
  //  Shader->compile();

  //  Shader->persist_uniform2f("iResolution", 1600, 1050);
  //  Shader->persist_uniform3f("windDir", .2, .1, .2);
  //  Shader->persist_uniform1f("intensity", 1);

  //  float quad[] = {
  //      0, 0, //0, 0,
  //      1, 0, //1, 0,
  //      1, 1, //1, 1,
  //      0, 0, //0, 0,
  //      1, 1, //1, 1,
  //      0, 1, //0, 1
  //  };
  //  Shader->set_vbo(0, 6, quad, GL_STATIC_DRAW);
  //}



  //Shader->uniform1f("iTime", t / .0014);
  ////Shader->draw(0, 6);

}
