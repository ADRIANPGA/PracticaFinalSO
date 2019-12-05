/* Inclusion de las librerias necesarias */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

/* Definicion de las funciones */
int calculaAleatorios(int min, int max);

/* Funcion principal */
int main(int argc, char const *argv[]) {
	printf("Funka esto.\n");

	/* Switch de comprobacion de los argumentos iniciales (Parte extra de parametros estáticos) */
	int numSolicitudes = 15, numAtendedores = 1;
	switch(argc){
		case 1:
			printf("No se han introducido parametros de entrada.\n");
			printf("Se inicializaran al valor por defecto (15 solicitudes y 1 atendedorPRO).\n\n");
		break;
		case 2:
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
        	printf("Se han introducido los dos valores.\n");

        	printf("El valor de las solicitudes \n");
         	if (atoi(argv[1]) < 1){
            	printf("no es valido, se introdujo un %d pero el valor desde de ser mayor que 1.\n", atoi(argv[1]));
            	printf("Se inicializara al valor por defecto (15 solicitudes).\n\n");
            } else{
            	printf("valor cambiado a %d.\n\n", atoi(argv[1]));
            	numSolicitudes = atoi(argv[1]);
            }

            printf("El valor de los atendedores \n");
            if (atoi(argv[2]) < 1){
            	printf("no es valido, se introdujo un %d pero el valor desde de ser mayor que 1.\n", atoi(argv[2]));
            	printf("Se inicializara al valor por defecto (1 atendedor).\n\n");
            } else{
            	printf("valor cambiado a %d.\n\n", atoi(argv[2]));
            	numAtendedores = atoi(argv[2]);
            }
        break;
        default:
        	printf("Error en los parametros de entrada (%d de 2) : Se inicializaran los parametros a los valores por defecto (15 solicitudes y 1 atendedor).\n", argc-1);
        break;
	}

	//Si pasa esto ya funka el programa
}

int calculaAleatorios(int min, int max){
	return rand() % (max-min+1) + min;
}
