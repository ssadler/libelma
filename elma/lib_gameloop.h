

#ifndef LIB_GAMELOOP_H
#define LIB_GAMELOOP_H

#include "simulation.h"

#define FRAME_FPS200 (0.182 * .0024 * 5) // 0.00218


Simulation* gameloop_init(const char* filenev);
long gameloop_step(Simulation* sim, double dt, int keys, std::vector<Simulation*> shadows);
void gameloop_render(Simulation* sim, std::vector<Simulation*>& shadows);


#endif
