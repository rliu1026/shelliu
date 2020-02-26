#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>

char error_message[30] = "An error has occurred\n";

// This method checks if the argument is a string full of space.
int checkspace(char *sub) {
	int i;
	for (i=0; i<(int)strlen(sub); i++) 
	{
		if (isspace(sub[i]) == 0)
			return 1;
	}
	return 0;
}

// This method handles built-in command "exit".
void call_exit(int *running) 
{
	*running = 0;
	return;
}

// This method handles built-in command "cd".
void call_cd(char **input, int *count_arg)
{
	if (*count_arg == 0 || *count_arg > 2)
	{
		write(STDERR_FILENO, error_message, strlen(error_message)); 
		return;
	}	
	int isChanged = chdir(input[1]); // changing directory
	if (isChanged == -1)
		write(STDERR_FILENO, error_message, strlen(error_message)); 
	return;
}

// This method handles built-in command "path".
void call_path(char **path, char **input, int *count_path, int *count_arg)
{
	// Add path:
	if (strcmp(input[1], "add") == 0) 
	{
		char *path_da = (char*)malloc(sizeof(char) * strlen(input[2]));
		strcpy(path_da, input[2]);

		// Shifting all path pointers on heap 1 index backward, 
		// leaving index 0 for the new path added:
		if (*count_path > 0) 
		{
			int i;
			for (i=*count_path; i>0; i--) // new ptr path[*count_path] created
			{
				// Every pointer points to address the prior pointer does
				path[i] = path[i-1];
				path[i-1] = NULL;
			}
		}
		
		// The first pointer pointing to NULL now points to the new path:
		path[0] = path_da;
		*count_path += 1;	
	}

	// Remove path:	
	else if (strcmp(input[1], "remove") == 0)
	{
		int i,j; 
		int found = 0;
		if (*count_path > 0) 
		{
			for (i=0; i<*count_path; i++) 
			{
				if (strcmp(path[i], input[2]) == 0) 
				{
					//printf("found %s and %s\n", path[i], input[2]);
					found = 1;
					// Shifting each pointer to point to the next one:
					for (j=i;j<*count_path-1; j++) 
						path[j] = path[j+1];
					path[*count_path - 1] = NULL;
					*count_path -= 1;
					
					break;
				}
			}
		}
		if (found == 0)	
			write(STDERR_FILENO, error_message, strlen(error_message)); 
	}
	
	// Clear all paths:
	else if (strcmp(input[1], "clear") == 0)
	{
		/*
		if (*count_path > 0) {
		int i;
		for (i=0; i<*count_path; i++) 
			free(path[i]);
		}*/
		free(path);
		*count_path = 0;
	}

	else 
	{
		write(STDERR_FILENO, error_message, strlen(error_message)); 
		//return NULL;	
	}	

	/*
	for (int j=0; j<*count_path; j++) 
		printf("path %d = %s\n", j, path[j]);
	*/
	return;
}

// This method calls all other commands non-built-in.
void cmd(char **path, char **input, int *count_path, int *count_arg, char *redir) 
{
	int i, pathFound = 0;
	char path_buff[512];	
	
	// Concatenate each path and the name of the command to check accessibility:
	for (i=0; i<*count_path; i++) 
	{

	    strcpy(path_buff, path[i]);
		strcat(path_buff, "/");
		strcat(path_buff, input[0]); 
		
		if (access(path_buff, X_OK) == 0) 
		{
			// Correct path found, will use this complete path_buff to run command.
			pathFound = 1;
			break;
		}
	}
		
	if (pathFound == 0) 
	{
		//printf("Nothing found in these paths\n");
		write(STDERR_FILENO, error_message, strlen(error_message)); 
		return;
	}

	// Prepare for output redirection:
	if (redir != NULL)
	{
		FILE *dest = fopen(redir, "w");
		if (dest == NULL) 
		{
			write(STDERR_FILENO, error_message, strlen(error_message)); 
			exit(1);
		}
		int fd_dest = fileno(dest); 
			// the file descritor number dest is at
		dup2(fd_dest, 1); 
		dup2(fd_dest, 2);
			// all output to fd is now redirected to fd_dest
		//printf("Welcome to the new output dest\n\n");
	}
	
	// Add null at end of input:
	input[*count_arg] = (char*)malloc(sizeof(char));
	input[*count_arg] = '\0';
	*count_arg += 1;
	
	// Run command:
	int exec_rc = execv(path_buff, input);
	
	// Error if execute this part due to fork:
	write(STDERR_FILENO, error_message, strlen(error_message)); 
	//printf("This should not be printed: exec_rc = %d\n", exec_rc);
	
}

