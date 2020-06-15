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
        printf("Created sharedmem\n");
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
        printf("Done create semaphores!\n");

        zmq_frame_response_socket = new zmq::socket_t(zmq_frame_request, ZMQ_PULL); // TODO unique_ptr..
        zmq_frame_response_socket->bind("ipc:///tmp/test_shared.sock");
    }
    else
    {
        if ((frame_request = sem_open(FROM_MIGHTY_SEM_NAME, 0, 0, 0)) == SEM_FAILED)
        {
            error("from_mighty sem_open");
        }
        zmq_frame_response_socket = new zmq::socket_t(zmq_frame_request, ZMQ_PUSH); // TODO unique_ptr..
        zmq_frame_response_socket->connect("ipc:///tmp/test_shared.sock");
    }
    printf("Yay down here\n");
}

SemaIPC::~SemaIPC()
{
    printf("destruct\n");
}

void SemaIPC::signal_frame_request()
{
    if (sem_post(frame_request) == -1)
    {
        error("failed to sem_post");
    }
}

void SemaIPC::wait_for_frame_request()
{
    if (sem_wait(frame_request) == -1)
    {
        error("failed to sem_wait");
    }
}

void SemaIPC::signal_frame_response()
{
    printf("Response!\n");
    zmq_frame_response_socket->send(zmq::str_buffer("Hello, world"), zmq::send_flags::dontwait);
}

void SemaIPC::wait_for_frame_response()
{
    try
    {
        zmq::message_t request;
        // TODO use return result?
        printf("Wait for response\n");
        zmq_frame_response_socket->recv(request, zmq::recv_flags::none);
        printf("Wooo\n");
    }
    catch (zmq::error_t err)
    {
        printf("ZMQ msg building: %s", err.what());
    }
}