/* $begin shellmain */
#include "csapp.h"
#include <errno.h>
#define MAXARGS 128
#define MAXPATH 4096

enum cmd_id { NONE, BIN, USRBIN, CD, HISTORY, LAST, NTH, JOBS, BG, FG, KILL, EXIT };

jmp_buf jbuf;

typedef struct _his_nd{
    int id;
    char *cmd;
    struct _his_nd *next;
} his_nd;

typedef struct _job_nd{
    int pid;
    char *command;
    char state[10];
    struct _job_nd *next;
} job_nd;

volatile sig_atomic_t ischild, for_pid;

his_nd *h_front, *h_rear;
job_nd *j_front, *j_rear;
FILE *fstream;

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);

/* My function prototypes */
void load_history();
void save_history(char* cmd);

enum cmd_id eval_command(char **argv);
int execute(char** argv);
int change_dir(char **argv);

int check_special(char* cmdline);
void remove_special(char* cmdline);
void readcmd(char cmdline[], char tmp[]);
char** check_pipe(char** argv);

void cleanup();

void sigint_handler(int sig);
void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigquit_handler(int sig);

void add_job(int pid, char* command);
job_nd* change_job(int pid, char* state);
int rmv_job(int pid); // return -1 if failed.
void print_job();
int find_job(int pid);

