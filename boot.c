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

int configure_tty(int *tty_fd, time_t timeout_sec, long int timeout_usec)
{
	int tty_dev;
	struct termios termios;
	struct timeval timeout;
	fd_set fds;
	int rc;

	tty_dev = open(TTY_DEVICE, O_RDWR | O_SYNC);
	if (tty_dev < 0) {
		printf("failed to open modem tty device\n");
		return -1;
	} else {
		printf("opened modem tty device\n");
	}

	tcgetattr(tty_dev, &termios);
	cfmakeraw(&termios);
	cfsetispeed(&termios, B9600);
	cfsetospeed(&termios, B9600);
	tcsetattr(tty_dev, TCSANOW, &termios);

	FD_ZERO(&fds);
	FD_SET(tty_dev, &fds);
	timeout.tv_sec = timeout_sec;
	timeout.tv_usec = timeout_usec;

	rc = select(tty_dev+1, NULL, &fds, NULL, &timeout);
	if (rc <= 0) {
		printf("failed to set timeout\n");
		return -1;
	}

	*tty_fd = tty_dev;

	return 0;
}

int main()
{
	int mdm_dev, tty_dev;
	int mode;
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

	rc = configure_tty(&tty_dev, 2, 0);
	if (rc < 0) {
		printf("failed to configure serial interface\n");
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
	} else {
		printf("unknown status for data end ack\n");
		return -1;
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

	printf("\nconfiguring EFS sync\n");

	rc = configure_tty(&tty_dev, 0, 500000);
	if (rc < 0) {
		printf("failed to configure serial interface\n");
	}

	while (1) {
		rc = handle_memory_debug(tty_dev);
		if (rc < 0) {
			printf("error during modem operation\n");
			return -1;
		}
	}

	return 0;
}
