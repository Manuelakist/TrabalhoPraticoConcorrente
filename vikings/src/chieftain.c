#include <stdlib.h>
#include "config.h"
#include "chieftain.h"
#include "valhalla.h"
#include <math.h>

void chieftain_init(chieftain_t *self, valhalla_t *valhalla)
{
    /* TODO: Adicionar código aqui se necessário! */
    pthread_mutex_init(&(self->mutex_deus), NULL);
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

int diferenca_deuses(chieftain_t *self, god_t god1, god_t god2) {
    int n1 = self->valhalla->prayers[god1];
    int n2 = self->valhalla->prayers[god2];
    int max = (n1 > n2) ? n1 : n2;

    int dif = abs(n1 - n2);
    int tol = (int)ceil(0.05 * max);

    return dif <= tol;
}

int deus_valido(chieftain_t *self, god_t god) {
    self->valhalla->prayers[god]++;
    int valido = 1;

    if (god % 2 == 0 && god < 6) {
        valido = diferenca_deuses(self, god, god+1);
    } else if (god % 2 == 1 && god < 6) {
        valido = diferenca_deuses(self, god, god-1);
    } else {
        int soma;
        for (int i = 0; i < 6; i++) {
            soma += self->valhalla->prayers[i];
        }
        int dif = self->valhalla->prayers[god] - soma;
        int tol = (int)ceil(soma * 0.1);
        valido = dif <= tol;
    }

    self->valhalla->prayers[god]--;
    return valido;
}

god_t chieftain_get_god(chieftain_t *self)
{
    god_t deuses[NUMBER_OF_GODS];
    int num_candidatos = 0;

    for (int i = 0; i < 8; i++) {
        pthread_mutex_lock(&(self->mutex_deus));
        if (deus_valido(self, i)) {
            deuses[num_candidatos++] = i;
        }
        pthread_mutex_unlock(&(self->mutex_deus));
    }
    
    god_t god = deuses[rand() % num_candidatos];

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
