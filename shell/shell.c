#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>

#define _POSIX_C_SOURCE 200112L
#define _SVID_SOURCE

int del_proc (int number);
typedef struct 
{
	char *name;
	int number_of_arguments;
	char **arguments;
	char *input_file, *output_file;
	int output_type;/* 1 - rewrite, 2 - append; */
}program;

typedef struct 
{
	program *programs;
	int number_of_programs;	
	int background;/* 0 - no; 1 - yes  */
}job;

typedef struct 
{
	job *jobs;
	int number_of_jobs;
}queue;

struct proc_node
{
	pid_t pid;
	char* name;
	char status[8];
	struct proc_node *next;
};

pid_t pid, pgid;
pid_t Wait;
int stat;
struct proc_node *proc_list = NULL;
int proc_num = 0;
int last_status = 0;
job currjob;

char MyGetChar(char* str)
{
	static int i = 0;
	if(str == NULL)
	{
		i = 0;
		return getchar();
	}
	return str[i++];
}

void FreeProgram(program prog)
{
	int i;
	if(prog.name != NULL) free(prog.name);
	if(prog.input_file != NULL) free(prog.input_file);
	if(prog.output_file != NULL) free(prog.output_file);
	if(prog.arguments != NULL)
	{
		for(i = 1; i < prog.number_of_arguments; i++) free(prog.arguments[i]);
		free(prog.arguments);
	}
}

void FreeJob(job jobb)
{
	int i;
	if(jobb.programs != NULL)
	{
		for(i = 0; i < jobb.number_of_programs; i++) FreeProgram(jobb.programs[i]);
		free(jobb.programs);
	}

}

void FreeQueue(queue que)
{
	int i;
	if(que.jobs != NULL)
	{
		for(i = 0; i < que.number_of_jobs; i++) FreeJob(que.jobs[i]);
		free(que.jobs);
	}

}

program DefineProgram()
{
	program newProgram;
	newProgram.name = NULL;
	newProgram.number_of_arguments = 0;
	newProgram.arguments = NULL;
	newProgram.input_file = NULL;
	newProgram.output_file = NULL;
	newProgram.output_type = 0;
	return newProgram;
}

char* Realloc(char *str, char symbol, int i)
{
	char* tempstr = NULL;
	if((tempstr = realloc(str, (i + 1) * sizeof(char))) == NULL)
	{
		free(str);
		printf("Failed to allocate memomory for string argument\n");
		return NULL;
	}
	tempstr[i - 1] = symbol;
	tempstr[i] = '\0';
	return tempstr;
}

void UnexpectedSymbol(int c)
{
	printf("Unexpected token: ");
	if(c == '\n') printf("new line");
	else if(c == '@') printf(">>");
	else putchar(c);
	putchar('\n');
}

char EscapeSlash(int symbol)
{
	switch(symbol)
	{
		case '\n':
		return 0;
		default:
		return symbol;	
	}
}

char* EscapeCurlyBraces(char *str, int *param, int *flag)
{
	int symbol, i = 0;
	if(str != NULL) i = strlen(str) - 2;
	while((symbol = getchar()) != '}')
	{
		i++;
		if (symbol == '\\')
		{
			if((symbol = getchar()) == EOF)
			{
				printf("You can not escape EOF\n");
				*param = 0;
				free(str);
				return NULL;
			}
			symbol = EscapeSlash(symbol);
		}
		if((str = Realloc(str, symbol, i)) == NULL || symbol == EOF)
		{
			*param = 0;
			if(symbol == EOF) printf("You did not close curly brace\n");
			else *flag = -1;
			return NULL;
		}
	}
	return str;
}

char* EscapeStrongQuotes(char *str, int *param, int *flag)
{
	int symbol, i = 0;
	if(str != NULL) i = strlen(str) - 2;
	while((symbol = getchar()) != '\'')
	{
		i++;
		if((str = Realloc(str, symbol, i)) == NULL || symbol == EOF)
		{
			*param = 0;
			if(symbol == EOF) printf("You did not close strong quote\n");
			else *flag = -1;
			return NULL;
		}
	}
	return str;
}

