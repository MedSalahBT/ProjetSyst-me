#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>   
#include <ctype.h>      
#include <string.h>
#include <errno.h>      
#include <pwd.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>

char   ligne[4096];     /* contient la ligne d'entree */
char *cmdsHistory[10];  /* contient tous les ligne */
int cmdHisC =0;         /*count pour les commande historique*/

/*  pointeurs sur les mots de ligne (voir decoupe) */
#define MAXELEMS 32
char* elems[MAXELEMS];
int elems_count;
char *path; 
int flag_pipe;


#define COMMANDE_HISTORY 0
#define EXIT 1
#define CD 2
#define HISTORY 3
#define TOUCH 4
#define CAT 5
#define COPY 6
#define JOBS 7
#define WAIT 8
#define FG 9
#define BG 10
#define KILL 11
#define PS 12

#define COMMANDE_normal 20   //commande en utilisant execvp (avec pipe)

#define NUMBER_Commande 12


char *builtin_str[] = {
  "exit",
  "cd",
  "history", 
  "touch",
  "cat", 
  "copy",
  "jobs",
  "wait",
  "fg",
  "bg",
  "kill",
  "ps",
};

void affiche_invite();
void lit_ligne();
void decoupe();
void execute();

int isNumber(char* str);
void commande_history(char** commande);
void exitt(char** commande);
void history(char** commande);
void cd(char** commande);
void touch(char** commande);
void cat(char** commande);
void copy(char** commande);
int Copy_f(char *fS,char *fD);
int Copy_dir(char* dirS,char* dirD);
void commande_basic(char** commande);
void redirection(char** commande);
int isBackground(char** commande);


char* commande_Path(char* commande);
void commande_normal() ;


#define FOREGROUND 'F'
#define BACKGROUND 'B'
#define STOP 'S'
static pid_t shell_pid;
static pid_t shell_pgid;
static int shell_terminal, is_shell_interactive;
static int child_pgid;
static pid_t child_pid;
static int numActiveJobs = 0;
int executionMode=FOREGROUND;


typedef struct job
{
    int id;
    char *name;
    pid_t pid;
    pid_t ppid;
    pid_t pgid;
    int status;
    struct job *next;
} job;

static job* JobList = NULL;

void init();;
job* insertJob(pid_t pid, pid_t ppid, pid_t pgid, char* name,int status);
job* newJob(pid_t pid, pid_t ppid, pid_t pgid, char* name,int status);
void printJobs(int flagPs);
void printStatus(job * job,int * termstatus);
void delJob();
void waitJobs();
void fgJobs();
void bgJobs();
void killJobs();
void psJobs();
int stopJob(int pid,int * termstatus);
void sigchld_handler(int signum);


int main()
{
  system("clear");
  
  while (1) {
    init();
    affiche_invite();
    lit_ligne();
    decoupe();
    execute();
  }
  return 0;
}

/*
  execute le commande
  */
void execute()
{

  if(elems[0] == NULL) return;
  commande_normal(); // on s'oocupe les commandes : commande1 | commande2 | command3 ...

  
}


/*
  pour decouper les commande de pipe recursivement.
  Au debut, flag_pipe equal le nombre de elems
  */

int decoupe_pipe(){
  int i= flag_pipe-1;
  while(i!=0){
    if(strcmp(elems[i],"|")==0){
      elems[i] =NULL;
      break;
    }
    i--;
  }
  flag_pipe =i;
  return i;
}

/*
  on suppose les commandes : commande1 | commande2 | command3 ...
  */
