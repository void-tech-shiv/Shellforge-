#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXTOK 256
#define MAXCMD 32
#define MAXARG 128

typedef struct { char *argv[MAXARG]; int argc; char *in,*out,*err; int app_out,app_err,err2out; } Cmd;
typedef struct { Cmd c[MAXCMD]; int n; int bg; } Pipe;
typedef struct { const char *nice,*real,*about; } Alias;

typedef struct { pid_t pid; char *text; } Job;

static int last_status=0;
static Job jobs[32];
static char histfile[4096];
static const Alias A[]={
 {"list","ls","list files"},{"path","pwd","show current path"},{"goto","cd","change directory"},
 {"make","mkdir","create directory"},{"new","touch","create file"},{"show","cat","show file"},
 {"copy","cp","copy"},{"move","mv","move/rename"},{"remove","rm","remove"},{"clean","clear","clear screen"},
 {"hist","history","history"},{"print","echo","print text"},{"who","whoami","current user"},
 {"time","date","date/time"},{"quit","exit","exit shell"},{"tasks","jobs","background jobs"},
 {"stop","kill","send signal"},{"find","grep","search text"},{"first","head","first lines"},{"last","tail","last lines"}
};

static const char *mapcmd(const char *s){ for(size_t i=0;i<sizeof(A)/sizeof(A[0]);i++) if(!strcmp(s,A[i].nice)) return A[i].real; return s; }
static int op(const char *s){ return !strcmp(s,"|")||!strcmp(s,"<")||!strcmp(s,">")||!strcmp(s,">>")||!strcmp(s,"2>")||!strcmp(s,"2>>")||!strcmp(s,"2>&1")||!strcmp(s,"&"); }
static void freetok(char **t,int n){ for(int i=0;i<n;i++) free(t[i]); }

static void addc(char **b,size_t *n,size_t *cap,char c){ if(*n+2>*cap){*cap*=2;*b=realloc(*b,*cap);if(!*b)exit(1);}(*b)[(*n)++]=c;(*b)[*n]=0; }
static void adds(char **b,size_t *n,size_t *cap,const char *s){ while(*s)addc(b,n,cap,*s++); }

static int tokenize(const char *s,char **t){
 int n=0; size_t i=0,L=strlen(s);
 while(i<L){
  while(i<L&&isspace((unsigned char)s[i]))i++; if(i>=L)break;
  if(n>=MAXTOK-1){fprintf(stderr,"shellforge: too many tokens\n");return -1;}
  if(!strncmp(s+i,"2>&1",4)){t[n++]=strdup("2>&1");i+=4;continue;}
  if(!strncmp(s+i,"2>>",3)){t[n++]=strdup("2>>");i+=3;continue;}
  if(!strncmp(s+i,"2>",2)){t[n++]=strdup("2>");i+=2;continue;}
  if(!strncmp(s+i,">>",2)){t[n++]=strdup(">>");i+=2;continue;}
  if(strchr("|<>&",s[i])){char x[2]={s[i++],0};t[n++]=strdup(x);continue;}
  size_t cap=64,k=0; char *b=calloc(cap,1); int sq=0,dq=0;
  while(i<L){
   char c=s[i]; if(!sq&&!dq&&(isspace((unsigned char)c)||strchr("|<>&",c)||(c=='2'&&s[i+1]=='>')))break;
   if(!dq&&c=='\''){sq=!sq;i++;continue;} if(!sq&&c=='"'){dq=!dq;i++;continue;}
   if(!sq&&c=='\\'&&i+1<L){addc(&b,&k,&cap,s[++i]);i++;continue;}
   if(!sq&&c=='$'){
    if(s[i+1]=='?'){char q[32];snprintf(q,sizeof q,"%d",last_status);adds(&b,&k,&cap,q);i+=2;continue;}
    if(s[i+1]=='$'){char q[32];snprintf(q,sizeof q,"%ld",(long)getpid());adds(&b,&k,&cap,q);i+=2;continue;}
    size_t a=i+1,z=a; if(s[a]=='{'){a++;z=a;while(s[z]&&s[z]!='}')z++; if(s[z]=='}'){char *v=strndup(s+a,z-a);const char *e=getenv(v);if(e)adds(&b,&k,&cap,e);free(v);i=z+1;continue;}}
    if(isalpha((unsigned char)s[a])||s[a]=='_'){while(isalnum((unsigned char)s[z])||s[z]=='_')z++;char *v=strndup(s+a,z-a);const char *e=getenv(v);if(e)adds(&b,&k,&cap,e);free(v);i=z;continue;}
   }
   addc(&b,&k,&cap,c);i++;
  }
  if(sq||dq){fprintf(stderr,"shellforge: unclosed quote\n");free(b);freetok(t,n);return -1;}
  if(b[0]=='~'&&(b[1]=='/'||!b[1])){const char *h=getenv("HOME");if(h){char *x=malloc(strlen(h)+strlen(b)+1);sprintf(x,"%s%s",h,b+1);free(b);b=x;}}
  t[n++]=b;
 }
 t[n]=NULL; return n;
}

