#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
/* #include <sys/shm.h> */
#include <fcntl.h>


#define PIPE_READ 0
#define PIPE_WRITE 1
#define MAX_IBUF_LEN 25
#define MAX_OBUF_LEN 48

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

    const char* req_key = "GEO_REQ";
    const int req_fd = shm_open(req_key, O_CREAT | O_RDWR, 0666);
    ftruncate(req_fd, 100);
    char* req_buf = mmap(0, 100, PROT_WRITE, MAP_SHARED, req_fd, 0);
    memset(req_buf, 0, 100);

    const char* res_key = "GEO_RES";
    const int res_fd = shm_open(res_key, O_RDONLY, 0666);
    const char* res_buf = mmap(0, 100, PROT_READ, MAP_SHARED, res_fd, 0);

    char cnt = 'A';
    const char* res;
    char input[100];
    memset(input, 0, 100);

    while (1) {
        while (cnt != *res_buf || 0 == *res_buf) {
            usleep(1);
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
    /* close(infp); //? */
    return EXIT_SUCCESS;
}