void commande_normal() {
  // printf("pipe %d\n", flag_pipe);
  int flag=decoupe_pipe();
  if(flag == 0) {commande_basic(elems);return;}   // le premiere commande (grande-grande-fils), commande1

  int fds[2];
    if(pipe(fds) == -1){
        perror("pipe error");
        exit(EXIT_FAILURE);
    }

  pid_t pid;
  if ((pid = fork()) < 0) {
    printf("fork failed.\n");
    return;
  }

  if(pid==0){   
        pid_t pid2;
        pid2 = fork();
        if(pid2 == -1){
            perror("fork error");
            exit(EXIT_FAILURE);
        }
        if(pid2 == 0){
            
            dup2(fds[1],STDOUT_FILENO);// mettre fds[1](write end) pour le sortie du processus de fils
            close(fds[0]);
            close(fds[1]);
                          
            commande_normal();           // s'il y a deux ou plus pipes (|) ,utiliserz commande_pipe() recursivement
        }
        else{
    
            dup2(fds[0],STDIN_FILENO);  // mettre fds[0](read end) comme l'entrée au processus de parent
            close(fds[0]);
            close(fds[1]);
            commande_basic(elems+flag+1);   // le commande prochain (parent)

        }
        exit(0);
  }
  else
  {
    close(fds[0]);
    close(fds[1]);
    int status;
    waitpid(pid,&status,WUNTRACED);
  }
}
/*
  commande 
  */ 
void commande_basic(char** commande){

  char* filename = commande_Path(commande[0]);
  if (filename == NULL) {
    printf("Command not found.\n");
    return;
  }
  else
    printf("Path de %s: %s\n", commande[0], filename);      //  Attention: cen effet ,c'est exactement sur le screem : stdout. 
                                                            //  Par example: cat toto | grep l    .
                                                            //  Les donné de la sortie de Cat contient le pharse "Path de Cat" ,
                                                            //  et il est une partie de l'entrée du Grep
                                                    
  int bg = isBackground(commande);
  /*
    Si c'est un tache de fond(background), on mettre le processus parent ignore le signal de fils. 
      Et c'est obligatoire pour éviter le processus zombie.
      L'autre facon : on ajoute un signal_handler à processus de fils
  // */  
  if (bg == 1)
    {    
        // printf("background\n");
        // signal(SIGCHLD, SIG_IGN);      
    }
  else
    signal(SIGCHLD, SIG_DFL);          // pour le parent surveille le fils ( le fonction waitpid )  

    child_pid = fork();
    if (child_pid < 0) {
      printf("fork s'echoue (%s)\n",strerror(errno));
      return;
    }

    else if (child_pid==0) {  // fils
        
        int save_fd = dup(1); // on stocker le entré de shell ,pour revenir apres le redirection
        redirection(commande);    // si il y a ">" ou "<", on fait redirection

        signal(SIGINT, SIG_DFL);                        
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTSTP, SIG_DFL); 
        signal(SIGCHLD, &sigchld_handler);                          

        if(bg == 1)setpgid(getpid(),getpid()); // Si c'est un processus de fond , on donne un ID de group nouveau
        commande_option(commande);
        dup2(save_fd, 1); //revenir apres le redirection
    }
    else {
          
          usleep(5000);     // attend un peu de processus de fil

          JobList = insertJob(child_pid,getpid(), getpgid(child_pid), elems[0],(bg==1)?BACKGROUND:FOREGROUND);

          if(bg==0)         // Si un processus de background, on n'attends pas 
          {
            pid_t ppid;
            int status;
            if((ppid=waitpid(child_pid,&status,WUNTRACED))!=-1)      /* attend fils */
              {
                if(WIFSTOPPED(status))  stopJob(ppid,&status);
                else   delJob(ppid,&status); 
                  tcsetpgrp(shell_terminal,shell_pgid);      
              }   
          }
      }

}

// On choit le commande pour excuter
void commande_option(char** commande)
{
  int option=0;
  int option_normal=0;
  while(option < NUMBER_Commande)
  {
    //commande !! et !n
    if(commande[0][0] == '!') {
        option_normal=1;
        option = COMMANDE_HISTORY;
        break;
    }
    //commande exit, cd , history, touch, cat, copy,jobs, wait,fg ,bg, kill, ps
    if( strcmp(builtin_str[option++],commande[0])==0)
      {
        option_normal=1;
        break;
      }
  } 

  if(option_normal != 1) option=COMMANDE_normal;
  switch(option)
  { 
    case COMMANDE_HISTORY: 
            commande_history(commande);break;
    case EXIT: 
            exitt();break;
    case CD: 
            cd(commande);break;
    case HISTORY: 
            history(commande);break;
    case TOUCH: 
            touch(commande);break;
    case CAT:
            cat(commande);break;    
    case COPY: 
            copy();break;    
    case JOBS:
            printJobs(0); break;    // n'affiche que le processus du fond
    case PS:
            printJobs(1); break;
    case WAIT:
            waitJobs();break;
    case FG:
            fgJobs();break;
    case BG:
            bgJobs();break;  
    case KILL:
            killJobs();break;     
    default:             
             if(execvp(commande[0], commande )==-1)     // le commande utilisant execvp
          printf("impossible d'execute \"%s\" (%s) en utilisant execvp\n",elems[0],strerror(errno));
  }   
}



