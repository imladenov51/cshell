#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <sysexits.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include "command.h"
#include "execute.h"

static int execute_aux(struct tree *t, int p_input_fd, int p_output_fd);

int execute(struct tree *t) {
    return execute_aux(t, STDIN_FILENO, STDOUT_FILENO);
}

static int execute_aux(struct tree *t, int p_input_fd, int p_output_fd) {
    int status, status2;
    pid_t pid;
    int in = p_input_fd, out = p_output_fd;
    if (t->conjunction == NONE) { /* leaf node (command w/o ops) */
        if (strcmp(t->argv[0], "exit") == 0) {
            exit(0);
        } else if (strcmp(t->argv[0], "cd") == 0) {
            if (t->argv[1] == NULL) { /* cd w/o args goes home */
                if (chdir(getenv("HOME")) != 0) {
                    perror(getenv("HOME"));
                }
            } else {
                if (chdir(t->argv[1]) != 0) {
                    perror(t->argv[1]);
                }
            }
        } else {
            pid = fork();
            switch (pid) {
            case -1:
                perror("fork");
                exit(EX_OSERR);
            case 0:
                /* change stdin/stdout */
                if (t->input != NULL) {
                    in = open(t->input, O_RDONLY);
                    if (in < 0) {
                        perror("open");
                        exit(EX_OSERR);
                    }
                }
                if (in != STDIN_FILENO) {
                    if (dup2(in, STDIN_FILENO) < 0) {
                        perror("dup2");
                        exit(EX_OSERR);
                    }
                    if (close(in) < 0) {
                        perror("close");
                        exit(EX_OSERR);
                    }
                }
                if (t->output != NULL) {
                    out = open(t->output, (O_WRONLY | O_TRUNC | O_CREAT), 0664);
                    if (out < 0) {
                        perror("open");
                        exit(EX_OSERR);
                    }
                }
                if (out != STDOUT_FILENO) {
                    if (dup2(out, STDOUT_FILENO) < 0) {
                        perror("dup2");
                        exit(EX_OSERR);
                    }
                    if (close(out) < 0) {
                        perror("close");
                        exit(EX_OSERR);
                    }
                }

                /* execute command with new i/o */
                execvp(t->argv[0], t->argv);
                fprintf(stderr, "Failed to execute %s\n", t->argv[0]);
                fflush(stdout);
                exit(EX_OSERR);
            default:
                wait(&status);
                return WEXITSTATUS(status);
            }
        }
    } else if (t->conjunction == AND) {
        status = execute_aux(t->left, p_input_fd, p_output_fd); /* LHS && */
        if (status == 0) {
            return execute_aux(t->right, p_input_fd, p_output_fd); /* && RHS */
        } else {
            return status;
        }
    } else if (t->conjunction == OR) {
        status = execute_aux(t->left, p_input_fd, p_output_fd);
        status2 = execute_aux(t->right, p_input_fd, p_output_fd);
        return status || status2; 
    } else if (t->conjunction == SEMI) {
        execute_aux(t->left, p_input_fd, p_output_fd);
        return execute_aux(t->right, p_input_fd, p_output_fd);
    } else if (t->conjunction == PIPE) {
        int pipe_fd[2];

        /* check for double input/output */
        if (t->left->output != NULL) { /* > | - fail */
            printf("Ambiguous output redirect.\n");
            return -1;
        } else if (t->right->input != NULL) { /* | < - fail */
            printf("Ambiguous input redirect.\n");
            return -1;
        }

        if (pipe(pipe_fd) < 0) { /* pipe error */
            perror("pipe");
            exit(EX_OSERR);
        }
        pid = fork();
        switch (pid) {
        case -1:
            perror("fork");
            exit(EX_OSERR);
        case 0:
            if (close(pipe_fd[0]) < 0) {
                perror("close");
                exit(EX_OSERR);
            }
            /* LHS | */
            status = execute_aux(t->left, in, pipe_fd[1]);
            if (close(pipe_fd[1]) < 0) {
                perror("close");
                exit(EX_OSERR);
            }
            exit(status);
        default:
            if (close(pipe_fd[1]) < 0) {
                perror("close");
                exit(EX_OSERR);
            }
            /* | RHS */
            execute_aux(t->right, pipe_fd[0], out);
            if (close(pipe_fd[0]) < 0) {
                perror("close");
                exit(EX_OSERR);
            }
            wait(&status);
            return WEXITSTATUS(status);
        }
    } else if (t->conjunction == SUBSHELL) {
        pid = fork();
        switch (pid) {
        case -1:
            perror("fork");
            exit(EX_OSERR);
        case 0:
            /* change stdin/stdout */
            if (t->input != NULL) {
                in = open(t->input, O_RDONLY);
                if (in < 0) {
                    perror("open");
                    exit(EX_OSERR);
                }
            }
            if (t->output != NULL) {
                out = open(t->output, (O_WRONLY | O_TRUNC | O_CREAT), 0664);
                if (out < 0) {
                    perror("open");
                    exit(EX_OSERR);
                }
            }

            /* execute () */
            status = execute_aux(t->left, in, out);
            exit(status);
        default:
            wait(&status);
            return WEXITSTATUS(status);
        }
    } else {
        return 1;
    }

    return 0;
}
