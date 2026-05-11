
#include <cstring>

#include "physics_init.h"
#include "simulation.h"
#include "main.h"


Simulation::Simulation() {
  //memset(this, 0, sizeof(*this));


  init_motor(&motor);


  //startTime = get_milliseconds();
}


//double Simulation::stopwatch() {
//  return get_milliseconds() - startTime;
//}
//
//void Simulation::pause() {
//  pauseTime = get_milliseconds();
//}
//
//void Simulation::resume() {
//  startTime += get_milliseconds() - pauseTime;
//  pauseTime = 0.0;
//}