/*
  justifier est qu'il a un ">", si oui, retourner son place, sinon retourner 0 
  */
void redirection(char** commande)
{
  int i=0,j=0;                // i pour stocker le place de ">", j pour "<"
  int temp1=0,temp2=0;
  while(commande[i]!=NULL){
    if(strcmp(commande[i],">")==0){
      commande[i] =NULL;
      temp1=1;
      break;
    }
    if(strcmp(commande[j],"<")==0){
      commande[i] =NULL;
      temp2=1;
      break;
    }
    i++;j++;
  }

  if(temp1==0) i=0;
  if(temp2==0) j=0;

  printf("%s\n",commande[i]);
  int fid;
  if(commande[i+1]==NULL)return;
    if (temp1 == 1) {

      fid = open(commande[i+1], O_CREAT | O_TRUNC | O_WRONLY, 0600);  // commande[i+1] est le nom de file
      dup2(fid, 1); // mettre sortie ver file
      
    } else if (temp2== 1) {
      fid = open(commande[j+1],O_RDONLY, 0600);         // commande[i+1] est le nom de file
      dup2(fid, 0);           // mettre file  comme l'entre  au processu
    }
  close(fid);

    return;
}

/*
 justifier est ce qu'il y a &
 */
int isBackground(char** commande)
{
  int temp=0;
  int i=0;
  while(commande[i]!=NULL){
    if(strcmp(commande[i],"&")==0){
      elems[i] =NULL;
      temp=1;
      break;
    }
    i++;
  }
  if(temp==0) return 0;else return 1;
  // return 
}

/*
  trouver chaque path de command basic utilisant execvp
*/
char* commande_Path(char* commande){
  FILE *fpin;
  char p[1024];
  char *temp = (char *)calloc(512, sizeof(char));
  path = (char *) getenv("PATH");

  strcpy ( p, path );
  path = strtok ( p, ":" );   // trouver le premiere path
  while ( path != NULL ) {
    strcpy ( temp, path );  // stocker le premiere path
    strcat ( temp, "/" );     
    strcat ( temp, commande );   //  path/filename
    if ( ( fpin = fopen ( temp, "r" ) ) == NULL ) {
      path = strtok (NULL, ":" );  // le prochain path
    } else {
      break;  
    }
  }

  if ( fpin != NULL ) {           //si on peut le trouver dans le "PATH",retourner le path
    return temp;
  } 
  else 
  {                            //sinon , on justifier est-ce qu'est ce que le commande est exactment un fichier
      struct stat buf;
      stat(commande, &buf);
      if(S_ISREG (buf.st_mode))
      {
          if(commande[0]=='.'){       // le path relative du fichier(commande)
              char repertoire[100];   
              getcwd(repertoire,sizeof(repertoire)); 
              char * path= (char *)calloc(1024, sizeof(char));
              strcpy(path,repertoire);
              strcat(path,commande+1);
              return path;
          }
          else                                  
          return commande;      // le path absolu du fichier(commande)
      }
  }

  int option=0;                   // À la fin, est0ce que c'est un commander re definie
  int option_normal=0;
  while(option < NUMBER_Commande)
  {
    //commande !! et !n
    if(commande[0] == '!') {
        option_normal=1;
        option = COMMANDE_HISTORY;
        break;
    }
    //commande exit, cd , history, touch, cat, copy,jobs, wait,fg ,bg, kill, ps
    if( strcmp(builtin_str[option++],commande)==0)
      {
        option_normal=1;
        break;
      }
  }
  if(option_normal == 1){
    char repertoire[100];   
    getcwd(repertoire,sizeof(repertoire)); 
    char * path= (char *)calloc(1024, sizeof(char));
    strcpy(path,repertoire);
    strcat(path,commande);
    return path;
    }
  return NULL; 
}