static int parse(char **t,int nt,Pipe *p){
 memset(p,0,sizeof(*p));p->n=1;
 for(int i=0;i<nt;i++){
  Cmd *c=&p->c[p->n-1];
  if(!strcmp(t[i],"|")){if(!c->argc||p->n==MAXCMD)return -1;p->n++;continue;}
  if(!strcmp(t[i],"&")){if(i!=nt-1)return -1;p->bg=1;continue;}
  if(!strcmp(t[i],"2>&1")){c->err2out=1;continue;}
  if(!strcmp(t[i],"<")||!strcmp(t[i],">")||!strcmp(t[i],">>")||!strcmp(t[i],"2>")||!strcmp(t[i],"2>>")){
   if(++i>=nt||op(t[i]))return -1; char *f=t[i];
   if(!strcmp(t[i-1],"<"))c->in=f; else if(!strcmp(t[i-1],">"))c->out=f; else if(!strcmp(t[i-1],">>")){c->out=f;c->app_out=1;} else if(!strcmp(t[i-1],"2>"))c->err=f; else {c->err=f;c->app_err=1;} continue;
  }
  if(c->argc>=MAXARG-1)return -1;c->argv[c->argc++]=t[i];c->argv[c->argc]=NULL;
 }
 return p->c[p->n-1].argc?0:-1;
}

static void aliases(void){puts("ShellForge custom commands:");for(size_t i=0;i<sizeof(A)/sizeof(A[0]);i++)printf("  %-8s -> %-8s %s\n",A[i].nice,A[i].real,A[i].about);}
static void history(void){FILE *f=fopen(histfile,"r");if(!f)return;char *l=NULL;size_t z=0;int i=1;while(getline(&l,&z,f)>0)printf("%4d  %s",i++,l);free(l);fclose(f);}
static void savehist(const char *s){FILE *f=fopen(histfile,"a");if(f){fprintf(f,"%s\n",s);fclose(f);}}
static void reap(void){for(int i=0;i<32;i++)if(jobs[i].pid){int st;pid_t r=waitpid(jobs[i].pid,&st,WNOHANG);if(r==jobs[i].pid){printf("[done] %d %s\n",jobs[i].pid,jobs[i].text);free(jobs[i].text);jobs[i]=(Job){0};}}}
static void listjobs(void){reap();for(int i=0;i<32;i++)if(jobs[i].pid)printf("[%d] Running pid=%d %s\n",i+1,jobs[i].pid,jobs[i].text);}
static void addjob(pid_t p,const char *s){for(int i=0;i<32;i++)if(!jobs[i].pid){jobs[i].pid=p;jobs[i].text=strdup(s);printf("[%d] %d\n",i+1,p);return;}}

static int builtin(Cmd *c,int parent){
 const char *n=c->argv[0];
 if(!strcmp(n,"goto")||!strcmp(n,"cd")){const char *d=c->argc>1?c->argv[1]:getenv("HOME");if(chdir(d?d:"/")<0){perror("shellforge: goto");return 1;}return 0;}
 if(!strcmp(n,"quit")||!strcmp(n,"exit")){int x=c->argc>1?atoi(c->argv[1]):last_status;if(parent)exit(x);return x;}
 if(!strcmp(n,"hist")||!strcmp(n,"history")){history();return 0;}
 if(!strcmp(n,"tasks")||!strcmp(n,"jobs")){listjobs();return 0;}
 if(!strcmp(n,"aliases")){aliases();return 0;}
 if(!strcmp(n,"help")){puts("ShellForge: aliases, pipes |, redirection < > >> 2>, background &, export/unset. Type 'aliases'.");return 0;}
 if(!strcmp(n,"export")){for(int i=1;i<c->argc;i++){char *e=strchr(c->argv[i],'=');if(e){*e=0;setenv(c->argv[i],e+1,1);*e='=';}}return 0;}
 if(!strcmp(n,"unset")){for(int i=1;i<c->argc;i++)unsetenv(c->argv[i]);return 0;}
 return 127;
}
static int isbuiltin(const char *n){return !strcmp(n,"goto")||!strcmp(n,"cd")||!strcmp(n,"quit")||!strcmp(n,"exit")||!strcmp(n,"hist")||!strcmp(n,"history")||!strcmp(n,"tasks")||!strcmp(n,"jobs")||!strcmp(n,"aliases")||!strcmp(n,"help")||!strcmp(n,"export")||!strcmp(n,"unset");}
static int redir(Cmd *c){
 if(c->in){int f=open(c->in,O_RDONLY);if(f<0){perror(c->in);return -1;}dup2(f,0);close(f);}
 if(c->out){int f=open(c->out,O_WRONLY|O_CREAT|(c->app_out?O_APPEND:O_TRUNC),0644);if(f<0){perror(c->out);return -1;}dup2(f,1);close(f);}
 if(c->err){int f=open(c->err,O_WRONLY|O_CREAT|(c->app_err?O_APPEND:O_TRUNC),0644);if(f<0){perror(c->err);return -1;}dup2(f,2);close(f);}
 if(c->err2out)dup2(1,2);return 0;
}

