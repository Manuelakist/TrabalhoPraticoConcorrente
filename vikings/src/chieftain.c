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

    self->prato1_usado = (int*) malloc(sizeof(int) * config.table_size);
    self->prato2_usado = (int*) malloc(sizeof(int) * config.table_size);
    for (unsigned int i=0; i<config.table_size; i++){
        self->prato1_usado[i] = -1;
        self->prato2_usado[i] = -1;
    }

    /*incializa o contador da berreira do banquete*/
    self->banquet_counter = 0;

    /*incializa os mutexes e condicoes da mesa e da berreira do banquete */
    pthread_mutex_init(&self->table_mutex, NULL);
    pthread_cond_init(&self->table_cond, NULL);

    pthread_mutex_init(&self->barrier_mutex, NULL);
    pthread_cond_init(&self->barrier_cond, NULL);

    for (int i = 0; i < NUMBER_OF_GODS; i++) {
        self->scheduled_prayers[i] = 0;
    }

    plog("[chieftain] Initialized\n");
}

int chieftain_acquire_seat_plates(chieftain_t *self, int berserker)
{
    pthread_mutex_lock(&self->table_mutex);

    int chair = -1;

    while (chair == -1) {
        for (int i = 0; i < config.table_size; i++) {
            
            if (self->seats[i] != -1) continue;

            int esq_ok = 1;
            int dir_ok = 1;

            /* Mesa é circular: usar módulo para verificar os vizinhos nas extremidades */
            int left  = (i - 1 + config.table_size) % config.table_size;
            int right = (i + 1) % config.table_size;

            if (self->seats[left]  != -1 && self->seats[left]  != berserker)
                esq_ok = 0;
            if (self->seats[right] != -1 && self->seats[right] != berserker)
                dir_ok = 0;
            
            if (!esq_ok || !dir_ok) continue;

            int p_esq = (i - 1 + config.table_size) % config.table_size;
            int p_meio = i;
            int p_dir = (i + 1) % config.table_size;

            int pratos_livres = 0;
            int p1 = -1, p2 = -1;

            if (self->plates[p_esq] == 0) { pratos_livres++; p1 = p_esq; }
            if (self->plates[p_meio] == 0) { 
                pratos_livres++; 
                if (p1 == -1) p1 = p_meio; else p2 = p_meio; 
            }
            if (self->plates[p_dir] == 0 && pratos_livres < 2) { 
                pratos_livres++; 
                p2 = p_dir; 
            }

            if (pratos_livres >= 2) {
                chair = i;
                
                self->seats[i] = berserker;
                self->plates[p1] = 1;
                self->plates[p2] = 1;
                
                self->prato1_usado[i] = p1;
                self->prato2_usado[i] = p2;
                
                break; 
            }
        }

        if (chair == -1) {
            pthread_cond_wait(&self->table_cond, &self->table_mutex);
        }
    }

    pthread_mutex_unlock(&self->table_mutex);
    return chair;
}

void chieftain_release_seat_plates(chieftain_t *self, int pos)
{
    pthread_mutex_lock(&self->table_mutex);
    self->seats[pos] = -1;
    self->plates[self->prato1_usado[pos]] = 0;
    self->plates[self->prato2_usado[pos]] = 0;
    pthread_cond_broadcast(&self->table_cond);
    pthread_mutex_unlock(&self->table_mutex);

    /* Incrementa o contador do banquete e notifica quando todos terminaram. */
    pthread_mutex_lock(&self->barrier_mutex);
    self->banquet_counter++;
    if (self->banquet_counter == config.horde_size)
        pthread_cond_broadcast(&self->barrier_cond);
    pthread_mutex_unlock(&self->barrier_mutex);
}

int is_valid_god(chieftain_t *self, god_t god) {
    self->scheduled_prayers[god]++;
    int is_valid = 1;
    if (god < 6) {
        int partner = (god % 2 == 0) ? god + 1 : god - 1;
        int diff = abs(self->scheduled_prayers[god] - self->scheduled_prayers[partner]);
        int max = (self->scheduled_prayers[god] > self->scheduled_prayers[partner]) ? self->scheduled_prayers[god] : self->scheduled_prayers[partner];
        is_valid = (diff <= ceil(0.05 * max));
    } else {
        int sum = 0;
        for (int i = 0; i < 6; i++) {
            sum += self->scheduled_prayers[i];
        }
        is_valid = ((self->scheduled_prayers[god] - sum) <= (int)ceil(sum * 0.1));
    }
    self->scheduled_prayers[god]--;
    return is_valid;
}

god_t chieftain_get_god(chieftain_t *self)
{
    /* Barreira: todos os vikings (normais e atrasados) devem esperar o banquete
       terminar antes de rezar. */
    pthread_mutex_lock(&self->barrier_mutex);
    while (self->banquet_counter < config.horde_size)
        pthread_cond_wait(&self->barrier_cond, &self->barrier_mutex);
    pthread_mutex_unlock(&self->barrier_mutex);

    pthread_mutex_lock(&(self->mutex_deus));
    god_t deuses[NUMBER_OF_GODS];
    int count = 0;
    for (int i = 0; i < NUMBER_OF_GODS; i++) {
        if (is_valid_god(self, i)) deuses[count++] = i;
    }
    god_t god = deuses[rand() % count];
    self->scheduled_prayers[god]++;
    pthread_mutex_unlock(&(self->mutex_deus));
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

    free(self->prato1_usado);
    free(self->prato2_usado);
    pthread_mutex_destroy(&(self->mutex_deus));
    
    plog("[chieftain] Finalized\n");
}