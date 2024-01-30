#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>

#define MIN_PRICE 1
#define MAX_PRICE 5
#define INITIAL_BALANCE 5
#define NUM_SMOKERS 3

#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_CYAN "\x1b[36m"

struct Message {
    long mtype;
    int mvalue[2]; // [0] - sender, [1] - cost of ingredient
};

// Semaphore functions
static struct sembuf buf;

void V(int semid, int semnum) {
    buf.sem_num = semnum;
    buf.sem_op = 1;
    buf.sem_flg = 0;
    if (semop(semid, &buf, 1) == -1) {
        perror("Semaphore: signal");
        exit(1);
    }
}

void P(int semid, int semnum) {
    buf.sem_num = semnum;
    buf.sem_op = -1;
    buf.sem_flg = 0;
    if (semop(semid, &buf, 1) == -1) {
        perror("Semaphore: wait");
        exit(1);
    }
}

void agent(int semid_price_change, int semid_agent, int semid_smokers, int semid_balance, volatile int *prices, volatile int *balances) {
    while(1) {
        // Wait until smoker is done smoking
        P(semid_agent, 0);

        // The agent is changing prices so no one can read them
        P(semid_price_change, 0);

        // The prices are being changed until some smoker can afford the ingredients
        bool can_afford = false;
        while (!can_afford) {
            // Generate new prices of ingredients
            for (int i = 0; i < NUM_SMOKERS; i++) {
                prices[i] = rand() % (MAX_PRICE - MIN_PRICE + 1) + MIN_PRICE;
            }

            printf("New prices: tobacco: %d, paper: %d, matches: %d\n", prices[0], prices[1], prices[2]);
            
            // Check if some smoker can afford the ingredients
            int cost = 0;

            for (int smoker_id = 0; smoker_id < NUM_SMOKERS; smoker_id++) {
                cost = 0;
                // Calculate cost of ingredients for the current smoker
                for (int ingredient_id = 0; ingredient_id < NUM_SMOKERS; ingredient_id++) {
                    if (ingredient_id != smoker_id) {
                        cost += prices[ingredient_id];
                    }
                }

                // Get balance of the current smoker
                P(semid_balance, smoker_id);
                int smoker_balance = balances[smoker_id];
                V(semid_balance, smoker_id);
        
                // If the smoker can afford the ingredients, signal him
                if (cost <= smoker_balance) {
                    V(semid_smokers, smoker_id);
                    printf("Smoker %d can afford the ingredients\n", smoker_id);
                    can_afford = true;
                    break;
                }
            }
        }

        // The agent is done changing prices so smokers can read them
        V(semid_price_change, 0);
    }
}

void smoker(int smoker_id, int semid_price_change, int semid_agent, int semid_smokers, int semid_balance, int semid_transaction, int msgid, volatile int *prices, volatile int *balances) {
    struct Message msg;

    if (fork() == 0) {  // Child process (smoker-buyer)
        while(1) {
            // Wait until smoker can afford the ingredients (signaled by the agent)
            P(semid_smokers, smoker_id);

            // Agent can't change the prices until smoker is done smoking
            P(semid_price_change, 0);

            // Buy the ingredients from the other smokers
            for (int i = 0; i < NUM_SMOKERS; i++) {
                if (i != smoker_id) {
                    int seller = i;
                    int cost = prices[seller];

                    msg.mtype = seller + 1;
                    msg.mvalue[0] = smoker_id;
                    msg.mvalue[1] = cost;

                    // Subtract the cost of ingredient from the balance
                    P(semid_balance, smoker_id);
                    balances[smoker_id] -= cost;
                    V(semid_balance, smoker_id);

                    // Send message to the seller (begin the transaction)
                    if (msgsnd(msgid, &msg, sizeof(msg.mvalue), 0) == -1) {
                        perror(ANSI_COLOR_GREEN "Sending message to seller" ANSI_COLOR_RESET);
                        exit(1);
                    }
                    
                    printf(ANSI_COLOR_GREEN "Smoker %d sent %d to smoker %d\n" ANSI_COLOR_RESET, smoker_id, cost, seller);

                    
                }
            }

            // Wait for all transactions to be done before smoking the cigarette
            for (int i = 0; i < NUM_SMOKERS - 1; i++) {
                P(semid_transaction, 0);
            }

            // Smoke the cigarette
            printf(ANSI_COLOR_GREEN "Smoker %d is smoking...\n" ANSI_COLOR_RESET, smoker_id);
            sleep(3);

            // Print balances of all smokers after the transactions
            printf("Balances after transaction: smoker 0: %d, smoker 1: %d, smoker 2: %d\n\n", balances[0], balances[1], balances[2]);
        
            // Agent can change the prices again
            V(semid_price_change, 0);
            V(semid_agent, 0);
        }
    } else {  // Parent process (smoker-seller)
        while(1) {
            // Wait for the message from the buyer
            if (msgrcv(msgid, &msg, sizeof(msg.mvalue), smoker_id + 1, 0) == -1) {
                perror("Receiving message from buyer");
                exit(1);
            }

            // Add received money to the balance
            P(semid_balance, smoker_id);
            balances[smoker_id] += msg.mvalue[1];
            V(semid_balance, smoker_id);

            printf(ANSI_COLOR_CYAN "Smoker %d received %d from smoker %d\n" ANSI_COLOR_RESET, smoker_id, msg.mvalue[1], msg.mvalue[0]);

            // Signal the buyer that the transaction is done
            V(semid_transaction, 0);
        }
    }
}

