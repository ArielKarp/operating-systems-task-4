/*
 ============================================================================
 Name        : os_task_4.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct {
	int finished_current_step;
	int reads_done;
	int total_reads;
} threadStatus;

threadStatus* stupid = NULL;
int number = 3;

void printThreadsStatus() {
	for (int i = 0; i < number; i++) {
		printf(
				"Thread [%d] with number of reads: [%d]... , with total of reads [%d]...\n",
				i, stupid[i].reads_done, stupid[i].total_reads);
	}
}

void* thread_print(void* arg) {
	printf("Inside thread\n");
	printThreadsStatus();
	pthread_exit(NULL);
}

int main(void) {
	puts("!!!Hello World!!!"); /* prints !!!Hello World!!! */

	stupid = malloc(number * sizeof(threadStatus));

	for (int i = 0; i < number; ++i) {
		stupid[i].total_reads = 0;
		stupid[i].finished_current_step = 0;
		stupid[i].reads_done = 0;
	}

	printThreadsStatus();
	pthread_t threads[3];

	pthread_create(&threads[0], NULL, thread_print, NULL);
	pthread_create(&threads[1], NULL, thread_print, NULL);
	pthread_create(&threads[2], NULL, thread_print, NULL);

	for (int i = 0; i < number; i++) {
		pthread_join(threads[i], NULL);
	}

	printf("Finsihed\n");

	return EXIT_SUCCESS;
}
