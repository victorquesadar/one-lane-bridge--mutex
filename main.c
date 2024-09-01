#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#define MAX_AUTOS 10
#define TIEMPO_SEMAFORO 5 // Tiempo en segundos que dura cada luz del semáforo
#define NUM_AMBULANCIAS 2  // Número de ambulancias a crear


int vehiculos_en_cola = 0;
pthread_mutex_t mutex;
pthread_cond_t cond_puente_libre = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_semaforo;
pthread_cond_t cond_oficial;
int autos_pasados_este, autos_pasados_oeste;
pthread_cond_t cond_nuevo_auto; // Nueva variable de condición
int autos_terminados = 0; // Variable para controlar cuando todos los autos han terminado
int semaforo_este = 0;   // 0: Rojo, 1: Verde
int semaphore_active = 1;
int semaforo_oeste = 0;  // 0: Rojo, 1: Verde

int puente_libre = 1; // Variable para controlar si el puente está libre o no

int ambulancias_esperando = 0;
// Parámetros leídos desde el archivo de configuración
int longitud_puente;
double media_este, media_oeste;
double rango_velocidad_este_inferior, rango_velocidad_este_superior;
double rango_velocidad_oeste_inferior, rango_velocidad_oeste_superior;
int K1, K2;
int tiempo_semaforo_este, tiempo_semaforo_oeste;
int porcentaje_ambulancias;

pthread_cond_t cond_este = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_oeste = PTHREAD_COND_INITIALIZER;

#define MAX_AUTOS 10


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_oficial = PTHREAD_COND_INITIALIZER;
int autos_pasados_en_sentido_actual = 0;
int direccionPuente = 0; // 0: Este a Oeste, 1: Oeste a Este
int K1;
int K2;

typedef struct {
    int id;
    int oesteEsteAuto; // 0: Este a Oeste, 1: Oeste a Este
    int esAmbulancia;
} InfoAuto;


void *_automovil_carnage(void *arg);
void *_automovil_semaforo(void *arg);
void *semaforo(void *arg);
void *oficial_transito(void *arg);
void *_ambulancia_oficial(void *arg);
void *_ambulancia_carnage(void *arg);
int todos_vehiculos_cruzaron();
double tiempo_exponencial(double media);
double calcular_tiempo_cruce(int longitud_puente, double velocidad);
double velocidad_aleatoria(double velocidad_min, double velocidad_max);


void leerConfiguracion(const char *archivo) {
    FILE *fp = fopen(archivo, "r");
    if (fp == NULL) {
        perror("Error al abrir el archivo");
        exit(EXIT_FAILURE);
    }

    char linea[256];
    while (fgets(linea, sizeof(linea), fp) != NULL) {
        sscanf(linea, "longitud_puente = %d", &longitud_puente);
        sscanf(linea, "media_este = %lf", &media_este);
        sscanf(linea, "media_oeste = %lf", &media_oeste);
        sscanf(linea, "rango_velocidad_este_inferior = %lf", &rango_velocidad_este_inferior);
        sscanf(linea, "rango_velocidad_este_superior = %lf", &rango_velocidad_este_superior);
        sscanf(linea, "rango_velocidad_oeste_inferior = %lf", &rango_velocidad_oeste_inferior);
        sscanf(linea, "rango_velocidad_oeste_superior = %lf", &rango_velocidad_oeste_superior);
        sscanf(linea, "K1 = %d", &K1);
        sscanf(linea, "K2 = %d", &K2);
        sscanf(linea, "tiempo_semaforo_este = %d", &tiempo_semaforo_este);
        sscanf(linea, "tiempo_semaforo_oeste = %d", &tiempo_semaforo_oeste);
        sscanf(linea, "porcentaje_ambulancias = %d", &porcentaje_ambulancias);
    }

    fclose(fp);
}


double tiempo_exponencial(double media) {
    double r = (double)rand() / (RAND_MAX + 1.0);1
    return -media * log(1.0 - r);
}


// Función para obtener una velocidad aleatoria dentro de un rango
double velocidad_aleatoria(double velocidad_min, double velocidad_max) {
    return velocidad_min + (velocidad_max - velocidad_min) * (rand() / (double)RAND_MAX);
}

// Simular el tiempo de cruce del puente basado en la velocidad
double calcular_tiempo_cruce(int longitud_puente, double velocidad) {
    double tiempo_original = (double)longitud_puente / velocidad;  // Tiempo en segundos
    double factor_de_escala = 0.1;  // Ajusta este valor para cambiar la rapidez del cruce
    return tiempo_original * factor_de_escala;
}




