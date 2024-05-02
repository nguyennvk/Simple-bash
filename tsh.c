/* 
 * tsh - A tiny shell program with job control
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
 
/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
 
/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */
 
/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */
 
/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
char sbuf[MAXLINE];         /* for composing sprintf messages */
 
struct job_t {              /* Per-job data */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, FG, BG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
 
volatile sig_atomic_t ready; /* Is the newest child in its own process group? */
 
/* End global variables */
 
 
/* Function prototypes */
 
/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);
void sigchld_handler(int sig);
void sigint_handler(int sig);
void sigtstp_handler(int sig);
 
/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);
void sigusr1_handler(int sig);
 
void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int freejid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);
 
void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

char **get_subarray(char **argv, int start, int stop);
int *split_indices(char **argv, int *count);
/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) {
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */
 
    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(STDOUT_FILENO, STDERR_FILENO);
 
    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != -1) {
        switch (c) {
            case 'h':             /* print help message */
                usage();
                break;
            case 'v':             /* emit additional diagnostic info */
                verbose = 1;
                break;
            case 'p':             /* don't print a prompt */
                emit_prompt = 0;  /* handy for automatic testing */
                break;
            default:
                usage();
        }
    }
 
    /* Install the signal handlers */
 
    Signal(SIGUSR1, sigusr1_handler); /* Child is ready */
 
    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */
 
    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 
 
    /* Initialize the job list */
    initjobs(jobs);
 
    /* Execute the shell's read/eval loop */
    while (1) {
 
        /* Read command line */
        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin)) { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }
 
        /* Evaluate the command line */
    eval(cmdline);
        fflush(stdout);
    } 
 
    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) {    
    char *argv[MAXLINE];
    char cmdline_copy[MAXLINE];
    strcpy(cmdline_copy, cmdline);
    int n = parseline(cmdline_copy, argv);    
    int bg = 0;
    int pid_list[MAXJOBS];
    if (argv[0]==NULL)
    {
        return;
    }
    else if (freejid(jobs) == 0)
    {
        printf("Too much jobs created\n");
    }
    if (builtin_cmd(argv)==0){
    
    
    if (strcmp(argv[n-1], "&")==0)
    {   
    bg = 1;
    argv[n-1] = NULL;
        
    }
    
    sigset_t set, oldset;
    sigemptyset(&set);
    sigemptyset(&oldset);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTSTP);
    if (sigprocmask(SIG_SETMASK, &set, NULL)==-1)
    {
        perror("sigprocmask");
        exit(1);
    }
 
    // Signal(SIGINT, SIG_IGN);
    // Signal(SIGTSTP, SIG_IGN);
    raise(SIGUSR1);
    if (ready != 1)
    {
        sigsuspend(&set);
    }
    ready = 0;

    int c = 0;
    int *split_factor = split_indices(argv, &c);
    if (c!=0 && split_factor[c-1] == n-1)
    {
        printf("Invalid using of < > |\n");
        return;
    }

    int in_fd = 0; // Default input file descriptor
    int out_fd = 1; // Default output file descriptor

    // Handle input and output redirection
    for (int i = 0; i < c; i++) {
        if (strcmp(argv[split_factor[i]], "<") == 0) {
            in_fd = open(argv[split_factor[i] + 1], O_RDONLY);
            if (in_fd < 0) {
                perror("input redirection failed");
                exit(1);
            }
            argv[split_factor[i]] = NULL; // Remove the "<" symbol from the arguments
        } else if (strcmp(argv[split_factor[i]], ">") == 0) {
            out_fd = open(argv[split_factor[i] + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd < 0) {
                perror("output redirection failed");
                exit(1);
            }
            argv[split_factor[i]] = NULL; // Remove the ">" symbol from the arguments
        }
    }
    //     else if (strcmp(arg[split_factor[i]], "|")==0){
    //         pipe(fd_array[fd_array_size]);
    //         fd_array_size++;
    //     }
    // }

 
    int pid = fork();
 
    
    if (pid == 0)
    {
        // Signal(SIGINT, sigint_handler);
        // Signal(SIGTSTP, sigtstp_handler);
        if (sigprocmask(SIG_SETMASK, &oldset, NULL)==-1)
        {
            perror("sigprocmask");
            exit(-1);
        }
        setpgid(0, 0);

        if (dup2(in_fd, STDIN_FILENO) == -1) {
                perror("input redirection failed");
                exit(1);
            }

        if (dup2(out_fd, STDOUT_FILENO) == -1) {
            perror("output redirection failed");
            exit(1);
        }

        if(c==0)
        {        
        
        execve(argv[0], argv, NULL);    
        if (errno == ENOENT)
        {
            printf("%s: Command not found\n",argv[0]);
            exit(1);
        }
        }
        else
        {
            for(int i=0;i<c;i++){
            if (strcmp(argv[split_factor[i]], "|")==0){
            
            int fd[2*c];
            for (int x=0;x<c;x++){
                if((pipe(&fd[2*x])==-1))
                {
                    perror("pipe");
                    exit(1);
                }
            }
            for (int i = 0; i<c;i++){
                int r;
                if((r=fork())>0){
                    if ((dup2(fd[2*i+1], fileno(stdout))) == -1) {
                        perror("dup2");
                        exit(1);
                    }

                    // Parent won't be reading from pipe
                    if ((close(fd[2*i])) == -1) {
                        perror("close");
                    }

                    // Because writes go to stdout, noone should write to fd[1]
                    if ((close(fd[2*i+1])) == -1) {
                        perror("close");
                    }
                    if(i==0){
                        execvp(argv[0], get_subarray(argv, 0, split_factor[0]-1));
                    }
                    else if (i<c-1){
                        char **subarray = get_subarray(argv, split_factor[i]+1, split_factor[i+1]-1);
                        execvp(argv[split_factor[i]+1], subarray);
                    }
                }
                else{
                    if ((dup2(fd[2*i], fileno(stdin))) == -1) {
                        perror("dup2");
                        exit(1);
                    }

                    // This process will never write to the pipe.
                    if ((close(fd[2*i+1])) == -1) {
                        perror("close");
                    }

                    // Since we reset stdin to read from the pipe, we don't need fd[0]
                    if ((close(fd[2*i])) == -1) {
                        perror("close");
                    }
                    
                }
                if (i==c-1){
                        pid_list[i+1] = getpid();
                        execvp(argv[split_factor[c-1]+1], get_subarray(argv, split_factor[c-1]+1, n-1));
                    }
            }
            for(int i=0; i<=c; i++)
            {
                printf("%d\n", pid_list[i]);
                waitpid(pid_list[i], NULL, 0);
            }
            }
            }
            
        }
        

        
    }
    


    if (sigprocmask(SIG_SETMASK, &oldset, NULL)==-1)
        {
            perror("sigprocmask");
            exit(-1);
        }
 
    if (!bg){
        addjob(jobs, pid, FG, cmdline);
        waitfg(pid);
    }
 
    else{
        addjob(jobs, pid, BG, cmdline);
        struct job_t *job = getjobpid(jobs, pid);
        printf("[%d] (%d) %s", job->jid,  job->pid, job->cmdline);
    }
    
    }
 
}

