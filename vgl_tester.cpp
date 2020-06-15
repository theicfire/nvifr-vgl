#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>

#include "shared_mem_comm.h"

int main(int argc, char **argv)
{
    printf("vgl_tester\n");
    SharedMem shared_mem(false);
    SemaIPC sema_ipc(false);
    printf("run loop\n");
    printf("Start connection!\n");
    while (1)
    {
        printf("Wait for request!\n");
        sema_ipc.wait_for_frame_request();
        printf("Send frame!\n");
        sema_ipc.signal_frame_response();
        sleep(1);
    }
}