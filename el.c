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
    printf("use: %s [-abdhitv] r1 ... rn\n", name);
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

static int compare(const void *a, const void *b) { 
    return strcmp(*(char **)a, *(char **)b);
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
    if( tot && bin != 1 && (hi / tot) > .30 ) bin = 1;
    fclose(file);
    return bin;
}  

int itofl(char * tok, char** toks, int *nt, char ** flst, int nf) {
    long n;
    char *end_ptr;
    n = strtol(tok, &end_ptr, 10);
    if( n > 0 && n <= nf && '\0' == *end_ptr ) {
        /* substitute from list */
        toks[*nt] = (char *)malloc(strlen(flst[n - 1]) + 1 * sizeof(char));
        strcpy(toks[*nt], flst[n - 1]);
    } else {
        /* leave alone */
        toks[*nt] = (char *)malloc(strlen(tok)+1 * sizeof(char));
        strcpy(toks[*nt], tok);
    } 
    (*nt)++;
    return 0;
}

int parse(char* str, char** toks, int *nt, char** flst, int nf, char* cmd) {
    /* split into tokens, account for ' and ", deal with special ! commands,
     * replace token with flst[token] if it exists, return a list of strings
     * suitable for exec.
     */
    int i, j, quoted;
    char *result = NULL;
    char** tmp = (char**)malloc(sizeof(char*) * sizeof(str));
     
    i = j = *nt = 0;
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
            if( !(*nt) ) {
                /* insert default: cmd */
                toks[*nt] = (char *)malloc(sizeof(cmd) * sizeof(char));
                strcpy(toks[*nt], cmd);
                (*nt)++;
            }
            toks[*nt] = (char *)malloc(strlen(tmp[j])+1 * sizeof(char));
            strcpy(toks[*nt], tmp[j]);
            (*nt)++;
            quoted = 0;
        } else {
            result = strtok(tmp[j], " ");
            if( !(*nt) ) {
                if( !strcmp(result, "!" ) ) {
                    result = strtok(NULL, " ");
                    if( result == NULL ) {
                        free(tmp); 
                        return 1;
                    }
                    itofl(result, toks, nt, flst, nf);
                } else if( !strncmp(result, "!", 1) ) {
                    itofl(result+1, toks, nt, flst, nf);
                } else {
                    /* insert default: cmd */
                    toks[*nt] = (char *)malloc(sizeof(cmd) * sizeof(char));
                    strcpy(toks[*nt], cmd);
                    (*nt)++;
                    itofl(result, toks, nt, flst, nf);
                }
                result = strtok(NULL, " ");
            }
            while( result != NULL ) {
                itofl(result, toks, nt, flst, nf);
                result = strtok(NULL, " ");
            }
            quoted = 1;
        }
    }
    toks[(*nt)++] = NULL;
    free(tmp); 
    return 0;
}

int pickfile(char** toks, int * nt, char** files, int nf, char* cmd) {
    /* pick a file from a list of 'em */
    int i, j;
    static char buff[NAME_MAX+ 1] = "";
    char *prompt;

    j = magnitude(nf);
    prompt = malloc((j+3) * sizeof(char));
    sprintf(prompt, "%*s: ", j, "");

    for( i=0; i<nf; i++ ) {  
        printf("%*d: %s\n", j, i + 1, files[i]);
    }
    if( nf == NUM_FILENAMES ) {
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

    if( parse(buff, toks, nt, files, nf, cmd) ) return 1; 
   
    free(prompt);
    return 0;
}

char** getfiles(int all, int bin, int dirs, int v, regex_t* re, int nr, int* nf) {
    /* return a list of files */
    DIR *d;
    struct dirent *dir;
    struct stat statbuf;
    int i, nok;
    char** files = (char**)malloc(sizeof(char*) * NUM_FILENAMES);

    *nf = 0;
    d = opendir(".");
    if( d ) {
        while( (dir = readdir(d)) != NULL && *nf < NUM_FILENAMES) {

            if( stat(dir->d_name, &statbuf) == -1 ) continue; 
            if( !strcmp(dir->d_name, ".") ) continue;
            if( !strcmp(dir->d_name, "..") ) continue;
            if( !all && dir->d_name[0] == '.' ) continue;
            if( dirs && S_ISDIR(statbuf.st_mode) ) {
                strcat(dir->d_name, "/");
            } else if( S_ISDIR(statbuf.st_mode) ) continue;
            if( nr ) {
                nok = v == 1;
                for( i=0; i<nr; i++ ) {
                    if( regexec(&re[i], dir->d_name, 0, NULL, 0) ) {
                        nok = !v;
                        break;
                    } 
                }
                if( nok ) continue;
            }
            if( !bin && isbin(dir->d_name) ) continue;

            files[*nf] = (char*)malloc(strlen(dir->d_name) + 1 * sizeof(char));
            strcpy(files[*nf], dir->d_name);
            ++(*nf);
        }
        closedir(d);
    }
    return files;
}

int main(int argc, char *argv[]) {
    int all, bin, dirs, icas, inv, test, i, nf, nt;
    char *cmd;
    char** files;
    char** toks = (char**)malloc(sizeof(char*) * NAME_MAX + 1);
    regex_t* re;

    cmd = getenv("EDITOR");
    if( cmd == NULL ) cmd = "vi";

    all = bin = dirs = icas = inv = test = 0;
    opterr = 1;
    while ((i = getopt(argc, argv, "abdhitv")) != -1) {
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
            case 'i':
                icas = 1;
                break;
            case 't':
                test = 1;
                break;
            case 'v':
                inv = 1;
                break;
            case '?':
                break;
            default:
                abort();
        }
    }

    re  = (regex_t*)malloc((argc - optind) * sizeof(regex_t));
    if( argc - optind ) {
        int flags = REG_EXTENDED | REG_NOSUB;
        if( icas ) flags = flags | REG_ICASE;
        for( i=optind; i<argc; i++ ) regcomp(&re[i-optind], argv[i], flags);
    }
    files = getfiles(all, bin, dirs, inv, re, (i-optind), &nf);
    free(re);
    qsort(files, nf, sizeof(char *), compare);

    if( nf == 1 ) {
        nt = 0;
        toks[nt] = (char *)malloc(strlen(cmd) + 1 * sizeof(char));
        strcpy(toks[nt++], cmd);
        toks[nt] = (char *)malloc(strlen(files[0]) + 1 * sizeof(char));
        strcpy(toks[nt++], files[0]);
        toks[nt++] = NULL;
    } else if( pickfile(toks, &nt, files, nf, cmd) ) return 0;

    if( test ) {
        printf("%s ", "[");
        for( i=0; i<nt; i++ ) {
            printf("\"%s\", ", toks[i]);
        }
        printf("%s", "]\n");
    } else return execvp(toks[0], toks);

    return 0;
}
