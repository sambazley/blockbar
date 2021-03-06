/* Copyright (C) 2018 Sam Bazley
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef WINDOW_H
#define WINDOW_H
#include <cairo/cairo-xlib.h>
#include <X11/Xlib.h>

enum RenderIndex {
    RI_VISIBLE,
    RI_BUFFER,
    RI_COUNT
};

struct Bar {
    Window window;
    int x;
    int width;
    char *output;

    cairo_surface_t *sfc [RI_COUNT];
    cairo_t *ctx [RI_COUNT];
};

extern Display *disp;
extern int barCount;
extern struct Bar *bars;

int createBars();
void updateGeom();
void pollEvents();
void cleanupBars();

#endif /* WINDOW_H */
