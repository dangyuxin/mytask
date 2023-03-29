#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 128
#define INPUT_REDIR 1
#define OUTPUT_REDIR 2
#define APPEND_REDIR 3
#define NONE_REDIR 0

int redir_status=NONE_REDIR;
bool HT=false;
bool PP=false;
int COUNT=0;

void parse_args(char* line, char** args) {
    char* token = strtok(line, " \t\r\n\a");
    args[0] = token;
    int i = 1;
    if(strcmp(args[0],"ls")==0)
        {
            args[i++]=(char*)"--color=auto";
        }
        if(strcmp(args[0],"ll")==0)
        {
            args[0]=(char*)"ls";
            args[i++]=(char*)"-l";
            args[i++]=(char*)"--color=auto";
        }
    while (token != NULL) {
        token = strtok(NULL, " \t\r\n\a");
        args[i++] = token;
    }
    COUNT=i-1;
}

void parse(char *line){
    HT=false;
    PP=false;
    for(int i=0;i<strlen(line);i++){
        if(line[i]=='&'){
            HT=true;
            line[i]=' ';
        }
        if(line[i]=='|'){
            PP=true;
        }
    }
}

void DoPipe(char **argv, int count)
{
    pid_t pid;
    int ret[10];
    int number=0;
    for(int i=0;i<count;i++)
    {
    if(!strcmp(argv[i],"|"))
    {
        ret[number++]=i;
    }
    }
    int cmd_count=number+1;
    char* cmd[cmd_count][10];
    for(int i=0;i<cmd_count;i++)
    {
    if(i==0)
    {
        int n=0;
        for(int j=0;j<ret[i];j++)
        {
        cmd[i][n++]=argv[j];
        }
        cmd[i][n]=NULL;
    }
    else if(i==number)
    {
        int n=0;
        for(int j=ret[i-1]+1;j<count;j++)
        {
        cmd[i][n++]=argv[j];
        }
        cmd[i][n]=NULL;
    }
    else 
    {
        int n=0;
        for(int j=ret[i-1]+1;j<ret[i];j++)
        {
        cmd[i][n++]=argv[j];
        }
        cmd[i][n]=NULL;
    }
    }
    int fd[number][2];  
    for(int i=0;i<number;i++)
    {
    pipe(fd[i]);
    }
    int i=0;
    for(i=0;i<cmd_count;i++)
    {
    pid=fork();
    if(pid==0)
    break;
    }
    if(pid==0)
    {
    if(number)
    {
        if(i==0)
        {
        dup2(fd[0][1],1); 
        close(fd[0][0]);
        for(int j=1;j<number;j++)
        {
            close(fd[j][1]);
            close(fd[j][0]);
        }
        }
        else if(i==number)
        {
        dup2(fd[i-1][0],0);
        close(fd[i-1][1]);
        for(int j=0;j<number-1;j++)
        {
            close(fd[j][1]);
            close(fd[j][0]);
        }
        }
        else
        {
        dup2(fd[i-1][0],0);
        close(fd[i-1][1]);
        dup2(fd[i][1],1);
        close(fd[i][0]);
        for(int j=0;j<number;j++)
        {
                if(j!=i&&j!=(i-1))
                {
                close(fd[j][0]);
                close(fd[j][1]);
                }
        }
        }
    }
    execvp(cmd[i][0],cmd[i]);
    perror("execvp");
    exit(1);
    }
    for(i=0;i<number;i++)
    {
        close(fd[i][0]);
        close(fd[i][1]);
    }
    for(int j=0;j<cmd_count;j++)
    wait(NULL);
}

int has_pipe(char** args, int* pipe_position) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            *pipe_position = i;
            return 1;
        }
    }
    return 0;
}

