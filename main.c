#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static size_t bufsize = 256;

int validate_options(const char *options, char *parsed_options,
                     size_t options_len);

int validate_bufsize(const char *argument);

size_t validate_buf_int(const char *argument);

int open_file(const char *filename);

int print_files_data(int file_count, int option_size, char *argv[],
                     const char *options);

int main(int argc, char *argv[]) {
    int file_count = argc - 1;
    int option_size = 0;

    bool is_options = false;
    bool is_bufsize = false;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s [-nA] [--bufsize] SIZE FILE...\n", argv[0]);
        return 1;
    }

    /*
     * Parse -nA / -An / -n / -A
     */
    if (argv[1][0] == '-') {
        size_t options_len = strlen(argv[1]) - 1;

        if (options_len == 0) {
            fprintf(stderr, "No options given\n");
            return 1;
        }

        char options[options_len];

        if (validate_options(argv[1], options, options_len) != 0) {
            return 1;
        }

        option_size++;
        is_options = true;

        /*
         * Check for --bufsize after the option.
         */
        if (argc > 2 && strcmp(argv[2], "--bufsize") == 0) {
            if (argc <= 3) {
                fprintf(stderr, "Missing bufsize value\n");
                return 1;
            }

            if (validate_bufsize(argv[2]) != 0) {
                return 1;
            }

            bufsize = validate_buf_int(argv[3]);

            if (bufsize == 0) {
                return 1;
            }

            option_size += 2;
            is_bufsize = true;
        }

        /*
         * Make sure at least one file exists.
         */
        if (argc <= option_size + 1) {
            fprintf(stderr, "No input files provided\n");
            return 1;
        }

        if (print_files_data(file_count, option_size, argv,
                             is_options ? options : NULL) != 0) {
            return 1;
        }

        return 0;
    }

    /*
     * No options.
     *
     * Check whether --bufsize is the first argument.
     */
    if (strcmp(argv[1], "--bufsize") == 0) {
        if (argc <= 2) {
            fprintf(stderr, "Missing bufsize value\n");
            return 1;
        }

        if (validate_bufsize(argv[1]) != 0) {
            return 1;
        }

        bufsize = validate_buf_int(argv[2]);

        if (bufsize == 0) {
            return 1;
        }

        option_size = 2;
        is_bufsize = true;

        if (argc <= option_size + 1) {
            fprintf(stderr, "No input files provided\n");
            return 1;
        }

        if (print_files_data(file_count, option_size, argv, NULL) != 0) {
            return 1;
        }

        return 0;
    }

    /*
     * No options and no bufsize.
     */
    if (print_files_data(file_count, option_size, argv, NULL) != 0) {
        return 1;
    }

    (void)is_bufsize;

    return 0;
}

int print_files_data(int file_count, int option_size, char *argv[],
                     const char *options) {
    bool is_option_n = false;
    bool is_option_A = false;

    file_count -= option_size;

    /*
     * Parse options.
     */
    if (options != NULL) {
        for (size_t i = 0; i < (size_t)(option_size - 1); i++) {
            if (options[i] == 'n') {
                is_option_n = true;
            } else if (options[i] == 'A') {
                is_option_A = true;
            } else {
                fprintf(stderr, "Invalid option: %c\n", options[i]);
                return 1;
            }
        }
    }

    /*
     * Allocate one input buffer.
     */
    char *line = malloc(bufsize);

    if (line == NULL) {
        perror("malloc");
        return 1;
    }

    /*
     * Keep line number across all files, like cat -n.
     */
    size_t line_number = 1;

    /*
     * Process every file.
     */
    for (int file_index = 0; file_index < file_count; file_index++) {

        const char *filename = argv[file_index + option_size + 1];

        int fd = open_file(filename);

        if (fd == -1) {
            fprintf(stderr, "Error opening file: %s: %s\n", filename,
                    strerror(errno));

            free(line);
            return 1;
        }

        bool is_line_start = true;
        ssize_t bytes_read;

        /*
         * Read the file in chunks.
         */
        while ((bytes_read = read(fd, line, bufsize)) > 0) {

            for (size_t j = 0; j < (size_t)bytes_read; j++) {

                /*
                 * We are at the beginning of a new line.
                 */
                if (is_line_start) {

                    if (is_option_n) {
                        char number[32];

                        int len = snprintf(number, sizeof(number),
                                           "%zu: ", line_number);

                        if (len < 0) {
                            fprintf(stderr, "Error formatting line number\n");
                            close(fd);
                            free(line);
                            return 1;
                        }

                        if (write(STDOUT_FILENO, number, (size_t)len) == -1) {
                            perror("write");
                            close(fd);
                            free(line);
                            return 1;
                        }
                    }

                    is_line_start = false;
                }

                /*
                 * Newline.
                 */
                if (line[j] == '\n') {

                    /*
                     * -A:
                     *
                     * newline:
                     *     \n
                     *
                     * becomes:
                     *     $\n
                     */
                    if (is_option_A) {
                        if (write(STDOUT_FILENO, "$", 1) == -1) {
                            perror("write");
                            close(fd);
                            free(line);
                            return 1;
                        }
                    }

                    if (write(STDOUT_FILENO, "\n", 1) == -1) {
                        perror("write");
                        close(fd);
                        free(line);
                        return 1;
                    }

                    line_number++;
                    is_line_start = true;

                } else {

                    /*
                     * Normal character.
                     */
                    if (write(STDOUT_FILENO, &line[j], 1) == -1) {
                        perror("write");
                        close(fd);
                        free(line);
                        return 1;
                    }
                }
            }
        }

        /*
         * read() returned -1.
         */
        if (bytes_read == -1) {
            fprintf(stderr, "Error reading %s: %s\n", filename,
                    strerror(errno));

            close(fd);
            free(line);
            return 1;
        }

        if (close(fd) == -1) {
            fprintf(stderr, "Error closing %s: %s\n", filename,
                    strerror(errno));

            free(line);
            return 1;
        }
    }

    free(line);

    return 0;
}

int validate_options(const char *options, char *parsed_options,
                     size_t options_len) {
    for (size_t i = 0; i < options_len; i++) {

        char option = options[i + 1];

        if (option != 'n' && option != 'A') {
            fprintf(stderr, "Invalid option: %c\n", option);
            return 1;
        }

        parsed_options[i] = option;
    }

    return 0;
}

int validate_bufsize(const char *argument) {
    if (strcmp(argument, "--bufsize") != 0) {
        fprintf(stderr, "Invalid option: %s\n", argument);
        return 1;
    }

    return 0;
}

size_t validate_buf_int(const char *argument) {
    if (argument == NULL || *argument == '\0') {
        fprintf(stderr, "Invalid bufsize\n");
        return 0;
    }

    for (size_t i = 0; argument[i] != '\0'; i++) {
        if (!isdigit((unsigned char)argument[i])) {
            fprintf(stderr, "Invalid bufsize: %s\n", argument);
            return 0;
        }
    }

    errno = 0;

    char *endptr;

    long value = strtol(argument, &endptr, 10);

    if (errno == ERANGE || value <= 0 || *endptr != '\0') {
        fprintf(stderr, "Invalid bufsize: %s\n", argument);
        return 0;
    }

    return (size_t)value;
}

int open_file(const char *filename) { return open(filename, O_RDONLY); }
