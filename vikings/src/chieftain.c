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

    /* Aloca arrays auxiliares para rastrear quais pratos foram usados por cada viking */
    self->prato1_usado = (int*) malloc(sizeof(int) * config.table_size);
    self->prato2_usado = (int*) malloc(sizeof(int) * config.table_size);
    for (unsigned int i=0; i<config.table_size; i++){
        self->prato1_usado[i] = -1; /* nenhum prato usado ainda */
        self->prato2_usado[i] = -1;
    }

    /* Incializa o contador da barreira do banquete*/
    /* Quando banquet_counter == config.horde_size, o banquete terminou */
    self->banquet_counter = 0;

    /* Incializa os mutexes e condicoes da mesa e da barreira do banquete */
    pthread_mutex_init(&self->table_mutex, NULL); /* Protege seats e plates */
    pthread_cond_init(&self->table_cond, NULL);  /* Protege seats e plates */

    pthread_mutex_init(&self->barrier_mutex, NULL);  /* Protege banquet_counter */
    pthread_cond_init(&self->barrier_cond, NULL);   /* Notifica fim do banquete */

    /* Inicializa cache local de preces agendadas (sincronizado com valhalla->prayers) */
    for (int i = 0; i < NUMBER_OF_GODS; i++) {
        self->scheduled_prayers[i] = 0;
    }

    plog("[chieftain] Initialized\n");
}

int chieftain_acquire_seat_plates(chieftain_t *self, int berserker)
{
    /* Adquire mutex exclusivo da mesa para proteger acesso a seats e plates */
    pthread_mutex_lock(&self->table_mutex);

    int chair = -1;

    /* Loop até encontrar uma cadeira e dois pratos disponíveis */
    while (chair == -1) {
        /* Itera sobre todas as cadeiras da mesa circular */
        for (int i = 0; i < config.table_size; i++) {
            
            if (self->seats[i] != -1) continue; /* Pula se a cadeira já estiver ocupada */

            /* Verifica os vizinhos considerando o vão da mesa*/
            /* Se há vizinho ocupado do tipo oposto (normal vs berserker), rejeita */
            int esq_ok = 1;
            int dir_ok = 1;

             /* Verifica vizinho da esquerda (apenas se não for a primeira cadeira) */
            if (i > 0) { 
                if (self->seats[i - 1] != -1 && self->seats[i - 1] != berserker)
                    esq_ok = 0; /* Vizinho do tipo oposto encontrado */
            }
             /* Verifica vizinho da direita (apenas se não for a última cadeira) */
            if (i < config.table_size - 1) { 
                if (self->seats[i + 1] != -1 && self->seats[i + 1] != berserker)
                    dir_ok = 0; /* Vizinho do tipo oposto encontrado */
            }

            /* Se algum vizinho é do tipo oposto, rejeita esta cadeira */
            if (!esq_ok || !dir_ok) continue;

            /* Verifica pratos disponíveis usando módulo (mesa circular) */
            int p_esq = (i - 1 + config.table_size) % config.table_size;  /* Prato esquerdo*/ 
            int p_meio = i; /*prato do meio */
            int p_dir = (i + 1) % config.table_size;  /* Prato direito */
            
            /* Conta quantos pratos estão livres e armazena seus índices */
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

            /* Se encontrou cadeira e dois pratos livres, aloca-os */
            if (pratos_livres >= 2) {
                /* Ocupa a cadeira e os pratos, salvando o estado */
                chair = i; 
                
                self->seats[i] = berserker;
                self->plates[p1] = 1; /*Marca como ocupado */
                self->plates[p2] = 1;
                   
                /* Armazena quais pratos foram usados para liberação posterior */
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
    /* Libera mutex da mesa antes de retornar */
    pthread_mutex_unlock(&self->table_mutex);
    return chair;
}

void chieftain_release_seat_plates(chieftain_t *self, int pos)
{   
    /* Libera a cadeira e os pratos, acordando vikings em espera */
    pthread_mutex_lock(&self->table_mutex);
    self->seats[pos] = -1; //marca como vazia
    self->plates[self->prato1_usado[pos]] = 0; //marca como livre
    self->plates[self->prato2_usado[pos]] = 0;
    pthread_cond_broadcast(&self->table_cond); // notifica todos os vikings que estão esperando por uma cadeira ou prato
    pthread_mutex_unlock(&self->table_mutex);

    /* Incrementa o contador do banquete e notifica quando todos terminaram. */
    pthread_mutex_lock(&self->barrier_mutex);
    self->banquet_counter++;

    /* Se todos os NORMAL_VIKINGs já comeram, sinaliza fim do banquete */
    if (self->banquet_counter == config.horde_size)
        pthread_cond_broadcast(&self->barrier_cond);
    pthread_mutex_unlock(&self->barrier_mutex);
}

int is_valid_god(chieftain_t *self, god_t god) {
    /* Simula a adição da prece temporariamente */
    self->scheduled_prayers[god]++;
    
    int valid = 1;
    int total_normal = 0;

    /* Calcula total de preces dos 6 deuses normais (não super-deuses) */
    for (int i = 0; i < 6; i++) {
        total_normal += self->scheduled_prayers[i];
    }

    /* Valida cada deus contra suas restrições */
    for (int i = 0; i < NUMBER_OF_GODS; i++) {
        int count = self->scheduled_prayers[i];
        if (i < 6) {
            /* Valida limites do deus rival */ 
            int rival = (i % 2 == 0) ? i + 1 : i - 1;
            int rival_count = self->scheduled_prayers[rival];

            /* Calcula limite máximo permitido*/
            int tolerance = (int)ceil(rival_count * (1.0 + RIVAL_TOLERANCE_RATE));
            
            /* Garante mínimo de 1 prece se rival tem preces */
            int max_allowed = (1 > tolerance) ? 1 : tolerance; 
            /* Calcula limite mínimo*/
            int min_allowed = (int)floor(rival_count * (1.0 - RIVAL_TOLERANCE_RATE));
            
            /* Se deus ultrapassa tolerância, configura inválido */
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
    /* Coleta todos os deuses válidos em um array */
    god_t deuses[NUMBER_OF_GODS];
    int count = 0;
    for (int i = 0; i < NUMBER_OF_GODS; i++) {
        if (is_valid_god(self, i)) deuses[count++] = i;  /* Adiciona deus válido ao array */
    }

    god_t god;
    if (count > 0) {
        god = deuses[rand() % count];
    } else {
        /* se nenhum deus é válido, atribui ODIN (super-deus com tolerância alta) */
        god = ODIN;
    }
     
    /* Incrementa contador de preces agendadas para o deus escolhido */
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