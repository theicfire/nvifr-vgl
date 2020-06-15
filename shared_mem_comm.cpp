#include "shared_mem_comm.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

struct shared_memory {
  size_t data_len;
  uint8_t data[1000 * 1000];  // 1mb
};

void error(const char *msg) {
  perror(msg);
  exit(1);
}

SharedMem::SharedMem(bool mighty_server, uint64_t shared_mem_id) {
  std::stringstream ss;
  ss << "/shared-mem-" << shared_mem_id;
  shared_mem_path = ss.str();
  if (mighty_server) {
    if ((fd_shm = shm_open(shared_mem_path.c_str(), O_RDWR | O_CREAT, 0660)) ==
        -1) {
      error("shm_open");
    }

    if (ftruncate(fd_shm, sizeof(struct shared_memory)) == -1) {
      error("ftruncate");
    }

    if ((shared_mem_ptr = (shared_memory *)mmap(
             NULL, sizeof(struct shared_memory), PROT_READ | PROT_WRITE,
             MAP_SHARED, fd_shm, 0)) == MAP_FAILED) {
      error("mmap");
    }
    shared_mem_ptr->data_len = 0;
  } else {
    if ((fd_shm = shm_open(shared_mem_path.c_str(), O_RDWR, 0)) == -1) {
      error("shm_open");
    }

    if ((shared_mem_ptr = (shared_memory *)mmap(
             NULL, sizeof(struct shared_memory), PROT_READ | PROT_WRITE,
             MAP_SHARED, fd_shm, 0)) == MAP_FAILED) {
      error("mmap");
    }
  }
}

SharedMem::~SharedMem() {
  if (munmap(shared_mem_ptr, sizeof(struct shared_memory)) == -1) {
    error("munmap");
  }
  shm_unlink(shared_mem_path.c_str());
}

void SharedMem::write(uint8_t *data, size_t len) {
  if (len > sizeof(shared_mem_ptr->data) / sizeof(shared_mem_ptr->data[0])) {
    printf("Writing too much data\n");
    return;
  }
  memcpy(shared_mem_ptr->data, data, len);
  shared_mem_ptr->data_len = len;
}

size_t SharedMem::get_written_size() { return shared_mem_ptr->data_len; }

SemaIPC::SemaIPC(bool mighty_server) : mighty_server(mighty_server) {
  if (mighty_server) {
    zmq_frame_response_socket =
        new zmq::socket_t(zmq_context, ZMQ_PULL);  // TODO unique_ptr..
    zmq_frame_response_socket->bind("ipc:///tmp/frame_response.sock");
    int t = 1000;
    zmq_frame_response_socket->setsockopt(ZMQ_RCVTIMEO, &t, sizeof(t));

    zmq_frame_request_socket =
        new zmq::socket_t(zmq_context, ZMQ_PUB);  // TODO unique_ptr..
    zmq_frame_request_socket->bind("ipc:///tmp/frame_request.sock");
  } else {
    zmq_frame_response_socket =
        new zmq::socket_t(zmq_context, ZMQ_PUSH);  // TODO unique_ptr..
    zmq_frame_response_socket->connect("ipc:///tmp/frame_response.sock");

    zmq_frame_request_socket =
        new zmq::socket_t(zmq_context, ZMQ_SUB);  // TODO unique_ptr..
    zmq_frame_request_socket->connect("ipc:///tmp/frame_request.sock");
    zmq_frame_request_socket->setsockopt(ZMQ_SUBSCRIBE, "", 0);
  }
}

SemaIPC::~SemaIPC() { printf("destruct\n"); }

void SemaIPC::send(VglRPC rpc) {
  if (mighty_server) {
    zmq::message_t msg(sizeof(VglRPC));
    memcpy(msg.data(), &rpc, sizeof(VglRPC));
    zmq_frame_request_socket->send(msg, zmq::send_flags::dontwait);

  } else {
    zmq::message_t msg(sizeof(VglRPC));
    memcpy(msg.data(), &rpc, sizeof(VglRPC));
    zmq_frame_response_socket->send(msg, zmq::send_flags::dontwait);
  }
}

VglRPC SemaIPC::receive() {
  if (mighty_server) {
    return receive_mighty_server();
  } else {
    return receive_vgl();
  }
}

VglRPC SemaIPC::receive_vgl() {
  try {
    zmq::message_t request;
    VglRPC *msg;
    // TODO use return result?
    while (true) {
      zmq_frame_request_socket->recv(request, zmq::recv_flags::none);
      msg = static_cast<VglRPC *>(request.data());
      if (msg->id == VglRPCId::PING) {
        zmq::message_t send_msg(sizeof(VglRPC));
        VglRPC pong = {.id = VglRPCId::PONG};
        memcpy(send_msg.data(), &pong, sizeof(VglRPC));
        zmq_frame_response_socket->send(send_msg, zmq::send_flags::dontwait);
      } else {
        break;
      }
    }
    return *msg;
    // printf("Got msg: %s\n", request.to_string().c_str());
  } catch (zmq::error_t err) {
    printf("ZMQ msg building: %s", err.what());
  }
  VglRPC ret = {.id = VglRPCId::NONE};
  return ret;
}

VglRPC SemaIPC::receive_mighty_server() {
  try {
    zmq::message_t response;
    zmq::recv_result_t success =
        zmq_frame_response_socket->recv(response, zmq::recv_flags::none);
    if (success) {
      VglRPC *msg = static_cast<VglRPC *>(response.data());
      return *msg;
    } else {
      VglRPC ret = {.id = VglRPCId::TIMEOUT};
      return ret;
    }
  } catch (zmq::error_t err) {
    printf("ZMQ msg building: %s", err.what());
  }
  printf("Return none!\n");
  VglRPC ret = {.id = VglRPCId::NONE};
  return ret;
}