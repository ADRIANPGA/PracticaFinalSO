#!/bin/bash

echo -e
echo Practica Final SSOO 2019-2020
echo -e
echo Adrián Pérez García
echo Diego Fernandez Velasco
echo Pablo de la Hera Martinez
echo Pablo Javier Barrio Navarro


if test -f PracticaFinal.c
then
	entrada=0
	salida=4
	while test $entrada -ne $salida
	do
		echo -e 
		echo Introduce una opción para realizar la acción deseada:
		echo 1 - Mostrar el codigo del programa
		echo 2 - Compilar el programa
		echo 3 - Ejecutarlo
		echo 4 - Salir
		read entrada
		echo -e 
		case $entrada in
			1)	
				cat PracticaFinal.c
			;;
			2)
				echo Compilando...
				gcc PracticaFinal.c -o PracticaFinal
				echo Codigo compilado con exito
			;;
			3)
				if test -f PracticaFinal
				then
					if test -x PracticaFinal
					then
						./PracticaFinal
					else
						echo Error: No hay permisos de ejecucion
					fi
				else 
					echo Error: No existe el ejecutable
				fi
			;;
			4)
				echo Saliendo del programa
			;;
			*)
				echo Introduce un valor entre 1 y 4
				echo Relanzando menu
			;;
		esac
	done
else
	echo Error: No se encuentra el codigo del programa
fi