#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>


pthread_mutex_t mutex_print = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_chanel = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_map = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_chanel = PTHREAD_COND_INITIALIZER;

const int map_length = 40;
char map[map_length];
int chanel[2] = {15, 20};
bool chanel_in_use = false;

char boat_symbol = '&';
char water_symbol = '_';

int vel_min = 50000;
int vel_max = 500000;

struct boat {
    int pos_x;
    bool after_chanel;
    int vel;
};

const int n_boats = 7;
pthread_t boats_th[n_boats];
pthread_t print_th;
pthread_t gate_th;
struct boat boats[n_boats];

void safe_printf(const char *fmt, ...) {
    pthread_mutex_lock(&mutex_print);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    pthread_mutex_unlock(&mutex_print);
}

bool after_chanel_entrance(i) {
    return (i >= chanel[0]);
}

bool in_chanel(i) {
    return (after_chanel_entrance(i) && (i <= chanel[1]));
}

void print_map(){
    pthread_mutex_lock(&mutex_print);
    printf("> ");
    for (int i = 0; i < map_length; i++) {
        if (in_chanel(i)) printf("\033[0;31m"); 
        printf("%c ", map[i]);
        if (in_chanel(i)) printf("\033[0m");

    }
    printf(">\n\n");
    pthread_mutex_unlock(&mutex_print);
}

void update_map(int current_pos_x, int new_pos_x) {
    pthread_mutex_lock(&mutex_map);
    map[current_pos_x] = water_symbol;
    map[new_pos_x] = boat_symbol;
    pthread_mutex_unlock(&mutex_map);
}

void *gate_thread_function(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex_chanel);
        bool old_chanel_in_use = chanel_in_use;

        bool any_boat_in_chanel = false;
        for (int i = 0; i < n_boats; i++) {
            if (in_chanel(boats[i].pos_x)) any_boat_in_chanel = true;
        }
        chanel_in_use = any_boat_in_chanel;

        if (old_chanel_in_use && !chanel_in_use) pthread_cond_broadcast(&cond_chanel);
        pthread_mutex_unlock(&mutex_chanel);
		usleep(vel_min);
    }
}

bool nothing_at(int new_pos_x) {
    return map[new_pos_x] != boat_symbol;
}

void wait_for_signal(pos_x) {
    pthread_mutex_lock(&mutex_chanel);
    while (chanel_in_use && !after_chanel_entrance(pos_x)) {    
        pthread_cond_wait(&cond_chanel, &mutex_chanel);
    }
    pthread_mutex_unlock(&mutex_chanel);
}

void *move_boat(struct boat *boat) {
    while (1) {
        int current_pos_x = boat->pos_x;
        int new_pos_x = current_pos_x + 1;

        if (nothing_at(new_pos_x)){
            if (new_pos_x == chanel[0]) {
                wait_for_signal(current_pos_x);
            }
            update_map(current_pos_x, new_pos_x);
            boat->pos_x = new_pos_x;
        }
		usleep(boat->vel);
    }
}

void *move_boat_thread_function(void *arg) {
    move_boat(arg);
}

void initialize_map() {
    for (int i = 0; i < map_length; i++) {
        map[i] = water_symbol;
    }

    for (int i = 0; i < n_boats; i++) {
        if ((i - 1) >= 0) boats[i].pos_x = boats[i - 1].pos_x + 1;
        else boats[i].pos_x = 0;
        map[boats[i].pos_x] = boat_symbol;
        boats[i].after_chanel = false;
        boats[i].vel = (rand() % (vel_max - vel_min + 1)) + vel_min;
        safe_printf("%d\n", boats[i].vel);
        
    }
}

void *print_thread_function(void *arg) {
	while(1) {
        safe_printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
        print_map();
		usleep(vel_min);
    }
}

void create_threads() {
    for (int i = 0; i < n_boats; i++) {
        pthread_create(&boats_th[i], NULL, &move_boat_thread_function, &boats[i]);
    }
    pthread_create(&print_th, NULL, &print_thread_function, NULL);
    pthread_create(&gate_th, NULL, &gate_thread_function, NULL);
}

void join_threads() {
    for (int i = 0; i < n_boats; i++) {
        pthread_join(boats_th[i], NULL);
    }
    pthread_join(print_th, NULL);
    pthread_join(gate_th, NULL);
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    int r = rand();

    initialize_map();
    
    create_threads();
    join_threads();
}