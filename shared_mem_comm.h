#include <semaphore.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "zmq.hpp"

enum VglRPCId {
  NONE,
  RESTART,
  PING,
  PONG,
  FRAME_REQUEST,
  FRAME_RESPONSE,
};

struct VglRPC {
  VglRPCId id;
  uint64_t shared_mem_id;
};

class SharedMem {
 public:
  SharedMem(bool create, uint64_t shared_mem_id);
  ~SharedMem();
  void write(uint8_t *data, size_t len);
  size_t get_written_size();

 private:
  struct shared_memory *shared_mem_ptr;
  int fd_shm;
  std::string shared_mem_path;
};

class SemaIPC {
 public:
  SemaIPC(bool create);
  ~SemaIPC();

  VglRPC wait_for_frame_request();
  void publish(VglRPC rpc);

  bool wait_for_frame_response();
  void signal_frame_response();

 private:
  zmq::context_t zmq_context;
  zmq::socket_t *zmq_frame_request_socket;
  zmq::socket_t *zmq_frame_response_socket;
};
