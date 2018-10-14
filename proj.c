#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>//
#include <sys/ipc.h>//messege queue
#include <sys/msg.h>//messege queue
#include <sys/mman.h> //MMF

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>	

//--------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------
//---------------------------------  Trabalho realizado por:                      ------------------------------------
//-------------------------------------                                           ------------------------------------
//------------------------------------- Lara Micaela Pereira da Costa e Fonseca Trindade    2015235594----------------
//------------------------------------- Maria Santos Paes           					    2015235863 ---------------
//-------------------------------------                                           ------------------------------------
//--------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------

#define DEBUG //comentar para remover debug messages

#define MAX 1000
#define PIPE_NAME "pipe"
#define BUFFSIZE 512
#define LOGFILE 16777216

typedef struct{
	int triage;
	int doctors;
	int shift_length;
	int mq_max;
}Config;
Config* conf;

typedef struct{
	int nmr_pac_triados;
	int nmr_pac_atendidos;
	long temp_antes_triage;
	long temp_triage_atend;
	long temp_total;
}Estatisticas;
Estatisticas* estats;

typedef struct{
	char nome[10];
	int nmr_pac;
	int t_triage;
	int t_atendimento;
	int prioridade;
	int op;
	int thp;//receber o numero de threads que agora deve existir
}Paciente;
Paciente p;

int marcador;

typedef struct{
	long mytype;//prioridade
	char nome[10];
	int nmr_pac;
	int t_triage;
	int t_atendimento;

	long tempo_antes_t;
	long tempo_entre_t_a;
	long tempo_t;
}Message_queue;
Message_queue mg;//estrutura para a msg queue triage

void escrita_mem();

/***THREADS***/
pthread_t* threads, *pool, *pool_u;
int* ids_thread, *id_pool, *id_pool_u;
sem_t semaf_t;
int lock;

/***PROCESSOS***/
pid_t *processos_doctors, processo;
sem_t semaf_p;

/***MEMORIA***/
int shmid;

/***NAMED PIPE***/
int fd;
char buf[BUFFSIZE];

/***MSG QUEUE***/
int mq_id_t;//fila FIFO
int mq_id_p;//fila ordenada

/***MMF***/
char* log_file_ptr;
char log_str[MAX], buff[BUFFSIZE];
int log_fd;
FILE * log_file;


void* worker();
void cria_pool_threads(int nmr_threads);
void destroy_pool();
void imprime_stats();
int nmr_messagens(int mq_id_p);
int nmr_p_antes_triagem=0;

#ifdef DEBUG
#endif

void my_handler(int signum){
    if (signum == SIGUSR1)
    	imprime_stats();
}

void cria_msg_queue_triage(){
	if( (mq_id_t = msgget(IPC_PRIVATE, IPC_CREAT|0700)) <0){
		perror("ERRO: A criar a msg queue para a triagem!\n");
		exit(-1);
	}
}

void cria_msg_queue_doctor(){
	if( (mq_id_p = msgget(IPC_PRIVATE, IPC_CREAT|0700)) <0){
		perror("ERRO: A criar a msg queue para o atendimento!\n");
		exit(-1);
	}
}

void cria_pipe(){
	if ((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST))
	{
		perror("ERRO: A criar a named pipe.");
		exit(-1);
	}
}

void abrir_ficheiro_log(){//MMF
	log_file = fopen("l.log","w");
	if((log_fd = open ("l.log", O_RDWR | O_CREAT , 0600)) < 0){
		perror("ERRO: A abrir o descritor do ficheiro log");
		fclose(log_file);
		exit(-1);
	}

	if((log_file_ptr = mmap(0, LOGFILE, PROT_WRITE, MAP_SHARED, log_fd, 0)) == (caddr_t) -1){
		perror("ERRO: A mapear o ficheiro l.log");
		exit(-1);
	}
}

void cria_pool_threads(int nmr_threads) {
	int i;

	if ((threads = malloc(nmr_threads * sizeof(pthread_t) )) == NULL){
		perror("ERRO: Não foi possivel alocar memoria para as threads. \n");
        exit(-1);
	}

	ids_thread = malloc(nmr_threads * sizeof (int));
  	
  	for(i=0; i<nmr_threads ; i++)
  	{
    	ids_thread[i] = i;

    	if(pthread_create(&threads[i], NULL, worker, &ids_thread[i]) != 0 )
    	{
	      	perror("ERRO: Na thread!");
	      	exit(1);
    	}
    }
}

