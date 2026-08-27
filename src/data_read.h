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

#include <stdint.h>
#include <cdio/cdio.h>

/**
 * Reads a single raw MODE1/2352 sector (sync + header + user data + EDC/ECC,
 * exactly as declared by a "MODE1/2352" CUE sheet FILE) from a CD device
 * into buf.
 *
 * The buffer must be large enough to hold CDIO_CD_FRAMESIZE_RAW bytes.
 */
driver_return_code_t cyanrip_read_data_sector(const CdIo_t *p_cdio, uint8_t *buf, const lsn_t lsn);
