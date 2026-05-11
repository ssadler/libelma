#ifndef GL_RENDERER_H
#define GL_RENDERER_H

#include <SDL.h>
#include <glad/glad.h>

int gl_init(SDL_Window* sdl_window, int width, int height, int pitch);
void gl_upload_frame(const unsigned char* indices, int pitch);
void gl_update_palette(const void* palette);
void gl_present();
int gl_resize(int width, int height, int pitch);
void gl_cleanup();

void gl_set_render_callback(int (*moo)(int));

extern void (*gl_render_callback)();

#endif // GL_RENDERER_H
