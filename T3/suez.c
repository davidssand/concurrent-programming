#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <stdarg.h>

pthread_mutex_t mutex_print = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_chanel = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_map = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_chanel = PTHREAD_COND_INITIALIZER;

const int map_length = 40;
int map[map_length];
int chanel[2] = {10, 13};
bool chanel_in_use = false;

struct boat {
    int pos_x;
    bool after_chanel;
};

const int n_boats = 7;
pthread_t boats_th[n_boats];
pthread_t print_th;
struct boat boats[n_boats];

void safe_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

bool after_chanel_entrance(i) {
    return (i >= chanel[0]);
}

bool in_chanel(i) {
    return (after_chanel_entrance(i) && (i <= chanel[1]));
}

void print_map(){
    safe_printf("> ");
    for (int i = 0; i < map_length; i++) {
        if (in_chanel(i)) safe_printf("\033[0;31m"); 
        safe_printf("%d ", map[i]);
        if (in_chanel(i)) safe_printf("\033[0m");

    }
    safe_printf(">\n\n");
}

void update_map(int current_pos_x, int new_pos_x) {
    pthread_mutex_lock(&mutex_map);
    map[current_pos_x] = 0;
    map[new_pos_x] = 1;
    pthread_mutex_unlock(&mutex_map);
}

void update_chanel_in_use(struct boat *boat, int new_pos_x) {
    if (!boat->after_chanel){
        bool old_chanel_in_use = chanel_in_use;
        chanel_in_use = in_chanel(new_pos_x);
        if (old_chanel_in_use && !chanel_in_use) {
            pthread_cond_broadcast(&cond_chanel);
            boat->after_chanel = true;
        }
        // safe_printf("Old Chanel in use? %s\n", old_chanel_in_use ? "true" : "false");
        // safe_printf("Chanel in use? %s\n", chanel_in_use ? "true" : "false");
    }
}

bool nothing_at(int new_pos_x) {
    return !map[new_pos_x];
}

void *move_boat(struct boat *boat) {
    while (1) {
        // safe_printf("-----");
        int current_pos_x = boat->pos_x;
        int new_pos_x = current_pos_x + 1;
        // safe_printf("Current pos %d\n", current_pos_x);
        // safe_printf("%d\n", new_pos_x);

        pthread_mutex_lock(&mutex_chanel);
        if (nothing_at(new_pos_x)){
            bool boat_after_chanel_entrance = after_chanel_entrance(new_pos_x);
            if (boat_after_chanel_entrance) update_chanel_in_use(boat, new_pos_x);

            while (chanel_in_use && !boat_after_chanel_entrance) {
                pthread_cond_wait(&cond_chanel, &mutex_chanel);
            }
            update_map(current_pos_x, new_pos_x);
            boat->pos_x = new_pos_x;
        }
        pthread_mutex_unlock(&mutex_chanel);
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
        boats[i].after_chanel = false;
    }
}

void *print_thread_function(void *arg) {
	while(1) {
        safe_printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
        print_map();
		usleep(250000);
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