char *CheckRedir(char *start)
{
    char *end=start+strlen(start)-1;
 
    while(end>=start)
    {
        if(*end=='>')
        {
            if(*(end-1)=='>')
            {
                redir_status=APPEND_REDIR;
                *(end-1)='\0';
                end++;
                while(*end==' ')
                    ++end;
                break;
            }
            redir_status=OUTPUT_REDIR;
            *end='\0';
            end++;
            while(*end==' ')
                ++end;
            break;
        }
        else if(*end=='<')
        {
            redir_status=INPUT_REDIR;
            *end='\0';
            end++;
            while(*end==' ')
                ++end;
            break;
        }
        else
        {
            end--;
        }
    }
    if(end>=start)
    {
        return end;
    }
    else
    {
        return NULL;
    }
}

void run_command_with_pipe(char** args1, char** args2) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid1 = fork();
    if (pid1 == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid1 == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execvp(args1[0], args1);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid2 == 0) {
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        execvp(args2[0], args2);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

void run_command(char** args) {
    int pipe_position;
    // if (has_pipe(args, &pipe_position)) {
    //     char* args1[MAX_ARGS];
    //     char* args2[MAX_ARGS];
    //     memcpy(args1, args, (pipe_position) * sizeof(char*));
    //     args1[pipe_position] = NULL;
    //     memcpy(args2, &args[pipe_position + 1], (MAX_ARGS - pipe_position - 1) * sizeof(char*));
    //     run_command_with_pipe(args1, args2);
    // } 
    if(PP){
        DoPipe(args,COUNT);
    }
    else {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) {
            execvp(args[0], args);
            perror("execvp");
            exit(EXIT_FAILURE);
        } else {
            wait(NULL);
        }
    }
}

int main() {
    char line[MAX_LINE];
    char* args[MAX_ARGS];
    char prior[100]={'\0'};
    args[0]="\0";
    signal(SIGINT,SIG_IGN); //屏蔽ctrl+c
    int x=dup(0),y=dup(1);
    while (1) {
        printf("[dyx-super-shell]# ");
        if (fgets(line, MAX_LINE, stdin) == NULL) {
            perror("fgets");
            exit(EXIT_FAILURE);
        }
        // line=readline(line);
        if(strcmp(line,"\n")==0||!line)
            continue;
        line[strlen(line)-1]='\0';
        // add_history(line);
        parse(line);
        char *sep=CheckRedir(line);
        parse_args(line, args);
        if (args[0] == NULL) {
            continue;
        }
        if (strcmp(args[0], "exit") == 0) {
            break;
        }
        if (strcmp(args[0], "cd") == 0)
        {
            if (args[1] && strcmp(args[1], "-") == 0)
            {
                if (prior)
                {
                    char buf[100];
                    getcwd(buf, 100);
                    printf("%s\n", prior);
                    chdir(prior);
                    strcpy(prior, buf);
                }
                else
                { 
                    printf("bash: cd: OLDPWD 未设定\n");
                }
            }
            else
            {
                getcwd(prior, 100);
                chdir(args[1]);
            }
            continue;
        }
        if(HT){
                int fd=-1;
                fd=open("/dev/null",O_WRONLY);
                dup2(fd,0);
                dup2(fd,1);
                close(fd);
        }
        if(sep!=NULL)
        {
            int fd=-1;
            switch(redir_status) {
                case INPUT_REDIR:
                    fd=open(sep,O_RDONLY);
                    dup2(fd,0);
                    close(fd);
                    break;
                case OUTPUT_REDIR:
                    fd=open(sep,O_WRONLY|O_TRUNC|O_CREAT,0666);
                    dup2(fd,1);
                    close(fd);
                    break;
                case APPEND_REDIR:
                    fd=open(sep,O_WRONLY|O_APPEND|O_CREAT,0666);
                    dup2(fd,1);
                    close(fd);
                    break;
                default:
                    printf("bug?\n");
                    break;
            }
        }
        run_command(args);
        // free(line);
        // line=NULL;
        dup2(x,0);
        dup2(y,1);
    }
    return 0;
}
