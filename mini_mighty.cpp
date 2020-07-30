/*
 *
 *       logger.c: Write strings in POSIX shared memory to file
 *                 (Server process)
 */

#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "shared_mem_comm.h"

int main(int argc, char **argv) {
  printf("MAIN mini_mighty\n");
  uint64_t shared_mem_id = SharedMem::generate_mem_id();
  SharedMem shared_mem(true, shared_mem_id);
  VglMightyIPC vgl_ipc(true);
  printf("Startup\n");
  bool is_connected = false;
  while (1) {
    if (!is_connected) {
      vgl_ipc.clear_receive();
      printf("Send ping!\n");
      VglRPC rpc_ping = {.id = VglRPCId::PING};
      vgl_ipc.send(rpc_ping);
      VglRPC response = vgl_ipc.receive();
      if (response.id != VglRPCId::TIMEOUT) {
        is_connected = true;
        printf("Send restart with id: %lu!\n", shared_mem_id);
        VglRPC rpc = {.id = VglRPCId::RESTART,
                      .shared_mem_id = shared_mem_id,
                      .width = 0,
                      .height = 0};
        vgl_ipc.send(rpc);
      }
    } else {
      VglRPC rpc_req = {.id = VglRPCId::FRAME_REQUEST};
      vgl_ipc.send(rpc_req);
      VglRPC response = vgl_ipc.receive();
      if (response.id == VglRPCId::FRAME_RESPONSE) {
        printf("Got frame response. Size is %zu\n",
               shared_mem.get_written_size());
      } else if (response.id == VglRPCId::EMPTY_RESPONSE) {
        usleep(20000);
      } else if (response.id == VglRPCId::TIMEOUT) {
        printf("Timed out.\n");
        is_connected = false;
      } else {
        printf("Error: Some other response\n");
      }
    }
  }
}