program MakeMeProgram(program prog, char* argument, int *symbol, int *flag)
{
	char **temp_arguments = NULL;
	if(argument == NULL) 
	{
		UnexpectedSymbol(*symbol);
		*symbol = 0;
		return prog;
	}
	if(*symbol == '@')/* >> */
	{
		if(prog.output_type != 0)
		{
			UnexpectedSymbol(*symbol);
			free(argument);
			*symbol = 0;
			return prog;
		}
		prog.output_type = 2;
		prog.output_file = argument;
		return prog;
	}
	if(*symbol == '>')
	{
		if(prog.output_type != 0)
		{
			UnexpectedSymbol(*symbol);
			free(argument);
			*symbol = 0;
			return prog;
		}
		prog.output_type = 1;
		prog.output_file = argument;
		return prog;
	}
	if(*symbol == '<')
	{
		if(prog.input_file != NULL)
		{
			UnexpectedSymbol(*symbol);
			free(argument);
			*symbol = 0;
			return prog;
		}
		prog.input_file = argument;
		return prog;
	}
	if(*symbol == ' ' || *symbol =='|')
	{	
		if(*symbol == '|' && argument == NULL)
		{
			free(argument);
			UnexpectedSymbol(*symbol);
			free(argument);
			*symbol = 0;
			return prog;
		}
		if(prog.name == NULL) prog.name = argument;		
		if((temp_arguments = realloc(prog.arguments, sizeof(char*) * (prog.number_of_arguments + 2))) == NULL)
		{
			free(argument);
			printf("Failed to allocate memory for arguments\n");
			*symbol = 0;
			*flag = -1;
			return prog;
		}
		prog.arguments = temp_arguments;
		prog.arguments[prog.number_of_arguments] = argument;
		prog.number_of_arguments++;
		prog.arguments[prog.number_of_arguments] = NULL;
		return prog;		
	}
	*symbol = 0;
	return prog;
}

int NumberOfDigits(int x)
{
	int i = 0, j;
	j = x;
	do
	{
		i++;
		j = j / 10;
	}while(j != 0);
	return i;
}
int CheckForNumbers(char *str)
{
	size_t i, len;
    int number = 0;

    len=strlen(str);
	for(i = 0; i < len; i++)
	{
		if('0' <= str[i] &&  str[i] <= '9') number = number * 10 + (str[i] - '0');
		else return -1;
	}
	return number;
}

char* SavedStr(char *dollar, int argc, char **argv, int *symbol, int *flag)/* TO - DO!!! */
{
	char* str = NULL;
	int i = 0, number;
	if((number = CheckForNumbers(dollar)) != -1)
	{
		if(number == 0 || number > argc)
		{
			free(dollar);
			return str;
		}
		str = argv[number];
		if((str = (char*)calloc(strlen(str) + 1, sizeof(char))) == NULL)
		{
			*symbol = 0;
			*flag = -1;
			printf("Failed to allocate string argument\n");
			free(dollar);
			return NULL;
		}
		strcpy(str, argv[number]);
		free(dollar);
		return str;
	}
	if(strcmp(dollar, "#") == 0)
	{
		if((str = (char*)calloc(NumberOfDigits(argc) + 1, sizeof(char))) == NULL)
		{
			*symbol = 0;
			*flag = -1;
			printf("Failed to allocate string argument\n");
			free(dollar);
			return NULL;
		}
		str[NumberOfDigits(argc)] = '\0';
		for(i = NumberOfDigits(argc); i >  0 ; i--) 
		{
			str[i - 1] = (argc % 10) + '0';
			argc = argc / 10;
		}
		free(dollar);
		return str;
	}
	if(strcmp(dollar, "?") == 0)/* TO-DO */
	{
		if((str = (char*)calloc(NumberOfDigits(last_status) + 1, sizeof(char))) == NULL)
		{
			*symbol = 0;
			*flag = -1;
			printf("Failed to allocate string argument\n");
			free(dollar);
			return NULL;
		}
		str[NumberOfDigits(last_status)] = '\0';
		for(i = NumberOfDigits(last_status); i >  0 ; i--) 
		{
			str[i - 1] = (argc % 10) + '0';
			argc = argc / 10;
		}
		free(dollar);
		return str;
	}
	if(strcmp(dollar, "UID") == 0)
	{
		if((str = (char*)calloc(NumberOfDigits(getuid()) + 1, sizeof(char))) == NULL)
		{
			*symbol = 0;			
			*flag = -1;
			printf("Failed to allocate string argument\n");
			free(dollar);
			return NULL;			
		}
		str[NumberOfDigits(getuid())] = '\0';
		argc = getuid();
		for(i = NumberOfDigits(getuid()); i > 0 ; i--) 
		{
			str[i - 1] = (argc % 10) + '0';
			argc = argc / 10;
		}
		free(dollar);
		return str;
	}
	if(strcmp(dollar, "USER") == 0)
	{
		str = getlogin();
		if((str = (char*)calloc(strlen(str) + 1, sizeof(char))) == NULL)
		{
			*symbol = 0;
			*flag = -1;
			printf("Failed to allocate string argument\n");
			free(dollar);
			return NULL;
		}
		strcpy(str, getlogin());
		free(dollar);
		return str;
	}
	if(strcmp(dollar, "HOME") == 0)
	{
		str = getenv("HOME");
		if((str = (char*)calloc(strlen(str) + 1, sizeof(char))) == NULL)
		{
			*symbol = 0;
			*flag = -1;
			printf("Failed to allocate string argument\n");
			free(dollar);
			return NULL;
		}
		strcpy(str, getenv("HOME"));
		free(dollar);
		return str;
	}
	if(strcmp(dollar, "SHELL") == 0)
	{
		str = getenv("SHELL");
		if((str = (char*)calloc(strlen(str) + 1, sizeof(char))) == NULL)
		{
			*symbol = 0;
			*flag = -1;
			printf("Failed to allocate string argument\n");
			free(dollar);
			return NULL;
		}
		strcpy(str, getenv("SHELL"));
		free(dollar);
		return str;
	}
	if(strcmp(dollar, "PWD") == 0)
	{		
		if((str = getcwd(str, 0)) == NULL)
		{
			*symbol = 0;
			*flag = -1;
			printf("Failed to allocate string argument\n");
			free(dollar);
			return NULL;
		}
		free(dollar);
		return str;
	}
	if(strcmp(dollar, "PID") == 0)
	{
		if((str = (char*)calloc(NumberOfDigits(getpid()) + 1, sizeof(char))) == NULL)
		{
			*symbol = 0;			
			*flag = -1;
			printf("Failed to allocate string argument\n");
			free(dollar);
			return NULL;			
		}
		str[NumberOfDigits(getpid())] = '\0';
		argc = getpid();
		for(i = NumberOfDigits(getpid()); i > 0 ; i--) 
		{
			str[i - 1] = (argc % 10) + '0';
			argc = argc / 10;
		}
		free(dollar);
		return str;
	}
	if((str = getenv(dollar)) != NULL)
	{
		if((str = (char*)calloc(strlen(str) + 1, sizeof(char))) == NULL)
		{
			*symbol = 0;
			*flag = -1;
			printf("Failed to allocate string argument\n");
			free(dollar);
			return NULL;
		}
		strcpy(str, getenv(dollar));
		free(dollar);
		return str;
	}
	free(dollar);
	return str;
}

