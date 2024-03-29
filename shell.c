#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFSIZE 4096 // max size of command
#define HOME_DIR getenv("HOME")
#define BEGINNING_PROMPT_LENGTH 8 // size of "1730sh:"

char buf[BUFFSIZE];
char prompt[100] = "1730sh:";
char * exec_args[BUFFSIZE];
char* array_without_redirects[BUFFSIZE];
char cd_args[1000];
pid_t pid;
int saved_stdout;
int saved_stdin;

int getPrompt();
int check_for_redirect(int exec_arg_size);
int begin_fork(char ** exec_args);
char* get_cd_args(char* args);

int main() {
    char * delimiters = " \n";

    // main program loop
    while(1) {
        int exec_arg_size = 0;
        char cmd[BUFFSIZE];
        for (int i = 0; i < BUFFSIZE; i++) {
            cmd[i] = '\0';
            exec_args[i] = '\0';
        } // for
        int prompt_size = getPrompt();
        write(STDOUT_FILENO, prompt, prompt_size);
        int cmd_bytes_read = read(STDIN_FILENO, cmd, BUFFSIZE);

        // check if there is no command
        if (cmd_bytes_read < 2) continue;

        char * tokenized_cmd = calloc(cmd_bytes_read, sizeof(char));
        tokenized_cmd = strtok(cmd, delimiters);
        int index = 0;
        while (tokenized_cmd != NULL) {
            exec_args[index] = tokenized_cmd;
            if (strcmp("exit", tokenized_cmd) == 0) {
                kill(0, SIGKILL);
                exit(0);
            } // if
            index++;
            exec_arg_size++;
            tokenized_cmd = strtok(NULL, delimiters);
        } // while
        if (check_for_redirect(exec_arg_size)) {
            begin_fork(array_without_redirects);
        } else {
            if (begin_fork(exec_args) < 0) printf("cd error\n");
        } // if

        // restore stdin/out to origional
        dup2(saved_stdout, STDOUT_FILENO);
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdout);
        close(saved_stdin);
    } // while

    return 0;
} // main

int getPrompt() {
    int homeLength = strlen(HOME_DIR);
    char * cwd = getcwd(buf, BUFFSIZE);
    int inHome = 1;
    int index = BEGINNING_PROMPT_LENGTH;

    for (int i = 0; i < homeLength; i++) {
        if (cwd[i] != HOME_DIR[i]) {
            inHome = 0;
            break;
        } // if
    } // for

    if (inHome) {
        // replace home with "~"
        prompt[7] = '~';
        for (int i = homeLength; i < strlen(cwd); i++) {
            prompt[index] = cwd[i];
            index++;
        } // for
    } else {
        for (int i = 0; i < strlen(cwd); i++) {
            prompt[i] = cwd[i];
            index++;
        } // for
    }
    prompt[index] = '$';
    prompt[index + 1] = ' ';
    return index + 2;
} // getPrompt


/**
 * Checks the command for redirects
 * @returns 1 if there is redirects, 0 if no redirects
 * Fills @code exec_args_without_redirects with
 * exec args, removing redirect operators.
 * @param exec_arg_size the size of the exec_arg array.
 */
int check_for_redirect(int exec_arg_size) {
    int nomatch = 1;
    int index = 0;
    int fd;

    saved_stdout = dup(STDOUT_FILENO);
    saved_stdin = dup(STDIN_FILENO);
    for (int i = 0; i < exec_arg_size; i++) {
        if (strcmp(exec_args[i], ">") == 0) { // redirect output
            nomatch = 0;
            fd = open(exec_args[i + 1], O_RDWR | O_CREAT | O_TRUNC, 0666);
            if (fd < 0) {
                printf("Error writing to file: %s\n", exec_args[i + 1]);
                break;
            } // if
            dup2(fd, STDOUT_FILENO);
        } else if (strcmp(exec_args[i], "<") == 0) { // redirect input
            nomatch = 0;
            fd = open(exec_args[i + 1], O_RDONLY, 0666);
            if (fd < 0) {
                printf("Error reading from file: %s\n", exec_args[i + 1]);
                break;
            } // if
            dup2(fd, STDIN_FILENO);
        } else if (strcmp(exec_args[i], ">>") == 0) { // redirect append output
            nomatch = 0;
            fd = open(exec_args[i + 1], O_WRONLY | O_APPEND | O_CREAT, 0666);
            if (fd < 0) {
                printf("Error writing to file: %s\n", exec_args[i + 1]);
                break;
            } // if
            dup2(fd, STDOUT_FILENO);
        } else if (nomatch) {
            array_without_redirects[index] = exec_args[i];
            index++;
        } // if
    } // for
    array_without_redirects[index + 1] = NULL;
    if (nomatch) {
        return 0; // no redirects in command
    } else {
        return 1; // redirects are in command
    } // if
} // redirect

/**
 * Forks process, child calls exec() while parent waits.
 * @returns 1 if success, 0 if fail.
 * @param exec_args the commands for the exec() call.
 */
int begin_fork(char ** exec_args) {
    if (strcmp(exec_args[0], "cd") == 0) {
        // change directory

        for (int i = 0; i < 1000; i++) {
            cd_args[i] = '\0';
        } // for

        if (exec_args[1] == NULL) {
            exec_args[1] = "..";
        } // if
        char* cd_path = get_cd_args(exec_args[1]);
        return chdir(cd_path);
    } // if

    pid = fork();
    if (pid < 0) { // fork error
        return -1;
    } // if
    if (pid == 0) { // in child
        execvp(exec_args[0], exec_args);
    } else { // in parent
        wait(NULL);
    } // if
    return 1;
} // begin_fork

char* get_cd_args(char* args) {
    for (int i = 0; i < strlen(args); i++) {
        if (args[i] == '.') {
            if (args[i + 1] == '.') {
                // case for '..'
                char * cwd_buf = calloc(256, sizeof(char));
                getcwd(cwd_buf, 256);
                int j;
                for (j = strlen(cwd_buf); j >= 0; j--) {
                    if (cwd_buf[j] == '/') break;
                } // for
                for (int k = 0; k < j; k++) {
                    cd_args[k] = cwd_buf[k];
                } // for
                free(cwd_buf);
                return cd_args;

            } else {
                if (i > 0 && args[i - 1] == '.') continue;
                // case for '.'
                char * cwd_buf = calloc(256, sizeof(char));
                getcwd(cwd_buf, 256);
                int j;
                // replace '.' with getenv("HOME")
                for (j = 0; j < strlen(cwd_buf); j++) {
                    cd_args[j] = cwd_buf[j];
                } // for

                // append args after '.'
                j++;
                for (int k = 2; k < strlen(args); k++, j++) {
                    cd_args[j] = args[k];
                } // for
            } // if

        } else if (args[i] == '~') {
            // case for '~'
            char * home = getenv("HOME");
            int j;
            for (j = 0; j < strlen(home); j++) {
                // replaces ~ with home
                cd_args[j] = home[j];
            } // for
            j++;
            for (int k = 1; k < strlen(args); k++, j++) {
                // appends the rest of args after "~" to cd_args
                cd_args[j] = args[k];
            } // for

        } else {
            // no special cd characters used
            return args;
        } // if

    } // for
    return cd_args;
} // get_cd_args