/*
  interface pour entrée du commande
  */
void affiche_invite()
{
  struct passwd *pwd;
  pwd = getpwuid(getuid());
  char name[50];
  gethostname(name, sizeof(name));
  char repertoire[50];   
  getcwd(repertoire,sizeof(repertoire)); 
  printf("%s@%s : %s> ", pwd->pw_name, name,repertoire);
  fflush(stdout);
}

/*
  stocker le commande 
  */
void lit_ligne()
{
  if (!fgets(ligne,sizeof(ligne)-1,stdin)) {
  	printf("\n");
    exit(0);
  }
}

/* 
   découpe ligne en mots 
   fait pointer chaque elems[i] sur un mot différent
   elems se termine par NULL
 */
void decoupe()
{
  char* debut = ligne;
  cmdsHistory[cmdHisC++] = strdup(ligne);

  int i;
  for (i=0; i<MAXELEMS-1; i++) {

    /* saute les espaces */
    while (*debut && isspace(*debut)) debut++;

    /* fin de ligne ? */
    if (!*debut) break;

    /* on se souvient du dÃ©but de ce mot */
    elems[i] = debut;

    /* cherche la fin du mot */
    while (*debut && !isspace(*debut)) debut++; /* saute le mot */

    /* termine le mot par un \0 et passe au suivant */
    if (*debut) { *debut = 0; debut++; }
  }
  elems_count = i;
  flag_pipe= elems_count;
  elems[i] = NULL;
  fflush(stdin);
  fflush(stdout);


}


/*
  Justifier une chaine de caractere est un nombre positive ou negative (pas 0)  , si oui retourner le nombre,
sinon, retourner 0
  */
int isNumber(char* str)
{
	int number = atoi(str);
  	return number;
}





/*
le commande history and history n
  */
void history(char** commande)
{	
	int n=5;   // nombre du commande pour afficher
  	
  	if(commande[1] != NULL){
  		n=isNumber(commande[1]);
  	}

  	if( n!=-1 && n>cmdHisC) n=cmdHisC;

    for(int i=cmdHisC-n; i<cmdHisC; i++)
    {
        printf(" %d : %s",i+1, cmdsHistory[i]);     
    }
    return;
}

/*
 le commande exit : sortie de shell
 */
void exitt()
{
	exit(0);
}

/*
  le commande cd : change le repertoire
  */
void cd(char** commande)
{
	if(commande[1] == NULL || strcmp(commande[1],"~")==0 ){
            chdir(getenv("HOME"));
          }
    else  chdir(commande[1]);
}


/*
  le commande touch file et touch -d file
  */
void touch(char** commande)
{
	
    	struct stat stat_file;
    	// c'est touch file 
    	if(commande[2] ==NULL)
    	{
    		stat(commande[1],&stat_file);
    		//s'il exist , on doit changer sa date de modification
    		time_t lt;
			   lt=time(NULL);
			   stat_file.st_ctime=lt;
    	}
    	// c'est touch -d file
   		else if (strcmp("-d",commande[1])==0)
    	{
    		  // si le file  n'exist pas, on le cree, et donne la date maintanant
    		  if(stat(commande[2],&stat_file)== -1)
    		  {
    			   time_t t;
				      t=time(NULL);
    			   printf("%s : %s",commande[2], ctime(&t));
   			  }
   			  // s'il exist deja , on affich la date de derniere modification 	
    		  else 
    		  {
    		  	printf("%s : %s",commande[2],ctime(&(stat_file.st_ctime)));
    		  }
    		  return ;

    	}
    	else
    		{
    			printf("Les parametres ne sont pas correct\n");
    		}	
   
}

/*
   le commande !! et !n ，n est un entier positive ou negative
   */
