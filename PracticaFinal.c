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
void llegaSolicitud(int s);
void *accionesSolicitud(void *ptr);
void *accionesAtendedor(void *ptr);
void *accionesCoordinadorSocial(void *ptr);
void llegaCambioValores(int s);
void llegaFinalizacion(int s);
void escribirEnLog(char* id, char* mensaje);

/* Variables globales */
FILE *logFile;

int numSolicitudes = 15;
int numAtendedores = 1;

int contadorAtendedor;
int contadorSolicitudes;
int contadorActividad;
int tipoAt[3] = {0,1,2};

struct Usuario{	/*Numero con el que entra en la cola*/
	int id;/*0->No 1->Si */
	int atendido;	/*0->Invitacion 1->QR */
	int tipo;	/*0->Todo OK 1->Error en datos personales 2->Antecedentes 3->No ha sido atendido*/
int tipoAtencion;
	pthread_t hiloUsuario; /*Hilo que ejecuta cada Usuario*/
};

struct Atendedor{
	int tipo; //0->Inv 1->QR 2->PRO
	int solAtendidas;
	int atendiendo; //0->No 1->Si
	pthread_t hiloAtendedor; /*Hilo ejecuta cada Atendedor*/
};

struct Atendedor *listaAtendedores;
struct Usuario *listaDeUsuarios; //esto es lo mismo listaDeSolicitudes
struct Usuario listaActividad[4];

/* Para controlar el acceso a los recursos compartidos utilizamos dos semáforos (mutex) */
pthread_mutex_t mutexCreaHilos;
pthread_mutex_t mutexColaSolicitudes;
pthread_mutex_t mutexLog;
pthread_mutex_t mutexSolicitudes;
pthread_mutex_t mutexActividad;
pthread_mutex_t mutexColaAtendedores;

/*Variables condición*/
pthread_cond_t condicionIniciarActividad;
pthread_cond_t condicionAcabarActividad;


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
	logFile = fopen("registroTiempos.log" , "wt");
	fclose(logFile);
	listaDeUsuarios = (struct Usuario*)malloc(sizeof(struct Usuario)*numSolicitudes);
	listaAtendedores = (struct Atendedor*)malloc(sizeof(struct Atendedor)*(numAtendedores+2));
	printf("Definidas las colas\n");

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
	printf("Asignadas handlers\n");

  /* Inicializamos los mutex */
	if (pthread_mutex_init(&mutexCreaHilos, NULL)!=0) exit(-1); 
	if (pthread_mutex_init(&mutexColaSolicitudes, NULL)!=0) exit(-1); 
	if (pthread_mutex_init(&mutexLog, NULL)!=0) exit(-1); 
	if (pthread_mutex_init(&mutexSolicitudes, NULL)!=0) exit(-1); 
	if (pthread_mutex_init(&mutexActividad, NULL)!=0) exit(-1);
	if (pthread_mutex_init(&mutexColaAtendedores, NULL)!=0) exit(-1);
	

  /* Inicializamos las variables condición */
	if (pthread_cond_init(&condicionIniciarActividad, NULL)!=0) exit(-1);
	if (pthread_cond_init(&condicionAcabarActividad, NULL)!=0) exit(-1);

	printf("Asignados mutex y variales condicion\n");

	/*Incializamos los atendedores*/
	int i;
	for (i = 0; i < (numAtendedores+2); i++) {
		switch (i){
			case 0:
			(listaAtendedores+i)->tipo=0;
			(listaAtendedores+i)->solAtendidas=0;
			(listaAtendedores+i)->atendiendo=0;
			break;
			case 1:
			(listaAtendedores+i)->tipo=1;
			(listaAtendedores+i)->solAtendidas=0;
			(listaAtendedores+i)->atendiendo=0;
			break;
			default:
			(listaAtendedores+i)->tipo=2;
			(listaAtendedores+i)->solAtendidas=0;
			(listaAtendedores+i)->atendiendo=0;
		}
	}

	printf("Iniciados los atendedores\n");

	/*J representa el contador de ID's*/
	int j=0;
	for (i = 0; i < numSolicitudes; i++) {
		(listaDeUsuarios+i)->atendido=1;
		j++;
	}

	printf("Iniciadas las solicitudes\n");
	
	pthread_t *atpros;
	pthread_t atqr,atinvitacion; 
	pthread_mutex_lock(&mutexCreaHilos);
	/* Casteamos el int a el void pointer */
	pthread_create(&listaAtendedores[0].hiloAtendedor, NULL, accionesAtendedor, (void *)(intptr_t)tipoAt[0]);
	pthread_create(&listaAtendedores[1].hiloAtendedor, NULL, accionesAtendedor, (void *)(intptr_t)tipoAt[1]);
	for(i=0;i<numAtendedores;i++){
		printf("creado %d\n", +i);
		pthread_create(&listaAtendedores[i].hiloAtendedor, NULL, accionesAtendedor, (void *)(intptr_t)tipoAt[2]);
	}
	pthread_mutex_unlock(&mutexCreaHilos);

	/* Se imprime el encabezado y se espera un intro para empezar a recibir señales */
	printf("Se podra simular la entrada de invitaciones al programa mediante el uso de señales.\n");
	printf("Introduzca 'kill -10 PID' en el terminal si desea realizar una solicitud por invitacion.\n");
	printf("Introduzca 'kill -12 PID' en el terminal si desea realizar una solicitud por QR.\n");
	printf("Introduzca 'kill -13 PID' en el terminal si desea realizar un cambio de los valores.\n");
	printf("Introduzca 'kill -2 PID' en el terminal si desea finalizar el programa.\n");
	printf("Pulse intro para continuar...\n");
	printf("---------------------------------------------------------------------------------------------------------------------------------------\n");

	/* Guardamos en el log la apertura de la aplicacion Tsunami Democratico*/
	escribirEnLog("INICIO", "Tsunami Democratico Cultural Leones");

	/* Bucle en espera de llegada de señales */
	while(TRUE){
		pause();
	}
	
	return 0;
}


