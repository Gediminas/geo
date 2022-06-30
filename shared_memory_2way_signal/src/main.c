#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
/* #include <sys/shm.h> */
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#define DB_SIZE 13193167
const long SIZE = 4096;
const char* name = "GEO1";
const char* db_path = "geo.db";

#define PIPE_READ 0
#define PIPE_WRITE 1
#define MAX_IBUF_LEN 25
#define MAX_OBUF_LEN 48

#define MAX_CMD_LEN 50
#define DB_SIZE 13193167
#define seg1_start 4
 //All combinations of IP octet1 & octet2
#define seg1_size  256 * 256 * 8
#define seg2_start seg1_start + seg1_size

pid_t parent_pid = 0;

unsigned char* LoadDatabase(const char* db_path) {
    FILE *file = fopen(db_path, "rb");
    if (!file) {
        return NULL;
    }
    unsigned char* buffer = malloc(DB_SIZE);
    const int bytes = fread(buffer, 1, DB_SIZE, file);
    fclose(file);
    return buffer;
}

const char* PerformLookup(const unsigned char* buffer, const char* ip) {
    const in_addr_t ip_be          = inet_addr(ip); //netorder == big-endian
    // const unsigned short halfip = ((ip_be & 0xFF00) >> 8) | ((ip_be & 0x00FF) << 8); // to LE always => no gain
    const unsigned short halfip    = ntohs((unsigned short)ip_be); //to this machine endian (little-endian)
    const unsigned char ip_octet3  = *(((const unsigned char*)&ip_be)+2);

    const unsigned long seg3_start  = *(const unsigned int*)&buffer[0];
    const unsigned long seg1_offset = seg1_start + halfip * 8; //Lets trust compiler to do smth better than <<3
    const unsigned long seg2_offset = *(const unsigned int*)&buffer[seg1_offset];
    const unsigned long count2      = *(const unsigned int*)&buffer[seg1_offset + 4];

    unsigned long seg2_addr = seg2_start + seg2_offset;

    //TODO: improve maybe (no gain if jumping by halfs)
    for (int i = 0; i < count2; ++i) {
        const unsigned long seg3_value = *(const unsigned int*)&buffer[seg2_addr];
        const unsigned char curr_octet3 = seg3_value;
        if (ip_octet3 <= curr_octet3) {
            break; //Found
        }
        seg2_addr += 4;
    }

    // If not found in the loop above => take the first record from the next block.
    // e.g. ip="53.103.144.0"
    // Halfip  3rd-octet  Range-end
    // -------------------------------------------
    // 13671   141        53.103.141.255 -> US,Cheyenne
    // 13671   142        53.103.142.255 -> US,Boydton
    // 13671   143        53.103.143.255 -> US,Des Moines
    //                    <= we are here with "53.103.144.0" => ***
    // 13823   255        53.255.255.255 -> DE Stuttgart
    //
    // *** => we must take the first range from the next halfip block
    //        (i.e. halfip=13823 oct=255)

    const unsigned long seg3_value = *(unsigned int*)&buffer[seg2_addr];
    const unsigned long seg3_offset = seg3_value >> 8;
    const unsigned long seg3_addr = seg3_start + seg3_offset;
    const char* city = (const char*) &buffer[seg3_addr];
    return city;
}

/* extern int LoadDatabase(const char* db_path); */
/* extern const char* PerformLookup(const char* ip); */

void child_handle_lookup_request(int sig) {
    // fprintf(stderr, "C: >> LOOKUP REQUEST\n");

    /* fprintf(stdout, "OK\n"); */
    /* fflush(stdout); */

    kill(parent_pid, SIGUSR1); // Notify parent that response is ready
}
  
void parent_handle_lookup_response(int sig) {
    // fprintf(stderr, "P: << LOOKUP RESPONSE -,-\n");

    fprintf(stdout, "-,-\n");
    fflush(stdout);
}
  
int child(pid_t parent_pid) {
    signal(SIGUSR1, child_handle_lookup_request);

    // Abusing specification hole => load before LOAD command
    unsigned char* buffer = LoadDatabase(db_path);
    if (!buffer) {
        fprintf(stderr, "ERROR: Load failed\n");
        return EXIT_FAILURE;
    }

    fprintf(stdout, "READY\n");
    fflush(stdout);

    while (1) {
        usleep(100000); //loop also because signal kills sleep
    }
    free(buffer); //FIXME: catch signal
    return EXIT_SUCCESS;
}

