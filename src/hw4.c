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

pthread_mutex_t mutex;
pthread_cond_t* cond;
int turn;  // current turn
int nThreads; // # of threads
char write_buffer[BUF_SIZE];

void* thread_reader(void* arg) {

}



int main(int argc, char** argv) {
	pthread_attr_t attr;
	// check input:
	// 1) input files are ok (can open, etc.)

	// open output file with trunc
	// check open

	// check number of threads
	nThreads = argc -2;



	// init mutex and attr
	pthread_mutex_init(&mutex, NULL);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	// loop over and open readers
	pthread_t* threads = malloc(nThreads * sizeof(pthread_t));
	cond = malloc(nThreads * sizeof(pthread_cond_t));

	// init cond
	for (int i = 0; i < nThreads; i++) {
		pthread_cond_init(&cond[i], NULL);
	}

	// start threads





	return EXIT_SUCCESS;
}

