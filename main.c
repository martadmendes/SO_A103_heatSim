/*
// Projeto SO - exercicio 1, version 03
// Sistemas Operativos, DEI/IST/ULisboa 2017-18
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

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
} args_simul;

/*--------------------------------------------------------------------
| Global Variables
---------------------------------------------------------------------*/

DoubleMatrix2D   *matrices[2];
pthread_mutex_t  *iteration_mutex;
pthread_cond_t   *wait_for_workers;
int              *num_workers;

/*--------------------------------------------------------------------
| Function: simul
---------------------------------------------------------------------*/

void *simul(void* args) {
  args_simul *arg = (args_simul *) args;
  int i, j, actual, next, iter, line;


  /* Iteration */
  line = (arg->id - 1) * arg->slicesz;
  for (iter = 0; iter < arg->iter; iter++) {
    actual = iter % 2;
    next = 1 - iter % 2;

    /* Calculate simulation */
    for (i = 0; i < arg->slicesz; i++) {
      for (j = 0; j < arg->N; j++) {
        double val = (dm2dGetEntry(matrices[actual], line + i, j+1) +
                      dm2dGetEntry(matrices[actual], line + i+2, j+1) +
                      dm2dGetEntry(matrices[actual], line + i+1, j) +
                      dm2dGetEntry(matrices[actual], line + i+1, j+2))/4;
        dm2dSetEntry(matrices[next], line + i+1, j+1, val);
      }
    }

    if(pthread_mutex_lock(&iteration_mutex[iter]) != 0) {
      fprintf(stderr, "\nErro ao bloquear mutex\n");
      pthread_exit(NULL);
    }
    num_workers[iter]--;
    
    if (num_workers[iter] == 0) {
        if (pthread_cond_broadcast(&wait_for_workers[iter]) != 0) {
          fprintf(stderr, "\nErro ao desbloquear variável de condição\n");
          pthread_exit(NULL);
        }
    } else {
        while (num_workers[iter] > 0) {
            if (pthread_cond_wait(&wait_for_workers[iter], &iteration_mutex[iter]) != 0) {
                fprintf(stderr, "\nErro ao esperar pela variável de condição\n");
                pthread_exit(NULL);
            }
        }
    }
    if(pthread_mutex_unlock(&iteration_mutex[iter]) != 0) {
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

  if(argc != 9) {
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

  int slicesz, i;
  args_simul *slave_args;
  pthread_t *slaves;

  fprintf(stderr, "\nArgumentos:\n"
	" N=%d tEsq=%.1f tSup=%.1f tDir=%.1f tInf=%.1f iter=%d trab=%d csz=%d\n",
          N, tEsq, tSup, tDir, tInf, iter, trab, csz);

  /* Input verification */
  if(N < 1 || tEsq < 0 || tSup < 0 || tDir < 0 || tInf < 0 || iter < 1 || trab < 1 || csz < 1) {
    fprintf(stderr, "\nErro: Argumentos invalidos.\n"
	" Lembrar que N >= 1, temperaturas >= 0, iter >= 1, trab >= 1 e csz >= 0\n\n");
    return 1;
  }

  /* Initialization of Mutexes and Condition Variables for the Iterations*/
  iteration_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t)*iter);
  wait_for_workers = (pthread_cond_t*)malloc(sizeof(pthread_cond_t)*iter);
  num_workers = (int*)malloc(sizeof(int)*iter);
  for (i=0; i<iter; i++) {
    if(pthread_mutex_init(&iteration_mutex[i], NULL) != 0) {
        fprintf(stderr, "\nErro ao inicializar mutex\n");
        exit(1);
    }
    if(pthread_cond_init(&wait_for_workers[i], NULL) !=0) {
        fprintf(stderr, "\nErro ao inicializar a variável de condição\n");
        exit(1);
    }
    num_workers[i] = trab;
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

  /* Print resulting matrix */
  dm2dPrint(matrices[iter%2]);

  /* Release memory */
  for(i=0; i<iter; i++) {
    if(pthread_mutex_destroy(&iteration_mutex[i]) != 0) {
      fprintf(stderr, "\nErro ao destruir mutex\n");
      exit(1);
    }
    if(pthread_cond_destroy(&wait_for_workers[i]) != 0) {
      fprintf(stderr, "\nErro ao destruir variável de condição\n");
      exit(1);
    }
  }
  dm2dFree(matrices[0]);
  dm2dFree(matrices[1]);
  free(slaves);
  free(slave_args);
  free(iteration_mutex);
  free(wait_for_workers);
  free(num_workers);
  return 0;
}
