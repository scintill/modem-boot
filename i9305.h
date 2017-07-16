/*
 * Copyright (C) 2017 Wolfgang Wiedmeyer <wolfgit@wiedmeyer.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DEVICE_I9305__
#define __DEVICE_I9305__

#define MDM_DEVICE "/dev/mdm"
#define TTY_DEVICE "/dev/ttyUSB0"

#define CHARM_CODE		0xCC
#define WAKE_CHARM		_IO(CHARM_CODE, 1)
#define NORMAL_BOOT_DONE	_IOW(CHARM_CODE, 5, int)
#define WAIT_FOR_ERROR		_IOW(CHARM_CODE, 12, int)

#define MAX_DATA_SEND_SIZE 1*1024*1024

// files to upload
#define FILE_APPS "/firmware/image/apps.mbn"
#define FILE_ACDB "/tombstones/qcks/acdb.bin"
#define FILE_DSP1 "/firmware/image/dsp1.mbn"
#define FILE_DSP2 "/firmware/image/dsp2.mbn"
#define FILE_DSP3 "/firmware/image/dsp3.mbn"
#define FILE_EFS1 "/tombstones/qcks/efs1.bin"
#define FILE_EFS2 "/tombstones/qcks/efs2.bin"
#define FILE_EFS3 "/tombstones/qcks/efs3.bin"
#define FILE_SBL1 "/firmware/image/sbl1.mbn"
#define FILE_SBL2 "/firmware/image/sbl2.mbn"
#define FILE_RPM  "/firmware/image/rpm.mbn"

#endif
