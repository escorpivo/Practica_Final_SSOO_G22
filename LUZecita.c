    #include <stdio.h>
    #include <stdlib.h>
    #include <ctype.h>
    #include <time.h>
    #include <pthread.h>
    #include <signal.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
    #include <errno.h>
    #include <string.h>
    #include <stdbool.h>


    //Atributos Cliente
    struct Cliente{
        //Identificador
        int id;
        //Tipo de cliente(0 es de red, 1 es de App)
        int tipo;
        //Estado en el que está (0 = esperando, 1 = siendo atendido, 2 = acabado de ser atendido, 3 = se ha ido)
        int estadoAtendido;
        // Prioridad
        int prioridad;
    };

    //Atributos Personal
    struct Personal {
        //Identificador
        int id;
        //Tipo de personal(0 = Tecnicos, 1 = Responsables, 2 = Encargado)
        int tipo;
        //Estado en el que está (0 = libre, 1 = ocupado)
        int ocupado;
    };

    //Comprueba si la ejecucion está acabada(0 = lo está, 1 = no lo está)
    int ejecucionAcabada;
    //Numero total de clientes
    int numClientes;
    //Contador de clientes en cola o siendo atendidos
    int contadorClientesActuales;
    //Clientes atendidos para comprobar si el tecnico descansa(Cada 5 clientes atendidos el técnico descansa 5 segundos)
    int clientesAtendidosTecnico;
    //Clientes atendidos para comprobar si el responsables descansa(Cada 6 clientes atendidos el responsable descansa 5 segundos)
    int clientesAtendidosResponsable;
    //Solicitudes al tecnico(necesita 4 solicitudes para viajar)
    int solicitudesTecnico;

    int numMaxClientes;

    //Mutex globales
    pthread_mutex_t finalizar;
    pthread_mutex_t colaClientes;
    pthread_mutex_t tocarFicheros;
    pthread_mutex_t salirDeCola;
    pthread_mutex_t auxM;
    pthread_mutex_t auxV;
    pthread_mutex_t elegirClientesTecnico;
    pthread_mutex_t crearRandom;
    pthread_mutex_t clienteResponsable;
    pthread_mutex_t elegirClientesEncargado;


    //Nombre del fichero log donde se guarda la ejecución del programa
    char logFileName[20]; 
    FILE* logFile;


    //Lista de clientes
    struct Cliente* clientes;

    //Declaro los metodos
    void entraCliente(int signal);
    void writeLogMessage(char* id, char* msg);
    void* acciones(void* arg);
    void fin();
    void clientesSalenDeCola(int cantidad);
    void mostrarCola();
    int aleatorios(int min, int max);
    int sizeClientes();
    int clientesEnCola();
    void salirDeColaCliente(int idCliente);
    int idContenido(int tecnico[], int idPosible);
    int buscarClientesParaTecnico(int tecnico[]);
    int buscarMaxPrioridad(int tecnico[], int index, int tipo);
    void devolverACola(int tecnico[]);
    int contarClientesAppEnEspera();
    int contarClientesRedEnEspera();


    //HANDLERS
    void* accionesEncargado(void* arg);
    void* accionesResponsable(void* arg);
    void* accionesTecnico(void* arg);
    void* accionesCliente(void* arg);

    int main(int argc, char* argv[]){

        srand(getpid());
        printf("Rand1: %d / Rand 2: %d\n", rand() % 100, rand() % 100);

        printf("Este es el pid del proceso: %d\n", getpid());

        sprintf(logFileName,"registroTiempos.log");

        printf("Bienvenido al servicio al cliente de lucezita\n");

        ejecucionAcabada = 0;
        numClientes = 0;
        contadorClientesActuales = 0;
        numMaxClientes = 20;

        if (pthread_mutex_init(&finalizar, NULL)!=0) exit(-1); 
        if (pthread_mutex_init(&colaClientes, NULL)!=0) exit(-1); 
        if (pthread_mutex_init(&tocarFicheros, NULL)!=0) exit(-1); 
        if (pthread_mutex_init(&salirDeCola, NULL)!=0) exit(-1); 
        if (pthread_mutex_init(&auxM, NULL)!=0) exit(-1); 
        if (pthread_mutex_init(&auxV, NULL)!=0) exit(-1); 
        if (pthread_mutex_init(&elegirClientesTecnico, NULL)!=0) exit(-1);
        if (pthread_mutex_init(&elegirClientesEncargado, NULL)!=0) exit(-1);
        if (pthread_mutex_init(&crearRandom, NULL)!=0) exit(-1);  
        if (pthread_mutex_init(&clienteResponsable, NULL)!=0) exit(-1);  


        clientes = (struct Cliente*)malloc(sizeof(struct Cliente) * numMaxClientes);

        for (int i = 0; i < numMaxClientes; i++) {
            clientes[i].id = 0;
            clientes[i].tipo = 0;
            clientes[i].estadoAtendido = 0;
            clientes[i].prioridad = 0;
        }

        //Clientes de Red (SIGUSR1)
        struct sigaction ClientesRed;
        ClientesRed.sa_handler = entraCliente;
        ClientesRed.sa_flags = 0;
        sigemptyset(&ClientesRed.sa_mask);
      
        if (sigaction(SIGUSR1, &ClientesRed, NULL) == -1) {
            writeLogMessage("FicheroLog", "La señal de los clientes de red no se ha montado");
            perror("La señal de los clientes de red no se ha montado");
            return 1;
        } else {
            writeLogMessage("FicheroLog", "Se ha montado la señal de los clientes de red");
        }

        //Cliente app (SIGUSR2)
        struct sigaction ClientesApp;
        ClientesApp.sa_handler = entraCliente;
        ClientesApp.sa_flags = 0;
        sigemptyset(&ClientesApp.sa_mask);
        if (sigaction(SIGUSR2, &ClientesApp, NULL)  ==  -1) {       
            writeLogMessage("FicheroLog", "La señal de los clientes de app no se ha montado");
            perror("La señal de los clientes de app no se ha montado");
            return 1;
        } else {
            writeLogMessage("FicheroLog", "Se ha montado la señal de los clientes de app");
        }

        //Señal para terminar (SIGINT)
        struct sigaction cancelar;
        cancelar.sa_handler = fin;
        cancelar.sa_flags = 0;
        sigemptyset(&cancelar.sa_mask);
        if (-1 == sigaction(SIGINT, &cancelar, NULL)) {
        
            writeLogMessage("FicheroLog", "Fallo montando la señal de terminar");
            perror("Fallo montando la señal de terminar");
            return 1;
        } else {
            writeLogMessage("FicheroLog", "Se ha montado la señal de terminar");
        }

        //Crear encargado
        pthread_t encargado;
        int encargadoArr[] = {0};
        pthread_create(&encargado, NULL, accionesEncargado, &encargadoArr);

        //Crear responsables
        pthread_t responsable1,responsable2;
        int responsable1Arr[] = {1/*id*/,0/*numero de personas atendidas*/,0 /*Numero de personas atendidas para comprobar descansos*/};
        pthread_create(&responsable1, NULL, accionesResponsable, &responsable1Arr);

        int responsable2Arr[] = {2/*id*/,0/*numero de personas atendidas*/,0 /*Numero de personas atendidas para comprobar descansos*/};
        pthread_create(&responsable2, NULL, accionesResponsable, &responsable2Arr);

        //Crear tecnico
        pthread_t tecnico1,tecnico2;
        int tecnico1Arr[] = {1/*id*/,0/*numero de personas que hay que atender*/, 0, 0, 0, 0, /*Numero de personas atendidas para comprobar descansos*/0};
        pthread_create(&tecnico1, NULL, accionesTecnico, &tecnico1Arr);

        int tecnico2Arr[] = {2/*id*/,0/*numero de personas que hay que atender*/, 0, 0, 0, 0, /*Numero de personas atendidas para comprobar descansos*/0};
        pthread_create(&tecnico2, NULL, accionesTecnico, &tecnico2Arr);

       

        //Dejo el programa que recibe las señales en pausa
        while (!ejecucionAcabada) {
            pause();
        }

        pthread_join(encargado, NULL);
        pthread_join(responsable1, NULL);
        pthread_join(responsable2, NULL);
        pthread_join(tecnico1, NULL);
        pthread_join(tecnico2, NULL);


    }

    //Metodo usado para crear un cliente en funcion de la señal que se reciba
    void entraCliente(int signal){

        srand(numClientes);

        int final = 0;

        //Usamos un mutex para que no puedan acceder a la vez
        pthread_mutex_lock(&finalizar);
        final = ejecucionAcabada;
        //Si hay mas de 20 clientes en el sistema. se ignoran las solicitudes que llegan
        if(contadorClientesActuales >= 20){
            sleep(0.5);
            //Lo escribo en el log
            writeLogMessage("FicheroLog", "Contador actual de clientes es mayor que 20");
            writeLogMessage("Main:","hay 20 clientes en cola\n");
            return;
        }
        pthread_mutex_unlock(&finalizar);

        if (final == 0){
            int numeroDelCliente = -1;
            
            // Se bloquea el mutex para que se administren de uno en uno
            pthread_mutex_lock(&colaClientes);
            
            // Asignacion de id en funcion de la posicion que ocupen en la lista
            for (int i = 0; i < numMaxClientes && numeroDelCliente == -1; i++) {
                if (clientes[i].id == 0) {
                    numeroDelCliente = i;
                }
            }
        
            if (numeroDelCliente == -1){
                return;
            } else {
                //Sumo uno al contador actual y al total de clientes que han pasado por el sistema
                contadorClientesActuales++;
                numClientes++;

                //El id es unico
                clientes[numeroDelCliente].id = numClientes;

                //Le atribuyo un valor aleatorio como prioridad entre 1 y 10
                int randomPrio = aleatorios(1,10);
                clientes[numeroDelCliente].prioridad = randomPrio;       

                //Defino su estado como en espera
                clientes[numeroDelCliente].estadoAtendido = 0;           

                //Creo un array para pasarselo por parametro al thread, que contiene el id y el tipo
                int clienteAux[] = {numeroDelCliente,0};

                //Diferenciacion del tipo de cliente
                if (signal == SIGUSR2){
                    //Cliente de red
                    clientes[numeroDelCliente].tipo = 0;
                } else if (signal == SIGUSR1){
                    //Cliente de app
                    clientes[numeroDelCliente].tipo = 1;
                    clienteAux[1] = 1;
                }

                //Creo el hilo
                pthread_t aux;
                pthread_create(&aux, NULL, accionesCliente, &clienteAux);
            }
            //Desbloqueo el mutex
            pthread_mutex_unlock(&colaClientes);
        }  
    }


    

