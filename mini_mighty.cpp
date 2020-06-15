/*
 *
 *       logger.c: Write strings in POSIX shared memory to file
 *                 (Server process)
 */

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
    printf("MAIN mini_mighty\n");
    SharedMem shared_mem(true);
    SemaIPC sema_ipc(true);
    printf("Startup\n");
    bool is_connected = false;
    while (1)
    {
        if (!is_connected)
        {
            printf("Send ping!\n");
            sema_ipc.publish(VglRPCId::PING);
            if (sema_ipc.wait_for_frame_response())
            {
                is_connected = true;
                sema_ipc.publish(VglRPCId::RESTART);
            }
        }
        else
        {
            printf("Requesting frame!\n");
            sema_ipc.publish(VglRPCId::FRAME_REQUEST);
            printf("Wait for response!\n");
            if (sema_ipc.wait_for_frame_response())
            {
                printf("Got frame response. Size is %zu\n", shared_mem.get_written_size());
            }
            else
            {
                printf("No response! Timed out.\n");
                is_connected = false;
            }
        }
    }
}