void *_ambulancia(void *arg) {
    int id = *((int *)arg);
    int mi_direccion;

    // Asegurar que haya ambulancias desde ambas direcciones
    if (id % 2 == 0) {
        mi_direccion = 0; // Este
    } else {
        mi_direccion = 1; // Oeste
    }

    usleep(rand() % 1000000); // Simula el tiempo de espera antes de llegar al puente

    pthread_mutex_lock(&mutex);
    printf("Ambulancia %d llega al puente desde el %s\n", id, mi_direccion ? "Oeste" : "Este");

    while (!puente_libre || vehiculos_en_cola > 0) {
        // La ambulancia espera hasta que el puente esté libre y no haya vehículos en cola
        printf("Ambulancia %d debe esperar a que el puente esté libre y sin vehículos en cola\n", id);
        pthread_cond_wait(&cond_puente_libre, &mutex);
    }

    // La ambulancia cruza el puente ya que está libre y no hay vehículos en cola
    printf("Ambulancia %d cruza el puente\n", id);
    puente_libre = 0; // Ocupar el puente
    pthread_mutex_unlock(&mutex);

    // Simular el tiempo de cruce del puente
    usleep(rand() % 1000000);

    pthread_mutex_lock(&mutex);
    puente_libre = 1; // Liberar el puente
    printf("Ambulancia %d ha cruzado el puente\n", id);
    pthread_cond_signal(&cond_puente_libre); // Señalar que el puente está libre
    pthread_mutex_unlock(&mutex);

    pthread_exit(NULL);
}

void *_ambulancia_semaforo(void *arg) {
    int id = *((int *)arg);
    int mi_direccion = (id % 2 == 0) ? 0 : 1; // Este para IDs pares, Oeste para impares

    usleep(rand() % 1000000); // Simula el tiempo de espera antes de llegar al puente

    pthread_mutex_lock(&mutex);
    printf("Ambulancia %d llega al puente desde el %s\n", id, mi_direccion ? "Oeste" : "Este");

    printf("Ambulancia %d puede cruzar el puente incluso con semáforo en Rojo.\n", id);

    double velocidad = 60; // Suponemos una velocidad fija para ambulancias
    double tiempo_cruce = calcular_tiempo_cruce(longitud_puente, velocidad); // tiempo en segundos

    printf("Ambulancia %d comienza a cruzar el puente desde el %s a %.2f km/h. Tiempo estimado: %.2f segundos\n", id, mi_direccion ? "Oeste" : "Este", velocidad, tiempo_cruce);

    pthread_mutex_unlock(&mutex);

    usleep(tiempo_cruce * 1000000); // Simular tiempo de cruce

    pthread_mutex_lock(&mutex);
    printf("Ambulancia %d ha cruzado el puente desde el %s. Tiempo real de cruce: %.2f segundos\n", id, mi_direccion ? "Oeste" : "Este", tiempo_cruce);
    pthread_mutex_unlock(&mutex);

    pthread_exit(NULL);
}



void *_ambulancia_carnage(void *arg) {
    int id = *((int *)arg);
    const char *lado = (id % 2 == 0) ? "Este" : "Oeste";

    usleep(rand() % 500000); // Espera aleatoria hasta 0.5 segundos

    pthread_mutex_lock(&mutex);
    printf("Ambulancia %d llega al puente desde el %s.\n", id, lado);

    while (!puente_libre) {
        printf("Ambulancia %d espera por el puente libre desde el %s.\n", id, lado);
        pthread_cond_wait(&cond_puente_libre, &mutex);
    }

    double velocidad = 60; // Suponemos una velocidad fija para ambulancias, ajustar según necesario
    double tiempo_cruce = calcular_tiempo_cruce(longitud_puente, velocidad); // tiempo en segundos

    printf("Ambulancia %d comienza a cruzar el puente desde el %s a %.2f km/h. Tiempo estimado: %.2f segundos\n", id, lado, velocidad, tiempo_cruce);

    puente_libre = 0;
    pthread_mutex_unlock(&mutex);

    usleep(tiempo_cruce * 1000000); // Simular tiempo de cruce

    pthread_mutex_lock(&mutex);
    puente_libre = 1; // Liberar el puente
    pthread_cond_signal(&cond_puente_libre); // Notificar a otros que el puente está libre
    printf("Ambulancia %d ha cruzado el puente desde el %s. Tiempo real de cruce: %.2f segundos\n", id, lado, tiempo_cruce);
    pthread_mutex_unlock(&mutex);

    pthread_exit(NULL);
}





