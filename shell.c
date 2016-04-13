#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/wait.h>   /* pour avoir wait & co. */
#include <ctype.h>      /* pour avoir isspace & co. */
#include <string.h>
#include <errno.h>      /* pour avoir errno */

#include <pwd.h>

char   ligne[4096];     /* contient la ligne d'entrÃ©e */

/* XXX pointeurs sur les mots de ligne (voir decoupe) */
#define MAXELEMS 32
char* elems[MAXELEMS];

char *builtin_str[] = {
  "cd",
  "help",
  "exit"
};


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

void lit_ligne()
{
  if (!fgets(ligne,sizeof(ligne)-1,stdin)) {
    printf("\n");
    exit(0);
  }
}

/* XXX 
   dÃ©coupe ligne en mots 
   fait pointer chaque elems[i] sur un mot diffÃ©rent
   elems se termine par NULL
 */
void decoupe()
{
  char* debut = ligne;
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

  elems[i] = NULL;
}

void attent(pid_t pid)
{
  while (1) {
    int status;
    int r = waitpid(pid,&status,0);
    if (r<0) { 
      if (errno==EINTR) continue;
      printf("erreur de waitpid (%s)\n",strerror(errno));
      break;
    }
    if (WIFEXITED(status))
      printf("terminaison normale, status %i\n",WEXITSTATUS(status));
    if (WIFSIGNALED(status))
      printf("terminaison par signal %i\n",WTERMSIG(status));
    break;
  }
}

void execute()
{

  if(strcmp(builtin_str[0],elems[0])==0) 
  {

    if(elems[1] == NULL || strcmp(elems[1],"~")==0 ){

            chdir(getenv("HOME"));
         }
         else  chdir(elems[1]);
    
  }
  else 
  {
    pid_t pid;
    if (!elems[0]) return; /* ligne vide */

    pid = fork();
    if (pid < 0) {
      printf("fork s'echoue (%s)\n",strerror(errno));
      return;
    }

    if (pid==0) {
    /* XXX fils */
        execvp(elems[0], /* programme a  executer */
          elems     /* argv du programme a  executer */
          );
        printf("impossible d'execute \"%s\" (%s)\n",elems[0],strerror(errno));
    exit(1);
    }
    else {
      /* pÃ¨re */
      attent(pid);
    }
  }
}

int main()
{
  while (1) {
    affiche_invite();
    lit_ligne();
    decoupe();
    execute();
  }
  return 0;
}