int main() 
{
    char cmdline[MAXLINE], tmp[MAXLINE], **history;/* Command line */
    Signal(SIGINT, sigint_handler);
    Signal(SIGQUIT, sigquit_handler);
    Signal(SIGCHLD, sigchld_handler);
    Signal(SIGTSTP, SIG_IGN);

    load_history();

    while(1){
        readcmd(cmdline, tmp);
        save_history(cmdline);
        remove_special(cmdline);
       
	    /* Evaluate */
	    eval(cmdline); 
    }
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    char *argv[MAXLINE]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    int sus = 0;
    pid_t pid, ret;

    int pipefd[2], stdid[2] = {0, 1}, cnt = 0;
    sigset_t mask_all, prev_all, mask, prev;
    Sigfillset(&mask_all);
    Sigaddset(&mask, SIGCHLD);
   
    strcpy(buf, cmdline);
    bg = parseline(buf, argv);

    int idx = 0;
    while(cmdline[idx] != '\0') {
        if(cmdline[idx] == -1) cmdline[idx] = ' ';
        idx++;
    }
    
    if (argv[0] == NULL)  
		return;   /* Ignore empty lines */
   

    Sigprocmask(SIG_SETMASK, &mask, &prev);

    if(bg){
        if((pid = fork()) == 0){
            ischild = 1;
            Signal(SIGINT, SIG_IGN);
            Sigprocmask(SIG_SETMASK, &prev, NULL);

            pid_t result;
            result = execute(argv);
            if(result > 0) waitpid(result, NULL, 0);
            exit(0);
        }
    } else {
        pid = execute(argv);
    }

    if(pid > 0) add_job(pid, cmdline);

    int jmpret = 0;
    if((jmpret = setjmp(jbuf)) == -1){
        if(pid > 0) Kill(pid, SIGTSTP);
        sus = 1, bg = 1;
        change_job(pid, "suspended");
        Signal(SIGTSTP, SIG_IGN);
    } else if(jmpret){
        pid = jmpret;
        job_nd *jd = change_job(pid, "running");
        printf("[%d] %s %s\n", jd->pid, jd->state, jd->command);
        Kill(pid, SIGCONT);
    } 

    Sigprocmask(SIG_SETMASK, &prev, NULL);
    
    if(!bg){
        int status; 
        Signal(SIGTSTP, sigtstp_handler);
        if(pid > 0){
            for_pid = pid;
            waitpid(pid, &status, 0);
            for_pid = 0;
        }
        Signal(SIGTSTP, SIG_IGN);

        Sigprocmask(SIG_SETMASK, &mask_all, &prev_all);
        if(pid > 0) rmv_job(pid);
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);

    } else if(!sus){    
        printf("%d %s\n", pid, cmdline);
    }

    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{

    if (!strcmp(argv[0], "quit")){
        cleanup();
        exit(0);
    } /* quit command */
		 
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
		return 0;
    
	enum cmd_id cid;
	cid = eval_command(argv);
	pid_t pid;

	if (cid == BIN){
		/* Execute /bin/ls */
		char filename[MAXLINE] = "/bin/";
		strcat(filename, argv[0]);
		if((pid = Fork()) == 0){
                Signal(SIGINT, SIG_IGN);
                Signal(SIGTSTP, SIG_DFL);
        
				execve(filename, argv, environ);
        }
		return pid;
	}

	if (cid == USRBIN){
		char filename[MAXLINE] = "/usr/bin/";
		strcat(filename, argv[0]);
		if((pid = Fork()) == 0){
                Signal(SIGINT, SIG_IGN);
                Signal(SIGTSTP, SIG_DFL);
				execve(filename, argv, environ);
        }
		return pid;
	}
	
	if (cid == CD) {
		change_dir(argv);
        return 0;    
	}

	if (cid == HISTORY) {
        his_nd *tmp = h_front->next;
        while(tmp){
            printf("%d %s\n", tmp->id, tmp->cmd);
            tmp = tmp->next;
        } 
        return 0;    
	}

    if (cid == LAST) {
        char buff[MAXLINE];
        strcpy(buff, h_rear->cmd);
        printf("%s\n", buff);

        remove_special(buff);
        eval(buff);
        return 0;
    }

    if (cid == NTH){
        int n = atoi(&argv[0][1]);
        his_nd* tmp = h_front;
        for(int i = 0; i < n && tmp; i++)
            tmp = tmp->next;
        if(tmp && tmp->cmd){
            char buff[MAXLINE];
            strcpy(buff, tmp->cmd);
            printf("%s\n", buff);
            save_history(buff);

            remove_special(buff);
            eval(buff);
            return 0;
        } 
        return 0;
    }

    if (cid == JOBS){
        print_job();
        return 0;
    }

    if (cid == BG){
        int target = atoi(&argv[1][1]);
        if(find_job(target) == -1){
            printf("No Such Job\n");
            return 0;
        }
        longjmp(jbuf, target);
        Kill(target, SIGCONT);
        job_nd *jd = change_job(pid, "running");
        printf("[%d] %s %s\n", jd->pid, jd->state, jd->command);
        return 0;
    }

    if(cid == FG){
        int target = atoi(&argv[1][1]);
        if(find_job(target) == -1){
            printf("No Such Job\n");
            return 0;
        }
        longjmp(jbuf, target);
        return 0;
    } 

    if(cid == KILL){
        int target = atoi(&argv[1][1]);
        if(find_job(target) == -1){
            printf("No Such Job\n");
            return 0;
        }
        Kill(target, 9);
        return 0;
    }


	if (cid == EXIT){
	    cleanup();
        exit(0);
    }

    return -1;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf) + 1] = '\0';
    buf[strlen(buf)] = ' ';

    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
		buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
		argv[argc++] = buf;
		*delim = '\0';
		buf = delim + 1;
		while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
		return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
		argv[--argc] = NULL;
    else if(argv[argc-1][strlen(argv[argc-1])-1] == '&'){
        bg = 1;
        argv[argc-1][strlen(argv[argc-1])-1] = '\0';    
    }

    for(int i = 0; i < argc; i++){
        int k = 0;
        while(argv[i][k] != '\0'){
            if(argv[i][k] == -1)
                argv[i][k] = ' ';
            k++;
        }
    }

    return bg;
}
/* $end parseline */

