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

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <sahara.h>
#include <i9305.h>

int check_mode(int mode_recv, int mode_expected)
{
	if ((mode_expected == SAH_MODE_TRANSFER_PENDING
	     || mode_expected == SAH_MODE_TRANSFER_COMPLETE)
	    && (mode_recv == SAH_MODE_TRANSFER_PENDING
		|| mode_recv == SAH_MODE_TRANSFER_COMPLETE))
		return 0;
	else if (mode_recv == mode_expected)
		return 0;
	else
		return -1;
}

int read_hello_data(int tty_fd, int mode, struct sah_hello_req *hello_req)
{
	int rc;

	rc = read(tty_fd, hello_req, sizeof(*hello_req));
	if (rc < (int) sizeof(hello_req)) {
		printf("error receiving hello, wrong packet size\n");
		return -1;
	}

	rc = check_mode(hello_req->mode, mode);
	if (rc < 0) {
		printf("mode %d is not the expected mode %d\n",
		       hello_req->mode, mode);
		return -1;
	}

	return 0;
}

int hello_response(int tty_fd, int mode)
{
	struct sah_hello_req hello_req;
	struct sah_hello_resp hello_resp;
	int rc;

	rc = read_hello_data(tty_fd, mode, &hello_req);
	if (rc < 0) {
		printf("error receiving hello data\n");
		return -1;
	}

	hello_resp.header.command = SAH_COMMAND_HELLO_RESP;
	hello_resp.header.packet_size = sizeof(hello_resp);
	hello_resp.version = hello_req.version;
	hello_resp.min_version = hello_req.min_version;
	hello_resp.status = 0;
	hello_resp.mode = hello_req.mode;
	memcpy(hello_resp.appended, hello_req.appended, sizeof(hello_req.appended));

	rc = write(tty_fd, &hello_resp, sizeof(hello_resp));
	if (rc < (int) sizeof(hello_resp)) {
		printf("failed to write hello response\n");
		return -1;
	}

	return 0;
}

int hello_handshake(int tty_fd, int mode)
{
	struct sah_header header;
	int rc;

	rc = read(tty_fd, &header, sizeof(header));
	if (rc < (int) sizeof(header)) {
		printf("failed to receive header for hello request\n");
		return -1;
	} else if (header.command != SAH_COMMAND_HELLO_REQ) {
		printf("error receiving hello, received command %d\n",
		       header.command);
		return -1;
	}

	rc = hello_response(tty_fd, mode);
	if (rc < 0) {
		printf("failed to send hello response\n");
		return -1;
	}

	return 0;
}

int file_for_id(unsigned int id, char **file)
{
	switch (id) {
	case 6:
		*file = FILE_APPS;
		break;
	case 8:
		*file = FILE_DSP1;
		break;
	case 12:
		*file = FILE_DSP2;
		break;
	case 16:
		*file = FILE_EFS1;
		break;
	case 17:
		*file = FILE_EFS2;
		break;
	case 20:
		*file = FILE_EFS3;
		break;
	case 21:
		*file = FILE_SBL1;
		break;
	case 22:
		*file = FILE_SBL2;
		break;
	case 23:
		*file = FILE_RPM;
		break;
	case 28:
		*file = FILE_DSP3;
		break;
	case 29:
		*file = FILE_ACDB;
		break;
	default:
		return -1;
	}

	return 0;
}

