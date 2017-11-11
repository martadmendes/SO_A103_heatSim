/*
// Biblioteca de troca de mensagens, versao 4
// Sistemas Operativos, DEI/IST/ULisboa 2017-18
*/

#ifndef MPLIB_H
#define MPLIB_H

int inicializarMPlib(int capacidade_de_cada_canal, int ntasks);
void libertarMPlib();

int receberMensagem(int tarefaOrig, int tarefaDest, void *buffer, int tamanho);
int enviarMensagem(int tarefaOrig, int tarefaDest, void *msg, int tamanho);

#endif
