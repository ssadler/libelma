
#include "main.h"
#include "gl_canvas.h"
#include "gl_shaders.h"
#include "gl_common.h"

#include <string>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <thread>
#include <fstream>
#include <sstream>



void GlManaged::set_fragment_shader_from_file(const char* path) {
  if (vert_watcher != nullptr) {
    internal_error("set_fragment_shader_from_file: already active");
  }
  if (!std::filesystem::exists(path)) {
    internal_error("set_fragment_shader_from_file does not exist");
  }

  const std::string p = std::string(path);
  vert_last_write = std::filesystem::last_write_time(path);

  std::ifstream f(path);
  if (!f.is_open()) { internal_error("Error opening the file!"); }
  std::ostringstream ss;
  ss << f.rdbuf();
  f.close();
  frag = ss.str();

  vert_watcher = std::make_shared<std::jthread>(
    [this, p](std::stop_token stopToken) {
      watchLoop(stopToken, p);   // NOLINT(performance-unnecessary-value-param)
    }
  );
}


void GlManaged::compile() {
  if (program) {
    internal_error("GlManaged::compile: already compiled");
  }

  std::string name = this->name;
  program = GlProgram(_compile_shader());

  if (*program == -1) {
    internal_error("Glso all Managed::compile: failed creating program");
  }
  _compile_vao();
}

GLuint GlManaged::_compile_shader() {
  if (frag.empty()) {
    internal_error("GlManaged::compile: frag empty");
  }
  if (vert.empty()) {
    internal_error("GlManaged::compile: vert empty");
  }
  if (attribute_pointers.empty()) {
    internal_error("GlManaged::compile: attribute pointers not set");
  }

  return gl_shader_program(vert.c_str(), frag.c_str());
}

void GlManaged::_compile_vao() {
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  int stride = get_stride();
  size_t offset = 0;

  for (auto [i, p]: enumerate(attribute_pointers)) {
    glEnableVertexAttribArray(i);
    glVertexAttribPointer(i, p.size, p.type, p.normalized, stride, (void*)offset);
    offset += p.size * 4; // TODO
  }
}

void GlManaged::_init_draw() {

  if (dirty != nullptr && dirty->exchange(false)) {
    auto p = _compile_shader();
    if (p <= 0) {
      printf("error recompiling program\n");
    } else {
      glDeleteProgram(*program);
      *program = p;
    }
  }

  glUseProgram(*program);
  //printf("!! %i\n", glGetError());

  for (auto& cb : draw_cbs) { cb(); }
  //printf("!! %i\n", glGetError());
  for (auto& [id, cb] : persistant_uniforms) { cb(id); }
  //printf("!! %i\n", glGetError());
  for (auto [slot, texture] : textures) {
    glBindTextureUnit(slot-GL_TEXTURE0, texture);
  //printf("!! %i\n", glGetError());
  }

  //glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindVertexArray(vao);
  //printf("!! %i\n", glGetError());
}


void GlManaged::watchLoop(
  std::stop_token stopToken,  // NOLINT(performance-unnecessary-value-param)
  const std::string& path
) {
  using namespace std::chrono_literals;

  while (!stopToken.stop_requested()) {
    try {
      if (std::filesystem::exists(path)) {
        auto current = std::filesystem::last_write_time(path);

        if (current != vert_last_write) {

          std::ifstream f(path);
          if (!f.is_open()) { internal_error("Error opening the file!"); }
          std::ostringstream ss;
          ss << f.rdbuf();
          f.close();
          frag = ss.str();

          vert_last_write = current;
          dirty->store(true, std::memory_order_release);
        }
      }
    }
    catch (...) {
      printf("Error polling vert path");
    }

    std::this_thread::sleep_for(200ms);
  }
}


void GlManaged::uniform1i(const char* name, int value) const {
  glUniform1i(glGetUniformLocation(*program, name), value);
}
void GlManaged::uniform1f(const char* name, float value) const {
  glUniform1f(glGetUniformLocation(*program, name), value);
}
void GlManaged::uniform2f(const char* name, float val0, float val1) const {
  glUniform2f(glGetUniformLocation(*program, name), val0, val1);
}
void GlManaged::uniform3f(const char* name, float val0, float val1, float val2) const {
  glUniform3f(glGetUniformLocation(*program, name), val0, val1, val2);
}
void GlManaged::uniform4f(const char* name, float val0, float val1, float val2, float val3) const {
  glUniform4f(glGetUniformLocation(*program, name), val0, val1, val2, val3);
}

void GlManaged::persist_uniform1i(const char* name, int value) {
  persist_uniform(name, [=](GLuint idx) { glUniform1i(idx, value); });
}
void GlManaged::persist_uniform1f(const char* name, float value) {
  persist_uniform(name, [=](GLuint idx) { glUniform1f(idx, value); });
}
void GlManaged::persist_uniform2f(const char* name, float val0, float val1) {
  persist_uniform(name, [=](GLuint idx) { glUniform2f(idx, val0, val1); });
}
void GlManaged::persist_uniform3f(const char* name, float val0, float val1, float val2) {
  persist_uniform(name, [=](GLuint idx) { glUniform3f(idx, val0, val1, val2); });
}
void GlManaged::persist_uniform4f(const char* name, float val0, float val1, float val2, float val3) {
  persist_uniform(name, [=](GLuint idx) { glUniform4f(idx, val0, val1, val2, val3); });
}
