#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define READ 0
#define WRITE 1
#define MAX_CMD_LEN 50

pid_t popen2(const char *command, int *infp, int *outfp) {
    int p_stdin[2];
    int p_stdout[2];

    if (pipe(p_stdin) || pipe(p_stdout))
        return -1;

    const pid_t pid = fork();

    if (pid < 0)
        return pid;

    if (!pid) {
        close(p_stdin[WRITE]);
        dup2(p_stdin[READ], READ);
        close(p_stdout[READ]);
        dup2(p_stdout[WRITE], WRITE);

        execl("/bin/sh", "sh", "-c", command, NULL);
        perror("execl");
        exit(1);
    }

    if (infp)
        *infp = p_stdin[WRITE];
    else
        close(p_stdin[WRITE]);

    if (outfp)
        *outfp = p_stdout[READ];
    else
        close(p_stdout[READ]);

    return pid;
}

int main(int argc, char **argv) {
    int infp, outfp;
    char tx[MAX_CMD_LEN];
    char rx[MAX_CMD_LEN];

    if (popen2("./geo ./geo.db", &infp, &outfp) <= 0) {
        printf("Unable to exec sort\n");
        exit(1);
    }

    memset(rx, 0, MAX_CMD_LEN);
    read(outfp, rx, MAX_CMD_LEN);
    printf(rx);
    fflush(stdout);

    while (1) {
        memset(tx, 0, MAX_CMD_LEN);
        memset(rx, 0, MAX_CMD_LEN);

        fgets(tx, MAX_CMD_LEN, stdin);
        write(infp, tx, strlen(tx));
        read(outfp, rx, MAX_CMD_LEN);
        printf(rx);
        fflush(stdout);

        if (tx[0] == 'E') {
            break;
        }
    }

    close(infp); //?
    return EXIT_SUCCESS;
}
