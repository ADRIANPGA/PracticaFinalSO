/* Inclusion de las librerias necesarias */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>

#define TRUE  1

/* Definicion de las funciones */
int calculaAleatorios(int min, int max);
void llegaSolicitudInv(int s);
void llegaSolicitudQR(int s);
void llegaCambioValores(int s);
void llegaFinalizacion(int s);
void escribirEnLog(char* id, char* mensaje);

/* Variables globales */
FILE *logFile;
int count;

struct Usuario{
	int id;
	int atendido;
	int tipo;
};
int numSolicitudes = 15;
int numAtendedores = 1;

struct Usuario *listaDeUsuarios;
struct Usuario listaActividad[4];

/* Funcion principal */
int main(int argc, char const *argv[]) {
	printf("Funka esto.\n");

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
            	printf("valor cambiado a %d.\n\n", atoi(argv[1]));
            	numSolicitudes = atoi(argv[1]);
            }

            printf("El valor de los atendedores ");
            if (atoi(argv[2]) < 1){
            	printf("no es valido, se introdujo un %d pero el valor desde de ser mayor que 1.\n", atoi(argv[2]));
            	printf("Se inicializara al valor por defecto (1 atendedor).\n\n");
            } else{
            	printf("valor cambiado a %d.\n\n", atoi(argv[2]));
            	numAtendedores = atoi(argv[2]);
            }
        break;
        default:
            /* Si se introducen más de 2 parametros, se muestra un error y se procede a la ejecución con los valores por defecto. */
        	printf("Error en los parametros de entrada (%d de 2) : Se inicializaran los parametros a los valores por defecto (15 solicitudes y 1 atendedor).\n", argc-1);
        break;
	}


	listaDeUsuarios = (struct Usuario*)malloc(sizeof(struct Usuario)*numSolicitudes);










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
	
	while(TRUE){
		pause();
	}
	printf("xd");

}

void llegaSolicitudInv(int s){
    printf("No hay nada de solicitudes por invitacion.\n");
}

void llegaSolicitudQR(int s){
    printf("No hay nada de solicitudes por QR.\n");
}

void llegaCambioValores(int s){
    printf("No hay nada de cambio de valores.\n");
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
