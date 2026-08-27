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

#include "data_read.h"

#include <cdio/mmc_ll_cmds.h>

/* Restricting the read to "Mode 1 sectors only" is rejected outright by
 * some drives for sectors in a data session following an audio session,
 * so every read below asks for any sector type instead; the field
 * selectors pin the returned layout regardless. */
#define ANY_SECTOR_TYPE 0

driver_return_code_t cyanrip_read_data_sector(const CdIo_t *p_cdio, uint8_t *buf, const lsn_t lsn)
{
    driver_return_code_t drc;

    /* Some drives (observed on an Optiarc AD-7740H) silently return an
     * all-zero user data field whenever Sync is requested together with
     * User Data in the same READ CD command, without signalling an error.
     * Splitting the sector into two reads - sync+header, then user
     * data+EDC/ECC - sidesteps that and works everywhere else too, at the
     * cost of an extra command per sector. */

    /* Sync (12 bytes) + the 4-byte sector header */
    drc = mmc_read_cd(p_cdio, buf, lsn, ANY_SECTOR_TYPE,
        false, /* n/a outside of CD-DA */
        true,  /* return the sync bytes */
        1,     /* return the 4-byte sector header */
        false, /* user data comes from the second read */
        false,
        0,     /* no C2 error info */
        0,     /* no subchannel data */
        CDIO_CD_SYNC_SIZE + CDIO_CD_HEADER_SIZE, 1);
    if (drc != DRIVER_OP_SUCCESS)
        return drc;

    /* User data (2048 bytes) + EDC/ECC (288 bytes) */
    return mmc_read_cd(p_cdio, buf + CDIO_CD_SYNC_SIZE + CDIO_CD_HEADER_SIZE, lsn,
        ANY_SECTOR_TYPE,
        false,
        false, /* sync already read above */
        0,     /* header already read above */
        true,  /* return the user data */
        true,  /* return the EDC/ECC bytes */
        0,
        0,
        CDIO_CD_FRAMESIZE_RAW0, 1);
}
