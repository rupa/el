/*
 * el: mnemonic wrapper for $EDITOR
 * by rupa@lrrr.us 2008
 *
 * COMPILE:
 *     cc -Wall -lreadline -lncurses el.c
 *     cc -Wall -DNO_READLINE el.c
 *
 * USE:
 *     el [-abdhitv] [regex1 regex2 ... regexn]
 *         -a show hidden files
 *         -b show binary files
 *         -d show directories
 *         -h print this help message
 *         -i matching is case insensitive
 *         -t test - print cmd that would be run
 *         -v only show files that don't match regexes
 *         -V some info
 *
 * DESCRIPTION:
 *     If only one file matches, opens $EDITOR on that file, else displays a
 * list and shows a prompt. Enter a list number to open that file. Other input
 * is considered arguments to $EDITOR.
 *     If the first nonoption argument end in / and is a valid directory, list
 * files there instead of $PWD.
 *     If the first char entered at the prompt is !, the entire line is passed
 * to the shell as a command. List numbers are replaced by the corresponding
 * file name before being passed to the shell. An attempt is made to handle
 * escapes and double quoting correctly. If compiled with readline support, 
 * tab completion on the prompt list works.
 *
 * EXAMPLE:
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

#define NUM_FILENAMES 2000
#define MAX_READ 512

static char** files;
static int nf;

void use(char * name) {
    printf("use: %s [-abdhil:ptvV] [dir/] [regex1 regex2 ... regexn]\n", name);
    printf("\
        -a show hidden files                            \n\
        -b show binary files                            \n\
        -d show directories                             \n\
        -h print this help message                      \n\
        -i matching is case insensitive                 \n\
        -l use the locate call to get file list         \n\
        -t test - print cmd that would be run           \n\
        -v only show files that don't match regexes     \n\
        -V some info\n");
}

#ifndef NO_READLINE
/* custom readline completion */
char * dupstr (char* s) {
  char *r;
  r = (char*)malloc((strlen (s) + 1));
  strcpy(r, s);
  return(r);
}

char* my_generator(const char* text, int state) {
    static int idx, len;
    char *name;
    if( !state ) {
        len = strlen(text);
        idx = 0;
    }
    while( idx < nf && (name = files[idx++]) ) {
        if( strncmp(name, text, len) == 0 ) return( dupstr(name) );
    }
    return((char *)NULL);
}

static char** my_completion( const char * text , int start,  int end) {
    char **matches;
    matches = (char **)NULL;
    if( start == 0 ) {
        matches = rl_completion_matches((char*)text, &my_generator);
    } else rl_bind_key('\t',rl_abort);
    return( matches );
    end = 0;
}
#endif

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
    file = fopen(filename, "rb");
    if( file == 0 ) return -1;
    tot = hi = bin = 0;
    while( (ch = fgetc(file) ) != EOF && (++tot < MAX_READ) ) {
        if( '\0' == ch ) {
            bin = 1;
            break;
        }
        if( (ch < 32 || ch > 127) && (ch < 8 || ch > 10) && ch != 13 ) hi++;
    }
    if( tot && !bin && (hi / tot) > .30 ) bin = 1;
    fclose(file);
    return bin;
}  

int itofl(char *tok, char** toks, int *nt) {
    long n;
    char *end_ptr;
    n = strtol(tok, &end_ptr, 10);
    if( n > 0 && n <= nf && '\0' == *end_ptr ) {
        /* substitute from list */
        toks[*nt] = (char *)malloc(strlen(files[n - 1]) + 1 * sizeof(char));
        strcpy(toks[*nt], files[n - 1]);
    } else {
        /* leave alone */
        toks[*nt] = (char *)malloc(strlen(tok) + 1 * sizeof(char));
        strcpy(toks[*nt], tok);
    } 
    (*nt)++;
    return 0;
}

int parse(char* str, char** toks, int *nt, char* cmd) {
    /* split into tokens, account for ", deal with special ! commands,
     * replace token with files[token] if it exists, return a list of strings
     * suitable for exec.
     */
    int i, j, quoted;
    char *result = NULL;
    char** tmp = (char**)malloc(sizeof(char*) * sizeof(str));
    char *tmp2;
     
    i = j = *nt = 0;
    quoted = !strncmp(str,"\"", 1);

    result = strtok(str, "\"");
    if( result == NULL ) return 1;
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
                toks[*nt] = (char *)malloc(strlen(cmd) + 1 * sizeof(char));
                strcpy(toks[*nt], cmd);
                (*nt)++;
            }
            toks[*nt] = (char *)malloc(strlen(tmp[j]) + 1 * sizeof(char));
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
                    itofl(result, toks, nt);
                } else if( !strncmp(result, "!", 1) ) {
                    itofl(result + 1, toks, nt);
                } else {
                    /* insert default: cmd */
                    toks[*nt] = (char *)malloc(strlen(cmd) + 1 * sizeof(char));
                    strcpy(toks[*nt], cmd);
                    (*nt)++;
                    itofl(result, toks, nt);
                }
                result = strtok(NULL, " \t");
            }
            while( result != NULL ) {
                itofl(result, toks, nt);
                result = strtok(NULL, " \t");
            }
            quoted = 1;
        }
    }
    toks[(*nt)++] = NULL;
    free(tmp); 
    return 0;
}

