#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

    int infp, outfp;
    char obuffer[MAX_IBUF_LEN];
    char ibuffer[MAX_OBUF_LEN];

    const char *db_path = argv[1];
    sprintf(obuffer, "./geo %s", db_path);

    if (popen2(obuffer, &infp, &outfp) < 0) {
        fprintf(stderr, "Unable to exec\n");
        exit(1);
    }

    while (1) {
        memset(ibuffer, 0, MAX_OBUF_LEN);
        read(outfp, ibuffer, MAX_OBUF_LEN);
        fprintf(stdout, ibuffer);
        fflush(stdout);

        if (obuffer[0] == 'E') {
            break;
        }

        memset(obuffer, 0, MAX_IBUF_LEN);
        fgets(obuffer, MAX_IBUF_LEN, stdin);
        write(infp, obuffer, strlen(obuffer));
    }

    close(infp); //?
    return EXIT_SUCCESS;
}
