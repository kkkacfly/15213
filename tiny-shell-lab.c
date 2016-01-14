/* 
 * tsh - A tiny shell program with job control
 * Name: Xuan Li
 * ID: xuanli1
 * Date: 03/14/2015
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF         0   /* undefined */
#define FG            1   /* running in foreground */
#define BG            2   /* running in background */
#define ST            3   /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Parsing states */
#define ST_NORMAL   0x0   /* next token is an argument */
#define ST_INFILE   0x1   /* next token is the input file */
#define ST_OUTFILE  0x2   /* next token is the output file */


/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t job_list[MAXJOBS]; /* The job list */

struct cmdline_tokens {
    int argc;               /* Number of arguments */
    char *argv[MAXARGS];    /* The arguments list */
    char *infile;           /* The input file */
    char *outfile;          /* The output file */
    enum builtins_t {       /* Indicates if argv[0] is a builtin command */
        BUILTIN_NONE,
        BUILTIN_QUIT,
        BUILTIN_JOBS,
        BUILTIN_BG,
        BUILTIN_FG} builtins;
};

/* End global variables */


/* Function prototypes */
void eval(char *cmdline);

/* Helper functions */
int buildin_cmds(struct cmdline_tokens token);
void dup_redirect(char *input, char *output);
void listJob(struct cmdline_tokens token);
void bgjob(struct cmdline_tokens token);
void fgjob(struct cmdline_tokens token);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, struct cmdline_tokens *tok); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *job_list);
int maxjid(struct job_t *job_list); 
int addjob(struct job_t *job_list, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *job_list, pid_t pid); 
pid_t fgpid(struct job_t *job_list);
struct job_t *getjobpid(struct job_t *job_list, pid_t pid);
struct job_t *getjobjid(struct job_t *job_list, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *job_list, int output_fd);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);



/*
 * main - The shell's main routine 
 */
int 
main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];    /* cmdline for fgets */
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
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

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */
    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(job_list);


    /* Execute the shell's read/eval loop */
    while (1) {

        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin)) { 
            /* End of file (ctrl-d) */
            printf ("\n");
            fflush(stdout);
            fflush(stderr);
            exit(0);
        }
        
        /* Remove the trailing newline */
        cmdline[strlen(cmdline)-1] = '\0';
        
        /* Evaluate the command line */
        eval(cmdline);
        
        fflush(stdout);
        fflush(stdout);
    } 
    
    exit(0); /* control never reaches here */
}

/* Kill function: sending signal to process for wrapping */
void Kill(pid_t pid, int sig)
{
    if (kill(pid, sig) < 0)
        unix_error("Kill error encountered");
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
void 
eval(char *cmdline) 
{
    int bg;              
    sigset_t mask, mask1;

    struct cmdline_tokens token;	//From parseline

    int pid;			// pid for fork return


    /* Parse command line */
    bg = parseline(cmdline, &token);

    if (bg == -1)
    {
	return;
    }		
    if (token.argv[0] == NULL)
    {
	return;	
    }

    /* If not a built-in command */
    if (!buildin_cmds(token)) 
    {
	/* initialize mask */
	if (sigemptyset(&mask) == -1) 
	{
            printf("System Errors\n");
	    return;
	}

	/* Adds SIGCHLD, SIGINT, SIGTSTP to mask */
	if (!(sigaddset(&mask, SIGCHLD) != -1
	    && sigaddset(&mask, SIGINT) != -1
	    && sigaddset(&mask, SIGTSTP) != -1)) 
	{
	    printf("System Errors\n");
	    return;
	}

	/* Block SIGCHILD, SIGINT, STGTSTP signals for race conditions */
	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) 
	{
	    printf("System Errors\n");
	    return;
	}

	/* CHILD PROCESS */
	if ((pid = fork()) == 0) 
	{
	    /* Unblock SIGCHLD, SIGINT, SIGTSTP signals before execve */
	    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) 
	    {
            	printf("System Errors\n");
		return;
	    }
	    /* redirect the file descriptors */
	    dup_redirect(token.infile, token.outfile);

	    /* Putting child in new process group */
	    if (setpgid(0, 0) == -1) 
	    {
		printf("System Errors\n");
		return;
	    }

	    /* invoke execve in child to load the program */
	    if (execve(token.argv[0], token.argv, environ) < 0) 
	    {
		printf("%s: Command not found. \n", token.argv[0]);
		exit(0);
	    }

	}


	/* PARENT PROCESS */
         if (bg)  //add background job with BG parameter
         {
             if (addjob(job_list, pid, BG, cmdline) == 0)
             {
                 printf("System Errors\n");
                 return;
             }
         }
         else  //add foreground job with FG parameter
         {
             if (addjob(job_list, pid, FG, cmdline) == 0)
             {
                 printf("System Errors\n");
                 return;
             }
         }



	/* Unblock the signals in parent */
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) 
	{
	    printf("System Errors\n");
	    return;
	}


	/* If background */
	if (bg) 
	{
	    sprintf(sbuf, "[%d] ", getjobpid(job_list, pid)->jid);
	    sprintf(sbuf, "%s(%d) ", sbuf, pid);
	    sprintf(sbuf, "%s%s\n", sbuf, cmdline);
	    printf(sbuf);

	} 
	else 		/* If foreground */
	{    
	    while (fgpid(job_list) != 0 &&
		   getjobpid(job_list, pid)->state != ST) 
	    {
		sigemptyset(&mask1); //Empty the mask2 to be used as a parameter in sigsuspend
		sigsuspend(&mask1); //Wait for a signal
	    }

	}
    }

    return;
}


