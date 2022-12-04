#define _GNU_SOURCE
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>
#include <errno.h>
#include <stdbool.h>


void set_sig_int_ignorance(){
    struct sigaction ignore_sig_int;
    memset(&ignore_sig_int,0,sizeof(ignore_sig_int));
    ignore_sig_int.sa_handler = SIG_IGN;
    ignore_sig_int.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &ignore_sig_int, 0) < 0){
        fprintf(stderr, "Couldn't set signal for SIG_IGN. Error was: %s\n", strerror(errno));
        exit(1);
    }
}

static void child_handler(int sig)
{
    //    Eran's Trick
    pid_t pid;
    int status;

    /* EEEEXTEERMINAAATE! */
    while((pid = waitpid(-1, &status, WNOHANG)) > 0);
}

void set_sig_chld_behavior(){
//    Eran's Trick
    struct sigaction exterminate_children;
    memset(&exterminate_children,0,sizeof(exterminate_children));
    exterminate_children.sa_handler = child_handler;
    exterminate_children.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if(sigaction(SIGCHLD, &exterminate_children, NULL) < 0){
        fprintf(stderr, "Couldn't set signal for SIGCHLD. Error was: %s\n", strerror(errno));
        exit(1);
    }
}

// prepare and finalize calls for initialization and destruction of anything required
int prepare(void){
    set_sig_int_ignorance();
    set_sig_chld_behavior();
}

bool is_containing_ampersand(int count, char** arglist){
    return strcmp(arglist[count - 1], "&") == 0;
}

bool is_containing_pipe(int count, char** arglist){
    for (int i = 0; i < count; ++i) {
        if(strcmp(arglist[i], "|") == 0){
            return true;
        }
    }
    return false;
}

bool is_containing_gt(int count, char** arglist){
    if(count < 2)
        return false;
    else
        return strcmp(arglist[count - 2], ">") == 0;
}

void process_background_operation(int count, char** arglist) {

}

void process_pipe_operation(int count, char** arglist) {

}

void process_gt_operation(int count, char** arglist) {

}

void process_normal_operation(int count, char** arglist) {

}

// arglist - a list of char* arguments (words) provided by the user
// it contains count+1 items, where the last item (arglist[count]) and *only* the last is NULL
// RETURNS - 1 if should continue, 0 otherwise
int process_arglist(int count, char** arglist){
    if(is_containing_ampersand(count, arglist))
        process_background_operation(count - 1, arglist);
    else if(is_containing_pipe(count, arglist))
        process_pipe_operation(count, arglist);
    else if(is_containing_gt(count, arglist))
        process_gt_operation(count, arglist);
    else
        process_normal_operation(count, arglist);
}

int finalize(void){
    return 0;
}
