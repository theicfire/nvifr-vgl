#include <semaphore.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "zmq.hpp"

enum VglRPCId {
  NONE,
  TIMEOUT,
  RESTART,
  PING,
  PONG,
  FRAME_REQUEST,
  FRAME_RESPONSE,
  EMPTY_RESPONSE,
};

struct VglRPC {
  VglRPCId id;
  uint64_t shared_mem_id = 0;
  uint16_t width = 0;
  uint16_t height = 0;
};

class SharedMem {
 public:
  SharedMem(bool create, uint64_t shared_mem_id);
  ~SharedMem();
  void write(uint8_t *data, size_t len);
  size_t get_written_size();
  std::vector<uint8_t> get_frame();
  static uint64_t generate_mem_id();

 private:
  struct shared_memory *shared_mem_ptr;
  int fd_shm;
  std::string shared_mem_path;
};

class VglMightyIPC {
 public:
  VglMightyIPC(bool create);
  ~VglMightyIPC();

  void send(VglRPC rpc);
  VglRPC receive();
  void clear_receive();

 private:
  VglRPC receive_vgl();
  VglRPC receive_mighty_server();
  zmq::context_t zmq_context;
  zmq::socket_t *zmq_frame_request_socket;
  zmq::socket_t *zmq_frame_response_socket;
  bool mighty_server;
};
