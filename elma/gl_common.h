#ifndef GL_COMMON_H
#define GL_COMMON_H

#include "affine_pic.h"
#include <glad/glad.h>
#include <memory>


extern int FrameWidth;
extern int FrameHeight;

GLuint gl_shader_program(const char* vert, const char* frag);
GLuint upload_affine_texture(affine_pic* pic);
GLuint upload_pic8_texture(pic8* pic);
GLuint upload_pcx8(unsigned char* pixels, int width, int height);
GLuint upload_rgba(unsigned char* pixels, int width, int height);

extern GLuint PaletteTexture;


/*
 * Attach reference counted automatic cleanup to GL resources
 */

template<typename Derived>
class GlResource {
  std::shared_ptr<GLuint> id;
  public:
    GlResource() {}
    GlResource(GLuint p) : id(new GLuint(p), Derived::cleanup) {}
    const bool empty() const { return !id || *id == 0; };
    operator bool() const { return !empty(); }
    GLuint& operator*() { return *id; }
    const GLuint& operator*() const { return *id; }
};
class GlTexture : public GlResource<GlTexture> {
  public:
    using GlResource::GlResource;
    static void cleanup(GLuint* p) { glDeleteTextures(1, p); }
};
class GlProgram : public GlResource<GlProgram> {
  public:
    using GlResource::GlResource;
    static void cleanup(GLuint* p) { glDeleteProgram(*p); }
};

#endif // GL_COMMON_H