// This method parse the command line partially, 
// checking for redirection and seperate line by space:
int prepare(char *raw_input, char **input, int *count_arg, char **redir) 
{

	// Check for redirection:
	int i = 0;
	int count_redir = 0;
	char *raw_redir = NULL;
	//redir = (char*)malloc(sizeof(char));
	*redir = NULL; 

	// Iterate through every character of the line to find '>':
	while (raw_input[i] != '\0')
	{
		if (raw_input[i] == '>') 
		{
			count_redir++;
			if (count_redir > 1) 
			{	
				write(STDERR_FILENO, error_message, strlen(error_message));
				break;
			}
			raw_redir = raw_input;
			raw_input = strsep(&raw_redir, ">");	
				// raw_redir now points to the part 
				// of the string after '>' (space trimmed), 
				// which is the output file name
			
			//printf("raw_input = %s\n", raw_input);
			//printf("raw_redir = %s\n", raw_redir);
					
			if (checkspace(raw_input) == 0)
				return 0; // Syntax error: no command entered
			if (checkspace(raw_redir) == 0)
				return 0; // Syntax error: no output file entered
			

			// Parsing redirected destination:
			*redir = strsep(&raw_redir, " ");
			while (*redir != NULL) 
			{
				//if (strlen(*redir) > 0 || strcmp(*redir, " ") == 1)
				if (checkspace(*redir) == 1)
					break;
				*redir = strsep(&raw_redir, " ");
			}
			// No destination file provided:
			//if (strlen(*redir) == 0 || strcmp(*redir, " ") == 0)
			
			// Delete the \n at the end:
			if (*redir != NULL)
				*redir = strsep(redir, "\n");
			
			if ((*redir != NULL) && (*redir[0] == '\0')) 
			{
				*redir = NULL;
				write(STDERR_FILENO, error_message, strlen(error_message)); 
			}
		} 
		i++;
	}
	// If there is no > in command line, redir is still NULL by now
	
	// Parsing:
	raw_input = strsep(&raw_input, "\n"); // delete \n at the end
	char *seg = strsep(&raw_input, " "); // first token from input
	*count_arg = 0; // token counter
	// Tokenize the entire input line:
	while(seg != NULL) 
	{
		//if (strlen(seg) > 0 || strcmp(seg, " ") == 1) {
		if (checkspace(seg) == 1)
		{
			//isEmpty = 0; // No longer empty
			//input[*count_arg] = (char*)malloc(sizeof(char));
			input[*count_arg] = seg;
			*count_arg += 1;
		}
		seg = strsep(&raw_input, " ");
	}

	return 1;
}


