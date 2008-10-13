/*
 * el: fuzzy wrapper for $EDITOR
 *
 * compile:
 *     cc -Wall -lreadline -lncurses el.c
 *     cc -Wall -DNO_READLINE el.c
 *
 * use:
 *     el [-abdhitv] [regex1 regex2 ... regexn]
 *         -a show hidden files
 *         -b show binary files
 *         -d show directories
 *         -h print this help message
 *         -i matching is case insensitive
 *         -t test - print cmd that would be run
 *         -v only show files that don't match regexes
 *
 * If only one file matches, opens $EDITOR on that file, else displays a list
 * and shows a prompt. Enter a list number to open that file. Other input is 
 * considered arguments to $EDITOR. If the first char entered at the prompt
 * is !, the entire line is passed to the shell as a command. List numbers are
 * always replaced by the corresponding file name before being passed to the
 * shell.
 *
 * Example:
 *     $ el
 *     1: Makefile
 *     2: README
 *     3: el.c
 *      : !echo 2 3
 *     el.c shell.c
 *     $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#ifndef NO_READLINE
#include <readline/readline.h>
#endif

#define NUM_FILENAMES 1000
#define MAX_READ 512

void use(char * name) {
    printf("\
use: %s [-abdhiptv] [regex1 regex2 ... regexn]     \n\
        -a show hidden files                       \n\
        -b show binary files                       \n\
        -d show directories                        \n\
        -h print this help message                 \n\
        -i matching is case insensitive            \n\
        -t test - print cmd that would be run      \n\
        -v only show files that don't match regexes\n", name);
}

int lastedited(char *cmd, int test ) {
    /* vi only */
    if( test ) {
       printf("[ \"%s\", \"-c\", \"normal '0\", \"(null)\", ]\n", cmd);
    } else execlp(cmd, cmd, "-c", "normal '0", NULL);
    return 0;
}

int magnitude(int num) {
    /* strlen(int) */
    int mag;
    for( mag=0; num > 0; (num /= 10) ) mag++;
    return mag;
}

static int compare(const void *a, const void *b) { 
    return strcmp(*(char **)a, *(char **)b);
}

int isbin(char *filename) {
    /* implement perl's -T test */
    int ch, bin;
    float hi, tot;
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

int itofl(char *tok, char** toks, int *nt, char** flst, int nf) {
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
    /* split into tokens, account for ", deal with special ! commands,
     * replace token with flst[token] if it exists, return a list of strings
     * suitable for exec.
     */
    int i, j, quoted;
    char *result = NULL;
    char** tmp = (char**)malloc(sizeof(char*) * sizeof(str));
    char *tmp2;
     
    i = j = *nt = 0;
    quoted = !strncmp(str,"\"", 1);

    result = strtok(str, "\"");
    tmp2 = (char *)malloc(strlen(str) + 1 * sizeof(char));
    /* deal with double quotes */
    while( result != NULL ) {
        strcpy(tmp2, "");
        while( result != NULL && result[strlen(result) - 1] == '\\' ) {
            /* which might be escaped doublequote */
            result[strlen(result) - 1] = '"';
            strcat(tmp2, result);
            result = strtok(NULL, "\"");
        }
        tmp[i] = (char *)malloc((strlen(tmp2) + (result == NULL? 0 : strlen(result)) + 1) * sizeof(char));
        strcpy(tmp[i], tmp2);
        if( result != NULL ) strcat(tmp[i], result);
        result = strtok(NULL, "\"");
        i++;
    }
    free(tmp2);
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
            result = strtok(tmp[j], " \t");
            if( !(*nt) ) {
                if( !strcmp(result, "!" ) ) {
                    result = strtok(NULL, " \t");
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
                result = strtok(NULL, " \t");
            }
            while( result != NULL ) {
                itofl(result, toks, nt, flst, nf);
                result = strtok(NULL, " \t");
            }
            quoted = 1;
        }
    }
    toks[(*nt)++] = NULL;
    free(tmp); 
    return 0;
}