/*-------------------------- DEFINICION DE FUNCIONES -----------------------*/
/* Funcion llega Solicitud: Comprobamos si las solicitudes pueden entrar en la cola*/
void llegaSolicitud(int signal){
	printf("Vamog a esperar un Mutex\n");
	
	if(contadorSolicitudes>=numSolicitudes){
		printf("Solicitud rechazada, la cola esta llena\n");
	} else{
		pthread_mutex_lock(&mutexColaSolicitudes); 
		printf("La solicitud entra en la cola\n");
		listaDeUsuarios[contadorSolicitudes].id = contadorSolicitudes;
		listaDeUsuarios[contadorSolicitudes].atendido = 0;
		listaDeUsuarios[contadorSolicitudes].tipoAtencion = 3;

		switch(signal){
			case SIGUSR1:
			printf("ES DE INVITACION.\n");
			listaDeUsuarios[contadorSolicitudes].tipo=0;
			pthread_create(&listaDeUsuarios[contadorSolicitudes].hiloUsuario, NULL, accionesSolicitud, (void *)(intptr_t)contadorSolicitudes);
			pthread_mutex_unlock(&mutexColaSolicitudes);
			break;
			case SIGUSR2:
			printf("ES DE QR.\n");
			listaDeUsuarios[contadorSolicitudes].tipo=1;
			pthread_create(&listaDeUsuarios[contadorSolicitudes].hiloUsuario, NULL, accionesSolicitud, (void *)(intptr_t)contadorSolicitudes);
			pthread_mutex_unlock(&mutexColaSolicitudes);
			break;
		}
		contadorSolicitudes++;
		pthread_mutex_unlock(&mutexColaSolicitudes);
	}
}

