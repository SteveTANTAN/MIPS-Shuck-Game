////////////////////////////////////////////////////////////////////////
// COMP1521 21t2 -- Assignment 2 -- shuck, A Simple Shell
// <https://www.cse.unsw.edu.au/~cs1521/21T2/assignments/ass2/index.html>
//
// Written by Xingyu Tan (z5237560) on 03/08/2021.
//
// 2021-07-12    v1.0    Team COMP1521 <cs1521@cse.unsw.edu.au>
// 2021-07-21    v1.1    Team COMP1521 <cs1521@cse.unsw.edu.au>
//     * Adjust qualifiers and attributes in provided code,
//       to make `dcc -Werror' happy.
//

#include <sys/types.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <glob.h>
#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <spawn.h>


#define ADDRESS "/.shuck_history"

//
// Interactive prompt:
//     The default prompt displayed in `interactive' mode --- when both
//     standard input and standard output are connected to a TTY device.
//
static const char *const INTERACTIVE_PROMPT = "shuck& ";

//
// Default path:
//     If no `$PATH' variable is set in Shuck's environment, we fall
//     back to these directories as the `$PATH'.
//
static const char *const DEFAULT_PATH = "/bin:/usr/bin";


//
// Input line length:
//     The length of the longest line of input we can read.
//
static const size_t MAX_LINE_CHARS = 1024;

//
// Special characters:
//     Characters that `tokenize' will return as words by themselves.
//
static const char *const SPECIAL_CHARS = "!><|";

//
// Word separators:
//     Characters that `tokenize' will use to delimit words.
//
static const char *const WORD_SEPARATORS = " \t\r\n";

// adding the struct for history each node
typedef struct Node {
    char* data;
    struct Node* next;
    int number;
}LinkStack;

// adding the struct for historylist and reacord 
// the first one and last one
typedef struct {
    // point to the first one
    LinkStack* front; 
    // point to the last one
    LinkStack* rear; 
}Queue;

static void execute_command(char **words, char **path, char **environment, Queue* stack,char *line);
static void do_exit(char **words);
static int is_executable(char *pathname);
static char **tokenize(char *s, char *separators, char *special_chars);
static void free_tokens(char **tokens);

//////////////////////////////////
//       My own Function        //
//////////////////////////////////
Queue* InitHistoryQueue();
void ADDQueue(Queue* q, char *data);
void PrintHistory(Queue* q, int number);
char *getHistoryCmd(Queue* q,int number);
int getlastseqNo(Queue* q);
char **globFileName(char **tokens);
int tokenLen(char **tokens);
void write_out(char **words, char *path, char **environment,Queue* stack);
void read_in(char **words, char *path, char **environment,Queue* stack);
void saveCommandHistory(Queue* q);


int main (void) {
    // Ensure `stdout' is line-buffered for autotesting.
    setlinebuf(stdout);

    // Environment variables are pointed to by `environ', an array of
    // strings terminated by a NULL value -- something like:
    //     { "VAR1=value", "VAR2=value", NULL }
    extern char **environ;

    // Grab the `PATH' environment variable for our path.
    // If it isn't set, use the default path defined above.
    char *pathp;
    if ((pathp = getenv("PATH")) == NULL) {
        pathp = (char *) DEFAULT_PATH;
    }
    char **path = tokenize(pathp, ":", "");

    // Should this shell be interactive?
    bool interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
    // initial the history and record from the file.
    Queue* stack = InitHistoryQueue();

    while (1) {
        // If `stdout' is a terminal (i.e., we're an interactive shell),
        // print a prompt before reading a line of input.
        if (interactive) {
            fputs(INTERACTIVE_PROMPT, stdout);
            fflush(stdout);
        }

        char line[MAX_LINE_CHARS];
        if (fgets(line, MAX_LINE_CHARS, stdin) == NULL)
            break;

        // Tokenise and execute the input line.
        char **command_words =
            tokenize(line, (char *) WORD_SEPARATORS, (char *) SPECIAL_CHARS);
        execute_command(command_words, path, environ, stack,line);
        free_tokens(command_words);
        saveCommandHistory(stack);

    }
    free_tokens(path);
    return 0;
}