void* accionesCliente(void* arg) {

    srand(getpid());

    //Creo una variable para usarla para los logs
    char mensaje[75];

    int idCliente = ((int*)arg)[0];   
 
    char identificador[20];

    pthread_mutex_lock(&colaClientes);
    int id = clientes[idCliente].id;
    pthread_mutex_unlock(&colaClientes);

    if (((int*)arg)[1] == 1){
        
        sprintf(identificador, "Cliente %d (de App)", id);
        sprintf(mensaje, "acabo de entar al sistema y estoy en espera de ser atendido");

    } else if (((int*)arg)[1] == 0){

        sprintf(identificador, "Cliente %d (de Red)", id);        
        sprintf(mensaje, "acabo de entar al sistema y estoy en espera de ser atendido");
    }

    // Lock del mutex para que el writeMessage vaya uno a uno al log
    pthread_mutex_lock(&tocarFicheros);
    writeLogMessage(identificador, mensaje);
    // Unlock del mutex
    pthread_mutex_unlock(&tocarFicheros);

    while(clientes[idCliente].estadoAtendido == 0){
        
        //Genero un random para comprobar si se van por alguna causa
        int random = aleatorios(0,100);        

        if (random <= 20){
            //Sale de la cola
            salirDeColaCliente(id);
            sprintf(mensaje, "Me estoy cansando de esperar");

            pthread_mutex_lock(&tocarFicheros);
            writeLogMessage(identificador, mensaje);
            pthread_mutex_unlock(&tocarFicheros);

            //Duerme durante 8 segundos
            sleep(8);

            sprintf(mensaje, "Me he cansando de esperar y me he ido");

            pthread_mutex_lock(&tocarFicheros);
            writeLogMessage(identificador, mensaje);
            pthread_mutex_unlock(&tocarFicheros);

        } else if (random <= 30){
            salirDeColaCliente(id);

            sprintf(mensaje, "Encuentro la aplicacion dificil, asi que me voy");

            pthread_mutex_lock(&tocarFicheros);
            writeLogMessage(identificador, mensaje);
            pthread_mutex_unlock(&tocarFicheros);

        } else {
            int random2 = aleatorios(0,100);
            if (random2 <= 5){
            salirDeColaCliente(id);

                sprintf(mensaje, "Perdí la conexión y me he ido");

                pthread_mutex_lock(&tocarFicheros);
                writeLogMessage(identificador, mensaje);
                pthread_mutex_unlock(&tocarFicheros);

        } else {
                if (((int*)arg)[1] == 0){
                    int random3 = aleatorios(0,100);
                        if (random <=30){
                            sprintf(mensaje, "Estoy esperando por un tecnico a domicilio");

                            pthread_mutex_lock(&tocarFicheros);
                            writeLogMessage(identificador, mensaje);
                            pthread_mutex_unlock(&tocarFicheros);

                            pause();
                        }
                }

                sprintf(mensaje, "Sigo esperando en la cola");

                pthread_mutex_lock(&tocarFicheros);
                writeLogMessage(identificador, mensaje);
                pthread_mutex_unlock(&tocarFicheros);
            }
        }

        sleep(2);

        if(clientes[idCliente-1].estadoAtendido > 0 && clientes[idCliente-1].estadoAtendido != 3){
            pause();
        }

        
    }

}

