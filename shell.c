/**************

Miaobing CHEN, Mohamed Salah BEN TAARIT

***************/


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
#define COMMANDE_BASIC 10

#define NUMBER_Commande 6


char *builtin_str[] = {
  "exit",
  "cd",
  "history", 
  "touch",
  "cat", 
  "copy"
};

void affiche_invite();
void lit_ligne();
void decoupe();
void execute();

int isNumber(char* str);
void commande_history();
void exitt();
void history();
void cd();
void touch();
void cat();
void copy();
int Copy_f(char *fS,char *fD);
int Copy_dir(char* dirS,char* dirD);
void commande_basic(char** commande);
int isRedirection(char** commande);
void redirection(char** commande);


char* commande_Path();
void commande_pipe() ;


int main()
{
        system("clear");
  while (1) {
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

  int option=0;
  int option_normal=0;
  while(option < NUMBER_Commande)
  {
    //commande !! et !n
    if(elems[0][0] == '!') {
        option_normal=1;
        option = COMMANDE_HISTORY;
        break;
    }
    //commande exit, cd , history, touch, cat, copy
    if( strcmp(builtin_str[option++],elems[0])==0)
      {
        option_normal=1;
        break;
      }
  }

  

  if(option_normal != 1) option=COMMANDE_BASIC;
  switch(option)
  { 
    case COMMANDE_HISTORY: 
            commande_history();break;
    case EXIT: 
            exitt();break;
    case CD: 
            cd();break;
    case HISTORY: 
            history();break;
    case TOUCH: 
            touch();break;
    case CAT: 
            // cat();
            commande_pipe();
            break;    
            /*
             En fait, on peux excuter le commande cat en utilisant execvp().
            et si vous voulez tester le commande pipe ou redirection (example: cat file1 > file2 ou cat file1 | grep p),
            le mettez sans commantaire et supprimer cat().
               */

    case COPY: 
            copy();break;      
    default: 
            
            commande_pipe();
  }   
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

}

/*
  attendre que le pid de fils processus (l'action du commande) se termine
  */
void attent(pid_t pid)
{
    int status;
    int r = waitpid(pid,&status,0);
    if (r<0) { 
      printf("erreur de waitpid (%s)\n",strerror(errno));
    }
  
 
}





/*
  Justifier une chaine de caractere est un nombre positive ou pas , si oui retourner le nombre,
sinon, retourner -1
  */
int isNumber(char* str)
{
	int number = atoi(str);
  	int len=strlen(str);
  	int i=0;
  	while( (str[i]>='0' && str[i]<='9' )|| str[i]=='+')i++;
  	if(i==len) return number;
  	else return -1;
}



/*
  commande utilisant execvp créant un pid,  utilisant execvp
  */ 
void commande_basic(char** commande){

  // printf("commade basic\n");
  char* filename = commande_Path(commande[0]);
  if (filename == NULL) {
    printf("Command not found.\n");
    return;
  }
  else
    printf("Path de %s: %s\n", commande[0], filename);

  pid_t pid;
    pid = fork();
    if (pid < 0) {
      printf("fork s'echoue (%s)\n",strerror(errno));
      return;
    }

    if (pid==0) { // fils

                                 
        redirection(commande);    // si il y a ">" ou "<", on fait redirection  

        if(execvp(commande[0], commande )==-1)
        printf("impossible d'execute \"%s\" (%s)\n",elems[0],strerror(errno));
      exit(1);
    }
    else {
      attent(pid);
      }
}

/*
  justifier est qu'il a un ">", si oui, retourner son place, sinon retourner 0 
  */
void redirection(char** commande){

  int i=0,j=0;                // i pour stocker le place de ">", j pour "<"
  int temp1=0,temp2=0;
  while(commande[i]!=NULL){
    if(strcmp(commande[i],">")==0){
      elems[i] =NULL;
      temp1=1;
      break;
    }
    if(strcmp(commande[j],"<")==0){
      elems[i] =NULL;
      temp2=1;
      break;
    }

    i++;j++;
  }

  if(temp1==0) i=0;
  if(temp2==0) j=0;

  int fid;
    if (temp1 == 1) {
      fid = open(commande[i+1], O_CREAT | O_TRUNC | O_WRONLY, 0600);  // commande[i+1] est le nom de file
      dup2(fid, 1);           // mettre sortie ver file
      
    } else if (temp2== 1) {
      fid = open(commande[j+1],O_RDONLY, 0600);         // commande[i+1] est le nom de file
      dup2(fid, 0);           // mettre file  comme l'entre  au processu
    }
    close(fid);
    return;

}


/*
le commande history and history n
  */
void history()
{	
	int n=5;   // nombre du commande pour afficher
  	if(elems[2] != NULL)
  	{
  		printf("Trop de parametres");
  		return;
  	}
  	if(elems[1] != NULL){
  		n=isNumber(elems[1]);
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
void cd()
{
	if(elems[1] == NULL || strcmp(elems[1],"~")==0 ){
            chdir(getenv("HOME"));
          }
    else  chdir(elems[1]);
}


/*
  le commande touch file et touch -d file
  */
void touch()
{
	if(elems[3]==NULL)
    {
    	struct stat stat_file;
    	// c'est touch file 
    	if(elems[2] ==NULL)
    	{
    		stat(elems[1],&stat_file);
    		//s'il exist , on doit changer sa date de modification
    		time_t lt;
			   lt=time(NULL);
			   stat_file.st_ctime=lt;
    	}
    	// c'est touch -d file
   		else if (strcmp("-d",elems[1])==0)
    	{
    		  // si le file  n'exist pas, on le cree, et donne la date maintanant
    		  if(stat(elems[2],&stat_file)== -1)
    		  {
    			   time_t t;
				      t=time(NULL);
    			   printf("%s : %s",elems[2], ctime(&t));
   			  }
   			  // s'il exist deja , on affich la date de derniere modification 	
    		  else 
    		  {
    		  	printf("%s : %s",elems[2],ctime(&(stat_file.st_ctime)));
    		  }
    		  return ;

    	}
    	else
    		{
    			printf("Les parametres ne sont pas correct\n");
    		}	
    }
  else
    {
    	printf("Trop de parametres\n");
    }     
}

/*
   le commande !! et !n ，n est un entier positive ou negative
   */
void commande_history()
{
  if(elems[0][1]!='\0')
    {
      int n; // numéro du commande , si c'est negative ,ca veut dire le n derniere commande
      if( cmdHisC >=2 && elems[0][1] == '!' && elems[0][2]=='\0')
      {
      	cmdHisC -- ;
        strcpy(ligne,cmdsHistory[cmdHisC-1]);
        decoupe();
      }
      else
      {
        elems[0]++;  
        int n = isNumber(elems[0]);
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
  le commande cat file , cat -d file et cat [-d] file1 - file2 [- file3]
  */
void cat()
{
  if(elems[1]!=NULL)
    {
      int flag=0;  // pour justifier -d
      if(strcmp("-n",elems[1])==0)
      {
        flag =1;
      }
      int i= flag+1;
      while(elems[i] != NULL)
      {
        // printf("%s\n", elems[i]);
        FILE *file;  
        file = fopen(elems[i], "r");  
        char ch;  
        if(file==NULL){
          printf( "Il n'y a pas file: %s\n", elems[i]);  //源文件不存在的时候提示错误
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
void copy()
{

    // check if the entre is right or not
  if( elems[2]==NULL || elems[3] != NULL)
  {
    printf ("there are not enough or too many parameters ");
    return ;
  }

  // check which kind of document for copy
  struct stat buf;
    stat(elems[1], &buf);
    if(S_ISREG (buf.st_mode))
    {
      Copy_f(elems[1],elems[2]);
    }
    else if(S_ISDIR(buf.st_mode))
    {
    Copy_dir(elems[1],elems[2]);
    }
    else 
    {
      printf("%s is not a file, and not a folder  ",elems[1]);
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
  trouver chaque path de command basic utilisant execvp
*/
char* commande_Path(char* command){
  FILE *fpin;
  char p[1024];
  char *temp = (char *)calloc(1024, sizeof(char));

  path = (char *) getenv("PATH");

  strcpy ( p, path );
  path = strtok ( p, ":" );   // trouver le premiere path
  while ( path != NULL ) {
    strcpy ( temp, path );  // stocker le premiere path
    strcat ( temp, "/" );     
    strcat ( temp, command );   //  path/filename
    if ( ( fpin = fopen ( temp, "r" ) ) == NULL ) {
      path = strtok (NULL, ":" );  // le prochain path
    } else {
      break;  
    }
  }
  if ( fpin == NULL ) {
    return NULL;
  } else {
    return temp;
  }
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
void commande_pipe() {
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
                          
            commande_pipe();           // s'il y a deux ou plus pipes (|) ,utiliserz commande_pipe() recursivement
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
    attent(pid);
  }

}






