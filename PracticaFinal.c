/* Inclusion de las librerias necesarias */
# include <stdio.h>
# include <stdlib.h>
# include <ctype.h>
# include <time.h>
# include <pthread.h>
# include <signal.h>
# include <sys/types.h>
# include <sys/wait.h>
# include <unistd.h>
# include <errno.h>

#define TRUE  1

/* Definicion de las funciones */
int calculaAleatorios(int min, int max);
void llegaSolicitudInv(int s);
void llegaSolicitudQR(int s);
void *atenderSolicitudes(void *ptr);
void llegaCambioValores(int s);
void llegaFinalizacion(int s);
void escribirEnLog(char* id, char* mensaje);

/* Variables globales */
FILE *logFile;
int contadorAtendedor;
int contadorSolicitudes;

struct Usuario{
  //Numero con el que entra en la cola
	int id;
    //0->No 1->Si
	int atendido;
    //0->Invitacion 1->QR
	int tipo;
  pthread_t hiloUsusario; /*Hilo que ejecuta cada Usuario*/
};

struct Atendedor{
    //0->Inv 1->QR 2->PRO
    int tipo;
    int solatendidas;
  	pthread_t hiloAtendedor; /*Hilo ejecuta cada Atendedor*/
};

/*Es tupoAt porque si no @DeLaHera llora*/
int tipoAt[3] = {0,1,2};

int numSolicitudes = 15;
int numAtendedores = 1;

struct Atenderdor *listaAtendedores;
struct Usuario *listaDeUsuarios;
struct Usuario listaActividad[4];
struct Usuario *listaCoordinadores;

/* Para controlar el acceso a los recursos compartidos utilizamos dos semáforos (mutex) */
pthread_mutex_t mutexCreaHilos;
pthread_mutex_t mutexColaSolicitudesQR;
pthread_mutex_t mutexColaSolicitudesInv;
pthread_mutex_t mutexLog;
pthread_mutex_t mutex5;