/* 
 * help function: buildin commands judgment
 * return 1 if is built-in; return 0 if not
 */
int buildin_cmds(struct cmdline_tokens token)
{

    switch (token.builtins) {

    case BUILTIN_NONE:
	return 0;
    
    /* shell terminates when "quit" */
    case BUILTIN_QUIT:
	exit(0);
    
    /* lists all background jobs */
    case BUILTIN_JOBS:
	listJob(token);
	return 1;

    case BUILTIN_BG:
	bgjob(token);
	return 1;

    case BUILTIN_FG:
	fgjob(token);
	return 1;

    default:
	return 0;
    }
}

/* 
 * help function: redirect file description 
 */
void dup_redirect(char *input, char *output)
{
    int redirect_input, redirect_output;
    FILE *inFile = NULL;
    FILE *outFile = NULL;

    if (input != NULL)        // If input needs redirection
    {
        if ((redirect_input = open(input, O_RDWR)) < 0) 
        {
            inFile = fopen(input, "r");
            redirect_input = open(input, O_RDWR);
        }

        dup2(redirect_input, STDIN_FILENO);     //Duplicating file descriptor 

        if (inFile != NULL)
        {
            fclose(inFile);
        }
        close(redirect_input);
    }

    if (output != NULL)    // If output needs redirection
    {
	if ((redirect_output = open(output, O_RDWR)) < 0)	 
	{   
	    outFile = fopen(output, "w");
	    redirect_output = open(output, O_RDWR);
	}

	dup2(redirect_output, STDOUT_FILENO);	// Calls dup2: duplicate the file description

	if (outFile != NULL)
	{
	    fclose(outFile);
	}
	close(redirect_output);
    }

    return;

}

void  listJob(struct cmdline_tokens token)
{
        FILE *outFile = NULL; // File pointer to open file when errors
        int redirect_output; // To redirect the output of jobs

        if (token.outfile != NULL)
        {
            if ((redirect_output = open(token.outfile, O_RDWR)) < 0)
            {
                outFile = fopen(token.outfile, "w");
                redirect_output = open(token.outfile, O_RDWR);  // get file          
            }

            listjobs(job_list, redirect_output);        // Listing jobs 

            if (outFile != NULL)
            {
                fclose(outFile);
            }
            close(redirect_output);
        }
        else
        {
            listjobs(job_list, STDOUT_FILENO);  // If needs to be redirected
        }
}

void bgjob( struct cmdline_tokens token)
{
	struct job_t *job;
        if (token.argv[token.argc - 1][0] == '%')       //if jobid specified
        {
            /* Fetch the job pointer using jid */
            job = getjobjid(job_list, atoi(token.argv[token.argc - 1] + 1));

            if (job == NULL)
            {
                printf("Error: job does not exist: %s\n", token.argv[1]);
            }

        }
        else
        {
            /* fetch the job pointer using pid when jid specified */
            job = getjobpid(job_list, atoi(token.argv[token.argc - 1]));

            if (job == NULL)
            {
                printf(" Error: process %s does not exist\n", token.argv[1]);
            }
        }

        printf("[%d] (%d) %s\n", job->jid, job->pid,    // display job to be run in background
               job->cmdline);

        if (job->state != ST && job->state != BG)       // when background job not allowed in current status
        {
            printf("Error: process not stopped nor runs in background.\n");
        }

        job->state = BG;        // change job state
        kill(job->pid, SIGCONT);        // send signal        
}

