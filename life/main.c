#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <signal.h>


// cd /media/psf/Home/edd01/Programa; make;
// G_SLICE=always-malloc G_DEBUG=gc-friendly  valgrind -v --tool=memcheck --leak-check=full --show-leak-kinds=all --num-callers=40 --log-file=valgrind.log ./simulate tests/test.txt 2

int neighbors(int row, int col, int total_rows, int total_columns, int* array){
	int num_neighbors = 0;
	for (int delta_x=-1; delta_x<=1; delta_x++){
		for (int delta_y=-1; delta_y<=1; delta_y++){
			int new_x = row + delta_x;
			int new_y = col + delta_y;
			if (new_x >=0 && new_y>=0 && new_x <total_rows && new_y<total_columns && (row != new_x || col != new_y) && array[new_x*total_columns + new_y]){
				num_neighbors++;
			}
		}
	}
	return num_neighbors;
}

struct wrapper
{
	int* matriz_actual;
	int* matriz_pasada;
	int max_filas_total;
	int max_columnas_total;
	int min_filas;
	int max_filas;
};

typedef struct wrapper Wrapper;

Wrapper* createWrapper(int* matriz_actual, int* matriz_pasada, int max_filas_total, int max_columnas_total, int min_filas, int max_filas){
	Wrapper* wpr = malloc(sizeof(Wrapper));
	wpr->matriz_actual = matriz_actual;
	wpr->matriz_pasada = matriz_pasada;
	wpr->max_filas_total = max_filas_total;
	wpr->max_columnas_total = max_columnas_total;
	wpr->min_filas = min_filas;
	wpr->max_filas = max_filas;
	return wpr;
}

void print_array(int array[], int filas, int columnas){
	// printf("[");
	for (int fila=0;fila<filas; fila++){
		// printf("[");
		for (int columna = 0; columna<columnas;columna++){
			printf("%d ", array[fila * columnas + columna]);
		}
		printf("\n");
	}
	printf("\n");
}

void*  update_matrix(void* arguments){
	Wrapper *args = arguments;
	int* matriz_actual = args->matriz_actual;
	int* matriz_pasada = args->matriz_pasada;
	int max_filas_total = args->max_filas_total;
	int max_columnas_total = args->max_columnas_total;
	int min_filas = args->min_filas;
	int max_filas = args->max_filas;
	// printf("Entro thread filas de %d a %d\n", min_filas, max_filas);
	for(int i = min_filas; i < max_filas; i++) {
		for(int j = 0; j < max_columnas_total; j++) {
			// printf("%d, ", matriz[i * columnas + j]);
				//ACTUALIZAR ARRAY CON NUEVOS VALORES
				if (matriz_pasada[i * max_columnas_total + j]) {
					if (neighbors(i, j, max_filas_total, max_columnas_total, matriz_pasada) < 2 || neighbors(i, j, max_filas_total, max_columnas_total, matriz_pasada) > 3){
						matriz_actual[i * max_columnas_total + j] = 0;	// muere
					}
				}
				else {
					if (neighbors(i, j, max_filas_total, max_columnas_total, matriz_pasada) == 3){
						matriz_actual[i * max_columnas_total + j] = 1;	// se genera una nueva
					}
				}
		}
	}
	free(arguments);
	pthread_exit(NULL);
}