void commande_history(char** commande)
{
  if(commande[0][1]!='\0')
    {
      int n; // numéro du commande , si c'est negative ,ca veut dire le n derniere commande
      if( cmdHisC >=2 && commande[0][1] == '!' && commande[0][2]=='\0')
      {
      	cmdHisC -- ;
        strcpy(ligne,cmdsHistory[cmdHisC-1]);
        decoupe();
      }
      else
      {
        commande[0]++;  
        int n = isNumber(commande[0]);
        printf("in commande history %d\n", n);
        if( n >= 1 && n < cmdHisC )
        {
          cmdHisC -- ;
          strcpy(ligne,cmdsHistory[n-1]);
          decoupe();  
        }
        else if( n< 0 && (cmdHisC+n) >=1)
            {
              cmdHisC -- ;
              strcpy(ligne,cmdsHistory[cmdHisC+n]);
              decoupe();  
            }
          else
          {
          	cmdHisC -- ;
          	printf("On peut pas trouver le commande\n");
         	return;
          } 
        }
        execute();
    }

}

/*
  le commande cat file , cat -d file et cat [-d] file1 file2 [file3]
  */
void cat(char** commande)
{
  if(commande[1]!=NULL)
    {
      int flag=0;  // pour justifier -d
      if(strcmp("-n",commande[1])==0)
      {
        flag =1;
      }
      int i= flag+1;
      while(commande[i] != NULL)
      {
        // printf("%s\n", commande[i]);
        FILE *file;  
        file = fopen(commande[i], "r");  
        char ch;  
        if(file==NULL){
          printf( "Il n'y a pas file: %s\n", commande[i]);  //源文件不存在的时候提示错误
          return;
        }
        else{
          ch=fgetc(file);
          int numberColome=0;
          if(flag == 1)
              printf("   %d  ",++numberColome);
          while(ch != EOF)
          {           
            putchar(ch);
            if(ch == '\n' && flag == 1)
              printf("   %d  ",++numberColome);
            ch=fgetc(file);
          }
        }
        printf("\n");
        i++;
      }  
    }
  else
    {
      printf("peu de parametres\n");
    }     
}

/*
 le commande copy 
 */
void copy(char** commande)
{
  // check if the entre is right or not
  if( commande[2]==NULL || commande[3] != NULL)
  {
    printf ("there are not enough or too many parameters\n ");
    return ;
  }

  // check which kind of document for copy
  struct stat buf;
  stat(commande[1], &buf);
  if(S_ISREG (buf.st_mode))
  {
    Copy_f(commande[1],commande[2]);
  }
  else if(S_ISDIR(buf.st_mode))
  {
  Copy_dir(commande[1],commande[2]);
  }
  else 
  {
    printf("%s is not a file, and not a folder  ",commande[1]);
  }

}


/*
    copy the file
 */
int Copy_f(char *fS,char *fD)
{
    char buffer[512];
    int fSrc, fDst;


    fSrc = open(fS, O_RDONLY);
    if(fSrc == 0)
    {
        perror("open source file wrong");
        return 0;
    }

    fDst = open(fD, O_CREAT|O_WRONLY);
    if(fDst == 0)
    {
        perror("open destination file wrong");
        return 0;
    }

    errno=0;
    int Nb;
    while ((Nb = read(fSrc, buffer, sizeof(buffer))) != 0) 
    {
        if( write(fDst, buffer, Nb)<0 )
        { 
            perror("write destination file wrong ");
            return 0;
        }
    }
    if(errno != 0)
    {
        perror("read source file wrong");
        return 0;
    }
    close(fSrc);
    close(fDst);

// change the permission of the destination file 
  struct stat buf;
  stat(fS, &buf);
  chmod(fD,buf.st_mode);
  return 1;
}

/*
    copy the folder
 */
int Copy_dir(char* dirS,char* dirD)
{
    DIR* dpS;
    DIR* dpD;
    struct dirent *entry;
    char srcInside[200];
    char desInside[500];

    if((dpS = opendir(dirS))  ==  NULL)
    {
        perror("open source diretory wrong");
        return 0;
    }

    struct stat buf;
    stat(dirS, &buf);   //for give the same permission to destination 
   
    if((dpD =opendir(dirD)) == NULL){ // if there no dirD,we creat one 
        if (mkdir(dirD,buf.st_mode) <0 )  
        {  
            printf("creat the distination file wrong");  
        }  
    }  

    while((entry = readdir(dpS)) != NULL)
    {
        // to know the path for copy
        strcpy(srcInside, dirS);
        strcpy(desInside, dirD);
        if(srcInside[strlen(srcInside)-1]!='/')
        strcat(srcInside, "/");
        if(desInside[strlen(desInside)-1]!='/')
        strcat(desInside, "/");
        strcat(srcInside, entry->d_name);
        strcat(desInside, entry->d_name);

        //begin to copy      
        struct stat buf;
        stat(srcInside, &buf);
        if(S_ISDIR(buf.st_mode)) //it's a subfolders dans the dirS 
        {
            // ignore the documents "." et ".."
            if(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) 
                continue;
            //copy the folder recursively 
            Copy_dir(srcInside, desInside);
        }
        else//it's a file
        {
            Copy_f(srcInside, desInside); 
        }
    }     
    return 1;
}