char* Dollar(char* str, int *symbol, int argc, char **argv, int *flag)
{
	char *dollar = NULL, *tempstr;
	int j = 0;
	size_t i = 0;
	
	tempstr=str;
	
	if(str != NULL) 
		i = strlen(str) - 1;

	do
	{
		while(((*symbol = getchar()) != '\n') && (*symbol != ' ') && (*symbol != '\\') && 
				(*symbol != '"') && (*symbol != '\'') && (*symbol != ';') && (*symbol != '$') && 
					(*symbol != '|') && (*symbol != '<') && (*symbol != '>') && (*symbol != '{') &&
						(*symbol != '}') && (*symbol != '!') && (*symbol != EOF))
		{
			j++;
			if((dollar = Realloc(dollar, *symbol, j)) == NULL)
			{
				*flag = -1;
				*symbol = 0;
				free(str);
				return NULL;
			}
		}
		if(dollar == NULL && *symbol == '{') 
		{
			if((dollar = EscapeCurlyBraces(dollar, symbol, flag)) == NULL && *symbol == 0)
			{
				free(str);
				return NULL;
			}
			*symbol = getchar();
		}
		if (dollar == NULL)
		{
			i++;
			if((tempstr = Realloc(tempstr, '$', i)) == NULL)
			{
				*flag = -1;
				*symbol = 0;
				free(dollar);
				return NULL;
			}
		}
		else
		{
			if((dollar = SavedStr(dollar, argc - 1, argv, symbol, flag)) != NULL)
			{
				i += strlen(dollar);
				if(i != strlen(dollar))
				{
					if((tempstr = realloc(str, (i + 1) * sizeof(char))) == NULL)
					{
						printf("Failed to allocate memomory for string argument\n");
						free(str);
						*flag = -1;
						*symbol = 0;
						return NULL;
					}
				}
				else
				{
					if((tempstr = (char*)calloc(i + 1, sizeof(char))) == NULL)
					{
						printf("Failed to allocate memomory for string argument\n");
						*flag = -1;
						*symbol = 0;
						return NULL;
					}				
				}
				tempstr[i] = '\0';
				strcat(tempstr, dollar);	
				str = tempstr;
				free(dollar);
			}
			if(*symbol == 0)
			{
				free(str);
				return NULL;
			}
		}
		dollar = NULL;
		j = 0;
	} while(*symbol == '$');
	return tempstr;
}

char* GetArghs(int *symbol, int argc, char **argv, int *flag)
{
	int i = 0, textmode = 0;
	char* str = NULL;
	while((((*symbol = getchar()) != '\n') && (*symbol != ' ') && (*symbol != ';') && (*symbol != '&') && 
		(*symbol != '>') && (*symbol != '<') && (*symbol != '|') && (*symbol != EOF)) || (textmode == 1))
	{	
		if(*symbol == '$')
		{
			str = Dollar(str, symbol, argc, argv, flag);
			if(*symbol == 0) return NULL;
			if(*symbol == EOF) break;
			if((*symbol == '\n' || *symbol == ' ' || *symbol == ';' || *symbol == '>' || *symbol == '<' || 
				*symbol == '|' || *symbol == EOF) && (textmode == 0)) break;
			if(str != NULL) i = strlen(str);
		}
		if(textmode == 1 && *symbol == EOF)
		{
			printf("You did not close weak quotes\n");
			*symbol = 0;
			free(str);
			return NULL;
		}
		if (*symbol == '\\')
		{
			if((*symbol = getchar()) == EOF)
			{
				printf("You can not escape EOF\n");
				*symbol = 0;
				free(str);
				return NULL;
			}
			*symbol = EscapeSlash(*symbol);
		}else if(*symbol == '\'' && textmode == 0)
		{
			str = EscapeStrongQuotes(str, symbol, flag);
			if(*symbol == 0) return NULL;
			*symbol = 0;
			if (i != 0) i = strlen(str) - 1;			
		}
		else if(*symbol == '"')
		{			
			if(textmode == 0) textmode = 1;
			else textmode = 0;
			*symbol = 0;
		}else if(*symbol == '#' && textmode == 0)
		{
			while((*symbol = getchar()) != '\n' || *symbol == EOF);
			break;
		}
		if(*symbol != 0)
		{
			i++;
			if((str = Realloc(str, *symbol, i)) == NULL)
			{
				*flag = -1;
				*symbol = 0;
				return NULL; 
			}
		}
	}
	return str;
}

