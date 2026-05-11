
#ifndef SIMULATION_H
#define SIMULATION_H

#include "LEJATSZO.H"
#include "physics_init.h"


struct Apl;

class Simulation {
  public:
    motorst motor{};
    valtozok valt{};

    camera current_camera{};
    double eddig{};
    int meghalt{};
    long megvanido{};

    std::vector<Apl> apples;

    Simulation();
};

struct Apl {
  short id;
  bool got;
};



#endif
