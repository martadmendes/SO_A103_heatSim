/*
// Biblioteca de troca de mensagens, versao 3
// Sistemas Operativos, DEI/IST/ULisboa 2017-18
*/

#include "mplib3.h"
#include "leQueue.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>




/*--------------------------------------------------------------------
| Types
---------------------------------------------------------------------*/

typedef struct message_t {
  QueElem   elem;
  void      *contents;
  int       mess_size;
  int       consumed;
} Message_t;

typedef struct channel_t {
  QueHead           *message_list;
  pthread_mutex_t   mutex;
  pthread_cond_t    wait_for_free_space;
  pthread_cond_t    wait_for_messages;
} Channel_t;


/*--------------------------------------------------------------------
| Global Variables
---------------------------------------------------------------------*/

int                channel_capacity;
int                number_of_tasks;
Channel_t          **channel_array;

/*--------------------------------------------------------------------
  | Function: createChannel
  | Description: Cria um canal de comunicação com suporte para uma lista
  | de mensagens e um mutex para síncronização dessa lista.
  ---------------------------------------------------------------------*/

Channel_t* createChannel () {
  Channel_t     *channel = (Channel_t*) malloc (sizeof(Channel_t));

  if (channel == NULL) {
    fprintf(stderr, "\nErro ao criar canal\n");
    exit(1);
  }

  channel->message_list = leQueNewHead();

  if (channel->message_list == NULL) {
    fprintf(stderr, "\nErro ao criar lista de mensagens\n");
    exit(1);
  }

  if(pthread_mutex_init(&(channel->mutex), NULL) != 0) {
    fprintf(stderr, "\nErro ao inicializar mutex\n");
    exit(1);
  }

  if(pthread_cond_init(&(channel->wait_for_free_space), NULL) != 0) {
    fprintf(stderr, "\nErro ao inicializar variável de condição\n");
    exit(1);
  }

  if(pthread_cond_init(&(channel->wait_for_messages), NULL) != 0){
    fprintf(stderr, "\nErro ao inicializar variável de condição\n");
    exit(1);
  }

  leQueHeadInit (channel->message_list, 0);

  return channel;
}

/*--------------------------------------------------------------------
  | Function: inicializarMPlib
  | Description: Inicializa a API de troca de mensagens
  ---------------------------------------------------------------------*/

int inicializarMPlib(int capacidade_de_cada_canal, int ntasks) {
  int           i;
  Channel_t     *channel;

  number_of_tasks  = ntasks;
  channel_capacity = capacidade_de_cada_canal;
  channel_array    = (Channel_t**) malloc (sizeof(Channel_t*)*ntasks*ntasks);

  if (channel_array == NULL) {
    fprintf(stderr, "\nErro ao inicializar MPlib\n");
    exit(1);
  }

  for (i = 0; i < ntasks * ntasks; i++) {
    channel = createChannel();
    if (channel == NULL)
      exit(1);

    channel_array[i] = channel;
  }

  return 0;
}


/*--------------------------------------------------------------------
  | Function: libertarMPlib
  | Description: Liberta a memória usada por esta API
  ---------------------------------------------------------------------*/

void libertarMPlib() {
  int i;

  for (i = 0; i < number_of_tasks * number_of_tasks; ++i) {
    Channel_t   *channel = channel_array[i];
    Message_t   *mess    = (Message_t*) leQueRemFirst(channel->message_list);

    while (mess) {
      if (channel_capacity > 0)
        free (mess->contents);
      free (mess);
      mess = (Message_t*) leQueRemFirst(channel->message_list);
    }

    if(pthread_mutex_destroy(&(channel->mutex)) != 0) {
      fprintf(stderr, "\nErro ao destruir mutex\n");
      exit(1);
    }

    if(pthread_cond_destroy(&(channel->wait_for_free_space)) != 0) {
      fprintf(stderr, "\nErro ao destruir variável de condição\n");
      exit(1);
    }

    if(pthread_cond_destroy(&(channel->wait_for_messages)) != 0) {
      fprintf(stderr, "\nErro ao destruir variável de condição\n");
      exit(1);
    }

    free (channel);
  }

  free (channel_array);
}


/*--------------------------------------------------------------------
  | Function: receberMensagem
  | Description: Lê uma mensagem que esteja pendente na lista de
  | mensagens do canal ou bloqueia a tarefa que chama a função enquanto
  | não há mensagens para ler.
  ---------------------------------------------------------------------*/