void* accionesTecnico(void* arg) {


    int* clientela = ((int*)arg);

    char identificador[20];

    //Creo una variable para usarla para los logs
    char mensaje[75];

    int id = ((int*)arg)[0];

    sprintf(identificador, "Tecnico%d", id);

    while(ejecucionAcabada == 0 ){   

        if (clientela[1] <= 4 && clientela[1] > 0){

            for(int i = 2; i < 6;i++){
                if (clientela[i] != 0 ){
                    sprintf(mensaje, "Estoy atendiendo al cliente %d", clientela[i]);

                    pthread_mutex_lock(&tocarFicheros);
                    writeLogMessage(identificador, mensaje);
                    pthread_mutex_unlock(&tocarFicheros);
                    
                    sleep(1);
                    clientela[6]++;
                    contadorClientesActuales--;

                    clientela[1]--;

                    sprintf(mensaje, "Ya acabe con el cliente %d y me queda por atender %d clientes en este viaje", clientela[i], clientela[1]);

                    pthread_mutex_lock(&tocarFicheros);
                    writeLogMessage(identificador, mensaje);
                    pthread_mutex_unlock(&tocarFicheros);

                    clientes[clientela[i]].estadoAtendido = 2;

                    if (clientela[6] == 5){

                        sprintf(mensaje, "Estoy descansando");
//                        devolverACola(clientela);

                        pthread_mutex_lock(&tocarFicheros);
                        writeLogMessage(identificador, mensaje);
                        pthread_mutex_unlock(&tocarFicheros);

                        sleep(5);

                        sprintf(mensaje, "Ya he descansado");

                        pthread_mutex_lock(&tocarFicheros);
                        writeLogMessage(identificador, mensaje);
                        pthread_mutex_unlock(&tocarFicheros);

                        clientela[6] = 0;
                    }
                }             
            }

            clientela[2] = 0;            
            clientela[3] = 0;
            clientela[4] = 0;
            clientela[5] = 0;
        } else {
            pthread_mutex_lock(&elegirClientesTecnico);
            printf("Tecnico %d: Busco clientes\n", clientela[0]);
            buscarClientesParaTecnico(clientela);
            pthread_mutex_unlock(&elegirClientesTecnico);
            sleep(2);
        }
    }
}