int nmr_aleatorio(){//nome das pessoas que vêm em grupo
	int x;
	x = rand() % 10000;//gera valores aleatórios entre zero e 10000
	return x;
}

void ler_receber_pipe(){
	Paciente pac_;//estrutura da pipe
	int i;

    if((fd = open(PIPE_NAME, O_RDWR)) < 0){
        perror("ERRO:  A abrir a pipe para leitura.");
        exit(0);
    }

	while(lock!=1){
		read(fd, &pac_, sizeof(Paciente));//le a estrutura que foi enviada pela pipe
	    if (pac_.op == 1){//um paciente
	    	time_t tempo = time(NULL);//tempo

	    	strcpy(p.nome, pac_.nome);
	    	p.nmr_pac = pac_.nmr_pac;
	    	p.t_triage = pac_.t_triage;
	    	p.t_atendimento = pac_.t_atendimento;
	    	p.prioridade = pac_.prioridade;

	    	//colocar os dados da estrutura de pacientes na estrutura da MESSAGE QUEUE
	    	mg.mytype = (long) p.prioridade;
	    	strcpy(mg.nome, p.nome);
	    	mg.nmr_pac = p.nmr_pac;//um paciente
	    	mg.t_triage = p.t_triage;//tempo na traigem
	    	mg.t_atendimento = p.t_atendimento;//tempo no atendimento

	    	mg.tempo_antes_t = (long) time(NULL);//(long)tempo;//tempo que demorou desde que chegou(este momento) até a triagem
	    	mg.tempo_entre_t_a = 0;//tempo entre a pos triagem e o atendimeto
			mg.tempo_t = (long)tempo;//tempo total

	    	if ((msgsnd(mq_id_t, &mg, sizeof(/*struct Message_queue*/mg) - sizeof(long), 0)) == -1) //MSG QUEUE fila de espera entre a chegada e a triagem
	    		perror("ERRO: A enviar a mensagem inicial\n");

	    	nmr_p_antes_triagem = nmr_messagens(mq_id_t) + nmr_p_antes_triagem;
	    }
	    
	    else if (pac_.op == 2){//um grupo de pacientes
	    	strcpy(p.nome, pac_.nome);
	    	//p.nmr_pac = pac_.nmr_pac;
	    	p.t_triage = pac_.t_triage;
	    	p.t_atendimento = pac_.t_atendimento;
	    	p.prioridade = pac_.prioridade;

			for (i=0; i<pac_.nmr_pac; i++){//transforma o grupo em pessoas individuais
				int n;
				time_t tempo = time(NULL);//tempo

				n = nmr_aleatorio();
				mg.mytype = (long) p.prioridade;
				sprintf(p.nome,"%d",n);//converter nmr para string
		    	strcpy(mg.nome,p.nome);
		    	mg.nmr_pac = 1;//um paciente
		    	mg.t_triage = p.t_triage;//tempo de traigem
		    	mg.t_atendimento = p.t_atendimento;//tempo de atendimento

		    	mg.tempo_antes_t = (long)tempo;//tempo que demorou desde que chegou(este momento) até a triagem
		    	mg.tempo_entre_t_a = 0;//tempo entre a pos triagem e o atendimeto
				mg.tempo_t = (long)tempo;//tempo total

		    	msgsnd(mq_id_t, &mg, sizeof(mg) - sizeof(long), 0);//MSG QUEUE -> envia para a thread *worker para ser tirado(um de cada vez)
		    	nmr_p_antes_triagem = nmr_messagens(mq_id_t) + nmr_p_antes_triagem;
			}
	    }

	    else if (pac_.op == 3){//threads
	    	p.thp = pac_.thp;
	    	sem_wait(&semaf_t);
	        destroy_pool();
	    	conf->triage = p.thp;	    	
	        cria_pool_threads(p.thp);
	        sem_wait(&semaf_t);
	    }
	}
}

void *func_pipe(){
	#ifdef DEBUG
    printf("A iniciar a thread da pipe!\n");
	#endif
    
    while(1)
    {
        ler_receber_pipe();
        pthread_exit(NULL); 
    }
}