int main() {
    // Initialize random number generator
    srand(time(NULL));

    // SHARED MEMORY

    // Create shared memory for balances of smokers
    int shmid_balances = shmget(IPC_PRIVATE, NUM_SMOKERS * sizeof(int), IPC_CREAT | 0600);
    if (shmid_balances == -1) {
        perror("Creating shared memory");
        exit(1);
    }
    volatile int *balances = shmat(shmid_balances, NULL, 0);
    if (balances == NULL) {
        perror("Attaching shared memory");
        exit(1);
    }

    // Initilize balances of smokers
    for (int i = 0; i < NUM_SMOKERS; i++) {
        balances[i] = INITIAL_BALANCE;
    }

    printf("Initial balances: smoker 0: %d, smoker 1: %d, smoker 2: %d\n", balances[0], balances[1], balances[2]);

    // Create shared memory for prices of ingredients
    int shmid_prices = shmget(IPC_PRIVATE, NUM_SMOKERS * sizeof(int), IPC_CREAT | 0600);
    if (shmid_prices == -1) {
        perror("Creating shared memory");
        exit(1);
    }
    volatile int *prices = shmat(shmid_prices, NULL, 0);
    if (prices == NULL) {
        perror("Attaching shared memory");
        exit(1);
    }

    // Initialize prices of ingredients
    for (int i = 0; i < NUM_SMOKERS; i++) {
        prices[i] = rand() % (MAX_PRICE - MIN_PRICE + 1) + MIN_PRICE;
    }

    // SEMAPHORES

    // Create semaphore for agent
    int semid_agent = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    if (semid_agent == -1) {
        perror("Creating semaphore");
        exit(1);
    }
    semctl(semid_agent, 0, SETVAL, 1);

    // Create semaphores for smokers
    int semid_smokers = semget(IPC_PRIVATE, NUM_SMOKERS, IPC_CREAT | 0600);
    if (semid_smokers == -1) {
        perror("Creating array of semaphores");
        exit(1);
    }
    for (int i = 0; i < NUM_SMOKERS; i++) {
        semctl(semid_smokers, i, SETVAL, 0);
    }

    // Create semaphore for price change
    int semid_price_change = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    if (semid_price_change == -1) {
        perror("Creating semaphore");
        exit(1);
    }
    semctl(semid_price_change, 0, SETVAL, 1);

    // Create semaphores for balance
    int semid_balance = semget(IPC_PRIVATE, NUM_SMOKERS, IPC_CREAT | 0600);
    if (semid_balance == -1) {
        perror("Creating array of semaphores");
        exit(1);
    }
    for (int i = 0; i < NUM_SMOKERS; i++) {
        semctl(semid_balance, i, SETVAL, 1);
    }

    // Create semaphore for transactions
    int semid_transaction = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    if (semid_transaction == -1) {
        perror("Creating semaphore");
        exit(1);
    }
    semctl(semid_transaction, 0, SETVAL, 0);

    // MESSAGE QUEUE

    // Create message queue
    int msgid = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    if (msgid == -1) {
        perror("Creating message queue");
        exit(1);
    }

    // Create smokers processes
    for (int i = 0; i < NUM_SMOKERS; i++) {
        if (fork() == 0) {
            smoker(i, semid_price_change, semid_agent, semid_smokers, semid_balance, semid_transaction, msgid, prices, balances);
            exit(0);
        }
    }

    // Agent process
    agent(semid_price_change, semid_agent, semid_smokers, semid_balance, prices, balances);

    return 0;
}