int main(int argc, char** argv)
{
	////////////////////////////////////////////
	/////// leer archivo
	////////////////////////////////////////////

	FILE* f = fopen(argv[1], "r");
	int iteraciones, filas, columnas, numero_vivas, n_threads;
	fscanf(f, "%d %d %d %d %d", &iteraciones, &filas, &columnas, &numero_vivas, &n_threads);
	int* matriz = malloc(filas * columnas * sizeof(int));
	printf("numero de threads: %d \n", n_threads);

	// inicializamos valores a 0.
	for(int row = 0; row < filas; row++){
		for(int col = 0; col < columnas; col++){
			matriz[row * columnas + col] = 0;
		}
	}	
	// Abrimos la ventana con las dimensiones especificadas
	for (int viva=0;viva<numero_vivas;viva++){
		int fila_actual, col_actual;
		fscanf(f, "%d %d", &fila_actual, &col_actual);
		matriz[fila_actual * columnas + col_actual] = 1;
	}

	// Cerramos el archivo de input
	fclose(f);

	print_array(matriz, filas, columnas);


	////////////////////////////////////////////
	/////// memoria compartida
	//////////////////////////////////////////

	int cores = sysconf(_SC_NPROCESSORS_ONLN);
	printf("cores= %d\n", cores);


    int* shared = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);									// sirve de lock entre procesos para que solo el primero imprima.
    pid_t* status = mmap(NULL, sizeof(int) * cores, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);							// almacena el pid para cada proceso.
    int* iterations = mmap(NULL, sizeof(int) * cores, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);						// almacena el numero de iteraciones de cada proceso.
    int* states = mmap(NULL, sizeof(int) * cores * filas * columnas, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);			// almacena los estados actuales de cada worker.

    // pthread_mutex_t* mutex = mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);

	////////////////////////////////////////////
	/////// creacion de procesos
	////////////////////////////////////////////

	int is_son = 0;
	int value;
	pid_t* pids[cores];
	for (int core=0; core<cores; core++){
		pids[core] = fork();
		if (!pids[core]){	// es hijo
			value = core;
			is_son++;
			break;
		} else {
			status[core] = pids[core];
		}
	}

	////////////////////////////////////////////
	/////// simulacion
	////////////////////////////////////////////

	if (is_son) {	// slave

		// printf("hijo\n");

		int* aux_array = malloc(filas * columnas * sizeof(int));		// copia de estado original de la matriz

		for (int iter = 1; iter < iteraciones + 1; iter++)		// +1 porque primera iteracion solo muestra valores iniciales. !!!
		{	
			if (!shared) break;
			memcpy(aux_array, matriz, filas * columnas * sizeof(int));

			// cuantas filas le toca a cada uno
			int num_filas[n_threads];
			for (int actual = 0; actual<n_threads; actual++){
				num_filas[actual%n_threads]=0;
			}
			for (int actual = 0; actual<filas; actual++){
				num_filas[actual%n_threads]++;
			}

			// crear los threads
			pthread_t threads[n_threads];;
			int status;
			int fila_max_anterior = 0;
			for (int t_index = 0; t_index<n_threads; t_index++){
				// wrapper de argumentos
				Wrapper* wpr = createWrapper(matriz, aux_array, filas, columnas, fila_max_anterior, fila_max_anterior + num_filas[t_index]);

				status = pthread_create(&threads[t_index], NULL, update_matrix, (void *)wpr);
				if (status != 0) {
					printf("[main] Oops. pthread_create returned error code %d\n", status);
					exit(-1);
				}
				fila_max_anterior += num_filas[t_index];
			}
			// join
			for (int t_index = 0; t_index<n_threads; t_index++){
				pthread_join(threads[t_index], NULL);
			}
			memcpy(states + value * (filas * columnas * sizeof(int)), matriz, filas * columnas * sizeof(int));
			iterations[value]++;
		}
		
		////////////////////////////////////////////
		/////// mostrar tablero
		////////////////////////////////////////////

		// watcher_open(filas, columnas);
		// for(int row = 0; row < filas; row++)
		// 	{
		// 		for(int col = 0; col < columnas; col++)
		// 		{
		// 			/* Marcamos todas las celdas como muertas */
		// 			watcher_set_cell(row, col, false);
		// 		}
		// 	}
		// for(int row = 0; row < filas; row++)
		// 	{
		// 		for(int col = 0; col < columnas; col++)
		// 		{
		// 			/* Marcamos todas las celdas como muertas */
		// 			watcher_set_cell(row, col, matriz[row * columnas + col]);
		// 		}
		// 	}
		// watcher_refresh();
		// watcher_close();

		// printf("llego\n");
		// pthread_mutex_lock(mutex);
		if (!*shared){
			*shared = 1;
			for (int index=0; index<cores; index++){					// matar al resto de los hijos
				if (index != value) {
					kill(status[index], SIGKILL);
					printf("Killed pid: %d\n", status[index]);
				}
			}
			printf("\n\nOutput:\n");
			print_array(states + value * (filas * columnas * sizeof(int)), filas, columnas);
		}
        // pthread_mutex_unlock(mutex);
        // printf("salio\n");

		////////////////////////////////////////////
		/////// cierre proceso hijo (no debiera llegar)
		////////////////////////////////////////////
		free(matriz);
		free(aux_array);
		exit(0);

	} else {	// master
		////////////////////////////////////////////
		/////// cierre proceso padre
		////////////////////////////////////////////

		for (int core=0; core<cores; core++){
	        int exit_code;
			waitpid(-1, &exit_code, 0);
		}
	    munmap(shared, sizeof(int));
	    munmap(status, sizeof(int) * cores);
	    munmap(iterations, sizeof(int) * cores);
	    munmap(states, sizeof(int) * cores * filas * columnas);

	    // munmap(mutex, sizeof(pthread_mutex_t));
    }
}
