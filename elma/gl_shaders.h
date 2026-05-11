
#ifndef GL_SHADERS_H
#define GL_SHADERS_H

#include "main.h"
#include "gl_common.h"
#include <memory>
#include <tuple>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <functional>
#include <format>
#include <map>


class GlRingBuffer {
  int stride;
  int buftype;
  int max_verts;
  int offset = 0;

  public:
  GLuint vbo;

  GlRingBuffer(int _buftype, int _stride, int _max_verts)
    : GlRingBuffer(
        _buftype, _stride, _max_verts,
        []{ GLuint vbo; glGenBuffers(1, &vbo); return vbo; }()
      ) {}

  GlRingBuffer(int _buftype, int _stride, int _max_verts, GLuint _vbo)
    : stride(_stride), buftype(_buftype), max_verts(_max_verts), vbo(_vbo) {
    glBindBuffer(buftype, vbo);
    glBufferData(buftype, max_verts * 3 * stride, nullptr, GL_STREAM_DRAW);
  }

  void push_data(int num_verts, void* ptr) {
    if (num_verts > max_verts) {
      internal_error("GlRingBuffer::push_data: num_verts > max_verts");
    }
    auto size = num_verts * stride;
    if (offset + size > max_verts * stride * 3) {
      offset = 0;
    }
    glBindBuffer(buftype, vbo);
    glBufferSubData(buftype, offset, size, ptr);
  }
};



struct GLVertexAttributePointer {
  GLuint vbo;
 	GLint size;
 	GLenum type;
 	GLboolean normalized;
};


class GlManaged {
  std::string name;
  std::string frag;
  std::string vert;
  std::vector<GLVertexAttributePointer> attribute_pointers;
  std::vector<std::function<void()>> draw_cbs;
  std::map<GLuint, std::function<void(GLuint idx)>> persistant_uniforms;
  std::map<GLenum, unsigned long> textures;
  GlRingBuffer* ring_buf = nullptr;
  int _ring_buffer_max_verts = 0;
  int _ring_buffer_offset = 0;

  
  public:
  int _num_verts = -1;
    GLuint vao = 0;
    GLuint vbo = 0;

    GlManaged(std::string shader_name) : name(std::move(shader_name)) {
      glGenBuffers(1, &vbo);
    }
    ~GlManaged() {
      glDeleteBuffers(1, &vbo);
      glDeleteVertexArrays(1, &vao);
      delete ring_buf;
    }
    
    /*
     * Clone allows you to copy the whole structure in order to assign a different
     * data buffer
     */
    GlManaged* clone() {
      GlManaged* s = new GlManaged(*this);
      glGenBuffers(1, &s->vbo);
      s->vao = 0;
      s->_compile_vao();
      return s;
    }

    GLuint get_program() {
      return *program;
    }

    void set_fragment_shader(const char* cfrag) {
      frag = std::string(cfrag);
    }

    void set_fragment_shader_from_file(const char* path);

    void set_vertex_shader(const char* cvert) {
      vert = std::string(cvert);
    }

    void add_input_floats(GLint num_vals, GLboolean normalized) {
      attribute_pointers.push_back({ vbo, num_vals, GL_FLOAT, normalized });
    }

    void buffer_data(int num_verts, const void* ptr, GLenum usage) {
      if (!vao) {
        internal_error("GlManaged::buffer_data: compile first");
      }
      if (_ring_buffer_max_verts) {
        internal_error("GlManaged::buffer_data: ring enabled, use push_data");
      }

      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glBufferData(GL_ARRAY_BUFFER, num_verts * get_stride(), ptr, usage);
      _num_verts = num_verts;
    }

    void sub_data(int offset, int num_verts, void* ptr) {
      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glBufferSubData(GL_ARRAY_BUFFER, offset, num_verts * get_stride(), ptr);
    }


    // call: map_buffer_range<float>(0, n_verts, [](float* ptr) { ...
    template <typename T, typename F>
    void map_buffer_range(int offset, int num_verts,
        F&& cb, int flags
    ) {
      if (num_verts > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        auto ptr = (T*)glMapBufferRange(GL_ARRAY_BUFFER, offset, num_verts * get_stride(), flags);
        cb(ptr);
        glUnmapBuffer(GL_ARRAY_BUFFER);
      }
    }

