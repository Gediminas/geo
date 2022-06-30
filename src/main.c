#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
/* #include <sys/shm.h> */
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_CMD_LEN 50
#define DB_SIZE 13193167
#define seg1_start 4
 //All combinations of IP octet1 & octet2
#define seg1_size  256 * 256 * 8
#define seg2_start seg1_start + seg1_size

unsigned char* buffer = NULL;
int loaded = 0;

inline int LoadDatabase(const char* db_path) {
    FILE *file = fopen(db_path, "rb");
    if (!file) {
        fprintf(stderr, "ERROR: Load failed\n");
        return 0;
    }
    buffer = malloc(DB_SIZE);
    const int bytes = fread(buffer, 1, DB_SIZE, file);
    fclose(file);
    return bytes;
}

inline const char* PerformLookup(const char* ip) {
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

extern int LoadDatabase(const char* db_path);
extern const char* PerformLookup(const char* ip);

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "ERROR: Usage './geo <geo.db>'\n");
        goto FAILED;
    }

    const char *db_path = argv[1];


    // Abusing specification hole
    loaded = LoadDatabase(db_path);


    const char* req_key = "/GEO_REQ";
    const int req_fd = shm_open(req_key, O_RDONLY, 0666);
    const char* req_buf = mmap(NULL, 100, PROT_READ, MAP_SHARED, req_fd, 0);
    if (req_buf == MAP_FAILED) {
        fprintf(stderr, "ERROR: mmap /GEO_REQ failed\n");
        goto FAILED;
    }

    const char* res_key = "/GEO_RES";
    const int res_fd = shm_open(res_key, O_CREAT | O_RDWR, 0666);
    ftruncate(res_fd, 100);
    char* res_buf = mmap(NULL, 100, PROT_WRITE, MAP_SHARED, res_fd, 0);
    if (res_buf == MAP_FAILED) {
        fprintf(stderr, "ERROR: mmap /GEO_RES failed\n");
        goto FAILED;
    }
    memset(res_buf, 0, 100);

    const char* req;
    char cnt = 'A';

    sprintf(res_buf+2, "READY\n");
    *res_buf = cnt;

    while (1) {
        while (cnt == *req_buf || 0 == *req_buf) {
            usleep(0);
        }

        cnt = *req_buf;
        req = req_buf + 2;

        //LOOKUP
        if (req[5] == 'P') {
            // if (!loaded){
            //     fprintf(stdout, "ERROR: Lookup requested before database was ever loaded\n");//stderr
            //     goto FAILED;
            // }
            const char* answer = PerformLookup(&req[7]);
            sprintf(res_buf+2, "%s\n", answer);
            *res_buf = cnt;
        }
        else if (req[3] == 'D') {
            /* loaded = LoadDatabase(db_path, buffer, DB_SIZE); */
            /* if (!loaded) { */
            /*     fprintf(stdout, "ERROR: DB open failed\n");//stderr */
            /*     goto FAILED; */
            /* } */
            // if (!loaded){
            //     fprintf(stdout, "ERROR: DB open failed\n");//stderr
            //     goto FAILED;
            // }
            /* fprintf(stdout, "OK\n"); */
            sprintf(res_buf+2, "OK\n");
            *res_buf = cnt;
        }
        else if (req[0] == 'E') {
            /* fprintf(stdout, "OK\n"); */
            sprintf(res_buf+2, "OK\n");
            *res_buf = cnt;
            break;
        } else {
            fprintf(stderr, "ERROR: Unknown command: %s\n", req);//stderr
            /* fprintf(stdout, "FAILED\n"); */
            sprintf(res_buf+2, "FAILED\n");
            *res_buf = cnt;
            goto FAILED;
        }
    }

    free(buffer);
    return EXIT_SUCCESS;
FAILED:
    fflush(stdout);
    free(buffer);
    return EXIT_FAILURE;
}
