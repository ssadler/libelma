
#include <algorithm>
#include <cmath>
#include <cstring>
#include <directinput/scancodes.h>
#include <filesystem>

#include "LEJATSZO.H"
#include "EDITUJ.H"
#include "eol/console.h"
#include "eol_settings.h"
#include "keys.h"
#include "flagtag.h"
#include "fs_utils.h"
#include "KIRAJZOL.H"
#include "physics_forces.h"
#include "level.h"
#include "lgr.h"
#include "main.h"
#include "object.h"
#include "physics_init.h"
#include "platform_impl.h"
#include "segments.h"
#include "timer.h"

#include "lib_gameloop.h"
#include "simulation.h"


static viewtimest Viewtime1 = {1, 0, 1, 0};
static viewtimest Viewtime2 = {1, 0, 1, 0};



Simulation* gameloop_init(const char* filenev) {

  Simulation* sim = new Simulation();

  init_physics_data();

  if (!Ptop) {
      internal_error("setup_gameloop() !Ptop!");
  }
  Ptop->flip_objects();
  Ptop->sort_objects();
  // setallaktiv allitja be motor kezdeti helyzetet is!:
  Ptop->initialize_objects(&sim->motor);

  // Eloszor keretet tobbszor is kirakja kirajzol320:
  Kitoltestmegrak = Kitoltestmegrakkezd;

  Lgr->pal->set();

  reset_event_buffer();

  // Valtozok inicializalasa:
  sim->valt.baljobbv_f.ucsoford = -1000.0;
  sim->valt.baljobbv_f.ucsoforgas = -1000.0;
  sim->valt.baljobbv_h.ucsoford = -1000.0;
  sim->valt.baljobbv_h.ucsoforgas = -1000.0;
  sim->valt.utolsougras = -100.0;
  sim->valt.showkep = 1;

  reset_motor_forces(&sim->motor);

  { // setup camera
    auto c = &sim->current_camera;
    c->mode = CameraMode::Normal;
    c->x = sim->motor.bike.r.x;
    c->y = sim->motor.bike.r.y;

    Ptop->get_boundaries(&c->min_x, &c->min_y, &c->max_x, &c->max_y, false);
    // convert level y-coordinates to camera y-coordinates
    c->min_y *= -1;
    c->max_y *= -1;
  }

  // Get apples
  for (int i=0; i<MAX_OBJECTS; i++) {
    if (Ptop->objects[i] && Ptop->objects[i]->type == object::Type::Food) {
      sim->apples.push_back({ (short)i, false });
    }
  }

  // fixes a weird bug with random stuff at start
  gameloop_step(sim, 0.0, 0, {});

  return sim;
}



long gameloop_step(Simulation* sim, double dt, int keys, std::vector<Simulation*> shadows) {

  // restore apples
  for (Apl apl : sim->apples) {
    Ptop->objects[apl.id]->active = !apl.got;
  }

  if (!sim->meghalt) {

    // Ugras elintezese:
    bool can_volt = sim->eddig > sim->valt.utolsougras + VoltDelay;

    bool gas = (keys & 1) > 0;
    bool brake = (keys & 2) > 0;
    bool left_volt = can_volt && (keys & 4) > 0;
    bool right_volt = can_volt && (keys & 8) > 0;

    if (left_volt || right_volt) {
      sim->valt.utolsougras = sim->eddig;
      sim->valt.ugras1volt = right_volt;
    }

    simulate_bike_physics(&sim->motor, sim->eddig, dt, gas, brake, right_volt, left_volt);

    sim->meghalt = check_object_collision(&sim->motor) == BikeState::Dead;

    int objszam;
    while (get_event_buffer(nullptr, nullptr, &objszam)) {

      if (objszam >= 0) {
        int prev_apple_count = sim->motor.apple_count;
        BikeState eredmeny = spritefeldolgoz(objszam, &sim->motor.apple_count, &sim->motor);

        if (eredmeny == BikeState::Dead) {
          sim->meghalt = 1;
        } else if (eredmeny == BikeState::Finish) {
          if (std::all_of(sim->apples.begin(), sim->apples.end(), [](Apl a) { return a.got; })) {
            sim->megvanido = sim->eddig * TimeToCentiseconds;
          }
        }

        if (prev_apple_count < sim->motor.apple_count) {
            sim->motor.last_apple_time = sim->eddig * TimeToCentiseconds;
        }

        if (Ptop->objects[objszam]->type == object::Type::Food) {
          for (auto &apl : sim->apples) {
            if (apl.id == objszam) {
              apl.got = true;
              break;
            }
          }
        }
      }
    }

    sim->eddig += dt;
  }

  kulsoresz(
      &sim->motor, &State->keys1, &sim->valt, Rec1, &Viewtime1,
      sim->eddig, &sim->valt.showkep, sim->meghalt, keys
  );

  return sim->meghalt ? -1 : sim->megvanido;
}


void gameloop_render(Simulation* sim, std::vector<Simulation*>& shadows) {
  kirajzol320(sim->eddig, &sim->valt, &sim->valt, Viewtime1.viewkinplay, Viewtime1.timekinplay,
              Viewtime2.viewkinplay, Viewtime2.timekinplay, sim->current_camera, &sim->motor,
              shadows
              );
}


