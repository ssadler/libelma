
#include <chrono>
#include <thread>

#include "./state.h"
#include "./physics_init.h"
#include "eol_settings.h"
#include "M_PIC.H"
#include "fs_utils.h"
#include "level_load.h"
//#include "canvas.h"
#include "platform_impl.h"
#include "gl_renderer.h"
#include "platform_sdl_keyboard.h"
#include "util/util.h"
#include "KIRAJZOL.H"
#include "qopen.h"
#include "lib_gameloop.h"
#include "simulation.h"
#include "physics_init.h"
#include "platform_sdl_keyboard.h"


void elma_init() {

  util::random::seed();
  EolSettings = new eol_settings();
  eol_settings::read_settings();

  SCREEN_WIDTH = EolSettings->screen_width();
  SCREEN_HEIGHT = EolSettings->screen_height();

  platform_init(); // ~120ms

  init_qopen();

  State = new state;
}

Simulation* init_game(const char* levName) {

  init_physics_data();
  init_renderer();
  Rec1 = new recorder;
  Rec2 = new recorder;

  load_level_play(levName); // ~400ms

  return gameloop_init(levName);
}

Simulation* init_game(int internal_index) {
  finame filename;
  sprintf(filename, "QWQUU%03d.LEV", internal_index + 1);
  return init_game(filename);
}


bool key_is_down(int sdlCode) {
  return keyboard::is_down(static_cast<SDL_Scancode>(sdlCode));
}

bool key_just_pressed(int sdlCode) {
  return keyboard::was_just_pressed(static_cast<SDL_Scancode>(sdlCode));
}

void adjust_zoom(double adj) {
  EolSettings->set_zoom(EolSettings->zoom() + adj);
  //set_zoom_factor();
  //if (adj > 0) { novelkepmeret(); }
  //else { csokkentkepmeret(); }
}

void sleep(double ms) {
  std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(ms));
}

void set_quality(int q) {
  if (q == 101) {
    State->high_quality = (State->high_quality + 1) % 3;
  } else {
    State->high_quality = q;
  }
//  canvas::create_canvases();
}

const unsigned char* get_keyboard_buffer(size_t* size) {
  *size = SDL_NUM_SCANCODES;
  return keyboard::SdlLive;
}


void set_gl_render_callback(void (*callback)()) {
  gl_render_callback = callback;
}
