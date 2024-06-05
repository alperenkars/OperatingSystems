#include <sys/types.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h> // termios, TCSANOW, ECHO, ICANON
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
const char *sysname = "mishell";

enum return_codes {
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};

struct command_t {
	char *name;
	bool background;
	bool auto_complete;
	int arg_count;
	char **args;
	char *redirects[3]; // in/out redirection
	struct command_t *next; // for piping
};

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command) {
	int i = 0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background ? "yes" : "no");
	printf("\tNeeds Auto-complete: %s\n",
		   command->auto_complete ? "yes" : "no");
	printf("\tRedirects:\n");

	for (i = 0; i < 3; i++) {
		printf("\t\t%d: %s\n", i,
			   command->redirects[i] ? command->redirects[i] : "N/A");
	}

	printf("\tArguments (%d):\n", command->arg_count);

	for (i = 0; i < command->arg_count; ++i) {
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	}

	if (command->next) {
		printf("\tPiped to:\n");
		print_command(command->next);
	}
}

/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command) {
	if (command->arg_count) {
		for (int i = 0; i < command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}

	for (int i = 0; i < 3; ++i) {
		if (command->redirects[i])
			free(command->redirects[i]);
	}

	if (command->next) {
		free_command(command->next);
		command->next = NULL;
	}

	free(command->name);
	free(command);
	return 0;
}

/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt() {
	char cwd[1024], hostname[1024];
	gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}

/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command) {
	const char *splitters = " \t"; // split at whitespace
	int index, len;
	len = strlen(buf);

	// trim left whitespace
	while (len > 0 && strchr(splitters, buf[0]) != NULL) {
		buf++;
		len--;
	}

	while (len > 0 && strchr(splitters, buf[len - 1]) != NULL) {
		// trim right whitespace
		buf[--len] = 0;
	}

	// auto-complete
	if (len > 0 && buf[len - 1] == '?') {
		command->auto_complete = true;
	}

	// background
	if (len > 0 && buf[len - 1] == '&') {
		command->background = true;
	}

	char *pch = strtok(buf, splitters);
	if (pch == NULL) {
		command->name = (char *)malloc(1);
		command->name[0] = 0;
	} else {
		command->name = (char *)malloc(strlen(pch) + 1);
		strcpy(command->name, pch);
	}

	command->args = (char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index = 0;
	char temp_buf[1024], *arg;

	while (1) {
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch)
			break;
		arg = temp_buf;
		strcpy(arg, pch);
		len = strlen(arg);

		// empty arg, go for next
		if (len == 0) {
			continue;
		}

		// trim left whitespace
		while (len > 0 && strchr(splitters, arg[0]) != NULL) {
			arg++;
			len--;
		}

		// trim right whitespace
		while (len > 0 && strchr(splitters, arg[len - 1]) != NULL) {
			arg[--len] = 0;
		}

		// empty arg, go for next
		if (len == 0) {
			continue;
		}

		// piping to another command
		if (strcmp(arg, "|") == 0) {
			struct command_t *c = malloc(sizeof(struct command_t));
			int l = strlen(pch);
			pch[l] = splitters[0]; // restore strtok termination
			index = 1;
			while (pch[index] == ' ' || pch[index] == '\t')
				index++; // skip whitespaces

			parse_command(pch + index, c);
			pch[l] = 0; // put back strtok termination
			command->next = c;
			continue;
		}

		// background process
		if (strcmp(arg, "&") == 0) {
			// handled before
			continue;
		}

		// handle input redirection
		redirect_index = -1;
		if (arg[0] == '<') {
			redirect_index = 0;
		}

		if (arg[0] == '>') {
			if (len > 1 && arg[1] == '>') {
				redirect_index = 2;
				arg++;
				len--;
			} else {
				redirect_index = 1;
			}
		}

		if (redirect_index != -1) {
			command->redirects[redirect_index] = malloc(len);
			strcpy(command->redirects[redirect_index], arg + 1);
			continue;
		}

		// normal arguments
		if (len > 2 &&
			((arg[0] == '"' && arg[len - 1] == '"') ||
			 (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
		{
			arg[--len] = 0;
			arg++;
		}

		command->args =
			(char **)realloc(command->args, sizeof(char *) * (arg_index + 1));

		command->args[arg_index] = (char *)malloc(len + 1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count = arg_index;

	// increase args size by 2
	command->args = (char **)realloc(
		command->args, sizeof(char *) * (command->arg_count += 2));

	// shift everything forward by 1
	for (int i = command->arg_count - 2; i > 0; --i) {
		command->args[i] = command->args[i - 1];
	}

	// set args[0] as a copy of name
	command->args[0] = strdup(command->name);

	// set args[arg_count-1] (last) to NULL
	command->args[command->arg_count - 1] = NULL;

	return 0;
}

void prompt_backspace() {
	putchar(8); // go back 1
	putchar(' '); // write empty over
	putchar(8); // go back 1 again
}

/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command) {
	size_t index = 0;
	char c;
	char buf[4096];
	static char oldbuf[4096];

	// tcgetattr gets the parameters of the current terminal
	// STDIN_FILENO will tell tcgetattr that it should write the settings
	// of stdin to oldt
	static struct termios backup_termios, new_termios;
	tcgetattr(STDIN_FILENO, &backup_termios);
	new_termios = backup_termios;
	// ICANON normally takes care that one line at a time will be processed
	// that means it will return if it sees a "\n" or an EOF or an EOL
	new_termios.c_lflag &=
		~(ICANON |
		  ECHO); // Also disable automatic echo. We manually echo each char.
	// Those new settings will be set to STDIN
	// TCSANOW tells tcsetattr to change attributes immediately.
	tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

	show_prompt();
	buf[0] = 0;

	while (1) {
		c = getchar();
		// printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

		// handle tab
		if (c == 9) {
		       	buf[index++] = '?'; // autocomplete
			break;
		}

		// handle backspace
		if (c == 127) {
			if (index > 0) {
				prompt_backspace();
				index--;
			}
			continue;
		}

		if (c == 27 || c == 91 || c == 66 || c == 67 || c == 68) {
			continue;
		}

		// up arrow
		if (c == 65) {
			while (index > 0) {
				prompt_backspace();
				index--;
			}

			char tmpbuf[4096];
			printf("%s", oldbuf);
			strcpy(tmpbuf, buf);
			strcpy(buf, oldbuf);
			strcpy(oldbuf, tmpbuf);
			index += strlen(buf);
			continue;
		}

		putchar(c); // echo the character
		buf[index++] = c;
		if (index >= sizeof(buf) - 1)
			break;
		if (c == '\n') // enter key
			break;
		if (c == 4) // Ctrl+D
			return EXIT;
	}

	// trim newline from the end
	if (index > 0 && buf[index - 1] == '\n') {
		index--;
	}

	// null terminate string
	buf[index++] = '\0';

	strcpy(oldbuf, buf);

	parse_command(buf, command);

	// print_command(command); // DEBUG: uncomment for debugging

	// restore the old settings
	tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
	return SUCCESS;
}

int process_command(struct command_t *command);

void handle_piping(char *input_command) {
    char *splitters = "|";
    char *pch;
    struct command_t *first_command = NULL;
    struct command_t *prev_command = NULL;

    pch = strtok(input_command, splitters);
    while (pch != NULL) {
        struct command_t *c = malloc(sizeof(struct command_t));
        parse_command(pch, c);

        if (first_command == NULL) {
            first_command = c;
        } else {
            prev_command->next = c;
        }
        prev_command = c;

        pch = strtok(NULL, splitters);
    }

    // Now 'first_command' points to the first command in the pipeline
    // 'prev_command' points to the last command in the pipeline

    struct command_t *current_command = first_command;
    int pipefd[2];

    while (current_command != NULL) {
        if (current_command->next != NULL) {
            // Create pipe for communication between commands
            if (pipe(pipefd) == -1) {
                perror("Pipe creation failed");
                exit(EXIT_FAILURE);
            }

            pid_t pid = fork();
            if (pid == -1) {
                perror("Fork failed");
                exit(EXIT_FAILURE);
            }

            if (pid == 0) {
                // Child process
                close(pipefd[0]); // Close unused read end of pipe
                dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to write end of pipe
                close(pipefd[1]); // Close write end of pipe

                // Execute current command
                // For simplicity, assume execute_command function executes the command
                process_command(current_command);

                exit(EXIT_SUCCESS); // Exit child process
            } else {
                // Parent process
                close(pipefd[1]); // Close write end of pipe
                dup2(pipefd[0], STDIN_FILENO); // Redirect stdin to read end of pipe
                close(pipefd[0]); // Close read end of pipe
            }
        } else {
            // Last command in the pipeline, execute without creating pipe
            process_command(current_command);
        }

        current_command = current_command->next; // Move to next command in the pipeline
    }

    // Wait for all child processes to finish
    while (wait(NULL) > 0);
}


int main() {
	while (1) {
		struct command_t *command = malloc(sizeof(struct command_t));

		// set all bytes to 0
		memset(command, 0, sizeof(struct command_t));

		int code;
		code = prompt(command);
		if (code == EXIT) {
			break;
		}
		

		code = process_command(command);
		if (code == EXIT) {
			break;
		}
//                printf("main: %s\n", command);

		free_command(command);
	}

	printf("\n");
	return 0;
}

int process_command(struct command_t *command) {
	int r;

//	printf("process: %s\n", command);
	
	if(command->auto_complete){
	command->name[strlen(command->name) - 1] = '\0';
	autocomplete_command(command->name);}

	if (strcmp(command->name, "") == 0) {
	return SUCCESS;
	}

	if (strcmp(command->name, "exit") == 0) {
		return EXIT;
	}

if (strcmp(command->name, "mindmap") == 0) {
		if (command->arg_count != 2) {
			printf("%d\n", command->arg_count);
			printf("Usage: mindmap\n");
			return SUCCESS;
	}
	    mindmap();}

if (strcmp(command->name, "hdiff") == 0) {
		if (command->arg_count < 4) {
			printf("Usage: hdiff [-a | -b] file1 file2\n");
			return SUCCESS;
	}
		else{
			FILE *file1 = fopen(command->args[2], "rb");
   		    FILE *file2 = fopen(command->args[3], "rb");

    	if (!file1 || !file2) {
        perror("Error opening files");
        return SUCCESS;
    }

    if (strcmp(command->args[1], "-a") == 0) {
        compareTextFiles(file1, file2);
    } else if (strcmp(command->args[1], "-b") == 0) {
        compareBinaryFiles(file1, file2);
    } else {
        printf("Invalid option: %s\n", command->args[1]);
        
    }

    fclose(file1);
    fclose(file2);
		}
		return SUCCESS;}

	if (strcmp(command->name, "cd") == 0) {
		if (command->arg_count > 0) {
  			r = chdir(command->args[1]);
			if (r == -1) {
				printf("-%s: %s: %s\n", sysname, command->name,
					   strerror(errno));
			                   
			}

			return SUCCESS;
		}
	}
/* if (command->arg_count > 0 && strcmp(command->args[command->arg_count - 1], "&") == 0) {
        command->background = 1; // Set background flag
        command->arg_count--; // Remove "&" from argument list
        command->args[command->arg_count] = NULL; // Null-terminate argument list
    } */

char *pch = strtok(command->name, "|");
    if (pch != NULL && strtok(NULL, "|") != NULL) {
        // Command contains piping, handle it
        handle_piping(command->name);
        return SUCCESS;
    }

	pid_t pid = fork();
	// child
	if (pid == 0) {
		/// This shows how to do exec with environ (but is not available on MacOs)
		// extern char** environ; // environment variables
		// execvpe(command->name, command->args, environ); // exec+args+path+environ

		/// This shows how to do exec with auto-path resolve
		// add a NULL argument to the end of args, and the name to the beginning
		// as required by exec

// own exec with path resolving using execv()
		// do so by replacing the execvp call below

		execvp(command->name, command->args);		
		exit(0);}

/*		char path[1024]; // Buffer to store resolved path
    	if (find_executable(command->name, path)) {
        	// Execute the command with the resolved path and argument list
printf("HUAAAA\n");        	
execv(path, command->args);}
else{
printf("kdgjksglks\n");}
exit(0);
	} */ else {
		// TODO: implement background processes here
	/*	if (command->background) {
        	// If the command is supposed to run in the background
        	// You may want to store background process information for later handling
        	printf("[%d] %s\n", pid, command->name);
    } 	
    */
    
		wait(0); // wait for child process to finish
		return SUCCESS;
	}

	// TODO: your implementation here

	printf("-%s: %s: command not found\n", sysname, command->name);
	return 0;
}

int find_executable(const char *command, char *path) {
    // Function to find the executable path of a command
    // You need to implement this function based on your system's path resolution logic
  
    snprintf(path, 1024, "/Users/akars20/%s", command);
    return access(path, X_OK) == 0; // Check if the file is executable
}

void autocomplete_command(char *buf) {
//	printf("%s\n", &buf);

	char matched_commands[10][100];
    // List of existing and newly introduced commands
    char *commands[] = {"mindmap","ls", "cd", "pwd", "echo", "mkdir", "touch", "cat", "cp", "mv", "rm"};
    int num_commands = sizeof(commands) / sizeof(commands[0]);
//  buf[strlen(buf) - 1] = '\0';
    // Extract the partially typed command from the buffer
    char partial_command[100]; // Adjust the size as needed
    sscanf(buf, "%s", partial_command);
//      buf[strlen(buf) - 1] = '\0';
  
    
//partial_command[strlen(partial_command)-1] = '\0';
		int match_found = 0;
        for (int i = 0; i < num_commands; i++) {
		
            if (strcmp(partial_command, commands[i]) == 0) {
                match_found = 1;
//		printf("buldum\n");
				//list files in dir
  
            }

        }

//printf("1\n");
        // If the command is in the commands array, list files in the current directory
   if (match_found) {

        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            DIR *dir;
            struct dirent *entry;

            // Open the current directory
            if ((dir = opendir(cwd)) != NULL) {
                // List files in the current directory
                printf("\nFiles in current directory:\n");
                while ((entry = readdir(dir)) != NULL) {
                    printf("%s\n", entry->d_name);
                }
                closedir(dir);
            }
        }
    }
	else{ 
    // Find matches for the partially typed command
    

    int num_matched = 0;
//    printf("num commands: %d", num_commands);
    for (int i = 0; i < num_commands; i++) {

//	printf("partial and command: %s - %s\n", partial_command, commands[i]);
        if (strncmp(partial_command, commands[i], strlen(partial_command)) == 0) {

//	  printf("emwkgj\n");  
          strcpy(matched_commands[num_matched++], commands[i]);
	 
        }
    }

    // Autocomplete the command if there's a single match
    if (num_matched == 1) {


        strcpy(buf, matched_commands[num_matched-1]); // Replace input buffer with the match
	return;
    } else if (num_matched > 1) {
        // Print all matches for the user to choose from
        printf("\nMultiple matches found:\n");
        for (int i = 0; i < num_matched; i++) {
           
                printf("%s\n", matched_commands[i]);
            
        }
    }
}
}


void compareTextFiles(FILE *file1, FILE *file2) {
    char line1[256], line2[256];
    int lineNum = 1;
    int diffCount = 0;

    while (fgets(line1, sizeof(line1), file1) && fgets(line2, sizeof(line2), file2)) {
        if (strcmp(line1, line2) != 0) {
            printf("%s:Line %d: %s", "file1.txt", lineNum, line1);
            printf("%s:Line %d: %s", "file2.txt", lineNum, line2);
            diffCount++;
        }
        lineNum++;
    }

    if (diffCount > 0) {
        printf("%d different lines found\n", diffCount);
    } else {
        printf("The two text files are identical\n");
    }
}

void compareBinaryFiles(FILE *file1, FILE *file2) {
    fseek(file1, 0, SEEK_END);
    fseek(file2, 0, SEEK_END);

    long fileSize1 = ftell(file1);
    long fileSize2 = ftell(file2);

    if (fileSize1 != fileSize2) {
        printf("The two files are different in %ld bytes\n", abs(fileSize1 - fileSize2));
    } else {
        printf("The two files are identical\n");
    }
}



void mindmap() {
    char* directory = malloc(1024); // Allocate memory for directory
    if (directory == NULL) {
        perror("Memory allocation error");
        return;
    }
    scanf("%s", directory);

    printf("Creating mind map for directory: %s\n", directory);
    exploreDirectory(directory, 0);
}

void printIndent(int level) {
    for (int i = 0; i < level; i++) {
        printf("|   ");
    }
}

void exploreDirectory(const char *path, int level) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("Unable to open directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(fullPath, &st) == -1) {
            perror("Unable to get file status");
            closedir(dir);
            return;
        }

        if (S_ISDIR(st.st_mode)) {
            printIndent(level);
            printf("|-- %s (Directory)\n", entry->d_name);
            exploreDirectory(fullPath, level + 1);
        } else {
            printIndent(level);
            printf("|-- %s\n", entry->d_name);
        }
    }

    closedir(dir);
}   