int receberMensagem(int tarefaOrig, int tarefaDest, void *buffer, int tamanho) {
  Channel_t      *channel;
  Message_t      *mess;
  int            copysize;

  channel = (Channel_t*) channel_array[tarefaDest*number_of_tasks+tarefaOrig];

  if(pthread_mutex_lock(&(channel->mutex)) != 0) {
    fprintf(stderr, "\nErro ao bloquear mutex\n");
    exit(1);
  }

  mess    = (Message_t*) leQueRemFirst (channel->message_list);

  while (!mess) {
    if(pthread_cond_wait(&(channel->wait_for_messages), &(channel->mutex)) != 0) {
        fprintf(stderr, "\nErro ao esperar pela variável de condição\n");
        exit(1);
    }
    mess = (Message_t*) leQueRemFirst (channel->message_list);
  }

  copysize = (mess->mess_size<tamanho) ? mess->mess_size : tamanho;
  memcpy(buffer, mess->contents, copysize);

  if (channel_capacity > 0) {
    free(mess->contents);
    free(mess);
  }
  else {
    mess->consumed = 1;
  }

  if (pthread_cond_signal(&(channel->wait_for_free_space)) != 0) {
    fprintf(stderr, "\nErro ao desbloquear variável de condição\n");
    exit(1);
  }

  if (pthread_mutex_unlock(&(channel->mutex)) != 0) {
    fprintf(stderr, "\nErro ao desbloquear mutex\n");
    exit(1);
  }

  return copysize;
}

/*--------------------------------------------------------------------
  | Function: enviarMensagem
  | Description: Envia uma mensagem pelo canal correspondente caso
  | não exceda a capacidade do canal. Caso exceda, a tarefa que
  | chamou a função espera.
  ---------------------------------------------------------------------*/

int enviarMensagem(int tarefaOrig, int tarefaDest, void *msg, int tamanho) {
  Channel_t     *channel;
  Message_t     *mess;

  channel = (Channel_t*) channel_array[tarefaDest*number_of_tasks+tarefaOrig];
  mess = (Message_t*) malloc (sizeof(Message_t));

  if (mess == NULL) {
    fprintf(stderr, "\nErro ao alocar memória para mensagem\n");
    exit(1);
  }

  leQueElemInit (mess);
  mess->mess_size = tamanho;

  // If channels are buffered, copy message to a temporary buffer
  if (channel_capacity > 0) {
    mess->contents = malloc (tamanho);
    mess->consumed = 0;

    if(mess->contents == NULL) {
      fprintf(stderr, "\nErro ao alocar memória para o conteúdo da mensagem\n");
      exit(1);
    }

    memcpy(mess->contents, msg, tamanho);
  }
  // if channels are not buffered, message will be copied directly from sender to receiver
  else {
    mess->contents = msg;
    mess->consumed = 0;
  }

  if (pthread_mutex_lock(&(channel->mutex)) != 0) {
    fprintf(stderr, "\nErro ao bloquear mutex\n");
    exit(1);
  }

  // if channels are buffered, wait until there is buffer available
  if (channel_capacity > 0) {
    while (leQueSize(channel->message_list) >= channel_capacity) {
      if (pthread_cond_wait(&(channel->wait_for_free_space), &(channel->mutex)) != 0) {
        fprintf(stderr, "\nErro ao esperar pela variável de condição\n");
        exit(1);
      }
    }
  }

  leQueInsLast (channel->message_list, mess);

  if (pthread_cond_signal(&(channel->wait_for_messages)) != 0) {
    fprintf(stderr, "\nErro ao desbloquear variável de condição\n");
    exit(1);
  }

  // if channels are not buffered, wait for message to be read
  if (channel_capacity == 0) {
    while (!mess->consumed) {
      if(pthread_cond_wait(&(channel->wait_for_free_space), &(channel->mutex)) != 0) {
        fprintf(stderr, "\nErro ao esperar pela variável de condição\n");
        exit(1);
      }

    }
    free(mess);
  }

  if(pthread_mutex_unlock(&(channel->mutex)) != 0) {
    fprintf(stderr, "\nErro ao desbloquear mutex\n");
    exit(1);
  }

  return tamanho;
}
