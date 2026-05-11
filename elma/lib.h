

#include "simulation.h"
#include "lib_gameloop.h"



//extern "C" {
  
  void elma_init();
  Simulation* init_game(const char* lev);
  Simulation* init_game(int internalIdx);
  void sim_step(Simulation* sim, double dt);
  bool key_just_pressed(int sdlCode);
  bool key_is_down(int sdlCode);
  void adjust_zoom(double adj);
  void sleep(double ms);
  void set_quality(int quality);
  void set_gl_render_callback(void (*callback)());
  const unsigned char* get_keyboard_buffer(size_t* size);
void gl_render_box(float r, float g, float b, float a, int x, int y, int rows, int cols, int paddingX, int paddingY);
void gl_render_text(float r, float g, float b, float a, int x, int y, int colOff, int rowOff, const char* text);


//}