//
// Execute a command, and wait until it finishes.
//
//  * `words': a NULL-terminated array of words from the input command line
//  * `path': a NULL-terminated array of directories to search in;
//  * `environment': a NULL-terminated array of environment variables.
//
static void execute_command(char **words, char **path, char **environment,Queue* stack, char *line)
{
    assert(words != NULL);
    assert(path != NULL);
    assert(environment != NULL);
    int cmdNo;   
    ///////////////////////////////////////////////////////
    //                SubSet 2    '!' features
    ///////////////////////////////////////////////////////
    
    // starting with '!'
    if (words[0][0] == '!') {
        // '!' with a number
        if (sscanf(line, "!%d", &cmdNo) == 1) {
            if (getHistoryCmd(stack,cmdNo) != NULL) {
                strcpy(line, getHistoryCmd(stack,cmdNo) );
                words = tokenize(line, (char *) WORD_SEPARATORS, (char *) SPECIAL_CHARS);
                printf("%s",line);
            } else {
                // no history with the specific number
                printf("No command #%d\n", cmdNo);
                return;
            }
        } else {
            // only the "!" sign, return the last history
            if (getlastseqNo(stack) >= 0) {
                strcpy(line, getHistoryCmd(stack, getlastseqNo(stack)));
                words = tokenize(line, (char *) WORD_SEPARATORS, (char *) SPECIAL_CHARS);
                printf("%s", line);
            } else {
                printf("No command #0\n");
                return;
            } 
        }
        
    }
    ///////////////////////////////////////////////////////
    //              SubSet 3     globFileName
    ///////////////////////////////////////////////////////
    words = globFileName(words);
    int tokenLength = tokenLen(words);

    char *program = words[0];
    if (program == NULL) {
        // nothing to do
        return;
    }
    if (strcmp(program, "exit") == 0) {
        do_exit(words);
        // `do_exit' will only return if there was an error.
        return;
    }
    ////////////////////////////////////////////////////
    //                  SubSet 4
    ////////////////////////////////////////////////////
    for (int i = 0; words[i] != NULL; i++) {
        if (!strcmp(words[i], "<")){
            if ((tokenLen(words) < 3)||i != 0) {
                fprintf(stderr,"invalid input redirection\n");
                return;
            } else {
                FILE *input = fopen(words[i+1], "r");
                if (input != NULL) {
                    fclose(input);
                } else {
                    fprintf(stderr,"%s: No such file or directory\n", words[i+1]);   
                    return;
                }
            }    
            
        }
        if (!strcmp(words[i], ">")){
            if ((tokenLen(words) < 3)||((i != tokenLen(words)-2) && (i != tokenLen(words)-3) )) {
                fprintf(stderr,"invalid output redirection\n");
                return;
            }
        }
    }
        
    
    if (strcmp(program, "<") == 0) {     
        program = words[2];
    }
    // adding the file and program name into path
    char fullPath[MAX_LINE_CHARS];
    // inorder to check if this program is able running
    if (strrchr(program, '/') == NULL) {
        int i;
        for (i = 0; path[i] != NULL; i++) {
            // connect the path address and the prgram,
            sprintf(fullPath, "%s/%s", path[i], program);
            if (is_executable(fullPath)) break;
        }
        if (path[i] == NULL) fullPath[0] = '\0';
    } else {
        strcpy(fullPath, program);
        if (!is_executable(fullPath)) fullPath[0] = '\0';
    }
    ////////////////////////////////////////////////////
    //             features for history
    ////////////////////////////////////////////////////
    if (strcmp(program, "history") ==0) {
        if ((strchr(line, '>') != NULL || strchr(line, '<') != NULL)) {
            fprintf(stderr,"%s: I/O redirection not permitted for builtin commands\n", program);
            return;
        }
        // if only "history" return deafult 10 returns
        if (words[1] == NULL) {
            PrintHistory(stack, 10);
            ADDQueue(stack, line);
        } else if (words[2] == NULL)  {
            // if only "history" with number return the values's histories
            if (sscanf(line, "history %d", &cmdNo) == 1) {
                PrintHistory(stack, cmdNo);
                ADDQueue(stack, line);
            // if only "history" with non-number return error
            } else {
                fprintf(stderr,"history: %s: numeric argument required\n", words[1]);   
            }
        } else{
            fprintf(stderr,"history: too many arguments\n");   
        }
        return;
    }
    ////////////////////////////////////////////////////
    //             features for cd
    ////////////////////////////////////////////////////
    if (strcmp(program, "cd") == 0) {
        // cd -> "HOME"
        if ((strchr(line, '>') != NULL || strchr(line, '<') != NULL)) {
            fprintf(stderr,"%s: I/O redirection not permitted for builtin commands\n", program);
            return;
        }
        if (words[1] == NULL) {
            chdir(getenv("HOME"));
            ADDQueue(stack, line);
        } else {
            // cd to new direction
            if (chdir(words[1]) == 0) {
                ADDQueue(stack, line);
            } else {
                fprintf(stderr,"%s: %s: No such file or directory\n", words[0],words[1]);   
            }
        }
        return;
    }
    ////////////////////////////////////////////////////
    //             features for pwd
    ////////////////////////////////////////////////////
    if (strcmp(program, "pwd") == 0) {
        if ((strchr(line, '>') != NULL || strchr(line, '<') != NULL)) {
            fprintf(stderr,"%s: I/O redirection not permitted for builtin commands\n", program);
            return;
        }
        char buffer[200];
        getcwd(buffer, sizeof(buffer));
        printf("current directory is '%s'\n", buffer);
        ADDQueue(stack, line);
        return;
    }



    // if it is able running
    if (fullPath[0] != '\0') {
        ////////////////////////////////////////////////////
        //             features for '<'
        ////////////////////////////////////////////////////
        if (strcmp(words[0], "<") == 0){
            write_out(words, fullPath, environment,stack);
            ADDQueue(stack, line);
            return;
        } 
        ////////////////////////////////////////////////////
        //             features for '>'
        ////////////////////////////////////////////////////
        if (tokenLength > 3 && strcmp(words[tokenLength-2], ">") == 0) {
            read_in(words, fullPath, environment,stack);
            ADDQueue(stack, line);
            return;
        }
        pid_t pid;
        // spawn "/bin/date" as a separate process
        if (posix_spawn(&pid, fullPath, NULL, NULL, words, environment) != 0) {
            perror("spawn");
            exit(1);
        }
        // wait for spawned processes to finish
        int exit_status;
        if (waitpid(pid, &exit_status, 0) == -1) {
            perror("waitpid");
            exit(1);
        }
        printf("%s exit status = %d\n",fullPath, WEXITSTATUS(exit_status));
        ADDQueue(stack, line);
        return;
        
    } else {
        fprintf(stderr,"%s: command not found\n", *words);
    }
}

