#!/usr/bin/python

import os

NUM_OF_FILES = 10
SIZE_OF_FILE = 1024000
TIMES = 1

if __name__ == '__main__':

	file_prefix = 'in_file'
	for index in range(NUM_OF_FILES):
		with open(file_prefix + str(index), 'wb') as fout:
			fout.write(os.urandom(SIZE_OF_FILE * TIMES))
