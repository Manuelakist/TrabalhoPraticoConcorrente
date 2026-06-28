#include <stdlib.h>
#include "config.h"
#include "chieftain.h"
#include "valhalla.h"
#include <math.h>

void chieftain_init(chieftain_t *self, valhalla_t *valhalla)
{   
    /* Inicializa mutex para a escolha dos deuses */
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

    /* Incializa o contador da barreira do banquete*/
    self->banquet_counter = 0;

    /* Incializa os mutexes e condicoes da mesa e da barreira do banquete */
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
            
            if (self->seats[i] != -1) continue; /* Pula se a cadeira já estiver ocupada */

            /* Verifica os vizinhos considerando o vão da mesa*/
            int esq_ok = 1;
            int dir_ok = 1;

            if (i > 0) { 
                if (self->seats[i - 1] != -1 && self->seats[i - 1] != berserker)
                    esq_ok = 0;
            }
            if (i < config.table_size - 1) { 
                if (self->seats[i + 1] != -1 && self->seats[i + 1] != berserker)
                    dir_ok = 0;
            }
            
            if (!esq_ok || !dir_ok) continue;

            /* Verifica pratos disponíveis usando módulo (mesa circular) */
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
                /* Ocupa a cadeira e os pratos, salvando o estado */
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
            /* Aguarda liberação de assentos ou pratos para tentar novamente */
            pthread_cond_wait(&self->table_cond, &self->table_mutex);
        }
    }

    pthread_mutex_unlock(&self->table_mutex);
    return chair;
}

void chieftain_release_seat_plates(chieftain_t *self, int pos)
{   
    /* Libera a cadeira e os pratos, acordando vikings em espera */
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
    /* Simula a adição da prece temporariamente */
    self->scheduled_prayers[god]++;
    
    int valid = 1;
    int total_normal = 0;
    
    for (int i = 0; i < 6; i++) {
        total_normal += self->scheduled_prayers[i];
    }
    
    for (int i = 0; i < NUMBER_OF_GODS; i++) {
        int count = self->scheduled_prayers[i];
        if (i < 6) {
            /* Valida limites do deus rival */ 
            int rival = (i % 2 == 0) ? i + 1 : i - 1;
            int rival_count = self->scheduled_prayers[rival];

            int tolerance = (int)ceil(rival_count * (1.0 + RIVAL_TOLERANCE_RATE));
            
            int max_allowed = (1 > tolerance) ? 1 : tolerance;
            int min_allowed = (int)floor(rival_count * (1.0 - RIVAL_TOLERANCE_RATE));
            
            if (count < min_allowed || count > max_allowed) {
                valid = 0;
                break;
            }
        } else { 
            /* Valida limites dos super deuses */
            if (count > ceil(total_normal * (1.0 + SUPER_GOD_TOLERANCE_RATE))) {
                valid = 0;
                break;
            }
        }
    }
    /* Desfaz a simulação */
    self->scheduled_prayers[god]--;
    return valid;
}

god_t chieftain_get_god(chieftain_t *self)
{
    /* Barreira: todos os vikings (normais e atrasados) devem esperar o banquete
       terminar antes de rezar. */
    pthread_mutex_lock(&self->barrier_mutex);
    while (self->banquet_counter < config.horde_size)
        pthread_cond_wait(&self->barrier_cond, &self->barrier_mutex);
    pthread_mutex_unlock(&self->barrier_mutex);

    /* Região crítica para escolha aleatória de um deus válido */
    pthread_mutex_lock(&(self->mutex_deus));
    god_t deuses[NUMBER_OF_GODS];
    int count = 0;
    for (int i = 0; i < NUMBER_OF_GODS; i++) {
        if (is_valid_god(self, i)) deuses[count++] = i;
    }
    god_t god;
    if (count > 0) {
        god = deuses[rand() % count];
    } else {
        god = ODIN;
    }
    self->scheduled_prayers[god]++;
    pthread_mutex_unlock(&(self->mutex_deus));
    return god;
}

void chieftain_finalize(chieftain_t *self)
{
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