/*
// Projeto SO - exercicio 1, version 03
// Sistemas Operativos, DEI/IST/ULisboa 2017-18
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>

#include "matrix2d.h"
#include "mplib3.h"

/*--------------------------------------------------------------------
| Types
---------------------------------------------------------------------*/

typedef struct argumentos_simul {
  int id;
  int N;
  int iter;
  int trab;
  int slicesz;
  int maxD;
} args_simul;

typedef struct iteration_attributes {
  pthread_mutex_t iteration_mutex;
  pthread_cond_t  wait_for_workers;
  int             num_workers;
  int             final_iteration;
} iter_attr;

/*--------------------------------------------------------------------
| Global Variables
---------------------------------------------------------------------*/

DoubleMatrix2D   *matrices[2];
iter_attr        *iter_attr_array;

/*--------------------------------------------------------------------
| Function: simul
---------------------------------------------------------------------*/

void *simul(void* args) {
  args_simul *arg = (args_simul *) args;
  int i, j, actual, next, iter, line;
  double maxD, currD;


  /* Iteration */
  line = (arg->id - 1) * arg->slicesz;
  for (iter = 0; iter < arg->iter; iter++) {
    actual = iter % 2;
    next = 1 - iter % 2;
    maxD = 0;

    /* Calculate simulation */
    for (i = 0; i < arg->slicesz; i++) {
      for (j = 0; j < arg->N; j++) {
        double val = (dm2dGetEntry(matrices[actual], line + i, j+1) +
                      dm2dGetEntry(matrices[actual], line + i+2, j+1) +
                      dm2dGetEntry(matrices[actual], line + i+1, j) +
                      dm2dGetEntry(matrices[actual], line + i+1, j+2))/4;
        dm2dSetEntry(matrices[next], line + i+1, j+1, val);

        currD = fabs(val - dm2dGetEntry(matrices[actual], line + i+1, j+1));
        if (currD > maxD)
          maxD = currD;
      }
    }

    if (maxD < arg->maxD) 
      iter_attr_array[iter].final_iteration = 1;

    if (iter_attr_array[iter].final_iteration)
      break;
    

    /* Lock shared memory */
    if(pthread_mutex_lock(&iter_attr_array[iter].iteration_mutex) != 0) {
      fprintf(stderr, "\nErro ao bloquear mutex\n");
      pthread_exit(NULL);
    }
    /* Update worker counter per iteration */
    iter_attr_array[iter].num_workers--;
    
    if (iter_attr_array[iter].num_workers == 0) {
        /* Frees all waiting workers if this is last worker in iteration */
        if (pthread_cond_broadcast(&iter_attr_array[iter].wait_for_workers) != 0) {
          fprintf(stderr, "\nErro ao desbloquear variável de condição\n");
          pthread_exit(NULL);
        }
    } else {
        /* If there are more workers on this iteration, wait for them */
        while (iter_attr_array[iter].num_workers > 0) {
            if (pthread_cond_wait(&iter_attr_array[iter].wait_for_workers,
                                  &iter_attr_array[iter].iteration_mutex) != 0) {
                fprintf(stderr, "\nErro ao esperar pela variável de condição\n");
                pthread_exit(NULL);
            }
        }
    }
    /* Unlock shared memory */
    if(pthread_mutex_unlock(&iter_attr_array[iter].iteration_mutex) != 0) {
      fprintf(stderr, "\nErro ao desbloquear mutex\n");
      exit(1);
    }
  }
  pthread_exit(NULL);
}

/*--------------------------------------------------------------------
| Function: parse_integer_or_exit
---------------------------------------------------------------------*/

int parse_integer_or_exit(char const *str, char const *name)
{
  int value;

  if(sscanf(str, "%d", &value) != 1) {
    fprintf(stderr, "\nErro no argumento \"%s\".\n\n", name);
    exit(1);
  }
  return value;
}

/*--------------------------------------------------------------------
| Function: parse_double_or_exit
---------------------------------------------------------------------*/

double parse_double_or_exit(char const *str, char const *name)
{
  double value;

  if(sscanf(str, "%lf", &value) != 1) {
    fprintf(stderr, "\nErro no argumento \"%s\".\n\n", name);
    exit(1);
  }
  return value;
}


/*--------------------------------------------------------------------
| Function: main
---------------------------------------------------------------------*/

