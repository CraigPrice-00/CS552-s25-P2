/**Update this file with the starter code**/
#include "lab.h"
#include <stdio.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <readline/history.h>
#include <pwd.h>
#include <errno.h>

char* get_prompt(const char* env) {
    char* prompt = getenv(env);
    if ( prompt == NULL) { 
        prompt = "shell>";
    }
    //limit prompt to 50 chars
    char* returnValue = calloc(51, sizeof(char));
    if (!returnValue) { return NULL; }
    strncpy(returnValue, prompt, 50);
    returnValue[50] = '\0';
    return returnValue;
}

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
    //see if there is a path or empty
    if (!dir[1]) {
        //no path, try to get home directory path from getenv
        char* home = getenv("HOME");
        //continue if it exist
        if (home) {
            result = chdir(home);
        }
        //otherwise fall back to getuid and getpwuid
        else {
            uid_t uid = getuid();
            struct passwd* pass = getpwuid(uid);
            result = chdir(pass->pw_dir);
        }
    }
    //otherwise just change to the path
    else {
        result = chdir(dir[1]);
    }
    if (result) {
        fprintf(stderr,"Error: %s\n", strerror(errno));
    }
    return result;
}

char** cmd_parse(char const* line) {
    // UNUSED(line);
    //printf("The line is: %s >\n", line);
    long maxCommands = sysconf(_SC_ARG_MAX);
    //printf("The maximum args is: %ld\n", maxCommands);
    char** args = calloc(maxCommands, sizeof(char*));
    if (!args) { return NULL; }
    
    char* stringCopy = malloc(strlen(line) + 1);
    if (!stringCopy) { return NULL; }

    strcpy(stringCopy, line);
    char* startSubstr = stringCopy;
    char* endSubstr = stringCopy;
    size_t argCount = 0;

    while(*startSubstr != '\0') {
        if (*endSubstr == ' ' || *endSubstr == '\0') {
            //printf("arg #%ld start: %c\n", argCount, *startSubstr);
            //printf("arg #%ld end: %c\n", argCount, *(endSubstr-1));
            
            args[argCount] = malloc(endSubstr - startSubstr + 1);
            memcpy(args[argCount],startSubstr, endSubstr - startSubstr);
            args[argCount][endSubstr - startSubstr] = '\0';
            
            //printf("ARG %ld: %s|\n", argCount, args[argCount]);
            
            argCount++;
            
            if (*endSubstr == '\0') { break; }
            endSubstr++;
            startSubstr = endSubstr;
            continue;
        }
        endSubstr++;
    }
    //printf("There are %ld arguments\n", argCount);
    free(stringCopy);
    return args;
}

void cmd_free(char** line) {
    size_t i = 0;
    while(line[i]) {
        free(line[i++]);
    }
    free(line);
}

char* trim_white(char* line) {
    //create a pointer to the first character and last character
    char* startLine = line;
    char* endLine = line + strlen(line) - 1;

    //printf("the first character is: %c  >\n", startLine[0]);
    //printf("the last character is: %c  >\n", endLine[0]);
    //counters for offset (number of spaces)
    size_t startOffset = 0;
    size_t endOffset = 0;

    //calculate how many spaces in front and rear
    while (*(startLine) == ' ' && startLine != endLine) { 
        startLine++;
        startOffset++; }
    //printf("the first non-space character is: %c  >\n", startLine[0]);
    //printf("the last non-space character is: %c  >\n", endLine[0]);
    //check if startLine made it all the way to the end, meaning only spaces
    if (*startLine == '\0') { 
        line[0] = '\0';
        return line;
     }
    while (*(endLine) == ' ' && endLine != startLine - 1) { 
        endLine--;
        endOffset++; }

    //shift everything forward by startOffset
    for (size_t i = startOffset; i < (strlen(line) - endOffset); i++) {
        line[i - startOffset] = line[i];
    }

    //write the null terminator at the new end
    *(line + strlen(line) - endOffset - startOffset) = '\0';
    return line;
}

bool do_builtin(struct shell* sh, char** argv) {
    UNUSED(sh);
    if (!strcmp(argv[0],"exit")) {
        //free the things we have allocated
        sh_destroy(sh);
        cmd_free(argv);
        clear_history();
        printf("Shell exited successfully\n");
        exit(0);
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

void sh_init(struct shell* sh) {
    //set the prompt from environment variable
    sh->prompt = get_prompt("MY_PROMPT");
    
    //grab terminal
    sh->shell_terminal = STDIN_FILENO;
    sh->shell_is_interactive = isatty(sh->shell_terminal);

    //set signals to ignore
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    
    if (sh->shell_is_interactive) {
        //check shell's process group
        sh->shell_pgid = getpgrp();            
        while (tcgetpgrp (sh->shell_terminal) != (sh->shell_pgid = getpgrp ()))
            kill (- sh->shell_pgid, SIGTTIN);
        }
        /* Put ourselves in our own process group.  */
        sh->shell_pgid = getpid ();
      if (setpgid (sh->shell_pgid, sh->shell_pgid) < 0)
        {
          perror ("Couldn't put the shell in its own process group");
          exit (1);
        }

      /* Grab control of the terminal.  */
      tcsetpgrp (sh->shell_terminal, sh->shell_pgid);

      /* Save default terminal attributes for shell.  */
      tcgetattr (sh->shell_terminal, &sh->shell_tmodes);

}

void sh_destroy(struct shell* sh) {
    free(sh->prompt);
}

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
                exit(1);
            default:
                abort();
        }
    }
    
    for (int i = optind; i < argc; i++) {
        printf ("%s uses invalid syntax or invalid argument\n", argv[i]);
        exit(1);
    }
    
    if (vflag) {
        printf("Shell version is: %d.%d\n", lab_VERSION_MAJOR, 
            lab_VERSION_MINOR);
            exit(0);
    }
}