//
// Implement the `exit' shell built-in, which exits the shell.
//
// Synopsis: exit [exit-status]
// Examples:
//     % exit
//     % exit 1
//
static void do_exit(char **words)
{
    assert(words != NULL);
    assert(strcmp(words[0], "exit") == 0);

    int exit_status = 0;

    if (words[1] != NULL && words[2] != NULL) {
        // { "exit", "word", "word", ... }
        fprintf(stderr, "exit: too many arguments\n");

    } else if (words[1] != NULL) {
        // { "exit", something, NULL }
        char *endptr;
        exit_status = (int) strtol(words[1], &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "exit: %s: numeric argument required\n", words[1]);
        }
    }

    exit(exit_status);
}

//
// Check whether this process can execute a file.  This function will be
// useful while searching through the list of directories in the path to
// find an executable file.
//
static int is_executable(char *pathname)
{
    struct stat s;
    return
        // does the file exist?
        stat(pathname, &s) == 0 &&
        // is the file a regular file?
        S_ISREG(s.st_mode) &&
        // can we execute it?
        faccessat(AT_FDCWD, pathname, X_OK, AT_EACCESS) == 0;
}

//
// Split a string 's' into pieces by any one of a set of separators.
//
// Returns an array of strings, with the last element being `NULL'.
// The array itself, and the strings, are allocated with `malloc(3)';
// the provided `free_token' function can deallocate this.
//
static char **tokenize(char *s, char *separators, char *special_chars)
{
    size_t n_tokens = 0;
    // Allocate space for tokens.  We don't know how many tokens there
    // are yet --- pessimistically assume that every single character
    // will turn into a token.  (We fix this later.)
    char **tokens = calloc((strlen(s) + 1), sizeof *tokens);
    assert(tokens != NULL);
    while (*s != '\0') {
        // We are pointing at zero or more of any of the separators.
        // Skip all leading instances of the separators.
        s += strspn(s, separators);
        // Trailing separators after the last token mean that, at this
        // point, we are looking at the end of the string, so:
        if (*s == '\0') {
            break;
        }
        // Now, `s' points at one or more characters we want to keep.
        // The number of non-separator characters is the token length.
        size_t length = strcspn(s, separators);
        size_t length_without_specials = strcspn(s, special_chars);
        if (length_without_specials == 0) {
            length_without_specials = 1;
        }
        if (length_without_specials < length) {
            length = length_without_specials;
        }
        // Allocate a copy of the token.
        char *token = strndup(s, length);
        assert(token != NULL);
        s += length;
        // Add this token.
        tokens[n_tokens] = token;
        n_tokens++;
    }

    // Add the final `NULL'.
    tokens[n_tokens] = NULL;
 
    // Finally, shrink our array back down to the correct size.
    tokens = realloc(tokens, (n_tokens + 1) * sizeof *tokens);
    assert(tokens != NULL);

    return tokens;
}