/* Funcion acciones Solicitud: comprobamos si las solicitudes pueden unirse o no a una actividad social*/
void *accionesSolicitud(void *ptr){

	/* Se guarda para escribir en el Log el id del atendedor */
	int identificadorSolicitud = (intptr_t)ptr;
	char idSolicitud[100];
	char mensajeLog[100];

	/* Se registra en el Log el momento de llegada de la solicitud */
	printf("Solicitud_%d llega a la cola", identificadorSolicitud+1);

	sprintf(idSolicitud, "Solicitud_%d", identificadorSolicitud+1);
	sprintf(mensajeLog, "Llega a la cola.");
	escribirEnLog(idSolicitud,mensajeLog);

		/* Se registra en el Log el tipo de solicitud que ha llegado */
	if(listaDeUsuarios[identificadorSolicitud].tipo==0){
		printf("Es de tipo invitacion \n");
		sprintf(mensajeLog, "Es de tipo invitacion");
	}else{
		printf("Es de tipo QR \n");
		sprintf(mensajeLog, "Es de tipo QR");
	}
	escribirEnLog(idSolicitud,mensajeLog);

	/* Se duerme 4 segundos */
	if(listaDeUsuarios[identificadorSolicitud].tipo == 1){
		/* Si es de QR, se genera el 30% de que se vayan por no ser muy fiables. */
		if(calculaAleatorios(0, 100) > 70){
			pthread_mutex_lock(&mutexColaSolicitudes); 
			// El metodo para compactar el array (llamandolo con la posicion identificadorSolicitud)
			numSolicitudes--;
			pthread_exit(NULL);
			pthread_mutex_unlock(&mutexColaSolicitudes);
			printf("Se expulso a la solicitud %d por no ser demasiado fiable.\n" ,identificadorSolicitud+1);
		}
	}
	
  	/* Se declaran variables que usaran en el for*/
	int i, tipo;

	int puedeIrActividad = -1;
	while(puedeIrActividad != 0){
		tipo = listaDeUsuarios[i].tipoAtencion;
		printf("La solicitud %d es de tipo: %d\n", identificadorSolicitud+1, tipo);
		switch(tipo){
			/* Solicitud que llega de manera correcta */
			case 0:
				sleep(calculaAleatorios(1,4));
				if(calculaAleatorios(0,100) > 50){
					pthread_mutex_lock(&mutexSolicitudes);
					/* Escribimos en el log que el usuario no quiere participar */
					printf("La solicitud %d decide no pertenecer a una actividad cultural.\n", identificadorSolicitud+1);
					sprintf(mensajeLog,"El usuario decide no pertenecer a una actividad cultural.");
					escribirEnLog(idSolicitud,mensajeLog);
					//TODO El metodo para compactar el array (llamandolo con la posicion identificadorSolicitud)
					pthread_exit(NULL);
					numSolicitudes--;
					pthread_mutex_unlock(&mutexSolicitudes);
				}
				puedeIrActividad = 0;
			break;
			/* Solicitud que tiene errores en datos personales */
			case 1:
				sleep(calculaAleatorios(2,6));
				if(calculaAleatorios(0,100) > 50){
					pthread_mutex_lock(&mutexSolicitudes);
					/* Escribimos en el log que el usuario no quiere participar */
					printf("La solicitud %d decide no pertenecer a una actividad cultural.\n", identificadorSolicitud+1);
					sprintf(mensajeLog,"El usuario decide no pertenecer a una actividad cultural.");
					escribirEnLog(idSolicitud,mensajeLog);
					//TODO El metodo para compactar el array (llamandolo con la posicion identificadorSolicitud)
					pthread_exit(NULL);
					numSolicitudes--;
					pthread_mutex_unlock(&mutexSolicitudes);
				}
				puedeIrActividad = 0;
			break;
			/* Solicitud que llega con errores por antecedentes */
			case 2:
				pthread_mutex_lock(&mutexSolicitudes);
				/* Escribimos en el log que el usuario no puede participar y es expulsado*/
				printf("La solicitud %d no puede pertenecer a una actividad cultural. Es expulsado!\n", identificadorSolicitud+1);
				sprintf(mensajeLog,"El usuario no puede pertenecer a una actividad cultural. Es expulsado!");
				escribirEnLog(idSolicitud,mensajeLog);
				//TODO El metodo para compactar el array (llamandolo con la posicion identificadorSolicitud)
				pthread_exit(NULL);
				numSolicitudes--;
				pthread_mutex_unlock(&mutexSolicitudes);
			break;

				  /* Solicitud llega cuando no ha sido atendida por ningun atendedor (queda en espera) */
			case 3:
				printf("LA SOLICITUD %d ENTRA AL CASE 3.\n", identificadorSolicitud+1);
				sleep(3);
				/* Si es de Invitacion se genera el aleatorio y si sale 10% se va */
				if(listaDeUsuarios[identificadorSolicitud].tipo == 1){
					if(calculaAleatorios(0,100)>90){
						pthread_mutex_lock(&mutexColaSolicitudes);
						/* Escribimos en el log que el usuario se ha cansado */
						printf("La solicitud %d se cansa de esperar y se va.\n", identificadorSolicitud+1);
						sprintf(mensajeLog,"El usuario se cansa de esperar y se va.");
						escribirEnLog(idSolicitud,mensajeLog); 
						//TODO El metodo para compactar el array (llamandolo con la posicion identificadorSolicitud)
						pthread_exit(NULL);
						numSolicitudes--;
						pthread_mutex_unlock(&mutexColaSolicitudes);
					}
				}

				/* Sea de cualquier tipo se comprueba el 15% de que la app lo expulse de forma aleatoria */
				if(calculaAleatorios(0,100)>85){
					pthread_mutex_lock(&mutexColaSolicitudes); 
					//TODO Borrar este printf despues del debug
					printf("Se expulso a la solicitud %d por el fallo random de la app.\n" ,identificadorSolicitud+1);
					//TODO El metodo para compactar el array (llamandolo con la posicion identificadorSolicitud)
					pthread_exit(NULL);
					numSolicitudes--;
					pthread_mutex_unlock(&mutexColaSolicitudes);
				}
			break;
			default:
				perror("Error en el tipo de atencion de la solicitud \n");
		}
	}
	printf("La solicitud %d decide que quiere ir a una actividad cultural.\n", identificadorSolicitud+1)
	sprintf(mensajeLog, "Decide entrar a la cola de actividades culturales");
	escribirEnLog(idSolicitud, mensajeLog);
	pthread_cond_wait(&condicionIniciarActividad, &mutexActividad);
	sleep(3);                       
	contadorActividad--; //TODO Esto deberia estar protegido por un mutex?????
	if(contadorActividad==0){
        /* Salimos de la cola y el ultimo avisa al coordinador */
		pthread_cond_signal(&condicionAcabarActividad);
	}
	printf("La solicitud deja de participar en la actividad social y se va.\n");
    /* Escribimos en el log que la solicitud deja de participar en la actividad */
	sprintf(mensajeLog,"La solicitud deja de participar en la actividad social y se va.");
	escribirEnLog(idSolicitud, mensajeLog);
}