void cria_thread_pipe(){
    
    if ((pool =  malloc(sizeof(pthread_t) )) == NULL){
        perror("ERRO: Não foi possível alocar memória para a pool. \n");
        exit(-1);
    }

    if ((id_pool = malloc(sizeof(int)))==NULL)
    {
        perror("ERRO: Não foi possível alocar memória para id_pool. \n");
        exit(-1);
    }
    
    id_pool[0]=0;
    pthread_create(pool,NULL,func_pipe,&id_pool[0]);  
}	

void ler_config(Config* conf) {
	char *token, linha[4][MAX];
	int i=0, l=0;

	FILE *fich = fopen("config.txt","r");
	if (fich == NULL)
	{
		perror("ERRO ao abrir a ficheiro config.txt.");
	    exit(1);
	}

	while(fgets(linha[i], MAX, fich) != NULL)
	{
	    i++;
	}

	fclose(fich);

	for(l=0; l< i ; l++)
	{
		token = strtok(linha[l], "=");
	    token = strtok(NULL,"=");
	    strtok(token,"\n");

	    if(l==0)
	    	conf->triage = atoi(token);
	    else if(l==1)
	    	conf->doctors = atoi(token);
	    else if(l==2)
	    	conf->shift_length = atoi(token);
	    else
	    	conf->mq_max = atoi(token);
	}
	#ifdef DEBUG
	printf("Triagem = %d\nDoctors = %d\nShift length = %d\nMq max = %d\n", conf->triage, conf->doctors, conf->shift_length, conf->mq_max);
	#endif
}

void criar_semaforos_processos(){
	if( (sem_init(&semaf_p, 1,1) ) == -1)//segundo parametro indica que é para processos(1), threads (0)
	{
		perror("ERRO ao criar o semaforo.\n");
		exit(-1);
	}
}

void criar_semaforos_threads(){

	if( (sem_init(&semaf_t, 0,1) ) == -1)//segundo parametro indica que é para processos(1), threads (0)
	{
		perror("ERRO ao criar o semaforo.\n");
		exit(-1);
	}
}

int nmr_messagens(int mq){//verifica se a message queue esta vazia ou nao
	struct msqid_ds buf;
	int nmr_msg;

	if((msgctl(mq, IPC_STAT, &buf)) != 0){
		return 0;
	}

	nmr_msg = buf.msg_qnum;
	return nmr_msg;
}

void* worker(void* id_ptr) {//TRIAGEM(uma pessoa em cada thread)
	int id = *((int *)id_ptr);
	#ifdef DEBUG
	printf("Thread numero %d inicializada.\n", id);
	#endif

	//escreve no MMF
	sprintf(buff,"Thread numero %d inicializada.\n", id);
	write(log_fd, buff , strlen(buff));

	while(1){
		if(nmr_messagens(mq_id_t) != 0){

			msgrcv(mq_id_t, &mg, sizeof(mg)-sizeof(long), 0, 0);// first message in the queue with the lowest type value is read.vai tirar todos os 1's primeiro, depois os 2's e depois os 3's

			//printf("time NULL: %ld\n",(long)time(NULL) );
			//printf("time antes: %ld\n", mg.tempo_antes_t );

			mg.tempo_antes_t = (long)time(NULL) - mg.tempo_antes_t;
			//printf("---------->tempo antes da triage %ld\n", mg.tempo_antes_t);

			sleep(mg.t_triage/1000);//fica aqui o tempo da triagem

			mg.tempo_entre_t_a = (long) time(NULL);//tempo atual que vai sair da triagem
			//printf("agora vai Para o atendimento %ld\n",mg.tempo_entre_t_a);

			estats->nmr_pac_triados = estats->nmr_pac_triados + mg.nmr_pac;

			if ((msgsnd(mq_id_p, &mg, sizeof(mg) - sizeof(long), 0)) == -1) //MSG QUEUE fila de espera entre a chegada e a triagem
			    perror("ERRO: a enviar a mensagem para a MQ para ou processos doutores\n");
		}

		if(lock == 1)
			pthread_exit(NULL);
		
	}
}

