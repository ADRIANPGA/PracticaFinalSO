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
void llegaSolicitud(int s);
void *accionesSolicitud(void *ptr);
void *accionesAtendedor(void *ptr);
void *accionesCoordinadorSocial(void *ptr);
void llegaCambioValores(int s);
void llegaFinalizacion(int s);
void escribirEnLog(char* id, char* mensaje);

/* Variables globales */
FILE *logFile;
int contadorAtendedor;
int contadorSolicitudes;
int contadorActividad;

struct Usuario{
  	/*Numero con el que entra en la cola*/
	int id;
	/*0->No 1->Si */
	int atendido;
	/*0->Invitacion 1->QR */
	int tipo;
  	/*0->Todo OK 1->Error en datos personales 2->Antecedentes */
  	int tipoAtencion;
  	pthread_t hiloUsuario; /*Hilo que ejecuta cada Usuario*/
};

struct Atendedor{
	//0->Inv 1->QR 2->PRO
	int tipo;
	int solAtendidas;
      //0->No 1->Si
      int atendiendo;
	pthread_t hiloAtendedor; /*Hilo ejecuta cada Atendedor*/
};

int tipoAt[3] = {0,1,2};

int numSolicitudes = 15;
int numAtendedores = 1;

struct Atendedor *listaAtendedores;
struct Usuario *listaDeUsuarios; //esto es lo mismo listaDeSolicitudes
struct Usuario listaActividad[4];

/* Para controlar el acceso a los recursos compartidos utilizamos dos semáforos (mutex) */
pthread_mutex_t mutexCreaHilos;
pthread_mutex_t mutexColaSolicitudes;
pthread_mutex_t mutexLog;
pthread_mutex_t mutexSolicitudes;
pthread_mutex_t mutexActividad;
//pthread_mutex_t mutex7;

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
	listaDeUsuarios = (struct Usuario*)malloc(sizeof(struct Usuario)*numSolicitudes);
	listaAtendedores = (struct Atendedor*)malloc(sizeof(struct Atendedor)*(numAtendedores+2));
	pthread_mutex_init(&mutexColaSolicitudes, NULL);

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
  if (pthread_mutex_init(&mutexSolicitudes, NULL)!=0) exit(-1); 
  if (pthread_mutex_init(&mutexActividad, NULL)!=0) exit(-1);
  
  /* Inicializamos las variables condición */
  if (pthread_cond_init(&condicionIniciarActividad, NULL)!=0) exit(-1);
  if (pthread_cond_init(&condicionAcabarActividad, NULL)!=0) exit(-1);
  
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

	/*J representa el contador de ID's*/
	int j=0;
	for (i = 0; i < numSolicitudes; i++) {
		   (listaDeUsuarios+i)->id=j;
		   (listaDeUsuarios+i)->atendido=0;
		   (listaDeUsuarios+i)->tipo=0;
               (listaDeUsuarios+i)->tipoAtencion=0;
		   j++;
	}
		
	pthread_t *atpros;
	pthread_t atqr,atinvitacion; 
	pthread_mutex_lock(&mutexCreaHilos);
	pthread_create(&listaAtendedores[0].hiloAtendedor, NULL, accionesAtendedor, &tipoAt[0]);
	pthread_create(&listaAtendedores[1].hiloAtendedor, NULL, accionesAtendedor, &tipoAt[1]);
	for(i=0;i<numAtendedores;i++){
		pthread_create(&listaAtendedores[i].hiloAtendedor, NULL, accionesAtendedor, &tipoAt[2]);
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
	
  	free(listaAtendedores);
	return 0;
}