int listfiles(char** files, int nf, int fmax, int pad ) {
    int i, j, ncol, nrow, scol;
    struct winsize ws;
    ioctl(1, TIOCGWINSZ, &ws);

    scol = fmax + pad + 4;
    ncol = (ws.ws_col / scol) ? (ws.ws_col / scol) : 1;
    nrow = (nf / ncol) + ((nf % ncol) ? 1 : 0); 

    /* one per line
     * 
     * for( i=0; i<nf; i++ ) printf("%*d: %s\n", pad-4, i+1, files[i]);
     */ 

    /* hsort
     * 
     * for( i=0; i<nrow; i++ ) {
     *     for( j=0; j<ncol; j++ ) {
     *         if(i*ncol+j >= nf ) break;
     *         printf("%*d: %-*s  ", pad-4, i*ncol+j+1, scol-4, files[i*ncol+j]);
     *     }
     *     printf("\n");
     * }
     */

    /* vsort */
    for( i=0; i<nrow; i++ ) {
        for( j=0; j<ncol; j++ ) {
            if(j*nrow+i >= nf ) break;
            printf("%*d: %-*s", pad, j*nrow+i+1, ((j<ncol-1) ? fmax+2 : 0), files[j*nrow+i]);
        }
        printf("\n");
    }

    if( nf >= NUM_FILENAMES ) {
        printf("\n* limit %d reached. *\n", NUM_FILENAMES);
    }

    return 0;
}

int pickfile(char** toks, int * nt, char** files, int nf, int fmax, char* cmd) {
    /* pick a file from a list of 'em */
    int i;
    static char buff[NAME_MAX+ 1] = "";
    char *prompt;

    i = magnitude(nf);
    prompt = malloc((i+3) * sizeof(char));
    sprintf(prompt, "%*s: ", i, "");

    listfiles(files, nf, fmax, i);

    #ifdef NO_READLINE
        printf("%s", prompt);
        fgets(buff, sizeof(buff) - 1, stdin);
        buff[strlen(buff) - 1] = '\0';
    #else
        strncpy(buff, readline(prompt), sizeof(buff) - 1);
        buff[sizeof(buff) - 1] = '\0';
    #endif

    free(prompt);
    if( !strcmp(buff, "") ) return 1;
    if( parse(buff, toks, nt, files, nf, cmd) ) return 1; 

    return 0;
}

char** getfiles(int all, int bin, int dirs, int v, regex_t* re, int nr, int* nf, int* fmax) {
    /* return a list of files */
    DIR *d;
    struct dirent *dir;
    struct stat statbuf;
    int i, nok;
    char** files = (char**)malloc(sizeof(char*) * NUM_FILENAMES);

    *nf = *fmax = 0;
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
                nok = 0;
                for( i=0; i<nr; i++ ) {
                    if( v ^ regexec(&re[i], dir->d_name, 0, NULL, 0) ) {
                        nok = 1;
                        break;
                    }
                }
                if( nok ) continue;
            }
            if( !bin && isbin(dir->d_name) ) continue;

            if( (int)strlen(dir->d_name) > *fmax ) *fmax = strlen(dir->d_name);

            files[*nf] = (char*)malloc(strlen(dir->d_name) + 1 * sizeof(char));
            strcpy(files[*nf], dir->d_name);
            ++(*nf);
        }
        closedir(d);
    }

    return files;
}

int main(int argc, char *argv[]) {
    int all, bin, dirs, fmax, icas, inv, test, i, nf, nr, nt, r;
    char *cmd;
    char err[NAME_MAX];
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
    nr = 0;
    if( argc - optind ) {
        int flags = REG_EXTENDED | REG_NOSUB;
        if( icas ) flags = flags | REG_ICASE;
        icas = 0;
        r = 0;
        for( i=optind; i<argc; i++ ) {
            if( !strcmp(argv[i], "/") && !strncmp(cmd, "vi", 2) ) {
                /* reopen last file */
                lastedited(cmd, test);
                return 0;
            } else if( argv[i][0] == '/' ) {
                /* set numbered file */
                if( atoi(argv[i]+1) ) icas = atoi(argv[i]+1);
            } else if( (r = regcomp(&re[nr], argv[i], flags)) ) {
                regerror(r, &re[nr], err, 80 );
                printf("%s\n", err);
            } else nr++;
        }
    }

    files = getfiles(all, bin, dirs, inv, re, nr, &nf, &fmax);
    free(re);
    qsort(files, nf, sizeof(char *), compare);

    if( icas && icas <= nf ) {
        /* open numbered file */
        nf = 1;
        files[0] = files[icas - 1];
    }
    if( nf == 1 ) {
        /* single match */
        nt = 0;
        toks[nt] = (char *)malloc(strlen(cmd) + 1 * sizeof(char));
        strcpy(toks[nt++], cmd);
        toks[nt] = (char *)malloc(strlen(files[0]) + 1 * sizeof(char));
        strcpy(toks[nt++], files[0]);
        toks[nt++] = NULL;
    } else if( pickfile(toks, &nt, files, nf, fmax, cmd) ) return 0;

    if( test ) {
        printf("[ ");
        for( i=0; i<nt; i++ ) printf("\"%s\", ", toks[i]);
        printf("]\n");
    } else return execvp(toks[0], toks);

    return 0;
}