void del_procs()
{
	struct proc_node *temp_proc;

	while(proc_list != NULL)
	{
		temp_proc = proc_list;
		proc_list = proc_list -> next;
		free(temp_proc -> name);
		free(temp_proc);
	}
	proc_num = 0;
}

void print_procs()
{
	struct proc_node *curr_proc = proc_list;
	int i = 1;
	while(curr_proc != NULL)
	{
		printf("[%d]\t%s\t%s\n", i++, curr_proc -> status, curr_proc -> name);
		curr_proc = curr_proc -> next;
	}
}

void print_proc (int number)
{
	struct proc_node *curr_proc = proc_list;
	int i = 1;
	for (i = 1; i < number && curr_proc != NULL; i++)
		curr_proc = curr_proc -> next;
	
	if (curr_proc == NULL)
	{
		printf("There is no such job\n");
		return;
	}

	printf("[%d]\t%s\t%s\n", number, curr_proc -> status, curr_proc -> name);
}

pid_t proc_pid(int number)
{
	struct proc_node *curr_proc = proc_list;
	int i = 1;
	for(i = 1; i < number && curr_proc != NULL; i++)
		curr_proc = curr_proc -> next;

	if (curr_proc == NULL)
	{
		printf("There is no such job\n");
		return -2;
	}

	return curr_proc -> pid;
}


char *proc_name(int number)
{
	struct proc_node *curr_proc = proc_list;
	int i = 1;
	for (i = 1; i < number && curr_proc != NULL; i++)
		curr_proc = curr_proc -> next;

	if (curr_proc == NULL)
	{
		printf("Theres is no such job\n");
		return NULL;
	}

	return curr_proc -> name;
}

int find_proc (pid_t tpid)
{
	struct proc_node *curr_proc = proc_list;
	int i;
	for (i = 1; i < proc_num && curr_proc -> pid != tpid; i++)
		curr_proc = curr_proc -> next;

	if (curr_proc -> pid != tpid)
	{
		printf("There is no such job\n");
		return 0;
	}
	return i;
}

void proc_stat (int number, int arg)
{
	struct proc_node *curr_proc = proc_list;
	int i = 1;
	for (i = 1; i < number && curr_proc != NULL; i++)
		curr_proc = curr_proc -> next;

	if (curr_proc == NULL)
	{
		printf("There is no such job\n");
		return;
	}
	if (arg > 0)
		strcpy(curr_proc -> status, "Stopped");
	else
		strcpy(curr_proc -> status, "Running");
}

void wait_in_bg()
{
	int tcjob;
	if (tcgetpgrp(0) != getpgrp())
	{
		tcjob = find_proc(tcgetpgrp(0));
		while (waitpid(-proc_pid(tcjob), &stat, WUNTRACED) != -1)
		{
			if(WIFEXITED(stat))
			last_status = WEXITSTATUS(stat);
			if(WIFSTOPPED(stat))
			{
				proc_stat(tcjob, 1);
				tcsetpgrp(0, getpgrp());
				return;
			}
		}
		del_proc(tcjob);
		tcsetpgrp(0, getpgrp());
	}
}

void check_bg ()
{
	pid_t curr_pid;
	int i = 0;
	for (i = 1; i <= proc_num; i++)
	{
		while((curr_pid = waitpid(-proc_pid(i), &stat, WNOHANG | WUNTRACED | WCONTINUED)) != 0)
		{
			if (curr_pid == -1)
			{
				if (tcgetpgrp(0) == proc_pid(i))
					tcsetpgrp(0, getpgrp());
				del_proc(i--);
				break;
			}	
	
			if (WIFSTOPPED(stat))
			{
				if (tcgetpgrp(0) == getpgid(curr_pid))
					tcsetpgrp(0, getpgrp());
				proc_stat(i, 1);
			}
			if(WIFCONTINUED(stat))
				proc_stat(i, 0);
		}
	}
}