void* accionesResponsable(void* arg) {

    int* clientela = ((int*)arg);
    char identificador[20];

    //Creo una variable para usarla para los logs
    char mensaje[75];

    int id = ((int*)arg)[0];

    sprintf(identificador, "Responsable%d", id);

    while(ejecucionAcabada == 0){

        if(numClientes > 0){

        int maxPrio = -5;
        int prioID = -5;  

        pthread_mutex_lock(&clienteResponsable);

        for (int i = 0; i < numClientes; i++){
            if (clientes[i].estadoAtendido == 0){
                if (clientes[i].prioridad > maxPrio){
                    maxPrio = clientes[i].prioridad;
                    if (0 == clientes[i].tipo){
                        prioID = clientes[i].id - 1;
                        clientela[2]++;

                        if (clientela[2] == 6){
                            sprintf(mensaje, "Estoy descansando");
                            
                            pthread_mutex_lock(&tocarFicheros);
                            writeLogMessage(identificador, mensaje);
                            pthread_mutex_unlock(&tocarFicheros);
                            
                            sleep(5);

                            sprintf(mensaje, "Ya he descansado");

                            pthread_mutex_lock(&tocarFicheros);
                            writeLogMessage(identificador, mensaje);
                            pthread_mutex_unlock(&tocarFicheros);                        
                        }
                    }            
                }
            }
        }
        pthread_mutex_unlock(&clienteResponsable);

        sprintf(mensaje, "%d prioID", prioID);
                            
        pthread_mutex_lock(&tocarFicheros);
        writeLogMessage(identificador, mensaje);
        pthread_mutex_unlock(&tocarFicheros);

        pthread_mutex_lock(&clienteResponsable);
        if (clientes[prioID].id != 0 && contarClientesRedEnEspera > 0){

            clientes[prioID].estadoAtendido = 1;

            int random = aleatorios(0,100);

                if(random <= 10){
                    int randomConfusion = aleatorios(1,2);

                    sprintf(mensaje, "Cliente %d se confundió de compañia, tardo %d segundos en procesarlo",clientes[prioID].id , randomConfusion);

                    pthread_mutex_lock(&tocarFicheros);
                    writeLogMessage(identificador, mensaje);
                    pthread_mutex_unlock(&tocarFicheros);

                    pthread_mutex_unlock(&clienteResponsable);
                    sleep(randomConfusion);
                    clientes[prioID].estadoAtendido = 1;

                } else if(random <= 20){
                    int randomIdentificado = aleatorios(2,6);

                    sprintf(mensaje, "Cliente %d está mal identificado, tardo %d segundos en procesarlo",clientes[prioID].id, randomIdentificado );

                    pthread_mutex_lock(&tocarFicheros);
                    writeLogMessage(identificador, mensaje);
                    pthread_mutex_unlock(&tocarFicheros);

                    pthread_mutex_unlock(&clienteResponsable);
                    sleep(randomIdentificado);

                } else {
                    int randomTodoBien = aleatorios(1,4);

                    sprintf(mensaje, "Cliente %d tiene todo en orden, tardo %d segundos en procesarlo",clientes[prioID].id , randomTodoBien);

                    pthread_mutex_lock(&tocarFicheros);
                    writeLogMessage(identificador, mensaje);
                    pthread_mutex_unlock(&tocarFicheros);

                    pthread_mutex_unlock(&clienteResponsable);
                    sleep(randomTodoBien);
                }
        }
        }
    }
}