void fgjob( struct cmdline_tokens token)
{
	sigset_t mask0;
        sigemptyset(&mask0); //empty the signal set for sigsuspend
	struct job_t *job;
        if (token.argv[token.argc - 1][0] == '%')       //If jid is specified
        {
            /* Fetch the job pointer using jid */
            job = getjobjid(job_list, atoi(token.argv[token.argc - 1] + 1));

            if (job == NULL)
            {
                printf("Error: job %s does not exist\n", token.argv[1]);
            }

        }
        else
        {
            /* fetch the job pointer using pid when specified */
            job = getjobpid(job_list, atoi(token.argv[token.argc - 1]));

            if (job == NULL)
            {
                printf("Error: process %s does not exist\n", token.argv[1]);
            }

        }

        /* If current state doesn't allow fg */
        if (job->state != ST && job->state != BG)
        {
            printf("Error: not a background job\n");

        }

        /* Wait until current fg finishes | change state */
        while (fgpid(job_list) != 0 && getjobpid(job_list, job->pid)->state != ST)
        {
            sigsuspend(&mask0);
        }

        job->state = FG;                //Change state
        kill(job->pid, SIGCONT);        //Send signal 
}




/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Parameters:
 *   cmdline:  The command line, in the form:
 *
 *                command [arguments...] [< infile] [> oufile] [&]
 *
 *   tok:      Pointer to a cmdline_tokens structure. The elements of this
 *             structure will be populated with the parsed tokens. Characters 
 *             enclosed in single or double quotes are treated as a single
 *             argument. 
 * Returns:
 *   1:        if the user has requested a BG job
 *   0:        if the user has requested a FG job  
 *  -1:        if cmdline is incorrectly formatted
 * 
 * Note:       The string elements of tok (e.g., argv[], infile, outfile) 
 *             are statically allocated inside parseline() and will be 
 *             overwritten the next time this function is invoked.
 */
