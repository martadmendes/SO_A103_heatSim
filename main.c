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

typedef struct argumentos_simul {
  int id;
  int N;
  int iter;
  int trab;
  int slicesz;
} args_simul;


/*--------------------------------------------------------------------
| Function: simul
---------------------------------------------------------------------*/

void *simul(void* args) {
  args_simul *arg = (args_simul *) args;
  DoubleMatrix2D *slices[2];
  int i, j, actual, next, msgsz, iter;

  /* Create slices */
  slices[0] = dm2dNew(arg->slicesz+2, arg->N+2);
  slices[1] = dm2dNew(arg->slicesz+2, arg->N+2);

  if (slices[0] == NULL || slices[1] == NULL) {
    fprintf(stderr, "\nErro ao criar Matrix2d numa trabalhadora.\n");
    exit(-1);
  }
  
  /* Receive slice from master */
  msgsz = (arg->N+2) * sizeof(double);
  for (i = 0; i < arg->slicesz + 2; i++) {
    if (receberMensagem(0, arg->id, dm2dGetLine(slices[0], i), msgsz) != msgsz) {
      fprintf(stderr, "\nErro ao receber mensagem da tarefa mestre.\n");
      exit(-1);
    }
  }

  dm2dCopy(slices[1], slices[0]);
  
  /* Iteration */
  for (iter = 0; iter < arg->iter; iter++) {
    actual = iter % 2;
    next = 1 - iter % 2;

    /* Calculate simulation */
    for (i = 0; i < arg->slicesz; i++) {
      for (j = 0; j < arg->N; j++) {
        double val = (dm2dGetEntry(slices[actual], i, j+1) +
                      dm2dGetEntry(slices[actual], i+2, j+1) +
                      dm2dGetEntry(slices[actual], i+1, j) +
                      dm2dGetEntry(slices[actual], i+1, j+2))/4;
        dm2dSetEntry(slices[next], i+1, j+1, val);
      }
    }

    /* Exchange new values of adjacent lines. Even slaves send before receiving */
    if (arg->id % 2 == 0) {
      if (arg->id > 1) {
        if (enviarMensagem(arg->id, arg->id - 1, dm2dGetLine(slices[next], 1), msgsz) != msgsz) {
          fprintf(stderr, "\nErro ao enviar mensagem entre trabalhadoras.\n");
          exit(-1);
        }
        if (receberMensagem(arg->id - 1, arg->id, dm2dGetLine(slices[next], 0), msgsz) != msgsz) {
          fprintf(stderr, "\nErro ao receber mensagem entre trabalhadoras.\n");
          exit(-1);
        }
      }
      if (arg->id < arg->trab) {
        if (enviarMensagem(arg->id, arg->id + 1, dm2dGetLine(slices[next], arg->slicesz), msgsz) != msgsz) {
          fprintf(stderr, "\nErro ao enviar mensagem entre trabalhadoras.\n");
          exit(-1);
        }
        if (receberMensagem(arg->id + 1, arg->id, dm2dGetLine(slices[next], arg->slicesz + 1), msgsz) != msgsz) {
          fprintf(stderr, "\nErro ao receber mensagem entre trabalhadoras.\n");
          exit(-1);
        }
      }
    } else {
      if (arg->id > 1) {
        if (receberMensagem(arg->id - 1, arg->id, dm2dGetLine(slices[next], 0), msgsz) != msgsz) {
          fprintf(stderr, "\nErro ao receber mensagem entre trabalhadoras.\n");
          exit(-1);
        }
        if (enviarMensagem(arg->id, arg->id - 1, dm2dGetLine(slices[next], 1), msgsz) != msgsz) {
          fprintf(stderr, "\nErro ao enviar mensagem entre trabalhadoras\n");
          exit(-1);
        }
      }
      if (arg->id < arg->trab) {
        if (receberMensagem(arg->id + 1, arg->id, dm2dGetLine(slices[next], arg->slicesz + 1), msgsz) != msgsz) {
          fprintf(stderr, "\nErro ao receber mensagem entre trabalhadoras\n");
          exit(-1);
        }
        if (enviarMensagem(arg->id, arg->id + 1, dm2dGetLine(slices[next], arg->slicesz), msgsz) != msgsz) {
          fprintf(stderr, "\nErro ao enviar mensagem entre trabalhadoras\n");
          exit(-1);
        }
      }
    }
  }

  /* Send final slices to master */
  for (i = 0; i < arg->slicesz; i++) {
    if (enviarMensagem(arg->id, 0, dm2dGetLine(slices[next], i + 1), msgsz) != msgsz) {
      fprintf(stderr, "\nErro ao enviar mensagem à tarefa mestre.\n");
      exit(-1);
    }
  }

  /* Free memory */
  dm2dFree(slices[0]);
  dm2dFree(slices[1]);

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

  DoubleMatrix2D *matrix;
  int slicesz, msgsz, i, j; 
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

  /* Initialization of Message Passing Library */
  if (inicializarMPlib(csz,trab+1) != 0) {
    fprintf(stderr, "\nErro ao inicializar MPLib.\n");
    return 1;
  }

  /* Calculate initial matrix */
  matrix = dm2dNew(N+2, N+2);

  if (matrix == NULL) {
    fprintf(stderr, "\nErro: Nao foi possivel alocar memoria para a matriz.\n\n");
    return -1;
  }

  dm2dSetLineTo (matrix, 0, tSup);
  dm2dSetLineTo (matrix, N+1, tInf);
  dm2dSetColumnTo (matrix, 0, tEsq);
  dm2dSetColumnTo (matrix, N+1, tDir);

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

  /* Send slices to slaves */
  msgsz = (N+2) * sizeof(double);
  for (i = 0; i < trab; i++) {
    for (j = 0; j < (slicesz + 2); j++) {
      if (enviarMensagem(0, i+1, dm2dGetLine(matrix, i*slicesz + j), msgsz)
          != msgsz) {
        fprintf(stderr, "\nErro ao enviar mensagem para trabalhadora.\n");
        return -1;
      }
    }
  }
  
  /* Receive final slices of each slave and saves in matrix */
  for (i = 0; i < trab; i++) {
    for (j = 0; j < slicesz; j++) {
      if (receberMensagem(i+1, 0, dm2dGetLine(matrix, i*slicesz + j + 1), msgsz) != msgsz) {
        fprintf(stderr, "\nErro ao receber mensagem de uma trabalhadora.\n");
        return -1;
      }
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
  dm2dPrint(matrix);

  /* Release memory */
  dm2dFree(matrix);
  free(slaves);
  free(slave_args);
  libertarMPlib();

  return 0;
}