/*-------------------------- DEFINICION DE FUNCIONES -----------------------*/
/* Funcion llega Solicitud: Comprobamos si las solicitudes pueden entrar en la cola*/
void llegaSolicitud(int signal){
	pthread_mutex_lock(&mutexColaSolicitudes); 
	printf("No hay nada de solicitudes por QR.\n");
	if(contadorSolicitudes==numSolicitudes){
		printf("Solicitud rechazada\nLa cola esta llena\n");
	} else{
		sleep(4);
		listaDeUsuarios[contadorSolicitudes].id = contadorSolicitudes;
		listaDeUsuarios[contadorSolicitudes].atendido = 0;
		switch(signal){
		  case SIGUSR1:
				listaDeUsuarios[contadorSolicitudes].tipo=0;
		  case SIGUSR2:
				listaDeUsuarios[contadorSolicitudes].tipo=1;
		}
		contadorSolicitudes++;
	}
	pthread_mutex_unlock(&mutexColaSolicitudes);
}

/* Funcion acciones Solicitud: comprobamos si las solicitudes pueden unirse o no a una actividad social*/
void *accionesSolicitud(void *ptr){ 
  	/* Se guarda para escribir en el Log el id del atendedor */
	int identificadorSolicitud = (int)ptr;
	char idSolicitud[100];
    char mensajeLog[100];

    /* Se registra en el Log el momento de llegada de la solicitud */
	sprintf(idSolicitud, "Solicitud_%d: ", identificadorSolicitud+1);
    sprintf(mensajeLog, "Llegada de solicitud: ");
    escribirEnLog(mensajeLog,idSolicitud);

    /* Se registra en el Log el tipo de solicitud que ha llegado */
    if(listaDeUsuarios[identificadorSolicitud].tipo==0){
        sprintf(mensajeLog, "Es de tipo invitacion");
    }else{
        sprintf(mensajeLog, "Es de tipo QR");
    }
    escribirEnLog(idSolicitud,mensajeLog);

    /* Se duerme 4 segundos */
    sleep(4);
    /* Se declaran variables que usaran en el for*/
    int errorSistema;
    int i;
    int esperar;
    int tipoSol;

    while(TRUE){
      	for(i=0; i<numSolicitudes; i++){
            int tipo = listaDeUsuarios[i].tipoAtencion;
            switch(tipo){
                /* Solicitud que llega de manera correcta */
                case 0:
                    pthread_mutex_lock(&mutexSolicitudes);
                    if(listaDeUsuarios[i].atendido==0){
                        tipoSol = listaDeUsuarios[i].tipo;
                        switch(tipoSol){
                            case 0:
                                esperar = calculaAleatorios(0,100);
                                /* Si es >90 se sale por mucho tiempo de espera*/
                                if(esperar>90){
                                    numSolicitudes--;
                                    pthread_exit(NULL);
                                    sprintf(mensajeLog,"La solicitud se va por cansarse de esperar");
                                    escribirEnLog(idSolicitud,mensajeLog);
                                }else{
                                    esperar = calculaAleatorios(0,100);
                                    /* Si esperar tiene un valor mayor que 85 se expulsa la solicitud por error en la app, sin avisar, sin log */
                                    if(esperar>85){
                                      numSolicitudes--;
                                      pthread_exit(NULL);                                      	
                                    }else{
                                        sleep(4);
                                    }
                                }
                            break;

                            case 1:
                                esperar = calculaAleatorios(0,100);
                                /* Si es >70 se sale por no ser fiables*/
                                if(esperar>70){
                                    numSolicitudes--;
                                    sprintf(mensajeLog,"La solicitud de va por cansarse de esperar");
                                    escribirEnLog(idSolicitud,mensajeLog);
                                }else{
                                    esperar = calculaAleatorios(0,100);
                                    /* Si esperar tiene un valor mayor que 85 se expulsa la solicitud por error en la app, sin avisar, sin log */
                                    if(esperar>85){
                                        numSolicitudes--;
                                        pthread_exit(NULL);	
                                    }else{
                                        sleep(4); 
                                    }
                                }
                            break;
                            default:
                                perror("Error en el tipo de solicitud");
                        }
                    }else{
                        errorSistema = calculaAleatorios(0,100);
                        if(errorSistema >= 50){
                            sprintf(mensajeLog,"El usuario decide no puede pertenecer a una actividad cultural");
                            escribirEnLog(idSolicitud,mensajeLog);
                            pthread_exit(NULL);
                            numSolicitudes--;
                        }else{
                            while(TRUE){
                              pthread_mutex_lock(&mutexActividad);
                              if(contadorActividad==4){
                                	/* Si no puede participar, espera 3 segundos*/
                                      sleep(3);
                              }else{
                                //Parte a) del pdf: si puede entrar en la lista de la actividad
						contadorActividad++;
                                	if(contadorActividad==4){
                                        pthread_mutex_unlock(&mutexSolicitudes); 
                                    }
                                    numSolicitudes--;
                                	/* Escribimos en el log que la solicitud esta ya lista para participar */
                                	sprintf(mensajeLog,"La solicitud esta preparada para participar en la actividad social");
                                	escribirEnLog(idSolicitud, mensajeLog);
                                	/* Esperamos a que puedan comenzar la actividad*/
                                	pthread_cond_wait(&condicionIniciarActividad, &mutexActividad);
                                	sleep(3); 
                                	/* Salimos de la cola y avisamos al coordinador */
                                	contadorActividad--;
                               	if(contadorActividad==0){
                                      /* Salimos de la cola y el ultimo avisa al coordinador */
                                      pthread_cond_signal(&condicionAcabarActividad);
                                  	}
                               	/* Escribimos en el log que la solicitud deja de participar en la actividad */
                                	sprintf(mensajeLog,"La solicitud deja de participar en la actividad social");
                                	escribirEnLog(idSolicitud, mensajeLog);
                              }
                            //pthread_mutex_unlock(&mutexActividad);
                            }
                        }
                    }

                break;

                /* Solicitud que llega con errores de datos personales */
                case 1:	
                    pthread_mutex_lock(&mutexSolicitudes);
                    if(listaDeUsuarios[i].atendido==0){
                        tipoSol = listaDeUsuarios[i].tipo;
                        switch(tipoSol){
                            case 0:
                                esperar = calculaAleatorios(0,100);
                                /* Si es >90 se sale por mucho tiempo de espera*/
                                if(esperar>90){
                                    numSolicitudes--;
                                    pthread_exit(NULL);
                                  	sprintf(mensajeLog, "La solicitud sale debido a un gran tiempo de espera");
                                  	escribirEnLog(idSolicitud, mensajeLog);
                                }else{
                                    esperar = calculaAleatorios(0,100);
                                    /* Si esperar tiene un valor mayor que 85 se expulsa la solicitud por error en la app, sin avisar, sin log */
                                    if(esperar>85){
                                        numSolicitudes--;
                                        pthread_exit(NULL);
                                    }else{
                                        sleep(4);
                                    }
                                }
                            break;

                            case 1:
                                esperar = calculaAleatorios(0,100);
                                /* Si es >70 se sale por no ser fiables*/
                                if(esperar>70){                                    
                                  	numSolicitudes--;
                                    pthread_exit(NULL);
                                    sprintf(mensajeLog, "La solicitud sale debido a no ser fiable");
                                  	escribirEnLog(idSolicitud, mensajeLog);
                                }else{
                                    esperar = calculaAleatorios(0,100);
                                    /* Si esperar tiene un valor mayor que 85 se expulsa la solicitud por error en la app, sin avisar, sin log */
                                    if(esperar>85){
                                        numSolicitudes--;
                                        pthread_exit(NULL);
                                    }else{
                                        sleep(4); 
                                    }
                                }
                            break;
                            
                            default:
                                perror("Error en el tipo de solicitud");
                        }
                    }else{
                        errorSistema = calculaAleatorios(0,100);
                        if(errorSistema >= 50){
                            sprintf(mensajeLog,"El usuario no puede pertenecer a una actividad cultural. Es expulsado!");
                            escribirEnLog(idSolicitud,mensajeLog);
                            pthread_exit(NULL); 
                       	    numSolicitudes--;
                        }else{
                            contadorActividad++;
                            if(contadorActividad==4){
                                pthread_mutex_unlock(&mutexSolicitudes);
                            }
                            numSolicitudes--;
                            /* Escribimos en el log que la solicitud esta ya lista para participar */
                            sprintf(mensajeLog,"La solicitud esta preparada para participar en la actividad social");
                            escribirEnLog(idSolicitud, mensajeLog);
                            /* Esperamos a que puedan comenzar la actividad*/
                            pthread_cond_wait(&condicionIniciarActividad, &mutexActividad);
                            sleep(3);                       
                            contadorActividad--;
                            if(contadorActividad==0){
                              /* Salimos de la cola y el ultimo avisa al coordinador */
                            	pthread_cond_signal(&condicionAcabarActividad);
                            }
                            /* Escribimos en el log que la solicitud deja de participar en la actividad */
                            sprintf(mensajeLog,"La solicitud deja de participar en la actividad social");
                            escribirEnLog(idSolicitud, mensajeLog);
                        }
                    }
             
                break;

                /* Solicitud que llega con errores por antecedentes */
                case 2:
                    pthread_exit(NULL);  	
                    numSolicitudes--;
                	  /* Escribimos en el log que el usuario no puede participar y es expulsado*/
                    sprintf(mensajeLog,"El usuario no puede pertenecer a una actividad cultural. Es expulsado!");
                    escribirEnLog(idSolicitud,mensajeLog);
                break;

                default:
                    perror("Error en el tipo de atencion de la solicitud\n");
            }
            escribirEnLog(idSolicitud, mensajeLog);
            pthread_mutex_unlock(&mutexSolicitudes);
        }
    }
  	
}