void* accionesEncargado(void* arg) {

    //AL ENCARGADO LO LANZO DESDE EL PPIO, Y TENGO QUE REVISAR CND UN TECNICO O UN RESPONSABLE DEVUELVEN CLIENTES A COLA.

    //atiendo a todos
    //tengo prioridad por los de red
    //actuo cuando tenicos o encargados descansan

    int* clientela = ((int*)arg);

    char identificador[20];

    //Creo una variable para usarla para los logs
    char mensaje[75];

    sprintf(identificador,"Encargado");

    printf("soy el Encargado, voy a cubrir huecos de tecnicos y Responsables\n");

    int maxPrio = -5;
    int prioID = -5;  

    while(ejecucionAcabada == 0){

        if(clientela[1] > 0 ){

            for (int i = 0; i < numClientes; i++){
                srand(clientela[1]);
                int aleat = aleatorios(1,100);

                if (clientes[i].estadoAtendido == 0){
                    if (contarClientesRedEnEspera() > 0){
                        if(clientes[i].tipo == 0){
                            if (clientes[i].prioridad > maxPrio){
                                //falta calcular tiempos de atencion
                                if(aleat <= 80){
                                    sprintf(mensaje, "el cliente %d, tiene todo en regla", clientes[i].id);
                                    pthread_mutex_lock(&tocarFicheros);
                                    writeLogMessage(identificador, mensaje);
                                    pthread_mutex_unlock(&tocarFicheros);

                                    sleep(aleatorios(1,4));
                                    if(aleatorios(1,2) == 2){
                                        maxPrio = clientes[i].prioridad;
                                        prioID = clientes[i].id - 1;
                                        clientela[2]++;
                                        clientes[i].estadoAtendido = 1;
                                        sprintf(mensaje, "Cliente de red numero %d atendido", clientes[i].id );
                                        pthread_mutex_lock(&tocarFicheros);
                                        writeLogMessage(identificador, mensaje);
                                        pthread_mutex_unlock(&tocarFicheros);

                                        clientes[i].estadoAtendido = 2;
                                    }else{
                                        sprintf(mensaje,"La atencion no se ha podido realizar");
                                        pthread_mutex_lock(&tocarFicheros);
                                        writeLogMessage(identificador, mensaje);
                                        pthread_mutex_unlock(&tocarFicheros);
                                    }
                                }else if(aleat > 80 && aleat <= 90){
                                    sleep(aleatorios(2,6));
                                    sprintf(mensaje, "cliente mal identificado");
                                    pthread_mutex_lock(&tocarFicheros);
                                    writeLogMessage(identificador, mensaje);
                                    pthread_mutex_unlock(&tocarFicheros);
                                    if(aleatorios(1,2) == 2){
                                        maxPrio = clientes[i].prioridad;
                                        prioID = clientes[i].id - 1;
                                        clientela[2]++;
                                        clientes[i].estadoAtendido = 1;
                                        sprintf(mensaje, "Cliente de red numero %d atendido", clientes[i].id );
                                        pthread_mutex_lock(&tocarFicheros);
                                        writeLogMessage(identificador, mensaje);
                                        pthread_mutex_unlock(&tocarFicheros);
                                        clientes[i].estadoAtendido = 2;
                                    }else{
                                        sprintf(mensaje,"La atencion no se ha podido realizar");
                                        pthread_mutex_lock(&tocarFicheros);
                                        writeLogMessage(identificador, mensaje);
                                        pthread_mutex_unlock(&tocarFicheros);
                                    }
                                }else{
                                    sleep(aleatorios(1,2));
                                    sprintf(mensaje,"el cliente ha abandonado el sistema");
                                    pthread_mutex_lock(&tocarFicheros);
                                    writeLogMessage(identificador, mensaje);
                                    pthread_mutex_unlock(&tocarFicheros);
                                }
                            }
                        }
                    } else {
                        if(clientes[i].tipo == 1){

                            //falta calcular tiempos de atencion
                    
                            if (clientes[i].prioridad > maxPrio){
                                //falta calcular tiempos de atencion
                                if(aleat <= 80){
                                    sprintf(mensaje, "el cliente %d, tiene todo en regla", clientes[i].id);
                                    pthread_mutex_lock(&tocarFicheros);
                                    writeLogMessage(identificador, mensaje);
                                    pthread_mutex_unlock(&tocarFicheros);

                                    sleep(aleatorios(1,4));
                                    if(aleatorios(1,2) == 2){
                                        maxPrio = clientes[i].prioridad;
                                        prioID = clientes[i].id - 1;
                                        clientela[2]++;
                                        clientes[i].estadoAtendido = 1;
                                        sprintf(mensaje, "Cliente de red numero %d atendido", clientes[i].id );
                                        pthread_mutex_lock(&tocarFicheros);
                                        writeLogMessage(identificador, mensaje);
                                        pthread_mutex_unlock(&tocarFicheros);

                                        clientes[i].estadoAtendido = 2;
                                    }else{
                                        sprintf(mensaje,"La atencion no se ha podido realizar");
                                        pthread_mutex_lock(&tocarFicheros);
                                        writeLogMessage(identificador, mensaje);
                                        pthread_mutex_unlock(&tocarFicheros);
                                    }
                                }else if(aleat > 80 && aleat <= 90){
                                    sleep(aleatorios(2,6));
                                    sprintf(mensaje, "cliente mal identificado");
                                    pthread_mutex_lock(&tocarFicheros);
                                    writeLogMessage(identificador, mensaje);
                                    pthread_mutex_unlock(&tocarFicheros);
                                    if(aleatorios(1,2) == 2){
                                        maxPrio = clientes[i].prioridad;
                                        prioID = clientes[i].id - 1;
                                        clientela[2]++;
                                        clientes[i].estadoAtendido = 1;
                                        sprintf(mensaje, "Cliente de red numero %d atendido", clientes[i].id );
                                        pthread_mutex_lock(&tocarFicheros);
                                        writeLogMessage(identificador, mensaje);
                                        pthread_mutex_unlock(&tocarFicheros);
                                        clientes[i].estadoAtendido = 2;
                                    }else{
                                        sprintf(mensaje,"La atencion no se ha podido realizar");
                                        pthread_mutex_lock(&tocarFicheros);
                                        writeLogMessage(identificador, mensaje);
                                        pthread_mutex_unlock(&tocarFicheros);
                                    }
                                }else{
                                    sleep(aleatorios(1,2));
                                    sprintf(mensaje,"el cliente ha abandonado el sistema");
                                    pthread_mutex_lock(&tocarFicheros);
                                    writeLogMessage(identificador, mensaje);
                                    pthread_mutex_unlock(&tocarFicheros);
                                }
                            }
                        }
                    }
                }
            }

            if(clientes[prioID].estadoAtendido == 2){
                pthread_mutex_lock(&tocarFicheros);
                writeLogMessage(identificador, mensaje);
                pthread_mutex_unlock(&tocarFicheros);

                salirDeColaCliente(prioID);
            }
        }else{
            pthread_mutex_lock(&elegirClientesEncargado);
            printf("Encargado: Busco clientes\n");
            accionesEncargado(clientela);
            pthread_mutex_unlock(&elegirClientesEncargado);
            sleep(3);
        }

    }

}