//
// Free an array of strings as returned by `tokenize'.
//
static void free_tokens(char **tokens)
{
    for (int i = 0; tokens[i] != NULL; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

///////////////////////////////////////////
//
//          MY Own Function
//
//////////////////////////////////////////


// function uesd for globFileName
char **globFileName(char **tokens){
    glob_t matches; // holds pattern expansion
    
    if (tokens[0] != NULL) {
        // global name for the top pattern
        glob(tokens[0], GLOB_NOCHECK|GLOB_TILDE, NULL, &matches);

    }
    // adding the rest filename after first one,
    for (int i  = 1; tokens[i] != NULL; i++ ) {
        glob(tokens[i], GLOB_NOCHECK|GLOB_TILDE|GLOB_APPEND, NULL, &matches);
    }
    // remanage the mermory
    tokens = calloc((matches.gl_pathc + 1), sizeof (char *));
    int i = 0;
    for (i = 0; i < matches.gl_pathc; i++) {
        // conneccting all the token patterns into together.
        tokens[i] = strdup(matches.gl_pathv[i]);
    }
    tokens[i] = NULL;
    //tokens = realloc(tokens, (i + 1) * sizeof *tokens);
    globfree(&matches);
    return tokens;
}
/////////////////////////////////
//
// QUEUE History adding
//
/////////////////////////////////
void saveCommandHistory(Queue* q) {    
    char Hisadd[1024];  
    strcpy(Hisadd, getenv("HOME"));
    strcat(Hisadd, ADDRESS);
    FILE *histf = fopen(Hisadd, "w");
    LinkStack* cell = q->front;
    while (cell != NULL ) {
        fprintf(histf, "%s",cell->data);
        cell = cell->next;
    }
    fclose(histf);
}

Queue* InitHistoryQueue() {
    char Hisadd[1024];  
    strcpy(Hisadd, getenv("HOME"));
    strcat(Hisadd, ADDRESS);
    Queue* q = (Queue*)malloc(sizeof(Queue));
    FILE *histf = fopen(Hisadd, "r");
    q->front = NULL;
    q->rear = NULL;
    if (histf != NULL) {
        char buffer[1024];
        char data[1024];

        while (fgets(buffer, 1024, histf) != NULL) {
            sscanf(buffer, "%[^\n]", data);
            strcat(data,"\n");
            ADDQueue(q, data);
            saveCommandHistory(q);
        }
        
        fclose(histf);
    }
    return q;
}
 
 
void ADDQueue(Queue* q, char *data) {
    LinkStack* cell = (LinkStack*)malloc(sizeof(LinkStack));
    cell->data = malloc(sizeof(char[1024]));
    strcpy(cell->data,data);
    cell->next = NULL;
    if (q->front == NULL) {
        q->front = cell;
    }
    if (q->rear == NULL) {
        q->rear = cell;
        cell->number = 0;
    }else {
        cell->number = q->rear->number +1;
        q->rear->next = cell;
        q->rear = cell;
    }
}
 
void PrintHistory(Queue* q, int number) {
    LinkStack* cell = q->front;
    while (cell != NULL ) {
        if (cell->number > (q->rear->number - number)) {
            printf("%d: %s",cell->number,cell->data);
        }
        cell = cell->next;
    }
}

char *getHistoryCmd(Queue* q,int number) {
    LinkStack* cell = q->front;
    while (cell != NULL ) {
        if (cell->number == number) {
            return(cell->data);
        }
        cell = cell->next;
    }
    return(NULL);
}
int getlastseqNo(Queue* q) {
    if (q->rear == NULL){
        return -1;
    }
    return q->rear->number;
}
/////////////////////////////////
//
// function used for posix_spawn
//
/////////////////////////////////

void write_out(char **words, char *path, char **environment,Queue* stack){
    int i;
    FILE *f1 = fopen(words[1], "r");

    for (i = 0; words[i+2] != NULL; i++) {
        words[i] = words[i+2];
    }
    words[i] = NULL;
    words[++i] = NULL;
    // create a pipe
    int pipe_file_descriptors[2];
    if (pipe(pipe_file_descriptors) == -1) {
        perror("pipe");
        return;
    }

    // create a list of file actions to be carried out on spawned process
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        perror("posix_spawn_file_actions_init");
        return;
    }

    // tell spawned process to close unused write end of pipe
    // without this - spawned process will not receive EOF
    // when write end of the pipe is closed below,
    // because spawned process also has the write-end open
    // deadlock will result
    if (posix_spawn_file_actions_addclose(&actions, pipe_file_descriptors[1]) != 0) {
        perror("posix_spawn_file_actions_init");
        return;
    }

    // tell spawned process to replace file descriptor 0 (stdin)
    // with read end of the pipe
    if (posix_spawn_file_actions_adddup2(&actions, pipe_file_descriptors[0], 0) != 0) {
        perror("posix_spawn_file_actions_adddup2");
        return;
    }


    // create a process running /usr/bin/sort
    // sort reads lines from stdin and prints them in sorted order

    pid_t pid;
    if (posix_spawn(&pid, path, &actions, NULL, words, environment) != 0) {
        perror("spawn");
        return;
    }

    // close unused read end of pipe
    close(pipe_file_descriptors[0]);

    // create a stdio stream from write-end of pipe
    FILE *f = fdopen(pipe_file_descriptors[1], "w");
    if (f == NULL) {
        perror("fdopen");
        return;
    }
    // send some input to the /usr/bin/sort process
    //sort with will print the lines to stdout in sorted order
    // Read contents from file
    int c = fgetc(f1);
    while (c != EOF)
    {
        fputc(c, f);
        c = fgetc(f1);
    }
    // close write-end of the pipe
    // without this sort will hang waiting for more input
    fclose(f);
    fclose(f1);

    int exit_status;
    if (waitpid(pid, &exit_status, 0) == -1) {
        perror("waitpid");
        return;
    }
    printf("%s exit status = %d\n",path, WEXITSTATUS(exit_status));

    // free the list of file actions
    posix_spawn_file_actions_destroy(&actions);

    return;


}

void read_in(char **words, char *path, char **environment,Queue* stack){
    //echo hello shuck >hello.txt
    FILE *f1;
    int overwrite = -1;
    if (strcmp(words[tokenLen(words)-3],">") == 0){
        overwrite = 1;
        f1 = fopen(words[tokenLen(words)-1], "a");
    } else {
        f1 = fopen(words[tokenLen(words)-1], "w");
    }
    if (overwrite == 1) {
        words[tokenLen(words)-3] = NULL;

    } else {
        words[tokenLen(words)-2] = NULL; 

    }

    //free(words[tokenLen(words)-1]);
    int pipe_file_descriptors[2];
    if (pipe(pipe_file_descriptors) == -1) {
        perror("pipe");
        return;
    }

    // create a list of file actions to be carried out on spawned process
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        perror("posix_spawn_file_actions_init");
        return;
    }

    // tell spawned process to close unused read end of pipe
    // without this - spawned process would not receive EOF
    // when read end of the pipe is closed below,
    if (posix_spawn_file_actions_addclose(&actions, pipe_file_descriptors[0]) != 0) {
        perror("posix_spawn_file_actions_init");
        return;
    }

    // tell spawned process to replace file descriptor 1 (stdout)
    // with write end of the pipe
    if (posix_spawn_file_actions_adddup2(&actions, pipe_file_descriptors[1], 1) != 0) {
        perror("posix_spawn_file_actions_adddup2");
        return;
    }

    pid_t pid;


    if (posix_spawn(&pid, path, &actions, NULL, words, environment) != 0) {
        perror("spawn");
        return;
    }

    // close unused write end of pipe
    // in some case processes will deadlock without this
    // not in this case, but still good practice
    close(pipe_file_descriptors[1]);

    // create a stdio stream from read end of pipe
    FILE *f = fdopen(pipe_file_descriptors[0], "r");
    if (f == NULL) {
        perror("fdopen");
        return;
    }

    // send some input to the /usr/bin/sort process
    //sort with will print the lines to stdout in sorted order
    // Read contents from file
    int c = fgetc(f);
    while (c != EOF)
    {
        fputc(c, f1);
        c = fgetc(f);
    }
    // close write-end of the pipe
    // without this sort will hang waiting for more input
    fclose(f);
    fclose(f1);

    int exit_status;
    if (waitpid(pid, &exit_status, 0) == -1) {
        perror("waitpid");
        return;
    }
    printf("%s exit status = %d\n",path, WEXITSTATUS(exit_status));

    // free the list of file actions
    posix_spawn_file_actions_destroy(&actions);
    return;
}

int tokenLen(char **tokens) {
    int i = 0;
    while (tokens[i] != NULL) {
        i++;
    }
    return i;
}