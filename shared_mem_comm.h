#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "zmq.hpp"

enum VglRPCId
{
    NONE,
    RESTART,
    PING,
    PONG,
    FRAME_REQUEST,
    FRAME_RESPONSE,
};

struct VglRPC
{
    VglRPCId id;
};

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

    VglRPCId wait_for_frame_request();
    void publish(VglRPCId id);

    bool wait_for_frame_response();
    void signal_frame_response();

private:
    zmq::context_t zmq_context;
    zmq::socket_t *zmq_frame_request_socket;
    zmq::socket_t *zmq_frame_response_socket;
};