int main (int argc, char** argv) {

  if(argc != 10) {
    fprintf(stderr, "\nNumero invalido de argumentos.\n");
    fprintf(stderr, "Uso: heatSim N tEsq tSup tDir tInf iter trab csz\n\n");
    return 1;
  }

  /* Read input */
  /* argv[0] = program name */
  int N = parse_integer_or_exit(argv[1], "N");
  double tEsq = parse_double_or_exit(argv[2], "tEsq");
  double tSup = parse_double_or_exit(argv[3], "tSup");
  double tDir = parse_double_or_exit(argv[4], "tDir");
  double tInf = parse_double_or_exit(argv[5], "tInf");
  int iter = parse_integer_or_exit(argv[6], "iter");
  int trab = parse_integer_or_exit(argv[7], "trab");
  int csz = parse_integer_or_exit(argv[8], "csz");
  double maxD = parse_double_or_exit(argv[9], "maxD");

  int slicesz, final_iteration, i;
  args_simul *slave_args;
  pthread_t *slaves;

  fprintf(stderr, "\nArgumentos:\n"
	" N=%d tEsq=%.1f tSup=%.1f tDir=%.1f tInf=%.1f iter=%d trab=%d csz=%d maxD=%.2f\n",
          N, tEsq, tSup, tDir, tInf, iter, trab, csz, maxD);

  /* Input verification */
  if(N < 1 || tEsq < 0 || tSup < 0 || tDir < 0 || tInf < 0 || iter < 1 || trab < 1 || csz < 1 || maxD < 0) {
    fprintf(stderr, "\nErro: Argumentos invalidos.\n"
	" Lembrar que N >= 1, temperaturas >= 0, iter >= 1, trab >= 1, csz >= 1 e maxD >= 0\n\n");
    return 1;
  }

  /* Initialization of Mutexes and Condition Variables for the Iterations*/
  iter_attr_array = (iter_attr*)malloc(sizeof(iter_attr)*iter);
  for (i=0; i<iter; i++) {
    if(pthread_mutex_init(&iter_attr_array[i].iteration_mutex, NULL) != 0) {
        fprintf(stderr, "\nErro ao inicializar mutex\n");
        exit(1);
    }
    if(pthread_cond_init(&iter_attr_array[i].wait_for_workers, NULL) !=0) {
        fprintf(stderr, "\nErro ao inicializar a variável de condição\n");
        exit(1);
    }
    iter_attr_array[i].num_workers = trab;
    iter_attr_array[i].final_iteration = 0;
  }

  /* Calculate initial matrix */
  matrices[0] = dm2dNew(N+2, N+2);
  matrices[1] = dm2dNew(N+2, N+2);

  if (matrices[0] == NULL || matrices[1] == NULL) {
    fprintf(stderr, "\nErro: Nao foi possivel alocar memoria para a matriz.\n\n");
    return -1;
  }

  dm2dSetLineTo (matrices[0], 0, tSup);
  dm2dSetLineTo (matrices[0], N+1, tInf);
  dm2dSetColumnTo (matrices[0], 0, tEsq);
  dm2dSetColumnTo (matrices[0], N+1, tDir);
  dm2dCopy(matrices[1], matrices[0]);

  /* Calculate slice size */
  slicesz = N / trab;

  /* Memory allocation for slaves and args */
  slaves     = (pthread_t*)malloc(trab*sizeof(pthread_t));
  slave_args = (args_simul*)malloc(trab*sizeof(args_simul));

  if (slaves == NULL || slave_args == NULL) {
    fprintf(stderr, "\nErro ao alocar memória para trabalhadoras.\n");
    return -1;
  }

  /* Create slaves and args */
  for (i=0; i<trab; i++) {
    slave_args[i].id = i+1;
    slave_args[i].N = N;
    slave_args[i].iter = iter;
    slave_args[i].trab = trab;
    slave_args[i].slicesz = slicesz;
    slave_args[i].maxD = maxD;
    if (pthread_create(&slaves[i], NULL, simul, &slave_args[i]) != 0) {
      fprintf(stderr, "\nErro ao criar uma tarefa trabalhadora.\n");
      return -1;
    }
  }

  /* Esperar que os Escravos Terminem */
  for (i = 0; i < trab; i++) {
    if (pthread_join(slaves[i], NULL)) {
      fprintf(stderr, "\nErro ao esperar por um escravo.\n");
      return -1;
    }
  }

  /* Get final iteration number */
  final_iteration = iter -1;
  for (i = 0; i < iter; i++) 
    if (iter_attr_array[i].final_iteration){
      final_iteration = i;
      printf("The final iteration is %d, skipping %d iterations", final_iteration + 1, iter - final_iteration - 1);
      break;
    }
  

  /* Print resulting matrix */
  dm2dPrint(matrices[1-final_iteration%2]);

  /* Release memory */
  for(i=0; i<iter; i++) {
    if(pthread_mutex_destroy(&iter_attr_array[i].iteration_mutex) != 0) {
      fprintf(stderr, "\nErro ao destruir mutex\n");
      exit(1);
    }
    if(pthread_cond_destroy(&iter_attr_array[i].wait_for_workers) != 0) {
      fprintf(stderr, "\nErro ao destruir variável de condição\n");
      exit(1);
    }
  }
  dm2dFree(matrices[0]);
  dm2dFree(matrices[1]);
  free(slaves);
  free(slave_args);
  free(iter_attr_array);
  return 0;
}
