/* $begin shellmain */
#include "csapp.h"
#include <errno.h>
#define MAXARGS 128
#define MAXPATH 4096


volatile sig_atomic_t ischild, for_pid;

enum cmd_id { NONE, BIN, USRBIN, CD, HISTORY, LAST, NTH, EXIT };

typedef struct _his_nd{
    int id;
    char *cmd;
    struct _his_nd *next;
} his_nd;

his_nd *front, *rear;
FILE *fstream;

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);

/* My function prototypes */
void load_history();
void save_history(char* cmd);
enum cmd_id eval_command(char **argv);
void execute(char** argv);
int change_dir(char **argv);
int check_special(char* cmdline);
void remove_special(char* cmdline);
void readcmd(char cmdline[], char tmp[]);
char** check_pipe(char** argv);
void cleanup();

void sigint_handler(int sig);

int main() 
{
    char cmdline[MAXLINE], tmp[MAXLINE], **history;/* Command line */
    Signal(SIGINT, sigint_handler);

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
    pid_t pid;           /* Process id */

    int pipefd[2], stdid[2] = {0, 1}, cnt = 0;
    
    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    
    if (argv[0] == NULL)  
		return;   /* Ignore empty lines */

    execute(argv);

	/* Parent waits for foreground job to terminate */
	if (!bg){ 
	    int status;
        if(pid > 0) waitpid(pid, NULL, 0);
	}
	else{//when there is backgrount process!
	    printf("%d %s", pid, cmdline);
    }

    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "quit")) /* quit command */
		exit(0);  
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
		return 0;
    
	enum cmd_id cid;
	cid = eval_command(argv);

	pid_t pid;

	if (cid == BIN){
		/* Execute /bin/ls */
		char filename[MAXLINE] = "/bin/";
		strcat(filename, argv[0]);
		if((pid = Fork()) == 0)
				execve(filename, argv, environ);
        
		return pid;
	}

	if (cid == USRBIN){
		/* Execute /bin/ls */
		char filename[MAXLINE] = "/usr/bin/";
		strcat(filename, argv[0]);
		if((pid = Fork()) == 0)
				execve(filename, argv, environ);
        
		return pid;
	}
	
	if (cid == CD) {
		change_dir(argv);
        return 0;    
	}

	if (cid == HISTORY) {
        his_nd *tmp = front->next;
        while(tmp){
            printf("%d %s\n", tmp->id, tmp->cmd);
            tmp = tmp->next;
        } 
        return 0;    
	}

    if (cid == LAST) {
        char buff[MAXLINE];
        strcpy(buff, rear->cmd);
        printf("%s\n", buff);

        remove_special(buff);
        eval(buff);
        return 0;
    }

    if (cid == NTH){
        int n = atoi(&argv[0][1]);
        his_nd* tmp = front;
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

    front = malloc(sizeof(his_nd));
    front->id = 0;
    front->cmd = NULL;
    front->next = NULL;
    rear = front;

    if((fstream = fopen(".history.txt", "r")) == NULL){
        fstream = fopen(".history.txt", "a");
        return;
    }

    while(1){
        cptr = fgets(buff, MAXLINE, fstream); 
        if(cptr == NULL) break;
        
        new_nd = malloc(sizeof(his_nd));
        new_nd->id = rear->id + 1;
        new_nd->next = NULL;
        len = strlen(buff);

        if(buff[len - 1] == '\n') {
            buff[len - 1] = '\0';
            len--;
        }

        new_nd->cmd = malloc((len + 10));
        strcpy(new_nd->cmd, buff);

        rear->next = new_nd;
        rear = new_nd;
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
    new_node->id = rear->id + 1;
    new_node->next = NULL;

    rear->next = new_node;
    rear = new_node;

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
    fclose(fstream);
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

void execute(char** argv){
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
                if((pid = fork()) == 0){
                    if (execve(argv[0], argv, environ) < 0) {}
                }
                exit(0);
            }

            if(pid > 0) waitpid(pid, NULL, 0);
            exit(0);
        }

        dup2(pipefd[0], 0);
        close(pipefd[0]);
        close(pipefd[1]);

        waitpid(pid, NULL, 0);

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
    if(pid > 0) {
        for_pid = pid;
        waitpid(pid, NULL, 0);
        for_pid = 0;
    }

    if(stdid[0] != 0){
        dup2(stdid[0], 0);
        dup2(stdid[1], 1);
        close(stdid[0]);
        close(stdid[1]);
    }
}

void sigint_handler(int sig){

    sigset_t mask, prev;
    Sigfillset(&mask);
    Sigprocmask(SIG_SETMASK, &mask, &prev);
    
    int old = errno, status;
    
    if(for_pid > 0) {
        kill(for_pid, SIGINT);
        while(waitpid(-1, NULL, 0) != -1);
    }

    errno = old;

    Sigprocmask(SIG_SETMASK, &prev, NULL);
}
