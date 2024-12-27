
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>



#define MAGIC_VALUE "BW"
#define MIN_VERSION 99
#define MAX_VERSION 140
#define EXACT_SECTIONS 2
#define MIN_SECTIONS 4
#define MAX_SECTIONS 18
#define VALID_SECTION_TYPES {32, 63, 97, 37, 54, 76, 78}
//#define LINE_DELIMITER 0x0A
#define LINE_DELIMITER '\n'
#define LINE_BYTES 10


void listDirectory(char *basePath, int recursive , int permFilter, char *suffix) {
    struct dirent *dp;
    DIR *dir = opendir(basePath);
    struct stat statbuf;

    if (!dir) {
        printf("ERROR\ninvalid directory path\n");
        return;
    }

    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
            char fullPath[1024];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", basePath, dp->d_name);

            if (stat(fullPath, &statbuf) == -1) {
                continue;
            }

            

            if (permFilter && !(statbuf.st_mode & S_IXUSR)) {
                continue;
            }

            if (suffix && *suffix) {
                size_t lenName = strlen(dp->d_name);
                size_t lenSuffix = strlen(suffix);
                if (lenName < lenSuffix || strcmp(dp->d_name + lenName - lenSuffix, suffix) != 0) {
                    continue;
                }
            }

            printf("%s\n", fullPath);

            if (recursive && S_ISDIR(statbuf.st_mode)) {
                listDirectory(fullPath, recursive, permFilter, suffix);
            }
        }
    }

    closedir(dir);
}


void parseSFFile(const char *filePath) {
    int validSectionTypes[] = VALID_SECTION_TYPES;
    int fd = open(filePath, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return;
    }

    char magic[3] = {0};
    if (read(fd, magic, 2) != 2 || strcmp(magic, MAGIC_VALUE) != 0) {
        printf("ERROR\nwrong magic\n");
        close(fd);
        return;
    }

    uint16_t header_size;
    if (read(fd, &header_size, sizeof(header_size)) != sizeof(header_size)) {
        printf("ERROR\nwrong header size\n");
        close(fd);
        return;
    }

    uint32_t version;
    if (read(fd, &version, sizeof(version)) != sizeof(version)) {
        printf("ERROR\nwrong version\n");
        close(fd);
        return;
    }

    // No need to convert if file and system endianess match
    // version = ntohl(version);

    if (version < MIN_VERSION || version > MAX_VERSION) {
        printf("ERROR\nwrong version\n");
        close(fd);
        return;
    }

    uint8_t sections;
    if (read(fd, &sections, sizeof(sections)) != sizeof(sections)) {
        perror("Error reading number of sections");
        close(fd);
        return;
    }

    if (!(sections == EXACT_SECTIONS || (sections >= MIN_SECTIONS && sections <= MAX_SECTIONS))) {
        printf("ERROR\nwrong sect_nr\n");
        close(fd);
        return;
    }

    // First pass: validate all sections
    for (int i = 0; i < sections; i++) {
        lseek(fd, 12, SEEK_CUR);  // Skip section name

        uint8_t sect_type;
        if (read(fd, &sect_type, sizeof(sect_type)) != sizeof(sect_type)) {
            perror("Error reading section type");
            close(fd);
            return;
        }

        int typeIsValid = 0;
        for (int j = 0; j < sizeof(validSectionTypes) / sizeof(int); j++) {
            if (sect_type == validSectionTypes[j]) {
                typeIsValid = 1;
                break;
            }
        }
        if (!typeIsValid) {
            printf("ERROR\nwrong sect_types\n");
            close(fd);
            return;
        }

        lseek(fd, 8, SEEK_CUR);  // Skip section offset and size
    }

    // All sections are valid; rewind and print details
    lseek(fd, 2 + sizeof(header_size) + sizeof(version) + 1, SEEK_SET); // Rewind to section details

    printf("SUCCESS\nversion=%u\nnr_sections=%u\n", version, sections);

    for (int i = 0; i < sections; i++) {
        char sect_name[13] = {0};
        uint8_t sect_type;
        uint32_t sect_offset, sect_size;

        read(fd, sect_name, 12);
        sect_name[12] = '\0';

        read(fd, &sect_type, sizeof(sect_type));
        read(fd, &sect_offset, sizeof(sect_offset));
        read(fd, &sect_size, sizeof(sect_size));

        printf("section%d: %s %u %u\n", i + 1, sect_name, sect_type, sect_size);
    }

    close(fd);
}


void findAllSF(const char *dirPath) {
    DIR *dir = opendir(dirPath);
    if (!dir) {
        perror("ERROR\ninvalid directory path");
        return;
    }


    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dirPath, entry->d_name);
            findAllSF(path);  // Recursively call findAllSF for directories
        } else if (entry->d_type == DT_REG) {
            char filePath[1024];
            snprintf(filePath, sizeof(filePath), "%s/%s", dirPath, entry->d_name);
            
            int fd = open(filePath, O_RDONLY);
            if (fd == -1) {
                continue;  // If the file cannot be opened, move to the next one
            }

            char magic[3] = {0};
            if (read(fd, magic, 2) != 2 || strncmp(magic, MAGIC_VALUE, 2) != 0) {
                close(fd);
                continue;
            }

            uint16_t header_size;
            if (read(fd, &header_size, sizeof(header_size)) != sizeof(header_size)) {
                close(fd);
                continue;
            }

            uint32_t version;
            if (read(fd, &version, sizeof(version)) != sizeof(version)) {
                close(fd);
                continue;
            }

            if (version < MIN_VERSION || version > MAX_VERSION) {
                close(fd);
                continue;
            }

            uint8_t sections;
            if (read(fd, &sections, sizeof(sections)) != sizeof(sections)) {
                close(fd);
                continue;
            }

            int hasLargeSection = 0;
            for (int i = 0; i < sections; i++) {
                lseek(fd, 12, SEEK_CUR); // Skip section name

                uint8_t sect_type;
                if (read(fd, &sect_type, sizeof(sect_type)) != sizeof(sect_type)) {
                    break;  // Break the loop if reading sect_type fails
                }

                // Assuming section type validation is done here...

                uint32_t sect_size;
                lseek(fd, 4, SEEK_CUR); // Skip section offset
                if (read(fd, &sect_size, sizeof(sect_size)) != sizeof(sect_size)) {
                    break;  // Break the loop if reading sect_size fails
                }

                if (sect_size > 15 * LINE_BYTES) { // Check if section has more than 15 lines
                    hasLargeSection = 1;
                    break;
                }
            }

            close(fd);

            if (hasLargeSection) {
                printf("%s\n", filePath);  // Print the file path if it has a large section
            }
        }
    }
    closedir(dir);
}