int add_proc(pid_t pid, char *name)
{
	struct proc_node *curr_proc = NULL;
	
	if (proc_list == NULL)
	{
		proc_list = (struct proc_node*)malloc(sizeof(struct proc_node));
		if (proc_list == NULL)
		{
			printf("Can`t allocate memory\n");
			return -1;
		}
	

	proc_list -> name = (char*)malloc((strlen(name) + 1)*sizeof(char));
	if (proc_list -> name == NULL)
	{
		printf("Can`t allocate memory\n");
		free(proc_list);
		proc_list = NULL;
		return -1;
	}

	strcpy(proc_list -> name, name);
	proc_list -> pid = pid;
	strcpy(proc_list -> status, "Running");
	proc_list -> next = NULL;
	proc_num++;
	return 0;
	}

	curr_proc = proc_list;
	
	while(curr_proc -> next != NULL)
		curr_proc = curr_proc -> next;

	curr_proc -> next = (struct proc_node*)malloc(sizeof(struct proc_node));
	if (curr_proc -> next == NULL)
	{
		printf("Can`t allocate memory\n");
		return -1;
	}

	curr_proc -> next -> name = (char*)malloc((strlen(name) + 1)*sizeof(char));
	if (curr_proc -> next -> name == NULL)
	{
		printf("Can`t allocate memory\n");
		free(curr_proc -> next);
		curr_proc -> next = NULL;
		return -1;
	}

	strcpy((curr_proc -> next) -> name, name);
	(curr_proc -> next) -> next = NULL;
	strcpy(curr_proc -> next -> status, "Running");
	(curr_proc -> next) -> pid = pid;
	proc_num++;
	return 0;
}

int PrintPrefix()
{
	char* str = NULL;
	if((str = getcwd(str, 0)) == NULL)
	{
		printf("Failed to allocate string argument\n");
		return -1;
	}
	printf("%s@%s:%s$ ", getlogin(), getenv("SHELL"), str);
	free(str);
	return 0;
}

void fg()
{
	int number;
	char *str_ptr;

	if (currjob.programs[0].number_of_arguments != 2)
	{
		printf("fg: 1 incorrect input\n");
		return;
	}
	number = (int)strtol(currjob.programs[0].arguments[1], &str_ptr, 10);
	if ((str_ptr[0] != '\0') || (number <= 0))
	{
		printf("fg: 2 incorrect input \n");
		return;
	}

	if (number > proc_num)
	{
		printf("fg: no such job\n");
		return;
	}

	proc_stat(number,0);
	print_proc(number);
	tcsetpgrp(0, proc_pid(number));
	killpg(proc_pid(number), SIGCONT);
	wait_in_bg();
}

void bg()
{
	int number;
	char *str_ptr;

	if (currjob.programs[0].number_of_arguments != 2)
	{
		printf("bg: incorrect input\n");
		return;
	}

	number = (int)strtol(currjob.programs[0].arguments[1], &str_ptr, 10);
	if ((str_ptr[0] != '\0') || (number <= 0))
	{
		printf("bg: incorrect input \n");
		return;
	}   

	if (number > proc_num)
	{
		printf("bg: no such job\n");
		return;
	}

	proc_stat(number, 0);
	print_proc(number);
	killpg(proc_pid(number), SIGCONT);
}

int del_proc (int number)
{
	struct proc_node *curr_proc = proc_list;
	struct proc_node *temp_proc = NULL;
	int i;

	for(i = 1; i < (number - 1) && curr_proc != NULL; i++)
		curr_proc = curr_proc -> next;

	if (curr_proc == NULL)
	{
		printf("There is no such job\n");
		return -2;
	}
	
	if (number == 1)
	{
		temp_proc = proc_list -> next;
		free(proc_list -> name);
		free(proc_list);
		proc_list = temp_proc;
		proc_num--;
		return 0;
	}

	if (curr_proc -> next == NULL)
	{
		printf("There is no such job\n");
		return -2;
	}

	temp_proc = curr_proc -> next -> next;
	free(curr_proc -> next -> name);
	free(curr_proc -> next);
	curr_proc -> next = temp_proc;
	proc_num--;
	return 0;
}

