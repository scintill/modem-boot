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

#ifndef __SAHARA_PROTOCOL_H__
#define __SAHARA_PROTOCOL_H__

/* 
 * Most of these details of the SAHARA protocol were retrieved from the
 * libopenpst library (https://github.com/openpst/libopenpst).
 */

//modes
#define SAH_MODE_TRANSFER_PENDING  0x00
#define SAH_MODE_TRANSFER_COMPLETE 0x01

// commands
#define SAH_COMMAND_HELLO_REQ     0x01
#define SAH_COMMAND_HELLO_RESP    0x02
#define SAH_COMMAND_DATA_REQ      0x03
#define SAH_COMMAND_DATA_END_REQ  0x04
#define SAH_COMMAND_DATA_END_RESP 0x05
#define SAH_COMMAND_DATA_END_ACK  0x06

struct sah_header {
	unsigned int command;
	unsigned int packet_size;
} __attribute__((__packed__));

struct sah_hello_req {
	struct sah_header header;
	unsigned int version;
	unsigned int min_version;
	unsigned int not_needed;
	unsigned int mode;
	unsigned int appended[6];
} __attribute__((__packed__));

struct sah_hello_resp {
	struct sah_header header;
	unsigned int version;
	unsigned int min_version;
	unsigned int status;
	unsigned int mode;
	unsigned int appended[6];
} __attribute__((__packed__));

struct sah_data_req {
	unsigned int id;
	unsigned int offset;
	unsigned int size;
} __attribute__((__packed__));

struct sah_data_end_req {
	unsigned int id;
	unsigned int status;
} __attribute__((__packed__));

struct sah_data_end_ack {
	struct sah_header header;
	unsigned int status; // 0 more file requests to come, 1 complete
} __attribute__((__packed__));

#endif
