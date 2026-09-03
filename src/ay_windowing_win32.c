/*
   ay_windowing_win32.c
*/

#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "ay_windowing.h" // pulls in glfw3.h, must come before glfw3native.h below

#define GLFW_EXPOSE_NATIVE_WIN32
#include <glfw3native.h>

//-----------------------------------------------------------------------------
// [SECTION] concrete struct (backend-private)
//-----------------------------------------------------------------------------

struct _ayWindow
{
    GLFWwindow* pGLFWWindow;
    HWND        hWnd;
    HDC         hMemDC;      // memory device context holding the DIB
    HBITMAP     hBitmap;     // the DIB section bitmap
    HBITMAP     hOldBitmap;  // original bitmap selected into hMemDC, restored on destroy
    void*       pDIBPixels;  // raw BGRA pixel memory backing hBitmap, written to directly
    uint32_t    uWidth;
    uint32_t    uHeight;
};

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

ayWindow*
ay_create_window(uint32_t uWidth, uint32_t uHeight, const char* pcTitle)
{
    ayWindow* ptNewWindow = malloc(sizeof(ayWindow));
    if(!ptNewWindow) return NULL;
    memset(ptNewWindow, 0, sizeof(ayWindow));

    glfwInit();

    // no GPU context at all, glfw is only used for the window/input here
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    ptNewWindow->pGLFWWindow = glfwCreateWindow(uWidth, uHeight, pcTitle, NULL, NULL);
    ptNewWindow->hWnd = glfwGetWin32Window(ptNewWindow->pGLFWWindow);

    HDC hWindowDC = GetDC(ptNewWindow->hWnd);
    ptNewWindow->hMemDC = CreateCompatibleDC(hWindowDC);

    // top-down 32bpp DIB: negative height means row 0 is the top row, matching
    // our framebuffer's origin, and BI_RGB 32bpp expects BGRA byte order
    BITMAPINFO tBMI = {0};
    tBMI.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    tBMI.bmiHeader.biWidth       = (LONG)uWidth;
    tBMI.bmiHeader.biHeight      = -(LONG)uHeight;
    tBMI.bmiHeader.biPlanes      = 1;
    tBMI.bmiHeader.biBitCount    = 32;
    tBMI.bmiHeader.biCompression = BI_RGB;

    ptNewWindow->hBitmap = CreateDIBSection(hWindowDC, &tBMI, DIB_RGB_COLORS, &ptNewWindow->pDIBPixels, NULL, 0);
    ptNewWindow->hOldBitmap = (HBITMAP)SelectObject(ptNewWindow->hMemDC, ptNewWindow->hBitmap);

    ReleaseDC(ptNewWindow->hWnd, hWindowDC);

    ptNewWindow->uWidth  = uWidth;
    ptNewWindow->uHeight = uHeight;

    return ptNewWindow;
}

void
ay_destroy_window(ayWindow* ptWindow)
{
    if(!ptWindow) return;

    SelectObject(ptWindow->hMemDC, ptWindow->hOldBitmap);
    DeleteObject(ptWindow->hBitmap);
    DeleteDC(ptWindow->hMemDC);

    glfwDestroyWindow(ptWindow->pGLFWWindow);
    glfwTerminate();
    free(ptWindow);
}

bool
ay_window_should_close(ayWindow* ptWindow)
{
    return glfwWindowShouldClose(ptWindow->pGLFWWindow);
}

void
ay_window_set_title(ayWindow* ptWindow, const char* pcTitle)
{
    glfwSetWindowTitle(ptWindow->pGLFWWindow, pcTitle);
}

uint32_t
ay_window_get_width(ayWindow* ptWindow)
{
    return ptWindow->uWidth;
}

uint32_t
ay_window_get_height(ayWindow* ptWindow)
{
    return ptWindow->uHeight;
}

void
ay_window_present_pixels(ayWindow* ptWindow, const uint8_t* pucRGBAPixels, uint32_t uWidth, uint32_t uHeight)
{
    // convert engine rgba -> the dib's expected bgra while copying into the
    // window's backing pixel memory, no gpu involved anywhere in this path
    uint8_t* pucDst = (uint8_t*)ptWindow->pDIBPixels;
    uint32_t uPixelCount = uWidth * uHeight;

    for(uint32_t i = 0; i < uPixelCount; i++)
    {
        pucDst[i * 4 + 0] = pucRGBAPixels[i * 4 + 2]; // B
        pucDst[i * 4 + 1] = pucRGBAPixels[i * 4 + 1]; // G
        pucDst[i * 4 + 2] = pucRGBAPixels[i * 4 + 0]; // R
        pucDst[i * 4 + 3] = pucRGBAPixels[i * 4 + 3]; // A
    }

    HDC hWindowDC = GetDC(ptWindow->hWnd);
    BitBlt(hWindowDC, 0, 0, (int)ptWindow->uWidth, (int)ptWindow->uHeight, ptWindow->hMemDC, 0, 0, SRCCOPY);
    ReleaseDC(ptWindow->hWnd, hWindowDC);
}
