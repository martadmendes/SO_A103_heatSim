/*
// Projeto SO - exercicio 1, version 03
// Sistemas Operativos, DEI/IST/ULisboa 2017-18
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "matrix2d.h"
#include "mplib3.h"
#define BUFFSZ 256

typedef struct argumentos_simul {
  DoubleMatrix2D *matrix;
  DoubleMatrix2D *matrix_aux;
  int            linhas;
  int            colunas;
  int            iteracoes;
  int            thread_id;
  int            thread_num;
} args_simul;


/*--------------------------------------------------------------------
| Function: simul
---------------------------------------------------------------------*/

void *simul(void* args) {

  DoubleMatrix2D *m, *aux, *tmp;
  int iter, i, j, linhas, colunas, id, trab;
  double value;
  double *line_values;
  args_simul *arg = (args_simul *) args;


  if(linhas < 2 || colunas < 2)
    return -1;

  m = arg->matrix;
  aux = arg->matrix_aux;
  linhas = arg->linhas;
  colunas = arg->colunas;
  id = arg->thread_id;
  trab = arg->thread_num;

  for (iter = 0; iter < numIteracoes; iter++) {

    for (i = 1; i < linhas - 1; i++)
      for (j = 1; j < colunas - 1; j++) {
        value = ( dm2dGetEntry(m, i-1, j) + dm2dGetEntry(m, i+1, j) +
		dm2dGetEntry(m, i, j-1) + dm2dGetEntry(m, i, j+1) ) / 4.0;
        dm2dSetEntry(aux, i, j, value);
      }

    tmp = aux;
    aux = m;
    m = tmp;

    //trocar mensagens entre outros trabalhadores
    if (id > 1) {
      line_values = dm2dGetLine(m, 1);
      enviarMensagem(id, id-1, line_values, BUFFSZ);
      receberMensage(id-1, id, line_values, BUFFSZ);
      dm2SetLine(m, 0, line_values);
    }
    if (id < trab) {
      line_values = dm2GetLine(m, linhas-2);
      enviarMensagem(id, id+1, line_values, BUFFSZ);
      receberMensagem(id+1, id, line_values, BUFFSZ);
      dm2dSetLine(m, linhas-1, line_values);
    }
  }

  // enviar msg ao master
  enviarMensagem(id, 0, m, BUFFSZ);
  dm2dFree(aux);

  return 0;
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
    fprintf(stderr, "Uso: heatSim N tEsq tSup tDir tInf iteracoes trab csz\n\n");
    return 1;
  }

  /* argv[0] = program name */
  int N = parse_integer_or_exit(argv[1], "N");
  double tEsq = parse_double_or_exit(argv[2], "tEsq");
  double tSup = parse_double_or_exit(argv[3], "tSup");
  double tDir = parse_double_or_exit(argv[4], "tDir");
  double tInf = parse_double_or_exit(argv[5], "tInf");
  int iteracoes = parse_integer_or_exit(argv[6], "iteracoes");
  int trab = parse_integer_or_exit(argv[7], "trab");
  int csz = parse_integer_or_exit(argv[8], "csz");

  DoubleMatrix2D *matrix, *result, *slice, *slice_aux;


  fprintf(stderr, "\nArgumentos:\n"
	" N=%d tEsq=%.1f tSup=%.1f tDir=%.1f tInf=%.1f iteracoes=%d\n",
	N, tEsq, tSup, tDir, tInf, iteracoes);

  if(N < 1 || tEsq < 0 || tSup < 0 || tDir < 0 || tInf < 0 || iteracoes < 1 || trab < 1 || csz < 1) {
    fprintf(stderr, "\nErro: Argumentos invalidos.\n"
	" Lembrar que N >= 1, temperaturas >= 0 e iteracoes >= 1\n\n");
    return 1;
  }

  matrix = dm2dNew(N+2, N+2);
  result = dm2dNew(N+2, N+2);

  if (matrix == NULL || matrix_aux == NULL) {
    fprintf(stderr, "\nErro: Nao foi possivel alocar memoria para as matrizes.\n\n");
    return -1;
  }

  int slicesz = N / trab;
  int i;

  if (inicializarMPlib(csz,trab+1) == -1) {
    printf("Erro ao inicializar MPLib.\n");
    return 1;
  }

  for(i=0; i<N+2; i++)
    dm2dSetLineTo(matrix, i, 0);

  dm2dSetLineTo (matrix, 0, tSup);
  dm2dSetLineTo (matrix, N+1, tInf);
  dm2dSetColumnTo (matrix, 0, tEsq);
  dm2dSetColumnTo (matrix, N+1, tDir);

  dm2dCopy(result, matrix);

  /* create slaves */
  args_simul *slave_args;
  pthread_t *slaves;
  int j;
  double *line_values;

  slave_args = (args_simul*)malloc(trab*sizeof(args_simul));
  slaves     = (pthread_t*)malloc(trab*sizeof(pthread_t));

  for (i=0; i<trab; i++) {
    slave_args[i].thread_id = i+1;
    slave_args[i].thread_num  = trab;
    slave_args[i].linhas = slicesz+2;
    slave_args[i].colunas = N+2;
    slave_args[i].iteracoes = iteracoes;
    slice = dm2dNew(slicesz+2, N+2);
    slice_aux = dm2dNew(slicesz+2, N+2);

    for (j=0; j<slicesz+2; j++) {
      line_values = dm2dGetLine(matrix, i*slicesz + j);
      dm2dSetLine(slice, j, line_values);
    }
    dm2dCopy(slice_aux, slice);
    slave_args[i].matrix = slice;
    slave_args[i].matrix_aux = slice_aux;

    pthread_create(&slaves[i], NULL, simul, &slave_args[i]);
  }

  /* Receber os dados dos escravos */
  for (i = 0; i < trab; i++) {
    receberMensagem(i+1, 0, slice, BUFFSZ);
    for (j = 1; j < slicesz; j++) {
      line_values = dm2dGetLine(slice, j);
      dm2dSetLine(result, i*slicesz + j, line_values);
    }
    dm2dFree(slice);
  }

  /* Esperar que os Escravos Terminem */
  for (i = 0; i < trab; i++) {
    if (pthread_join(slaves[i], NULL)) {
      fprintf(stderr, "\nErro ao esperar por um escravo.\n");
      return -1;
    }
  }

  dm2dPrint(result);

  dm2dFree(matrix);
  dm2dFree(result);

  return 0;
}