/* --------------------------- MAIN --------------------------- */
/*Función principal del programa*/
int main(int argc, char const *argv[]){
   printf("-----------------------------------------------BENVINGUTS A L'TSUMANI DEMOCRÀTIC LLIONÉS-----------------------------------------------\n");
	
	/* Switch de comprobacion de los argumentos iniciales (Parte extra de parametros estáticos). */
	switch(argc){
		case 1:
      	/* En el caso de que no haya parametros se muestra un mensaje y se procede con los valores por defecto. */
				printf("No se han introducido parametros de entrada, se inicializaran al valor por defecto (15 solicitudes y 1 atendedorPRO).\n\n");
		break;
		case 2:
    			/* En el caso de que haya un solo parametro (solicitudes) se comprueba que sea válido y se cambia si así es (Si no se procede con los valores por defecto). */
          printf("Se ha introducido el valor de las solicitudes, ");
         	if (atoi(argv[1]) < 1){
            	printf("pero no era valido, se introdujo un %d pero el valor desde de ser mayor que 1.\n", atoi(argv[1]));
            	printf("Se inicializara al valor por defecto (15 solicitudes).\n\n");
          } else{
            	printf("valor cambiado a %d.\n\n", atoi(argv[1]));
            	numSolicitudes = atoi(argv[1]);
          }
      break;
        case 3:
            /* En el caso de que se hayan introducido ambos valores, se comprueban y, si son válidos, se cambian (Si no se procede con los valores por defecto). */
        	printf("Se han introducido los dos valores.\n");

        	printf("El valor de las solicitudes ");
         	if (atoi(argv[1]) < 1){
            	printf("no es valido, se introdujo un %d pero el valor desde de ser mayor que 1.\n", atoi(argv[1]));
            	printf("Se inicializara al valor por defecto (15 solicitudes).\n\n");
            } else{
            	printf("sera cambiado a %d.\n\n", atoi(argv[1]));
            	numSolicitudes = atoi(argv[1]);
            }

            printf("El valor de los atendedores ");
            if (atoi(argv[2]) < 1){
            	printf("no es valido, se introdujo un %d pero el valor desde de ser mayor que 1.\n", atoi(argv[2]));
            	printf("Se inicializara al valor por defecto (1 atendedor).\n\n");
            } else{
            	printf("sera cambiado a %d.\n\n", atoi(argv[2]));
            	numAtendedores = atoi(argv[2]);
            }
        break;
        default:
            /* Si se introducen más de 2 parametros, se muestra un error y se procede a la ejecución con los valores por defecto. */
        	printf("Error en los parametros de entrada (%d de 2) : Se inicializaran los parametros a los valores por defecto (15 solicitudes y 1 atendedor).\n", argc-1);
        break;
	}
	
  	srand(getpid());
    int numCoordinadores= (int) numeroSolicitudes/4;
		listaDeUsuarios = (struct Usuario*)malloc(sizeof(struct Usuario)*numSolicitudes);
    listaCoordinadores = (struct Usuario*)malloc(sizeof(struct Usuario)*numCoordinadores);
    listaAtendedores = (struct Atendedor*)malloc(sizeof(struct Atendedor)*(numAtendedores+2));
  	pthread_mutex_init(&mutexCola, NULL);

	/* Se definen las estructuras para las entradas de las solicitudes y se enmascaran las señales. */
	struct sigaction sInv = {0};
	sInv.sa_handler = llegaSolicitud;
  struct sigaction sQR = {0};
  sQR.sa_handler = llegaSolicitud;
  if(-1 == sigaction(SIGUSR1, &sInv, NULL)  || -1 == sigaction(SIGUSR2, &sQR, NULL) ){
      perror("Entrada de solicitudes: sigaction");
      exit(-1);
  }

	/* Se define la estructura para controlar la entrada de SIGINT y terminar la ejecucion */
	struct sigaction sFin = {0};
	sFin.sa_handler = llegaFinalizacion;
	if( -1 == sigaction(SIGINT, &sFin, NULL) ){
		perror("Finalizacion: sigaction");
		exit(-1);
	}

   	/* Se define la estructura y se enmascara SIGPIPE para la entrada de una petición de cambio de valores (Parte extra de parametros dinámicos) */
   	struct sigaction sVal = {0};
   	sVal.sa_handler = llegaCambioValores;
   	if(-1 == sigaction(SIGPIPE, &sVal, NULL) ){
      	perror("Cambio de valores: sigaction");
      	exit(-1);
    }

  /* Inicializamos los mutex */
  if (pthread_mutex_init(&mutexCreaHilos, NULL)!=0) exit(-1); 
  if (pthread_mutex_init(&mutexColaSolicitudes, NULL)!=0) exit(-1); 
  if (pthread_mutex_init(&mutexLog, NULL)!=0) exit(-1); 
  if (pthread_mutex_init(&mutex5, NULL)!=0) exit(-1); 
  
    /*Incializamos los atendedores*/
    int i;
    for (i = 0; i < (numAtendedores+2); i++) {
        switch (i){
            case 0:
                *(listaAtendedores+i)->tipo=0;
                *(listaAtendedores+i)->solatendidas=0;
                break;
            case 1:
                *(listaAtendedores+i)->tipo=1;
                *(listaAtendedores+i)->solatendidas=0;
                break;
            default:
                *(listaAtendedores+i)->tipo=2;
                *(listaAtendedores+i)->solatendidas=0;
                break;
        }
    }

    /*J representa el contador de ID's*/
    int j=0;
    for (i = 0; i < numSolicitudes; i++) {
           *(listaDeUsuarios+i)->id=j;
           *(listaDeUsuarios+i)->atendido=0;
           *(listaDeUsuarios+i)->tipo=0
           j++;
    }
		
    pthread *atpros;
    pthread atqr,atinvitacion; 
  	pthread_mutex_lock(&mutexcreahilos);
		pthread_create(&atinvitacion, NULL, atenderSolicitudes, (void *)&tipoAt[0]);
    pthread_create(&atqr, NULL, atenderSolicitudes, (void *)&tipoAt[1]);
    for(i=0;i<numAtendedores;i++){
        pthread_create(&(*(atpros+i)), NULL, atenderSolicitudes, (void *)&tipoAt[2]);
    }
  	pthread_mutex_unlock(&mutexCreaHilos);
  
  	/* Se imprime el encabezado y se espera un intro para empezar a recibir señales */
    printf("Se podra simular la entrada de invitaciones al programa mediante el uso de señales.\n");
    printf("Introduzca 'kill -10 PID' en el terminal si desea realizar una solicitud por invitacion .\n");
    printf("Introduzca 'kill -12 PID' en el terminal si desea realizar una solicitud por QR.\n");
    printf("Introduzca 'kill -2 PID' en el terminal si desea finalizar el programa.\n");
    printf("Pulse intro para continuar...\n");
   	printf("---------------------------------------------------------------------------------------------------------------------------------------\n");
   
    /* Bucle en espera de llegada de señales */
		while(TRUE){
			pause();
		}

    return 0;
}