/*
  initialiser l'statut
  */
void init()
{
    shell_pid = getpid();
    shell_terminal = STDIN_FILENO;
    is_shell_interactive = isatty(shell_terminal);
    if (is_shell_interactive)
    {
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(shell_pid, SIGTTIN);
        signal(SIGQUIT, SIG_IGN);     //au debut , on ignore tout les signaux
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);  
        signal(SIGINT, SIG_IGN);
        signal(SIGCHLD,&sigchld_handler);
        setpgid(shell_pid, shell_pid);
        shell_pgid = getpgrp();
        if (shell_pid != shell_pgid)
        {
            printf("Error, the shell is not process group leader");
            exit(EXIT_FAILURE);
        }
        tcsetpgrp(shell_terminal, shell_pgid);
    }
    else
    {
        printf("Could not make SHELL interactive. Exiting..\n");
        exit(EXIT_FAILURE);
    }
}


job* insertJob(pid_t pid, pid_t ppid, pid_t pgid, char* name,int status)
{

    job *tempJob;
    tempJob=newJob(pid,ppid,pgid,name,status);
    if (JobList == NULL)
    {
        numActiveJobs++;
        tempJob->id = numActiveJobs;
        return tempJob;
    }
    else
    {
        job *tempNode = JobList;
        while (tempNode->next != NULL)
        {
            tempNode = tempNode->next;
        }
        tempJob->id = tempNode->id + 1;
        tempNode->next = tempJob;
        numActiveJobs++;
        return JobList;
    }
}

job* newJob(pid_t pid, pid_t ppid, pid_t pgid, char* name,int status)
{
    job *tempJob = malloc(sizeof(job));
    tempJob->name = (char*) malloc(sizeof(name));
    tempJob->name = strcpy(tempJob->name, name);
    tempJob->pid = pid;
    tempJob->ppid=ppid;
    tempJob->pgid = pgid;
    tempJob->status = status;
    tempJob->next = NULL;
    return tempJob;
}

/*
  affiche les information de jobs
  Si flagPs est 0, c'est un commande jobs qui n'affich que le processus de background,
  sinon c'est ps ,qui affiche tout les processus
  */
void printJobs(int flagPs)    
{
    job* Job = JobList;
    if (Job == NULL){printf("Rien~\n" );return ;}
    if(flagPs == 0)
    {
    	while (Job != NULL)
    	{
    	    if(Job->status==FOREGROUND ) continue;
    	    if(Job->status == STOP )
    	   		printf("  [%d]+Stopped     %10s  \n", Job->id, Job->name);
    		else
    	   		printf("  [%d]+Running  %10s  \n", Job->id, Job->name);
    	    Job = Job->next;
    	}
    	return;
    }
    else
    {
    	printf("--------------------------------------------------------\n");
    	printf(" %5s   %10s  %5s  %5s  %5s  %6s \n", "  NO", "NAME", "PID" ,"PPID", "PGID", "STATUS");
    	printf("--------------------------------------------------------\n");
    	while (Job != NULL)
    	{
    	    printf("  %5d  %10s  %5d  %5d  %5d  %6c \n", Job->id, Job->name, Job->pid ,Job->ppid,Job->pgid, Job->status);
    	    Job = Job->next;
    	}
    	printf("--------------------------------------------------------\n");
    }
}
/*
  fonction pour afficher les status quand il y a un signal 
  */
