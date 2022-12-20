/*
-anotaciones de cara al codigo-

-2 Hilos tecnico, 2 Hilos responsable de reparaciones y 1 Hilo encargado.

-solamente van a poder atenderse a la vez 20 peticiones cualquier otra que llegue a mayores
si no hay hueco sera ignorada.

-Tecnicos atienden clientes con problemas en APP, Responsables de reparaciones se encargan
de clientes con problemas en la red 

    SE DA PRIORIDAD A PROBLEMAS EN RED

-Toda la actividad quedar´a registrada en un fichero plano de texto llamado
registroTiempos.log  
El log y variables globales sera prudente reiniciarlo en cada ejecucion del codigo, nunca en mitad de ejecucion.

Se pueden usar linkedList pero habria que implementarlas (mejor evitarlas)

Si veis algo mas que falte, ponedlo/modificarlo
    -Victor
*/