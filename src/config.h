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

#ifndef CONFIG_H
#define CONFIG_H

#include "blocks.h"
#include "util.h"

struct {
    int height;
    int padding;
    color bg;
    color fg;
    char *font;
    int shortLabels;

    int trayPadding;
    int trayIconSize;
    char *trayBar;
    enum Pos traySide;
} conf;

void configParse(const char *config);

#endif /* CONFIG_H */
