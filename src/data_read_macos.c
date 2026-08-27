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

#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>
#include <sys/errno.h>
#include <cdio/cdio.h>

static driver_return_code_t map_errno(void)
{
    /* Map the ioctl() failure to the closest driver_return_code_t, so that
     * callers can retry on errors that are actually transient, i.e. DRIVER_OP_ERROR.
     */
    switch (errno) {
        case EBADF:  /* fd is invalid, e.g. the device was already closed */
            return DRIVER_OP_UNINIT;
        case EINVAL: /* Invalid argument, e.g. bad offset/buffer length */
            return DRIVER_OP_BAD_PARAMETER;
        case ENOTTY: /* DKIOCCDREAD is not supported on this fd/device */
            return DRIVER_OP_UNSUPPORTED;
        default:     /* Most likely a transient read error (e.g. EIO), retryable */
            return DRIVER_OP_ERROR;
    }
}

driver_return_code_t cyanrip_read_data_sector(const CdIo_t *p_cdio, uint8_t *buf, const lsn_t lsn)
{
    const int fd = cdio_get_device_fd((CdIo_t *)p_cdio);
    if (fd < 0) {
        return DRIVER_OP_ERROR;
    }

    const unsigned block_size = CDIO_CD_FRAMESIZE_RAW;
    const unsigned sync_header_size = 12 + 4; /* sync + the 4-byte sector header */

    /* Some drives silently return an all-zero user data field whenever
     * Sync is requested together with User Data in the same read, without
     * signalling an error (observed on Linux with an Optiarc AD-7740H via
     * the generic MMC driver). Split the sector into two reads - sync +
     * header, then user data + auxiliary (EDC/ECC) - to sidestep that;
     * this works everywhere else too, at the cost of one extra ioctl. */

    dk_cd_read_t cd_read_sync_header = {
        .offset = (uint64_t)block_size*lsn,
        .sectorArea = kCDSectorAreaSync | kCDSectorAreaHeader,
        .sectorType = kCDSectorTypeMode1,
        .bufferLength = sync_header_size,
        .buffer = buf,
    };
    if (ioctl(fd, DKIOCCDREAD, &cd_read_sync_header) < 0)
        return map_errno();

    dk_cd_read_t cd_read_user_aux = {
        .offset = (uint64_t)block_size*lsn,
        .sectorArea = kCDSectorAreaUser | kCDSectorAreaAuxiliary,
        .sectorType = kCDSectorTypeMode1,
        .bufferLength = block_size - sync_header_size,
        .buffer = buf + sync_header_size,
    };
    if (ioctl(fd, DKIOCCDREAD, &cd_read_user_aux) < 0)
        return map_errno();

    return DRIVER_OP_SUCCESS;
}