void printStatus(job * job,int * termstatus)
{
    if (WIFEXITED(*termstatus))
    {
        if (job->status == BACKGROUND)
        {
            printf("\n[%d]+  Done\t   %s\n", job->id, job->name);
        }
    }
    else if (WIFSIGNALED(*termstatus))
    { 

        printf("\n[%d]+  Killed\t   %s\n", job->id, job->name);
    }
    else if (WIFSTOPPED(*termstatus))
    {
  
            printf("\n[%d]+   stopped\t   %s\n", job->id, job->name);
    }
    printf("\n");
}


/*
 fonction supprimer le jobs quand il a été tué ou il a fini
 */
void delJob(int pid,int * termstatus)
{   
    if(JobList==NULL) return ;
    numActiveJobs--;
    if(JobList->pid==pid && JobList->next==NULL)
    {
        printStatus(JobList,termstatus);
        JobList=NULL;
        return ;
    }

    job * prevJob, * tempJob;

    if(JobList->pid==pid)
    {
        tempJob=JobList;
        JobList=JobList->next;
        printStatus(tempJob,termstatus);
        free(tempJob);
        return ;
    }
    tempJob=JobList->next;
    prevJob=JobList;
    while (tempJob != NULL)
    {
        if(tempJob->pid==pid)
        {
            prevJob->next=tempJob->next;
            printStatus(tempJob,termstatus);
            free(tempJob);
            return ;
        }
        tempJob = tempJob->next;
    }
    return ;
}

/*
 fonction pour le Ctrl+Z, qui suspend le processus
 */
int stopJob(int pid,int * termstatus)
{   

    job * tempJob=JobList;
    while (tempJob != NULL)
    {
        if(tempJob->pid==pid)
        {
            tempJob->status=STOP;
            printStatus(tempJob,termstatus);
            signal(SIGCHLD, SIG_IGN);
            return 1;
        }
        tempJob = tempJob->next;
    }
    return 0;
}


void waitJobs()      //   wait [n]
{
  // printf("wait jobs\n");
  if (elems[1]==NULL){          // wait , qui attend tout les processus de fond
    job * tempJob=JobList;
    while (tempJob != NULL)
    {
        tempJob->status=FOREGROUND;
        signal(SIGCHLD, SIG_DFL);
        int status;
        waitpid(tempJob->pid,&status,WUNTRACED);
        tempJob = tempJob->next;
    }
    return;
  }

  if(elems[2]==NULL)            // wait pid
  {
    int number=isNumber(elems[1]);
    if(number!=0)
    {
        int temp =0;
        job * tempJob=JobList;
        while (tempJob != NULL)
        {
            if(tempJob->pid==number)
            {
                tempJob->status=FOREGROUND;
                temp=1;
                signal(SIGCHLD, SIG_DFL);
                int status;
                waitpid(tempJob->pid,&status,WUNTRACED);
                break;
            }
            tempJob = tempJob->next;
        }
        if(temp==0) printf("Ce job n'exist pas\n");
        return;
    }
    else printf("Ce job n'exist pas\n");
      return;
  }
  else 
      printf("Trop de parametre\n");
  return;
}


void fgJobs()       // fg n ; fg %n
{
  // printf("fg jobs\n");
  if (elems[1]==NULL){
    printf("Ce job n'exist pas\n");return;}

  if(elems[2]==NULL)
  {
    int number;
      if(elems[1][0]=='%')            //  fg %n
      {   
            elems[1]++;
            number=isNumber(elems[1]);
            if(number != 0)
            {
              int temp =0;
              job * tempJob=JobList;
              while (tempJob != NULL)
              {
                  if(tempJob->id==number)
                  {
                      tempJob->status=FOREGROUND;
                      temp=1;
                      kill(tempJob->pid,SIGCONT);       // envoyer le signal pour continuer le job
                      signal(SIGCHLD, SIG_DFL);
                      tempJob->status=STOP;
                      int status;
                      waitpid(tempJob->pid,&status,WUNTRACED);
                      break;
                  }
                  tempJob = tempJob->next;
              }
              if(temp==0) printf("Ce job n'exist pas\n");
              return;
            }
            else printf("Ce job n'exist pas\n");
      }
      else if(isNumber(elems[1]))         // fg pid
            {
              number=isNumber(elems[1]);
              int temp =0;
              job * tempJob=JobList;
              while (tempJob != NULL)
              {
                  if(tempJob->pid==number)
                  {
                      tempJob->status=FOREGROUND;
                      temp=1;
                      kill(tempJob->pid,SIGCONT);
                      signal(SIGCHLD, SIG_DFL);
                      tempJob->status=STOP;
                      int status;
                      waitpid(tempJob->pid,&status,WUNTRACED);
                      break;
                  }
                  tempJob = tempJob->next;
              }
              if(temp == 0)printf("Ce job n'exist pas\n");
              return;
            }
      else 
        printf("les parametres ne sont pas correct\n"); 
  }
  else 
    printf("Trop de parametre\n");
  return;

}

