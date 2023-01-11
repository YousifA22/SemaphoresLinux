
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
#include <sys/stat.h>

#include "shm.h"
#include "sem.h"

#define FILE_IN "TEST_IN.txt"

int write_to_shared_memory(struct buffer *shared_buffer_head, int *buffer_index, char *input_buffer, int ncopied, int nread);

//Semaphores

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

	// Make the shared memory accessible by the program
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
	void *shared_memory1 = (void *)0;

 	struct buffer *shared_buffer_head;
 	
 	int buffer_index;  

	int S, N, E;

	
	int file_in;
	int file_size;
	struct stat st;  
  
  
  char input_buffer[BUFSIZ];

  int num_bytes_to_read, bytes_W_shm, total_bytes_shm;


 
 	if (circular_memory(SHM_KEY, &shmid, &shared_memory1) == -1) {
		exit(EXIT_FAILURE);
	}
	printf("Shared Memory attached at: %p", (int*)shared_memory1);	
	shared_buffer_head = (struct buffer*) shared_memory1;		
	buffer_index = 0;


	

	S = semget((key_t)S_KEY, 1, 0666 | IPC_CREAT);
	N = semget((key_t)N_KEY, 1, 0666 | IPC_CREAT);
	E = semget((key_t)E_KEY, 1, 0666 | IPC_CREAT);

	if (!set_semvalue(S, 1) || !set_semvalue(N, 0) || !set_semvalue(E, BUFFERS_SIZE)) {
    fprintf(stderr, "Failed to initialize semaphore\n");
    exit(EXIT_FAILURE);
  }


	stat(FILE_IN, &st);
	file_size = st.st_size;
	shared_buffer_head->total_byte_count = file_size;
	printf("Input File size: %d\n",shared_buffer_head->total_byte_count);
	semaphore_v(N);

	
	file_in = open(FILE_IN, O_RDONLY);
	if (file_in == -1) {
		fprintf(stderr, "file open failed");
		exit(EXIT_FAILURE);
	}

	
	total_bytes_shm = 0;
	bytes_W_shm = 0;
	printf("Reading from file '%s' and putting it into shared memory\n", FILE_IN);
	while (total_bytes_shm < file_size) {
		num_bytes_to_read = read(file_in, input_buffer, BUFSIZ);
		if (num_bytes_to_read == -1) {
			fprintf(stderr, "read failed ");
			exit(EXIT_FAILURE);
		}
		else if (num_bytes_to_read == 0) {
			break;		
		}

		while (bytes_W_shm < num_bytes_to_read) {
			semaphore_p(E);
			semaphore_p(S);
			
			bytes_W_shm += write_to_shared_memory(shared_buffer_head, &buffer_index, input_buffer, bytes_W_shm,num_bytes_to_read);
			semaphore_v(S);
			semaphore_v(N);			
		}
		total_bytes_shm += bytes_W_shm;
		printf("Bytes written to shared memory: %d\n", bytes_W_shm);
		bytes_W_shm= 0;
	}
	
	printf("Total Bytes written from '%s' to shared memory: %d\n", FILE_IN, total_bytes_shm);
	sleep(2);		
	
	if (close(file_in) == -1) {
		fprintf(stderr, "File close failed\n");
	}

  if (shmdt(shared_memory1) == -1) {
    fprintf(stderr, "shmdt failed\n");
    exit(EXIT_FAILURE);
  }

	exit(EXIT_SUCCESS);
}



int write_to_shared_memory(struct buffer *shared_buffer_head, int *buffer_index, char *input_buffer, int bytes_copied, int num_bytes_to_read)
{
	int bytes_to_write = 0;				
	if ((num_bytes_to_read- bytes_copied) > TXT_SZ)
		bytes_to_write = TXT_SZ;
	else
		bytes_to_write = num_bytes_to_read-bytes_copied;
	
	shared_buffer_head[*buffer_index].bytes_to_count = bytes_to_write;
	strncpy(shared_buffer_head[*buffer_index].string_data, input_buffer+bytes_copied, TXT_SZ);

	*buffer_index = ((*buffer_index)+1)%BUFFERS_SIZE;

	return bytes_to_write;
}
