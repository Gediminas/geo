#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
/* #include <sys/shm.h> */
#include <fcntl.h>


#define DB_SIZE 13193167
const long SIZE = 4096;
const char* name = "GEO1";
const char* db_path = "geo.db";

#define PIPE_READ 0
#define PIPE_WRITE 1
#define MAX_IBUF_LEN 25
#define MAX_OBUF_LEN 48

int main()
{
    fprintf(stderr, "MAIN\n");
    const pid_t parent_pid = getpid();
    const pid_t child_pid = fork();
    if (child_pid < 0)
        return child_pid;

    if (child_pid == 0) {
        // child
        fprintf(stderr, "CHILD\n");

        const int fd = open(db_path, O_RDONLY, 0666);
        void* fbuf = mmap(NULL, DB_SIZE, PROT_READ, MAP_SHARED, fd, 0);

        fprintf(stderr, "load: fbuf: %u (=? 12296156)\n", *(unsigned int*)fbuf);

        /* shm_unlink(name); */
        const int mfd = shm_open(name, O_CREAT | O_RDWR, 0666);
        ftruncate(mfd, DB_SIZE);
        void* mbuf = mmap(0, DB_SIZE, PROT_WRITE, MAP_SHARED, mfd, 0);

        memcpy(mbuf, fbuf, DB_SIZE);
        shm_unlink(fbuf);
        close(fd);

        fprintf(stderr, "load: mbuf: %u\n", *(unsigned int*)mbuf);
        sleep(1000);
        return 0;
    }

    // parent
    fprintf(stderr, "PARENT\n");
    fprintf(stderr, "Waiting for child to finish\n");
    int child_status;
    while (wait(&child_status) != child_pid);
    fprintf(stderr, "Continue\n");

    const int fd = open(db_path, O_RDONLY, 0666);
    void* fbuf = mmap(0, DB_SIZE, PROT_READ, MAP_SHARED, fd, 0);

    // const int mfd = shm_open(name, O_RDONLY, 0666);
    // void* ptr = mmap(0, SIZE, PROT_READ, MAP_SHARED, mfd, 0);
    /* printf("RX: %s\n", s); */

    // unsigned int addr;
    // sscanf((char*)ptr, "%x",&addr);
    // printf("RX: %x (%x)\n", addr, ptr);
    /* sleep(10); */
    // shm_unlink(name);

    return 0;
}



/*
pid_t popen2(const char *command, int *infp, int *outfp) {
    int pipe_in_fds[2], pipe_out_fds[2];

    if (pipe(pipe_in_fds) != 0 || pipe(pipe_out_fds) != 0)
        return -1;

    const pid_t pid = fork();

    if (pid < 0)
        return pid;

    if (!pid) {
        close(pipe_in_fds[PIPE_WRITE]);
        dup2(pipe_in_fds[PIPE_READ], PIPE_READ);

        close(pipe_out_fds[PIPE_READ]);
        dup2(pipe_out_fds[PIPE_WRITE], PIPE_WRITE);

        execl("/bin/sh", "sh", "-c", command, NULL);
        perror("execl");
        exit(1);
    }

    if (infp)
        *infp = pipe_in_fds[PIPE_WRITE];
    else
        close(pipe_in_fds[PIPE_WRITE]);

    if (outfp)
        *outfp = pipe_out_fds[PIPE_READ];
    else
        close(pipe_out_fds[PIPE_READ]);

    return pid;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "ERROR: Usage './geo_proxy <geo.db>'\n");
        return EXIT_FAILURE;
    }

    const char *db_path = argv[1];

    int infp, outfp;
    char cmd[256];
    sprintf(cmd, "./geo %s", db_path);
    if (popen2(cmd, &infp, &outfp) < 0) {
        fprintf(stderr, "Unable to exec\n");
        exit(1);
    }

    const char* req_key = "/GEO_REQ";
    const int req_fd = shm_open(req_key, O_CREAT | O_RDWR, 0666);
    ftruncate(req_fd, 100);
    char* req_buf = mmap(NULL, 100, PROT_WRITE, MAP_SHARED, req_fd, 0);
    memset(req_buf, 0, 100);

    const char* res_key = "/GEO_RES";
    const int res_fd = shm_open(res_key, O_RDONLY, 0666);
    const char* res_buf = mmap(NULL, 100, PROT_READ, MAP_SHARED, res_fd, 0);

    char cnt = 'A';
    const char* res;
    char input[100];
    memset(input, 0, 100);

    while (1) {
        while (cnt != *res_buf || 0 == *res_buf) {
            usleep(0);
        }
        cnt = *res_buf;
        res = res_buf + 2;

        fprintf(stdout, "%s", res);
        fflush(stdout);

        if (input[0] == 'E') {
            break;
        }

        fgets(input, 100, stdin);
        input[strcspn(input, "\n")] = 0;
        cnt += 1;
        if (cnt < 65 || 85 < cnt) {
            cnt = 65;
        }

        sprintf(req_buf+2, "%s", input);
        *req_buf = cnt;
    }
EXIT:
    return EXIT_SUCCESS;
}
*/


