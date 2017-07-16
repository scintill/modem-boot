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
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

#include <sahara.h>
#include <i9305.h>

int hello_handshake(int tty_fd)
{
	struct sah_hello_req hello_req;
	struct sah_hello_resp hello_resp;
	int rc;

	rc = read(tty_fd, &hello_req, sizeof(hello_req));
	if (rc < (int) sizeof(hello_req)
	    || hello_req.header.command != SAH_COMMAND_HELLO_REQ) {
		printf("error receiving hello\n");
		return -1;
	}

	if (hello_req.mode != SAH_MODE_TRANSFER_PENDING
	    && hello_req.mode != SAH_MODE_TRANSFER_COMPLETE) {
		printf("requested SAHARA mode %d is not supported\n", hello_req.mode);
		return -1;
	}

	hello_resp.header.command = SAH_COMMAND_HELLO_RESP;
	hello_resp.header.packet_size = sizeof(hello_resp);
	hello_resp.version = hello_req.version;
	hello_resp.min_version = hello_req.min_version;
	hello_resp.status = 0;
	hello_resp.mode = hello_req.mode;

	rc = write(tty_fd, &hello_resp, sizeof(hello_resp));
	if (rc < (int) sizeof(hello_resp)) {
		printf("failed to write hello response\n");
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

	rc = hello_handshake(tty_fd);
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

int main()
{
	int mdm_dev, tty_dev;
	struct termios termios;
	struct timeval timeout;
	fd_set fds;

	struct sah_data_end_ack data_end_ack;

	int rc;

	mdm_dev = open(MDM_DEVICE, O_RDONLY | O_NONBLOCK);
	if (mdm_dev < 0) {
		printf("failed to open modem device\n");
		return -1;
	}

	rc = ioctl(mdm_dev, WAKE_CHARM);
	if (rc < 0) {
		printf("failed to issue modem wake\n");
		return -1;
	}

	printf("modem wake issued\n");

	if (access(TTY_DEVICE, F_OK) == -1)
		// wait until modem is awake
		sleep(2);

	tty_dev = open(TTY_DEVICE, O_RDWR | O_SYNC);
	if (tty_dev < 0) {
		printf("failed to open modem tty device\n");
		return -1;
	}
	else {
		printf("opened modem tty device\n");
	}

	tcgetattr(tty_dev, &termios);
	cfmakeraw(&termios);
	cfsetispeed(&termios, B9600);
	cfsetospeed(&termios, B9600);
	tcsetattr(tty_dev, TCSANOW, &termios);

	FD_ZERO(&fds);
	FD_SET(tty_dev, &fds);
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;

	rc = select(tty_dev + 1, NULL, &fds, NULL, &timeout);
	if (rc <= 0) {
		printf("failed to set timeout\n");
		return -1;
	}

	do {
		rc = send_file(tty_dev, &data_end_ack);
		if (rc < 0) {
			printf("error while sending files\n");
			return -1;
		}
	} while (data_end_ack.status == 0);

	if(data_end_ack.status == 1) {
		printf("all files transferred\n");
	}

	close(tty_dev);

	rc = ioctl(mdm_dev, NORMAL_BOOT_DONE);
	if (rc < 0) {
		printf("failed to wait for normal boot\n");
		return -1;
	}

	printf("waited for normal boot\n");

	rc = ioctl(mdm_dev, WAIT_FOR_ERROR);
	if (rc < 0) {
		printf("error during boot\n");
		return -1;
	}

	printf("modem booted\n");

	sleep(2);
	if (access("/dev/cdc-wdm0", F_OK) != -1)
		printf("You can now use qmicli\n");
	else
		printf("error: cdc-wdm device does not exist\n");

	return 0;
}
