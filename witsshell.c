#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

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

int main(int MainArgc, char *MainArgv[]) {
    char *cmd = NULL;
    size_t len = 0;
    ssize_t nread;

    init_path();


    if(MainArgc == 1) {

        // Interactive mode
        while (1) {
            printf("witsshell> ");
            fflush(stdout);

            nread = getline(&cmd, &len, stdin);
            if (nread == -1) break;

            cmd[strcspn(cmd, "\n")] = 0; // strip newline

            // Split by '&' for parallel commands
            char *parallel_cmds[100];
            int parallel_count = 0;
            char *pc_token = strtok(cmd, "&");
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

                if (argc_tk == 0) { free(argv_tk); continue; }

                // Determine if foreground (last command without trailing '&' is foreground)
                int is_background = 1;
                if (i == parallel_count - 1 && cmd[strlen(cmd)-1] != '&') {
                is_background = 0;
                }

                // Built-ins
                if (strcmp(argv_tk[0], "exit") == 0) {
                for (int j = 0; j < argc_tk; j++) free(argv_tk[j]);
                free(argv_tk);
                if (!is_background) exit(0);
                continue; // ignore exit in background
                }

                if (strcmp(argv_tk[0], "cd") == 0) {
                    if (argc_tk < 2) fprintf(stderr, "cd: expected argument\n");
                    else if (argc_tk > 2) fprintf(stderr, "cd: too many arguments\n");
                    else if (chdir(argv_tk[1]) != 0) perror("cd");
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
                char *full_path = NULL;
                for (int j = 0; PATHS && PATHS[j]; j++) {
                    char candidate[1024];
                    snprintf(candidate, sizeof(candidate), "%s%s", PATHS[j], argv_tk[0]);
                    if (access(candidate, X_OK) == 0) {
                        full_path = strdup(candidate);
                        break;
                    }
                }
                if (!full_path) {
                    fprintf(stderr, "Command not found: %s\n", argv_tk[0]);
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
                            if (argv_tk[j+1] == NULL) { fprintf(stderr, "Redirection misformatted\n"); exit(1); }
                            output = argv_tk[j+1];
                            argv_tk[j] = NULL;
                            break;
                        }
                    }
                    if (output) {
                        int fd = open(output, O_CREAT | O_TRUNC | O_WRONLY, 0644);
                        if (fd < 0) { perror("open"); exit(1); }
                        dup2(fd, STDOUT_FILENO);
                        dup2(fd, STDERR_FILENO);
                        close(fd);
                    }
                    execv(full_path, argv_tk);
                    perror("execv failed");
                    exit(1);
                } else if (pid > 0) {
                    if (!is_background) pids[pid_count++] = pid; // only wait for foreground
                } else perror("fork failed");

                free(full_path);
                for (int j = 0; j < argc_tk; j++) free(argv_tk[j]);
                free(argv_tk);
            }

            // Wait for foreground children only
            for (int i = 0; i < pid_count; i++) waitpid(pids[i], NULL, 0);

            // Cleanup
            for (int i = 0; i < parallel_count; i++) free(parallel_cmds[i]);
        }
    }else {
        fprintf(stderr, "Batch mode not implemented yet.\n");
        return 1;
    }

    free(cmd);
    return 0;
}
