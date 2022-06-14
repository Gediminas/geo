#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

unsigned char* buffer = NULL;
int loaded = 0;
char command[25];
const int DB_SIZE = 13193167;
const unsigned long seg1_start = 4;
const unsigned long seg1_size  = 256 * 256 * 8; //All combinations of IP octet1 & octet2
const unsigned long seg2_start = seg1_start + seg1_size;

int LoadDatabase(const char* db_path, unsigned char* buffer, int size) {
    FILE *file = fopen(db_path, "rb");
    if (!file) {
        return 0;
    }
    const int bytes = fread(buffer, 1, size, file);
    fclose(file);
    loaded = 1;
    return bytes;
}

const char* PerformLookup(const char* ip) {
    const in_addr_t inet_ip         = inet_addr(ip);
    const unsigned short halfip     = htons(inet_ip); //little-endian
    const unsigned char ip_octet3   = *(((const unsigned char*)&inet_ip)+2);
    const unsigned long seg3_start  = *(const unsigned int*)&buffer[0];
    const unsigned long seg1_offset = seg1_start + halfip * 8;
    const unsigned long seg2_offset = *(const unsigned int*)&buffer[seg1_offset];
    const unsigned long count2      = *(const unsigned int*)&buffer[seg1_offset + 4];

    unsigned long seg2_addr = seg2_start + seg2_offset;

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

    unsigned long seg3_value = *(unsigned int*)&buffer[seg2_addr];
    unsigned long seg3_offset = seg3_value >> 8;
    unsigned long seg3_addr = seg3_start + seg3_offset;
    const unsigned char* city = &buffer[seg3_addr];
    return city;
}

void test(const char* ip) {
    printf("%s\t%s\n", ip, PerformLookup(ip));
}

int main(int argc, char** argv) {
    // Testing
    if (argc == 3) {
        buffer = malloc(DB_SIZE);
        const char *db_path = argv[1];
        const int bytes = LoadDatabase(db_path, buffer, DB_SIZE);
        if (bytes < 1) {
            fprintf(stderr, "ERROR: DB open failed\n");//stderr
            goto FAILED;
        }
        printf("DB size: %d\n", bytes);
        test("0.0.0.0");
        test("0.1.0.0");
        test("1.0.0.0");
        test("1.2.3.4");
        test("8.8.8.8");
        test("10.0.0.0");
        test("53.103.143.255");
        test("53.103.144.0");
        test("53.103.255.255");
        test("53.104.0.0");
        test("53.255.255.255");
        test("79.238.202.1");
        test("197.211.217.7");
        test("255.255.255.255");
        free(buffer);
        return EXIT_SUCCESS;
    }

    if (argc != 2) {
        fprintf(stderr, "ERROR: Usage './geo <database.csv>'\n");
        goto FAILED;
    }

    const char *db_path = argv[1];
    buffer = malloc(DB_SIZE);

    printf("READY\n");
    fflush(stdout);

    while (1) {
        fgets(command, 25, stdin);
        /* fprintf(stderr, "%s", command); */

        //LOOKUP
        if (command[5] == 'P') {
            if (!loaded){
                fprintf(stderr, "ERROR: Lookup requested before database was ever loaded\n");//stderr
                goto FAILED;
            }
            const char* ip = &command[7];
            const char* answer = PerformLookup(ip);
            printf("%s\n", answer);
            fflush(stdout);
        }
        else if (command[0] == 'L') {
            const int bytes = LoadDatabase(db_path, buffer, DB_SIZE);
            if (bytes < 1024) {
                fprintf(stderr, "ERROR: DB open failed\n");//stderr
                goto FAILED;
            }
            printf("OK\n");
            fflush(stdout);
        }
        else if (command[0] == 'E') {
            printf("OK\n");
            fflush(stdout);
            break;
        } else {
            fprintf(stderr, "ERROR: Unknown command received\n");//stderr
            goto FAILED;
        }
    }
    free(buffer);
    return EXIT_SUCCESS;
FAILED:
    free(buffer);
    return EXIT_FAILURE;
}
