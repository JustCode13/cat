#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static size_t bufsize = 0;
static bool is_options = false;
static bool is_bufsize = false;

int validate_options(char *options, char *address, size_t options_len);

int validate_bufsize(char *argv);

size_t validate_buf_int(char *argv);

int open_file(int *fd, char *filename);

int read_line(int file_fd, char *line);

int print_files_data(int file_count, int option_size, char *argv[]);

int main(int argc, char *argv[]) {

    int file_count = argc - 1;
    int option_size = 0;

    if (argc > 2) {

        // Check if options

        size_t options_len = argv[1] ? strlen(argv[1]) - 1 : 0;
        char options[options_len];

        if (argv[1][0] == '-') {

            if (validate_options(argv[1], options, options_len) != 0) {
                return 1;
            }

            option_size += 1;
            is_options = true;
        }

        // Check for valid bufsize

        if (argv[2] && argv[2][0] == '-' && argv[2][1] == '-') {
            if (validate_bufsize(argv[2]) != 0) {
                return 1;
            }

            option_size += 2;
            is_bufsize = true;
        }

        // validate integers

        bufsize = argv[3] ? validate_buf_int(argv[3]) : 0;

        if (bufsize == 0) {
            return 1;
        }

        if (print_files_data(file_count, option_size, argv) != 0) {
            return 1;
        }
    } else {
        if (print_files_data(file_count, option_size, argv) != 0) {
            return 1;
        }
    }

    return 0;
}

int print_files_data(int file_count, int option_size, char *argv[]) {
    char *files[file_count];
    int file_fds[file_count];

    file_count -= option_size;

    // open file
    for (int i = 0; i < file_count; i++) {
        files[i] = argv[i + option_size + 1];

        open_file(&file_fds[i], files[i]);

        if (file_fds[i] == -1) {
            printf("Error opening file: %s\n", files[i]);
            return 1;
        }
    }

    char *line = malloc(is_bufsize ? bufsize : 256);

    if (line == NULL) {
        printf("Error allocating memory\n");
        return 1;
    }

    for (int i = 0; i < file_count; i++) {

        ssize_t bytes_read;

        while ((bytes_read = read(file_fds[i], line, sizeof(line) - 1)) > 0) {

            line[bytes_read] = '\0';
            write(STDOUT_FILENO, line, (size_t)bytes_read);
        }

        close(file_fds[i]);
    }

    return 0;
}

int read_line(int file_fd, char *line) {
    ssize_t bread = read(file_fd, line, bufsize);

    if (bread == -1 || bread == 0) {
        return 1;
    }

    return 0;
}

int validate_options(char *options, char *address, size_t options_len) {
    if (options_len == 0) {
        printf("No options given\n");
        return 1;
    }

    for (size_t i = 0; i < options_len; i++) {
        if (strchr("nA", options[i + 1]) == NULL) {
            printf("Invalid option: %c\n", options[i + 1]);
            return 1;
        }

        address[i] = options[i + 1];
    }

    return 0;
}

int validate_bufsize(char *argv) {
    if (strstr(argv, "bufsize") == NULL) {
        printf("No --bufsize provided\n");
        return 1;
    }

    return 0;
}

size_t validate_buf_int(char *argv) {
    size_t length = strlen(argv);

    for (size_t i = 0; i < length; i++) {
        if (!isdigit(argv[i])) {
            printf("Invalid bufsize\n");
            return 0;
        }
    }

    char *endptr;

    long buffersize = strtol(argv, &endptr, 10);

    bufsize = (size_t)buffersize;

    return bufsize;
}

int open_file(int *fd, char *filename) {
    *fd = open(filename, O_RDONLY);

    if (*fd == -1) {
        return 1;
    }

    return 0;
}
