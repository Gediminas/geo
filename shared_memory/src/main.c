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
#include <arpa/inet.h>


#define MAX_CMD_LEN 50
#define DB_SIZE 13193167
#define seg1_start 4
 //All combinations of IP octet1 & octet2
#define seg1_size  256 * 256 * 8
#define seg2_start seg1_start + seg1_size

unsigned char* buffer = NULL;
int loaded = 0;
char command[MAX_CMD_LEN];

const char* name = "GEO1";

/* const char* name = "./geo.db"; */
/* void* ptr = NULL; */

/* unsigned char* LoadDatabase(const char* db_path) { */
/*     /1* FILE *file = fopen(db_path, "rb"); *1/ */
/*     /1* if (!file) { *1/ */
/*     /1*     return 0; *1/ */
/*     /1* } *1/ */
/*     /1* const int bytes = fread(buffer, 1, size, file); *1/ */
/*     /1* fclose(file); *1/ */
/*     /1* return bytes; *1/ */
/* } */

const char* PerformLookup(const char* ip) {
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

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "ERROR: Usage './geo <geo.db>'\n");
        goto FAILED;
    }
    const char *db_path = argv[1];

    const pid_t pid = fork();
    if (pid < 0)
        return pid;

    if (pid == 0) {
        popen("./geo_load geo.db", "r");
        /* const int ffd = open(db_path, O_RDONLY, 0666); */
        /* void* fbuf = mmap(0, DB_SIZE, PROT_READ, MAP_PRIVATE, ffd, 0); */

        /* fprintf(stderr, "fbuf: %u (=? 12296156)\n", *(unsigned int*)fbuf); */

        /* const int mfd = shm_open("GEOOO", O_CREAT | O_RDWR, 0666); */
        /* ftruncate(mfd, DB_SIZE); */
        /* void* mbuf = mmap(0, DB_SIZE, PROT_WRITE, MAP_PRIVATE, mfd, 0); */
        /* memcpy(mbuf, fbuf, DB_SIZE); */
        /* close(ffd); */
        /* shm_unlink(db_path); */

        /* fprintf(stderr, "mbuf: %u\n", *(unsigned int*)mbuf); */
        /* sleep(20); */
        return 0;
    }
    /* /1* int status; *1/ */
    /* /1* while (wait(&status) != pid); *1/ */
    /* /1* printf("Continue\n"); *1/ */
    /* sleep(1); */

    const int mfd = shm_open(name, O_RDONLY, 0666);
    void* mbuf = mmap(0, DB_SIZE, PROT_READ, MAP_SHARED, mfd, 0);
    buffer = (unsigned char*) mbuf;

    fprintf(stderr, "buff: %u\n", *(unsigned int*)buffer);


    /*
    const int shm_fd = open(db_path, O_RDONLY, 0666);
    ftruncate(shm_fd, DB_SIZE);
    void* ptr = mmap(0, DB_SIZE, PROT_READ, MAP_PRIVATE, shm_fd, 0);
    buffer = (unsigned char*) ptr;
    */

    fprintf(stdout, "READY\n");
    fflush(stdout);

    while (1) {
        memset(command, 0, MAX_CMD_LEN);
        fgets(command, MAX_CMD_LEN, stdin);

        //LOOKUP
        if (command[5] == 'P') {
            // if (!loaded){
            //     fprintf(stdout, "ERROR: Lookup requested before database was ever loaded\n");//stderr
            //     goto FAILED;
            // }
            const char* answer = PerformLookup(&command[7]);
            fprintf(stdout, "%s\n", answer);
        }
        else if (command[0] == 'L') {
            /* loaded = LoadDatabase(db_path, buffer, DB_SIZE); */
            /* if (!loaded) { */
            /*     fprintf(stdout, "ERROR: DB open failed\n");//stderr */
            /*     goto FAILED; */
            /* } */
            // if (!loaded){
            //     fprintf(stdout, "ERROR: DB open failed\n");//stderr
            //     goto FAILED;
            // }
            fprintf(stdout, "OK\n");
        }
        else if (command[0] == 'E') {
            fprintf(stdout, "OK\n");
            break;
        } else {
            fprintf(stderr, "ERROR: Unknown command: %s\n", command);//stderr
            fprintf(stdout, "FAILED\n");
            goto FAILED;
        }
        fflush(stdout);
    }
    shm_unlink(name);
    return EXIT_SUCCESS;
FAILED:
    fflush(stdout);
    shm_unlink(name);
    return EXIT_FAILURE;

}

