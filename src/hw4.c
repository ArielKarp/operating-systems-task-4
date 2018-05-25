/*
 * hw4.c
 *
 *  Created on: May 19, 2018
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

typedef struct {
	int id;
	int write_fd;
	char* input_file;
} threadInfo;

typedef struct {
	int finished_current_step;
	int reads_done;
	int total_reads;
} threadStatus;

pthread_mutex_t mutex;
pthread_cond_t* cond = NULL;
int nThreads; // # of threads
threadStatus* threadsStatusArray = NULL;
char write_buffer[BUF_SIZE];
int current_size_of_buffer = 0;
int current_turn = -1;

int my_ceil(float num) {
	int inum = (int) num;
	if (num == (float) inum) {
		return inum;
	}
	return inum + 1;
}

void xorBuffers(char* out_buff, char* in_buff, int size_of_in_buf) {
	for (int i = 0; i < size_of_in_buf; i++) {
		out_buff[i] = (char) out_buff[i] ^ in_buff[i];
	}
}

int findNextThread(threadStatus* threadsStatus) {
	for (int i = 0; i < nThreads; i++) {
		if (threadsStatus[i].reads_done < threadsStatus[i].total_reads) { // still active
			if (!threadsStatus[i].finished_current_step) {  // did not finish i-th step
				return i;
			}
		}
	}
	return -1; // finished current step
}

void finishedStep(threadStatus* threadsStatus) {
	for (int i = 0; i < nThreads; i++) {
		if (threadsStatus[i].reads_done < threadsStatus[i].total_reads) { // still active
			threadsStatus[i].finished_current_step = 0;
		}
	}
}

int handle_error_exit(const char* error_msg) {
	int errsv = errno;
	printf("Error message: [%s] | ERRNO: [%s]\n", error_msg, strerror(errsv));
	if (errsv == 0) {
		// in case error did not change errno
		exit(EXIT_FAILURE);
	}
	exit(errsv);
}

void* thread_reader(void* arg) {
	// Get thread info
	int id = ((threadInfo*) arg)->id;
	char* file_name = ((threadInfo*) arg)->input_file;
	int out_fd = ((threadInfo*) arg)->write_fd;

	// create temp buffer
	char* reader_buffer = malloc(BUF_SIZE * sizeof(char));
	if (reader_buffer == NULL) {
		handle_error_exit("Failed to allocate readers' buffer array");
	}

	// open to read
	int reader_fd = open(file_name, O_RDONLY);

	if (reader_fd < 0) {
		handle_error_exit("Failed to open input file for read");
	}

	ssize_t len_in;
	while ((len_in = read(reader_fd, reader_buffer, BUF_SIZE)) > 0) { // have more to read
		// read was successful
		pthread_mutex_lock(&mutex);  // lock mutex
		while (current_turn != id) {
			pthread_cond_wait(&cond[id], &mutex);  // wait for signal
		}
		// signaled, mutex is locked
		// xor the data in (byte by byte)
		xorBuffers(write_buffer, reader_buffer, len_in);
		// update thread's status
		threadsStatusArray[id].finished_current_step = 1; // finished
		threadsStatusArray[id].reads_done++;
		// update buffer size
		if (len_in > current_size_of_buffer) {
			current_size_of_buffer = len_in;
		}
		// check who to wake up next
		int current_step = findNextThread(threadsStatusArray);
		//if thread is last one- write to file and reset flags
		if (current_step == -1) {
			ssize_t len_out;
			// finished i-th step, write buffer to file
			finishedStep(threadsStatusArray);
			// write to output
			len_out = write(out_fd, write_buffer, current_size_of_buffer);
			if (len_out < 0) {
				handle_error_exit("Failed to write to shared buffer");
			}
			// clear write buffer
			current_size_of_buffer = 0;
			memset(write_buffer, 0, sizeof(write_buffer));
		}

		int wake_next =
				current_step == -1 ?
						findNextThread(threadsStatusArray) : current_step;

		if (wake_next != -1) { // not finished all
			current_turn = wake_next;
			pthread_cond_signal(&cond[wake_next]);
		}
		pthread_mutex_unlock(&mutex);
	}

	if (len_in < 0) {
		handle_error_exit("Failed to read from input file");
	}

	// finished reading
	// free memory and exit
	if (close(reader_fd) < 0) {
		handle_error_exit("Failed to close readers' thread fd");
	}
	free((threadInfo*) arg);
	pthread_exit(NULL);
}

int main(int argc, char** argv) {

	printf("Hello, creating %s from %d input files\n", argv[1], argc - OFFSET);

	pthread_attr_t attr;

	// open output file with trunc
	int write_fd = open(argv[1], O_TRUNC | O_WRONLY | O_CREAT, 0666);

	// check open
	if (write_fd < 1) {
		handle_error_exit("Error opening output file");
	}

	// check number of threads
	nThreads = argc - OFFSET;

	// init mutex and attr
	pthread_mutex_init(&mutex, NULL);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	// allocate threads, cond and arrays
	pthread_t* threads = malloc(nThreads * sizeof(pthread_t));
	if (threads == NULL) {
		handle_error_exit("Failed to allocate pthread array");
	}
	cond = malloc(nThreads * sizeof(pthread_cond_t));
	if (cond == NULL) {
		handle_error_exit("Failed to allocate pthread_cond's array");
	}
	threadsStatusArray = calloc(nThreads, sizeof(threadStatus));
	if (threadsStatusArray == NULL) {
		handle_error_exit("Failed to allocate threadStatus array");
	}

	// clear write buffer
	memset(write_buffer, 0, sizeof(write_buffer));

	// set max number of reads
	for (int i = 0; i < argc - OFFSET; i++) {
		struct stat file_stat;
		int file_size;
		float temp_reads;
		// calculate number of reads
		if (stat(argv[i + OFFSET], &file_stat) == -1) {
			handle_error_exit("Failed to retrieve stat data");
		}
		file_size = file_stat.st_size;
		temp_reads = file_size / BUF_SIZE;
		threadsStatusArray[i].total_reads = my_ceil(temp_reads);
	}

	// loop over and init cond
	for (int i = 0; i < nThreads; i++) {
		pthread_cond_init(&cond[i], NULL);
	}

	// start threads
	for (int i = 0; i < nThreads; i++) {
		// create info struct for thread
		threadInfo* current_info = malloc(sizeof(threadInfo));
		if (current_info == NULL) {
			handle_error_exit("Failed to allocate threadInfo struct");
		}
		current_info->id = i;
		current_info->write_fd = write_fd;
		current_info->input_file = malloc(
				(strlen(argv[i + OFFSET]) + 1) * sizeof(char));
		if (current_info->input_file == NULL) {
			handle_error_exit("Failed to allocate threadInfos' input file char array");
		}
		strcpy(current_info->input_file, argv[i + OFFSET]);
		// start thread
		pthread_create(&threads[i], &attr, thread_reader, (void*) current_info);
	}

	// signal first thread
	sleep(1);
	current_turn = 0;
	pthread_cond_signal(&cond[0]); // TODO: maybe use other method of waking the cycle

	// join threads
	for (int i = 0; i < nThreads; i++) {
		pthread_join(threads[i], NULL);
	}

	//Clean up and exit
	close(write_fd);
	pthread_attr_destroy(&attr);
	pthread_mutex_destroy(&mutex);

	// loop over and destroy cond
	for (int i = 0; i < nThreads; i++) {
		pthread_cond_destroy(&cond[i]);
	}

	// free arrays
	free(threads);
	free(cond);
	free(threadsStatusArray);

	return EXIT_SUCCESS;
}