int send_data(int tty_fd, struct sah_header *header)
{
	struct sah_data_req data_req;
	char *file = NULL;
	int file_fd;
	char file_data[MAX_DATA_SEND_SIZE];
	int rc;

	rc = read(tty_fd, header, sizeof(*header));
	if (rc < (int) sizeof(header)) {
		printf("failed to receive header for data request\n");
		return -1;
	}

	if (header->command == SAH_COMMAND_DATA_END_REQ)
		return 1;

	rc = read(tty_fd, &data_req, sizeof(data_req));
	if (rc < (int) sizeof(data_req)) {
		printf("error receiving data req\n");
		return -1;
	}

	if(data_req.size > MAX_DATA_SEND_SIZE) {
		printf("requested data size is too big\n");
		return -1;
	}

	rc = file_for_id(data_req.id, &file);
	if (rc < 0) {
		printf("failed to identify requested file\n");
		return -1;
	}

	file_fd = open(file, O_RDONLY);
	if (file_fd < 0) {
		printf("failed to open requested file\n");
		return -1;
	}

	lseek(file_fd, data_req.offset, SEEK_SET);

	rc = read(file_fd, &file_data, data_req.size);
	if (rc < (int) data_req.size) {
		printf("failed to read data from file\n");
		return -1;
	}

	write(tty_fd, &file_data, data_req.size);
	if (rc < (int) data_req.size) {
		printf("failed to send data\n");
	}

	return 0;
}

int send_file(int tty_fd, struct sah_data_end_ack *data_end_ack)
{
	struct sah_header header;
	struct sah_data_end_req data_end_req;
	int rc;

	rc = hello_handshake(tty_fd, SAH_MODE_TRANSFER_PENDING);
	if (rc < 0) {
		printf("failed to do hello handshake\n");
		return -1;
	}

	do {
		rc = send_data(tty_fd, &header);
		if (rc < 0) {
			printf("error while sending data\n");
			return -1;
		}
	} while (header.command == SAH_COMMAND_DATA_REQ);

	rc = read(tty_fd, &data_end_req, sizeof(data_end_req));
	if (rc < (int) sizeof(data_end_req)) {
		printf("error receiving data end request\n");
		return -1;
	}

	if (data_end_req.status != 0) {
		printf("file transfer end status is nonzero\n");
		return -1;
	}

	printf("id %d: file transfer complete\n", data_end_req.id);

	header.command = SAH_COMMAND_DATA_END_RESP;
	header.packet_size = 8;
	rc = write(tty_fd, &header, sizeof(header));
	if (rc < (int) sizeof(header)) {
		printf("failed to write data end response\n");
		return -1;
	}

	rc = read(tty_fd, data_end_ack, sizeof(*data_end_ack));
	if (rc < (int) sizeof(*data_end_ack)
	    || data_end_ack->header.command != SAH_COMMAND_DATA_END_ACK) {
		printf("error receiving data end ack\n");
		return -1;
	}

	return 0;
}

int check_efs_file_request(unsigned char name[20])
{
	unsigned char efs1[20] = SYNC_EFS1;
	unsigned char efs2[20] = SYNC_EFS2;

	if(memcmp(name, efs1, 20) == 0 || memcmp(name, efs2, 20) == 0)
		return 0;
	else
		return -1;
}

int request_efs_data(int tty_fd,
		     struct sah_memory_read_req memory_read_req)
{
	char chunk_data[MAX_MEMORY_CHUNK_SIZE];
	char file_data[MAX_DATA_SEND_SIZE];
	unsigned int size_read = 0;
	fd_set fds;
	struct timeval timeout;
	int size;
	int rc;

	rc = write(tty_fd, &memory_read_req, sizeof(memory_read_req));
	if (rc < (int) sizeof(memory_read_req)) {
		printf("failed to send memory read request for table\n");
		return -1;
	}

	FD_ZERO(&fds);
	FD_SET(tty_fd, &fds);

	while (size_read < memory_read_req.size) {
		if (memory_read_req.size - size_read > MAX_MEMORY_CHUNK_SIZE)
			size = MAX_MEMORY_CHUNK_SIZE;
		else
			size = memory_read_req.size - size_read;

		timeout.tv_sec = 0;
		timeout.tv_usec = 500000;
		rc = select(tty_fd+1, &fds, NULL, NULL, &timeout);
		if (rc <= 0) {
			printf("failed to set timeout\n");
			return -1;
		}
		rc = read(tty_fd, &chunk_data, size);
		if (rc < 1) {
			printf("failed to read memory table data\n");
			printf("received %d out of %d\n", rc, size);
			printf("%d of %d read so far\n", size_read,
			       memory_read_req.size);
			return -1;
		}

		memcpy(&file_data[size_read], chunk_data, rc);
		size_read += rc;
	}

