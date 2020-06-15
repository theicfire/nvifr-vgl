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
        VglRPCId id = sema_ipc.wait_for_frame_request();
        if (id == VglRPCId::FRAME_REQUEST)
        {
            printf("Got frame request. Sending response.\n");
            sema_ipc.signal_frame_response();
        }
        else if (id == VglRPCId::RESTART)
        {
            printf("Got restart\n");
        }
        sleep(1);
    }
}