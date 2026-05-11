#ifndef GL_CANVAS_H
#define GL_CANVAS_H

#include "simulation.h"
#include "vect2.h"
#include "physics_init.h"
#include "LEJATSZO.H"
#include "level.h"
#include "sprite.h"
#include <glad/glad.h>
#include <vector>

class GLCanvas {
  public:
    size_t ground_n_verts = 0;
    std::vector<std::vector<float>> ground;
    std::vector<std::vector<float>> sky;
    size_t sky_n_verts = 0;
    std::vector<std::vector<float>> grass;
    size_t grass_n_verts = 0;

    std::vector<std::vector<float>> merged;
    

    GLCanvas();

    void render(double t, motorst* pmot, valtozok* metadata, vect2 corner, int x1, int y1, int x2, int y2,
        std::vector<Simulation*>& shadows
        );
    //void render_minimap(bool player1, pic8* pic, vect2 corner, int x1, int y1, int x2, int y2);
    static void setup();
};

struct canvas_ubo0 {
  float frustrum[4];
  float pixels_to_meters_at_load;
  float pixels_to_meters;
};

extern GLCanvas* GL_Canvas;

int gl_canvas_init();

void gl_canvas_render_back(float*);
void gl_canvas_render_sky();
void gl_canvas_render_fore();


int gl_init_objects();
void gl_render_objects(float* frustrum, float t);

int gl_init_kuski();
void gl_render_kuski(float* frustrum, motorst* pmot, valtozok* metadata, bool is_shadow);

int gl_init_sprites();
int gl_init_sprite_system();
void gl_load_sprites();
void gl_render_sprites(float* frustrum, Clipping clipping);


void gl_init_grass();
void gl_render_grass(float* frustrum);
void gl_render_extra(float* frustrum, float t);


extern bool IsRenderCallback;

int gl_init_tas();
void gl_render_box(float r, float g, float b, float a, int x, int y, int rows, int cols, int paddingX, int paddingY);
void gl_render_text(float r, float g, float b, float a, int x, int y, int colOff, int rowOff, const char* text);


void gl_render_minimap(motorst* mot, valtozok* metadata);
void gl_init_minimap();


#endif
