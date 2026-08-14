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

/* Looks up the disc on GNUdb (a CDDB-protocol CD database), the same
 * service (and default server, gnudb.gnudb.org) XLD and older EAC versions
 * use - see XLDCDDBUtil.m's use of libcddb. Enabled by supplying a contact
 * email via -e/--gnudb-email (matching XLD's "CDDB Mail" preference); the
 * server can be overridden via -g/--gnudb-host, e.g. for a personalized,
 * pre-registered host. GNUdb requires every app name to be pre-registered
 * with them (see gnudb.org/howtognudb.php) - the email alone isn't enough,
 * unlike XLD, which already has "XLD" approved. Until "cyanrip" is
 * approved too, requests will get a clear rejection logged rather than
 * silently doing nothing.
 *
 * On a hit, fills ctx->gnudb (album/album_artist/date/genre) and each
 * ctx->tracks[i].gnudb (title/artist), then copies those into ctx->meta /
 * ctx->tracks[i].meta as fallback tags for when MusicBrainz doesn't have
 * this disc, or is disabled entirely (-N). Never a hard error.
 *
 * Must be called after crip_fill_discid() (needs the CDDB disc ID) and
 * before crip_fill_metadata(), so a successful MusicBrainz lookup can still
 * overwrite these (lower priority) values. */
void crip_fill_gnudb(cyanrip_ctx *ctx);
