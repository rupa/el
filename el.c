/*
 * el: fuzzy wrapper for $EDITOR
 *
 * cc -Wall -lreadline el.c
 * cc -Wall -DNO_READLINE el.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <regex.h>

#ifndef NO_READLINE
#include <readline/readline.h>
#endif

#define NUM_FILENAMES 1000
#define MAX_READ 512

void use(char * name) {
    printf("%s [-ab] r1 ... rn\n", name);
}

char* rtrim(char *string) {
    char *ws;
    while( (ws = strrchr(string,' ')) ) {
        if( ws-string != (int)(strlen(string)-1) ) break;
        string[ws-string] = '\0';
    }
    return string;
}

int magnitude(int num) {
    /* strlen(int) */
    int mag = 0;
    while (num > 0) {
        mag++;
        num /= 10;
    }
    return mag;
}

int isbin(char *filename) {
    /* implement perl's -T test */
    int ch, tot, hi, bin;
    FILE *file;
    file = fopen( filename, "r" );
    if( file == 0 ) return -1;
    tot = hi = bin = 0;
    while( (ch = fgetc(file) ) != EOF && (++tot < MAX_READ) ) {
        if( '\0' == ch ) {
            bin = 1;
            break;
        }
        if( (ch < 32 || ch > 127) && (ch < 8 || ch > 10) && ch != 13 ) hi++;
    }
    if( bin != 1 && (hi / MAX_READ) > .30 ) bin = 1;
    fclose(file);
    return bin;
}  

char* parse(char* buff, char** files, int numf) {
    /* munge up our input */
    long n;
    char *end_ptr;
    n = strtol(buff, &end_ptr, 10);
    if( n > 0 && n <= numf && '\0' == *end_ptr ) {
        strncpy(buff, files[n - 1], strlen(files[n - 1]));
    } 
    return buff;
}

char** getfiles(int all, int bin, int idx, int numa, char** args, int* numf) {
    /* return a list of files */
    DIR *d;
    struct dirent *dir;
    struct stat statbuf;
    int i, nok;
    regex_t* re = (regex_t*)malloc(sizeof(regex_t) * (numa-idx));
    char** files = (char**)malloc(sizeof(char*) * NUM_FILENAMES);
    if( numa - idx ) {
        for( i=idx; i<numa; i++ ) {
            regcomp(&re[i-idx], args[i], REG_EXTENDED);
        }
    }
    *numf = 0;
    d = opendir(".");
    if( d ) {
        while( (dir = readdir(d)) != NULL && *numf <= NUM_FILENAMES) {
            nok = 0;
            if( (stat(dir->d_name, &statbuf) == -1 ) ) {
                continue;
            } else if( S_ISDIR(statbuf.st_mode) ) {
                continue;
            } else if( !all && dir->d_name[0] == '.' ) {
                continue;
            } else if( !bin && isbin(dir->d_name) ) {
                continue; 
            } else if( numa - idx ) {
                for( i=idx; i<numa; i++ ) {
                    if( regexec(&re[i-idx], dir->d_name, 0, NULL, 0) ) {
                        nok = 1;
                        break;
                    } 
                }
                if( nok ) continue;
            }
            files[*numf] = (char*)malloc((strlen(dir->d_name)+1) * sizeof(char));
            strcpy(files[*numf], dir->d_name);
            ++(*numf);
        }
        closedir(d);
    }
    return files;
}

char* pickfile(char** files, int numf) {
    /* pick a file from a list of 'em */
    int i, j;
    static char buff[_POSIX_PATH_MAX] = "";
    char *prompt;

    j = magnitude(numf);
    prompt = malloc((j+2) * sizeof(char));
    sprintf(prompt, "%*s> ", j, "");

    for( i=0; i<numf; i++ ) {  
        printf("%*d: %s\n", j, i + 1, files[i]);
    }
    if( numf == NUM_FILENAMES ) {
        printf("\n* limit %d reached. *\n", NUM_FILENAMES);
    }

    #ifdef NO_READLINE
        printf("%s", prompt);
        fgets(buff, sizeof(buff) - 1, stdin);
        buff[strlen(buff) - 1] = '\0';
    #else
        strncpy(buff, readline(prompt), sizeof(buff) - 1);
        buff[sizeof(buff) - 1] = '\0';
    #endif

    if( !strcmp(buff, "") ) return "";
       
    return parse(rtrim(buff), files, numf);
}

int main(int argc, char *argv[]) {
    int all, bin, i, numf;
    char toopen[_POSIX_PATH_MAX] = "";
    char *editor;
    char** files;

    editor = getenv("EDITOR");
    if( editor == NULL ) editor = "vi";

    all = bin = 0;
    opterr = 1;
    while ((i = getopt(argc, argv, "abh")) != -1) {
        switch (i) {
            case 'a':
                all = 1;
                break;
            case 'b':
                bin = 1;
                break;
            case 'h':
                use(argv[0]);
                return 0;
            case '?':
                break;
            default:
                abort();
        }
    }

    files = getfiles(all, bin, optind, argc, argv, &numf);

    if( numf == 1 ) {
        strncpy(toopen, files[0], sizeof(toopen));
    } else {
        strncpy(toopen, pickfile(files, numf), sizeof(toopen));
    }

    if( !strcmp(toopen, "") ) {
        return 0;
    }
    return execlp(editor, editor, toopen, NULL);
}