void extractLine(const char *filePath, int sectionNumber, int lineNumber) {
    int fd = open(filePath, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return;
    }

    // Skipping the magic value and header size
    lseek(fd, 2 + sizeof(uint16_t) + sizeof(uint16_t), SEEK_CUR);

    uint8_t sections;
    if (read(fd, &sections, sizeof(sections)) != sizeof(sections)) {
        perror("Error reading number of sections");
        close(fd);
        return;
    }

    printf("Debug: Total sections = %u\n", sections);

     if (!(sections == EXACT_SECTIONS || (sections >= MIN_SECTIONS && sections <= MAX_SECTIONS))) {
        printf("ERROR\nInvalid section number\n");
        close(fd);
        return;
    }

    // Iterate over sections to find the correct one
    for (int i = 0; i < sections; i++) {
        char sect_name[13] = {0};
        uint8_t sect_type;
        uint32_t sect_offset, sect_size;

        if (read(fd, sect_name, 12) != 12) {
            perror("Error reading section name");
            close(fd);
            return;
        }
        sect_name[12] = '\0'; // Ensure null termination

        if (read(fd, &sect_type, sizeof(sect_type)) != sizeof(sect_type)) {
            perror("Error reading section type");
            close(fd);
            return;
        }

        if (read(fd, &sect_offset, sizeof(sect_offset)) != sizeof(sect_offset) ||
            read(fd, &sect_size, sizeof(sect_size)) != sizeof(sect_size)) {
            perror("Error reading section offset/size");
            close(fd);
            return;
        }

        if (i + 1 == sectionNumber) {
            lseek(fd, sect_offset, SEEK_SET);
            char *content = malloc(sect_size + 1);
            if (!content) {
                perror("Memory allocation failed");
                close(fd);
                return;
            }
            if (read(fd, content, sect_size) != sect_size) {
                perror("Error reading section content");
                free(content);
                close(fd);
                return;
            }
            content[sect_size] = '\0'; // Null-terminate the content

            // Now content contains the whole section. Find and print the desired line.
            int currentLine = 1;
            char *lineStart = content;
            for (char *p = content; *p; p++) {
                if (*p == LINE_DELIMITER) {
                    if (currentLine == lineNumber) {
                        *p = '\0'; // Null-terminate the line
                        printf("SUCCESS\n%s\n", lineStart);
                        free(content);
                        close(fd);
                        return;
                    }
                    currentLine++;
                    lineStart = p + 1;
                }
            }

            printf("ERROR\nLine number out of range.\n");
            free(content);
            close(fd);
            return;
        }
    }

    printf("ERROR\nSection number %d not found.\n", sectionNumber);
    close(fd);
}


int main(int argc, char *argv[]) {
    int recursive = 0;
    char *path = NULL;
    int variantFlag = 0;
    int listFlag = 0;
    int parseFlag = 0;
    int permFilter = 0;
    int extractFlag = 0;
    int sectionNumber = 0;
    int lineNumber = 0;
    int findallFlag = 0;
    
    char *suffix = NULL;  // For name_ends_with

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "findall") == 0) {
            findallFlag = 1;
        } else if (strcmp(argv[i], "variant") == 0) {
            variantFlag = 1;
        } else if (strcmp(argv[i], "list") == 0) {
            listFlag = 1;
        } else if (strcmp(argv[i], "parse") == 0) {
            parseFlag = 1;
        } else if (strcmp(argv[i], "recursive") == 0) {
            recursive = 1;
        } else if (strncmp(argv[i], "path=", 5) == 0) {
            path = argv[i] + 5;
        } else if (strcmp(argv[i], "has_perm_execute") == 0) {
            permFilter = 1;
        } else if (strncmp(argv[i], "name_ends_with=", 15) == 0) {
            suffix = argv[i] + 15;
        } else if (strcmp(argv[i], "extract") == 0) {
            extractFlag = 1;
        } else if (strncmp(argv[i], "section=", 8) == 0) {
            sectionNumber = atoi(argv[i] + 8);
        } else if (strncmp(argv[i], "line=", 5) == 0) {
            lineNumber = atoi(argv[i] + 5);
        }
    }

    if (variantFlag) {
        printf("96375\n");
    } else if (path && listFlag) {
        printf("SUCCESS\n");
        listDirectory(path, recursive, permFilter, suffix);
    } else if (path && parseFlag) {
        parseSFFile(path);
    } else if(path && extractFlag){
        extractLine(path, sectionNumber, lineNumber);
    } else if (path && findallFlag) {
        printf("SUCCESS\n");
        findAllSF(path);
    } else {
        printf("ERROR\ninvalid command or directory path\n");
    }

    return 0;
}