void fin() {
    pthread_mutex_lock(&finalizar);
    ejecucionAcabada = 1;
    pthread_mutex_unlock(&finalizar);
}


void salirDeColaCliente(int idCliente){
    clientes[idCliente].estadoAtendido = 3;
//   clientes[idCliente].prioridad = -5;
}

void mostrarCola(){
    char mensaje[200];

    for(int i = 0; i < numClientes; i++){
        sprintf(mensaje,"\nCliente %d: con estado atendido %d de tipo %d con prioridad %d\n\n", clientes[i].id, clientes[i].estadoAtendido, clientes[i].tipo, clientes[i].prioridad);
        pthread_mutex_lock(&tocarFicheros);
        writeLogMessage("Cola", mensaje);
        pthread_mutex_unlock(&tocarFicheros);
    }
}

int sizeClientes() {
    int total = 0;
    pthread_mutex_lock(&colaClientes);
    for (int i = 0; i < numClientes; i++) {
        if (clientes[i].id != 0) {
            total++;
        }
    }
    pthread_mutex_unlock(&colaClientes);
    return total;
}

int clientesEnCola() {
    int total = 0;
    pthread_mutex_lock(&colaClientes);
    for (int i = 0; i < numClientes; i++) {
        if (clientes[i].estadoAtendido == 0) {
            total++;
        }
    }
    pthread_mutex_unlock(&colaClientes);
    return total;
}

