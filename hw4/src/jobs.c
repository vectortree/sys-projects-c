#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <wait.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

#include "mush.h"
#include "debug.h"

char **get_argv_array(ARG *);
void sigchld_handler(int);

/*
 * This is the "jobs" module for Mush.
 * It maintains a table of jobs in various stages of execution, and it
 * provides functions for manipulating jobs.
 * Each job contains a pipeline, which is used to initialize the processes,
 * pipelines, and redirections that make up the job.
 * Each job has a job ID, which is an integer value that is used to identify
 * that job when calling the various job manipulation functions.
 *
 * At any given time, a job will have one of the following status values:
 * "new", "running", "completed", "aborted", "canceled".
 * A newly created job starts out in with status "new".
 * It changes to status "running" when the processes that make up the pipeline
 * for that job have been created.
 * A running job becomes "completed" at such time as all the processes in its
 * pipeline have terminated successfully.
 * A running job becomes "aborted" if the last process in its pipeline terminates
 * with a signal that is not the result of the pipeline having been canceled.
 * A running job becomes "canceled" if the jobs_cancel() function was called
 * to cancel it and in addition the last process in the pipeline subsequently
 * terminated with signal SIGKILL.
 *
 * In general, there will be other state information stored for each job,
 * as required by the implementation of the various functions in this module.
 */
typedef enum {
    NEW,
    RUNNING,
    COMPLETED,
    ABORTED,
    CANCELED
} STATUS_VAL;

static const char *status[] = {"new", "running", "completed", "aborted", "canceled"};

typedef struct job {
    pid_t pgid;
    STATUS_VAL status;
    PIPELINE *pipeline;
} JOB;

static struct {
    JOB jobs[MAX_JOBS];
} JOBS_MODULE;

static int length;

static volatile sig_atomic_t leader_exit_status;

char **get_argv_array(ARG *args_list) {
    ARG *node = args_list;
    int length = 0;
    while(node != NULL) {
        node = node->next;
        ++length;
    }
    // args_array should be NULL-terminated
    char **args_array = calloc(length + 1, sizeof(char *));
    node = args_list;
    int i = 0;
    while(node != NULL) {
        args_array[i++] = eval_to_string(node->expr);
        node = node->next;
    }
    args_array[i] = NULL;
    return args_array;
}

void sigchld_handler(int signal) {
    // Installer: main mush process
    // Triggered when a job (i.e., leader process) is terminated
    int olderrno = errno;
    pid_t wpid;
    int status;
    if((wpid = waitpid(-1, &status, WNOHANG)) < 0) {
        errno = olderrno;
        return;
    }
    for(int i = 0; i < length; ++i) {
        if(JOBS_MODULE.jobs[i].pgid == wpid) {
            if(WIFEXITED(status)) {
                leader_exit_status = WEXITSTATUS(status);
                JOBS_MODULE.jobs[i].status = COMPLETED;
            }
            // Set job status to aborted or canceled accordingly
            else if(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL) {
                leader_exit_status = SIGKILL + 128;
                JOBS_MODULE.jobs[i].status = CANCELED;
            }
            else if(WIFSIGNALED(status)) {
                leader_exit_status = SIGABRT + 128;
                JOBS_MODULE.jobs[i].status = ABORTED;
            }
            break;
        }
    }
    errno = olderrno;
}

/**
 * @brief  Initialize the jobs module.
 * @details  This function is used to initialize the jobs module.
 * It must be called exactly once, before any other functions of this
 * module are called.
 *
 * @return 0 if initialization is successful, otherwise -1.
 */
int jobs_init(void) {
    // TO BE IMPLEMENTED
    length = 0;
    return 0;
}

/**
 * @brief  Finalize the jobs module.
 * @details  This function is used to finalize the jobs module.
 * It must be called exactly once when job processing is to be terminated,
 * before the program exits.  It should cancel all jobs that have not
 * yet terminated, wait for jobs that have been cancelled to terminate,
 * and then expunge all jobs before returning.
 *
 * @return 0 if finalization is completely successful, otherwise -1.
 */
