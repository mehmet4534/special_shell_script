#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
 
#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */

#define CREATE_FLAGS (O_WRONLY | O_CREAT | O_TRUNC)
#define CREATE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

#define CREATE_FLAGS2 (O_WRONLY | O_CREAT | O_APPEND)

#define CREATE_FLAGS3 (O_WRONLY | O_APPEND)

#define CREATE_FLAGS4 (O_RDONLY )
 
/* The setup function below will not return any value, but it will just: read
in the next command line; separate it into distinct arguments (using blanks as
delimiters), and set the args array entries to point to the beginning of what
will become null-terminated, C-style strings. */
struct process{
	int index;
	char *value;
	pid_t pid;
	struct process *next;
};
typedef struct process PROCESS;
PROCESS *first=NULL;
PROCESS *last=NULL;

void setup(char inputBuffer[], char *args[],int *background)
{
    int length, /* # of characters in the command line */
        i,      /* loop index for accessing inputBuffer array */
        start,  /* index where beginning of next command parameter is */
        ct;     /* index of where to place the next parameter into args[] */
    
    ct = 0; 
    /* read what the user enters on the command line */
    		length = read(STDIN_FILENO,inputBuffer,MAX_LINE);  
    
    /* 0 is the system predefined file descriptor for stdin (standard input),
       which is the user's screen in this case. inputBuffer by itself is the
       same as &inputBuffer[0], i.e. the starting address of where to store
       the command that is read, and length holds the number of characters
       read in. inputBuffer is not a null terminated C-string. */

    start = -1;
    if (length == 0)
        exit(0);            /* ^d was entered, end of user command stream */

/* the signal interrupted the read system call */
/* if the process is in the read() system call, read returns -1
  However, if this occurs, errno is set to EINTR. We can check this  value
  and disregard the -1 value */
    if ( (length < 0) && (errno != EINTR) ) {
        perror("error reading the command");
	exit(-1);           /* terminate with error code of -1 */
    }
	//printf("%d",length);
	//printf(">>%s<<",inputBuffer);
    for (i=0;i<length;i++){ /* examine every character in the inputBuffer */

        switch (inputBuffer[i]){
	    case ' ':
	    case '\t' :               /* argument separators */
		if(start != -1){
                    args[ct] = &inputBuffer[start];    /* set up pointer */ 
		    ct++;
		}
                inputBuffer[i] = '\0'; /* add a null char; make a C string */
		//printf("%s\n",args[ct-1]);
		start = -1;
		break;

            case '\n':                 /* should be the final char examined */
		if (start != -1){
                    args[ct] = &inputBuffer[start];     
		    ct++;
		}
                inputBuffer[i] = '\0';
		//printf("%s\n",args[ct-1]);
                args[ct] = NULL; /* no more arguments to this command */
		//printf("%s\n",args[ct]);
		break;

	    default :             /* some other character */
		if (start == -1)
		    start = i;
                if (inputBuffer[i] == '&'){
		    *background  = 1;
		    //for ">&" redirection 
		    if(inputBuffer[i-1]!='>'&&inputBuffer[i-2]!='\0')
                   	 inputBuffer[i] = '\0';
		}
	} /* end of switch */
     }    /* end of for */
     args[ct] = NULL; /* just in case the input line was > 80 */

	/*for (i = 0; i <= ct; i++)
		printf("args %d = %s\n",i,args[i]);*/
} /* end of setup routine */
const char *execute(char *args){
	//must be restricted with null
	char *path=getenv("PATH")+'\0';
	strtok(path,":");
	while(path){
		char *path2=(char *)malloc(sizeof path);
		strcpy(path2,path);
		strcat(path2,"/");
		strcat(path2,args);
		const char *path3=path2;
		if(access(path3,F_OK)==0)
			return path2;
		path=strtok(NULL,":");
		
	}
return NULL;
}
char* seperate(char *args[],char *cmd[],int *i,int *background){
	int k=0;
	//a new char pointer is allocated for holding all command.
	char *sep=malloc(sizeof(args[*i]));
	//the command arguments are placed on cmd pointer array until args pointer arrives null or '&'
	//sep char pointer is fulled.
	while(args[*i]!=NULL){
		if(strcmp(args[*i],"&")==0){
			cmd[k]=NULL;
			*i=*i+1;
			return sep;
		}
		cmd[k]=args[*i];
		strcat(sep,args[*i]);
		strcat(sep," ");
		k++;
		*i=*i+1;
	}
	*background=0;
	cmd[k]=NULL;
	return sep;
}
void addProcess(char *cmd,pid_t childpid,int *ind){
	PROCESS *newProc=malloc(sizeof(PROCESS));
	newProc->index=*ind;
	newProc->value=malloc(sizeof(cmd));
	strcpy(newProc->value,cmd);
	newProc->pid=childpid;
	newProc->next=NULL;
	if (first==NULL){
		*ind=1;
		newProc->index=*ind;
		first=newProc;
		last=newProc;
	}
	else{
		last->next=newProc;
		last=last->next;
	}
	*ind=*ind+1;
}
void showProcess(){
	PROCESS *temp;
	temp=first;
	printf("\tRunning:\n\n");
	while(temp!=NULL){
		if(waitpid(temp->pid,NULL,WNOHANG)==0)
			printf("\t\t[%d] %s(%ld)\n",temp->index,temp->value,(long)temp->pid);
		temp=temp->next;
	}
	printf("\tTerminated:\n\n");
	temp=first;
	PROCESS *prev;
	prev=first;
	while(temp!=NULL){
		if(waitpid(temp->pid,NULL,WNOHANG)<0){
			printf("\t\t[%d] %s(%ld)\n",temp->index,temp->value,(long)temp->pid);
			if(prev==temp){
				first=prev->next;
				if(first==NULL)
					last=NULL;
				free(temp);
				temp=first;
				prev=temp;
				continue;				
			}
			else{
				prev->next=temp->next;
				if(prev->next==NULL)
					last=prev;
				free(temp);
				temp=prev;
			}
		}
		prev=temp;
		temp=temp->next;
		
	}
}
void killProcess(char *cmd){
	if(*cmd=='%'){
		cmd++;
		//get the number
		int ind=atoi(cmd);
		//whether the getting value is a number
		if(ind==0){
			printf("wrong request\n");
			return;	
		}
		//research the the number in background processes
		PROCESS *temp;
		temp=first;
		while(temp!=NULL){
			if(temp->index==ind){
				if(kill(temp->pid,SIGUSR1)==-1){
					perror("Failed to send the SIGUSR1 signal to child");
					return;
				}
				else{
					return;
				}
			}
			temp=temp->next;	
		}
	}
	else{
		pid_t childpid=atoi(cmd);
		if(childpid==0){
			printf("wrong request\n");
			return;	
		}	
		PROCESS *temp;
		temp=first;
		while(temp!=NULL){
			if(temp->pid==childpid){
				if(kill(temp->pid,SIGUSR1)==-1){
					perror("Failed to send the SIGUSR1 signal to child");
					return;
				}
				else{
					return;
				}
			}
			temp=temp->next;
		}
	}
	printf("such process is not found\n");
}
void exitShell(){
	PROCESS *temp;
	temp=first;
	while(temp!=NULL){
		if(waitpid(temp->pid,NULL,WNOHANG)==0){
			printf("There are running processes.You need to terminate the running processes.\n");
			return;
		}
		temp=temp->next;
	}
	exit(0);
}
char *seperateForRedirection(char *cmd[],char* fileName[]){
	char *sep;
	int i=0;
	while(cmd[i]!=NULL){
		//one condition for >, >> and >&
		if(strcmp(cmd[i],">")==0||strcmp(cmd[i],">>")==0||strcmp(cmd[i],">&")==0){
			if(cmd[i+1]!=NULL&&cmd[i+2]==NULL){
				fileName[0]=cmd[i+1];
				fileName[1]=NULL;
				sep=malloc(sizeof cmd[i]);
				strcpy(sep,cmd[i]);
				cmd[i]=NULL;
				return sep;
			}
			fileName[0]=NULL;
			fileName[1]=NULL;
			return NULL;
		}
		//two condition for <, either of "< filename" or "< filename > filename2"
		else if(strcmp(cmd[i],"<")==0&&cmd[i+1]!=NULL){
			fileName[0]=cmd[i+1];
			sep=malloc(sizeof cmd[i]);
			strcpy(sep,cmd[i]);
			cmd[i]=NULL;
			if(cmd[i+2]==NULL){
				fileName[1]=NULL;
				return sep;
			}
			else if(strcmp(cmd[i+2],">")==0&&cmd[i+3]!=NULL){
				fileName[1]=cmd[i+3];
				if(cmd[i+4]==NULL){
					return sep;
				}
			}
			fileName[1]=NULL;
			return NULL;
		}
		i++;
	}
	fileName[0]=NULL;
	fileName[1]=NULL;
	return NULL;
}
void redirection(char *std,char *fileName[]){
	if(strcmp(std,">")==0||strcmp(std,">>")==0){
		int fd;
		if(strcmp(std,">")==0)
			fd = open(fileName[0], CREATE_FLAGS, CREATE_MODE);
		else
			fd = open(fileName[0], CREATE_FLAGS2, CREATE_MODE);
		if (fd == -1) {
			perror("Failed to open");
			exit(0);
		}
		if (dup2(fd, STDOUT_FILENO) == -1) {
			perror("Failed to redirect standard output");
			exit(0);
		}
		if (close(fd) == -1) {
			perror("Failed to close the file");
			exit(0);
		}
	}
	else if(strcmp(std,">&")==0){
		int fd;
		fd = open(fileName[0], CREATE_FLAGS3, CREATE_MODE);
		if (fd == -1) {
			perror("Failed to open");
			exit(0);
		}
		if (dup2(fd, STDERR_FILENO) == -1) {
			perror("Failed to redirect standard error");
			exit(0);
		}
		if (close(fd) == -1) {
			perror("Failed to close the file");
			exit(0);
		}
	}
	else if(strcmp(std,"<")==0){
		int fd;
		int fd2;
		fd = open(fileName[0], CREATE_FLAGS4, CREATE_MODE);
		if (fd == -1) {
			perror("Failed to open");
			exit(0);
		}
		if (dup2(fd, STDIN_FILENO) == -1) {
			perror("Failed to redirect standard error");
			exit(0);
		}
		if (close(fd) == -1) {
			perror("Failed to close the file");
			exit(0);
		}
		if(fileName[1]!=NULL){
			fd2 = open(fileName[1], CREATE_FLAGS3, CREATE_MODE);
			if (fd2 == -1) {
				perror("Failed to open");
				exit(0);
			}
			if (dup2(fd2, STDOUT_FILENO) == -1) {
				perror("Failed to redirect standard error");
				exit(0);
			}
			if (close(fd2) == -1) {
				perror("Failed to close the file");
				exit(0);
			}
		}
	}
}
void addForPipe(char *cmd[]){
	
	char *value[10];
	int i=0;
	int k=0;
	int *ptr=malloc(15*sizeof (int*));
	ptr[0]=-1;
	ptr[1]=0;
	int a=1;
	while(cmd[i]!=NULL){
		if(strcmp(cmd[i],"|")==0){
			cmd[i]=NULL;
			k=0;
			if(i==0||i==ptr[a]){
				perror("unexpected token");
				exit(-1);
			}
			a++;
			i++;
			ptr[a]=i;
			continue;
		}
		value[k]=cmd[i];
		
		i++;
		k++;
	}
	ptr[a+1]=-1;

	const char *path;
	while(ptr[a-1]!=-1){
		pid_t childpid;
		int fd[2];
		if ((pipe(fd) == -1) || ((childpid = fork()) == -1)) {
			perror("Failed to setup pipeline");
			exit(-1);
		}
		if (childpid == 0) {
			if (dup2(fd[1], STDOUT_FILENO) == -1)
				perror("Failed to redirect stdout");
			else if ((close(fd[0]) == -1) || (close(fd[1]) == -1))
				perror("Failed to close extra pipe descriptors");
			else {
				a--;
			}
		}
		else{
			if (dup2(fd[0], STDIN_FILENO) == -1) 
				perror("Failed to redirect stdin");
			else if ((close(fd[0]) == -1) || (close(fd[1]) == -1))
				perror("Failed to close extra pipe file descriptors");
			else {
				char *tmp[10];
				int k=ptr[a];
				int i=0;
				if(cmd[k]==NULL){
					perror("unexpected");
					exit(-1);
				}
				while(cmd[k]!=NULL){				
					tmp[i]=cmd[k];
					k++;
					i++;
				}
				tmp[i]=NULL;
				if((path=execute(tmp[0]))!=NULL){
					//printf("girdi0\n");
					execv(path,tmp);
					exit(-1);				
				}
				else{
					perror("not found");
					exit(0);				
				}	
			}
		}
	}
	if((path=execute(cmd[0]))!=NULL){
		//printf("girdi0\n");
		execv(path,cmd);
		exit(-1);				
	}
	else{
		perror("not found");
		exit(0);				
	}	
}