void turno(int i) {
	int pid;
	time_t tempo = time(NULL);

	while(( time(NULL) - (long)tempo) < conf->shift_length)//tempo do turno
	{		
		if(nmr_messagens(mq_id_p) != 0)//se a msg queue nao estiver vazia
		{
			sem_wait(&semaf_p);//so um processo de cada vez é que pode aceder
			if( (msgrcv(mq_id_p, &mg, sizeof(mg)-sizeof(long), -3, 0)) == -1)// Se receber um msg retira a -> first message in the queue with the lowest type value is read.
				perror("ERRO: Não recebeu bem o paciente no processo doutor");

			/*#ifdef DEBUG
			printf("Paciente recebido: nome %s | %ld. Início do atendimento.\n", mg.nome, mg.mytype);
			#endif*/

			//escreve no MMF
			sprintf(buff,"Paciente recebido: nome %s | %ld. Início do atendimento.\n", mg.nome, mg.mytype);
			write(log_fd, buff , strlen(buff));

			//printf("->%s<- %ld \n",mg.nome, mg.mytype );
			//printf("---------->tempo atual %ld\nchegou ao atendimento%ld\n",(long)time(NULL), mg.tempo_entre_t_a);
			mg.tempo_entre_t_a = (long)time(NULL) - mg.tempo_entre_t_a;//tempo que demorou da triagem até ser atendido
			//printf("******tempo entre a triag e o atend %ld\n", mg.tempo_entre_t_a);

			sleep(mg.t_atendimento/1000);//fica aqui o tempo de atendimento
			mg.tempo_t = time(NULL) - mg.tempo_t;//tempo que demorou desde que chegou ate ao fim do atendimento

			sprintf(buff,"Paciente recebido: nome %s | %ld. Fim do atendimento.\n", mg.nome, mg.mytype);
			write(log_fd, buff , strlen(buff));

			/*#ifdef DEBUG
			printf("Paciente recebido: nome %s | %ld. Fim do atendimento.\n", mg.nome, mg.mytype);
			#endif*/

			//printf("~~~~tempo total %ld\n", mg.tempo_t);
			//mg.tempo_t = mg.tempo_t + mg.t_triage + mg.t_atendimento;
			//escreve na memória partilhada as informaçoes

			estats->temp_antes_triage = estats->temp_antes_triage + mg.tempo_antes_t;
			//printf("valor grande?%ld \n", estats->temp_triage_atend);
			estats->temp_triage_atend = estats->temp_triage_atend + mg.tempo_entre_t_a;
			estats->temp_total = estats->temp_total + mg.tempo_t;
		    estats->nmr_pac_atendidos = estats->nmr_pac_atendidos + mg.nmr_pac;
			sem_post(&semaf_p);
		}
	}

	pid = getpid();
	processo = fork();

	if( processo == -1 ){
    	perror("ERRO: A fazer o fork");
    	exit(1);
    }

   else if(processo == 0) {
    	#ifdef DEBUG
    	printf("Turno do doutor numero %d inicializado. ->%d\n",getpid(), getppid() );
    	#endif
    	sprintf(buff,"Turno do doutor numero %d inicializado.\n", getpid());
		write(log_fd, buff , strlen(buff));
    	processos_doctors[i] = processo;
    	turno(i);

    }
    #ifdef DEBUG
    printf("Turno do doutor numero %d terminado.\n",pid );
    #endif
    sprintf(buff,"Turno do doutor numero %d terminado.\n", pid);
	write(log_fd, buff , strlen(buff));

    exit(0);
}

void wait_turno(){//espera que os processos filhos acabem
	wait(NULL);
}

void cria_processos(int nmr_processos){
	int i=1;
	processos_doctors = (pid_t *) malloc(sizeof(pid_t) * nmr_processos +1);

	for(i=0; i<=nmr_processos ; i++)
	{
		processo = fork();
		if( processo == -1 )
	    {
	    	perror("ERRO: A fazer o fork.");
	    	exit(1);
	    }

	   else if(processo == 0) 
	    {
	    	#ifdef DEBUG
	    	printf("Turno do doutor numero %d inicializado, pai %d.\n",getpid(), getppid() );
	    	#endif
	    	processos_doctors[i] = processo;
	    	turno(i);
	    }
	}
	for(i=0; i<nmr_processos ; i++){
		wait(NULL);
	}
}