/*-------------------------- DEFINICION DE FUNCIONES -----------------------*/
void llegaSolicitud(int signal){
  	pthread_mutex_lock(&mutexColaSolicitudes); 
    printf("No hay nada de solicitudes por QR.\n");
    if(contadorsolicitudes==numSolicitudes){
        printf("Solicitud rechazada\nLa cola esta llena\n");
    }else{
       	sleep(4);
      	usuario.id[contadorsolicitudes] = contadorsolicitudes;
      	usuario.atendido[contadorsolicitudes] = 0;
      	switch(signal){
          case SIGUSR1:
            	usuarios.tipo[cotadorsolicitudes]=0;
          case SIGUSR2:
            	usuarios.tipo[contadorsolicitudes]=1;
        }
        contadorsolicitudes++;
    }
  	pthread_mutex_unlock(&mutexColaSolicitudes);
}


void *atenderSolicitudes(void *ptr){
    //TODO Funcion del hilo que descarta las solicitudes (o no)
    switch(ptr){
        case 0:
        case 1:
        case 2:
        default:
            printf("ERROR\n");
    }

}


void llegaCambioValores(int s){
    printf("Se ha detectado una solicitud de cambio de valores.\n");
    int valorACambiar, nuevoValor, valido = 0;
        do{
            printf("¿Qué valor deseas cambiar?\n");
            printf("1- Cambiar numero de solicitudes.\n");
            printf("2- Cambiar numero de atendedores.\n");
            scanf("%d", &valorACambiar);
            if(valorACambiar != 1 && valorACambiar != 2){
                printf("Valor introducido no valido (%d). Necesario un valor entre 1 y 2.\n", valorACambiar);
                printf("Relanzando menu");
                valido = -1;
            }
        }while(valido != -1);
        do{
            if(valorACambiar == 1){
                printf("Introduce cuantas solicitudes quieres incrementar:");
            } else{
                printf("Introduce cuantos atendedores quieres incrementar:");
            }
            scanf("%d", &nuevoValor);
            if(nuevoValor < 0){
                printf("Valor introducido no valido (%d). Necesario un valor entre positivo.\n", nuevoValor);
                printf("Relanzando menu");
                valido = -1;    
            }      
        }while(valido != -1);
        if(valorACambiar == 1){
            numSolicitudes += nuevoValor;
            printf("Valor de las solicitudes incrementados en %d hasta %d.\n", nuevoValor, numSolicitudes);
        } else{
            numAtendedores += nuevoValor;
            printf("Valor de las solicitudes incrementados en %d hasta %d.\n" nuevoValor, numAtendedores);
        }
  			char mensajeLog[250];
  			sprintf(mensajeLog, "Se ha cambiado el valor de solicitudes incrementandolo en %d hasta %d.", nuevoValor, numAtendedores);
  			escribirEnLog("AVISO", mensajeLog);
        //TODO Realizar efectivamente el cambio
}


void llegaFinalizacion(int s){
  /* Se escribe en el log que ha llegado la señal para acabar y se espera a que las solicitudes ya dentro del sistema sean atendidas */
  char mensajeLog[250];
  sprintf(mensajeLog, "Ha llegado la señal de finalización, cerrada la entrada a más solicitudes.");
  escribirEnLog("AVISO", mensajeLog);
  
  //TODO cambiar los mutex para que no entren más en la cola y para que las actividades culturales ya no esten y al acabar las solicitudes mueran
  
  //TODO Despues de eso liberar los punteros abiertos con free
  printf("PRUEBA: Acabose\n");
  
  /* Se escribe en el log que se sale del programa y se sale de manera efectiva */
  sprintf(mensajeLog, "Saliendo del programa.")
  escribirEnLog("FIN", mensajeLog);
	exit(0);
}


int calculaAleatorios(int min, int max){
	return rand() % (max-min+1) + min;
}


void escribirEnLog(char * id , char * mensaje){
  	/* Se protege la escritura concurrente en el log mediante un mutex */
  	pthread_mutex_lock(&mutexLog);
  
    /* Se obtiene la fecha y hora actuales para el inicio de la linea de log */
    time_t now = time (0) ;
    struct tm * tlocal = localtime (& now );
    char stnow[19];
    strftime(stnow ,19 ," %d/ %m/ %y %H: %M: %S", tlocal);

    /* Se escribe el mensaje en el fichero Log ussando la fecha y hora y los parametros */
    logFile = fopen("registroTiempos.log" , "a");
    fprintf(logFile, "[ %s] %s: %s\n", stnow , id , mensaje);
    fclose(logFile);

  	/* Se libera el mutex para que otros hilos puedan escribir en el log */
		pthread_mutex_unlock(&mutexLog);
}