int listfiles(int fmax, int pad, int srt ) {
    int i, j, ncol, nrow, scol;
    struct winsize ws;
    ioctl(1, TIOCGWINSZ, &ws);

    scol = fmax + pad + 4;
    ncol = (ws.ws_col / scol) ? (ws.ws_col / scol) : 1;
    nrow = (nf / ncol) + ((nf % ncol) ? 1 : 0); 

    if( srt == 0 ) {
        /* vsort */
        for( i=0; i<nrow; i++ ) {
            for( j=0; j<ncol; j++ ) {
                if(j*nrow+i >= nf ) break;
                printf("%*d: %-*s", pad, j*nrow+i+1, ((j<ncol-1) ? fmax+2 : 0), files[j*nrow+i]);
            }
            printf("\n");
        }
    } else if( srt == 1 ) {
        /* hsort */
        for( i=0; i<nrow; i++ ) {
            for( j=0; j<ncol; j++ ) {
                if(i*ncol+j >= nf ) break;
                printf("%*d: %-*s", pad, i*ncol+j+1, ((j<ncol-1) ? fmax+2 : 0), files[i*ncol+j]);
            }
            printf("\n");
        }
    } else {
        /* one per line */
        for( i=0; i<nf; i++ ) printf("%*d: %s\n", pad, i+1, files[i]);
    }

    if( nf >= NUM_FILENAMES ) {
        fprintf(stderr, "\n* limit %d reached. *\n", NUM_FILENAMES);
    }

    return 0;
}

int pickfile(char** toks, int * nt, int fmax, char* cmd, int srt) {
    /* pick a file from a list of 'em */
    int i;
    static char buff[NAME_MAX + 1] = "";
    char *prompt;

    i = magnitude(nf);
    prompt = malloc((i+3) * sizeof(char));
    sprintf(prompt, "%*s: ", i, "");

    listfiles(fmax, i, srt);

    #ifdef NO_READLINE
        printf("%s", prompt);
        fgets(buff, sizeof(buff) - 1, stdin);
        buff[strlen(buff) - 1] = '\0';
    #else
        rl_attempted_completion_function = my_completion;
        strncpy(buff, readline(prompt), sizeof(buff) - 1);
        buff[sizeof(buff) - 1] = '\0';
    #endif

    free(prompt);
    if( !strcmp(buff, "") ) return 1;
    if( parse(buff, toks, nt, cmd) ) return 1; 

    return 0;
}

int filt(char *file, regex_t* re, int nr, int all, int bin, int dirs, int inv) {
    struct stat statbuf;
    int i, nok; 
    if( stat(file, &statbuf) == -1 ) return 1; 
    if( !strcmp(file, ".") ) return 1;
    if( !strcmp(file, "..") ) return 1;
    if( !all && file[0] == '.' ) return 1;
    if( dirs && S_ISDIR(statbuf.st_mode) ) {
        strcat(file, "/");
    } else if( S_ISDIR(statbuf.st_mode) ) return 1;
    if( nr ) {
        nok = 0;
        for( i=0; i<nr; i++ ) {
            if( inv ^ regexec(&re[i], file, 0, NULL, 0) ) {
                nok = 1;
                break;
            }
        }
        if( nok ) return 1;
    }
    if( !bin && isbin(file) ) return 1;
    return 0;
}

char ** locfiles(char* loc, int all, int bin, int dirs, int v, regex_t* re, int nr, int *nf, int* fmax) {
    /* filtered list of files from locate */
    FILE* p;
    char ln[NAME_MAX + 1];
    char** files = (char**)malloc(sizeof(char*) * NUM_FILENAMES);
    *nf = *fmax = 0;

    p = popen(loc, "r");
    while( fgets(ln, sizeof(ln), p) && *nf < NUM_FILENAMES) {
        ln[strlen(ln) - 1] = '\0';
        if( filt(ln, re, nr, all, bin, dirs, v) ) continue;
        files[*nf] = (char*)malloc(strlen(ln) + 1 * sizeof(char));
        strcpy(files[*nf], ln);
        if( (int)strlen(files[*nf]) > *fmax ) *fmax = strlen(files[*nf]);
        ++(*nf);
    }
    return files;
}