void load_history(){
    int len;
    char buff[MAXLINE], *cptr;
    his_nd* new_nd;

    h_front = malloc(sizeof(his_nd));
    h_front->id = 0;
    h_front->cmd = NULL;
    h_front->next = NULL;
    h_rear = h_front;

    if((fstream = fopen(".history.txt", "r")) == NULL){
        fstream = fopen(".history.txt", "a");
        return;
    }

    while(1){
        cptr = fgets(buff, MAXLINE, fstream); 
        if(cptr == NULL) break;
        
        new_nd = malloc(sizeof(his_nd));
        new_nd->id = h_rear->id + 1;
        new_nd->next = NULL;
        len = strlen(buff);

        if(buff[len - 1] == '\n') {
            buff[len - 1] = '\0';
            len--;
        }

        new_nd->cmd = malloc((len + 10));
        strcpy(new_nd->cmd, buff);

        h_rear->next = new_nd;
        h_rear = new_nd;
    }


    fclose(fstream);

    if((fstream = fopen(".history.txt", "a")) == NULL){
        printf("history file open failed\n");
    }
}

void save_history(char* cmd){
    if(cmd == NULL || cmd[0] == '\0' || cmd[0] == '!') return;
    if(fstream == NULL){
        if((fstream = fopen(".history.txt", "a")) == NULL){
            return;
        }
    }

    int len = strlen(cmd);

    his_nd *new_node = malloc(sizeof(his_nd));
    new_node->cmd = malloc((len + 10) * sizeof(char));
    strcpy(new_node->cmd, cmd);
    new_node->id = h_rear->id + 1;
    new_node->next = NULL;

    h_rear->next = new_node;
    h_rear = new_node;

    fputs(cmd, fstream);
    fputs("\n", fstream); 
    if(feof(fstream)) printf("history save failed\n");
}

/* $begin eval_command */
/* eval_command - Evaluate command and return command id */
enum cmd_id eval_command(char **argv)
{
	if(!strcmp(argv[0], "ls")
    || !strcmp(argv[0], "mkdir")
    || !strcmp(argv[0], "rmdir")
    || !strcmp(argv[0], "touch")
    || !strcmp(argv[0], "cat")
    || !strcmp(argv[0], "echo")
	|| !strcmp(argv[0], "pwd")
    || !strcmp(argv[0], "grep")
    || !strcmp(argv[0], "less"))
	    return BIN;

    if(!strcmp(argv[0], "sort"))
        return USRBIN;
	
	if(!strcmp(argv[0], "cd"))
	    return CD;

	if(!strcmp(argv[0], "history")) 
	    return HISTORY;

    if(!strcmp(argv[0], "!!"))
        return LAST;

    if(argv[0][0] == '!' && argv[0][1] != '\0'){
        int i;
        for(i = 1; argv[0][i] != '\0'; i++){
            if(!('0' <= argv[0][i] && argv[0][i] <= '9')) break;
        }
        if(argv[0][i] == '\0') return NTH;
    }

    if(!strcmp(argv[0], "jobs"))
        return JOBS;

    if(!strcmp(argv[0], "bg"))
        return BG;

    if(!strcmp(argv[0], "fg"))
        return FG;

    if(!strcmp(argv[0], "kill"))
        return KILL;


	if(!strcmp(argv[0], "exit"))
	    return EXIT;

	return NONE;
}
/* $end eval_command */

/* $begin change_dir */
/* Change directory and return 0 if successed, -1 if failed. */
int change_dir(char **argv)
{
    char pwd[MAXPATH], home[MAXPATH];
    strcpy(pwd, getenv("PWD"));
    strcpy(home, getenv("HOME"));


    if(argv[1] == NULL || !strcmp(argv[1], "~")){
       if(home != NULL){
           if(chdir(home) == -1){
               printf("Failed to change directory.\n");
               return -1;
           } 
           return 0;
       }
    } else {
        if(chdir(argv[1]) == -1) {
            printf("%s: No such file or directory\n", argv[1]);
            return -1;
        }
        return 0;
    }
}
/* end change_dir */