	printf("successfully received EFS data\n");
	printf("TODO: implement writing data to EFS partition\n");

	return 0;
}

int efs_sync(int tty_fd)
{
	struct sah_memory_debug_req memory_debug_req;
	struct sah_memory_read_req memory_read_req;
	struct sah_memory_table memory_table;
	struct sah_header header;
	int i = 0;
	int rc;

	rc = read(tty_fd, &memory_debug_req, sizeof(memory_debug_req));
	if (rc < (int) sizeof(memory_debug_req)) {
		printf("failed to receive memory debug data\n");
		return -1;
	}

	memory_read_req.header.command = SAH_COMMAND_MEMORY_READ_REQ;
	memory_read_req.header.packet_size = 16;
	memory_read_req.address = memory_debug_req.address;
	memory_read_req.size = memory_debug_req.size;
	rc = write(tty_fd, &memory_read_req, sizeof(memory_read_req));
	if (rc < (int) sizeof(memory_read_req)) {
		printf("failed to send initial memory read request\n");
		return -1;
	}

	rc = read(tty_fd, &memory_table, sizeof(memory_table));
	if (rc < (int) sizeof(memory_table)) {
		printf("failed to receive memory table\n");
		return -1;
	}

	printf("requested file %s with address %d and size %d\n",
	       memory_table.file, memory_table.address,
	       memory_table.size);

	rc = check_efs_file_request(memory_table.file);
	if (rc < 0) {
		printf("access to requested file is not allowed\n");
		return -1;
	}

	if( memory_table.size > MAX_DATA_SEND_SIZE) {
		printf("requested memory table size is too big\n");
		return -1;
	}

	memory_read_req.address = memory_table.address;
	memory_read_req.size = memory_table.size;
	do {
		rc = request_efs_data(tty_fd, memory_read_req);
		// abort after 5 retries
		if (i++ > 4 && rc < 0)
			break;
	} while (rc < 0);

	header.command = SAH_COMMAND_RESET_REQ;
	header.packet_size = 8;
	rc = write(tty_fd, &header, sizeof(header));
	if (rc < (int) sizeof(header)) {
		printf("failed to send reset request\n");
		return -1;
	}

	rc = read(tty_fd, &header, sizeof(header));
	if (rc < (int) sizeof(header)) {
		printf("error receiving reset response\n");
		return -1;
	} else if (header.command != SAH_COMMAND_RESET_RESP) {
		printf("received command %d instead of reset response\n",
		       header.command);
		return -1;
	}

	return 0;
}

int handle_memory_debug(int tty_fd, int *hellos)
{
	struct sah_header header;
	struct sah_hello_req hello_req;
	int rc;

	rc = read(tty_fd, &header, sizeof(header));
	if (rc < (int) sizeof(header)) {
		printf("failed to receive header for command\n");
		return -1;
	}

	switch (header.command) {
	case SAH_COMMAND_HELLO_REQ:
		printf("received hello\n");
		// Modem doesn't like it if the second hello is
		// answered, so only read data and keep quiet.
		if (++(*hellos) == 2) {
			rc = read_hello_data(tty_fd,
					     SAH_MODE_MEMORY_DEBUG,
					     &hello_req);
			if (rc < 0) {
				printf("failed to read hello data\n");
				return -1;
			}
		} else {
			rc = hello_response(tty_fd,
					    SAH_MODE_MEMORY_DEBUG);
			if (rc < 0) {
				printf("failed to send hello response\n");
				return -1;
			}
			printf("sent hello response\n");
		}
		break;
	case SAH_COMMAND_MEMORY_DEBUG_REQ:
		printf("received memory debug command\n");
		rc = efs_sync(tty_fd);
		if (rc < 0) {
			printf("failed to receive EFS data\n");
			return -1;
		}
		printf("finished EFS sync\n");
		break;
	default:
		printf("received unknown command %d with size %d\n",
		       header.command, header.packet_size);
		return -1;
	}

	return 0;
}