char** dirfiles(int all, int bin, int dirs, int inv, regex_t* re, int nr, int* nf, int* fmax) {
    /* filtered list of files from a directory */
    DIR *d;
    struct dirent *dir;
    char** files = (char**)malloc(sizeof(char*) * NUM_FILENAMES);

    *nf = *fmax = 0;
    d = opendir(".");
    if( d ) {
        while( (dir = readdir(d)) != NULL && *nf < NUM_FILENAMES) {
            if( filt(dir->d_name, re, nr, all, bin, dirs, inv) ) continue;
            files[*nf] = (char*)malloc(strlen(dir->d_name) + 1 * sizeof(char));
            strcpy(files[*nf], dir->d_name);
            if( (int)strlen(files[*nf]) > *fmax ) *fmax = strlen(files[*nf]);
            ++(*nf);
        }
        closedir(d);
    }

    return files;
}

int main(int argc, char* argv[]) {
    int all, bin, dirs, icas, inv, srt, test, fmax, i, nr, nt, r;
    char* cmd;
    char* loc = NULL;
   
    char err[NAME_MAX];
    char** toks = (char**)malloc(sizeof(char*) * NAME_MAX + 1);
    regex_t* re;
    struct stat statbuf;

    cmd = NULL;
    all = bin = dirs = icas = inv = srt = test = 0;
    opterr = 1;
    while ((i = getopt(argc, argv, "abdghil:tvVx")) != -1) {
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
            case 'g':
                cmd = getenv("VISUAL");
                break;
            case 'h':
                use(argv[0]);
                return 0;
            case 'i':
                icas = 1;
                break;
            case 'l':
                loc = optarg;
                break;
            case 't':
                test = 1;
                break;
            case 'v':
                inv = 1;
                break;
            case 'V':
                printf("%s %s %s\n", __FILE__, __DATE__, __TIME__);
                return 0;
            case 'x':
                srt = 1;
                break;
            case '?':
                break;
            default:
                abort();
        }
    }

    if( cmd == NULL ) cmd = getenv("EDITOR");
    if( cmd == NULL ) cmd = "vi";

    re  = (regex_t*)malloc((argc - optind) * sizeof(regex_t));
    nr = 0;
    if( argc - optind ) {
        int flags = REG_EXTENDED | REG_NOSUB;
        if( icas ) flags = flags | REG_ICASE;
        if( (argv[optind][strlen(argv[optind])-1] == '/') && 
            /* first non option arg might be a directory */
            (stat(argv[optind], &statbuf) != -1) &&
            (S_ISDIR(statbuf.st_mode)) ) {
            if( chdir(argv[optind++]) ) {
                fprintf(stderr, "Couldn't cd to %s\n", argv[optind-1]);
            }
        } else if( !strcmp(argv[optind], ".") ) {
            /* reopen last file */
            lastedited(cmd, test);
            return 0;
        }
        for( i=optind; i<argc; i++ ) {
            if( (r = regcomp(&re[nr], argv[i], flags)) ) {
                regerror(r, &re[nr], err, 80 );
                fprintf(stderr, "%s\n", err);
            } else nr++;
        }
    }

    if( loc == NULL ) {
        files = dirfiles(all, bin, dirs, inv, re, nr, &nf, &fmax);
    } else {
        char* locs = (char*)malloc(strlen(loc) + 16 * sizeof(char));
        sprintf(locs, "locate --regex %s", loc);
        files = locfiles(locs, all, bin, dirs, inv, re, nr, &nf, &fmax);
    }
    free(re);
    qsort(files, nf, sizeof(char *), compare);

    if( nf == 1 ) {
        /* single match */
        nt = 0;
        toks[nt] = (char *)malloc(strlen(cmd) + 1 * sizeof(char));
        strcpy(toks[nt++], cmd);
        toks[nt] = (char *)malloc(strlen(files[0]) + 1 * sizeof(char));
        strcpy(toks[nt++], files[0]);
        toks[nt++] = NULL;
    } else if( pickfile(toks, &nt, fmax, cmd, srt) ) {
        free(toks);
        return 0;
    }

    if( test ) {
        printf("[ ");
        for( i=0; i<nt; i++ ) printf("\"%s\", ", toks[i]);
        printf("]\n");
        free(toks);
    } else execvp(toks[0], toks);

    return 0;
}
