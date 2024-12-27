#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define MAGIC_SIZE 2
#define HEADER_SIZE 12
#define SECTION_NAME_SIZE 12
#define RESPONSE_BUFFER_SIZE 50
#define FILE_NAME_SIZE 250

int reqPipe, resPipe;
int sharedMemoryFD;
char *sharedMem = NULL, *fileMapped = NULL;
unsigned int sharedMemSize, fileMappedSize;

void respondToPing() {
    const char *msg = "PING$";
    write(resPipe, msg, strlen(msg));

    unsigned int num = 96375;
    write(resPipe, &num, sizeof(num));

    const char *reply = "PONG$";
    write(resPipe, reply, strlen(reply));
}

void initSharedMemory() {
    unsigned int size;
    read(reqPipe, &size, sizeof(size));
    char reply[RESPONSE_BUFFER_SIZE];

    sharedMemoryFD = shm_open("/N2rAdU", O_CREAT | O_RDWR, 0664);
    if (sharedMemoryFD < 0) {
        snprintf(reply, sizeof(reply), "CREATE_SHM$ERROR$");
        write(resPipe, reply, strlen(reply));
        return;
    }

    if (ftruncate(sharedMemoryFD, size) == -1) {
        snprintf(reply, sizeof(reply), "CREATE_SHM$ERROR$");
        write(resPipe, reply, strlen(reply));
        close(sharedMemoryFD);
        shm_unlink("/N2rAdU");
        return;
    }

    sharedMem = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemoryFD, 0);
    if (sharedMem == MAP_FAILED) {
        snprintf(reply, sizeof(reply), "CREATE_SHM$ERROR$");
        write(resPipe, reply, strlen(reply));
        close(sharedMemoryFD);
        shm_unlink("/N2rAdU");
        return;
    }

    snprintf(reply, sizeof(reply), "CREATE_SHM$SUCCESS$");
    write(resPipe, reply, strlen(reply));
    sharedMemSize = size;
}

void writeSharedMemory() {
    unsigned int offset, value;
    read(reqPipe, &offset, sizeof(offset));
    read(reqPipe, &value, sizeof(value));
    char reply[RESPONSE_BUFFER_SIZE];

    if (offset + sizeof(value) > sharedMemSize) {
        snprintf(reply, sizeof(reply), "WRITE_TO_SHM$ERROR$");
        write(resPipe, reply, strlen(reply));
        return;
    }

    unsigned int *memPtr = (unsigned int *)(sharedMem + offset);
    *memPtr = value;

    snprintf(reply, sizeof(reply), "WRITE_TO_SHM$SUCCESS$");
    write(resPipe, reply, strlen(reply));
}

void mapFileToMemory() {
    char fileName[FILE_NAME_SIZE];
    int i = 0;
    char ch;
    while (read(reqPipe, &ch, 1) && ch != '$') {
        fileName[i++] = ch;
    }
    fileName[i] = '\0';

    int fileFD = open(fileName, O_RDONLY);
    char reply[RESPONSE_BUFFER_SIZE];
    if (fileFD < 0) {
        snprintf(reply, sizeof(reply), "MAP_FILE$ERROR$");
        write(resPipe, reply, strlen(reply));
        return;
    }

    struct stat fileStat;
    if (fstat(fileFD, &fileStat) < 0) {
        snprintf(reply, sizeof(reply), "MAP_FILE$ERROR$");
        write(resPipe, reply, strlen(reply));
        close(fileFD);
        return;
    }
    fileMappedSize = fileStat.st_size;

    fileMapped = mmap(0, fileMappedSize, PROT_READ, MAP_SHARED, fileFD, 0);
    if (fileMapped == MAP_FAILED) {
        snprintf(reply, sizeof(reply), "MAP_FILE$ERROR$");
        write(resPipe, reply, strlen(reply));
        close(fileFD);
        return;
    }

    snprintf(reply, sizeof(reply), "MAP_FILE$SUCCESS$");
    write(resPipe, reply, strlen(reply));
    close(fileFD);
}