void modo_carnage() {
    pthread_t autos[MAX_AUTOS];
    int ids[MAX_AUTOS];
    int i;

    for (i = 0; i < MAX_AUTOS; i++) {
        ids[i] = i;
        double espera = tiempo_exponencial((i % 2 == 0) ? media_este : media_oeste);
        usleep((unsigned int)(espera * 1000000));

        if (rand() % 100 < porcentaje_ambulancias) {
            pthread_create(&autos[i], NULL, _ambulancia_carnage, (void *)&ids[i]);
        } else {
            pthread_create(&autos[i], NULL, _automovil_carnage, (void *)&ids[i]);
        }
    }

    for (i = 0; i < MAX_AUTOS; i++) {
        pthread_join(autos[i], NULL);
    }

    printf("Todos los vehiculos han cruzado.\n");
}






void *_automovil_carnage(void *arg) {
    int id = *((int *)arg);
    int mi_direccion = (id % 2 == 0) ? 0 : 1; // Este a Oeste para pares, Oeste a Este para impares

    usleep(rand() % 1000000); // Simula el tiempo de espera antes de llegar al puente

    pthread_mutex_lock(&mutex);

    if ((mi_direccion == 0 && autos_pasados_este >= K1) ||
        (mi_direccion == 1 && autos_pasados_oeste >= K2)) {
        pthread_mutex_unlock(&mutex);
        pthread_exit(NULL);
    }

    printf("Automovil %d llega al puente desde el %s\n", id, mi_direccion ? "Oeste" : "Este");

    while (!puente_libre || ambulancias_esperando > 0) {
        printf("Automovil %d debe esperar porque el puente no está libre o hay ambulancias esperando.\n", id);
        pthread_cond_wait(&cond_puente_libre, &mutex);
    }

    double velocidad = velocidad_aleatoria((mi_direccion == 0) ? rango_velocidad_este_inferior : rango_velocidad_oeste_inferior,
                                           (mi_direccion == 0) ? rango_velocidad_este_superior : rango_velocidad_oeste_superior);
    double tiempo_cruce = calcular_tiempo_cruce(longitud_puente, velocidad); // tiempo en segundos

    if (mi_direccion == 0) {
        autos_pasados_este++;
    } else {
        autos_pasados_oeste++;
    }

    printf("Automovil %d comienza a cruzar el puente desde el %s a %.2f km/h. Tiempo estimado: %.2f segundos\n", id, mi_direccion ? "Oeste" : "Este", velocidad, tiempo_cruce);
    puente_libre = 0; // Ocupar el puente
    pthread_mutex_unlock(&mutex);
    usleep(tiempo_cruce * 1000000); // Simular el tiempo de cruce del puente usando el tiempo calculado

    pthread_mutex_lock(&mutex);
    puente_libre = 1; // Liberar el puente
    printf("Automovil %d ha cruzado el puente y lo ha liberado. Tiempo real de cruce: %.2f segundos\n", id, tiempo_cruce);
    pthread_cond_broadcast(&cond_puente_libre); // Notificar a otros que el puente está libre

    pthread_mutex_unlock(&mutex);

    pthread_exit(NULL);
}










void *oficial_transito(void *arg) {
    int sentido = *((int *)arg);
    int cantidad_pasos = (sentido == 0) ? K1 : K2; // Determinar la cantidad de autos según el sentido

    while (autos_terminados < MAX_AUTOS) {
        pthread_mutex_lock(&mutex);

        // Esperar si el sentido actual no coincide con el sentido del oficial
        while (direccionPuente != sentido) {
            pthread_cond_wait(&cond_oficial, &mutex);
        }

        printf("Oficial de transito permite pasar %d autos en sentido %s\n", cantidad_pasos, sentido ? "Oeste a Este" : "Este a Oeste");

        for (int i = 0; i < cantidad_pasos; i++) {
            double velocidad = velocidad_aleatoria(rango_velocidad_este_inferior, rango_velocidad_este_superior); // Asumiendo rangos generalizados
            double tiempo_cruce = calcular_tiempo_cruce(longitud_puente, velocidad);

            printf("Automovil %d cruza el puente en sentido %s a %.2f km/h. Tiempo estimado de cruce: %.2f segundos.\n", autos_terminados, sentido ? "Oeste a Este" : "Este a Oeste", velocidad, tiempo_cruce);

            autos_terminados++;
            usleep(tiempo_cruce * 1000000);  // Simular tiempo de cruce
            sched_yield(); // Permitir que otros hilos se ejecuten
        }

        // Cambiar de sentido después de permitir el paso
        direccionPuente = !direccionPuente;
        pthread_cond_broadcast(&cond_oficial); // Notificar a todos los autos esperando que el sentido ha cambiado
        pthread_mutex_unlock(&mutex);
    }

    pthread_exit(NULL);
}



