#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fcntl.h"

// Function to check if a file exists
int file_exists(char *filename) {
    int fd = open(filename, O_RDONLY);
    if(fd < 0)
        return 0;
    close(fd);
    return 1;
}

// Function to convert integer to string
void itoa(int num, char *str) {
    int i = 0;
    int is_negative = 0;
    
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    // Handle case of zero explicitly
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    
    // Process digits
    int start = i;
    while (num != 0) {
        str[i++] = num % 10 + '0';
        num = num / 10;
    }
    
    if (is_negative)
        str[i++] = '-';
        
    str[i] = '\0';
    
    // Reverse the string
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

// Function to generate numbered filename
void generate_numbered_name(char *base_name, char *numbered_name, int num) {
    char numstr[16];
    int i = 0, j = 0;
    char *dot = 0;
    
    // Find the dot in the filename if it exists
    while(base_name[i] != '\0') {
        if(base_name[i] == '.')
            dot = &base_name[i];
        i++;
    }
    
    i = 0;
    if(dot) {
        // Copy filename up to dot
        while(&base_name[i] != dot) {
            numbered_name[j++] = base_name[i++];
        }
    } else {
        // Copy entire filename
        while(base_name[i] != '\0') {
            numbered_name[j++] = base_name[i++];
        }
    }
    
    // Add the number in parentheses
    numbered_name[j++] = '(';
    itoa(num, numstr);
    i = 0;
    while(numstr[i] != '\0') {
        numbered_name[j++] = numstr[i++];
    }
    numbered_name[j++] = ')';
    
    // Add the extension if it exists
    if(dot) {
        i = 0;
        while(dot[i] != '\0') {
            numbered_name[j++] = dot[i++];
        }
    }
    numbered_name[j] = '\0';
}

int main(int argc, char *argv[]) {
    int fd;
    char filename[128];
    char numbered_filename[128];
    
    // Set default or use provided filename
    if (argc == 1) {
        memmove(filename, "untitled.txt", 12);
    } else {
        memmove(filename, argv[1], strlen(argv[1]) + 1);
    }

    // Try to open file normally first
    if (!file_exists(filename)) {
        fd = open(filename, O_CREATE | O_RDWR);
        if(fd < 0) {
            printf("Error: couldn't create file %s\n", filename);
            exit(1);
        }
        printf("Created: %s\n", filename);
        close(fd);
        exit(0);
    }
    
    // If we get here, file exists, so try numbered versions
    int counter = 1;
    while(1) {
        generate_numbered_name(filename, numbered_filename, counter);
        
        // Check if this numbered version exists
        if(!file_exists(numbered_filename)) {
            // Create the file with this number
            fd = open(numbered_filename, O_CREATE | O_RDWR);
            if(fd < 0) {
                printf("Error: couldn't create file %s\n", numbered_filename);
                exit(1);
            }
            printf("Created: %s\n", numbered_filename);
            close(fd);
            break;
        }
        
        counter++;
        if(counter > 999) {  // Reasonable limit to prevent infinite loop
            printf("Error: too many files with similar names\n");
            exit(1);
        }
    }
    
    exit(0);
}

