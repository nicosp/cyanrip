/*
 * This file is part of cyanrip.
 *
 * cyanrip is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * cyanrip is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with cyanrip; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once

#include <cyanrip_main.h>

int crip_fill_discid(cyanrip_ctx *ctx);

/* Builds the MusicBrainz disc TOC string: "first+last+leadout+off1+off2+...",
 * '+'-joined so it can be dropped straight into a URL query value
 * (musicbrainz.org decodes '+' as a space there), from the TOC the disc IDs
 * were computed from. NULL if crip_fill_discid() hasn't computed them.
 * Free the result with av_free(). */
char *crip_disc_toc_string(cyanrip_ctx *ctx);
