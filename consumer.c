

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>


#include "shm.h"
#include "sem.h"

#define FILE_OUT "TEST_OUT.txt"



// Semaphores


int set_semvalue(int sem_id, int val)
{
    union semun sem_union;

    sem_union.val = val;
    if (semctl(sem_id, 0, SETVAL, sem_union) == -1) return(0);
    return(1);
}


void del_semvalue(int sem_id)
{
    union semun sem_union;
    
    if (semctl(sem_id, 0, IPC_RMID, sem_union) == -1)
        fprintf(stderr, "Failed to delete semaphore\n");
}


int semaphore_p(int sem_id)
{
    struct sembuf sem_b;
    
    sem_b.sem_num = 0;
    sem_b.sem_op = -1; /* wait */
    sem_b.sem_flg = 0;
    if (semop(sem_id, &sem_b, 1) == -1) {
        fprintf(stderr, "semaphore_wait failed\n");
        return(0);
    }
    return(1);
}
int semaphore_v(int sem_id)
{
    struct sembuf sem_b;
    
    sem_b.sem_num = 0;
    sem_b.sem_op = 1; /* signal */
    sem_b.sem_flg = 0;
    if (semop(sem_id, &sem_b, 1) == -1) {
        fprintf(stderr, "semaphore_signal failed\n");
        return(0);
    }
    return(1);
}


// Shared memory

int circular_memory(int shm_key, int *shmid, void **shared_memory)
{
	*shmid = shmget((key_t)shm_key, sizeof(struct buffer)*BUFFERS_SIZE, 0666 | IPC_CREAT);
	if (*shmid == -1) {
		fprintf(stderr, "shmget failed\n");
		return -1;
	}

	
	*shared_memory = shmat(*shmid, (void *)0, 0);
	if (*shared_memory == (void *)-1) {
		fprintf(stderr, "shmat failed\n");
		return -1;
	}
	return 0;
}


//main

int main(void)
{
	int shmid;
	void *shared_memory= (void *)0;
	
 	struct buffer *shared_buffer;
 	
 	int buffer_index;
  
  int S, N, E;

 
  int file_out;
  int total_written; 	

	
	
	if (circular_memory(SHM_KEY, &shmid, &shared_memory) == -1) {
		exit(EXIT_FAILURE);
	}
	printf("Shared Memory attached at: %p.", (int*)shared_memory);	
	shared_buffer = (struct buffer*) shared_memory;			
	buffer_index = 0;

	S = semget((key_t)S_KEY, 1, 0666 | IPC_CREAT);
	N = semget((key_t)N_KEY, 1, 0666 | IPC_CREAT);
	E = semget((key_t)E_KEY, 1, 0666 | IPC_CREAT);


  open_file_to_write(&file_out);
  
  printf("Waiting for producer\n");
  semaphore_p(N);
  printf("File size: %d bytes\n", shared_buffer->total_byte_count);


  printf("Reading and writing from shared memory\n");
  total_written = 0;
	while (total_written < shared_buffer->total_byte_count) {
		int bytes_written=0;
		semaphore_p(N);
		semaphore_p(S);
		bytes_written += copy_to_file(file_out, shared_buffer, &buffer_index);
		total_written += bytes_written;
		semaphore_v(S);
		semaphore_v(E);
		printf("Bytes written to file: %d\n", bytes_written);
	}

	
	printf("Total Bytes written: %d\n",total_written);


	
	if (close(file_out) == -1) {
		fprintf(stderr, "File close failed\n");
	}

  if (shmdt(shared_memory) == -1) {
    fprintf(stderr, "shmdt failed\n");
    exit(EXIT_FAILURE);
  }
  if (shmctl(shmid, IPC_RMID, 0) == -1) {
    fprintf(stderr, "shmctl(IPC_RMID) failed\n");
    exit(EXIT_FAILURE);
  }



  del_semvalue(S);
  del_semvalue(N);
  del_semvalue(E);

	exit(EXIT_SUCCESS);
}

void open_file_to_write(int *file_out)
{
	

	*file_out = open(FILE_OUT, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
	if (*file_out == -1) {
		fprintf(stderr, "file open failed");
		exit(EXIT_FAILURE);
	}
	}

int copy_to_file(int file_out, struct buffer *shared_buffer, int *buffer_index)
{
	int num_bytes_W = 0;

	num_bytes_W = write(file_out, shared_buffer[*buffer_index].string_data, shared_buffer[*buffer_index].bytes_to_count);	
	if (num_bytes_W == -1) {
		fprintf(stderr, "file write failed ");
		exit(EXIT_FAILURE);
	}
	*buffer_index = ((*buffer_index)+1)%BUFFERS_SIZE;

	return num_bytes_W;
}