int ExecuteJob()
{
	
	int oldfd[2], newfd[2], fd[2]; /* for pipes */
	int i = 0;
	char *tmp_string;
	signal(SIGTTOU, SIG_IGN);
	if(currjob.number_of_programs == 0)
		return 0;
	
	for (i = 0; i < currjob.number_of_programs; i++)
	{ 
		if (currjob.number_of_programs != 1)
			pipe(newfd); 

		if (currjob.programs[i].input_file)
		{
			fd[0] = open(currjob.programs[i].input_file, O_RDONLY, 0666);
			if (fd[0] < 0)
			{
				perror(currjob.programs[i].input_file);
				return -1;
			}
		}		 
	
		if (currjob.programs[i].output_file)
		{
			if (currjob.programs[i].output_type == 1)
				fd[1] = open(currjob.programs[i].output_file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
			else
				fd[1] = open(currjob.programs[i].output_file, O_CREAT | O_WRONLY | O_APPEND, 0666);

			if (fd[1] < 0)
			{
				perror(currjob.programs[i].output_file);
				return -1;
			}
		}

		pid = fork(); 
		
		if (pid == -1)
		{
			perror("fork");
			return -1;
		}	
			
		if (i == 0)
			pgid = pid;

		setpgid(pid, pgid);
		
		if (i == 0 && currjob.background == 0)
			tcsetpgrp(0, pgid);			

		if (pid == 0)
		{
			signal(SIGTTOU, SIG_DFL);

			if (i > 0)
			{
				dup2(oldfd[0], 0); 
				close(oldfd[0]);
				close(oldfd[1]);
			}
				
			if (currjob.programs[i].input_file != NULL)
			{
				dup2(fd[0], 0); 
				close(fd[0]);
			}
				
			if (i != currjob.number_of_programs - 1)
			{
				dup2(newfd[1], 1);
				close(newfd[0]);
				close(newfd[1]);
			}
			
			if (currjob.programs[i].output_file != NULL)
			{
				dup2(fd[1],1);
				close(fd[1]);
			}
			if (strcmp(currjob.programs[i].name, "pwd") == 0)
			{	
				printf("%s\n", tmp_string = getcwd(NULL, 0));
				free(tmp_string);
				exit(0);
			}
			else
			{
				execvp(currjob.programs[i].name, currjob.programs[i].arguments);
				perror (currjob.programs[i].name);
				exit(0);
			}
				
		}
			
		if (pid > 0)
		{	
			if (i > 0)
			{
				close(oldfd[0]);
				close(oldfd[1]);
			}
		
			if (currjob.number_of_programs > 1)		
			{
				oldfd[0] = newfd[0];
				oldfd[1] = newfd[1];
			}
				
			if (currjob.programs[i].input_file != NULL)
				close(fd[0]);
			if (currjob.programs[i].output_file != NULL)
				close(fd[1]);
		}
	}
	add_proc (pgid, currjob.programs[0].name);
	wait_in_bg();
	print_procs();	
	return 0;
}

void CommandCenter(queue que)
{
	int i;
	if(que.number_of_jobs == 0) return;
	check_bg();
	for(i = 0; i < que.number_of_jobs; i++)
	{

		check_bg();
		if(que.jobs[i].programs != NULL)
		if (que.jobs[i].number_of_programs == 1) 
		{
			if (strcmp(que.jobs[i].programs[0].name, "exit") == 0)
			{
				del_procs();
				FreeQueue(que);
				exit(0);
				break;
			}
		}
		if(que.jobs[i].programs != NULL)
		{
			currjob = que.jobs[i];
			if (que.jobs[i].number_of_programs == 1)
			{
				if (strcmp(que.jobs[i].programs[0].name, "cd") == 0)
				{
					if (que.jobs[i].programs[0].number_of_arguments == 2)
					{
						if (strcmp(que.jobs[i].programs[0].arguments[1], "~") == 0)
						{
							if (chdir((getenv("HOME"))) != 0)
							printf("cd: %s: No such file or directory\n", que.jobs[i].programs[0].arguments[1]);
						}
						else
						if (chdir(que.jobs[i].programs[0].arguments[1]) != 0)
							printf("cd: %s: No such file or directory\n", que.jobs[i].programs[0].arguments[1]);
					}
					else
					printf("cd: (null):  No such file or directory\n");
						continue;
				}
		
				if (strcmp(que.jobs[i].programs[0].name, "jobs") == 0)
				{
					print_procs();
					continue;
				}			
				if (strcmp(que.jobs[i].programs[0].name, "fg") == 0)
				{
					fg();
					continue;
				}	
		
				if (strcmp(que.jobs[i].programs[0].name, "bg") == 0)
				{
					bg();
					continue;
				}
			}
		}		
		ExecuteJob();
	}
	FreeQueue(que);
}

queue Input(int argc, char **argv)
{
	
	int symbol = 0, prevsymbol = ' ', flag = 0, conveyor = 0;
	program* tempprograms = NULL;
	char *str = NULL;
	job* tempjobs = NULL;
	program prog = DefineProgram();
	queue que;
	que.jobs = NULL;
	que.number_of_jobs = 0;
	/*char *histin = NULL;*/

	if(PrintPrefix() == -1)
	{			
		exit(1);
	}
	do
	{
		str = GetArghs(&symbol, argc, argv, &flag);
		if(flag == -1)
		{
			
			if(conveyor == 1)que.number_of_jobs++;
			FreeProgram(prog);
			FreeQueue(que);
			exit(1);
		}

		if(symbol == 0 || (symbol == '|' && prevsymbol == '|' && str == NULL))
		{
			if(conveyor == 1)que.number_of_jobs++;
			FreeProgram(prog);
			FreeQueue(que);
			UnexpectedSymbol(symbol);
			que.number_of_jobs = 0;
			return que;
		}
		if(str == NULL && prevsymbol == '>' && symbol == '>') prevsymbol = '@';
		if(str == NULL && symbol != '&' && symbol != '\n' && symbol != ';' && symbol != '|')
		{
			if(prevsymbol == ' ' && symbol != ' ' && flag == 0)
			{
				prevsymbol = symbol;
				flag = 1;
				continue;
			}
			continue;			
		}
		if(prevsymbol == ' ' && str != NULL)
		{
			prog = MakeMeProgram(prog, str, &prevsymbol, &flag);
			/* if(flag == -1)exit; */
			if(prevsymbol == 0)
			{
				if(conveyor == 1)que.number_of_jobs++;
				FreeProgram(prog);
				FreeQueue(que);
				free(str);				
				if(flag == -1)
				{
					
					exit(1);
				}
				que.number_of_jobs = 0;
				return que;
			}
		}
		if(prevsymbol == '>')
		{
			flag = 0;
			if(prog.name == NULL)
			{
				if(conveyor == 1)que.number_of_jobs++;
				FreeProgram(prog);
				FreeQueue(que);
				UnexpectedSymbol(prevsymbol);
				free(str);
				que.number_of_jobs = 0;
				return que;
			}
			prog = MakeMeProgram(prog, str, &prevsymbol, &flag);
			if(prevsymbol == 0)
			{
				if(conveyor == 1)que.number_of_jobs++;
				FreeProgram(prog);
				FreeQueue(que);
				free(str);
				if(flag == -1)
				{
					
					exit(1);
				}
				que.number_of_jobs = 0;
				return que;
			}
		}
		if(prevsymbol == '@')
		{
			flag = 0;
			if(prog.name == NULL)
			{
				if(conveyor == 1)que.number_of_jobs++;
				FreeProgram(prog);
				FreeQueue(que);
				UnexpectedSymbol(symbol);
				free(str);
				if(flag == -1)
				{
					
					exit(1);
				}
				que.number_of_jobs = 0;
				return que;
			}
			prog = MakeMeProgram(prog, str, &prevsymbol, &flag);
			if(prevsymbol == 0)
			{
				if(conveyor == 1)que.number_of_jobs++;
				FreeProgram(prog);
				FreeQueue(que);
				if(flag == -1)
				{
					
					exit(1);
				}
				que.number_of_jobs = 0;
				return que;
			}
		}
		if(prevsymbol == '<')
		{
			flag = 0;
			if(prog.name == NULL)
			{
				if(conveyor == 1)que.number_of_jobs++;
				FreeProgram(prog);
				FreeQueue(que);
				UnexpectedSymbol(symbol);
				free(str);
				que.number_of_jobs = 0;
				return que;
			}
			prog = MakeMeProgram(prog, str, &prevsymbol, &flag);
			if(prevsymbol == 0)
			{
				if(conveyor == 1)que.number_of_jobs++;
				FreeProgram(prog);
				FreeQueue(que);
				if(flag == -1)
				{
					
					exit(1);
				}
				que.number_of_jobs = 0;
				return que;
			}			
		}
		if(prevsymbol == '|')
		{
			if(str == NULL && prog.name == NULL)
			{
				if(conveyor == 1)que.number_of_jobs++;
				FreeProgram(prog);
				FreeQueue(que);
				UnexpectedSymbol(symbol);
				free(str);
				que.number_of_jobs = 0;
				return que;
			}
			prog = MakeMeProgram(prog, str, &prevsymbol, &flag);
			if(prevsymbol == 0)
			{
				if(conveyor == 1)que.number_of_jobs++;
				FreeProgram(prog);
				FreeQueue(que);
				if(flag == -1)
				{
					
					exit(1);
				}
				que.number_of_jobs = 0;
				return que;
			}				
		}
		if(symbol == '|')
		{
			flag = 0;
			if(prog.name == NULL)
			{
				if(conveyor == 1)que.number_of_jobs++;
				FreeProgram(prog);
				FreeQueue(que);
				UnexpectedSymbol(symbol);
				free(str);
				que.number_of_jobs = 0;
				return que;
			}
			if(conveyor == 0)
			{
				if((tempjobs = realloc(que.jobs, sizeof(job) * (que.number_of_jobs + 1))) == NULL)
				{
					
					FreeProgram(prog);
					FreeQueue(que);
					printf("Failed to allocate memomory for program\n");
					/* exit */
					return que;
				}						
				que.jobs = tempjobs;
				que.jobs[que.number_of_jobs].programs = NULL;
				que.jobs[que.number_of_jobs].number_of_programs = 0;
				que.jobs[que.number_of_jobs].background = 0;
				conveyor = 1;
			}			
			if((tempprograms = realloc(que.jobs[que.number_of_jobs].programs, sizeof(program) * (que.jobs[que.number_of_jobs].number_of_programs + 1))) == NULL)
			{
				
				if(conveyor == 1)que.number_of_jobs++;
				FreeProgram(prog);
				FreeQueue(que);
				printf("Failed to allocate memomory for program\n");
				exit(1);
			}
			que.jobs[que.number_of_jobs].programs = tempprograms;
			que.jobs[que.number_of_jobs].programs[que.jobs[que.number_of_jobs].number_of_programs] = prog;			
			que.jobs[que.number_of_jobs].number_of_programs++;
			prog = DefineProgram();
		}
		if(symbol == '&')
		{
			if(prog.name == NULL)
			{
				if(conveyor == 1)que.number_of_jobs++;
				FreeProgram(prog);
				FreeQueue(que);
				UnexpectedSymbol(symbol);
				free(str);
				que.number_of_jobs = 0;
				return que;
			}
			else 
			{	
				if(conveyor == 0)
				{
					if((tempjobs = realloc(que.jobs, sizeof(job) * (que.number_of_jobs + 1))) == NULL)
					{
						if(conveyor == 1)que.number_of_jobs++;
						FreeProgram(prog);
						FreeQueue(que);
						printf("Failed to allocate memomory for program\n");
						exit(1);
					}						
					que.jobs = tempjobs;
					que.jobs[que.number_of_jobs].programs = NULL;
					que.jobs[que.number_of_jobs].number_of_programs = 0;
				}
				que.jobs[que.number_of_jobs].background = 1;
				if((tempprograms = realloc(que.jobs[que.number_of_jobs].programs, sizeof(program) * (que.jobs[que.number_of_jobs].number_of_programs + 1))) == NULL)
				{
									
					if(conveyor == 1)que.number_of_jobs++;
					FreeProgram(prog);
					FreeQueue(que);
					printf("Failed to allocate memomory for program\n");
					exit(1);
				}
				que.jobs[que.number_of_jobs].programs = tempprograms;
				que.jobs[que.number_of_jobs].programs[que.jobs[que.number_of_jobs].number_of_programs] = prog;
				que.jobs[que.number_of_jobs].number_of_programs++;
				que.number_of_jobs++;
				prog = DefineProgram();
				prevsymbol = ' ';
				symbol = ' ';
				conveyor = 0;
			}
		}
		if(symbol == ';' || symbol == '\n' || symbol == EOF)
		{
			if(prog.name != NULL)
			{
				if(conveyor == 0)
				{
					if((tempjobs = realloc(que.jobs, sizeof(job) * (que.number_of_jobs + 1))) == NULL)
					{
						
						FreeProgram(prog);
						FreeQueue(que);
						printf("Failed to allocate memomory for program\n");
						exit(1);
					}						
					que.jobs = tempjobs;
					que.jobs[que.number_of_jobs].programs = NULL;				
					que.jobs[que.number_of_jobs].number_of_programs = 0;
				}
				que.jobs[que.number_of_jobs].background = 0;
				if((tempprograms = realloc(que.jobs[que.number_of_jobs].programs, sizeof( program) * (que.jobs[que.number_of_jobs].number_of_programs + 1))) == NULL)
				{
					
					FreeProgram(prog);
					FreeQueue(que);
					printf("Failed to allocate memomory for program\n");
					exit(1);
				}
				que.jobs[que.number_of_jobs].programs = tempprograms;
				que.jobs[que.number_of_jobs].programs[que.jobs[que.number_of_jobs].number_of_programs] = prog;
				que.jobs[que.number_of_jobs].number_of_programs++;
				que.number_of_jobs++;
				prog = DefineProgram();
			}
			conveyor = 0;
			prevsymbol = ' ';
			if(symbol == ';')symbol = ' ';
		}
		/* if(flag == -1)exit;  */
		str = NULL;
		if(flag == 0)prevsymbol = symbol;		
	}while(symbol != '\n' && symbol != EOF);	
	if(que.number_of_jobs == 0 && symbol == EOF) 
	{
		del_procs();
		putchar('\n');
		exit(0);
	}
	if(symbol == EOF) putchar('\n');
	return que;
}

void Debug(queue que)
{
	int i, j, k;
	job tempjob;
	program tempprog;
	if(que.number_of_jobs > 0) printf("Number of jobs: %d\n", que.number_of_jobs);
	else return;
	for(i = 0; i < que.number_of_jobs; i++)
	{	
		putchar('\n');
		tempjob = que.jobs[i];					
		if(tempjob.background == 1) printf("Do in the background:\n");
		for(j = 0; j < tempjob.number_of_programs; j++)
		{
			tempprog = tempjob.programs[j];
			printf("program: %s\n", tempprog.name);
			printf("with %d arguments ", tempprog.number_of_arguments);
			for(k = 0; k < tempprog.number_of_arguments; k++)
			{
				printf("%s ", tempprog.arguments[k]);
			}			
			putchar('\n');
			if(tempprog.input_file != NULL) printf("input file: %s\n", tempprog.input_file);
			if(tempprog.output_file != NULL) printf("output file: %s with mode: %d\n", tempprog.output_file, tempprog.output_type);
			if(tempjob.number_of_programs - j > 1) printf("Conveyor to:");
		}		
	}
	FreeQueue(que);
}

int main(int argc, char *argv[])
{
	setenv("SHELL", "llesh", 1);
	while(1)CommandCenter(Input(argc, argv));
    return 0;	
}
