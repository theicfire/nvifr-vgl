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
  printf("vgl_tester\n");
  SemaIPC sema_ipc(false);
  printf("run loop\n");
  printf("Start connection!\n");
  while (1) {
    printf("Wait for request!\n");
    VglRPC rpc = sema_ipc.wait_for_frame_request();
    if (rpc.id == VglRPCId::FRAME_REQUEST) {
      printf("Got frame request. Sending response.\n");
      sema_ipc.signal_frame_response();
    } else if (rpc.id == VglRPCId::RESTART) {
      printf("Got restart. id: %ld\n", rpc.shared_mem_id);
    }
    sleep(1);
  }
}