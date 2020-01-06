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
void compactarArray(int pos);
void escribirEnLog(char* id, char* mensaje);

/* Variables globales */
FILE *logFile;

int numSolicitudes = 15;
int numAtendedores = 1;

int contadorAtendedor;
int contadorSolicitudes;
int numSolicitudesTotal;
int contadorActividad;
int tipoAt[3] = {0,1,2};

struct Usuario{ /*Numero con el que entra en la cola*/
    int id;
    int idSolicitud;
  /*0->No 1->Si */
    int atendido;   
  /*0->Invitacion 1->QR */
    int tipo;   
  /*0->Todo OK 1->Error en datos personales 2->Antecedentes 3->No ha sido atendido*/
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

pthread_t hiloCoordinador;

/* Para controlar el acceso a los recursos compartidos utilizamos dos semáforos (mutex) */
pthread_mutex_t mutexCreaHilos;
pthread_mutex_t mutexColaSolicitudes;
pthread_mutex_t mutexLog;
pthread_mutex_t mutexSolicitudes;
pthread_mutex_t mutexActividad;
pthread_mutex_t mutexColaAtendedores;
pthread_mutex_t mutexListaActividad;
pthread_mutex_t mutexAcabarActividad;
//pthread_mutex_t mutex8;

/*Variables condición*/
pthread_cond_t condicionIniciarActividad;
pthread_cond_t condicionAcabarActividad;
//pthread_cond_t condicionIntermediaActividad;


/* --------------------------- MAIN --------------------------- */
/*Función principal del programa*/
int main(int argc, char const *argv[]){
    printf("-----------------------------------------------BENVINGUTS A L'TSUNAMI DEMOCRÀTIC LLIONÉS-----------------------------------------------\n");
    
    /* Switch de comprobacion de los argumentos iniciales (Parte extra de parametros estáticos). */
    switch(argc){
        case 1:
        /* En el caso de que no haya parametros se muestra un mensaje y se procede con los valores por defecto. */
        printf("No se han introducido parametros de entrada, se inicializaran al valor por defecto (15 solicitudes y 1 atendedorPRO).\n");
        break;
        case 2:
        /* En el caso de que haya un solo parametro (solicitudes) se comprueba que sea válido y se cambia si así es (Si no se procede con los valores por defecto). */
            printf("Se ha introducido un valor para las solicitudes, ");
        if (atoi(argv[1]) < 1){
            printf("pero no era valido, se introdujo un %d pero el valor desde de ser mayor que 1.\n", atoi(argv[1]));
            printf("Se inicializara al valor por defecto (15 solicitudes).\n");
        } else{
            printf("valor cambiado a %d.\n\n", atoi(argv[1]));
            numSolicitudes = atoi(argv[1]);
        }
        break;
        case 3:
            /* En el caso de que se hayan introducido ambos valores, se comprueban y, si son válidos, se cambian (Si no se procede con los valores por defecto). */
        printf("Se han introducido parametros para solicitudes y atendedores.\n");
            printf("El valor de las solicitudes ");
        if (atoi(argv[1]) < 1){
            printf("no es valido, se introdujo un %d pero el valor desde de ser mayor que 1.\n", atoi(argv[1]));
            printf("Se inicializara al valor por defecto (15 solicitudes).\n");
        } else{
            printf("sera cambiado a %d.\n", atoi(argv[1]));
            numSolicitudes = atoi(argv[1]);
        }
        printf("El valor de los atendedores ");
        if (atoi(argv[2]) < 1){
            printf("no es valido, se introdujo un %d pero el valor desde de ser mayor que 1.\n", atoi(argv[2]));
            printf("Se inicializara al valor por defecto (1 atendedor).\n");
        } else{
            printf("sera cambiado a %d.\n", atoi(argv[2]));
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
    if (pthread_mutex_init(&mutexColaAtendedores, NULL)!=0) exit(-1);
    if (pthread_mutex_init(&mutexListaActividad, NULL)!=0) exit(-1);
    if (pthread_mutex_init(&mutexAcabarActividad, NULL)!=0) exit(-1);
    

  /* Inicializamos las variables condición */
    if (pthread_cond_init(&condicionIniciarActividad, NULL)!=0) exit(-1);
    if (pthread_cond_init(&condicionAcabarActividad, NULL)!=0) exit(-1);

    contadorSolicitudes=0;
    numSolicitudesTotal=0;
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
        (listaDeUsuarios+i)->atendido=1;
        j++;
    }
    int num=1;
    
    //pthread_t *atpros;
    //pthread_t atqr,atinvitacion;
    
    pthread_mutex_lock(&mutexCreaHilos);
    /* Casteamos a int el void pointer */
    pthread_create(&listaAtendedores[0].hiloAtendedor, NULL, accionesAtendedor, (void *)(intptr_t)tipoAt[0]);
    pthread_create(&listaAtendedores[1].hiloAtendedor, NULL, accionesAtendedor, (void *)(intptr_t)tipoAt[1]);
    for(i=0;i<numAtendedores;i++){
        pthread_create(&listaAtendedores[i].hiloAtendedor, NULL, accionesAtendedor, (void *)(intptr_t)tipoAt[2]);
    }
    pthread_create(&hiloCoordinador,NULL, accionesCoordinadorSocial, (void *)(intptr_t)num);
    pthread_mutex_unlock(&mutexCreaHilos);

    /* Se imprime el encabezado y se espera un intro para empezar a recibir señales */
    printf("Se podra simular la entrada de invitaciones al programa mediante el uso de señales.\n");
    printf("Introduzca 'kill -10 %d' en el terminal si desea realizar una solicitud por invitacion.\n", getpid());
    printf("Introduzca 'kill -12 %d' en el terminal si desea realizar una solicitud por QR.\n", getpid());
    printf("Introduzca 'kill -13 %d' en el terminal si desea realizar un cambio de los valores.\n", getpid());
    printf("Introduzca 'kill -2 %d' en el terminal si desea finalizar el programa.\n", getpid());
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
    printf("-------------------------------------------------------------------------------------------\n");
    printf("Nº SOLICITUDES:");
    printf("%d\n", contadorSolicitudes+1);
    if(contadorSolicitudes>=numSolicitudes){
        printf("Solicitud rechazada, la cola esta llena\n");
    } else{
        pthread_mutex_lock(&mutexColaSolicitudes); 
        listaDeUsuarios[contadorSolicitudes].id = contadorSolicitudes;
        listaDeUsuarios[contadorSolicitudes].atendido = 0;
        listaDeUsuarios[contadorSolicitudes].tipoAtencion = 3;
        listaDeUsuarios[contadorSolicitudes].idSolicitud = ++numSolicitudesTotal;

        switch(signal){
            case SIGUSR1:
            printf("ES DE INVITACION.\n");
            pthread_mutex_unlock(&mutexColaSolicitudes);
            listaDeUsuarios[contadorSolicitudes].tipo=0;
            pthread_create(&listaDeUsuarios[contadorSolicitudes].hiloUsuario, NULL, accionesSolicitud, (void *)(intptr_t)contadorSolicitudes);
            //pthread_mutex_unlock(&mutexColaSolicitudes);
            break;
            case SIGUSR2:
            printf("ES DE QR.\n");
            pthread_mutex_unlock(&mutexColaSolicitudes);
            listaDeUsuarios[contadorSolicitudes].tipo=1;
            pthread_create(&listaDeUsuarios[contadorSolicitudes].hiloUsuario, NULL, accionesSolicitud, (void *)(intptr_t)contadorSolicitudes);
            //pthread_mutex_unlock(&mutexColaSolicitudes);
            break;
        }
        contadorSolicitudes++;
        pthread_mutex_unlock(&mutexColaSolicitudes);
    }
}

/* Funcion acciones Solicitud: comprobamos si las solicitudes pueden unirse o no a una actividad social*/
void *accionesSolicitud(void *ptr){

    /* Se guarda para escribir en el Log el id del atendedor */
    int identificadorSolicitud;
    char idSolicitud[100];
    char mensajeLog[100];
    identificadorSolicitud = listaDeUsuarios[(intptr_t)ptr].idSolicitud;
    /* Se registra en el Log el momento de llegada de la solicitud */
   
	pthread_mutex_lock(&mutexSolicitudes);
    printf("Solicitud_%d llega a la cola\n", identificadorSolicitud); //TODO printf DEBUG
    sprintf(idSolicitud, "Solicitud_%d", identificadorSolicitud+1);
    sprintf(mensajeLog, "Llega a la cola.");
    escribirEnLog(idSolicitud,mensajeLog);
  	

    	/* Se registra en el Log el tipo de solicitud que ha llegado */
    	if(listaDeUsuarios[identificadorSolicitud].tipo==0){
       	sprintf(mensajeLog, "Es de tipo invitacion");
    	}else{
      	sprintf(mensajeLog, "Es de tipo QR");
    	}
    	escribirEnLog(idSolicitud,mensajeLog);
  	pthread_mutex_unlock(&mutexSolicitudes);
  
    if(listaDeUsuarios[identificadorSolicitud].atendido==0){
        /* Se duerme 4 segundos */
        if(listaDeUsuarios[identificadorSolicitud].tipo == 1){
              pthread_mutex_lock(&mutexSolicitudes);
              /* Si es de QR, se genera el 30% de que se vayan por no ser muy fiables. */
              if(calculaAleatorios(0, 100) > 70){
                  compactarArray((intptr_t)ptr);
                	identificadorSolicitud = listaDeUsuarios[(intptr_t)ptr].idSolicitud;
                	printf("Se expulso a la solicitud %d por no ser demasiado fiable.\n" ,identificadorSolicitud);
                  pthread_mutex_unlock(&mutexSolicitudes);
                  contadorSolicitudes--;   
                  pthread_exit(NULL);
              }else{
                    pthread_mutex_unlock(&mutexSolicitudes);
              }
        }
      }
  
    /* Se declaran variables que usaran en el for*/
    int i, tipo;
    int puedeIrActividad = -1;
    while(puedeIrActividad != 0){
        pthread_mutex_lock(&mutexSolicitudes);
        identificadorSolicitud = listaDeUsuarios[(intptr_t)ptr].idSolicitud;
        tipo = listaDeUsuarios[(intptr_t)ptr].tipoAtencion;
        printf("La solicitud %d es de tipo: %d\n", identificadorSolicitud, tipo);
        switch(tipo){
            /* Solicitud que llega de manera correcta */
            case 0:
                if(calculaAleatorios(0,100) == 202){ //TODO CAMBIAR A 50%
                    //printf("esperamos a que el mutex se desbloquee\n");
                    /* Escribimos en el log que el usuario no quiere participar */
                    printf("La solicitud %d decide no pertenecer a una actividad cultural.\n", identificadorSolicitud);
                    sprintf(mensajeLog,"El usuario decide no pertenecer a una actividad cultural.");
                    escribirEnLog(idSolicitud,mensajeLog);
                    compactarArray((intptr_t)ptr);
                    pthread_mutex_unlock(&mutexSolicitudes);
                    pthread_exit(NULL);           
                }
                puedeIrActividad = 0;
                break;
            /* Solicitud que tiene errores en datos personales */
            case 1:
                if(calculaAleatorios(0,100) == 202){ //TODO CAMBIAR A 50%
                    /* Escribimos en el log que el usuario no quiere participar */
                    printf("La solicitud %d decide no pertenecer a una actividad cultural.\n", identificadorSolicitud);
                    sprintf(mensajeLog,"El usuario decide no pertenecer a una actividad cultural.");
                    escribirEnLog(idSolicitud,mensajeLog);
                    compactarArray((intptr_t)ptr);
                    pthread_mutex_unlock(&mutexSolicitudes);
                    pthread_exit(NULL);                 
                }
                puedeIrActividad = 0;
            break;
            /* Solicitud que llega con errores por antecedentes */
            case 2:
                /* Escribimos en el log que el usuario no puede participar y es expulsado*/
                printf("La solicitud %d no puede pertenecer a una actividad cultural. Es expulsado!\n", identificadorSolicitud);
                sprintf(mensajeLog,"El usuario no puede pertenecer a una actividad cultural. Es expulsado!");
                escribirEnLog(idSolicitud,mensajeLog);
                compactarArray((intptr_t)ptr);
                pthread_mutex_unlock(&mutexSolicitudes);
                pthread_exit(NULL);
                break;
            /* Solicitud llega cuando no ha sido atendida por ningun atendedor (queda en espera) */
            case 3:
                /* Si es de Invitacion se genera el aleatorio y si sale 10% se va */
                if(listaDeUsuarios[(intptr_t)ptr].tipo == 0){
                    if(calculaAleatorios(0,100)>90){
                        /* Escribimos en el log que el usuario se ha cansado */
                        printf("La solicitud %d se cansa de esperar y se va.\n", identificadorSolicitud);
                        sprintf(mensajeLog,"El usuario se cansa de esperar y se va.");
                        escribirEnLog(idSolicitud,mensajeLog);
                        compactarArray((intptr_t)ptr);
                        pthread_mutex_unlock(&mutexSolicitudes);
                        pthread_exit(NULL);                                  
                    }
                }
                /* Sea de cualquier tipo se comprueba el 15% de que la app lo expulse de forma aleatoria */
                if(calculaAleatorios(0,100)>85){
                    //TODO Borrar este printf despues del debug
                    printf("Se expulso a la solicitud %d por el fallo random de la app.\n" ,identificadorSolicitud);
                    compactarArray((intptr_t)ptr);
                    pthread_mutex_unlock(&mutexSolicitudes);
                    pthread_exit(NULL);
                }else{
                    pthread_mutex_unlock(&mutexSolicitudes);
                    sleep(4);
                }
                break;
            default:
                perror("Error en el tipo de atencion de la solicitud \n");
                pthread_exit(NULL);
        }
    }
    printf("La solicitud %d decide que quiere ir a una actividad cultural.\n", identificadorSolicitud);
    sprintf(mensajeLog, "Decide entrar a la cola de actividades culturales");
    pthread_mutex_unlock(&mutexSolicitudes);
    escribirEnLog(idSolicitud, mensajeLog);
    int puede = 1;
    int j;
    while(TRUE){
        pthread_mutex_lock(&mutexListaActividad);
        //printf("MUTEX LISTA ACTI COGIDO\n");
        if(contadorActividad<4){
          	pthread_mutex_lock(&mutexSolicitudes);
            listaActividad[contadorActividad]=listaDeUsuarios[(intptr_t)ptr];
          	compactarArray((intptr_t)ptr);
          	pthread_mutex_unlock(&mutexSolicitudes);
            //TODO Sacar de la lista de Usuarios a los hilos que entran en la actividad
            //pthread_mutex_unlock(&mutexActividad);
            pthread_mutex_unlock(&mutexListaActividad);
            pthread_mutex_lock(&mutexActividad);

            printf(" ----- Hilo %d Esta preparado para iniciar actividad-----\n", contadorActividad);
            printf("-----------------------------------------------------------------------------\n");
            sprintf(mensajeLog, "Esta preparado para iniciar actividad");
            escribirEnLog(idSolicitud, mensajeLog);
            contadorActividad++;
            //printf("CONTADOR ACTIVIDAD: %d",contadorActividad);
            /* Bucle while permite retener a los hilos hasta que llegue el ultimo */
            while(puede!=0){
              if(contadorActividad==4){
                    pthread_cond_wait(&condicionIniciarActividad,&mutexActividad);
                    sleep(3);
                    //printf("Esperando...\n");
                    pthread_mutex_lock(&mutexAcabarActividad);
                    //printf("Decrementando el contador\n");

                    contadorActividad=0;
                    //printf("CONTADOR ACTIVIDAD: %d",contadorActividad);
                    pthread_mutex_unlock(&mutexAcabarActividad);
                    //printf("SOMOS 0\n");
                    for(j=0;j<3;j++){
                      pthread_cancel(listaActividad[i].hiloUsuario);
                    }
                    pthread_cond_signal(&condicionAcabarActividad);
              }else{
                pthread_mutex_unlock(&mutexActividad);
                sleep(20);
                
              }
            }
            //printf("Sale de la actividad\n");
            sprintf(mensajeLog, "Sale de la actividad");
            escribirEnLog(idSolicitud, mensajeLog);
                
            //compactarArray(identificadorSolicitud);
            pthread_exit(NULL);
        }else{
            pthread_mutex_unlock(&mutexListaActividad);
            sleep(10);
            //pthread_mutex_unlock(&mutexActividad);          
        }
    }
    
    //pthread_cond_wait(&condicionIniciarActividad, &mutexActividad);
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
    sprintf(idAtendedor, "Atendedor_%d: ", identificadorAtendedor+1);
    char mensajeLog[100];
    char mensaje[100];
    char tomarCafe[100];
    char acabaCafe[100];

    /* Los atendedores se quedaran en un bucle infinito atendiendo solicitudes. */
    while(TRUE){    
        srand(time(NULL));
        switch(identificadorAtendedor){
            /* Caso de que sea un atendedor de invitaciones. */
            case 0:
                pthread_mutex_lock(&mutexSolicitudes);
               /* Se busca solicitudes de tipo 0 (Invitacion). */
                for(i=0;i<numSolicitudes;i++){
                  if(listaDeUsuarios[i].tipo==0 && listaDeUsuarios[i].atendido==0 && listaAtendedores[identificadorAtendedor].atendiendo==0){
                      /* Cambiamos el flag de atendiendo del atendedor. */
                      listaAtendedores[identificadorAtendedor].atendiendo=1;
                      listaAtendedores[identificadorAtendedor].solAtendidas++;

                          /* Calculamos el tipo de atencion. */
                          int numAle = calculaAleatorios(0,100);

                      /* Primer caso, 70% de que la solicitud tenga todo en orden. */
                      if(numAle < 70){
                          sleep(calculaAleatorios(1,4));
                          sprintf(mensajeLog,"La solicitud_%d tiene todo en orden.", listaDeUsuarios[i].idSolicitud);
                          printf("La solicitud_%d tiene todo en orden.\n", listaDeUsuarios[i].idSolicitud);
                          listaDeUsuarios[i].tipoAtencion=0;
                      /* Segundo caso, 20% de que la solicitud tenga algún error en los datos personales. */
                      } else if(numAle < 90) {
                          sleep(calculaAleatorios(2,6));
                          sprintf(mensajeLog,"La solicitud_%d tiene errores en los datos personales.",listaDeUsuarios[i].idSolicitud);
                          printf("La solicitud_%d tiene errores en los datos personales.\n",listaDeUsuarios[i].idSolicitud);
                          listaDeUsuarios[i].tipoAtencion=1;
                      /* Tercer y ultimo caso, 10% de que se corresponda con un usuario con antecedentes. */
                      } else {
                          sleep(calculaAleatorios(6,10));
                          sprintf(mensajeLog,"La solicitud_%d se corresponde con un usuario con antecedentes.",listaDeUsuarios[i].idSolicitud);
                          printf("La solicitud_%d se corresponde con un usuario con antecedentes.\n",listaDeUsuarios[i].idSolicitud);
                          listaDeUsuarios[i].tipoAtencion=2;
                      }


                      /* Log del momento de fin de atencion. */
                      printf("La solicitud_%d (aka %d) ha sido atendida correctamente.\n", listaDeUsuarios[i].idSolicitud, i+1);
                      sprintf(mensaje, "La solicitud_%d ha sido atendida correctamente", listaDeUsuarios[i].idSolicitud);
                      escribirEnLog(idAtendedor, mensaje);
                      /* Log de como ha sido la atencion. */                              
                      escribirEnLog(idAtendedor, mensajeLog);

                      /* Se cambia el flag de atendido de la solicitud y se desbloquea el mutex. */
                      listaDeUsuarios[i].atendido=1;
                      pthread_mutex_unlock(&mutexSolicitudes);

                        /* Comprobacion si le toca tomar cafe. */
                      if(listaAtendedores[0].solAtendidas != 0){
                          if(listaAtendedores[0].solAtendidas%5 == 0){
                                          /* Se registra la entrada al cafe. */
                              printf("**El atendedor de invitaciones se va a tomar cafe.** \n");
                              sprintf(tomarCafe, "El atendedor de invitaciones se va a tomar cafe.");
                              escribirEnLog(idAtendedor, tomarCafe);

                              /* Duerme 10 segundos */
                              sleep(10);

                              /* Se registra la salida al cafe. */
                              printf("**El atendedor de invitaciones regresa de tomar cafe.** \n");
                              sprintf(acabaCafe, "El atendedor de invitaciones regresa de tomar cafe.");
                              escribirEnLog(idAtendedor, acabaCafe);
                          }
                      }
                }
            }
          /* En este caso no existen solicitudes por invitacion, atendera la que mas tiempo lleve. */
            for(i=0;i<numSolicitudes;i++){
                if(listaDeUsuarios[i].atendido==0 && listaAtendedores[identificadorAtendedor].atendiendo==0){
                    //printf("VA A ATENDER UN INVITACIONES LA SOLICITUD %d, AUNQUE NO ES DE SU INCUMBENCIA\n", i); //TODO printf debug
                      /* Cambiamos el flag de atendiendo del atendedor. */
                      listaAtendedores[identificadorAtendedor].atendiendo=1;
                      listaAtendedores[identificadorAtendedor].solAtendidas++;

                          /* Calculamos el tipo de atencion. */
                          int numAle = calculaAleatorios(0,100);

                      /* Primer caso, 70% de que la solicitud tenga todo en orden. */
                      if(numAle < 70){
                          sleep(calculaAleatorios(1,4));
                          sprintf(mensajeLog,"La solicitud_%d tiene todo en orden.", listaDeUsuarios[i].idSolicitud);
                          printf("La solicitud_%d tiene todo en orden.\n", listaDeUsuarios[i].idSolicitud);
                          listaDeUsuarios[i].tipoAtencion=0;
                      /* Segundo caso, 20% de que la solicitud tenga algún error en los datos personales. */
                      } else if(numAle < 90) {
                          sleep(calculaAleatorios(2,6));
                          sprintf(mensajeLog,"La solicitud_%d tiene errores en los datos personales.",listaDeUsuarios[i].idSolicitud);
                          printf("La solicitud_%d tiene errores en los datos personales.\n",listaDeUsuarios[i].idSolicitud);
                          listaDeUsuarios[i].tipoAtencion=1;
                      /* Tercer y ultimo caso, 10% de que se corresponda con un usuario con antecedentes. */
                      } else {
                          sleep(calculaAleatorios(6,10));
                          sprintf(mensajeLog,"La solicitud_%d se corresponde con un usuario con antecedentes.",listaDeUsuarios[i].idSolicitud);
                          printf("La solicitud_%d se corresponde con un usuario con antecedentes.\n",listaDeUsuarios[i].idSolicitud);
                          listaDeUsuarios[i].tipoAtencion=2;
                      }


                      /* Log del momento de fin de atencion. */
                      printf("La solicitud_%d ha sido atendida correctamente.\n", listaDeUsuarios[i].idSolicitud);
                      sprintf(mensaje, "La solicitud_%d ha sido atendida correctamente", listaDeUsuarios[i].idSolicitud);
                      escribirEnLog(idAtendedor, mensaje);
                      /* Log de como ha sido la atencion. */                              
                      escribirEnLog(idAtendedor, mensajeLog);

                      /* Se cambia el flag de atendido de la solicitud y se desbloquea el mutex. */
                      listaDeUsuarios[i].atendido=1;
                      pthread_mutex_unlock(&mutexSolicitudes);

                        /* Comprobacion si le toca tomar cafe. */
                      if(listaAtendedores[0].solAtendidas != 0){
                          if(listaAtendedores[0].solAtendidas%5 == 0){
                                          /* Se registra la entrada al cafe. */
                              printf("**El atendedor de invitaciones se va a tomar cafe.**\n");
                              sprintf(tomarCafe, "El atendedor de invitaciones se va a tomar cafe.");
                              escribirEnLog(idAtendedor, tomarCafe);

                              /* Duerme 10 segundos */
                              sleep(10);

                              /* Se registra la salida al cafe. */
                              printf("**El atendedor de invitaciones regresa de tomar cafe.**\n");
                              sprintf(acabaCafe, "El atendedor de invitaciones regresa de tomar cafe.");
                              escribirEnLog(idAtendedor, acabaCafe);
                          }
                      }
                }
            }
        /* En caso de que no encuentre ninguna solicitud que atender libera el mutex. */
            pthread_mutex_unlock(&mutexSolicitudes);                    
            break;
            /* Caso de que sea un atendedor de tipo QR. */
            case 1:
            /* Se espera al mutex para empezar a revisar las solicitudes. */
            pthread_mutex_lock(&mutexSolicitudes);
            for(i=0;i<numSolicitudes;i++){
                /* Se comprueba que el atendedor puede atender dicha solicitud y que esta es de tipo QR. */
                if(listaDeUsuarios[i].tipo==1 && listaDeUsuarios[i].atendido==0 && listaAtendedores[identificadorAtendedor].atendiendo==0){
                  
                  //printf("VA A ATENDER UN QR UNA DE LAS SUYAS.\n"); //TODO printf debug
                    /* Cambiamos el flag de atendiendo del atendedor. */
                    listaAtendedores[identificadorAtendedor].atendiendo=1;
                    listaAtendedores[identificadorAtendedor].solAtendidas++;
                  
                    /* Calculamos el tipo de atencion. */
                        int numAle = calculaAleatorios(0,100);
    
                    /* Primer caso, 70% de que la solicitud tenga todo en orden. */
                    if(numAle < 70){
                        sleep(calculaAleatorios(1,4));
                        sprintf(mensajeLog,"La solicitud_%d tiene todo en orden.", listaDeUsuarios[i].idSolicitud);
                        printf("La solicitud_%d tiene todo en orden.\n", listaDeUsuarios[i].idSolicitud);
                        listaDeUsuarios[i].tipoAtencion=0;
                    /* Segundo caso, 20% de que la solicitud tenga algún error en los datos personales. */
                    } else if(numAle < 90) {
                        sleep(calculaAleatorios(2,6));
                        sprintf(mensajeLog,"La solicitud_%d tiene errores en los datos personales.",listaDeUsuarios[i].idSolicitud);
                        printf("La solicitud_%d tiene errores en los datos personales.\n",listaDeUsuarios[i].idSolicitud);
                        listaDeUsuarios[i].tipoAtencion=1;
                    /* Tercer y ultimo caso, 10% de que se corresponda con un usuario con antecedentes. */
                    } else {
                        sleep(calculaAleatorios(6,10));
                        sprintf(mensajeLog,"La solicitud_%d se corresponde con un usuario con antecedentes.",listaDeUsuarios[i].idSolicitud);
                        printf("La solicitud_%d se corresponde con un usuario con antecedentes.\n",listaDeUsuarios[i].idSolicitud);
                        listaDeUsuarios[i].tipoAtencion=2;
                    }


                    /* Log del momento de fin de atencion. */
                    printf("La solicitud_%d ha sido atendida correctamente.\n", listaDeUsuarios[i].idSolicitud);
                    sprintf(mensaje, "La solicitud_%d ha sido atendida correctamente", listaDeUsuarios[i].idSolicitud);
                    escribirEnLog(idAtendedor, mensaje);
                    /* Log de como ha sido la atencion. */                              
                    escribirEnLog(idAtendedor, mensajeLog);

                    /* Se cambia el flag de atendido de la solicitud y se desbloquea el mutex. */
                    listaDeUsuarios[i].atendido=1;
                    pthread_mutex_unlock(&mutexSolicitudes);
                
                  /* Se comprueba si el atendedor de QR's tiene que ir a tomar cafe. */
                    if(listaAtendedores[1].solAtendidas != 0){
                        if(listaAtendedores[1].solAtendidas%5 == 0){
                            /* Se registra la entrada al cafe. */
                            sprintf(tomarCafe, "El atendedor de QR se va a tomar cafe.");
                            printf("**El atendedor de QR se va a tomar cafe.**\n");
                            escribirEnLog(idAtendedor, tomarCafe);

                            /* Duerme 10 segundos. */
                            sleep(10);

                            /* Se registra la salida al cafe. */
                            sprintf(acabaCafe, "El atendedor de QR regresa de tomar cafe.");
                            printf("**El atendedor de QR regresa de tomar cafe.**\n");
                            escribirEnLog(idAtendedor, acabaCafe);
                        }
                    }
                }
            }
            
                /* En este caso no existen solicitudes por invitacion, se atendera la que mas tiempo lleve. */
            for(i=0;i<numSolicitudes;i++){
                if(listaDeUsuarios[i].atendido==0 && listaAtendedores[identificadorAtendedor].atendiendo==0){
                  //  printf("VA A ATENDER UNO DE QR LA SOLICITUD %d, AUNQUE NO SEA DE LAS SUYAS\n", i); //TODO PRINT DEBUG
                  
                  /* Cambiamos el flag de atendiendo del atendedor. */
                  listaAtendedores[identificadorAtendedor].atendiendo=1;
                  listaAtendedores[identificadorAtendedor].solAtendidas++;
                  
                    /* Calculamos el tipo de atencion. */
                        int numAle = calculaAleatorios(0,100);
    
                    /* Primer caso, 70% de que la solicitud tenga todo en orden. */
                    if(numAle < 70){
                        sleep(calculaAleatorios(1,4));
                        sprintf(mensajeLog,"La solicitud_%d tiene todo en orden.", listaDeUsuarios[i].idSolicitud);
                        printf("La solicitud_%d tiene todo en orden.\n", listaDeUsuarios[i].idSolicitud);
                        listaDeUsuarios[i].tipoAtencion=0;
                    /* Segundo caso, 20% de que la solicitud tenga algún error en los datos personales. */
                    } else if(numAle < 90) {
                        sleep(calculaAleatorios(2,6));
                        sprintf(mensajeLog,"La solicitud_%d tiene errores en los datos personales.",listaDeUsuarios[i].idSolicitud);
                        printf("La solicitud_%d tiene errores en los datos personales.\n",listaDeUsuarios[i].idSolicitud);
                        listaDeUsuarios[i].tipoAtencion=1;
                    /* Tercer y ultimo caso, 10% de que se corresponda con un usuario con antecedentes. */
                    } else {
                        sleep(calculaAleatorios(6,10));
                        sprintf(mensajeLog,"La solicitud_%d se corresponde con un usuario con antecedentes.",listaDeUsuarios[i].idSolicitud);
                        printf("La solicitud_%d se corresponde con un usuario con antecedentes.\n",listaDeUsuarios[i].idSolicitud);
                        listaDeUsuarios[i].tipoAtencion=2;
                    }


                    /* Log del momento de fin de atencion. */
                    printf("La solicitud_%d ha sido atendida correctamente.\n", listaDeUsuarios[i].idSolicitud);
                    sprintf(mensaje, "La solicitud_%d ha sido atendida correctamente", listaDeUsuarios[i].idSolicitud);
                    escribirEnLog(idAtendedor, mensaje);
                    /* Log de como ha sido la atencion. */                              
                    escribirEnLog(idAtendedor, mensajeLog);

                    /* Se cambia el flag de atendido de la solicitud y se desbloquea el mutex. */
                    listaDeUsuarios[i].atendido=1;
                    pthread_mutex_unlock(&mutexSolicitudes);
                
                  /* Se comprueba si el atendedor de QR's tiene que ir a tomar cafe. */
                    if(listaAtendedores[1].solAtendidas != 0){
                        if(listaAtendedores[1].solAtendidas%5 == 0){
                            /* Se registra la entrada al cafe. */
                            sprintf(tomarCafe, "**El atendedor de QR se va a tomar cafe.**");
                            printf("El atendedor de QR se va a tomar cafe.\n");
                            escribirEnLog(idAtendedor, tomarCafe);

                            /* Duerme 10 segundos. */
                            sleep(10);

                            /* Se registra la salida al cafe. */
                            sprintf(acabaCafe, "El atendedor de QR regresa de tomar cafe.");
                            printf("**El atendedor de QR regresa de tomar cafe.**\n");
                            escribirEnLog(idAtendedor, acabaCafe);
                        }
                    }
                }
            }
        /* Si no ha encontrado ninguna solicitud de ningun tipo libera el mutex. */
            pthread_mutex_unlock(&mutexSolicitudes);                    
            break;
            
            /* Atendedor de tipo PRO. */
            default:
            pthread_mutex_lock(&mutexSolicitudes);
            /* Recorre una sola vez las solicitudes ya que le da igual el tipo, solo busca la que mas tiempo lleve esperando. */
            for(i=0;i<numSolicitudes;i++){
                if(listaAtendedores[identificadorAtendedor].atendiendo==0 && listaDeUsuarios[i].atendido==0){
                   /* Se cambia el flag de atendiendo del atendedor. */
                    listaAtendedores[identificadorAtendedor].atendiendo=1;
                    listaAtendedores[identificadorAtendedor].solAtendidas++;
                   
                    //printf("VA A ATENDER UN PRO LA SOLICITUD %d\n", i); //TODO printf debug
                   
                    int numAle = calculaAleatorios(0,100);
                    /* Primer caso, 70% de que la solicitud tenga todo en orden. */
                    if(numAle < 70){
                        sleep(calculaAleatorios(1,4));
                        sprintf(mensajeLog,"La solicitud_%d tiene todo en orden.", listaDeUsuarios[i].idSolicitud);
                        printf("La solicitud_%d tiene todo en orden.\n", listaDeUsuarios[i].idSolicitud);
                        listaDeUsuarios[i].tipoAtencion=0;
                    /* Segundo caso, 20% de que la solicitud tenga algún error en los datos personales. */
                    } else if(numAle < 90) {
                        sleep(calculaAleatorios(2,6));
                        sprintf(mensajeLog,"La solicitud_%d tiene errores en los datos personales.",listaDeUsuarios[i].idSolicitud);
                        printf("La solicitud_%d tiene errores en los datos personales.\n",listaDeUsuarios[i].idSolicitud);
                        listaDeUsuarios[i].tipoAtencion=1;
                    /* Tercer y ultimo caso, 10% de que se corresponda con un usuario con antecedentes. */
                    } else {
                        sleep(calculaAleatorios(6,10));
                        sprintf(mensajeLog,"La solicitud_%d se corresponde con un usuario con antecedentes.",listaDeUsuarios[i].idSolicitud);
                        printf("La solicitud_%d se corresponde con un usuario con antecedentes.\n",listaDeUsuarios[i].idSolicitud);
                        listaDeUsuarios[i].tipoAtencion=2;
                    }
                    
                    /* Log del momento de fin de atencion. */
                    sprintf(mensaje, "La solicitud_%d ha sido atendida correctamente.\n", listaDeUsuarios[i].idSolicitud);
                    printf("La solicitud_%d ha sido atendida correctamente.\n", listaDeUsuarios[i].idSolicitud);
                    escribirEnLog(idAtendedor, mensaje);
                    /* Log de como ha sido la atencion. */                              
                    escribirEnLog(idAtendedor, mensajeLog);
                  
                    /* Se cambia el flag de atendido de la solicitud. */
                    listaDeUsuarios[i].atendido=1;
            
                  /* Se desbloquea el mutex. */
                  pthread_mutex_unlock(&mutexSolicitudes);
                            
                  /* Se comprueba si al atendedor le toca atender tomar cafe. */
                    if(listaAtendedores[identificadorAtendedor].solAtendidas != 0){
                        if(listaAtendedores[identificadorAtendedor].solAtendidas%5 == 0){
                            /* Se registra la entrada al cafe. */
                            sprintf(tomarCafe, "**El atendedor pro se va a tomar cafe.**");
                            printf("El atendedor pro se va a tomar cafe.\n");
                            escribirEnLog(idAtendedor, tomarCafe);

                            /* Duerme 10 segundos. */
                            sleep(10);

                            /* Se registra la salida al cafe. */
                            sprintf(acabaCafe, "**El atendedor pro regresa de tomar cafe.**");
                            printf("El atendedor pro regresa de tomar cafe.\n");
                            escribirEnLog(idAtendedor, acabaCafe);
                        }
                    }
                }
            }
        
            /* Si no ha encontrado ninguna solicitud disponible libera el mutex. */
            pthread_mutex_unlock(&mutexSolicitudes);    
        }
        /* Se libera al atendedor para que puede atender mas solicitudes. */
        listaAtendedores[identificadorAtendedor].atendiendo=0;
    }
                   
}

/* Funcion accionesCoordinadorSocial: se encarga de gestionar las actividades sociales*/
void *accionesCoordinadorSocial(void *ptr){
    char mensajeLog[100];
    char idCoordinador[100];
    sprintf(idCoordinador,"Coordinador_1: ");

    while(TRUE){
        //printf("INTENTO COGER EL MUTEX\n");
            /* Espera a que le avisen de que puede iniciar */
            pthread_mutex_lock(&mutexListaActividad);
        //printf("EYEYEY\n");
            if(contadorActividad == 4){
                //pthread_mutex_lock(&mutexListaActividad);
                //printf("BLOQUEAMOS MUTEX LISTA");
                //pthread_mutex_unlock(&mutexActividad);
                //printf("DESBLOQUEAMOS MUTEX ACTIVIDAD");
                printf("------------------------------------------------------------------------\n");
                sprintf(mensajeLog,"La actividad comienza.");
                escribirEnLog(idCoordinador, mensajeLog);
                printf("La actividad comienza.\n");
                
                pthread_cond_signal(&condicionIniciarActividad);

              printf("EL CORDINADOR ESPERA A ACABAR ACTIVIDAD\n");
                pthread_cond_wait(&condicionAcabarActividad,&mutexListaActividad);
                sprintf(mensajeLog, "La actividad ha terminado.");
                escribirEnLog(idCoordinador, mensajeLog);
                printf("La actividad ha terminado \n");
                /* Reabre la lista para que mas solicitudes puedan intentar entrar */
                pthread_mutex_unlock(&mutexListaActividad);  
                printf("------------------------------------------------------------------------\n");

            } else{
                printf("**COORDINADOR ESPERANDO**\n");
                  pthread_mutex_unlock(&mutexListaActividad);
                  sleep(3);
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
        printf("Valor de las solicitudes incrementados en %d hasta %d.\n", nuevoValor, numSolicitudes);
        //TODO HAbria que incrementar numSOlicitudes bloqueando colaSOlicitudes
        numSolicitudes += nuevoValor;
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
    pthread_mutex_lock(&mutexColaSolicitudes);
    //TODO O Poner a 0 la cola o subir numSOlicitudes a un valor grande para que no entren mas
    //TODO Como coño haceer que una vez una solicitud se acepte ( UN FLAG EN USUARIO QUE SEA SI ESTAN ABIERTAS O NO Y DESDE AQUI SE TOCASE ???)
    pthread_mutex_unlock(&mutexColaSolicitudes);

    
    /* Se liberan los punteros abiertos para liberar la memoria. */
    free(listaAtendedores);
    free(listaDeUsuarios);

    // -------------------------PRUEBA
   // printf("PRUEBA: Acabose\n");

    /* Se escribe en el log que se sale del programa y se sale de manera efectiva. */
    sprintf(mensajeLog, "Saliendo del programa.");
    printf("Saliendo del programa.\n");
    escribirEnLog("FIN", mensajeLog);
    exit(0);
}

/* Funcion calculaAleatorios: Utilizada para generar numeros aleatorios en el proceso de seleccion de invitaciones. */
int calculaAleatorios(int min, int max){
    /* Se utiliza la libreria stdlib para generar un numero aleatorio entre 0 y 1, posteriormente se opera para que sea un entero entre el minimo y el maximo. */
    return rand() % (max-min+1) + min;
}

void compactarArray(int posicion){
    int i;
    pthread_mutex_lock(&mutexColaSolicitudes);
    for(i=posicion;i<numSolicitudes-1;i++){
        //listaDeUsuarios[i].id = listaDeUsuarios[i+1].id;
            //printf("Compactamos a la posicion %d la putisima posicion %d.\n", i, i+1);
        listaDeUsuarios[i] = listaDeUsuarios[i+1];
    }
    //printf("Se va a tocar la puta posicion %d\n", numSolicitudes-1);
    listaDeUsuarios[numSolicitudes-1].id = 0; 
    listaDeUsuarios[numSolicitudes-1].tipoAtencion = -1;
    listaDeUsuarios[numSolicitudes-1].atendido = 1;
    contadorSolicitudes--;
    pthread_mutex_unlock(&mutexColaSolicitudes);
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