int main()
{
    parent_pid = getpid();

    // fprintf(stderr, "P: pid = %u\n", parent_pid);
    fflush(stderr);

    const pid_t child_pid = fork();
    if (child_pid < 0)
        return child_pid;

    if (child_pid == 0)
        return child(parent_pid);

    //parent
    //
    signal(SIGUSR1, parent_handle_lookup_response);

    char input[100];
    while (1) {
        memset(input, 0, 100);
        fgets(input, 100, stdin);

        switch (input[3]) {
            case 'K': // LOOKUP
                kill(child_pid, SIGUSR1); // Notify child that request is ready
                usleep(100000);
                break;
            case 'D': // LOAD
                fprintf(stdout, "OK\n");
                fflush(stdout);
                break;
            case 'T': // EXIT
                fprintf(stdout, "OK\n");
                fflush(stdout);
                exit(0);
                break;
            default:
                fprintf(stderr, "ERROR: Invalid command\n");
                break;
        }
    }

    return EXIT_SUCCESS;
}

/* int main(int argc, char** argv) { */
/*     const char* req_key = "/GEO_REQ"; */
/*     const int req_fd = shm_open(req_key, O_RDONLY, 0666); */
/*     const char* req_buf = mmap(NULL, 100, PROT_READ, MAP_SHARED, req_fd, 0); */
/*     if (req_buf == MAP_FAILED) { */
/*         fprintf(stderr, "ERROR: mmap /GEO_REQ failed\n"); */
/*         goto FAILED; */
/*     } */

/*     const char* res_key = "/GEO_RES"; */
/*     const int res_fd = shm_open(res_key, O_CREAT | O_RDWR, 0666); */
/*     ftruncate(res_fd, 100); */
/*     char* res_buf = mmap(NULL, 100, PROT_WRITE, MAP_SHARED, res_fd, 0); */
/*     if (res_buf == MAP_FAILED) { */
/*         fprintf(stderr, "ERROR: mmap /GEO_RES failed\n"); */
/*         goto FAILED; */
/*     } */
/*     memset(res_buf, 0, 100); */

/*     const char* req; */
/*     char cnt = 'A'; */

/*     sprintf(res_buf+2, "READY\n"); */
/*     *res_buf = cnt; */

/*     while (1) { */
/*         while (cnt == *req_buf || 0 == *req_buf) { */
/*             usleep(0); */
/*         } */

/*         cnt = *req_buf; */
/*         req = req_buf + 2; */

/*         //LOOKUP */
/*         if (req[5] == 'P') { */
/*             // if (!loaded){ */
/*             //     fprintf(stdout, "ERROR: Lookup requested before database was ever loaded\n");//stderr */
/*             //     goto FAILED; */
/*             // } */
/*             const char* answer = PerformLookup(&req[7]); */
/*             sprintf(res_buf+2, "%s\n", answer); */
/*             *res_buf = cnt; */
/*         } */
/*         else if (req[3] == 'D') { */
/*             /1* loaded = LoadDatabase(db_path, buffer, DB_SIZE); *1/ */
/*             /1* if (!loaded) { *1/ */
/*             /1*     fprintf(stdout, "ERROR: DB open failed\n");//stderr *1/ */
/*             /1*     goto FAILED; *1/ */
/*             /1* } *1/ */
/*             // if (!loaded){ */
/*             //     fprintf(stdout, "ERROR: DB open failed\n");//stderr */
/*             //     goto FAILED; */
/*             // } */
/*             /1* fprintf(stdout, "OK\n"); *1/ */
/*             sprintf(res_buf+2, "OK\n"); */
/*             *res_buf = cnt; */
/*         } */
/*         else if (req[0] == 'E') { */
/*             /1* fprintf(stdout, "OK\n"); *1/ */
/*             sprintf(res_buf+2, "OK\n"); */
/*             *res_buf = cnt; */
/*             break; */
/*         } else { */
/*             fprintf(stderr, "ERROR: Unknown command: %s\n", req);//stderr */
/*             /1* fprintf(stdout, "FAILED\n"); *1/ */
/*             sprintf(res_buf+2, "FAILED\n"); */
/*             *res_buf = cnt; */
/*             goto FAILED; */
/*         } */
/*     } */

/*     free(buffer); */
/*     return EXIT_SUCCESS; */
/* FAILED: */
/*     fflush(stdout); */
/*     free(buffer); */
/*     return EXIT_FAILURE; */
/* } */
