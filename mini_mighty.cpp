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

SharedMem shared_mem(true);
SemaIPC sema_ipc(true);

int main(int argc, char **argv)
{
    while (1)
    {
        printf("Requesting frame!\n");
        sema_ipc.signal_frame_request();
        sema_ipc.wait_for_frame_response();
        printf("Got frame response. Size is %zu\n", shared_mem.get_written_size());
    }
}