void verifica_pac(){
	int f=0;

	while(lock!=1){

		if( ((nmr_messagens(mq_id_t)) > conf->mq_max) && f==0)
		{
			processo = fork();
			if( processo == -1 )
		    {
		    	perror("ERRO: A fazer o fork.\n");
		    	exit(-1);
		    }

		    if(processo == 0){
		    	#ifdef DEBUG
		    	printf("Turno EXTRA DOUTOR numero %d inicializado.\n",getpid() );
		    	#endif
		    	processos_doctors[0] = processo;
		    }
		    f=1;
		}

		if ( (nmr_messagens(mq_id_t)) <conf->mq_max*0.8 && f==1)//verificar constantemente o nmr de pacientes da message queue da fila para os processos
		{	
			wait(NULL);
			printf("Turno EXTRA DOUTOR numero %d terminado.\n",getpid() );
			f=0;
		}
	}
}

void *func_ultimate(){
	#ifdef DEBUG
    printf("A iniciar a thread que verifica quantos pacientes estao na fila para serem atendidos!\n");
	#endif
    
    while(lock!=1)
    {
        verifica_pac();
    }
    pthread_exit(NULL); 
}

void cria_thread_doctor(){
    
    if ((pool_u =  malloc(sizeof(pthread_t) )) == NULL){
        perror("ERRO: Não foi possível alocar memória para a pool. \n");
        exit(-1);
    }

    if ((id_pool_u = malloc(sizeof(int)))==NULL)
    {
        perror("ERRO: Não foi possível alocar memória para id_pool. \n");
        exit(-1);
    }
    
    id_pool_u[0]=0;
    pthread_create(pool,NULL,func_ultimate,&id_pool_u[0]);  
}

void cria_mem_partilhada_estatisticas() {

	shmid = shmget(IPC_PRIVATE, sizeof(Estatisticas), IPC_CREAT | 0766);//criar memória partilhada 
	if (shmid == -1)
	{
		perror("ERRO: Ao criar a memoria partilhada das estatisticas.\n");
		exit(1);
	}

	estats = (Estatisticas*) shmat(shmid, NULL, 0);//mapear memoria
	if (estats == (Estatisticas*)-1)
	{
        perror("ERRO: A mapear a memoria partilhada das estatisticas.");
        exit(1);
    }

    estats->nmr_pac_triados = 0;
    estats->nmr_pac_atendidos = 0;
    estats->temp_antes_triage = 0;
    estats->temp_triage_atend = 0;
    estats->temp_total = 0;
}

void destroy_pool(){
	int i;
	//aguardar que quem esta a ser triado acabe e colocar uma flag a dizer que é possível destruir as threads
	for(i=0; i<(conf->triage); i++){
		pthread_cancel(threads[i]);
		#ifdef DEBUG
        printf("Thread %d a ser cancelada\n", i);
        #endif
        //escreve no MMF
		sprintf(buff,"Thread %d a ser cancelada\n", i);
		write(log_fd, buff , strlen(buff));
	}

	for(i=0; i < (conf->triage); i++)
	{
        pthread_join(threads[i],NULL);
        #ifdef DEBUG
        printf("Thread %d a terminar!\n", i);
        #endif
        sprintf(buff,"Thread %d a terminar!\n", i);
		write(log_fd, buff , strlen(buff));
    }

    free(threads);
    free(ids_thread);
}
    