/* Función acciones de los Atendedores: comprobamos en bucle si estos pueden o no atender una solicitud entrante */
void *accionesAtendedor(void *ptr){
	
	int i;
	/* Variable que representa el id del usuario a tratar*/
	int solicitudElegida;

	/* Se guarda para escribir en el Log el id del atendedor */
	int identificadorAtendedor = (intptr_t)ptr;
	char idAtendedor[100];
	printf("Atendedor_%d: \n", identificadorAtendedor+1);
	sprintf(idAtendedor, "Atendedor_%d: ", identificadorAtendedor+1);
	char mensajeLog[100];
	char mensaje[100];
	char tomarCafe[100];
	char acabaCafe[100];

	/* Los atendedores se quedaran en un bucle infinito atendiendo solicitudes*/
	printf("El tipo del atendedor es %d \n", identificadorAtendedor);
	printf("Tipo: %d\n",listaDeUsuarios[i].tipo);
	printf("Atendido: %d\n",listaDeUsuarios[i].atendido);
	printf("Atendiendo: %d\n",listaAtendedores[identificadorAtendedor].atendiendo);

	/* Id del antendedor*/
	while(TRUE){  	
		srand(time(NULL));
			//printf("PASO POR AQUI !!!!!. \n");
		switch(identificadorAtendedor){
			case 0:
					//printf("El de invitacion se pone al currele.\n");
					//printf("Tipo: %d\n",listaDeUsuarios[i].tipo);
					//printf("Atendido: %d\n",listaDeUsuarios[i].atendido);
					//printf("Atendiendo xddd: %d\n",listaAtendedores[identificadorAtendedor].atendiendo);
				/* Atendedor de tipo Invitacion */
			pthread_mutex_lock(&mutexSolicitudes);
					//printf("El de invitacion tiene el mutex");
			for(i=0;i<numSolicitudes;i++){
					/* Se comprueba que el atendedor puede atender dicha solicitud. */

				if(listaDeUsuarios[i].tipo==0 && listaDeUsuarios[i].atendido==0 && listaAtendedores[identificadorAtendedor].atendiendo==0){
					printf("VA A ATENDER UN INVITACIONES LA SOLICITUD %d\n", i);
					int numAle = calculaAleatorios(0,100);
					int tiempoDormir;
							/* Calculamos el tipo de atencion*/
					if(numAle<70){
						tiempoDormir = calculaAleatorios(1,4);
						sleep(tiempoDormir);
						printf("La solicitud_%d tiene todo en orden, atencion correcta. \n",i+1);
						sprintf(mensajeLog,"La solicitud_%d tiene todo en orden, atencion correcta.",i+1);
						listaDeUsuarios[i].tipoAtencion=0;
						printf("TIPO DE ATENCIÓN DADO %d\n", listaDeUsuarios[i].tipoAtencion);
							  /* Despues puede solicitar o no entrar en una actividad cultural */
					} else if(numAle<90) {
						tiempoDormir = calculaAleatorios(2,6);
						sleep(tiempoDormir);
						printf("La solicitud_%d tiene errores en los datos personales, atencion correcta. \n",i+1);
						sprintf(mensajeLog,"La solicitud_%d tiene errores en los datos personales, atencion correcta.",i+1);
						listaDeUsuarios[i].tipoAtencion=1;
						printf("TIPO DE ATENCIÓN DADO %d\n", listaDeUsuarios[i].tipoAtencion);
								/* Despues puede participar o no en una actividad cultural */
					} else {
						tiempoDormir = calculaAleatorios(6,10);
						sleep(tiempoDormir);
						printf("La solicitud_%d se corresponde con un usuario con antecedentes penales, atencion correcta. \n",i+1);
						sprintf(mensajeLog,"La solicitud_%d se corresponde con un usuario con antecedentes penales, atencion correcta.",i+1);
						listaDeUsuarios[i].tipoAtencion=2;
						printf("TIPO DE ATENCIÓN DADO %d\n", listaDeUsuarios[i].tipoAtencion);
					}
					

					/* Log del momento de fin de atencion */
					printf("La solicitud_%d ha sido atendida\n", i+1);
					sprintf(mensaje, "La solicitud_%d ha sido atendida", i+1);
					escribirEnLog(idAtendedor, mensaje);
					/* Log de como ha sido la atencion*/                              
					escribirEnLog(idAtendedor, mensajeLog);

					/* Cambiamos el flag de atendido de la solicitud */
					listaDeUsuarios[i].atendido=1;

					/* Cambiamos el flag de atendido del atendedor*/
					listaAtendedores[identificadorAtendedor].atendiendo=1;
					listaAtendedores[identificadorAtendedor].solAtendidas++;

					pthread_mutex_unlock(&mutexSolicitudes);

					if(listaAtendedores[0].solAtendidas != 0){
						if(listaAtendedores[0].solAtendidas%5 == 0){
										/* Se registra la entrada al cafe */
							printf("El atendedor de invitaciones se va a tomar cafe. \n");
							sprintf(tomarCafe, "El atendedor de invitaciones se va a tomar cafe.");
							escribirEnLog(idAtendedor, tomarCafe);

										/* Duerme 10 segundos */
							sleep(10);

										/* Se registra la salida al café */
							printf("El atendedor de invitaciones regresa de tomar cafe. \n");
							sprintf(acabaCafe, "El atendedor de invitaciones regresa de tomar cafe.");
							escribirEnLog(idAtendedor, acabaCafe);
						}
					}
				}
			}
			//printf("Salgo del for1\n");
				/*En este caso no existen solicitudes por invitacion, atendera la que mas tiempo lleve*/
			for(i=0;i<numSolicitudes;i++){
				if(listaDeUsuarios[i].atendido==0 && listaAtendedores[identificadorAtendedor].atendiendo==0){
					printf("VA A ATENDER UN INVITACIONES LA SOLICITUD %d, AUNQUE NO ES DE SU INCUMBENCIA\n", i);
					int numAle = calculaAleatorios(0,100);
					int tiempoDormir;
							/* Calculamos el tipo de atencion*/
					if(numAle<70){
						tiempoDormir = calculaAleatorios(1,4);
						sleep(tiempoDormir);
						printf("La solicitud_%d tiene todo en orden, atencion correcta. \n",i+1);
						sprintf(mensajeLog,"La solicitud_%d tiene todo en orden, atencion correcta.",i+1);
						listaDeUsuarios[i].tipoAtencion=0;
						printf("TIPO DE ATENCIÓN DADO %d\n", listaDeUsuarios[i].tipoAtencion);
							/* Despues puede solicitar o no entrar en una actividad cultural */
					} else if(numAle<90) {
						tiempoDormir = calculaAleatorios(2,6);
						sleep(tiempoDormir);
						printf("La solicitud_%d tiene errores en los datos personales, atencion correcta. \n",i+1);
						sprintf(mensajeLog,"La solicitud_%d tiene errores en los datos personales, atencion correcta.",i+1);
						listaDeUsuarios[i].tipoAtencion=1;
						printf("TIPO DE ATENCIÓN DADO %d\n", listaDeUsuarios[i].tipoAtencion);
							/*Despues puede participar o no en una actividad cultural*/
					} else {
						tiempoDormir = calculaAleatorios(6,10);
						sleep(tiempoDormir);
						printf("La solicitud_%d se corresponde con un usuario con antecedentes, atencion correcta.\n",i+1);
						sprintf(mensajeLog,"La solicitud_%d se corresponde con un usuario con antecedentes, atencion correcta.",i+1);
						listaDeUsuarios[i].tipoAtencion=2;
						printf("TIPO DE ATENCIÓN DADO %d\n", listaDeUsuarios[i].tipoAtencion);
					}
					

					/* Log del momento de fin de atencion */
					printf("La solicitud_%d ha sido atendida \n", i+1);
					sprintf(mensaje, "La solicitud_%d ha sido atendida", i+1);
					escribirEnLog(idAtendedor, mensaje);
					/* Log de como ha sido la atencion*/                              
					escribirEnLog(idAtendedor, mensajeLog);

					/* Se cambia el flag de atendido*/
					listaDeUsuarios[i].atendido=1;

					/* Cambiamos el flag de atendido del atendedor*/
					listaAtendedores[identificadorAtendedor].atendiendo=1;
					listaAtendedores[identificadorAtendedor].solAtendidas++;

					pthread_mutex_unlock(&mutexSolicitudes);

					if(listaAtendedores[0].solAtendidas != 0){
						if(listaAtendedores[0].solAtendidas%5 == 0){
									  /* Se registra la entrada al cafe */
							printf("El atendedor de invitaciones se va a tomar cafe. \n");
							sprintf(tomarCafe, "El atendedor de invitaciones se va a tomar cafe.");
							escribirEnLog(idAtendedor, tomarCafe);

									  /* Duerme 10 segundos */
							sleep(10);

									  /* Se registra la salida al café */
							printf("El atendedor de invitaciones regresa de tomar cafe. \n");
							sprintf(acabaCafe, "El atendedor de invitaciones regresa de tomar cafe.");
							escribirEnLog(idAtendedor, acabaCafe);
						}
					}
				}
			}

			pthread_mutex_unlock(&mutexSolicitudes);                	
					//printf("Salgo del for2\n");
				/*Se comprueba si al atendedor le toca atender tomar cafe*/
			
			break;
			case 1:
				  //printf("Tipo: %d\n",listaDeUsuarios[i].tipo);
				  //printf("Atendido: %d\n",listaDeUsuarios[i].atendido);
				  //printf("Atendiendo xddd: %d\n",listaAtendedores[identificadorAtendedor].atendiendo);
					//printf("El de QR se pone al currele.\n");
				/* Atendedor de tipo QR. */
			pthread_mutex_lock(&mutexSolicitudes);
				//printf("El de QU ERRE tiene el mutex\n");
			for(i=0;i<numSolicitudes;i++){
					/* Se comprueba que el atendedor puede atender dicha solicitud. */
				if(listaDeUsuarios[i].tipo==1 && listaDeUsuarios[i].atendido==0 && listaAtendedores[identificadorAtendedor].atendiendo==0){
					printf("VA A ATENDER UN QR LA SOLICITUD %d\n", i);
					int numAle = calculaAleatorios(0,100);
					int tiempoDormir;
							/* Calculamos el tipo de atencion*/
					if(numAle<70){
						tiempoDormir = calculaAleatorios(1,4);
						sleep(tiempoDormir);
						printf("La solicitud_%d tiene todo en orden, atencion correcta.\n",i+1);
						sprintf(mensajeLog,"La solicitud_%d tiene todo en orden, atencion correcta.",i+1);
						listaDeUsuarios[i].tipoAtencion=0;
						printf("TIPO DE ATENCIÓN DADO %d\n", listaDeUsuarios[i].tipoAtencion);
									/* Despues puede solicitar o no entrar en una actividad cultural */
					} else if(numAle<90) {
						tiempoDormir = calculaAleatorios(2,6);
						sleep(tiempoDormir);
						printf("La solicitud_%d tiene errores en los datos personales, atencion correcta.\n",i+1);
						sprintf(mensajeLog,"La solicitud_%d tiene errores en los datos personales, atencion correcta.",i+1);
						listaDeUsuarios[i].tipoAtencion=1;
						printf("TIPO DE ATENCIÓN DADO %d\n", listaDeUsuarios[i].tipoAtencion);
									/*Despues puede participar o no en una actividad cultural*/
					} else {
						tiempoDormir = calculaAleatorios(6,10);
						sleep(tiempoDormir);
						printf("La solicitud_%d se corresponde con un usuario con antecedentes, atencion correcta.\n",i+1);
						sprintf(mensajeLog,"La solicitud_%d se corresponde con un usuario con antecedentes, atencion correcta.",i+1);
						listaDeUsuarios[i].tipoAtencion=2;
						printf("TIPO DE ATENCIÓN DADO %d\n", listaDeUsuarios[i].tipoAtencion);
					}
					

					/* Log del momento de fin de atencion */
					printf("La solicitud_%d ha sido atendida\n", i+1);
					sprintf(mensaje, "La solicitud_%d ha sido atendida", i+1);
					escribirEnLog(idAtendedor, mensaje);
					/* Log de como ha sido la atencion*/                              
					escribirEnLog(idAtendedor, mensajeLog);

					/* Se cambia el flag de atendido*/
					listaDeUsuarios[i].atendido=1;

					/* Cambiamos el flag de atendido del atendedor*/
					listaAtendedores[identificadorAtendedor].atendiendo=1;
					listaAtendedores[identificadorAtendedor].solAtendidas++;

					pthread_mutex_unlock(&mutexSolicitudes);

					if(listaAtendedores[1].solAtendidas != 0){
						if(listaAtendedores[1].solAtendidas%5 == 0){
									  /* Se registra la entrada al cafe */
							sprintf(tomarCafe, "El atendedor de invitaciones se va a tomar cafe.");
							printf("El atendedor de invitaciones se va a tomar cafe.\n");
							escribirEnLog(idAtendedor, tomarCafe);

									  /* Duerme 10 segundos */
							sleep(10);

									  /* Se registra la salida al café */
							sprintf(acabaCafe, "El atendedor de invitaciones regresa de tomar cafe.");
							printf("El atendedor de invitaciones regresa de tomar cafe.\n");
							escribirEnLog(idAtendedor, acabaCafe);
						}
					}
				}
			}

				  //printf("Salgo del for1\n");
				/* En este caso no existen solicitudes por invitacion, se atendera la que mas tiempo lleve. */
			for(i=0;i<numSolicitudes;i++){
				if(listaDeUsuarios[i].atendido==0 && listaAtendedores[identificadorAtendedor].atendiendo==0){
					printf("VA A ATENDER UNO DE QR LA SOLICITUD %d, AUNQUE NO SEA DE LAS SUYAS\n", i);
					int numAle = calculaAleatorios(0,100);
					int tiempoDormir;
					if(numAle<70){
						tiempoDormir = calculaAleatorios(1,4);
						sleep(tiempoDormir);
						printf("La solicitud_%d tiene todo en orden, atencion correcta.\n",i+1);
						sprintf(mensajeLog,"La solicitud_%d tiene todo en orden, atencion correcta.",i+1);
						listaDeUsuarios[i].tipoAtencion=0;
						printf("TIPO DE ATENCIÓN DADO %d\n", listaDeUsuarios[i].tipoAtencion);
							/* Despues puede solicitar o no entrar en una actividad cultural */
					} else if(numAle<90) {
						tiempoDormir = calculaAleatorios(2,6);
						sleep(tiempoDormir);
						printf("La solicitud_%d tiene errores en los datos personales, atencion correcta.\n",i+1);
						sprintf(mensajeLog,"La solicitud_%d tiene errores en los datos personales, atencion correcta.",i+1);
						listaDeUsuarios[i].tipoAtencion=1;
						printf("TIPO DE ATENCIÓN DADO %d\n", listaDeUsuarios[i].tipoAtencion);
							/*Despues puede participar o no en una actividad cultural*/
					} else {
						tiempoDormir = calculaAleatorios(6,10);
						sleep(tiempoDormir);
						printf("La solicitud_%d se corresponde con un usuario con antecedentes, atencion correcta.\n",i+1);
						sprintf(mensajeLog,"La solicitud_%d se corresponde con un usuario con antecedentes, atencion correcta.",i+1);
						printf("La solicitud_%d se corresponde con un usuario con antecedentes, atencion correcta.\n",i+1);
						listaDeUsuarios[i].tipoAtencion=2;
						printf("TIPO DE ATENCIÓN DADO %d\n", listaDeUsuarios[i].tipoAtencion);
					}
					

							  /* Log del momento de fin de atencion */
					sprintf(mensaje, "La solicitud_%d ha sido atendida", i+1);
					printf("La solicitud_%d ha sido atendida\n", i+1);
					escribirEnLog(idAtendedor, mensaje);
						/* Log de como ha sido la atencion*/                              
					escribirEnLog(idAtendedor, mensajeLog);

							/*Se cambia el flag de atendido de la solicitud*/
					listaDeUsuarios[i].atendido=1;

						/* Cambiamos el flag de atendido del atendedor*/
					listaAtendedores[identificadorAtendedor].atendiendo=1;
					listaAtendedores[identificadorAtendedor].solAtendidas++;

					pthread_mutex_unlock(&mutexSolicitudes);

					if(listaAtendedores[1].solAtendidas != 0){
						if(listaAtendedores[1].solAtendidas%5 == 0){
									  /* Se registra la entrada al cafe */
							sprintf(tomarCafe, "El atendedor de invitaciones se va a tomar cafe.");
							printf("El atendedor de invitaciones se va a tomar cafe.\n");
							escribirEnLog(idAtendedor, tomarCafe);

									  /* Duerme 10 segundos */
							sleep(10);

									  /* Se registra la salida al café */
							sprintf(acabaCafe, "El atendedor de invitaciones regresa de tomar cafe.");
							printf("El atendedor de invitaciones regresa de tomar cafe.\n");
							escribirEnLog(idAtendedor, acabaCafe);
						}
					}
				}
			}

			pthread_mutex_unlock(&mutexSolicitudes);                	
				/*Se comprueba si al atendedor le toca atender tomar cafe*/
			break;

			/* Atendedor de tipo PRO */
			default:
				  /* printf("Tipo: %d\n",listaDeUsuarios[i].tipo);
				  printf("Atendido: %d\n",listaDeUsuarios[i].atendido);
				  printf("Atendiendo xddd: %d\n",listaAtendedores[identificadorAtendedor].atendiendo); */
			pthread_mutex_lock(&mutexSolicitudes);
					//printf("El pro tiene el mutex\n");
			for(i=0;i<numSolicitudes;i++){
				if(listaAtendedores[identificadorAtendedor].atendiendo==0 && listaDeUsuarios[i].atendido==0){
					printf("VA A ATENDER UN PRO LA SOLICITUD %d\n", i);
					int numAle = calculaAleatorios(0,100);
					int tiempoDormir;
					if(numAle<70){
						tiempoDormir = calculaAleatorios(1,4);
						sleep(tiempoDormir);
						sprintf(mensajeLog,"La solicitud_%d tiene todo en orden, atencion correcta.",i+1);
						printf("La solicitud_%d tiene todo en orden, atencion correcta.\n",i+1);
						listaDeUsuarios[i].tipoAtencion=0;
						printf("TIPO DE ATENCIÓN DADO %d\n", listaDeUsuarios[i].tipoAtencion);
								/* Despues puede solicitar o no entrar en una actividad cultural */
					} else if(numAle<90) {
						tiempoDormir = calculaAleatorios(2,6);
						sleep(tiempoDormir);
						sprintf(mensajeLog,"La solicitud_%d tiene errores en los datos personales, atencion correcta.",i+1);
						printf("La solicitud_%d tiene errores en los datos personales, atencion correcta.\n",i+1);
						listaDeUsuarios[i].tipoAtencion=1;
						printf("TIPO DE ATENCIÓN DADO %d\n", listaDeUsuarios[i].tipoAtencion);
									/*Despues puede participar o no en una actividad cultural*/
					} else {
						tiempoDormir = calculaAleatorios(6,10);
						sleep(tiempoDormir);
						sprintf(mensajeLog,"La solicitud_%d se corresponde con un usuario con antecedentes, atencion correcta.",i+1);
						printf("La solicitud_%d se corresponde con un usuario con antecedentes, atencion correcta.\n",i+1);
						listaDeUsuarios[i].tipoAtencion=2;
						printf("TIPO DE ATENCIÓN DADO %d\n", listaDeUsuarios[i].tipoAtencion);
					}
					

					  /* Log del momento de fin de atencion */
					sprintf(mensaje, "La solicitud_%d ha sido atendida", i+1);
					printf("La solicitud_%d ha sido atendida\n", i+1);
					escribirEnLog(idAtendedor, mensaje);
					  /* Log de como ha sido la atencion*/                              
					escribirEnLog(idAtendedor, mensajeLog);

					  /* Se cambia el flag de atendido */
					listaDeUsuarios[i].atendido=1;

					  /* Se cambia el flag de atendido del atendedor*/
					listaAtendedores[identificadorAtendedor].atendiendo=1;
					listaAtendedores[identificadorAtendedor].solAtendidas++;

					pthread_mutex_unlock(&mutexSolicitudes);
							/*Se comprueba si al atendedor le toca atender tomar cafe*/
					if(listaAtendedores[identificadorAtendedor].solAtendidas != 0){
						if(listaAtendedores[identificadorAtendedor].solAtendidas%5 == 0){
										/* Se registra la entrada al cafe */
							sprintf(tomarCafe, "El atendedor pro se va a tomar cafe.");
							printf("El atendedor pro se va a tomar cafe.\n");
							escribirEnLog(idAtendedor, tomarCafe);

										/* Duerme 10 segundos */
							sleep(10);

										/* Se registra la salida al café */
							sprintf(acabaCafe, "El atendedor pro regresa de tomar cafe.");
							printf("El atendedor pro regresa de tomar cafe.\n");
							escribirEnLog(idAtendedor, acabaCafe);
						}
					}
				}
			}

			pthread_mutex_unlock(&mutexSolicitudes);    
		}
		/* Se libera al atendedor para que puede atender mas solicitudes */
		listaAtendedores[identificadorAtendedor].atendiendo=0;
	}
}