    void enable_ring(int max_verts) {
      ring_buf = new GlRingBuffer(GL_ARRAY_BUFFER, get_stride(), max_verts, vbo);
      //_ring_buffer_max_verts = max_verts;
      ////glBindVertexArray(vao);
      //glBindBuffer(GL_ARRAY_BUFFER, vbo);
      //glBufferData(GL_ARRAY_BUFFER, max_verts * 3 * get_stride(), nullptr, GL_STREAM_DRAW);
    }
    void push_data(int num_verts, void* ptr) {
      if (!ring_buf) {
        internal_error("GlManaged::push_data: ring not enabled");
      }
      ring_buf->push_data(num_verts, ptr);
    }

    void set_texture(GLenum slot, GLuint texture) {
      textures[slot] = texture;
    }

    void add_draw_cb(std::function<void()> cb) {
      draw_cbs.push_back(std::move(cb));
    }


    void uniform1i(const char* name, int value) const;
    void uniform1f(const char* name, float value) const;
    void uniform2f(const char* name, float val0, float val1) const;
    void uniform3f(const char* name, float val0, float val1, float val2) const;
    void uniform4f(const char* name, float val0, float val1, float val2, float val3) const;
    void persist_uniform1i(const char* name, int value);
    void persist_uniform1f(const char* name, float value);
    void persist_uniform2f(const char* name, float val0, float val1);
    void persist_uniform3f(const char* name, float val0, float val1, float val2);
    void persist_uniform4f(const char* name, float val0, float val1, float val2, float val3);

    //void bind_ubo(int idx) {
    //  GLuint blockIndex = glGetUniformBlockIndex(*program, "GlobalData");
    //  glUniformBlockBinding(*program, blockIndex, idx);
    //}

    void use() const {
      if (!program) {
        internal_error("use: not compiled");
      }
      glUseProgram(*program);
    }
    void compile();
    void draw(GLenum mode, GLint first, GLsizei count) {
      _init_draw();
      glDrawArrays(mode, first, count);
  //printf("!! %i, %i, %i %i\n", glGetError(), first, GL_TRIANGLES, count);
    }
    void draw(GLint first, GLsizei count) {
      draw(GL_TRIANGLES, first, count);
    }
    void draw() {
      if (_num_verts == -1) {
        internal_error("GlManaged::draw: buffer not set with buffer_data");
      }
      if (_num_verts > 0) {
        draw(0, _num_verts);
      }
    }
    void draw_indexed(GLsizei count, GLenum type, const void * indices) {
      _init_draw();
      glDrawElements(GL_TRIANGLES, count, type, indices);
    }

  private:
    GlProgram program;
    std::atomic<bool>* dirty = nullptr;
    std::filesystem::file_time_type lastWrite;
    std::shared_ptr<std::jthread> vert_watcher;
    std::filesystem::file_time_type vert_last_write;

    GLint get_stride() {
      GLint stride = 0;
      for (auto &p : attribute_pointers) {
        stride += p.size * 4; // TODO: different sizes?
      }
      return stride;
    }

    void persist_uniform(const char* name, std::function<void(GLuint)> cb) {
      if (!program) {
        internal_error("persist_uniform: not compiled");
      }
      GLuint idx = glGetUniformLocation(*program, name);
      if (idx < 0) {
        internal_error(std::format("persist_uniform: invalid uniform name {}", name));
      }
      persistant_uniforms[idx] = std::move(cb);
    }

    void watchLoop(std::stop_token stopToken, const std::string& path);

    GLuint _compile_shader();
    void _compile_vao();
    void _init_draw();
};


//class GlResource {
//    std::shared_ptr<GLuint> ptr;
//public:
//    GlResource();
//    GlResource(GLuint p)
//      : ptr(new GLuint(p), [this](GLuint *p) { this->cleanup(*p); })
//      {}
//
//    GLuint& operator*() { return *ptr; }
//    const GLuint& operator*() const { return *ptr; }
//    virtual void cleanup(GLuint p) = 0;
//};
//
//class GlTexture : public GlResource {
//  void cleanup(GLuint p) override {
//    glDeleteTextures(1, &p);
//  }
//};



template <typename T,
          typename TIter = decltype(std::begin(std::declval<T>())),
          typename = decltype(std::end(std::declval<T>()))>
constexpr auto enumerate(T && iterable)
{
    struct iterator
    {
        std::size_t i;
        TIter iter;
        bool operator != (const iterator & other) const { return iter != other.iter; }
        void operator ++ () { ++i; ++iter; }
        auto operator * () const { return std::tie(i, *iter); }
    };
    struct iterable_wrapper
    {
        T iterable;
        auto begin() { return iterator{ 0, std::begin(iterable) }; }
        auto end() { return iterator{ 0, std::end(iterable) }; }
    };
    return iterable_wrapper{ std::forward<T>(iterable) };
}

#endif
