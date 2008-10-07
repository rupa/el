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
    printf("use: %s [-abdt] r1 ... rn\n", name);
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
    file = fopen(filename, "r");
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

int itofl(char * tok, char** toks, int *numtok, char ** flst, int numf) {
    long n;
    char *end_ptr;
    n = strtol(tok, &end_ptr, 10);
    if( n > 0 && n <= numf && '\0' == *end_ptr ) {
        /* substitute from list */
        toks[*numtok] = (char *)malloc(strlen(flst[n - 1]) + 1 * sizeof(char));
        strcpy(toks[*numtok], flst[n - 1]);
    } else {
        /* leave alone */
        toks[*numtok] = (char *)malloc(strlen(tok)+1 * sizeof(char));
        strcpy(toks[*numtok], tok);
    } 
    (*numtok)++;
    return 0;
}

int parse(char* str, char** toks, int *numtok, char** flst, int numf, char* editor) {
    /* split into tokens, account for ' and ", deal with special ! commands,
     * replace token with flst[token] if it exists, return a list of strings
     * suitable for exec.
     */
    int i, j, quoted;
    char *result = NULL;
    char** tmp = (char**)malloc(sizeof(char*) * sizeof(str));
     
    i = j = *numtok = 0;
    quoted = !strncmp(str,"\"", 1);

    result = strtok(str, "'\"");
    while( result != NULL ) {
        tmp[i] = (char *)malloc(strlen(result)+1 * sizeof(char));
        strcpy(tmp[i], result);
        result = strtok(NULL, "'\"");
        i++;
    }
    for( j=0; j<i; j++ ) { 
        if( quoted ) {
            if( !(*numtok) ) {
                /* insert default: editor */
                toks[*numtok] = (char *)malloc(sizeof(editor) * sizeof(char));
                strcpy(toks[*numtok], editor);
                (*numtok)++;
            }
            toks[*numtok] = (char *)malloc(strlen(tmp[j])+1 * sizeof(char));
            strcpy(toks[*numtok], tmp[j]);
            (*numtok)++;
            quoted = 0;
        } else {
            result = strtok(tmp[j], " ");
            if( !(*numtok) ) {
                if( !strcmp(result, "!" ) ) {
                    result = strtok(NULL, " ");
                    if( result == NULL ) {
                        free(tmp); 
                        return 1;
                    }
                    itofl(result, toks, numtok, flst, numf);
                } else if( !strncmp(result, "!", 1) ) {
                    itofl(result+1, toks, numtok, flst, numf);
                } else {
                    /* insert default: editor */
                    toks[*numtok] = (char *)malloc(sizeof(editor) * sizeof(char));
                    strcpy(toks[*numtok], editor);
                    (*numtok)++;
                    itofl(result, toks, numtok, flst, numf);
                }
                result = strtok(NULL, " ");
            }
            while( result != NULL ) {
                itofl(result, toks, numtok, flst, numf);
                result = strtok(NULL, " ");
            }
            quoted = 1;
        }
    }
    toks[(*numtok)++] = NULL;
    free(tmp); 
    return 0;
}

char** getfiles(int all, int bin, int dirs, int idx, int numa, char** args, int* numf) {
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

            if( (stat(dir->d_name, &statbuf) == -1) ||
                (!all && dir->d_name[0] == '.') ||
                (!bin && isbin(dir->d_name)) ) continue;

            if( dirs && S_ISDIR(statbuf.st_mode) ) {
                if( !strcmp(dir->d_name, ".") ||
                    !strcmp(dir->d_name, "..") ) continue;
                strcat(dir->d_name, "/");
            } else if( S_ISDIR(statbuf.st_mode) ) continue;

            if( numa - idx ) {
                nok = 0;
                for( i=idx; i<numa; i++ ) {
                    if( regexec(&re[i-idx], dir->d_name, 0, NULL, 0) ) {
                        nok = 1;
                        break;
                    } 
                }
                if( nok ) continue;
            }

            files[*numf] = (char*)malloc(strlen(dir->d_name) + 1 * sizeof(char));
            strcpy(files[*numf], dir->d_name);
            ++(*numf);
        }
        closedir(d);
    }
    free(re);
    return files;
}

int pickfile(char** toks, int * numtok, char** files, int numf, char* editor) {
    /* pick a file from a list of 'em */
    int i, j;
    static char buff[NAME_MAX+ 1] = "";
    char *prompt;

    j = magnitude(numf);
    prompt = malloc((j+3) * sizeof(char));
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

    if( !strcmp(buff, "") ) return 1;

    if( parse(buff, toks, numtok, files, numf, editor) ) return 1; 
   
    free(prompt);
    return 0;
}

int main(int argc, char *argv[]) {
    int all, bin, dirs, test, i, numf, numtok;
    char** files;
    char** toks = (char**)malloc(sizeof(char*) * NAME_MAX + 1);
    char *editor;

    editor = getenv("EDITOR");
    if( editor == NULL ) editor = "vi";

    all = bin = dirs = test = 0;
    opterr = 1;
    while ((i = getopt(argc, argv, "abdth")) != -1) {
        switch (i) {
            case 'a':
                all = 1;
                break;
            case 'b':
                bin = 1;
                break;
            case 'd':
                dirs = 1;
                break;
            case 'h':
                use(argv[0]);
                return 0;
            case 't':
                test = 1;
                break;
            case '?':
                break;
            default:
                abort();
        }
    }

    files = getfiles(all, bin, dirs, optind, argc, argv, &numf);

    if( numf == 1 ) {
        toks[numtok] = (char *)malloc(sizeof(editor) * sizeof(char));
        strcpy(toks[numtok++], editor);
        toks[numtok] = (char *)malloc(sizeof(files[0]) * sizeof(char));
        strcpy(toks[numtok++], files[0]);
        toks[numtok++] = NULL;
    } else if( pickfile(toks, &numtok, files, numf, editor) ) return 0;

    if( test ) {
        printf("%s ", "[");
        for( i=0; i<numtok; i++ ) {
            printf("\"%s\", ", toks[i]);
        }
        printf("%s", "]\n");
    } else return execvp(toks[0], toks);

    return 0;
}
