/*
 * el: fuzzy wrapper for $EDITOR
 *
 * cc -Wall el.c
 * cc -Wall -DWITH_READLINE -lreadline el.c
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

#ifdef WITH_READLINE
#include <readline/readline.h>
#endif

#define NUM_FILENAMES 1000
#define MAX_READ 512

int magnitude(int num) {
    int mag = 0;
    while (num > 0) {
        mag++;
        num /= 10;
    }
    return mag;
}

int isbin(char *filename) {
    /* perl's -T test */
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

char** getfiles(int all, int bin, int idx, int numa, char** args, int* numf) {
    DIR *d;
    struct dirent *dir;
    struct stat statbuf;
    int i, nok;
    regex_t *re = (regex_t*)malloc(sizeof(regex_t) * (numa-idx));
    char** files = (char**) malloc(sizeof(char*) * NUM_FILENAMES);

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

int main(int argc, char *argv[]) {
    int all, bin, i, numf;
    unsigned int j;
    long isnum;
    char buff[_POSIX_PATH_MAX + 2] = "";
    char *editor, *end_ptr, *toopen, *prompt;
    char** files;

    editor = getenv("EDITOR");
    if( editor == NULL ) editor = "vi";

    all = bin = 0;
    opterr = 1;
    while ((i = getopt(argc, argv, "ab")) != -1) {
        switch (i) {
            case 'a':
                all = 1;
                break;
            case 'b':
                bin = 1;
                break;
            case '?':
                break;
            default:
                abort();
        }
    }
    files = getfiles(all, bin, optind, argc, argv, &numf);

    if( numf == 1 ) {
        toopen = files[0];
    } else {
        j = magnitude(numf);
        sprintf(prompt, "%*s> ", j, "");
        for( i=0; i<numf; i++ ) {  
            printf("%*d: %s\n", j, i + 1, files[i]);
        }
        if( numf == NUM_FILENAMES ) {
            printf("\n* limit %d reached. *\n", NUM_FILENAMES);
        }

        #ifdef WITH_READLINE
            strncpy(buff, readline(prompt), _POSIX_PATH_MAX + 1);
            toopen = buff;
            if( !strcmp(toopen, "") ) return 0;
        #else
            printf(prompt);
            fgets(buff, sizeof(buff), stdin);
            toopen = buff;
            if( !strcmp(toopen, "\n") ) return 0;
            toopen[strlen(toopen)-1] = '\0';
        #endif
       
        while( (end_ptr=strrchr(toopen,' ')) ) {
            j = strlen(toopen);
            if( end_ptr-toopen != j-1 ) break;
            toopen[end_ptr-toopen] = '\0';
        }
        isnum = strtol(toopen, &end_ptr, 10);
        if( isnum > 0 && isnum <= i && '\0' == *end_ptr ) {
            printf("%d", *end_ptr);
            toopen = files[isnum - 1];
        } 
    }

    return execlp(editor, editor, toopen, NULL);
}