int aleatorios(int min, int max) { // Función para calcular numeros aleatorios

    return rand() % (max - min + 1) + min;

}

int contarClientesAppEnEspera(){
    int total = 0;
    for(int i = 0; i < numClientes;i++ ){
        
        if (clientes[i].tipo == 1 && (clientes[i].estadoAtendido == 0)){
            total++;
        }
    }

    return total;
}

int contarClientesRedEnEspera(){
    int total = 0;
    for(int i = 0; i < numClientes; i++){
        
        if (clientes[i].tipo == 0 && (clientes[i].estadoAtendido == 0)){
            total++;
        }
    }
    return total;
}

int buscarClientesParaTecnico(int tecnico[]){

    char mensaje[200];

    int total = 0;

    for(int i = 2; i < 6; i++){

        buscarMaxPrioridad(tecnico, i , 1);
        
    }

    sprintf(mensaje,"Id: %d , Personas atendidas %d, IdCliente1 %d con prioridad %d,IdCliente2 %d con prioridad %d,IdCliente3 %d con prioridad %d, IdCliente4 %d con prioridad %d\n",
    tecnico[0],tecnico[1], tecnico[2] + 1,clientes[tecnico[2]].prioridad, tecnico[3] + 1,clientes[tecnico[3]].prioridad, tecnico[4] + 1,clientes[tecnico[4]].prioridad, tecnico[5] + 1,clientes[tecnico[5]].prioridad);

    return total;
}