char **get_subarray(char **argv, int start, int stop) {
    int size = stop - start + 1;
    if (size < 0) {
        fprintf(stderr, "Error: Invalid start and stop indices\n");
        return NULL;
    }

    char **subarray = (char **)malloc((size + 1) * sizeof(char *));
    if (subarray == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    int i, j = 0;
    for (i = start; i <= stop; i++) {
        subarray[j++] = argv[i];
    }
    subarray[j] = NULL; // Null-terminate the array

    return subarray;
}

int *split_indices(char **argv, int *count) {
    int *indices = NULL;
    int index = 0;
    int size = 0;

    // Iterate over the argv array
    while (argv[index] != NULL) {
        // Check if the current argument is "<", ">", or "|"
        if (strcmp(argv[index], "<") == 0 || strcmp(argv[index], ">") == 0 || strcmp(argv[index], "|") == 0) {
            // Reallocate memory for the indices array
            indices = realloc(indices, (size + 1) * sizeof(int));
            if (indices == NULL) {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
            // Store the index of the special character
            indices[size++] = index;
        }
        index++;
    }

    // Update the count variable with the number of indices found
    *count = size;

    return indices;
}



 
/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return number of arguments parsed.
 */
int parseline(const char *cmdline, char **argv) {
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to space or quote delimiters */
    int argc;                   /* number of args */
 
    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
        buf++;
 
    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
        buf++;
        delim = strchr(buf, '\'');
    }
    else {
        delim = strchr(buf, ' ');
    }
 
    while (delim) {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* ignore spaces */
            buf++;
 
        if (*buf == '\'') {
            buf++;
            delim = strchr(buf, '\'');
        }
        else {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;
    
    return argc;
}
 
 
 
/* builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) {
    if (strcmp(argv[0], "quit") == 0) {
        exit(0);
    } else if (strcmp(argv[0], "jobs") == 0) {
        listjobs(jobs);
        return 1;
    } else if (strcmp(argv[0], "bg") == 0 || strcmp(argv[0], "fg") == 0) {
        do_bgfg(argv);
        return 1;
    }
    return 0;     /* not a builtin command */
}
 
/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) {
    if (argv[1] == NULL)
    {
        printf("%s command requires PID or %%jig argument\n", argv[0]);
        return;
    }
    char *id = argv[1];
    struct job_t *job;
    if (id[0] == '%'){
        int jid = strtol(id+1, NULL, 10);
        if (jid == 0)
        {
            printf("%s: argument must be a PID or %%jid\n", argv[0]);
            return;
        }
        job = getjobjid(jobs, jid);
        if(job == NULL){
            printf("%s: No such job\n", id);
            return;
        }
    }
    else if (strtol(id, NULL, 10)==0)
    {
        printf("%s: argument must be a PID or %%jid\n", argv[0]);
        return;
    }
    else if (strtol(id, NULL, 10)>0)
    {
        pid_t pid = strtol(id, NULL, 10);
        job = getjobpid(jobs, pid);
        if (job == NULL)
        {
            printf("(%s): No such process\n", id);
            return;
        }
    }
    else{
    printf("No such job\n");
    return;}
    if (strcmp(argv[0], "bg")==0 && job->state == ST)
    {
        kill(job->pid, SIGCONT);
        job->state = BG;
        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
        return;
    }
    else if (strcmp(argv[0], "fg")==0 && job->state != FG){
        kill(-job->pid, SIGCONT);
        job->state = FG;
        waitfg(job->pid);
        return;
        }
    return;
    
 
}
 
/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid) {
    int status;
    struct job_t *job;
 
    if (pid != 0) {
        // Wait for foreground job to terminate
        while (waitpid(pid, &status, WUNTRACED) > 0) {
            if (WIFSTOPPED(status)) {
                // Job stopped by signal
                job = getjobpid(jobs, pid);
                job->state = ST;
                return;
            }
            deletejob(jobs, pid); // Delete the job after reaping
        }
    }


}
 
 
/*****************
 * Signal handlers
 *****************/
 
