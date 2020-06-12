#include <semaphore.h>

#include <string.h>
#include <unistd.h>
#include <stdint.h>

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

    void wait_for_frame_response();
    void signal_frame_response();

private:
    sem_t *frame_request;
    sem_t *frame_response;
};