/* Función acciones de los Atendedores: comprobamos en bucle si estos pueden o no atender una solicitud entrante */
void *accionesAtendedor(void *ptr){
	int i;
	/* Variable que representa el id del usuario a tratar*/
	int solicitudElegida;
  
	/* Se guarda para escribir en el Log el id del atendedor */
	int identificadorAtendedor = (int)ptr;
	char idAtendedor[100];
	sprintf(idAtendedor, "Atendedor_%d: ", identificadorAtendedor+1);
  	char mensajeLog[100];
  	char mensaje[100];
  	char tomarCafe[100];
  	char acabaCafe[100];
  
	/* Los atendedores se quedaran en un bucle infinito atendiendo solicitudes*/
	/* Id del antendedor*/
    while(TRUE){
      
    	switch(identificadorAtendedor){
    		case 0:
    			/* Atendedor de tipo Invitacion */
    			pthread_mutex_lock(&mutexColaSolicitudes);
    			for(i=0;i<numSolicitudes;i++){
    				/* Se comprueba que el atendedor puede atender dicha solicitud. */
    				if(listaDeUsuarios[i].tipo==0 && listaDeUsuarios[i].atendido==0 && listaAtendedores[identificadorAtendedor].atendiendo==0){
    					int numAle = calculaAleatorios(0,100);
    					int tiempoDormir;
                          	/* Calculamos el tipo de atencion*/
    					if(numAle<70){
                              tiempoDormir = calculaAleatorios(1,4);
                          	sleep(tiempoDormir);
                          	sprintf(mensajeLog,"La solicitud_%d tiene todo en orden, atencion correcta.",i+1);
                          	listaDeUsuarios[i].tipoAtencion=0;
                              /* Despues puede solicitar o no entrar en una actividad cultural */
    					} else if(numAle<90) {
    						tiempoDormir = calculaAleatorios(2,6);
                          		sleep(tiempoDormir);
                            		sprintf(mensajeLog,"La solicitud_%d tiene errores en los datos personales, atencion correcta.",i+1);
                          		listaDeUsuarios[i].tipoAtencion=1;
                               	/* Despues puede participar o no en una actividad cultural */
    					} else {
    						tiempoDormir = calculaAleatorios(6,10);
                          		sleep(tiempoDormir);
                           		sprintf(mensajeLog,"La solicitud_%d se corresponde con un usuario con antecedentes penales, atencion correcta.",i+1);
    					  	listaDeUsuarios[i].tipoAtencion=2;
    					}
    				}
    				  
                    /* Log del momento de fin de atencion */
                    sprintf(mensaje, "La solicitud_%d ha sido atendida", i+1);
                    escribirEnLog(idAtendedor, mensaje);
                    /* Log de como ha sido la atencion*/                              
                    escribirEnLog(idAtendedor, mensajeLog);

                    /* Cambimos el flag de atendido de la solicitud */
                    listaDeUsuarios[i].atendido=1;

                    /* Cambiamos el flag de atendido del atendedor*/
                    listaAtendedores[identificadorAtendedor].atendiendo=1;

                    pthread_mutex_unlock(&mutexColaSolicitudes);
                  }
    			
    			/*En este caso no existen solicitudes por invitacion, atendera la que mas tiempo lleve*/
    			for(i=0;i<numSolicitudes;i++){
    				if(listaDeUsuarios[i].atendido==0 && listaAtendedores[identificadorAtendedor].atendiendo==0){
    					int numAle = calculaAleatorios(0,100);
    					int tiempoDormir;
                          	/* Calculamos el tipo de atencion*/
    					if(numAle<70){
    						tiempoDormir = calculaAleatorios(1,4);
                          		sleep(tiempoDormir);
                          		sprintf(mensajeLog,"La solicitud_%d tiene todo en orden, atencion correcta.",i+1);
    						listaDeUsuarios[i].tipoAtencion=0;
    						/* Despues puede solicitar o no entrar en una actividad cultural */
    					} else if(numAle<90) {
    						tiempoDormir = calculaAleatorios(2,6);
                  	        	sleep(tiempoDormir);
                            		sprintf(mensajeLog,"La solicitud_%d tiene errores en los datos personales, atencion correcta.",i+1);
                          		listaDeUsuarios[i].tipoAtencion=1;
    						/*Despues puede participar o no en una actividad cultural*/
    					} else {
    						tiempoDormir = calculaAleatorios(6,10);
                          		sleep(tiempoDormir);
                           		sprintf(mensajeLog,"La solicitud_%d se corresponde con un usuario con antecedentes, atencion correcta.",i+1);
                          		listaDeUsuarios[i].tipoAtencion=2;
    					}
    				}	  
                                  
                    /* Log del momento de fin de atencion */
                    sprintf(mensaje, "La solicitud_%d ha sido atendida", i+1);
                    escribirEnLog(idAtendedor, mensaje);
                    /* Log de como ha sido la atencion*/                              
                    escribirEnLog(idAtendedor, mensajeLog);

                    /* Se cambia el flag de atendido*/
                    listaDeUsuarios[i].atendido=1;

                    /* Cambiamos el flag de atendido del atendedor*/
                    listaAtendedores[identificadorAtendedor].atendiendo=1;

                    pthread_mutex_unlock(&mutexColaSolicitudes);
    			}
    			/*Se comprueba si al atendedor le toca atender tomar cafe*/
    			if(listaAtendedores[0].solAtendidas%5 == 0){
    				/* Se registra la entrada al cafe */
    				sprintf(tomarCafe, "El atendedor de invitaciones se va a tomar cafe.");
    				escribirEnLog(idAtendedor, tomarCafe);

    				/* Duerme 10 segundos */
    				sleep(10);

    				/* Se registra la salida al café */
    				sprintf(acabaCafe, "El atendedor de invitaciones regresa de tomar cafe.");
    				escribirEnLog(idAtendedor, acabaCafe);
    			}
    			break;
    		case 1:
    			/* Atendedor de tipo QR. */
    			pthread_mutex_lock(&mutexColaSolicitudes);
    			for(i=0;i<numSolicitudes;i++){
    				/* Se comprueba que el atendedor puede atender dicha solicitud. */
    				if(listaDeUsuarios[i].tipo==1 && listaDeUsuarios[i].atendido==0 && listaAtendedores[identificadorAtendedor].atendiendo==0){
    					int numAle = calculaAleatorios(0,100);
    					int tiempoDormir;
                          	/* Calculamos el tipo de atencion*/
    					if(numAle<70){
    						tiempoDormir = calculaAleatorios(1,4);
                          		sleep(tiempoDormir);
                          		sprintf(mensajeLog,"La solicitud_%d tiene todo en orden, atencion correcta.",i+1);
                          		listaDeUsuarios[i].tipoAtencion=0;
                                	/* Despues puede solicitar o no entrar en una actividad cultural */
    					} else if(numAle<90) {
    						tiempoDormir = calculaAleatorios(2,6);
                          		sleep(tiempoDormir);
                            		sprintf(mensajeLog,"La solicitud_%d tiene errores en los datos personales, atencion correcta.",i+1);
                          		listaDeUsuarios[i].tipoAtencion=1;
                                	/*Despues puede participar o no en una actividad cultural*/
    					} else {
    						tiempoDormir = calculaAleatorios(6,10);
                          		sleep(tiempoDormir);
                           		sprintf(mensajeLog,"La solicitud_%d se corresponde con un usuario con antecedentes, atencion correcta.",i+1);
                          		listaDeUsuarios[i].tipoAtencion=2;
    					}
    				}
                          
                    /* Log del momento de fin de atencion */
                    sprintf(mensaje, "La solicitud_%d ha sido atendida", i+1);
                    escribirEnLog(idAtendedor, mensaje);
                    /* Log de como ha sido la atencion*/                              
                    escribirEnLog(idAtendedor, mensajeLog);

                    /* Se cambia el flag de atendido*/
                    listaDeUsuarios[i].atendido=1;

                    /* Cambiamos el flag de atendido del atendedor*/
                    listaAtendedores[identificadorAtendedor].atendiendo=1;

                    pthread_mutex_unlock(&mutexColaSolicitudes);
    			}
    			/* En este caso no existen solicitudes por invitacion, se atendera la que mas tiempo lleve. */
    			for(i=0;i<numSolicitudes;i++){
    				if(listaDeUsuarios[i].atendido==0 && listaAtendedores[identificadorAtendedor].atendiendo==0){
    					int numAle = calculaAleatorios(0,100);
    					int tiempoDormir;
    					if(numAle<70){
    						tiempoDormir = calculaAleatorios(1,4);
                          		sleep(tiempoDormir);
                          		sprintf(mensajeLog,"La solicitud_%d tiene todo en orden, atencion correcta.",i+1);
                          		listaDeUsuarios[i].tipoAtencion=0;
    						/* Despues puede solicitar o no entrar en una actividad cultural */
    					} else if(numAle<90) {
    						tiempoDormir = calculaAleatorios(2,6);
                          		sleep(tiempoDormir);
                           		sprintf(mensajeLog,"La solicitud_%d tiene errores en los datos personales, atencion correcta.",i+1);
                          		listaDeUsuarios[i].tipoAtencion=1;
    						/*Despues puede participar o no en una actividad cultural*/
    					} else {
    						tiempoDormir = calculaAleatorios(6,10);
                          		sleep(tiempoDormir);
                           		sprintf(mensajeLog,"La solicitud_%d se corresponde con un usuario con antecedentes, atencion correcta.",i+1);
                          		listaDeUsuarios[i].tipoAtencion=2;
    					}
    				}
                              
                /* Log del momento de fin de atencion */
                sprintf(mensaje, "La solicitud_%d ha sido atendida", i+1);
                escribirEnLog(idAtendedor, mensaje);
                /* Log de como ha sido la atencion*/                              
                escribirEnLog(idAtendedor, mensajeLog);

                /*Se cambia el flag de atendido de la solicitud*/
                listaDeUsuarios[i].atendido=1;

                /* Cambiamos el flag de atendido del atendedor*/
                listaAtendedores[identificadorAtendedor].atendiendo=1;

                pthread_mutex_unlock(&mutexColaSolicitudes);
    			}
        		/*Se comprueba si al atendedor le toca atender tomar cafe*/
        		if(listaAtendedores[1].solAtendidas%5 == 0){
        			/* Se registra la entrada al cafe */
        			sprintf(tomarCafe, "El atendedor de invitaciones se va a tomar cafe.");
        			escribirEnLog(idAtendedor, tomarCafe);

        			/* Duerme 10 segundos */
        			sleep(10);
                    
        			/* Se registra la salida al café */
        			sprintf(acabaCafe, "El atendedor de invitaciones regresa de tomar cafe.");
        			escribirEnLog(idAtendedor, acabaCafe);
        		}
        		break;
          
    		/* Atendedor de tipo PRO */
    		default:
    		  pthread_mutex_lock(&mutexColaSolicitudes);
    			for(i=0;i<numSolicitudes;i++){
    				if(listaAtendedores[identificadorAtendedor].atendiendo==0 && listaDeUsuarios[i].atendido==0){
    					int numAle = calculaAleatorios(0,100);
    					int tiempoDormir;
    					if(numAle<70){
    						tiempoDormir = calculaAleatorios(1,4);
                                    sleep(tiempoDormir);
                                    sprintf(mensajeLog,"La solicitud_%d tiene todo en orden, atencion correcta.",i+1);
                                    listaDeUsuarios[i].tipoAtencion=0;
                               	/* Despues puede solicitar o no entrar en una actividad cultural */
    					} else if(numAle<90) {
    						tiempoDormir = calculaAleatorios(2,6);
                                    sleep(tiempoDormir);
                                    sprintf(mensajeLog,"La solicitud_%d tiene errores en los datos personales, atencion correcta.",i+1);
                                    listaDeUsuarios[i].tipoAtencion=1;
                                	/*Despues puede participar o no en una actividad cultural*/
    					} else {
    						tiempoDormir = calculaAleatorios(6,10);
                          		sleep(tiempoDormir);
                           		sprintf(mensajeLog,"La solicitud_%d se corresponde con un usuario con antecedentes, atencion correcta.",i+1);
                          		listaDeUsuarios[i].tipoAtencion=2;
    					}
    				}
    				  
                    /* Log del momento de fin de atencion */
                    sprintf(mensaje, "La solicitud_%d ha sido atendida", i+1);
                    escribirEnLog(idAtendedor, mensaje);
                    /* Log de como ha sido la atencion*/                              
                    escribirEnLog(idAtendedor, mensajeLog);

                    /* Se cambia el flag de atendido */
                    listaDeUsuarios[i].atendido=1;

                    /* Se cambia el flag de atendido del atendedor*/
                    listaAtendedores[identificadorAtendedor].atendiendo=1;

                    pthread_mutex_unlock(&mutexColaSolicitudes);
    			}
    			/*Se comprueba si al atendedor le toca atender tomar cafe*/
    			if(listaAtendedores[identificadorAtendedor].solAtendidas%5 == 0){
    				/* Se registra la entrada al cafe */
    				sprintf(tomarCafe, "El atendedor de invitaciones se va a tomar cafe.");
    				escribirEnLog(idAtendedor, tomarCafe);

    				/* Duerme 10 segundos */
    				sleep(10);

    				/* Se registra la salida al café */
    				sprintf(acabaCafe, "El atendedor de invitaciones regresa de tomar cafe.");
    				escribirEnLog(idAtendedor, acabaCafe);
    	   		}
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
        if(contadorActividad==4){
          	/*Avisa a los usuarios de que comiencen la actividad*/
          	pthread_cond_signal(&condicionIniciarActividad); 
          	sprintf(mensajeLog,"La actividad comienza.");
          	escribirEnLog(idCoordinador, mensajeLog);
          
          	/*Espera que los usuarios hayan finalizado*/
         	pthread_cond_wait(&condicionAcabarActividad, &mutexActividad);
          	sprintf(mensajeLog, "La actividad ha terminado.");
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
			printf("Relanzando menu");
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
      		pthread_create(&listaAtendedores[i].hiloAtendedor, NULL, accionesSolicitud, (void *)&tipoAt[2]);
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
	escribirEnLog("FIN", mensajeLog);
	exit(0);
}

/* Funcion calculaAleatorios: Utilizada para generar numeros aleatorios en el proceso de seleccion de invitaciones. */
int calculaAleatorios(int min, int max){
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


