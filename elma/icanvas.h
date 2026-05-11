#ifndef ICANVAS_H
#define ICANVAS_H

#include "grass.h"
#include "lgr.h"
#include "main.h"
#include "sprite.h"
#include "vect2.h"
#include <cstdint>
#include <vector>

class icanvas
{
  public:
    virtual void meters_to_pixels(vect2 meters, int* pixel_x, int* pixel_y);
    virtual void render(bool player1, pic8* pic, vect2 corner, int x1, int y1, int x2, int y2);
    virtual void render_minimap(bool player1, pic8* pic, vect2 corner, int x1, int y1, int x2, int y2);
    // Generate all 3 canvasses
    //static void create_canvases();
};


#endif
