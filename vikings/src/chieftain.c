#include <stdlib.h>
#include "config.h"
#include "chieftain.h"
#include "valhalla.h"

void chieftain_init(chieftain_t *self, valhalla_t *valhalla)
{
    /* TODO: Adicionar código aqui se necessário! */
 
    self->valhalla = valhalla;
    /* Aloca e incializa as cadeiras da mesa circular*/
    /* tabelinha: VAZIO = -1, VIKING_NORMAL =0, BERSERKER = 1 */
    self->seats = (int *) malloc(sizeof(int) * config.table_size);
    for (unsigned int i=0; i<config.table_size; i++){
        self->seats[i] = -1; /* incia com vazio*/
    }

    /* Aloca e incializa os pratos da mesa circular*/
    /* tabelinha: LIVRE = 0, OCUPADO = 1 */
    self->plates = (int*) malloc(sizeof(int) * config.table_size);
    for (unsigned int i=0; i<config.table_size; i++){
        self->plates[i] = 0; /* inicia com livre*/
    }
    /*incializa o contador da berreira do banquete*/
    self->banquet_counter = 0;

    /*incializa os mutexes e condicoes da mesa e da berreira do banquete */
    pthread_mutex_init(&self->table_mutex, NULL);
    pthread_cond_init(&self->table_cond, NULL);

    pthread_mutex_init(&self->barrier_mutex, NULL);
    pthread_cond_init(&self->barrier_cond, NULL);

    plog("[chieftain] Initialized\n");
}

int chieftain_acquire_seat_plates(chieftain_t *self, int berserker)
{
    /* TODO: Implementar! */
    return 1;
}

void chieftain_release_seat_plates(chieftain_t *self, int pos)
{
    /* TODO: Implementar! */
}

god_t chieftain_get_god(chieftain_t *self)
{
    /* TODO: Implementar! O código abaixo deve ser modificado! */
    god_t god = THOR;
    
    return god;
}

void chieftain_finalize(chieftain_t *self)
{
    /* TODO: Adicionar código aqui se necessário! */
    /*destroi os mutexes e condições */
    pthread_mutex_destroy(&self->table_mutex);
    pthread_cond_destroy(&self->table_cond);

    pthread_mutex_destroy(&self->barrier_mutex);
    pthread_cond_destroy(&self->barrier_cond);

    /*libera a memória dos pratos e cadeiras*/
    free(self->seats);
    free(self->plates);
    
    plog("[chieftain] Finalized\n");
}