int 
parseline(const char *cmdline, struct cmdline_tokens *tok) 
{

    static char array[MAXLINE];          /* holds local copy of command line */
    const char delims[10] = " \t\r\n";   /* argument delimiters (white-space) */
    char *buf = array;                   /* ptr that traverses command line */
    char *next;                          /* ptr to the end of the current arg */
    char *endbuf;                        /* ptr to end of cmdline string */
    int is_bg;                           /* background job? */

    int parsing_state;                   /* indicates if the next token is the
                                            input or output file */

    if (cmdline == NULL) {
        (void) fprintf(stderr, "Error: command line is NULL\n");
        return -1;
    }

    (void) strncpy(buf, cmdline, MAXLINE);
    endbuf = buf + strlen(buf);

    tok->infile = NULL;
    tok->outfile = NULL;

    /* Build the argv list */
    parsing_state = ST_NORMAL;
    tok->argc = 0;

    while (buf < endbuf) {
        /* Skip the white-spaces */
        buf += strspn (buf, delims);
        if (buf >= endbuf) break;

        /* Check for I/O redirection specifiers */
        if (*buf == '<') {
            if (tok->infile) {
                (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
                return -1;
            }
            parsing_state |= ST_INFILE;
            buf++;
            continue;
        }
        if (*buf == '>') {
            if (tok->outfile) {
                (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
                return -1;
            }
            parsing_state |= ST_OUTFILE;
            buf ++;
            continue;
        }

        if (*buf == '\'' || *buf == '\"') {
            /* Detect quoted tokens */
            buf++;
            next = strchr (buf, *(buf-1));
        } else {
            /* Find next delimiter */
            next = buf + strcspn (buf, delims);
        }
        
        if (next == NULL) {
            /* Returned by strchr(); this means that the closing
               quote was not found. */
            (void) fprintf (stderr, "Error: unmatched %c.\n", *(buf-1));
            return -1;
        }

        /* Terminate the token */
        *next = '\0';

        /* Record the token as either the next argument or the i/o file */
        switch (parsing_state) {
        case ST_NORMAL:
            tok->argv[tok->argc++] = buf;
            break;
        case ST_INFILE:
            tok->infile = buf;
            break;
        case ST_OUTFILE:
            tok->outfile = buf;
            break;
        default:
            (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
            return -1;
        }
        parsing_state = ST_NORMAL;

        /* Check if argv is full */
        if (tok->argc >= MAXARGS-1) break;

        buf = next + 1;
    }

    if (parsing_state != ST_NORMAL) {
        (void) fprintf(stderr,
                       "Error: must provide file name for redirection\n");
        return -1;
    }

    /* The argument list must end with a NULL pointer */
    tok->argv[tok->argc] = NULL;

    if (tok->argc == 0)  /* ignore blank line */
        return 1;

    if (!strcmp(tok->argv[0], "quit")) {                 /* quit command */
        tok->builtins = BUILTIN_QUIT;
    } else if (!strcmp(tok->argv[0], "jobs")) {          /* jobs command */
        tok->builtins = BUILTIN_JOBS;
    } else if (!strcmp(tok->argv[0], "bg")) {            /* bg command */
        tok->builtins = BUILTIN_BG;
    } else if (!strcmp(tok->argv[0], "fg")) {            /* fg command */
        tok->builtins = BUILTIN_FG;
    } else {
        tok->builtins = BUILTIN_NONE;
    }

    /* Should the job run in the background? */
    if ((is_bg = (*tok->argv[tok->argc-1] == '&')) != 0)
        tok->argv[--tok->argc] = NULL;

    return is_bg;
}


/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP, SIGTSTP, SIGTTIN or SIGTTOU signal. The 
 *     handler reaps all available zombie children, but doesn't wait 
 *     for any other currently running children to terminate.  
 */
void 
sigchld_handler(int sig) 
{
    pid_t pid;
    int status;

    /* capture child process's PID when status changes */ 
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
	/* normal termination case */ 
	if (WIFEXITED(status))
	{
	    deletejob(job_list, pid);
	}
	/* terminated when assigned with signal */
	else if (WIFSIGNALED(status)) 
	{
	    printf("Job [%d] (%d) terminated by signal %d\n",
		    pid2jid(pid), pid, WTERMSIG(status));
	    deletejob(job_list, pid);
	}
	/* terminated instantly */
	else if (WIFSTOPPED(status))
	{
	    printf("Job [%d] (%d) stopped by signal %d\n",
		    pid2jid(pid), pid, WSTOPSIG(status));
	    /* update status */
            getjobpid(job_list, pid)->state = ST;
	}
    }

    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void 
sigint_handler(int sig) 
{
    // send pid the SIGINT when encountered another process
    pid_t pid = fgpid(job_list);

    if (!pid)
    {
	return;
    }
    Kill(-pid, SIGINT);
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void 
sigtstp_handler(int sig) 
{
    int pid;
    int jobId;
    pid = fgpid(job_list);
    jobId = pid2jid(pid);

    if (pid == 0)
    {
	return;
    }
    if (kill(-pid,sig) < 0) 
    {
	unix_error("can't send signal stop to child process");
    }
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void 
clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void 
initjobs(struct job_t *job_list) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&job_list[i]);
}

/* maxjid - Returns largest allocated job ID */
int 
maxjid(struct job_t *job_list) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].jid > max)
            max = job_list[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int 
addjob(struct job_t *job_list, pid_t pid, int state, char *cmdline) 
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == 0) {
            job_list[i].pid = pid;
            job_list[i].state = state;
            job_list[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(job_list[i].cmdline, cmdline);
            if(verbose){
                printf("Added job [%d] %d %s\n",
                       job_list[i].jid,
                       job_list[i].pid,
                       job_list[i].cmdline);
            }
            return 1;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int 
deletejob(struct job_t *job_list, pid_t pid) 
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == pid) {
            clearjob(&job_list[i]);
            nextjid = maxjid(job_list)+1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t 
fgpid(struct job_t *job_list) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].state == FG)
            return job_list[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t 
*getjobpid(struct job_t *job_list, pid_t pid) {
    int i;

    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].pid == pid)
            return &job_list[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *job_list, int jid) 
{
    int i;

    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].jid == jid)
            return &job_list[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int 
pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].pid == pid) {
            return job_list[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void 
listjobs(struct job_t *job_list, int output_fd) 
{
    int i;
    char buf[MAXLINE];

    for (i = 0; i < MAXJOBS; i++) {
        memset(buf, '\0', MAXLINE);
        if (job_list[i].pid != 0) {
            sprintf(buf, "[%d] (%d) ", job_list[i].jid, job_list[i].pid);
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
            memset(buf, '\0', MAXLINE);
            switch (job_list[i].state) {
            case BG:
                sprintf(buf, "Running    ");
                break;
            case FG:
                sprintf(buf, "Foreground ");
                break;
            case ST:
                sprintf(buf, "Stopped    ");
                break;
            default:
                sprintf(buf, "listjobs: Internal error: job[%d].state=%d ",
                        i, job_list[i].state);
            }
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
            memset(buf, '\0', MAXLINE);
            sprintf(buf, "%s\n", job_list[i].cmdline);
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
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
 * usage - print a help message
 */
void 
usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void 
unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void 
app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t 
*Signal(int signum, handler_t *handler) 
{
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
void 
sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}

