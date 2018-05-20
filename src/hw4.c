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
int turn;  // current turn
int nThreads; // # of threads
int nWorkingThreads;  // # of working threads
//int* done_threads;
threadStatus* threadsStatus = NULL;
char write_buffer[BUF_SIZE];

void* thread_reader(void* arg) {
	// get thread id
	int id = ((threadInfo*)arg)->id;
	char* file_name = ((threadInfo*)arg)->input_file;
	int out_fd = ((threadInfo*)arg)->write_fd;

	// create temp buffer
	char reader_buffer[BUF_SIZE];

	// open to read
	int reader_fd = open(file_name, O_RDONLY);

	if (reader_fd < 0) {
		// TODO: handle this!
	}

	ssize_t len_in;
	ssize_t len_out;
	while ((len_in = read(reader_fd, reader_buffer, BUF_SIZE)) > 0) { // have more to read
		// read was successful
		pthread_mutex_lock(&mutex);  // lock mutex
		pthread_cond_wait(&cond[id], &mutex);  // wait for signal

		// signaled, mutex is locked

		// xor the data in (byte by byte)
		xorBuffers(write_buffer, reader_buffer, len);
		// update my status
		threadsStatus[id].finished_current_step = 1; // finished
		threadsStatus[id].reads_done++;
		// check who to wake up next
		int current_step = findNextThread(threadsStatus);
		//if i am last one- write to file and reset counter
		if (current_step == -1) {
			// finished i-th step, write buffer to file
			finishedStep(threadsStatus);
			// write to output
			len_out = write(out_fd, write_buffer, BUF_SIZE); // TODO: write correct size!
		}

		int wake_next = current_step == -1 ? findNextThread(threadsStatus) : current_step;

		if (wake_next != -1) { // not finished all
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

	// free memory and exit
	pthread_exit(NULL); // TODO: maybe change this?

}

void xorBuffers(char* out_buff, char* in_buff, int size_of_in_buf) {
	// out_buff is always bigger than in_buff
	for (int i = 0; i < size_of_in_buf; i++) {
		out_buff[i] = (char) out_buff[i] ^ in_buff[i];
	}
}

int findNextThread(threadStatus* threadsStatus) {
	for (int i = 0; i < nThreads; i++) {
		if (threadsStatus[i].reads_done < threadsStatus[i].total_reads) {  // still active
			if (!threadsStatus[i].finished_current_step) {  // did not finish i-th step
				return i;
			}
		}
	}
	return -1; // finished current step
}

void finishedStep(threadStatus* threadsStatus) {
	for (int i = 0; i < nThreads; i++) {
		if (threadsStatus[i].reads_done < threadsStatus[i].total_reads) {  // still active
			threadsStatus[i].finished_current_step = 0;
		}
	}
}

void fillTotakNumberOfReads(threadStatus* threadsStatus, char* input_file_names) {
	struct stat file_stat;
	int file_size;

}

int handle_error_exit(const char* error_msg) {
	int errsv = errno;
	release_resources();
	printf("Error message: [%s] | ERRNO: [%s]\n", error_msg, strerror(errsv));
	if (errsv == 0) {
		// in case error did not change errno
		return EXIT_FAILURE;
	}
	return errsv;
}


int main(int argc, char** argv) {
	pthread_attr_t attr;
	// check input:
	// 1) input files are ok (can open, etc.)

	// open output file with trunc
	int write_fd = open(argv[1], O_TRUNC);
	// check open
	if (write_fd < 1) {
		return handle_error_exit("Error opening file");
	}

	// check number of threads
	nThreads = argc -2;
	nWorkingThreads = nThreads;

	// init mutex and attr
	pthread_mutex_init(&mutex, NULL);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	// allocate threads, cond and arrays
	pthread_t* threads = malloc(nThreads * sizeof(pthread_t));
	cond = malloc(nThreads * sizeof(pthread_cond_t));
	//done_threads = calloc(nThreads, sizeof(int));
	threadsStatus = calloc(nThreads, sizeof(threadsStatus));

	// TODO: check allocations

	// clear write buffer
	memset(write_buffer, 0, sizeof(write_buffer));

	// loop over and init cond
	for (int i = 0; i < nThreads; i++) {
		pthread_cond_init(&cond[i], NULL);
	}

	// start threads
	for (int i = 0; i < nThreads; i++) {
		// create id for thread
		//int* id = malloc(sizeof(int));
		//id[0] = i;

		// create info struct for thread
		threadInfo* current_info = malloc(sizeof(threadInfo));
		current_info->id = i;
		current_info->write_fd = write_fd;
		current_info->input_file = malloc((strlen(argv[i + OFFSET]) + 1) * sizeof(char));
		strcpy(current_info->input_file, argv[i + OFFSET]);

		// start thread
		pthread_create(&threads[i], &atrr, thread_reader, (void*)current_info);
	}

	// signal first thread
	pthread_cond_signal(&cond[0]); // TODO: maybe use other method of waking the cycle

	// join threads
	for (int i = 0; i < nThreads; i++) {
		pthread_join(threads[i], NULL);
	}

	//Clean up and exit
	pthread_attr_destroy(&attr);
	pthread_mutex_destroy(&mutex);

	// loop over and destroy cond
	for (int i = 0; i < nThreads; i++) {
		pthread_cond_destroy(&cond[i]);
	}

	// free arrays
	free(threads);
	free(cond);
	free(done_threads);

	// TODO: pthread_exit(NULL); ?
	return EXIT_SUCCESS;
}