void cleanup(){
    his_nd *htmp, *hrmv;
    htmp = h_front;

    while(htmp){
        hrmv = htmp;
        htmp = htmp->next;
        free(hrmv);
    }
    job_nd* jtmp, *jrmv;
    jtmp = j_front;
    while(jtmp){
        jrmv = jtmp;
        
        if(!ischild && jrmv->pid > 0){
            Kill(jrmv->pid, SIGKILL);
            waitpid(jrmv->pid, NULL, 0);
        }

        jtmp = jtmp->next;
        free(jrmv);
    }

    int status;
    while(waitpid(-1, &status, 0) != -1) ;

    if(errno != ECHILD) sio_error("wait error\n");

    if(fstream) fclose(fstream);
}

int check_special(char* cmdline){
    /* delete escape character */
    char quote_type = '\0';
    int i, escape_flag = 0, len = strlen(cmdline);

    for(i = 0; i < len; i++){
        if(escape_flag) {
            escape_flag = 0;
            continue;
        }
        
        if(cmdline[i] == '\'' || cmdline[i] == '\"'){
            if(quote_type == '\0'){
                quote_type = cmdline[i];
            } else if(cmdline[i] == quote_type){
                quote_type = '\0';
            }
        }
    }

    return quote_type == '\0';
}

void remove_special(char* cmdline){
    /* delete escape character */
    char quote_type = '\0';
    int i, quote_count = 0, escape_count = 0, escape_flag = 0, len = strlen(cmdline);

    for(i = 0; i < len; i++){
        if(cmdline[i] == ' ' && quote_type != '\0') cmdline[i] = -1; 

        cmdline[i - quote_count - escape_count] = cmdline[i];

        if(escape_flag) {
            escape_flag = 0;
            continue;
        }
        
        if(cmdline[i] == '\'' || cmdline[i] == '\"'){
            if(quote_type == '\0'){
                quote_type = cmdline[i];
                quote_count++;
            } else if(cmdline[i] == quote_type){
                quote_count++;
                quote_type = '\0';
            }
        }
    }
    cmdline[len - quote_count - escape_count] = '\0';
}

void readcmd(char cmdline[], char tmp[]){
    char quote_type, *ptr; /* Command line */
    int cmd_len, tmp_len;

    
	/* Read */
    printf("CSE4100-MP-P1> ");
	ptr = fgets(cmdline, MAXLINE, stdin); 
	if (feof(stdin))
        exit(0);

    cmd_len = strlen(cmdline);
    cmdline[--cmd_len] = '\0';

    while(1){
        while(cmd_len > 0 && cmdline[cmd_len - 1] == '\\'){
            printf("> ");
            ptr = fgets(tmp, MAXLINE - cmd_len, stdin);
            if(feof(stdin))
                exit(0);
            
            
            tmp_len = strlen(tmp);
            tmp[--tmp_len] = '\0';
            cmd_len += tmp_len;

            strcat(cmdline, tmp);
        }

        if(check_special(cmdline)) break;

        printf("> ");
        ptr = fgets(tmp, MAXLINE - cmd_len, stdin);
        if(feof(stdin))
            exit(0);

        tmp_len = strlen(tmp);
        tmp[--tmp_len] = '\0';

        cmd_len += tmp_len;
        strcat(cmdline, tmp);
    }
}

char** check_pipe(char** argv){
    char** ret = NULL;
    int idx = 0;
    while(argv[idx] != NULL && strcmp(argv[idx], "|")){
        idx++;
    }

    if(argv[idx] != NULL && strcmp(argv[idx], "|") == 0){
        ret = &argv[idx + 1];
        argv[idx] = NULL;
    }

    return ret;
}

