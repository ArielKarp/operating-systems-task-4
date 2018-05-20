/*
 * no_threads.c
 *
 *  Created on: May 20, 2018
 *      Author: ariel
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>

#define BUF_SIZE 1024000
#define OFFSET 2

char write_buffer[BUF_SIZE];
int current_size_of_buffer = 0;

typedef struct {
	int number_of_reads;
	int total_reads;
	int finished;
} fileSt;

void xorBuffers(char* out_buff, char* in_buff, int size_of_in_buf) {
	// out_buff is always bigger than in_buff
	for (int i = 0; i < size_of_in_buf; i++) {
		out_buff[i] = (char) out_buff[i] ^ in_buff[i];
	}
}

int main(int argc, char** argv) {

	// open output in TRUNC
	int write_fd = open(argv[1], O_TRUNC | O_CREAT | O_WRONLY);
	printf("Opened file\n");

	// clear write buffer
	memset(write_buffer, 0, sizeof(write_buffer));
	printf("reset buffer\n");

	int read_bytes = 0;
	int* fd = calloc(argc - OFFSET, sizeof(int));

	printf("Creating fd array\n");

	for (int i = 0; i < argc - OFFSET; i++) {
		fd[i] = open(argv[i + OFFSET], O_RDONLY);
	}

	printf("done creating fd array\n");

	while (read_bytes >= 0) {
		printf("inside loop\n");
		read_bytes = 0;
		for (int i = 0; i < argc - OFFSET; i++) {
			printf("goin over file %d\n", i);
			char cur_buf[BUF_SIZE];
			printf("reading..\n");
			int read_len = read(fd[i], cur_buf, BUF_SIZE);
			if (read_len == 0) {
				continue;
			}
			if (read_len > read_bytes) {
				read_bytes = read_len;
			}
			printf("xoring...\n");
			xorBuffers(write_buffer, cur_buf, read_len);
		}
		if (read_bytes > 0) {
			printf("writing...\n");
			write(write_fd, write_buffer, read_bytes);
		} else {
			read_bytes = -1;
		}
	}

	for (int i = 0; i < argc - OFFSET; i++) {
		close(fd[i]);
	}

	close(write_fd);



}