void foregroundFromBackground(char *cmd){
	if(*cmd=='%'){
		cmd++;
		int ind=atoi(cmd);
		if(ind==0){
			printf("wrong request\n");
			return;	
		}
		PROCESS *temp,*prev;
		temp=first;
		prev=temp;
		while(temp!=NULL){
			if(temp->index==ind){
				pid_t childpid;
				childpid = waitpid(temp->pid, NULL, WNOHANG);
				while(childpid!=-1){
					childpid = waitpid(temp->pid, NULL, WNOHANG);	
				}
				if(prev==temp){
					first=prev->next;
					if(first==NULL)
						last=NULL;
					free(temp);
					//temp=first;
					//prev=temp;				
				}
				else{
					prev->next=temp->next;
					if(prev->next==NULL)
						last=prev;
					free(temp);
					//temp=prev;
				}
				return;
			}
			prev=temp;
			temp=temp->next;
		}
			
	}
	printf("wrong request\n");	
	return;	
}
int main(void)
{
            char inputBuffer[MAX_LINE]; /*buffer to hold command entered */
            int background; /* equals 1 if a command is followed by '&' */
            char *args[MAX_LINE/2 + 1]; /*command line arguments */
	    int ind=1;
            while (1){
			
                        background = 0;
                        printf(" 333sh: ");
			fflush(stdout);
			pid_t childpid;
                        /*setup() calls exit() when Control-D is entered */
                       	setup(inputBuffer, args, &background);
			char *cmd[10];
			int i=0;
			char* sep=seperate(args,cmd,&i,&background);
			if(args[i]!=NULL){
				background=0;
				perror("wrong request");
			}
			else if(cmd[0]==NULL){
				continue;
			}
			else if(strcmp(cmd[0],"exit")==0&&cmd[1]==NULL){
				exitShell();
			}
			else if(strcmp(cmd[0],"kill")==0&&cmd[1]!=NULL&&cmd[2]==NULL){
				killProcess(cmd[1]);
			}
			else if(strcmp(cmd[0],"fg")==0&&cmd[1]!=NULL&&cmd[2]==NULL){
				foregroundFromBackground(cmd[1]);
			}
			else if(strcmp(cmd[0],"ps_all")==0&&cmd[1]==NULL){
				showProcess();
			}			
			else if((childpid=fork())==0){
				char* fileName[2];
				char* std=seperateForRedirection(cmd,fileName);
				if(std!=NULL){
					if(cmd[0]==NULL){
						perror("wrong request");
						exit(-1);
					}
					redirection(std,fileName);
					
				}
				addForPipe(cmd);
				
								
			}
			else if(childpid==-1){
				perror("signal is failed\n");
			}
			else if(background==0){
				//printf("b:0\n");
				while(wait(NULL)!=childpid);
				//while(wait(NULL));
			
			}
			else{
				addProcess(sep,childpid,&ind);
			}
			
                        /** the steps are:
                        (1) fork a child process using fork()
                        (2) the child process will invoke execv()
                        (3) if background == 0, the parent will wait,
                        otherwise it will invoke the setup() function again. */
			
            }
}
