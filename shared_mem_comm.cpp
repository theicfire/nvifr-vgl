#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "shared_mem_comm.h"

#define SHARED_MEM_NAME "/posix-shared-mem-example"
#define FROM_MIGHTY_SEM_NAME "/from-mighty"
#define TO_MIGHTY_SEM_NAME "/to-mighty"

struct shared_memory
{
    size_t data_len;
    uint8_t data[1000 * 1000]; // 1mb
};

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

SharedMem::SharedMem(bool create)
{
    if (create)
    {
        shm_unlink(SHARED_MEM_NAME); // TODO this ok?
        if ((fd_shm = shm_open(SHARED_MEM_NAME, O_RDWR | O_CREAT, 0660)) == -1)
        {
            error("shm_open");
        }

        if (ftruncate(fd_shm, sizeof(struct shared_memory)) == -1)
        {
            error("ftruncate");
        }

        if ((shared_mem_ptr = (shared_memory *)mmap(NULL, sizeof(struct shared_memory), PROT_READ | PROT_WRITE, MAP_SHARED,
                                                    fd_shm, 0)) == MAP_FAILED)
        {
            error("mmap");
        }
        shared_mem_ptr->data_len = 0;
    }
    else
    {
        if ((fd_shm = shm_open(SHARED_MEM_NAME, O_RDWR, 0)) == -1)
        {
            error("shm_open");
        }

        if ((shared_mem_ptr = (shared_memory *)mmap(NULL, sizeof(struct shared_memory), PROT_READ | PROT_WRITE, MAP_SHARED,
                                                    fd_shm, 0)) == MAP_FAILED)
        {
            error("mmap");
        }
    }
}

SharedMem::~SharedMem()
{
    // if (shm_unlink(SHARED_MEM_NAME) == -1)
    // {
    //     error("shm_unlink");
    // }
    if (munmap(shared_mem_ptr, sizeof(struct shared_memory)) == -1)
    {
        error("munmap");
    }
}

void SharedMem::write(uint8_t *data, size_t len)
{
    if (len > sizeof(shared_mem_ptr->data) / sizeof(shared_mem_ptr->data[0]))
    {
        printf("Writing too much data\n");
        return;
    }
    memcpy(shared_mem_ptr->data, data, len);
    shared_mem_ptr->data_len = len;
}

size_t SharedMem::get_written_size()
{
    return shared_mem_ptr->data_len;
}

SemaIPC::SemaIPC(bool create)
{
    if (create)
    {
        printf("Create semaphores!\n");
        sem_unlink(FROM_MIGHTY_SEM_NAME);
        if ((frame_request = sem_open(FROM_MIGHTY_SEM_NAME, O_CREAT, 0660, 0)) == SEM_FAILED)
        {
            error("sem_open");
        }

        sem_unlink(TO_MIGHTY_SEM_NAME);
        if ((frame_response = sem_open(TO_MIGHTY_SEM_NAME, O_CREAT, 0660, 0)) == SEM_FAILED)
        {
            error("sem_open");
        }
        printf("Done create semaphores!\n");
    }
    else
    {
        if ((frame_request = sem_open(FROM_MIGHTY_SEM_NAME, 0, 0, 0)) == SEM_FAILED)
        {
            error("from_mighty sem_open");
        }

        if ((frame_response = sem_open(TO_MIGHTY_SEM_NAME, 0, 0, 0)) == SEM_FAILED)
        {
            error("to_mighty sem_open");
        }
    }
}

SemaIPC::~SemaIPC()
{
    // printf("Destruct semaipc\n");
    // if (sem_unlink(FROM_MIGHTY_SEM_NAME) == -1)
    // {
    //     printf("could not unlink semaphore..\n");
    // };
    // if (sem_unlink(TO_MIGHTY_SEM_NAME) == -1)
    // {
    //     printf("could not unlink semaphore..\n");
    // };
}

void SemaIPC::signal_frame_request()
{
    // TODO error check
    if (sem_post(frame_request) == -1)
    {
        error("failed to sem_post");
    }
}

void SemaIPC::wait_for_frame_request()
{
    // TODO error check
    if (sem_wait(frame_request) == -1)
    {
        error("failed to sem_wait");
    }
}

void SemaIPC::signal_frame_response()
{
    if (sem_post(frame_response) == -1)
    {
        error("failed to sem_post");
    }
}

void SemaIPC::wait_for_frame_response()
{
    // TODO error check
    if (sem_wait(frame_response) == -1)
    {
        error("failed to sem_wait");
    }
}