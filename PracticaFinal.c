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
int contatendedor;
int contadorsolicitudes;

struct Usuario{
	int id;
    //0->No 1->Si
	int atendido;
    //0->No coordinador 1->Coordinador
	int tipo;
};

struct Atendedor{
    //0->Inv 1->QR 2->PRO
    int tipo;
    int solatendidas;
};

/*Es tupoAt porque si no @DeLaHera llora*/
int tipoAt[3] = {0,1,2};

int numSolicitudes = 15;
int numAtendedores = 1;

struct Atenderdor *listaAtendedores;
struct Usuario *listaDeUsuarios;
struct Usuario listaActividad[4];
struct Usuario *listaCoordinadores;

/* Funcion principal */
int main(int argc, char const *argv[]){

	/* Switch de comprobacion de los argumentos iniciales (Parte extra de parametros estáticos). */
	switch(argc){
		case 1:
            /* En el caso de que no haya parametros se muestra un mensaje y se procede con los valores por defecto. */
			printf("No se han introducido parametros de entrada.\n");
			printf("Se inicializaran al valor por defecto (15 solicitudes y 1 atendedorPRO).\n\n");
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

    int numCoordinadores= (int) numeroSolicitudes/4;
	listaDeUsuarios = (struct Usuario*)malloc(sizeof(struct Usuario)*numSolicitudes);
    listaCoordinadores = (struct Usuario*)malloc(sizeof(struct Usuario)*numCoordinadores);
    listaAtendedores = (struct Atendedor*)malloc(sizeof(struct Atendedor)*(numAtendedores+2));

	/* Se definen las estructuras para las entradas de las solicitudes y se enmascaran las señales. */
	struct sigaction sInv = {0};
	sInv.sa_handler = llegaSolicitudInv;
    struct sigaction sQR = {0};
    sQR.sa_handler = llegaSolicitudQR;
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
	pthread_create(&atinvitacion, NULL, atenderSolicitudes, (void *)&tipoAt[0]);
    pthread_create(&atqr, NULL, atenderSolicitudes, (void *)&tipoAt[1]);
    for(i=0;i<numAtendedores;i++){
        pthread_create(&(*(atpros+i)), NULL, atenderSolicitudes, (void *)&tipoAt[2]);
    }

    /* Bucle en espera de llegada de señales */
	while(TRUE){
		pause();
	}

    return 0;
}

void llegaSolicitudInv(int s){
    printf("No hay nada de solicitudes por invitacion.\n");
    if(contadorsolicitudes==numSolicitudes){
        printf("Solicitud rechazada\nLa cola esta llena\n");
    }else{
        contadorsolicitudes++;
    }
    sleep(4);
}

void llegaSolicitudQR(int s){
    printf("No hay nada de solicitudes por QR.\n");
    if(contadorsolicitudes==numSolicitudes){
        printf("Solicitud rechazada\nLa cola esta llena\n");
    }else{
        contadorsolicitudes++;
    }
    sleep(4);
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
        //TODO Escribir en el log el cambio.
}

void llegaFinalizacion(int s){
	printf("Acabose.\n");
	exit(0);
}

int calculaAleatorios(int min, int max){
	return rand() % (max-min+1) + min;
}

void escribirEnLog(char * id , char * mensaje){
    // Calculamos la hora actual
    time_t now = time (0) ;
    struct tm * tlocal = localtime (& now );
    char stnow[19];

    strftime(stnow ,19 ," %d/ %m/ %y %H: %M: %S", tlocal);

    // Escribimos en el log
    logFile = fopen("registroTiempos.log" , "a");
    fprintf(logFile, "[ %s] %s: %s\n", stnow , id , mensaje);
    fclose(logFile);
}
