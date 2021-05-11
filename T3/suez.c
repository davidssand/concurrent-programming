#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

pthread_mutex_t mutex_chanel = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_fuel = PTHREAD_COND_INITIALIZER;

const int map_length = 40;
int map[map_length];
int chanel[2] = {20, 35};

struct boat {
    int pos_x;
};

const int n_boats = 3;
pthread_t boats_th[n_boats];
pthread_t print_th;
struct boat boats[n_boats];

void print_map(){
    printf("> ");
    for (int i = 0; i < map_length; i++) {
        if ((i >= chanel[0]) && (i <= chanel[1])) printf("\033[0;31m"); 
        printf("%d ", map[i]);
        if ((i > chanel[0]) && (i <= chanel[1])) printf("\033[0m");

    }
    printf(">\n\n");
}

void update_map(int old_pos_x, int new_pos_x) {
    pthread_mutex_lock(&mutex_chanel);
    map[old_pos_x] = 0;
    map[new_pos_x] = 1;
    pthread_mutex_unlock(&mutex_chanel);
}

void *move_boat(struct boat *boat) {
    // printf("-------------");
    while (1) {
        int old_pos_x = boat->pos_x;
        int new_pos_x = old_pos_x + 1;
        bool can_move = !map[new_pos_x];
        if (can_move){
            boat->pos_x = boat->pos_x + 1;
            update_map(old_pos_x, boat->pos_x);
        }
		usleep(500000);
    }
}

void *move_boat_thread_function(void *arg) {
    move_boat(arg);
}

void initialize_map() {
    for (int i = 0; i < map_length; i++) {
        map[i] = 0;
    }

    for (int i = 0; i < n_boats; i++) {
        if ((i - 1) >= 0) boats[i].pos_x = boats[i - 1].pos_x + 1;
        else boats[i].pos_x = 0;
        map[boats[i].pos_x] = 1;
    }
}

void *print_thread_function(void *arg) {
	while(1) {
        print_map();
		usleep(500000);
    }
}

int main(int argc, char* argv[]) {
    initialize_map();
    
    for (int i = 0; i < n_boats; i++) {
        pthread_create(&boats_th[i], NULL, &move_boat_thread_function, &boats[i]);
    }
    pthread_create(&print_th, NULL, &print_thread_function, NULL);

    for (int i = 0; i < n_boats; i++) {
        pthread_join(boats_th[i], NULL);
    }
    pthread_join(print_th, NULL);
}