int execute(char** argv){
    pid_t pid;           /* Process id */

    int pipefd[2], stdid[2] = {0, 1}, cnt=0;

    char** nextargv = check_pipe(argv);

    if(nextargv){
        stdid[0] = dup(stdid[0]);
        stdid[1] = dup(stdid[1]);
    }

    while(nextargv){
        if(pipe(pipefd) == -1){
            unix_error("pipe open err");
        }

        if((pid = fork()) == 0){
            dup2(pipefd[1], 1);
            close(pipefd[0]);
            close(pipefd[1]);

            if ((pid = builtin_command(argv)) == -1) {
                if (execve(argv[0], argv, environ) < 0) {
                    printf("%s: Command not found.\n", argv[0]);
                }
            }
            if(pid > 0) waitpid(pid, NULL, 0);
            exit(0);
        }

        dup2(pipefd[0], 0);
        close(pipefd[0]);
        close(pipefd[1]);

        argv = nextargv;
        nextargv = check_pipe(argv);
    }

    if ((pid = builtin_command(argv)) == -1) { //quit -> exit(0), & -> ignore, other -> run
        if((pid = fork()) == 0){
            if (execve(argv[0], argv, environ) < 0) {	//ex) /bin/ls ls -al &
                printf("%s: Command not found.\n", argv[0]);
            }
            exit(0);
        }
    }

    if(stdid[0] != 0){
        dup2(stdid[0], 0);
        dup2(stdid[1], 1);
        close(stdid[0]);
        close(stdid[1]);
    }

    return pid;
}

void sigquit_handler(int sig){
    sigset_t mask, prev;
    Sigfillset(&mask);
    Sigprocmask(SIG_SETMASK, &mask, &prev);
    
    int old = errno;

    cleanup();

    errno = old;

    Sigprocmask(SIG_SETMASK, &prev, NULL);

    exit(0);
}

void sigchld_handler(int sig){
    sigset_t mask, prev;
    Sigfillset(&mask);
    Sigprocmask(SIG_SETMASK, &mask, &prev);
    
    int old = errno, status;
    pid_t pid;

    while((pid = waitpid(-1, NULL, WNOHANG)) > 0){
        rmv_job(pid);
    }
    
    if(pid == -1 && errno != ECHILD)
       sio_error("wait error\n"); 
    

    errno = old;

    Sigprocmask(SIG_SETMASK, &prev, NULL);
}

void sigint_handler(int sig){

    sigset_t mask, prev;
    Sigfillset(&mask);
    Sigprocmask(SIG_SETMASK, &mask, &prev);
    
    int old = errno, status;
    pid_t pid;
    
    if(for_pid > 0) {
        kill(for_pid, SIGINT);
        while((pid = waitpid(-1, NULL, 0)) != -1){
            rmv_job(pid);
        }
    }

    errno = old;

    Sigprocmask(SIG_SETMASK, &prev, NULL);
}


void sigtstp_handler(int sig){
    longjmp(jbuf, -1);
}

void add_job(int pid, char* command){
    job_nd *newNode = malloc(sizeof(job_nd));
    newNode->pid = pid;
    strcpy(newNode->state, "running");
    newNode->command = malloc(strlen(command) + 5);
    strcpy(newNode->command, command);
    newNode->next = NULL;

    if(!j_front) {
        j_front = malloc(sizeof(job_nd));
        j_front->pid = 0;
        j_front->next = newNode;
        j_rear = newNode;
    }else{
        j_rear->next = newNode;
        j_rear = newNode;
    }
}

job_nd* change_job(int pid, char* state){
    job_nd *tmp;
    tmp = j_front;
    while(tmp && tmp->pid != pid) tmp = tmp->next;
    strcpy(tmp->state, state);
    return tmp;
}

int rmv_job(int pid){
    job_nd *tmp, *rmv;
    tmp = j_front;

    while(tmp->next && tmp->next->pid != pid) tmp = tmp->next;


    if(!(tmp->next)) return -1;

    rmv = tmp->next;
    tmp->next = rmv->next;

    if(rmv == j_rear) j_rear = tmp;
    free(rmv);
    return 1;
}

void print_job(){
    job_nd* tmp = j_front->next;
    while(tmp){
        printf("[%d] %s %s\n", tmp->pid, tmp->state, tmp->command);
        tmp = tmp->next;
    }
}

int find_job(int pid){
    job_nd* tmp = j_front->next;
    while(tmp && tmp->pid != pid) tmp = tmp->next;

    if(tmp) return 1;
    return -1;
}