void imprime_stats(){

	long aux1=0,aux2=0,aux3=0;
	long a;

	if(lock!=1){
		//printf("onde\n");
		sem_wait(&semaf_p);
		//printf("here\n");
		//sem_wait(&semaf_t);
	}

	if(estats->nmr_pac_triados !=0){//tempo antes da triagem
		//estats->temp_antes_triage = (estats->temp_antes_triage) / (estats->nmr_pac_triados);
		aux1 = (estats->temp_antes_triage) / (long) nmr_p_antes_triagem;//a dividir pelo nmr de pessoas que chegou a p message queue
		if(lock==1)
			estats->temp_antes_triage = aux1;
	}

	if(estats->nmr_pac_triados !=0){
		//estats->temp_triage_atend = estats->temp_triage_atend / estats->nmr_pac_triados;;
		a = estats->temp_triage_atend / (long) estats->nmr_pac_triados;;
		aux2 = time(NULL) - a;
		//printf("???? %ld\n", estats->temp_triage_atend);
		if(lock==1)
			estats->temp_antes_triage = aux2;
	}

	if(estats->nmr_pac_atendidos !=0){
		//estats->temp_total = estats->temp_total / estats->nmr_pac_atendidos;
		aux3 = estats->temp_total / (long) estats->nmr_pac_atendidos;
		if(lock==1)
			estats->temp_total = aux3;
	}

	printf("\n\n********* ESTATÍSTICAS *********\n");
	printf("Número total de pacientes triados: %d\n", estats->nmr_pac_triados);
	printf("Número total depacientes atendidos: %d\n", estats->nmr_pac_atendidos);
	if(lock!=1){
		printf("Tempo médio de espera antes do início da triagem: %ld\n", aux1);// % ??????
		printf("Tempo médio de espera entre o fim da triagem e o ínicio do atendimento: %ld\n", aux2);// %triados
		printf("Média do tempo de espera total que cada utilizador gastou desde que chegou ao sistema até sair: %ld\n", aux3);// % atendidos
		printf("\n********* FIM ESTATÍSTICAS *********\n\n");
	}

	else
	{
		printf("Tempo MÉDIO de espera antes do início da triagem: %ld\n", estats->temp_antes_triage);// % ??????
		printf("Tempo MÉDIO de espera entre o fim da triagem e o ínicio do atendimento: %ld\n", estats->temp_triage_atend);// %triados
		printf("MÉDIA do tempo de espera total que cada utilizador gastou desde que chegou ao sistema até sair: %ld\n", estats->temp_total);// % atendidos
		printf("\n********* FIM ESTATÍSTICAS *********\n\n");
		//sem_post(&semaf_t);
		sem_post(&semaf_p);

	}
	
}

void terminar(int signum) {
	int i;
	int nmr_processos = conf->doctors;
	
	unlink(PIPE_NAME);
	
	//MEMORIA
	shmdt(&estats);
	shmctl(shmid, IPC_RMID, NULL);

	lock = 1;
	//THREADS
	destroy_pool();
	pthread_cancel(pool[0]);
	sprintf(buff,"Thread %d a ser cancelada [Thread da pipe]\n", 1);
	write(log_fd, buff , strlen(buff));

    pthread_join(pool[0],NULL);
    sprintf(buff,"Thread %d a ser eliminada [Thread da pipe]\n", 1);
	write(log_fd, buff , strlen(buff));

    pthread_cancel(pool_u[0]);
    sprintf(buff,"Thread %d a ser cancelada [Thread que verifica quando pacientes estao na fila de espera]\n", 1);
	write(log_fd, buff , strlen(buff));

    pthread_join(pool_u[0],NULL);
    sprintf(buff,"Thread %d a ser eliminada [Thread que verifica quando pacientes estao na fila de espera]\n", 1);
	write(log_fd, buff , strlen(buff));
    
    free(pool_u);
    free(id_pool_u);
    free(pool);
    free(id_pool);

  	// TERMINAR OS PROCESSOS
  	for(i=0; i <= nmr_processos; i++)
  	{
  		///printf("Processo numero %d a terminar.\n",getpid() );
    	wait(NULL);
    	//kill(processos_doctors[i],SIGKILL);
  	}

	//libertar recursos de message queue
	msgctl(mq_id_t, IPC_RMID, NULL);
	msgctl(mq_id_p, IPC_RMID, NULL);

	//libertar MMF
	munmap(log_file_ptr, LOGFILE);

  	imprime_stats();
  
    free(conf);
    fclose(log_file);
  	printf("->Programa vai terminar!!<-");
  	exit(0);
}

int main(int argc, char const *argv[]) {

	signal(SIGUSR1, my_handler);
	signal(SIGCHLD,wait_turno);
	signal(SIGTSTP,SIG_IGN);//ignora o ctrl+z
	signal(SIGINT, terminar);//trata do sinal ctrl+c

	criar_semaforos_threads();
	criar_semaforos_processos();

	if((conf = (Config*) malloc(sizeof(Config))) == NULL)
	{
		perror("ERRO na alocação de memória\n");
		exit(-1);
	}

	cria_mem_partilhada_estatisticas();

	cria_pipe();
	cria_thread_pipe();
	abrir_ficheiro_log();

	cria_msg_queue_triage();
	cria_msg_queue_doctor();
	cria_thread_doctor();

	ler_config(conf);
	cria_processos(conf->doctors);
	cria_pool_threads(conf->triage);	
	
	while(1){
		continue;
	}
	return 0;
}