void readFileOffset() {
    unsigned int offset, numBytes;
    read(reqPipe, &offset, sizeof(offset));
    read(reqPipe, &numBytes, sizeof(numBytes));
    char reply[RESPONSE_BUFFER_SIZE];

    if (offset + numBytes > fileMappedSize) {
        snprintf(reply, sizeof(reply), "READ_FROM_FILE_OFFSET$ERROR$");
        write(resPipe, reply, strlen(reply));
        return;
    }

    memcpy(sharedMem, fileMapped + offset, numBytes);

    snprintf(reply, sizeof(reply), "READ_FROM_FILE_OFFSET$SUCCESS$");
    write(resPipe, reply, strlen(reply));
}

void readFileSection() {
    unsigned int sectionNum, offset, numBytes;
    read(reqPipe, &sectionNum, sizeof(sectionNum));
    read(reqPipe, &offset, sizeof(offset));
    read(reqPipe, &numBytes, sizeof(numBytes));
    char reply[RESPONSE_BUFFER_SIZE];

    char *topHeader = fileMapped;
    unsigned char sectionCount = topHeader[6]; // Read numberOfSections from topHeader

    if (sectionNum > sectionCount) {
        snprintf(reply, sizeof(reply), "READ_FROM_FILE_SECTION$ERROR$");
        write(resPipe, reply, strlen(reply));
        return;
    }

    char *sectionHeader = fileMapped + HEADER_SIZE + (sectionNum - 1) * SECTION_NAME_SIZE;
    unsigned int sectionOffset = *(unsigned int *)(sectionHeader + 13);
    unsigned int sectionSize = *(unsigned int *)(sectionHeader + 17);

    if (offset + numBytes > sectionSize) {
        snprintf(reply, sizeof(reply), "READ_FROM_FILE_SECTION$ERROR$");
        write(resPipe, reply, strlen(reply));
        return;
    }

    memcpy(sharedMem, fileMapped + sectionOffset + offset, numBytes);

    snprintf(reply, sizeof(reply), "READ_FROM_FILE_SECTION$SUCCESS$");
    write(resPipe, reply, strlen(reply));
}

int main(int argc, char **argv) {
    const char *responsePath = "RESP_PIPE_96375";
    const char *requestPath = "REQ_PIPE_96375";

    if (mkfifo(responsePath, 0777) < 0) {
        printf("ERROR\ncannot create the response pipe\n");
        return 1;
    }

    reqPipe = open(requestPath, O_RDONLY);
    if (reqPipe < 0) {
        printf("ERROR\ncannot open the request pipe\n");
        unlink(responsePath);
        return 1;
    }

    resPipe = open(responsePath, O_WRONLY);
    if (resPipe < 0) {
        printf("ERROR\ncannot open the response pipe\n");
        close(reqPipe);
        unlink(responsePath);
        return 1;
    }

    if (write(resPipe, "CONNECT$", 8) != 8) {
        printf("ERROR\nfailed to write CONNECT message\n");
        close(reqPipe);
        close(resPipe);
        unlink(responsePath);
        return 1;
    }

    printf("SUCCESS\n");

    char cmd[50];
    while (1) {
        int size = 0;
        char ch;
        while (read(reqPipe, &ch, 1) && ch != '$') {
            cmd[size++] = ch;
        }
        cmd[size] = '\0';

        if (strcmp(cmd, "PING") == 0) {
            respondToPing();
        } else if (strcmp(cmd, "CREATE_SHM") == 0) {
            initSharedMemory();
        } else if (strcmp(cmd, "WRITE_TO_SHM") == 0) {
            writeSharedMemory();
        } else if (strcmp(cmd, "MAP_FILE") == 0) {
            mapFileToMemory();
        } else if (strcmp(cmd, "READ_FROM_FILE_OFFSET") == 0) {
            readFileOffset();
        } else if (strcmp(cmd, "READ_FROM_FILE_SECTION") == 0) {
            readFileSection();
        } else if (strcmp(cmd, "EXIT") == 0) {
            break;
        }
    }

    if (sharedMem != NULL) {
        munmap(sharedMem, sharedMemSize);
        shm_unlink("/N2rAdU");
    }
    if (fileMapped != NULL) {
        munmap(fileMapped, fileMappedSize);
    }

    close(reqPipe);
    close(resPipe);
    unlink(responsePath);
    return 0;
}