/* Funcion accionesCoordinadorSocial: se encarga de gestionar las actividades sociales*/
void *accionesCoordinadorSocial(void *ptr){
	char mensajeLog[100];
	char idCoordinador[100];
	sprintf(idCoordinador,"Coordinador_1: ");

	while(TRUE){
		if(contadorActividad==4){ //TODO Aqui en vez del if no seria un lock del mutexActividad
          	/*Avisa a los usuarios de que comiencen la actividad*/
			pthread_cond_signal(&condicionIniciarActividad); 
			sprintf(mensajeLog,"La actividad comienza.");
			printf("La actividad comienza\n");
			escribirEnLog(idCoordinador, mensajeLog);

          	/*Espera que los usuarios hayan finalizado*/
			pthread_cond_wait(&condicionAcabarActividad, &mutexActividad);
			sprintf(mensajeLog, "La actividad ha terminado.");
			printf("La actividad ha terminado\n");
			escribirEnLog(idCoordinador, mensajeLog);

			sleep(1);
			contadorActividad=0;
		}else{
          	/* Dormimos un segundo y volvemos a comprobar si la actividad social esta completa o no */
			sleep(1);
		}
	}
}


/* Funcion llegaCambioValores: Cuando llega la señal SIGPIPE se accede a esta manejadora para cambiar los atendedores o las solicitudes. */
void llegaCambioValores(int s){
	/* Se avisa de la entrada a la manejadora tanto por pantalla como en el log. */
	printf("Se ha detectado una solicitud de cambio de valores.\n");
	char mensajeLog[250];
	sprintf(mensajeLog, "Llega una solicitud para cambiar los valores.");
	escribirEnLog("AVISO", mensajeLog);

	int valorACambiar, nuevoValor, valido = 0;

	/* Se muestra un menu al usuario preguntando que valor desea cambiar. */
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
		/* Una vez se introduce un valor valido, se muestra un segundo menu preguntando en cuantas unidades se quiere incrementar. */
		if(valorACambiar == 1){
			printf("Introduce cuantas solicitudes quieres incrementar:");
		} else{
			printf("Introduce cuantos atendedores quieres incrementar:");
		}
		scanf("%d", &nuevoValor);
		if(nuevoValor < 0){
			printf("Valor introducido no valido (%d). Necesario un valor entre positivo.\n", nuevoValor);
			printf("Relanzando menu\n");
			valido = -1;    
		}      
	}while(valido != -1);
	/* Se incrementan las variables globales en las unidades incrementadas. */
	int i;
	if(valorACambiar == 1){
		numSolicitudes += nuevoValor;
		printf("Valor de las solicitudes incrementados en %d hasta %d.\n", nuevoValor, numSolicitudes);
	} else{
		numAtendedores += nuevoValor;
		printf("Valor de los atendedores incrementados en %d hasta %d.\n", nuevoValor, numAtendedores);
			//TODO Habria que hacer un malloc en listaDeAtendedores incrementando la memoria reservada en nuevoValor
		for(i=0; i<nuevoValor; i++){
			pthread_create(&listaAtendedores[i].hiloAtendedor, NULL, accionesAtendedor, (void *)&tipoAt[2]);
		}
	}
	//TODO Realizar efectivamente el cambio añadiendo los nuevos hilos atendedores o de solicitudes que sean
	/* Se avisa en el log del cambio de valores. */
	sprintf(mensajeLog, "Se ha cambiado el valor de solicitudes incrementandolo en %d hasta %d.", nuevoValor, numAtendedores);
	escribirEnLog("AVISO", mensajeLog);
}

