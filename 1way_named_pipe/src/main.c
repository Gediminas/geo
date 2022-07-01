#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>

const char* db_path = "geo.db"; //TODO: pass via arg
const char* fifo_name = "/tmp/geo_fifo";

#define CMD_MAX_LEN 24
#define IP_MAX_LEN  16

//TODO: check file size
#define DB_SIZE 13193167

// Skip the first 4 bytes (address to SEG3)
#define seg1_start 4

 //All combinations of IP octet1 & octet2
#define seg1_size  256 * 256 * 8
#define seg2_start seg1_start + seg1_size

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

const char* PerformLookup(const unsigned char* buffer, const in_addr_t ip) {
    // const unsigned short halfip = ((ip_be & 0xFF00) >> 8) | ((ip_be & 0x00FF) << 8); // to LE always => no gain
    const unsigned short halfip    = ntohs((unsigned short)ip); //to this machine endian (little-endian)
    const unsigned char ip_octet3  = *(((const unsigned char*)&ip)+2);

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

extern unsigned char* LoadDatabase(const char* db_path);
extern const char* PerformLookup(const unsigned char* buffer, const in_addr_t ip);

int parent() {
    mkfifo(fifo_name, 0666);
    const int fd = open(fifo_name, O_WRONLY); //TODO: error handling

    char input[CMD_MAX_LEN];
    in_addr_t ip;
    while (1) {
        //memset(input, 0, CMD_MAX_LEN);
        fgets(input, CMD_MAX_LEN, stdin);

        switch (input[3]) {
            case 'K': // LOOKUP
                ip = inet_addr(input+7);
                write(fd, &ip, 4);
                break;
            case 'D': // LOAD
                fprintf(stdout, "OK\n"); //TODO: if not loaded
                fflush_unlocked(stdout);
                break;
            case 'T': // EXIT
                fprintf(stdout, "OK\n"); //TODO: close child
                fflush_unlocked(stdout);
                exit(0);
                break;
            default:
                //TODO: empty input received
                //fprintf(stderr, "ERROR: Invalid command\n");
                //exit(0);
                break;
        }
    }
    return EXIT_SUCCESS;
}

int child() {
    mkfifo(fifo_name, 0666);
    const int fd = open(fifo_name, O_RDONLY); //TODO: error handling

    // Abusing specification hole => load before READY
    unsigned char* buffer = LoadDatabase(db_path);
    if (!buffer) {
        fprintf(stderr, "ERROR: Load failed\n");
        return EXIT_FAILURE;
    }

    fprintf(stdout, "READY\n");
    fflush_unlocked(stdout);

    in_addr_t ip;
    while (1) {
        read(fd, &ip, 4);
        fprintf(stdout, "%s\n", PerformLookup(buffer, ip));
        fflush_unlocked(stdout);
    }

    // free(buffer); //TODO: cleanup in signal-trap maybe / memory leaks!!!!
    return EXIT_SUCCESS;
}

int main() {
    const pid_t child_pid = fork();
    if (child_pid < 0)
        return -1;

    if (child_pid == 0)
        return child();

    return parent();

    //TODO: gracefully close child
    //waitpid(pid, NULL, 0) 
}