void modo_oficial_transito() {
    pthread_t oficial1, oficial2, ambulancias[NUM_AMBULANCIAS];
    int sentido1 = 0; // Este a Oeste
    int sentido2 = 1; // Oeste a Este
    int i, ids[NUM_AMBULANCIAS];

    if (pthread_create(&oficial1, NULL, oficial_transito, (void *)&sentido1) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&oficial2, NULL, oficial_transito, (void *)&sentido2) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < NUM_AMBULANCIAS; i++) {
        ids[i] = i;
        if (rand() % 100 < porcentaje_ambulancias) {
            if (pthread_create(&ambulancias[i], NULL, _ambulancia_oficial, (void *)&ids[i]) != 0) {
                perror("pthread_create para ambulancia");
                exit(EXIT_FAILURE);
            }
        }
    }

    pthread_join(oficial1, NULL);
    pthread_join(oficial2, NULL);

    for (i = 0; i < NUM_AMBULANCIAS; i++) {
        pthread_join(ambulancias[i], NULL);
    }

    printf("Todos los vehículos han cruzado el puente. Los oficiales de tránsito y las ambulancias se retiran.\n");
}





void *_automovil_semaforo(void *arg) {
    int id = *((int *)arg);
    int mi_direccion = (id % 2 == 0) ? 0 : 1; // Oeste para pares, Este para impares

    usleep(rand() % 1000000); // Simula el tiempo de espera antes de llegar al puente

    pthread_mutex_lock(&mutex);
    printf("Automovil %d llega al puente desde el %s\n", id, mi_direccion ? "Este" : "Oeste");

    while ((mi_direccion == 0 && !semaforo_oeste) || (mi_direccion == 1 && !semaforo_este)) {
        printf("Automovil %d espera a que el semaforo este en Verde en el %s.\n", id, mi_direccion ? "Este" : "Oeste");
        pthread_cond_wait(&cond_semaforo, &mutex);
    }

    double velocidad = velocidad_aleatoria((mi_direccion == 0) ? rango_velocidad_este_inferior : rango_velocidad_oeste_inferior,
                                           (mi_direccion == 0) ? rango_velocidad_este_superior : rango_velocidad_oeste_superior);
    double tiempo_cruce = calcular_tiempo_cruce(longitud_puente, velocidad); // tiempo en segundos

    printf("Automovil %d comienza a cruzar el puente desde el %s a %.2f km/h. Tiempo estimado: %.2f segundos\n", id, mi_direccion ? "Este" : "Oeste", velocidad, tiempo_cruce);

    pthread_mutex_unlock(&mutex);

    usleep(tiempo_cruce * 1000000); // Simular tiempo de cruce

    pthread_mutex_lock(&mutex);
    printf("Automovil %d ha cruzado el puente desde el %s. Tiempo real de cruce: %.2f segundos\n", id, mi_direccion ? "Este" : "Oeste", tiempo_cruce);
    pthread_mutex_unlock(&mutex);

    pthread_exit(NULL);
}