/* Funcion llegaFinalizacion: Cuando llega la señal SIGINT se accede a esta manejadora para abandonar de forma correcta el programa. */
void llegaFinalizacion(int s){
	/* Se escribe en el log que ha llegado la señal para acabar y se espera a que las solicitudes ya dentro del sistema sean atendidas. */
	char mensajeLog[250];
	printf("Ha llegado la señal de finalización, cerrada la entrada a más solicitudes.\n");
	sprintf(mensajeLog, "Ha llegado la señal de finalización, cerrada la entrada a más solicitudes.");
	escribirEnLog("AVISO", mensajeLog);

	//TODO cambiar los mutex para que no entren más en la cola y para que las que ya estan no entren a una actividad */

	
	/* Se liberan los punteros abiertos para liberar la memoria. */
	free(listaAtendedores);
	free(listaDeUsuarios);

	// -------------------------PRUEBA
	printf("PRUEBA: Acabose\n");

	/* Se escribe en el log que se sale del programa y se sale de manera efectiva. */
	sprintf(mensajeLog, "Saliendo del programa.");
	printf("Saliendo del programa.\n");
	escribirEnLog("FIN", mensajeLog);
	exit(0);
}

/* Funcion calculaAleatorios: Utilizada para generar numeros aleatorios en el proceso de seleccion de invitaciones. */
int calculaAleatorios(int min, int max){
	/* Se utiliza la fecha actual como semilla para generar el numero. */
	//srand(time(NULL));
	/* Se utiliza la libreria stdlib para generar un numero aleatorio entre 0 y 1, posteriormente se opera para que sea un entero entre el minimo y el maximo. */
	return rand() % (max-min+1) + min;
}

/* Funcion escribirEnLog: Utilizada para imprimir en el fichero de logs todos los mensajes. */
void escribirEnLog(char * id , char * mensaje){
	/* Se protege la escritura concurrente en el log mediante un mutex. */
	pthread_mutex_lock(&mutexLog);

	/* Se obtiene la fecha y hora actuales para el inicio de la linea de log. */
	time_t now = time (0) ;
	struct tm * tlocal = localtime (& now );
	char stnow[19];
	strftime(stnow ,19 ," %d/ %m/ %y %H: %M: %S", tlocal);

	/* Se escribe el mensaje en el fichero Log ussando la fecha y hora y los parametros. */
	logFile = fopen("registroTiempos.log" , "a");
	fprintf(logFile, "[ %s] %s: %s\n", stnow , id , mensaje);
	fclose(logFile);

	/* Se libera el mutex para que otros hilos puedan escribir en el log. */
	pthread_mutex_unlock(&mutexLog);
}