void main(int argc, char** argv) 
{
	// Allocate the first path:
	int count_path = 1;
	char **path = (char**)malloc(sizeof(char*) * 512);
	//path[0] = (char*)malloc(sizeof(char));
	char *path_bin = (char*)malloc(sizeof(char) * 5);
	strcpy(path_bin, "/bin");
	path[0] = path_bin;
	
	int isBatch = 0; // 1 if in batch mode
	FILE *src = stdin; // interative mode in default
	
	// Handling batch mode:
	if (argc == 2) 
	{
		isBatch = 1;
		FILE *batch = fopen(argv[1], "r");
		if (batch == NULL) 
		{
			write(STDERR_FILENO, error_message, strlen(error_message)); 
			exit(1);
		}
		src = batch; // Change src from stdin to file
	} 
	else if (argc > 2)
	{	
		write(STDERR_FILENO, error_message, strlen(error_message)); 
		exit(1);
	}
		
	// Iterate to accept commands
	int running = 1;
	while(running) {
		
		int j = 0;
		char *raw_input = NULL; // (char*)malloc(sizeof(char)); 
			// input as a whole line with \n
		size_t raw_size;
		char **input = (char**)malloc(sizeof(char*) * 512); // parsed input

		// Prompt user:
		if (isBatch == 0)
		{
			printf("shelliu> ");
			fflush(stdout);
		}
		
		getline(&raw_input, &raw_size, src);				
		char *raw_backup = raw_input; // for freeing
	
		// Check for multiple commands:	
		char **cmd_list = (char**)malloc(sizeof(char*) * 512);
		char *raw_multi; // buffer for each seperate command
		int count_cmd = 0;
		
		while (raw_input[j] != '\0')	
		{
			if (raw_input[j] == ';') 
			{
				//count_cmd = 0;
				raw_multi = raw_input;
				char *cmd_sub = strsep(&raw_multi, ";");
				while (cmd_sub != NULL) {
					if (checkspace(cmd_sub) == 1)
					//if (strlen(cmd_sub) > 0 || strcmp(cmd_sub, " ") == 1) 
					{
						//cmd_list[count_cmd] = (char*)malloc(sizeof(char));
						cmd_list[count_cmd] = cmd_sub;
						count_cmd++;
					}
					cmd_sub = strsep(&raw_multi, ";");
				}
			}
			j++;
		}
		
		// If there is only one command, put it into cmd_list as well:
		if (count_cmd == 0) 
		{
			//cmd_list[count_cmd] = (char*)malloc(sizeof(char));
			cmd_list[count_cmd] = raw_input;
			count_cmd += 1;
		}	

		// For each of the seperated commands:
		for (j=0; j<count_cmd; j++) 
		{
	  		raw_input = cmd_list[j];
	
			int i = 0;
			int isEmpty = 1; // if input is empty
			while (raw_input[i] != '\n')
			{
				if (raw_input[i] != ' ')
					isEmpty = 0;
				i++;
			}
			if (isEmpty == 1)
				continue;
			
			// Check parallel commands: 
			i = 0;
			char **cmd_parallel = (char**)malloc(sizeof(char*) * 512);
			int count_para = 0; // buffer for each parallel command
			while (raw_input[i] != '\0')	
			{
				if (raw_input[i] == '&') 
				{
					// Seperate each parallel commands, store them in cmd_parallel:
					char* para = strsep(&raw_input, "&"); 
					while (para != NULL) 
					{
						//if (strlen(para) > 0 || strcmp(para, " ") == 1) 
						if (checkspace(para) == 1)
						{
							//cmd_parallel[count_para] = (char*)malloc(sizeof(char)); 
							cmd_parallel[count_para] = para;
							count_para++;
						}
						para = strsep(&raw_input, "&");
					}	
					break;
				}
				i++;
			}

			// Prepare variables for redirection and space: 
			char ***input_parallel = (char***)malloc(sizeof(char**) * 512); 
				// A list of list of list of character
				// 1st dimension: points to the head of all parallel commands
				// 2nd dimension: points to each command 
				// 3rd dimension: points each argument of each command
			int **argnum = (int**)malloc(sizeof(int*) * 512); 
				// A list of argument numbers for each argument 
			int count_arg = 0;
			char **redir = (char**)malloc(sizeof(char*) * 512);
				// A list of redirection output destination filenames
			if (count_para == 0) 
			{
				//cmd_parallel[0] = (char*)malloc(sizeof(char)); 
				cmd_parallel[0] = raw_input;
				count_para = 1;
			}
			
			// Parse and redirection for each parallel command:	
			for (i=0; i<count_para; i++) 
			{
				input_parallel[i] = (char**)malloc(sizeof(char*) * 512);
				argnum[i] = (int*)malloc(sizeof(int) * 512);
				//redir[i] = (char*)malloc(sizeof(char));
				int result = prepare(cmd_parallel[i], input_parallel[i], argnum[i], &redir[i]); 
				if (result == 0)
					argnum[i] = 0;
			}

			// Respond to commands:
			int rc;
			int pidlist[count_para];
			for (int i=0; i<count_para; i++) 
			{	
				if (argnum[i] == 0)
					continue; // Skip current command

				if (strcmp(input_parallel[i][0], "exit") == 0)
				{
					//printf("built-in exit\n");
					call_exit(&running);
					break;
				} 
					
				if (strcmp(input_parallel[i][0], "cd") == 0)
				{
					//printf("cd\n");
					call_cd(input_parallel[i], argnum[i]);		
				} 
				else if (strcmp(input_parallel[i][0], "path") == 0) 
				{
					//printf("path\n");
					//char *path_da = (char*)malloc(strlen(input_parallel[i][2]));
					//strcpy(&path_da, input_parallel[i][2]);
					call_path(path, input_parallel[i], &count_path, argnum[i]);
				}
				else 
				{
					rc = fork();
					if (rc == 0)
					{
						//printf("other commands:\n");
						cmd(path, input_parallel[i], &count_path, argnum[i], redir[i]);
						exit(0);
					}
					else 	
						pidlist[i] = rc;
				}		
			}
			
			for (i=0; i<count_para; i++)
			{	
				int status;
				waitpid(pidlist[i], &status, WUNTRACED | WCONTINUED);
				//wait(NULL);
			}
	
			int z;
			for(z=0; z<count_para; z++)
				free(argnum[z]);
					

			free(cmd_parallel);
			free(argnum);
			free(redir);
			free(raw_multi);
		}

		free(cmd_list);
		free(raw_backup);
	}

	int z;
	for (z=0; z<count_path; z++) 
		free(path[z]);
	free(path);
	
	//printf("\n");
	exit(0);	
}
