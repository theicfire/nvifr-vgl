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
  uint64_t seconds = static_cast<uint64_t>(time(NULL));
  SharedMem shared_mem(true, seconds);
  SemaIPC sema_ipc(true);
  printf("Startup\n");
  bool is_connected = false;
  while (1) {
    if (!is_connected) {
      printf("Send ping!\n");
      VglRPC rpc_ping = {.id = VglRPCId::PING};
      sema_ipc.publish(rpc_ping);
      if (sema_ipc.wait_for_frame_response()) {
        is_connected = true;
        printf("Send restart!\n");
        VglRPC rpc = {.id = VglRPCId::RESTART, .shared_mem_id = seconds};
        sema_ipc.publish(rpc);
      }
    } else {
      printf("Requesting frame!\n");
      VglRPC rpc_req = {.id = VglRPCId::FRAME_REQUEST};
      sema_ipc.publish(rpc_req);
      printf("Wait for response!\n");
      if (sema_ipc.wait_for_frame_response()) {
        printf("Got frame response. Size is %zu\n",
               shared_mem.get_written_size());
      } else {
        printf("No response! Timed out.\n");
        is_connected = false;
      }
    }
  }
}