void buscarClientesParaEncargado(int encargado[]){

    int maxPrio = -5;
    int prioID = -5; 

    if (contarClientesRedEnEspera() > 0){
        for (int i = 0; i < numClientes; i++){
            if (0 == clientes[i].tipo){
                if (clientes[i].estadoAtendido == 0){
                    maxPrio = clientes[i].prioridad;
                    prioID = clientes[i].id - 1;
                    if (clientes[i].prioridad > maxPrio){
                        encargado[0] = prioID;
                    }
                }
            }
        }
    } else {
        for (int i = 0; i < numClientes; i++){
            if (0 == clientes[i].tipo){
                if (clientes[i].estadoAtendido == 0){
                    maxPrio = clientes[i].prioridad;
                    prioID = clientes[i].id - 1;
                    if (clientes[i].prioridad > maxPrio){
                        encargado[0] = prioID;
                    }
                }
            }
        }
    }

}



int buscarMaxPrioridad(int tecnico[], int index, int tipo){
    int maxPrio = -5;
    int prioID = -5;  

    for (int i = 0; i < numClientes; i++){
        if (tipo == clientes[i].tipo){
            if (clientes[i].estadoAtendido == 0){
                 if (clientes[i].prioridad > maxPrio){
                    maxPrio = clientes[i].prioridad;
                    prioID = clientes[i].id - 1;
                    if(idContenido(tecnico,prioID) == 0){
                        tecnico[index] = prioID;
                    }
                }            
            }
        }
    }

    if (prioID >= 0){
        clientes[prioID].estadoAtendido = 1;
    }
    
    if (maxPrio != -5){
        tecnico[1]++;
    } 

}

int idContenido(int tecnico[], int idPosible){
    for (int i = 2; i < 6 ; i++){
        if(idPosible == tecnico[i]){
            return 1;
        }
    }
    return 0;
}

void writeLogMessage ( char * id , char * msg ) {
        // Calculamos la hora actual
        time_t now = time (0) ;
        struct tm * tlocal = localtime (& now );
        char stnow [25];
        strftime ( stnow , 25 , " %d/ %m/ %y %H: %M: %S", tlocal );

        // Escribimos en el log
        logFile = fopen ( logFileName , "a");
        fprintf ( logFile , "[ %s] %s: %s\n", stnow , id , msg ) ;
        fclose ( logFile );
}

void devolverACola(int tecnico[]){

    for (int i = 2; i < 6; i++){
        if (clientes[tecnico[i]].estadoAtendido == 1){
            clientes[tecnico[i]].estadoAtendido = 0;
        }
    }

}