void *semaforo(void *arg) {
    while (semaphore_active) {
        pthread_mutex_lock(&mutex);
        semaforo_este = 1;
        semaforo_oeste = 0;
        printf("Semaforo del lado Este: Verde\n");
        printf("Semaforo del lado Oeste: Rojo\n");
        pthread_cond_broadcast(&cond_semaforo);
        pthread_mutex_unlock(&mutex);
        sleep(tiempo_semaforo_este);

        pthread_mutex_lock(&mutex);
        semaforo_este = 0;
        semaforo_oeste = 1;
        printf("Semaforo del lado Este: Rojo\n");
        printf("Semaforo del lado Oeste: Verde\n");
        pthread_cond_broadcast(&cond_semaforo);
        pthread_mutex_unlock(&mutex);
        sleep(tiempo_semaforo_oeste);

        pthread_mutex_lock(&mutex);
        if (vehiculos_en_cola == 0) {
            semaphore_active = 0;
        }
        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}






void modo_semaforo() {
    pthread_t autos[MAX_AUTOS];
    pthread_t hilo_semaforo;
    int ids[MAX_AUTOS];
    int i;

    if (pthread_create(&hilo_semaforo, NULL, semaforo, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    sleep(1); // Esperar a que el semáforo se inicie correctamente

    for (i = 0; i < MAX_AUTOS; i++) {
        ids[i] = i;
        if (rand() % 100 < porcentaje_ambulancias) {
            if (pthread_create(&autos[i], NULL, _ambulancia_semaforo, (void *)&ids[i]) != 0) {
                perror("pthread_create ambulancia");
                exit(EXIT_FAILURE);
            }
        } else {
            if (pthread_create(&autos[i], NULL, _automovil_semaforo, (void *)&ids[i]) != 0) {
                perror("pthread_create automovil");
                exit(EXIT_FAILURE);
            }
        }
        usleep(1000000); // 1 segundo entre lanzamientos de autos, ajusta según la necesidad
    }

    for (i = 0; i < MAX_AUTOS; i++) {
        pthread_join(autos[i], NULL);
    }

    pthread_cancel(hilo_semaforo);
    pthread_join(hilo_semaforo, NULL);

    printf("Todos los vehiculos han cruzado.\n");
}





void *_ambulancia_oficial(void *arg) {
    int id = *((int *)arg);
    int mi_direccion = id % 2;  // 0: Este, 1: Oeste

    usleep(rand() % 1000000);  // Simulación de aproximación al puente

    pthread_mutex_lock(&mutex);
    ambulancias_esperando++;
    printf("Ambulancia %d llega al puente desde el %s.\n", id, mi_direccion ? "Oeste" : "Este");

    while (!puente_libre) {
        printf("Ambulancia %d espera a que el puente este libre.\n", id);
        pthread_cond_wait(&cond_puente_libre, &mutex);
    }

    ambulancias_esperando--;
    puente_libre = 0;  // Ocupar el puente

    double velocidad = velocidad_aleatoria(rango_velocidad_este_inferior, rango_velocidad_este_superior); // Asumiendo que los rangos son iguales para simplificar
    double tiempo_cruce = calcular_tiempo_cruce(longitud_puente, velocidad);

    printf("Ambulancia %d cruza el puente con prioridad desde el %s a %.2f km/h. Tiempo estimado de cruce: %.2f segundos.\n", id, mi_direccion ? "Oeste" : "Este", velocidad, tiempo_cruce);

    usleep(tiempo_cruce * 1000000);  // Simular cruce del puente

    puente_libre = 1;  // Liberar el puente
    printf("Ambulancia %d ha cruzado el puente desde el %s.\n", id, mi_direccion ? "Oeste" : "Este");
    pthread_cond_broadcast(&cond_puente_libre);  // Notificar a todos que el puente está libre
    pthread_mutex_unlock(&mutex);

    pthread_exit(NULL);
}




int continuar_semaforo = 1; // Variable de control externa





int main() {
    int opcion;
    int continuar = 1;

    // Inicializaciones necesarias
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond_semaforo, NULL);
    pthread_cond_init(&cond_nuevo_auto, NULL);
    pthread_cond_init(&cond_este, NULL);
    pthread_cond_init(&cond_oeste, NULL);

    leerConfiguracion("config.txt");

    srand(time(NULL));

    while (continuar) {
        printf("\nModo de administracion del puente:\n");
        printf("1. Modo carnage\n");
        printf("2. Modo semaforo\n");
        printf("3. Modo oficial de transito\n");
        printf("4. Salir\n");
        printf("Ingrese su opcion: ");
        scanf("%d", &opcion);

        switch (opcion) {
            case 1:
                modo_carnage();
                break;
            case 2:
                modo_semaforo();
                break;
            case 3:
                modo_oficial_transito();
                break;
            case 4:
                continuar = 0;
                break;
            default:
                printf("Opcion invalida\n");
                break;
        }
    }

    // Limpieza antes de finalizar
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond_semaforo);
    pthread_cond_destroy(&cond_nuevo_auto);
    pthread_cond_destroy(&cond_este);
    pthread_cond_destroy(&cond_oeste);

    return 0;
}