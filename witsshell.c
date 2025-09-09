#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

#define MAX_LINE 1024
#define ERROR_MSG "An error has occurred\n"

char **PATHS = NULL;
int PATH_count = 0;

// Print a generic error
void print_error() {
    write(STDERR_FILENO, ERROR_MSG, strlen(ERROR_MSG));
}

// Initialize PATHS with default /bin/
void init_path() {
    PATHS = malloc(2 * sizeof(char*));
    PATHS[0] = strdup("/bin/");
    PATHS[1] = NULL;
    PATH_count = 1;
}

// Update PATHS for 'path' builtin
void update_path(char **args, int argc) {
    for (int i = 0; PATHS && PATHS[i]; i++) free(PATHS[i]);
    free(PATHS);

    if (argc <= 1) {
        PATHS = NULL;
        PATH_count = 0;
        return;
    }

    PATH_count = argc - 1;
    PATHS = malloc((PATH_count + 1) * sizeof(char*));
    for (int i = 0; i < PATH_count; i++) {
        PATHS[i] = strdup(args[i + 1]);
    }
    PATHS[PATH_count] = NULL;
}

// Process a single line of commands (supports &, builtins, redirection)
void process_command(char *cmd_line) {
    // Split by '&' for parallel commands
    char *parallel_cmds[100];
    int parallel_count = 0;
    char *pc_token = strtok(cmd_line, "&");
    while (pc_token && parallel_count < 100) {
        parallel_cmds[parallel_count++] = strdup(pc_token);
        pc_token = strtok(NULL, "&");
    }

    pid_t pids[100];
    int pid_count = 0;

    for (int i = 0; i < parallel_count; i++) {
        // Trim whitespace
        char *trimmed = parallel_cmds[i];
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        // Tokenize command
        int argc_tk = 0, capacity = 4;
        char **argv_tk = malloc(capacity * sizeof(char*));
        char *token = strtok(trimmed, " \t\n");
        while (token) {
            if (argc_tk >= capacity) {
                capacity *= 2;
                argv_tk = realloc(argv_tk, capacity * sizeof(char*));
            }
            argv_tk[argc_tk++] = strdup(token);
            token = strtok(NULL, " \t\n");
        }
        argv_tk[argc_tk] = NULL;

        // Remove surrounding double quotes from each argument
        for (int i = 0; i < argc_tk; i++) {
            size_t len = strlen(argv_tk[i]);
            if (len >= 2 && argv_tk[i][0] == '"' && argv_tk[i][len - 1] == '"') {
            // Shift string left by 1 and replace last char with null
            memmove(argv_tk[i], argv_tk[i] + 1, len - 2);
            argv_tk[i][len - 2] = '\0';
            }
        }


        if (argc_tk == 0) { free(argv_tk); continue; }

        // Determine if foreground (last command without trailing '&' is foreground)
        int is_background = 1;
        if (i == parallel_count - 1 && cmd_line[strlen(cmd_line)-1] != '&') {
            is_background = 0;
        }

        // Built-ins
        if (strcmp(argv_tk[0], "exit") == 0) {
            if (argc_tk != 1) {
                print_error();
            } else {
                for (int j = 0; j < argc_tk; j++) free(argv_tk[j]);
                free(argv_tk);
                if (!is_background) exit(0);
            }
            continue; 
        }

        if (strcmp(argv_tk[0], "cd") == 0) {
            if (argc_tk != 2 || chdir(argv_tk[1]) != 0) {
                print_error();
            }
            for (int j = 0; j < argc_tk; j++) free(argv_tk[j]);
            free(argv_tk);
            continue;
        }

        if (strcmp(argv_tk[0], "path") == 0) {
            update_path(argv_tk, argc_tk);
            for (int j = 0; j < argc_tk; j++) free(argv_tk[j]);
            free(argv_tk);
            continue;
        }

        // Resolve executable path
        // Resolve executable path
        char *full_path = NULL;
        for (int j = 0; PATHS && PATHS[j]; j++) {
            char candidate[1024];
            if (PATHS[j][strlen(PATHS[j]) - 1] == '/') {
                snprintf(candidate, sizeof(candidate), "%s%s", PATHS[j], argv_tk[0]);
            } else {
                snprintf(candidate, sizeof(candidate), "%s/%s", PATHS[j], argv_tk[0]);
            }
            if (access(candidate, X_OK) == 0) {
                full_path = strdup(candidate);
                break;
            }
        }

        if (!full_path) {
            print_error();
            for (int j = 0; j < argc_tk; j++) free(argv_tk[j]);
            free(argv_tk);
            continue;
        }

        // Fork and execute
        pid_t pid = fork();
        if (pid == 0) { // Child
            char *output = NULL;
            for (int j = 0; j < argc_tk; j++) {
                if (strcmp(argv_tk[j], ">") == 0) {
                    if (argv_tk[j+1] == NULL || argv_tk[j+2] != NULL) { 
                        print_error(); 
                        exit(1); 
                    }
                    output = argv_tk[j+1];
                    argv_tk[j] = NULL;
                    break;
                }
            }
            if (output) {
                int fd = open(output, O_CREAT | O_TRUNC | O_WRONLY, 0644);
                if (fd < 0) { print_error(); exit(1); }
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
            execv(full_path, argv_tk);
            print_error();
            exit(1);
        } else if (pid > 0) {
            if (!is_background) pids[pid_count++] = pid; 
        } else {
            print_error();
        }

        free(full_path);
        for (int j = 0; j < argc_tk; j++) free(argv_tk[j]);
        free(argv_tk);
    }

    // Wait for foreground children only
    for (int i = 0; i < pid_count; i++) waitpid(pids[i], NULL, 0);

    // Cleanup
    for (int i = 0; i < parallel_count; i++) free(parallel_cmds[i]);
}

int main(int MainArgc, char *MainArgv[]) {
    char *cmd = NULL;
    size_t len = 0;
    ssize_t nread;

    init_path();

    if (MainArgc == 1) {
        // Interactive mode
        while (1) {
            printf("witsshell> ");
            fflush(stdout);

            nread = getline(&cmd, &len, stdin);
            if (nread == -1) break;

            cmd[strcspn(cmd,  "\r\n")] = 0; // strip newline
            if (strlen(cmd) > 0) {
                process_command(cmd);
            }
        }
    } else if (MainArgc == 2) {
        // Batch mode
        FILE *batch_file = fopen(MainArgv[1], "r");
        if (!batch_file) {
            print_error();
            return 1;
        }
        while ((nread = getline(&cmd, &len, batch_file)) != -1) {
            cmd[strcspn(cmd, "\r\n")] = 0; // strip newline
            if (strlen(cmd) > 0) {
                process_command(cmd);
            }
        }
        fclose(batch_file);
    } else {
        print_error();
        return 1;
    }

    free(cmd);
    return 0;
}
