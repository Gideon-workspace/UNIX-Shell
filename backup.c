#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include "buddy.h"
#define MAX_LINE 1024

char **PATHS = NULL;
int PATH_count = 0;

// Initialize PATHS with default /bin/
void init_path() {
    PATHS = malloc(2 * sizeof(char*));
    PATHS[0] = strdup("/bin/");
    PATHS[1] = NULL;
    PATH_count = 1;
}

void update_path(char **args, int argc) {
    // Free old PATHS
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

int main(int MainArgc, char *MainArgv[]) {
    char *cmd = NULL;
    FILE *file = NULL;
    size_t len = 0;
    ssize_t nread;

    init_path();

    if (MainArgc == 1) {
        // interactive mode
        while (1) {
            printf("witsshell> ");
            fflush(stdout);

            nread = getline(&cmd, &len, stdin);
            if (nread == -1) break;

            cmd[strcspn(cmd, "\n")] = 0; // strip newline

            // split line into parallel commands
            char *parallel_cmds[100];
            int parallel_count = 0;
            char *pc_token = strtok(cmd, "&");
            while (pc_token && parallel_count < 100) {
                parallel_cmds[parallel_count++] = strdup(pc_token);
                pc_token = strtok(NULL, "&");
            }

            pid_t pids[100];
            int pid_count = 0;

            for (int pc = 0; pc < parallel_count; pc++) {
                // tokenize this command
                int argc_tk = 0, capacity = 4;
                char **argv_tk = malloc(capacity * sizeof(char*));

                char *token = strtok(parallel_cmds[pc], " \t\n");
                while (token) {
                    if (argc_tk >= capacity) {
                        capacity *= 2;
                        argv_tk = realloc(argv_tk, capacity * sizeof(char*));
                    }
                    argv_tk[argc_tk++] = strdup(token);
                    token = strtok(NULL, " \t\n");
                }
                argv_tk[argc_tk] = NULL;

                if (argc_tk == 0) {
                    free(argv_tk);
                    continue;
                }

                // builtins
                if (strcmp(argv_tk[0], "exit") == 0) {
                    for (int i = 0; i < argc_tk; i++) free(argv_tk[i]);
                    free(argv_tk);
                    exit(0);
                }

                if (strcmp(argv_tk[0], "cd") == 0) {
                    if (argc_tk < 2) {
                        fprintf(stderr, "cd: expected argument\n");
                    } else if (argc_tk > 2) {
                        fprintf(stderr, "cd: too many arguments\n");
                    } else if (chdir(argv_tk[1]) != 0) {
                        perror("cd");
                    }
                    for (int i = 0; i < argc_tk; i++) free(argv_tk[i]);
                    free(argv_tk);
                    continue;
                }

                if (strcmp(argv_tk[0], "path") == 0) {
                    update_path(argv_tk, argc_tk);
                    for (int i = 0; i < argc_tk; i++) free(argv_tk[i]);
                    free(argv_tk);
                    continue;
                }

                // resolve path
                char *full_path = NULL;
                for (int i = 0; PATHS && PATHS[i]; i++) {
                    char candidate[1024];
                    snprintf(candidate, sizeof(candidate), "%s%s", PATHS[i], argv_tk[0]);
                    if (access(candidate, X_OK) == 0) {
                        full_path = strdup(candidate);
                        break;
                    }
                }

                if (!full_path) {
                    fprintf(stderr, "Command not found: %s\n", argv_tk[0]);
                    for (int i = 0; i < argc_tk; i++) free(argv_tk[i]);
                    free(argv_tk);
                    continue;
                }

                // fork + exec
                pid_t pid = fork();
                if (pid == 0) { // children
                    // redirection check
                    char *output = NULL;
                    for (int i = 0; i < argc_tk; i++) {
                        if (strcmp(argv_tk[i], ">") == 0) {
                            if (argv_tk[i+1] == NULL) {
                                fprintf(stderr, "Redirection misformatted\n");
                                exit(1);
                            }
                            output = argv_tk[i + 1];
                            argv_tk[i] = NULL;
                            break;
                        }
                    }

                    if (output) {
                        int fd = open(output, O_CREAT | O_TRUNC | O_WRONLY, 0644);
                        if (fd < 0) {
                            perror("open"); 
                            exit(1);
                        }
                        dup2(fd, STDOUT_FILENO);
                        dup2(fd, STDERR_FILENO);
                        close(fd);
                    }

                    execv(full_path, argv_tk);
                    perror("execv failed");
                    exit(1);
                } else if (pid > 0) {
                    pids[pid_count++] = pid;
                } else {
                    perror("fork failed");
                }

                free(full_path);
                for (int i = 0; i < argc_tk; i++) {
                    free(argv_tk[i]);
                } 
                free(argv_tk);
            }

            // wait for all children
            for (int i = 0; i < pid_count; i++) {
                waitpid(pids[i], NULL, 0);
            }

            // cleanup
            for (int i = 0; i < parallel_count; i++) {
                free(parallel_cmds[i]);
            }
        }
    } else {
        fprintf(stderr, "Batch mode not implemented yet.\n");
        exit(1);
    }

    free(cmd);
    return 0;
}
