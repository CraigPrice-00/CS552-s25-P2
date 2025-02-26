/**Update this file with the starter code**/
#include "lab.h"
#include <stdio.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <readline/history.h>
#include <pwd.h>
#include <errno.h>

/* get_prompt: this function accepts the name of an environment variable and returns the prompt
 * env: the name of an environment variable
 */
char* get_prompt(const char* env) {
    char* prompt = getenv(env);
    if ( prompt == NULL) { 
        prompt = "shell>";
    }
    //limit prompt to 50 chars
    char* returnValue = calloc(51, sizeof(char));
    if (!returnValue) { 
        fprintf(stderr, "Calloc failed in get_prompt");
        exit(EXIT_FAILURE);
    }
    strncpy(returnValue, prompt, 50);
    returnValue[50] = '\0';
    return returnValue;
}

/* change_dir: this function accepts a directory and returns 0 if successful and -1 if unsuccessful
 * dir: the directory to change to
 */
int change_dir(char** dir) {
    if (!dir) {
        fprintf(stderr, "NULL pointer within cd\n");
        return -1;
    }
    //make sure not too many arguments
    if (dir[2]) {
        fprintf(stderr, "Too many arguments for cd command\n");
        return -1;
    }

    int result;
    if (!dir[1]) {
        if ((result = chdir(getenv("HOME")))) {
            result = chdir(getpwuid(getuid())->pw_dir);
        }
    }
    else {
        result = chdir(dir[1]);
    }

    //print any errors
    if (result) {
        fprintf(stderr,"Error: %s\n", strerror(errno));
    }
    return result;
}

/* cmd_parse: this function accepts a line and parses it into an array of strings
 * line: the line to parse
 */
char** cmd_parse(char const* line) {
    if (!line) {
        fprintf(stderr,"NULL pointer passed to cmd_parse");
        exit(EXIT_FAILURE);
    }

    //the maximum number of args is from the system config, allocate an array of char pointers based on that size
    long maxArgs = sysconf(_SC_ARG_MAX);
    char** args = calloc(maxArgs, sizeof(char*));
    if (!args) { 
        fprintf(stderr,"Calloc failed in cmd_parse");
        exit(EXIT_FAILURE);
    }
    
    char* stringCopy = malloc(strlen(line) + 1);
    if (!stringCopy) { 
        fprintf(stderr,"Malloc failed in cmd_parse");
        exit(EXIT_FAILURE);
    }
    strncpy(stringCopy, line, strlen(line) + 1);
    stringCopy[strlen(stringCopy)] = '\0';

    char* startSubstr = stringCopy;
    char* endSubstr = stringCopy;
    long argCount = 0;

    while(true/*startSubstr != '\0'*/) {
        if (*endSubstr == ' ' || *endSubstr == '\0') {
            //if we've reached the max args and we attempt to add another, print an error and break from loop
            if (argCount == maxArgs) {
                fprintf(stderr, "Overflowed max args. May encounter undefined behavior.\n");
                break;
            }
            //allocate some space for the substring and copy it over
            args[argCount] = (char *)malloc(endSubstr - startSubstr + 1);
            memcpy(args[argCount],startSubstr, endSubstr - startSubstr);
            args[argCount][endSubstr - startSubstr] = '\0';
            argCount++;

            //check if we're ready to break the loop (loop should only ever end on a char )
            if (*endSubstr == 0) { break; }

            //skip spaces until we find the next char
            while(*endSubstr == ' ') { endSubstr++; }
            startSubstr = endSubstr;
            continue;
        }
        endSubstr++;
    }
    //printf("There are %ld arguments\n", argCount);
    free(stringCopy);
    return args;
}

/* cmd_free: this function frees a command that was allocated by cmd_parse
 * line: the command to be freed
 */
void cmd_free(char** line) {
    if (!line) {
        fprintf(stderr, "Null pointer passed to cmd_free");
        exit(EXIT_FAILURE);
    }
    size_t i = 0;
    while(line[i] != NULL) {
        free(line[i++]);
    }
    free(line);
}

/* trim_white: this function removes the white space at the beginning and end of a string
 * line: the string to remove white space from
 */
