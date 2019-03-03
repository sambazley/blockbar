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

#include "types.h"
#include "util.h"
#include <ujson.h>

void cleanupSettings();
int setSetting(struct Setting *setting, union Value val);
int isSettingModified(struct Setting *setting);

JsonObject *configInit(const char *config);
void configParseGeneral(JsonObject *jsonConfig);
void configParseBlocks(JsonObject *jsonConfig);
void configCleanup(JsonObject *jsonConfig);
char *configSave(FILE *file, int explicit);

extern const char *typeStrings [];
extern struct Settings settings;
extern int settingCount;
extern struct Properties defProperties;
extern int propertyCount;

extern int blockCount;
extern struct Block *blocks;

#endif /* CONFIG_H */
