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

#include "util.h"
#include <stdio.h>

void parseColor(JsonObject *jo, const char *key, color dest, JsonError *err) {
    if (jsonGetPairIndex(jo, key) == -1) {
        return;
    }

    JsonArray *col;
    jsonGetArray(jo, key, &col, err);
    if (jsonErrorIsSet(err)) {
        fprintf(stderr, "Error parsing array \"%s\"\n", key);
        jsonErrorCleanup(err);
        jsonErrorInit(err);
        return;
    }

    if (col->used != 3) {
        fprintf(stderr, "\"%s\" array must contain 3 values\n", key);
        jsonErrorCleanup(err);
        jsonErrorInit(err);
        return;
    }

    for (int j = 0; j < 3; j++) {
        void *val = col->vals[j];
        if (jsonGetType(val) != JSON_NUMBER) {
            fprintf(stderr, "Value in \"%s\" array is not a valid int\n", key);
            jsonErrorCleanup(err);
            jsonErrorInit(err);
            return;
        }

        JsonNumber *n = (JsonNumber *) val;
        dest[j] = n->data;
    }
}

