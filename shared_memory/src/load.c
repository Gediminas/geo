// C program for Consumer process illustrating
// POSIX shared-memory API.
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define DB_SIZE 13193167
const long SIZE = 4096;
const char* name = "GEO1";
const char* db_path = "geo.db";

int main()
{
    const int fd = open(db_path, O_RDONLY, 0666);
    void* fbuf = mmap(0, DB_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
    /* fprintf(stderr, "LOAD: fbuf: %u (=? 12296156)\n", *(unsigned int*)fbuf); */

    const int mfd = shm_open(name, O_CREAT | O_RDWR, 0666);
    ftruncate(mfd, DB_SIZE);
    void* mbuf = mmap(0, DB_SIZE, PROT_WRITE, MAP_SHARED, mfd, 0);

    memcpy(mbuf, fbuf, DB_SIZE);

    shm_unlink(fbuf);
    close(fd);

    /* fprintf(stderr, "LOAD: mbuf: %u\n", *(unsigned int*)mbuf); */
    sleep(20);
    return 0;
}
