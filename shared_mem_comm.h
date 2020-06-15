#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "zmq.hpp"

class SharedMem
{
public:
    SharedMem(bool create);
    ~SharedMem();
    void write(uint8_t *data, size_t len);
    size_t get_written_size();

private:
    struct shared_memory *shared_mem_ptr;
    int fd_shm;
};

class SemaIPC
{
public:
    SemaIPC(bool create);
    ~SemaIPC();

    void wait_for_frame_request();
    void signal_frame_request();

    bool wait_for_frame_response();
    void signal_frame_response();
    void publish_ping();

private:
    zmq::context_t zmq_context;
    zmq::socket_t *zmq_frame_request_socket;
    zmq::socket_t *zmq_frame_response_socket;
};
