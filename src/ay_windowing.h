/*
   ay_windowing.h

   Platform-agnostic window creation and GPU-free pixel presentation.
   Mirrors ay_threading.h: this header declares the contract, and exactly
   one backend .c file (ay_windowing_win32.c, future ay_windowing_linux.c,
   ay_windowing_macos.c, ...) is compiled in per-platform to implement it.
*/

#ifndef AY_WINDOWING_H
#define AY_WINDOWING_H

#include <stdint.h>
#include <stdbool.h>
#include <glfw3.h> // window creation and glfwPollEvents/glfwGetTime stay portable via glfw itself

//-----------------------------------------------------------------------------
// [SECTION] opaque type
//-----------------------------------------------------------------------------

// concrete fields (native window handle, presentation surface, etc.) are
// entirely backend-specific and defined only inside the backend .c file
typedef struct _ayWindow ayWindow;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

ayWindow* ay_create_window(uint32_t uWidth, uint32_t uHeight, const char* pcTitle);
void      ay_destroy_window(ayWindow* ptWindow);
bool      ay_window_should_close(ayWindow* ptWindow);
void      ay_window_set_title(ayWindow* ptWindow, const char* pcTitle);

uint32_t  ay_window_get_width(ayWindow* ptWindow);
uint32_t  ay_window_get_height(ayWindow* ptWindow);

// blits raw RGBA8 pixels (top-left origin, row-major, 4 bytes/pixel) directly
// onto the window with no GPU involved. each backend handles whatever pixel
// format/native api conversion it needs internally (e.g. the win32 backend
// converts to BGRA for its DIB section); callers never need to know that.
void ay_window_present_pixels(ayWindow* ptWindow, const uint8_t* pucRGBAPixels, uint32_t uWidth, uint32_t uHeight);

#endif