static int execute(Pipe *p,const char *raw){
 if(p->n==1&&!p->bg&&isbuiltin(p->c[0].argv[0])){int a=dup(0),b=dup(1),d=dup(2);int r=redir(&p->c[0])?1:builtin(&p->c[0],1);fflush(NULL);dup2(a,0);dup2(b,1);dup2(d,2);close(a);close(b);close(d);return r;}
 int prev=-1;pid_t pid[MAXCMD];
 for(int i=0;i<p->n;i++){
  int fd[2]={-1,-1};if(i<p->n-1&&pipe(fd)<0){perror("pipe");return 1;}pid[i]=fork();
  if(pid[i]<0){perror("fork");return 1;} if(!pid[i]){signal(SIGINT,SIG_DFL);signal(SIGQUIT,SIG_DFL);signal(SIGTSTP,SIG_DFL);if(prev!=-1)dup2(prev,0);if(i<p->n-1)dup2(fd[1],1);if(prev!=-1)close(prev);if(fd[0]!=-1)close(fd[0]);if(fd[1]!=-1)close(fd[1]);if(redir(&p->c[i]))_exit(1);if(isbuiltin(p->c[i].argv[0]))_exit(builtin(&p->c[i],0));p->c[i].argv[0]=(char*)mapcmd(p->c[i].argv[0]);execvp(p->c[i].argv[0],p->c[i].argv);fprintf(stderr,"shellforge: %s: %s\n",p->c[i].argv[0],strerror(errno));_exit(127);}
  if(prev!=-1)close(prev);if(fd[1]!=-1)close(fd[1]);prev=fd[0];
 }
 if(prev!=-1)close(prev);if(p->bg){addjob(pid[p->n-1],raw);return 0;}int st=0;for(int i=0;i<p->n;i++){int x;while(waitpid(pid[i],&x,0)<0&&errno==EINTR);if(i==p->n-1)st=x;}return WIFEXITED(st)?WEXITSTATUS(st):128+WTERMSIG(st);
}

static void sigint(int x){(void)x;write(1,"\n",1);}
int main(void){
 signal(SIGINT,sigint);signal(SIGQUIT,SIG_IGN);signal(SIGTSTP,SIG_IGN);const char *h=getenv("HOME");snprintf(histfile,sizeof histfile,"%s/.shellforge_history",h?h:".");
 char *line=NULL;size_t cap=0;if(isatty(0))puts("ShellForge ready. Type 'help' or 'aliases'.");
 for(;;){reap();if(isatty(0)){char cwd[4096];if(getcwd(cwd,sizeof cwd))printf("shellforge:%s$ ",cwd);else printf("shellforge$ ");fflush(stdout);}errno=0;ssize_t n=getline(&line,&cap,stdin);if(n<0){if(errno==EINTR){clearerr(stdin);continue;}break;}while(n>0&&(line[n-1]=='\n'||line[n-1]=='\r'))line[--n]=0;char *s=line;while(isspace((unsigned char)*s))s++;if(!*s)continue;savehist(s);char *t[MAXTOK]={0};int nt=tokenize(s,t);if(nt>0){Pipe p;if(!parse(t,nt,&p))last_status=execute(&p,s);else{fprintf(stderr,"shellforge: syntax error\n");last_status=2;}freetok(t,nt);}else if(nt<0)last_status=2;}
 free(line);return last_status;
}