/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) {
    pid_t pid;
    int status;
    
    while ((pid = waitpid(-1, &status, WUNTRACED | WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            // Child process terminated normally
            deletejob(jobs, pid);
        } else if (WIFSIGNALED(status)) {
            // Child process terminated by a signal
            printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), pid, WTERMSIG(status));
            deletejob(jobs, pid);
        } else if (WIFSTOPPED(status)) {
            // Child process stopped by a signal
            printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid, WSTOPSIG(status));
            struct job_t *job = getjobpid(jobs, pid);
            job->state = ST;
        }
    }

}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenever the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) {
    pid_t pid = fgpid(jobs);
    if (pid != 0){
        struct job_t *job = getjobpid(jobs, pid);
        printf("Job [%d] (%d) terminated by signal 2\n", job->jid, job->pid);
        kill(-pid, SIGINT);
        deletejob(jobs, pid);
    }
}
 
/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) {
    pid_t pid = fgpid(jobs);
    printf("STOPPPP");
    if (pid != 0) {
        printf("Job [%d] (%d) stopped by signal 20\n", getjobpid(jobs, pid)->jid, pid);
        kill(-pid, SIGTSTP);
        struct job_t *job = getjobpid(jobs, pid);
        job->state = ST;
        return;
    }
}
 
/*
 * sigusr1_handler - child is ready
 */
void sigusr1_handler(int sig) {
    ready = 1;
}
 
 
/*********************
 * End signal handlers
 *********************/
 
/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/
 
/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}
 
/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;
 
    for (i = 0; i < MAXJOBS; i++)
        clearjob(&jobs[i]);
}
 
/* freejid - Returns smallest free job ID */
int freejid(struct job_t *jobs) {
    int i;
    int taken[MAXJOBS + 1] = {0};
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid != 0) 
        taken[jobs[i].jid] = 1;
    for (i = 1; i <= MAXJOBS; i++)
        if (!taken[i])
            return i;
    return 0;
}
 
/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) {
    int i;
    
    if (pid < 1)
        return 0;
    int free = freejid(jobs);
    if (!free) {
        printf("Tried to create too many jobs\n");
        return 0;
    }
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == 0) {
            jobs[i].pid = pid;
            jobs[i].state = state;
            jobs[i].jid = free;
            strcpy(jobs[i].cmdline, cmdline);
            if(verbose){
                printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
        }
    }
    return 0; /*suppress compiler warning*/
}
 
/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) {
    int i;
 
    if (pid < 1)
        return 0;
 
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            clearjob(&jobs[i]);
            return 1;
        }
    }
    return 0;
}
 
/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;
 
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].state == FG)
            return jobs[i].pid;
    return 0;
}
 
/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;
 
    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
            return &jobs[i];
    return NULL;
}
 
/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;
 
    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid == jid)
            return &jobs[i];
    return NULL;
}
 
/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) {
    int i;
 
    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid) {
            return jobs[i].jid;
    }
    return 0;
}
 
/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) {
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid != 0) {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
            switch (jobs[i].state) {
                case BG: 
                    printf("Running ");
                    break;
                case FG: 
                    printf("Foreground ");
                    break;
                case ST: 
                    printf("Stopped ");
                    break;
                default:
                    printf("listjobs: Internal error: job[%d].state=%d ", 
                       i, jobs[i].state);
            }
            printf("%s", jobs[i].cmdline);
        }
    }
}
/******************************
 * end job list helper routines
 ******************************/
 
 
/***********************
 * Other helper routines
 ***********************/
 
/*
 * usage - print a help message and terminate
 */
void usage(void) {
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}
 
/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg) {
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}
 
/*
 * app_error - application-style error routine
 */
void app_error(char *msg) {
    fprintf(stdout, "%s\n", msg);
    exit(1);
}
 
/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) {
    struct sigaction action, old_action;
 
    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */
 
    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}
 
/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) {
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}