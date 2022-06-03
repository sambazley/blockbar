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

#include "config.h"
#include "exec.h"
#include "window.h"

void click(struct click *cd)
{
	for (int i = 0; i < block_count; i++) {
		struct block *blk = &blocks[i];

		if (!blk->id) {
			continue;
		}

		int rendered;
		if (blk->eachmon) {
			rendered = blk->data[cd->bar].rendered;
		} else {
			rendered = blk->data->rendered;
		}

		if (!rendered) {
			continue;
		}

		if (cd->x > blk->x[cd->bar] &&
				cd->x < blk->x[cd->bar] + blk->width[cd->bar]) {
			block_exec(blk, cd);
			break;
		}
	}
}