void bgJobs()       //  bg n ; bg %n
{
  // printf("bg jobs\n");
  if (elems[1]==NULL){
    printf("Ce job n'exist pas\n");return;}

  if(elems[2]==NULL)
  {
      int number;
      if(elems[1][0]=='%')        //  bg %n
      {   
            elems[1]++;
            number=isNumber(elems[1]);
            if(number != 0)
            {
              int temp =0;
              job * tempJob=JobList;
              while (tempJob != NULL)
              {
                  if(tempJob->id==number)
                  {
                      tempJob->status=FOREGROUND;
                      temp=1;
                      kill(tempJob->pid,SIGCONT);
                      signal(SIGQUIT, SIG_IGN);     //au debut , on ignore tout les signaux
        			  signal(SIGTTOU, SIG_IGN);
				      signal(SIGTTIN, SIG_IGN);
				      signal(SIGTSTP, SIG_IGN);  
				      signal(SIGINT, SIG_IGN);
                      signal(SIGCHLD, SIG_IGN);

                      tempJob->status=BACKGROUND;
                  }
                  tempJob = tempJob->next;
              }
              if(temp == 0)printf("Ce job n'exist pas\n");
              return;
            }
            else printf("Ce job n'exist pas\n");
      }
      else if(isNumber(elems[1]))   // bg pid
            {
              number=isNumber(elems[1]);
              int temp =0;
              job * tempJob=JobList;
              while (tempJob != NULL)
              {
                  if(tempJob->pid==number)
                  {
                      tempJob->status=FOREGROUND;
                      temp=1;
                      kill(tempJob->pid,SIGCONT);
                      signal(SIGCHLD, SIG_DFL);
                      tempJob->status=BACKGROUND;
                  }
                  tempJob = tempJob->next;
              }
              if(temp == 0)printf("Ce job n'exist pas\n");
              return;
            }
      else 
        printf("les parametres ne sont pas correct\n"); 
  }
  else 
    printf("Trop de parametre\n");
  return;
}


void killJobs()     // kill pid
{ 
  // printf("kill jobs\n");
  if (elems[1]==NULL){
    printf("Ce job n'exist pas\n");return;}

  if(elems[2]==NULL)
  {
    int number=isNumber(elems[1]);
    printf("%s ：%d\n",elems[1],number );
    if(number!=0)
    {
        int temp =0;
        job * tempJob=JobList;
        while (tempJob != NULL)
        {
            if(tempJob->pid==number)
            {
                tempJob->status=FOREGROUND;
                temp=1;
                signal(SIGCHLD, SIG_DFL);
                kill(tempJob->pid,SIGTERM);
                break;
            }
            tempJob = tempJob->next;
        }
        if(temp==0) printf("Ce job n'exist pas\n");
        return;
    }
    else printf("Ce job n'exist pas\n");
      return;
  }
  else 
      printf("Trop de parametre\n");
  return;
}

/*
  pour obtenir le signal ,en particulier le processus parent se termine avant le fils 
  */
void sigchld_handler(int signum)    
{
    // printf("sigchild handler\n");
    int ppid=-1;
     int termstatus;
    if((ppid=waitpid(-1,&termstatus,WNOHANG|WUNTRACED))>0)
    {
      if(WIFSTOPPED(termstatus))  stopJob(ppid,&termstatus);
      else   delJob(ppid,&termstatus); 
    } 
    tcsetpgrp(shell_terminal,shell_pgid);
}




