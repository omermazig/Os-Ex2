#define _GNU_SOURCE

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>


void set_sig_int_ignorance() {
    struct sigaction ignore_sig_int;
    memset(&ignore_sig_int, 0, sizeof(ignore_sig_int));
    ignore_sig_int.sa_handler = SIG_IGN;
    ignore_sig_int.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &ignore_sig_int, 0) < 0) {
        fprintf(stderr, "Couldn't set signal for SIG_IGN. Error was: %s\n", strerror(errno));
        exit(1);
    }
}

void set_sig_int_to_default() {
    struct sigaction default_sig_int;
    memset(&default_sig_int, 0, sizeof(default_sig_int));
    default_sig_int.sa_handler = SIG_DFL;
    default_sig_int.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &default_sig_int, 0) < 0) {
        fprintf(stderr, "Couldn't set signal for SIG_IGN. Error was: %s\n", strerror(errno));
        exit(1);
    }
}

static void child_handler(int sig) {
    //    Eran's Trick
    pid_t pid;
    int status;

    /* EEEEXTEERMINAAATE! */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0);
}

void set_sig_chld_behavior() {
//    Eran's Trick
    struct sigaction exterminate_children;
    memset(&exterminate_children, 0, sizeof(exterminate_children));
    exterminate_children.sa_handler = child_handler;
    exterminate_children.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &exterminate_children, NULL) < 0) {
        fprintf(stderr, "Couldn't set signal for SIGCHLD. Error was: %s\n", strerror(errno));
        exit(1);
    }
}

// prepare and finalize calls for initialization and destruction of anything required
int prepare(void) {
    set_sig_int_ignorance();
    set_sig_chld_behavior();
}

bool is_containing_ampersand(int count, char **arglist) {
    return strcmp(arglist[count - 1], "&") == 0;
}

bool is_containing_pipe(int count, char **arglist) {
    for (int i = 0; i < count; ++i) {
        if (strcmp(arglist[i], "|") == 0) {
            return true;
        }
    }
    return false;
}

bool is_containing_gt(int count, char **arglist) {
    if (count < 2)
        return false;
    else
        return strcmp(arglist[count - 2], ">") == 0;
}

int fork_with_error_handling() {
    int pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Fork has failed. Error was: %s\n", strerror(errno));
        exit(1);
    }
    return pid;
}

void do_normal_operation(char **arglist) {
    execvp(arglist[0], arglist);
//        After this we should exit automatically, due to the sigaction we did for SIGCHLD. If didn't, we have a problem
    fprintf(stderr, "%s Command didn't work. Error was: %s\n", arglist[0], strerror(errno));
    exit(1);
}

void process_background_operation(char **arglist) {
    int pid = fork_with_error_handling();

    if (pid == 0) {
//        Child process
        do_normal_operation(arglist);
    }
//    Parent doesn't do anything, because there's no need to wait
}

void process_pipe_operation(char **arglist) {
    char **firstArgList = arglist;
    //    Increase pointer until we reach | sign
    while (strcmp(*(++arglist), "|") != 0);
    //    Put NULL instead of |, to split the array into 2 arrays.
    *arglist = NULL;
    char **secondArgList = ++arglist;

    int pipe_fd[2];

    if (pipe(pipe_fd) < 0) {
        fprintf(stderr, "Couldn't open pipe. Error was: %s\n", strerror(errno));
        exit(1);
    }

    int pid1 = fork_with_error_handling();

    if (pid1 == 0) {
//        First command process
        set_sig_int_to_default();
        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
            fprintf(stderr, "dup2 didn't work. Error was: %s\n", strerror(errno));
            exit(1);
        }
        close(pipe_fd[1]);
        close(pipe_fd[0]);
        do_normal_operation(firstArgList);
    } else {
//        Parent process - Needs to run the second command
        close(pipe_fd[1]);
        int pid2 = fork_with_error_handling();
        if (pid2 == 0) {
//            Second command process
            set_sig_int_to_default();
            if (dup2(pipe_fd[0], STDIN_FILENO) == -1) {
                fprintf(stderr, "dup2 didn't work. Error was: %s\n", strerror(errno));
                exit(1);
            }
            close(pipe_fd[1]);
            close(pipe_fd[0]);
            do_normal_operation(secondArgList);
        } else {
//            Parent process - Ran both command and now needs to wait for them.
            waitpid(pid1, NULL, WUNTRACED);
            waitpid(pid2, NULL, WUNTRACED);
        }
    }
}

void process_normal_operation(char **arglist) {
    int pid = fork_with_error_handling();

    if (pid == 0) {
//        Child process
        set_sig_int_to_default();
        do_normal_operation(arglist);
    } else {
//        Parent process - Waits for child process to exit
        waitpid(pid, NULL, WUNTRACED);
    }
}

void process_gt_operation(char **arglist) {
    char **originalArgListPointer = arglist;
    //    Increase pointer until we reach | sign
    while (strcmp(*(++arglist), ">") != 0);
    //    Put NULL instead of > and the file name, to transform it into a regular command
    *arglist = NULL;
    char *filename = *(++arglist);
    *arglist = NULL;

    int file_desc = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (file_desc < 0) {
        fprintf(stderr, "Can't open output file. Error was: %s\n", strerror(errno));
        exit(1);
    }

//    Now we'll change the file_desc to get the number of the STDOUT, and that way it will receive to output of the
//    normal command. We'll save the original STDOUT fd in a variable, and revert this dup2 after we're done.
    int original_stdout_fd = dup(STDOUT_FILENO);
    if (dup2(file_desc, STDOUT_FILENO) == -1) {
        fprintf(stderr, "dup2 didn't work. Error was: %s\n", strerror(errno));
        exit(1);
    }
    close(file_desc);
    process_normal_operation(originalArgListPointer);
    if (dup2(original_stdout_fd, STDOUT_FILENO) == -1) {
        fprintf(stderr, "dup2 didn't work. Error was: %s\n", strerror(errno));
        exit(1);
    }
    close(original_stdout_fd);
}


// arglist - a list of char* arguments (words) provided by the user
// it contains count+1 items, where the last item (arglist[count]) and *only* the last is NULL
// RETURNS - 1 if should continue, 0 otherwise
int process_arglist(int count, char **arglist) {
    if (is_containing_ampersand(count, arglist)) {
//        Last character is ampersand, and it is not needed, so we'll change it to NULL so it ends the array
        arglist[count - 1] = NULL;
        process_background_operation(arglist);
    } else if (is_containing_pipe(count, arglist))
        process_pipe_operation(arglist);
    else if (is_containing_gt(count, arglist))
        process_gt_operation(arglist);
    else
        process_normal_operation(arglist);

//    We always want to keep going
    return 1;
}

int finalize(void) {
    return 0;
}
