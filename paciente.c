#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <stdbool.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define PIPE_NAME "pipe"
#define SIZE 100	

typedef struct{
	char nome[10];
	int nmr_pac;
	int t_triage;
	int t_atendimento;
	int prioridade;
	int op;
	int thp;
}Paciente;
Paciente pac; //estrutura partilhada com a named pipe

int fd;//pipe

void sair(int sig){
	printf("\nA pipe vai fechar...\n");
	close(fd);
	printf("A pipe fechou!\n");
	exit(0);
}

//pipe
/*void cria_pipe(){
	if ((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST))
	{
		perror("Cannot create pipe: ");
		exit(0);
	}
}*/

void open_pipe(){
	if((fd = open(PIPE_NAME, O_RDWR)) < 0){
		perror("ERRO: a abrir a pipe para escrita.\n");
		exit(-1);
	}
}

/*void escreve_pipe_paciente(){

}

void escreve_pipe_grupo(){
	
}

void mudar_threads(){

}*/

int main(int argc, char const *argv[]) {
	/*pac = malloc(sizeof(Paciente));
	if (pac == NULL){
		perror("ERROR TO ALOCATE MEMORY");
		exit(1);
	}*/

	char *pal, /*aux[SIZE], nome2[SIZE],*/ *t;
	int a=0, p/*prioridade*/, at/*tempo de atendimento*/, trio/*tempo de triagem*/, paci/*nmr de pacientes*/;

	signal(SIGTSTP,SIG_IGN);//ignorar p ctrl+z
	signal(SIGINT, sair);

	open_pipe();
	printf("\n-------- PIPE COM SUCESSO -------\n");
	while(1){
		char *pal, aux[SIZE], nome2[SIZE], aux1[SIZE];
		fgets(aux,100,stdin);//aux->guardar a frase toda ex:"lara 12 12 1"
		if(isalpha(aux[0]))//se o 1 caracter for uma letra
		{
			t = strtok(aux,"=");
			strcpy(aux1,t);
			t = strtok(NULL,"=");//apanha o nmr
			pal = strtok(aux, " ");//primeiro token (nome)
			//printf("-------->%s\n",t);

			if ((strcmp(aux1,"TRIAGE")) == 0)//se for escrito TRIAGE envia o novo nmr de threads que se pretende ter
			{	
				
				//printf("aux->%s\n",t );
				pac.op=3;
				pac.thp = atoi(t);//nmr de threads
				strcpy(pac.nome, "");
				pac.nmr_pac = 0;
				pac.t_triage = 0;
				pac.t_atendimento = 0;
				pac.prioridade = 0;

				//printf("a enviar\n");
				//printf("%d\n",pac.thp);
				write(fd, &pac, sizeof(Paciente));
				//printf("Enviado\n");
			}

			else//envia um paciente
			{
				a=0;
				while( pal != NULL )
				{
					a++;
					if(a == 1){
						pac.op=1;
						strcpy(nome2,pal);
						//printf("*%s\n",nome2);
						strcpy(pac.nome, nome2);//nome do paciente
						//printf("**%s\n",pac.nome);
						pac.nmr_pac = 1;//nmr de pacientes
					}
					else if(a == 2){
						trio = atoi(pal);//tempo de triagem
						//printf("««««««««««%d\n",trio);
						pac.t_triage = trio;
						//printf("%d\n",pac.t_triage);
						
					}
					else if(a == 3){//tempo de atendimento
						at = atoi(pal);
						pac.t_atendimento = at;
						//printf("%d\n",pac.t_atendimento);
					}
					else if(a == 4){
						//printf( "->%s\n", pal );
						p = atoi(pal);
						if(p == 1 || p == 2 || p == 3){
							pac.prioridade = p;
							//printf("%d\n",pac.prioridade);
						}
						else{
							printf("ERRO: Essa prioridade nao existe!\n");
							exit(0);
						}
					}
					else if(a =! 4)
					{
						printf("ERRO: Demasiados parametros!\n");
						//exit(0);
					}
					pal = strtok(NULL, " ");
				}
				//printf("a enviar\n");
				//printf("%s\n",aux );
				write(fd, &pac, sizeof(Paciente));
				//printf("%s\n",aux );
				//printf("Enviado\n");
			}
		}

		else{//se o 1 caracter for um nmr
			a=0;
			pal = strtok(aux, " ");//primeiro token
			while( pal != NULL ) {
				a++;

				if(a == 1){
					pac.op=2;
					strcpy(pac.nome, "");//nao tem nome ainda, vao ser atribuidos individualmente no programa proj.c
					paci = atoi(pal);//nmr de pacientes
					pac.nmr_pac = paci;
				}

				else if(a == 2){
					trio = atoi(pal);//tempo de triagem
					//printf("%d\n",at);
					pac.t_triage = trio;
					//printf("%d\n",pac.t_triage);
				}

				else if(a == 3){
					at = atoi(pal);//tempo de atendimento no doutor
					pac.t_atendimento = at;
					//printf("%d\n",pac.t_atendimento);
				}

				else if(a == 4){
					//printf( "->%s\n", pal );
					p = atoi(pal);//prioridade
					if(p == 1 || p == 2 || p== 3){
						pac.prioridade = p;
						//printf("%d\n",pac.prioridade);
					}

					else{
						printf("ERRO: Essa prioridade nao existe!\n");
						exit(0);
					}
				}

				else if(a != 4)
				{
					printf("ERRO: Demasiados parametros!\n");
					//exit(0);
				}				
				//printf( "%s<-\n", pal );
				pal = strtok(NULL, " ");
			}
			//printf("a enviar\n");
			//printf("%s\n",aux );
			write(fd, &pac, sizeof(Paciente));
			//printf("Enviado\n");
		}
	}
	return 0;
}
