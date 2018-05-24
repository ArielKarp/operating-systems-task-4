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
    int inum = (int)num;
    if (num == (float)inum) {
        return inum;
    }
    return inum + 1;
}

void xorBuffers(char* out_buff, char* in_buff, int size_of_in_buf) {
	// out_buff is always bigger than in_buff
	for (int i = 0; i < size_of_in_buf; i++) {
		out_buff[i] = (char) out_buff[i] ^ in_buff[i];
	}
}

int findNextThread(threadStatus* threadsStatus) {
	printf("Looping over threadsStatus...\n");
	for (int i = 0; i < nThreads; i++) {
		if (threadsStatus[i].reads_done < threadsStatus[i].total_reads) { // still active
			if (!threadsStatus[i].finished_current_step) { // did not finish i-th step
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

void* thread_reader(void* arg) {
	printf("Starting thread...\n");
	// get thread id
	int id = ((threadInfo*) arg)->id;
	char* file_name = ((threadInfo*) arg)->input_file;
	int out_fd = ((threadInfo*) arg)->write_fd;

	printf("Thread id is [%d]...\n", id);
	printf("Thread [%d]: file name is [%s]...\n", id, file_name);
	printf("Thread [%d]: out (write) fd is [%d]...\n", id, out_fd);
	// create temp buffer
	char reader_buffer[BUF_SIZE];

	printf("Thread [%d]: opening file...\n", id);
	// open to read
	int reader_fd = open(file_name, O_RDONLY);

	if (reader_fd < 0) {
		// TODO: handle this!
	}

	printf("Thread [%d]: starting main loop...\n", id);
	ssize_t len_in;
	while ((len_in = read(reader_fd, reader_buffer, BUF_SIZE)) > 0) { // have more to read
		printf("Thread [%d]: done read...\n", id);
		// read was successful
		pthread_mutex_lock(&mutex);  // lock mutex
		printf("Thread [%d]: lock mutex...\n", id);
		printf("Thread [%d]: wait for signal...\n", id);
		while (current_turn != id) {
			pthread_cond_wait(&cond[id], &mutex);  // wait for signal
		}

		printf("Thread [%d]:got signal, mutex locked...\n", id);

		// signaled, mutex is locked

		// xor the data in (byte by byte)
		printf("Thread [%d]: xoring buffer...\n", id);
		xorBuffers(write_buffer, reader_buffer, len_in);
		// update my status
		printf("Thread [%d]: updating status and buffer size...\n", id);
		threadsStatusArray[id].finished_current_step = 1; // finished
		threadsStatusArray[id].reads_done++;
		// update buffer size
		if (len_in > current_size_of_buffer) {
			current_size_of_buffer = len_in;
		}
		// check who to wake up next
		printf("Thread [%d]: find next thread...\n", id);
		int current_step = findNextThread(threadsStatusArray);
		printf("Thread [%d]: next thread (current step) is: [%d]...\n", id, current_step);
		//if i am last one- write to file and reset counter
		if (current_step == -1) {
			printf("Thread [%d]: finish current step...\n", id);
			ssize_t len_out;
			// finished i-th step, write buffer to file
			finishedStep(threadsStatusArray);
			// write to output
			printf("Thread [%d]: writing to output [%d] bytes...\n", id, current_size_of_buffer);
			len_out = write(out_fd, write_buffer, current_size_of_buffer);
			// clear write buffer
			printf("Thread [%d]: clearing write buffer...\n", id);
			current_size_of_buffer = 0;
			memset(write_buffer, 0, sizeof(write_buffer));
		}

		int wake_next =
				current_step == -1 ?
						findNextThread(threadsStatusArray) : current_step;

		printf("Thread [%d]: going to singal or exit, but first print...\n", id);
		//printThreadsStatus();
		if (wake_next != -1) { // not finished all
			current_turn = wake_next;
			pthread_cond_signal(&cond[wake_next]);
		}
		pthread_mutex_unlock(&mutex);
		// reset
	}

	if (len_in < 0) {
		// error reading
		// TODO: handle this
	}

	if (len_in == 0) {
		// finished reading
		// TODO: handle this
	}

	printf("Thread [%d]: exiting...\n", id);
	// free memory and exit
	close(reader_fd);
	free((threadInfo*) arg);
	pthread_exit(NULL); // TODO: maybe change this?

}

int handle_error_exit(const char* error_msg) {
	int errsv = errno;
	printf("Error message: [%s] | ERRNO: [%s]\n", error_msg, strerror(errsv));
	if (errsv == 0) {
		// in case error did not change errno
		return EXIT_FAILURE;
	}
	return errsv;
}

int main(int argc, char** argv) {
	printf("Starting...\n");
	pthread_attr_t attr;
	// check input:
	// 1) input files are ok (can open, etc.)

	// open output file with trunc
	printf("Opening output file...\n");
	int write_fd = open(argv[1], O_TRUNC | O_WRONLY);
	printf("Wrie fd is: [%d]\n", write_fd);
	// check open
	if (write_fd < 1) {
		return handle_error_exit("Error opening file");
	}
	printf("Finished opening output file...\n");
	// check number of threads
	nThreads = argc - 2;

	printf("nThreads is: [%d]...\n", nThreads);
	printf("Init and allocate...\n");
	// init mutex and attr
	pthread_mutex_init(&mutex, NULL);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	// allocate threads, cond and arrays
	pthread_t* threads = malloc(nThreads * sizeof(pthread_t));
	cond = malloc(nThreads * sizeof(pthread_cond_t));
	//done_threads = calloc(nThreads, sizeof(int));
	threadsStatusArray = calloc(nThreads, sizeof(threadStatus));

	// TODO: check allocations
	printf("Finished allocating...\n");

	printf("Clearing buffer...\n");
	// clear write buffer
	memset(write_buffer, 0, sizeof(write_buffer));

	printf("Set max read...\n");
	// set max number of reads
	for (int i = 0; i < argc - OFFSET; i++) {
		struct stat file_stat;
		int file_size;
		float temp_reads;
		// calculate number of reads
		if (stat(argv[i + OFFSET], &file_stat) == -1) {
			return handle_error_exit("Failed to retrieve stat data");
		}
		file_size = file_stat.st_size;
		temp_reads = file_size / BUF_SIZE;
		threadsStatusArray[i].total_reads = my_ceil(temp_reads);
		printf("File [%d] has [%d] reads...\n", i, threadsStatusArray[i].total_reads);
	}

	// loop over and init cond
	for (int i = 0; i < nThreads; i++) {
		pthread_cond_init(&cond[i], NULL);
	}

	printf("Starting threads...\n");
	// start threads
	for (int i = 0; i < nThreads; i++) {
		// create info struct for thread
		threadInfo* current_info = malloc(sizeof(threadInfo));
		current_info->id = i;
		current_info->write_fd = write_fd;
		current_info->input_file = malloc(
				(strlen(argv[i + OFFSET]) + 1) * sizeof(char));
		strcpy(current_info->input_file, argv[i + OFFSET]);
		// start thread
		pthread_create(&threads[i], &attr, thread_reader, (void*) current_info);
	}

	printf("Signal first thread...\n");
	// signal first thread
	sleep(1);
	current_turn = 0;
	pthread_cond_signal(&cond[0]); // TODO: maybe use other method of waking the cycle

	printf("Finished signal...\n");
	printf("Waiting on join...\n");
	// join threads
	for (int i = 0; i < nThreads; i++) {
		pthread_join(threads[i], NULL);
	}

	printf("Clearing up...\n");
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

	// TODO: pthread_exit(NULL); ?
	return EXIT_SUCCESS;
}

