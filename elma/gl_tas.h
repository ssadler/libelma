#ifndef GL_GLTAS_H
#define GL_GLTAS_H

#include <glad/glad.h>

extern bool IsRenderCallback;

int gl_init_tas();
void gl_render_box(float r, float g, float b, float a, int x, int y, int rows, int cols, int paddingX, int paddingY);
void gl_render_text(float r, float g, float b, float a, int x, int y, int colOff, int rowOff, const char* text);

#endif // GL_GLTAS_H
