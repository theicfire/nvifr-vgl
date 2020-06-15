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
        zmq_frame_response_socket = new zmq::socket_t(zmq_context, ZMQ_PULL); // TODO unique_ptr..
        zmq_frame_response_socket->bind("ipc:///tmp/frame_response.sock");
        int t = 1000;
        zmq_frame_response_socket->setsockopt(ZMQ_RCVTIMEO, &t, sizeof(t));

        zmq_frame_request_socket = new zmq::socket_t(zmq_context, ZMQ_PUB); // TODO unique_ptr..
        zmq_frame_request_socket->bind("ipc:///tmp/frame_request.sock");
    }
    else
    {
        zmq_frame_response_socket = new zmq::socket_t(zmq_context, ZMQ_PUSH); // TODO unique_ptr..
        zmq_frame_response_socket->connect("ipc:///tmp/frame_response.sock");

        zmq_frame_request_socket = new zmq::socket_t(zmq_context, ZMQ_SUB); // TODO unique_ptr..
        zmq_frame_request_socket->connect("ipc:///tmp/frame_request.sock");
        const char *filter = "hello";
        zmq_frame_request_socket->setsockopt(ZMQ_SUBSCRIBE, filter, strlen(filter));
    }
}

SemaIPC::~SemaIPC()
{
    printf("destruct\n");
}

void SemaIPC::publish_ping()
{
    // publish!
    zmq_frame_request_socket->send(zmq::str_buffer("hello ping"), zmq::send_flags::dontwait);
}

void SemaIPC::signal_frame_request()
{
    // publish!
    zmq_frame_request_socket->send(zmq::str_buffer("hello frame"), zmq::send_flags::dontwait);
}

void SemaIPC::wait_for_frame_request()
{
    try
    {
        zmq::message_t request;
        // TODO use return result?
        zmq_frame_request_socket->recv(request, zmq::recv_flags::none);
        // TODO determine if PING message. If so, send PONG and keep receiving.
    }
    catch (zmq::error_t err)
    {
        printf("ZMQ msg building: %s", err.what());
    }
}

void SemaIPC::signal_frame_response()
{
    zmq_frame_response_socket->send(zmq::str_buffer("Hello, world"), zmq::send_flags::dontwait);
}

bool SemaIPC::wait_for_frame_response()
{
    try
    {
        zmq::message_t response;
        // TODO use return result?
        zmq::recv_result_t success = zmq_frame_response_socket->recv(response, zmq::recv_flags::none);
        return !!success;
    }
    catch (zmq::error_t err)
    {
        printf("ZMQ msg building: %s", err.what());
    }
    return false;
}