#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define NUM_REINDEER 9
#define NUM_ELVES 10
#define NUM_REQUIRED_REINDEER 9
#define NUM_REQUIRED_ELVES 3

#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_GREEN   "\x1b[32m"

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cnd_santa = PTHREAD_COND_INITIALIZER;
pthread_cond_t cnd_reindeer = PTHREAD_COND_INITIALIZER;
pthread_cond_t cnd_elves = PTHREAD_COND_INITIALIZER;
pthread_cond_t cnd_wait = PTHREAD_COND_INITIALIZER;

int reindeer_waiting = 0, elves_waiting = 0, reindeer_arrived = 0, elves_arrived = 0;

void *santa() {
    while (1) {
        pthread_mutex_lock(&mtx);

        // Santa is sleeping until required number of reindeer or elves are waiting
        while (reindeer_waiting != NUM_REQUIRED_REINDEER && elves_waiting != NUM_REQUIRED_ELVES) {
            printf(ANSI_COLOR_RED "\nSanta is sleeping...\n" ANSI_COLOR_RESET); fflush(stdout);
            pthread_cond_wait(&cnd_santa, &mtx);
        } 
        
        while (reindeer_waiting >= NUM_REQUIRED_REINDEER || elves_waiting >= NUM_REQUIRED_ELVES) {
            if (reindeer_waiting >= NUM_REQUIRED_REINDEER) {
                reindeer_arrived = reindeer_waiting;
                printf(ANSI_COLOR_CYAN "\nReindeer woke up Santa\n" ANSI_COLOR_RESET); fflush(stdout);
                printf(ANSI_COLOR_RED "Santa harnesses the reindeer to the sleigh...\n" ANSI_COLOR_RESET); fflush(stdout);
                sleep(1);
                printf(ANSI_COLOR_RED "Santa is delivering toys...\n" ANSI_COLOR_RESET); fflush(stdout);
                sleep(3);
                printf(ANSI_COLOR_RED "Santa finished delivering toys and is unharnessing the reindeer...\n" ANSI_COLOR_RESET); fflush(stdout);

                // Reindeers can go rest
                pthread_cond_broadcast(&cnd_reindeer);
                reindeer_waiting -= reindeer_arrived;

                // Wait until all reindeers are resting
                while (reindeer_arrived > 0) {
                    pthread_cond_wait(&cnd_wait, &mtx);
                }

                printf(ANSI_COLOR_CYAN "All reindeer went to rest.\n"); fflush(stdout);

            } else if (elves_waiting >= NUM_REQUIRED_ELVES) {
                elves_arrived = elves_waiting;
                printf(ANSI_COLOR_GREEN "\nElves woke up Santa\n" ANSI_COLOR_RESET); fflush(stdout);
                printf(ANSI_COLOR_RED "Santa is bringing elves to his office...\n" ANSI_COLOR_RESET); fflush(stdout);
                sleep(1);
                printf(ANSI_COLOR_RED "Santa is helping elves...\n" ANSI_COLOR_RESET); fflush(stdout);
                sleep(3);
                printf(ANSI_COLOR_RED "Santa says goodbye to elves...\n" ANSI_COLOR_RESET); fflush(stdout);
            
                // Elves can go back to work
                pthread_cond_broadcast(&cnd_elves);
                elves_waiting -= elves_arrived;
            
                // Wait until all elves are working
                while (elves_arrived > 0) {
                    pthread_cond_wait(&cnd_wait, &mtx);
                }

                printf(ANSI_COLOR_GREEN "All elves went back to work.\n" ANSI_COLOR_RESET); fflush(stdout);
            }

        }

        pthread_mutex_unlock(&mtx);
    }

    return NULL;
}

void *reindeer(void *arg) {
    int reindeer_id = *((int *)arg);

    while (1) {
        printf(ANSI_COLOR_CYAN "Waiting: reindeer %d\n" ANSI_COLOR_RESET, reindeer_id); fflush(stdout);

        pthread_mutex_lock(&mtx);

        if (++reindeer_waiting == NUM_REQUIRED_REINDEER) {
            // Signal Santa to wake up
            pthread_cond_signal(&cnd_santa);
        } 

        // Wait until toys are delivered
        pthread_cond_wait(&cnd_reindeer, &mtx);

        // Signal Santa that reindeer is resting
        reindeer_arrived--;
        pthread_cond_signal(&cnd_wait);

        pthread_mutex_unlock(&mtx);

        // Simulate resting
        sleep((rand() % 5) + 1);
    }

    return NULL;
}

void *elf(void *arg) {
    int elf_id = *((int *)arg);

    while (1) {
        printf(ANSI_COLOR_GREEN "Waiting: elf %d\n" ANSI_COLOR_RESET, elf_id); fflush(stdout);

        pthread_mutex_lock(&mtx);

        if (++elves_waiting == NUM_REQUIRED_ELVES) {
            // Signal Santa to wake up
            pthread_cond_signal(&cnd_santa);
        }

        // Wait until consultation is over
        pthread_cond_wait(&cnd_elves, &mtx);

        // Signal Santa that elf went back to work
        elves_arrived--;
        pthread_cond_signal(&cnd_wait);

        pthread_mutex_unlock(&mtx);

        // Simulate working time
        sleep((rand() % 5) + 1);
    }

    return NULL;
}

int main() {
    pthread_t santa_thread, reindeer_threads[NUM_REINDEER], elves_threads[NUM_ELVES];
    int reindeer_arr[NUM_REINDEER], elves_arr[NUM_ELVES];

    srand(time(NULL));

    // Initialize reindeer and elves arrays
    for (int i = 0; i < NUM_REINDEER; i++)
        reindeer_arr[i] = i + 1;
    for (int i = 0; i < NUM_ELVES; i++)
        elves_arr[i] = i + 1;

    // Create Santa thread
    pthread_create(&santa_thread, NULL, santa, NULL);

    // Create reindeer threads
    for (int i = 0; i < NUM_REINDEER; i++)
        pthread_create(&reindeer_threads[i], NULL, reindeer, &reindeer_arr[i]);

    // Create elves threads
    for (int i = 0; i < NUM_ELVES; i++)
        pthread_create(&elves_threads[i], NULL, elf, &elves_arr[i]);

    pthread_join(santa_thread, NULL);

    for (int i = 0; i < NUM_REINDEER; i++)
        pthread_join(reindeer_threads[i], NULL);

    for (int i = 0; i < NUM_ELVES; i++)
        pthread_join(elves_threads[i], NULL);

    return 0;
}