int jobs_fini(void) {
    // TO BE IMPLEMENTED
    for(int i = 0; i < length; ++i) {
        char terminated = (JOBS_MODULE.jobs[i].status == COMPLETED) || (JOBS_MODULE.jobs[i].status == ABORTED) || (JOBS_MODULE.jobs[i].status == CANCELED);
        if(!terminated) {
            if(jobs_cancel(i) < 0) return -1;
        }
        if(JOBS_MODULE.jobs[i].status == CANCELED) {
            if(jobs_wait(i) < 0) return -1;
        }
        if(jobs_expunge(i) < 0) return -1;
    }
    return 0;
}

/**
 * @brief  Print the current jobs table.
 * @details  This function is used to print the current contents of the jobs
 * table to a specified output stream.  The output should consist of one line
 * per existing job.  Each line should have the following format:
 *
 *    <jobid>\t<pgid>\t<status>\t<pipeline>
 *
 * where <jobid> is the numeric job ID of the job, <status> is one of the
 * following strings: "new", "running", "completed", "aborted", or "canceled",
 * and <pipeline> is the job's pipeline, as printed by function show_pipeline()
 * in the syntax module.  The \t stand for TAB characters.
 *
 * @param file  The output stream to which the job table is to be printed.
 * @return 0  If the jobs table was successfully printed, -1 otherwise.
 */
int jobs_show(FILE *file) {
    // TO BE IMPLEMENTED
    if(file == NULL) return -1;
    for(int jobid = 0; jobid < length; ++jobid) {
        fprintf(file, "%d\t%d\t%10s\t", jobid, JOBS_MODULE.jobs[jobid].pgid, status[JOBS_MODULE.jobs[jobid].status]);
        show_pipeline(file, JOBS_MODULE.jobs[jobid].pipeline);
        fprintf(file, "\n");
    }
    return 0;
}

/**
 * @brief  Create a new job to run a pipeline.
 * @details  This function creates a new job and starts it running a specified
 * pipeline.  The pipeline will consist of a "leader" process, which is the direct
 * child of the process that calls this function, plus one child of the leader
 * process to run each command in the pipeline.  All processes in the pipeline
 * should have a process group ID that is equal to the process ID of the leader.
 * The leader process should wait for all of its children to terminate before
 * terminating itself.  The leader should return the exit status of the process
 * running the last command in the pipeline as its own exit status, if that
 * process terminated normally.  If the last process terminated with a signal,
 * then the leader should terminate via SIGABRT.
 *
 * If the "capture_output" flag is set for the pipeline, then the standard output
 * of the last process in the pipeline should be redirected to be the same as
 * the standard output of the pipeline leader, and this output should go via a
 * pipe to the main Mush process, where it should be read and saved in the data
 * store as the value of a variable, as described in the assignment handout.
 * If "capture_output" is not set for the pipeline, but "output_file" is non-NULL,
 * then the standard output of the last process in the pipeline should be redirected
 * to the specified output file.   If "input_file" is set for the pipeline, then
 * the standard input of the process running the first command in the pipeline should
 * be redirected from the specified input file.
 *
 * @param pline  The pipeline to be run.  The jobs module expects this object
 * to be valid for as long as it requires, and it expects to be able to free this
 * object when it is finished with it.  This means that the caller should not pass
 * a pipeline object that is shared with any other data structure, but rather should
 * make a copy to be passed to this function.
 *
 * @return  -1 if the pipeline could not be initialized properly, otherwise the
 * value returned is the job ID assigned to the pipeline.
 */
