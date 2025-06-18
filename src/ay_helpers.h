// a file for side project functions or stuff not necessary to Archery develpoment 

#ifndef AY_HELPERS_H
#define AY_HELPERS_H

#include "ay_rasterize.h"

ayVec2 transform_grid_to_isometric_view(float x, float y);
void ay_generate_quad_grid(int cols, int rows, float* vertices, uint32_t* indices, bool addRandomColor, bool DEBUG);

#endif