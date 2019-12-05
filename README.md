# PracticaFinalSO
Enunciado de la práctica final de S.O.
Definici´on del problema
El objetivo de la pr´actica es realizar un programa que simule una aplicaci´on social para la convocatoria de reuniones democr´atico-culturales pac´ıficas
en Le´on. A dicha aplicaci´on se accedera por invitaci´on o lectua de un QR y
tiene que ser aprobado por alguno de los miembros seniors de la organizaci´on
(threads). La pr´actica tiene una parte b´asica donde se define el funcionamiento
b´asico del sistema y que es obligatorio implementar (supondr´a como mucho el
80 % de la nota) y, adem´as, se proponen una serie de mejoras opcionales para
aumentar la nota (como mucho supondr´a el 20 % de la nota).
Para aprobar la pr´actica es necesario que la parte b´asica funcione correctamente. La nota se asignar´a en base a la calidad del c´odigo entregado, valor´andose:
Pol´ıtica de nombres (coherencia en el nombrado de variables y funciones).
Sangrado (indentaci´on).
Comentarios (cantidad, calidad y presentaci´on).
Legibilidad: Nombre de variables, funciones, etc.
Reusabilidad y mantenibilidad: Uso de par´ametros, etc.
La pr´actica es en grupo y se evaluar´a seg´un la metodolog´ıa CTMTC explicada en clase.
Parte B´asica (80 % de la nota)
La aplicaci´on funciona de la siguiente manera:
Los usuarios para poder emplear la aplicaci´on necesitan que se acepte su
solicitud por personal senior de la misma, esto supone entrar en una cola
de espera para que la solicitud sea aceptada y se pueda comenzar a usar.
1
La cola de solicitudes tiene un m´aximo de 15 usuarios, si se supera esa cifra
la solicitud ser´a rechazada. Para atender estas solicitudes se cuenta con 3
usuarios uno para las solicitudes por invitaci´on, otro para las solicitudes
por QR y un tercero que puede procesar ambos tipos de solicitudes (atendedor PRO) y que se dedica a unas u otras indistintamente. Esto implica
que 3 solicitudes pueden ser atendidas mientras las otras 12 esperan.
Un 30 % de las solicitudes por QR se descartan por no considerarse muy
fiables y un 10 % de las solicitudes por invitaci´on lo hacen por cansarse de
esperar. Adem´as La aplicaci´on no est´a muy bien hecha y descarta solicitudes de forma aleatoria y sin comunicarlo, as´ı que del porcentaje restante
de solicitudes encoladas se descarta un 15 %.
A cada solicitud se le asignar´a un identificador ´unico y secuencial (solicitud 1, solicitud 2,... solicitud N) a medida que vayan comenzando el
proceso.
Una vez una solicitud haya sido aceptada esta puede vincularse a una
inciativa cultural. Para que una iniciativa siga adelante es necesario tener
a 4 usuarios. Las iniciativas las gestiona un coordinador cultural. Que
una vez est´en los 4 solicitantes listos comienza la actividad que dura 3
segundos.
Si ya se tienen 4 solicitudes asociadas a una actividad cualquier nueva
solicitud que quiera vincularse a la misma requiere que finalicen todas las
otras y por tanto dejen hueco. Hasta que no finalicen las 4 solicitudes no
podr´a accederse a la espera por una nueva actividad y se seguir´a ocupando
espacio en la cola de solicitudes.
Aspectos a tener en cuenta:
El personal de atenci´on a solicitudes tiene un identificador ´unico (atendedor 1, atendedor 2, atendedor 3).
Solo cuando una solicitud haya sido atendida podra pasar a asociarse a
una actividad cultural.
Las solicitudes comprueban cada 3 segundos si han sido descartadas seg´un
alguna de las posibles cas´uisticas y en caso de ser as´ı abandonan la cola.
Cada vez que se atienden 5 solicitudes, el atendedor descansa 10 segundos,
con lo que nadie puede acceder al puesto hasta que vuelve de su descanso.
Las solicitudes de cada tipo deben atenderse por orden de llegada.
Si el atendedor de invitaciones o el de qrs no tiene solicitudes que atender
puede tratar una del otro tipo, pero una vez termine de hacerlo deber´a
comprobar de nuevo sus invitaciones y ver si hay alguna de las que le
corresponde atender.
El atendedor pro no dicriminar´ıa si hay m´as de uno u otro y siempre usa
el criterio de antig¨uedad.
2
De las solicitudes a atender, el 70 % tiene todo en regla, el 20 % tiene
errores en los datos personales y el 10 % tiene vinculada una ficha con
antecedentes policiales. Esto tiene una implicaci´on en los tiempos de atenci´on:
• 70 % atenci´on correcta – En estos casos, el tiempo de espera est´a
entre 1 y 4 segundos y despu´es se puede solicitar o no participar en
una actividad cultural.
• 20 % errores en datos personales – En estos casos, el tiempo de espera
est´a entre 2 y 6 segundos y despu´es se puede solicitar o no participar
en una actividad cultural.
• 10 % antecedentes – En estos casos, el tiempo de espera est´a entre
6 y 10 segundos y no puede unirse a una actividad cultural, luego
despu´es del tiempo de espera debe dejar su sitio en la cola.
Cuando la atenci´on ha sido correcta o el error era solo en los datos personales, la solicitud podr´a asociarse o no a una actividad cultural (50 % de
probabilidad). Si no se asocia a la actividad la solicitud deja el sistema,
en caso de querer asociarse se libera el hueco en la cola de atenci´on y se
ocupa uno en la cola de la actividad cultural (si esto es posible).
Toda la actividad quedar´a registrada en un fichero plano de texto llamado
registroTiempos.log. En concreto, es necesario registrar al menos:
Cada vez que una solicitud accede al sistema
Cada vez que una solicitud deja el sistema por no ser fiable o por cansarse
de esperar a ser atendida.
Cada vez que una solicitud termina correctamente cu´anto ha tardado.
Cada vez que es atendida una solicitud con los datos personales incorrectos
y cuanto ha tardado.
Cada vez que una solicitud ha abandonado el sistema por antecedentes.
Cuando una solicitud solicita la vinculaci´on a una actividad cultural.
Cuando una solicitud se vincula a la actividad cultural.
Cuando una solicitud finaliza una actividad cultural.
Se registra el inicio y final del descanso de cada atendedor.
Al finalizar el programa se debe terminar de atender todas las solicitudes
pendientes pero ya no podr´an vincularse a una actividad cultural.
Consideraciones pr´acticas:
Simularemos el inicio de funcionamiento del sistema mediante se˜nales. En
caso de que una solicitud por invitaci´on quiera acceder al sistema se usar´a
la se˜nal SIGUSR1 y en el caso de ser por QR se enviar´a la se˜nal SIGUSR2.
Cada vez que se le env´ıe la se˜nal, supone que ha accedido una solicitud
nueva al sistema.
3
Es obligatorio el uso de mensajes que se escribir´an en un log y se mostrar´an
por pantalla. El formato de tales mensajes ser´a:
[YYYY-MM-DD HH:MI:SS] identificador: mensaje
Donde identificador puede ser el identificador del usuario, el identificador del facturador y mensaje es una breve descripci´on del evento ocurrido.
Las entradas del log deben quedar escritas en orden cronol´ogico.
El programa finaliza cuando recibe la se˜nal SIGINT y deber´a hacerlo correctamente.
Partes opcionales (20 % de la nota)
Asignaci´on est´atica de recursos (10 %):
• Modifica el programa para que el n´umero de solicitudes que pueden
tratarse en el sistema sea un par´ametro que reciba el programa al ser
ejecutado desde la l´ınea de comandos.
• Modifica el programa para que el n´umero de atendedores que atienden
cualquier tipo de solicitud sea un par´ametro que reciba el programa
al ser ejecutado desde la l´ınea de comandos.
Asignaci´on din´amica de recursos I (5 %):
• Modifica el programa para que el n´umero solicitudes que pueden
acceder al sistema pueda modificarse en tiempo de ejecuci´on.
• Solamente es necesario contemplar un incremento en solicitudes. No
es necesario contemplar la reducci´on.
• Cada vez que se cambie el n´umero de solicitudes tiene que reflejarse
en el log.
Asignaci´on din´amica de recursos II (5 %):
• Modifica el programa para que el n´umero de atendedores se pueda
modificar en tiempo de ejecuci´on.
• Solamente es necesario contemplar un incremento en el n´umero de
atendedores. No es necesario contemplar la reducci´on.
• Cada vez que se produce un cambio en este sentido debe quedar
reflejado en el log.