int jobs_run(PIPELINE *pline) {
    // TO BE IMPLEMENTED
    if(pline == NULL) return -1;
    if(length == MAX_JOBS) return -1;
    pid_t pid = fork();
    if(pid < 0) {
        perror("fork failed");
        return -1;
    }
    if(pid == 0) {
        // Leader process (child of main mush process)
        // Create all an array for the child process id's (corresponding to
        // the commands of the pipeline).
        COMMAND *cmd_node = pline->commands;
        setpgid(getpid(), getpid());
        int num_of_children = 0;
        while(cmd_node != NULL) {
            ++num_of_children;
            cmd_node = cmd_node->next;
        }
        pid_t *cpid = calloc(num_of_children, sizeof(pid_t));
        int **pipe_fd = calloc(num_of_children - 1, sizeof(int *));
        for(int i = 0; i < num_of_children - 1; ++i) {
            pipe_fd[i] = calloc(2, sizeof(int));
            if(pipe(pipe_fd[i]) < 0) {
                perror("pipe failed");
                exit(EXIT_FAILURE);
            }
        }
        cmd_node = pline->commands;
        // Spawn child processes for each command and execute them
        for(int i = 0; cmd_node != NULL; cmd_node = cmd_node->next, ++i) {
            cpid[i] = fork();
            if(cpid[i] < 0) {
                perror("fork failed");
                exit(EXIT_FAILURE);
            }
            if(cpid[i] == 0) {
                // Child process (corresponding to a command)
                setpgid(getpid(), getppid());
                char **argv = get_argv_array(cmd_node->args);
                if(i > 0 && i < num_of_children - 1) {
                    if(dup2(pipe_fd[i - 1][0], STDIN_FILENO) < 0) {
                        perror("dup2 failed");
                        exit(EXIT_FAILURE);
                    }
                    if(dup2(pipe_fd[i][1], STDOUT_FILENO) < 0) {
                        perror("dup2 failed");
                        exit(EXIT_FAILURE);
                    }
                }
                if(i == 0) {
                    if(num_of_children > 1) {
                        if(dup2(pipe_fd[i][1], STDOUT_FILENO) < 0) {
                            perror("dup2 failed");
                            exit(EXIT_FAILURE);
                        }
                    }
                    // If "input_file" is set for the pipeline, then
                    // the standard input of the process running the
                    // first command in the pipeline should be redirected
                    // from the specified input file.
                    if(pline->input_file != NULL) {
                        int input_fd;
                        if((input_fd = open(pline->input_file, O_RDONLY)) < 0) {
                            perror("open failed");
                            exit(EXIT_FAILURE);
                        }
                        if(dup2(input_fd, STDIN_FILENO) < 0) {
                            perror("dup2 failed");
                            if(close(input_fd) < 0) {
                                perror("close failed");
                                exit(EXIT_FAILURE);
                            }
                            exit(EXIT_FAILURE);
                        }
                        if(close(input_fd) < 0) {
                            perror("close failed");
                            exit(EXIT_FAILURE);
                        }
                    }
                }
                if(i == num_of_children - 1) {
                    if(num_of_children > 1) {
                        if(dup2(pipe_fd[i - 1][0], STDIN_FILENO) < 0) {
                            perror("dup2 failed");
                            exit(EXIT_FAILURE);
                        }
                    }
                    // If the "capture_output" flag is set for the pipeline, then the standard output
                    // of the last process in the pipeline should be redirected to be the same as
                    // the standard output of the pipeline leader, and this output should go via a
                    // pipe to the main Mush process, where it should be read and saved in the data
                    // store as the value of a variable, as described in the assignment handout.
                    if(pline->capture_output) {

                    }
                    // If "capture_output" is not set for the pipeline, but "output_file" is non-NULL,
                    // then the standard output of the last process in the pipeline should be redirected
                    // to the specified output file.
                    else if(pline->output_file != NULL) {
                        int output_fd;
                        if((output_fd = open(pline->output_file, (O_CREAT | O_WRONLY), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) < 0) {
                            perror("open failed");
                            exit(EXIT_FAILURE);
                        }
                        if(dup2(output_fd, STDOUT_FILENO) < 0) {
                            perror("dup2 failed");
                            if(close(output_fd) < 0) {
                                perror("close failed");
                                exit(EXIT_FAILURE);
                            }
                            exit(EXIT_FAILURE);
                        }
                        if(close(output_fd) < 0) {
                            perror("close failed");
                            exit(EXIT_FAILURE);
                        }
                    }
                }
                for(int j = 0; j < num_of_children - 1; ++j) {
                    if(close(pipe_fd[j][0]) < 0) {
                        perror("close failed");
                        exit(EXIT_FAILURE);
                    }
                    if(close(pipe_fd[j][1]) < 0) {
                        perror("close failed");
                        exit(EXIT_FAILURE);
                    }
                }
                execvp(argv[0], argv);
                perror("execvp failed");
                abort();
            }
        }
        for(int j = 0; j < num_of_children - 1; ++j) {
            if(close(pipe_fd[j][0]) < 0) {
                perror("close failed");
                exit(EXIT_FAILURE);
            }
            if(close(pipe_fd[j][1]) < 0) {
                perror("close failed");
                exit(EXIT_FAILURE);
            }
        }
        int status;
        for(int i = 0; i < num_of_children; ++i) {
            if(cpid[i] > 0) {
                status = 0;
                if(waitpid(cpid[i], &status, 0) < 0) {
                    if(errno != ECHILD) {
                        perror("waitpid failed");
                        exit(EXIT_FAILURE);
                    }
                }
                if(i == num_of_children - 1) {
                    if(WIFEXITED(status)) exit(WEXITSTATUS(status));
                }
            }
        }
        // Otherwise, terminate leader via SIGABRT
        abort();
    }
    else {
        // Main mush process
        if(signal(SIGCHLD, sigchld_handler) == SIG_ERR) {
            perror("signal failed");
            return -1;
        }
        JOBS_MODULE.jobs[length].pgid = pid;
        JOBS_MODULE.jobs[length].status = RUNNING;
        JOBS_MODULE.jobs[length].pipeline = copy_pipeline(pline);
        return length++;
    }
}

/**
 * @brief  Wait for a job to terminate.
 * @details  This function is used to wait for the job with a specified job ID
 * to terminate.  A job has terminated when it has entered the COMPLETED, ABORTED,
 * or CANCELED state.
 *
 * @param  jobid  The job ID of the job to wait for.
 * @return  the exit status of the job leader, as returned by waitpid(),
 * or -1 if any error occurs that makes it impossible to wait for the specified job.
 */
int jobs_wait(int jobid) {
    // TO BE IMPLEMENTED
    if(length == 0) return -1;
    if(jobid < 0 || jobid >= length) return -1;
    sigset_t mask;
    sigemptyset(&mask);
    sigsuspend(&mask);
    int child_status;
    if(waitpid(JOBS_MODULE.jobs[jobid].pgid, &child_status, 0) < 0) {
        if(errno == ECHILD) return leader_exit_status;
    }
    return -1;
}

/**
 * @brief  Poll to find out if a job has terminated.
 * @details  This function is used to poll whether the job with the specified ID
 * has terminated.  This is similar to jobs_wait(), except that this function returns
 * immediately without waiting if the job has not yet terminated.
 *
 * @param  jobid  The job ID of the job to wait for.
 * @return  the exit status of the job leader, as returned by waitpid(), if the job
 * has terminated, or -1 if the job has not yet terminated or if any other error occurs.
 */
int jobs_poll(int jobid) {
    if(length == 0) return -1;
    if(jobid < 0 || jobid >= length) return -1;
    int child_status;
    if(waitpid(JOBS_MODULE.jobs[jobid].pgid, &child_status, WNOHANG) < 0) {
        if(errno == ECHILD) return leader_exit_status;
    }
    return -1;
}

/**
 * @brief  Expunge a terminated job from the jobs table.
 * @details  This function is used to expunge (remove) a job that has terminated from
 * the jobs table, so that space in the table can be used to start some new job.
 * In order to be expunged, a job must have terminated; if an attempt is made to expunge
 * a job that has not yet terminated, it is an error.  Any resources (exit status,
 * open pipes, captured output, etc.) that were being used by the job are finalized
 * and/or freed and will no longer be available.
 *
 * @param  jobid  The job ID of the job to expunge.
 * @return  0 if the job was successfully expunged, -1 if the job could not be expunged.
 */
int jobs_expunge(int jobid) {
    // TO BE IMPLEMENTED
    if(length == 0) return -1;
    if(jobid < 0 || jobid >= length) return -1;
    char status = (JOBS_MODULE.jobs[jobid].status == COMPLETED) || (JOBS_MODULE.jobs[jobid].status == ABORTED) || (JOBS_MODULE.jobs[jobid].status == CANCELED);
    if(!status) return -1;
    if(JOBS_MODULE.jobs[jobid].pipeline != NULL)
        free_pipeline(JOBS_MODULE.jobs[jobid].pipeline);
    JOBS_MODULE.jobs[jobid].pipeline = NULL;
    --length;
    return 0;
}

/**
 * @brief  Attempt to cancel a job.
 * @details  This function is used to attempt to cancel a running job.
 * In order to be canceled, the job must not yet have terminated and there
 * must not have been any previous attempt to cancel the job.
 * Cancellation is attempted by sending SIGKILL to the process group associated
 * with the job.  Cancellation becomes successful, and the job actually enters the canceled
 * state, at such subsequent time as the job leader terminates as a result of SIGKILL.
 * If after attempting cancellation, the job leader terminates other than as a result
 * of SIGKILL, then cancellation is not successful and the state of the job is either
 * COMPLETED or ABORTED, depending on how the job leader terminated.
 *
 * @param  jobid  The job ID of the job to cancel.
 * @return  0 if cancellation was successfully initiated, -1 if the job was already
 * terminated, a previous attempt had been made to cancel the job, or any other
 * error occurred.
 */
int jobs_cancel(int jobid) {
    // TO BE IMPLEMENTED
    if(length == 0) return -1;
    if(jobid < 0 || jobid >= length) return -1;
    char status = (JOBS_MODULE.jobs[jobid].status == COMPLETED) || (JOBS_MODULE.jobs[jobid].status == ABORTED) || (JOBS_MODULE.jobs[jobid].status == CANCELED);
    if(status) return -1;
    JOBS_MODULE.jobs[jobid].status = ABORTED;
    if(killpg(JOBS_MODULE.jobs[jobid].pgid, SIGKILL) < 0) {
        perror("kill failed");
        return -1;
    }
    return 0;
}

/**
 * @brief  Get the captured output of a job.
 * @details  This function is used to retrieve output that was captured from a job
 * that has terminated, but that has not yet been expunged.  Output is captured for a job
 * when the "capture_output" flag is set for its pipeline.
 *
 * @param  jobid  The job ID of the job for which captured output is to be retrieved.
 * @return  The captured output, if the job has terminated and there is captured
 * output available, otherwise NULL.
 */
char *jobs_get_output(int jobid) {
    // TO BE IMPLEMENTED
    return NULL;
}

/**
 * @brief  Pause waiting for a signal indicating a potential job status change.
 * @details  When this function is called it blocks until some signal has been
 * received, at which point the function returns.  It is used to wait for a
 * potential job status change without consuming excessive amounts of CPU time.
 *
 * @return -1 if any error occurred, 0 otherwise.
 */
int jobs_pause(void) {
    // TO BE IMPLEMENTED
    sigset_t mask;
    sigemptyset(&mask);
    sigsuspend(&mask);
    return 0;
}