char* trim_white(char* line) {
    if (!line) {
        fprintf(stderr,"NULL pointer passed to trim_white\n");
        exit(EXIT_FAILURE);
    }
    //create a pointer to the first character and last character
    char* startLine = line;
    char* endLine = line + strlen(line) - 1;

    //calculate how many spaces in front and rear
    while (*(startLine) == ' ') { startLine++; }

    //check if startLine made it all the way to the end, meaning empty or only spaces
    if (*startLine == '\0') { 
        line[0] = '\0';
        return line;
    }

    while (*(endLine) == ' ' && endLine != startLine) { endLine--; }

    //shift everything forward by startOffset
    for (long i = startLine - line; i < endLine - line + 1; i++) {
        line[i - (startLine - line)] = line[i];
    }

    //write the null terminator at the new end
    *(line - startLine + endLine + 1) = '\0';
    return line;
}

/* do_builtin: this function attempts to perform a builtin and returns true if s
 * sh: the shell
 * argv: the argument list
 */
bool do_builtin(struct shell* sh, char** argv) {
    if (!sh || !argv || !argv[0]) {
        fprintf(stderr,"NULL passed to do_builtin\n");
        exit(EXIT_FAILURE);
    }
    if (!strcmp(argv[0],"exit")) {
        //free the things we have allocated
        sh_destroy(sh);
        cmd_free(argv);
        clear_history();
        printf("Shell exited successfully\n");
        exit(EXIT_SUCCESS);
        return true;
    }
    else if (!strcmp(argv[0],"cd")) {
        change_dir(argv);
        return true;
    }
    else if (!strcmp(argv[0],"history")) {
        HIST_ENTRY** hist = history_list();
        if (!hist) { 
            fprintf(stderr, "Error retrieving history\n");
            return true; }
        size_t i = 0;
        printf("\n--HISTORY--\n");
        while(hist[i] != NULL) {
            printf("%s\n", hist[i]->line);
            i++;
        }
        printf("\n");
        return true;
    }
    return false;
}

/* sh_init: this function initializes the shell and performs operations to ensure if functions properly
 * sh: the shell to initialize
 */
void sh_init(struct shell* sh) {
    //set the prompt from environment variable
    sh->prompt = get_prompt("MY_PROMPT");
    
    //grab terminal
    sh->shell_terminal = STDIN_FILENO;
    sh->shell_is_interactive = isatty(sh->shell_terminal);
    
    if (sh->shell_is_interactive) {
        //check shell's process group
        sh->shell_pgid = getpgrp();

        //kill the process group until it matches shell_terminal
        while (tcgetpgrp (sh->shell_terminal) != (sh->shell_pgid = getpgrp())) {
            kill (sh->shell_pgid, SIGTTIN); 
        }

        //set signals to ignore
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);

        //put the shell in its own process group
        sh->shell_pgid = getpid ();
        if (setpgid (sh->shell_pgid, sh->shell_pgid) < 0) 
        {
          fprintf(stderr, "Couldn't put the shell in its own process group");
          exit (EXIT_FAILURE);
        }

      /* Grab control of the terminal.  */
      tcsetpgrp (sh->shell_terminal, sh->shell_pgid);

      /* Save default terminal attributes for shell.  */
      tcgetattr (sh->shell_terminal, &sh->shell_tmodes);
    }
}

/* sh_destroy: this function destroys a shell initialized by sh_init
 * sh: the shell to be destroyed
 */
void sh_destroy(struct shell* sh) {
    free(sh->prompt);
}

/* parse_args: this function parses the arguments passed to the shell
 * argc: the arg count
 * argv: the array of arg strings
 */
void parse_args(int argc, char** argv) {
    bool vflag = 0;
    int c;
    opterr = 0;

    while ((c = getopt (argc, argv, "v")) != -1) {
        switch (c)
            {
            case 'v':
                vflag = 1;
                break;
            case '?':
                fprintf(stderr, "-%c is not a valid option\n", optopt);
                exit(EXIT_FAILURE);
            default:
                abort();
        }
    }
    
    for (int i = optind; i < argc; i++) {
        printf ("%s uses invalid syntax or invalid argument\n", argv[i]);
        exit(EXIT_FAILURE);
    }
    
    if (vflag) {
        printf("Shell version is: %d.%d\n", lab_VERSION_MAJOR, lab_VERSION_MINOR);
        exit(EXIT_SUCCESS);
    }
}

