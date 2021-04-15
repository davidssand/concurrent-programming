// David Steiner Sand
// 17100655

/*  Programa de demonstração de uso de sockets UDP em C no Linux
 *  Funcionamento:
 *  O programa cliente envia uma msg para o servidor. Essa msg é uma palavra.
 *  O servidor envia outra palavra como resposta.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>

#define NSEC_PER_SEC (1000000000) /* The number of nsecs per sec. */

#define FALHA 1

struct controller_setup {
	char *prefix;
	float overflow;
	float underflow;

	// Controller
		// Ziegler–Nichols
		// ku -> ultimate proportional gain (kp = ku, ki = 0 / system oscillates)
		// pu -> oscillation period
	float ku;
	float pu;

		// Proportional
		// kp manually adjusted
	float kp;

		// Integral
		// ki manually adjusted
	float ki;

	int TAM_BUFFER;

	char control_action_str_parsed[103];
	char control_received_message[1000];
};

struct state_info {
	char *state;
	char *temperature;
	const int small_interval;
};

pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t screen_mutex = PTHREAD_MUTEX_INITIALIZER;

float alarming_temperature = 21;
float temperature_ref = 20;
float height_ref = 2;

struct control_info {
	char *prefix;
	struct controller_setup *controller_setup1;
	struct controller_setup *controller_setup2;
	float *error;
	float *ref;
	char *Ti;
	char *controlled_variable;
	int socket_local;
	struct sockaddr_in endereco_destino;
	const int small_interval;
	int TAM_BUFFER;
};

int cria_socket_local(void)
{
	int socket_local;		/* Socket usado na comunicacão */

	socket_local = socket( PF_INET, SOCK_DGRAM, 0);
	if (socket_local < 0) {
		perror("socket");
		return -1;
	}
	return socket_local;
}

struct sockaddr_in cria_endereco_destino(char *destino, int porta_destino)
{
	struct sockaddr_in servidor; 	/* Endereço do servidor incluindo ip e porta */
	struct hostent *dest_internet;	/* Endereço destino em formato próprio */
	struct in_addr dest_ip;		/* Endereço destino em formato ip numérico */

	if (inet_aton ( destino, &dest_ip ))
		dest_internet = gethostbyaddr((char *)&dest_ip, sizeof(dest_ip), AF_INET);
	else
		dest_internet = gethostbyname(destino);

	if (dest_internet == NULL) {
		fprintf(stderr,"Endereço de rede inválido\n");
		exit(FALHA);
	}

	memset((char *) &servidor, 0, sizeof(servidor));
	memcpy(&servidor.sin_addr, dest_internet->h_addr_list[0], sizeof(servidor.sin_addr));
	servidor.sin_family = AF_INET;
	servidor.sin_port = htons(porta_destino);

	return servidor;
}

void envia_mensagem(int socket_local, struct sockaddr_in endereco_destino, char *mensagem)
{
	/* Envia msg ao servidor */

	if (sendto(socket_local, mensagem, strlen(mensagem)+1, 0, (struct sockaddr *) &endereco_destino, sizeof(endereco_destino)) < 0 )
	{ 
		perror("sendto");
		return;
	}
}


int recebe_mensagem(int socket_local, char *buffer, int TAM_BUFFER)
{
	int bytes_recebidos;		/* Número de bytes recebidos */

	/* Espera pela msg de resposta do servidor */
	bytes_recebidos = recvfrom(socket_local, buffer, TAM_BUFFER, 0, NULL, 0);
	if (bytes_recebidos < 0) {
		perror("recvfrom");
	}

	return bytes_recebidos;
}

int send_request(int socket_local, struct sockaddr_in endereco_destino, char *mensagem, char *buffer, int TAM_BUFFER){
	int nrec;
	
	envia_mensagem(socket_local, endereco_destino, mensagem);
	nrec = recebe_mensagem(socket_local, buffer, TAM_BUFFER);
	buffer[nrec] = '\0';
	return nrec;
}

void slice_str(const char * str, char * buffer, size_t start, size_t end){
  size_t j = 0;
  for (size_t i = start; i <= end; ++i) {
    buffer[j++] = str[i];
  }
  buffer[j] = 0;
}

float controller(
	struct controller_setup *cs,
	float *error_pointer,
	float *ref,
	char *controlled_variable,
	int socket_local,
	struct sockaddr_in endereco_destino,
	const int small_interval,
	int TAM_BUFFER
){
	float proportional_control;
	float integral;
	float previous_integral = 0;
	float integral_control;

	float control_action;
	char control_action_str[100];

	float error;
	
	// Calculate error
	error = *ref - atof(controlled_variable);
	*error_pointer = error;

	// Controller
		// Proportional
	proportional_control = cs->kp * error;
	
		// Integral
	integral = previous_integral + error * small_interval / 1000000000;

	integral_control = cs->ki * integral ;

		// Sum control actions
	control_action = proportional_control + integral_control;
	
	if (control_action > cs->overflow){
		control_action = cs->overflow;
		integral = cs->overflow;
	} else if (control_action < cs->underflow){
		control_action = cs->underflow;
		integral = cs->underflow;
	}

	gcvt(control_action, 8, control_action_str); 

	strcpy(cs->control_action_str_parsed, cs->prefix);
	strcat(cs->control_action_str_parsed, control_action_str);

		// Store current integral value for next loop
	previous_integral = integral;
	
	pthread_mutex_lock(&socket_mutex);
	send_request(socket_local, endereco_destino, cs->control_action_str_parsed, cs->control_received_message, TAM_BUFFER);
	pthread_mutex_unlock(&socket_mutex);
}

float read_and_parse(
	int socket_local,
	struct sockaddr_in endereco_destino,
	char *read_from_prefix,
	char *read_from,
	int TAM_BUFFER
){
	char msg_recebida[TAM_BUFFER];
	const size_t read_from_prefix_size = strlen(read_from_prefix);

	pthread_mutex_lock(&socket_mutex);
	send_request(socket_local, endereco_destino, read_from_prefix, msg_recebida, TAM_BUFFER);
	pthread_mutex_unlock(&socket_mutex);

	slice_str(msg_recebida, read_from, read_from_prefix_size - 1, TAM_BUFFER);
}

void check_user_input() {
	char enter;
	scanf("%c", &enter);
	if(enter != 'r') {
		check_user_input();
	}
}

void check_for_user_inputs() {
	float new_temperature_ref;
	float new_height_ref;

	check_user_input();

	// Lock screen
    pthread_mutex_lock(&screen_mutex);

	printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");

	printf("\n\tReferencia Temperature: ");
	scanf("%f", &new_temperature_ref);
	printf("\n\tReferencia Altura:      ");
	scanf("%f", &new_height_ref);

	// Unlock screen
    pthread_mutex_unlock(&screen_mutex);

	temperature_ref = new_temperature_ref;
	height_ref = new_height_ref;
}

void *inputs_thread_function(void *args) {
	while (1) {
		check_for_user_inputs();
	}
	// Not needed, just to keep it safe for the future
	pthread_exit(NULL);

}

void handle_alarm(struct state_info *alarm_info) {
	struct timespec t0;
	clock_gettime(CLOCK_MONOTONIC ,&t0);

	while (1) {
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t0, NULL);

		if (atof(alarm_info->temperature) > alarming_temperature) {
			strcpy(alarm_info->state, "ALARME");
		} else {
			strcpy(alarm_info->state, "OK");
		}

		t0.tv_nsec += alarm_info->small_interval;

		while (t0.tv_nsec >= NSEC_PER_SEC) {
			t0.tv_nsec -= NSEC_PER_SEC;
			t0.tv_sec++;
		}
	}
	// Not needed, just to keep it safe for the future
	pthread_exit(NULL);
}

void *alarm_thread_function(void *received_struct) {
	// struct state_info *alarm_info = (struct state_info*) received_struct;
	handle_alarm(received_struct);
}

void control_function(struct control_info *control_info_struct) {
	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC ,&t0);
	long response_time;

	// File
	FILE *output_file;

	output_file = fopen("response_times.csv", "w");
	
	if(output_file == NULL) {
        printf("File couldn't be opened\n");
        exit(1);
    }

	while (1) {

		// Espera ate proximo periodo
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t0, NULL);

		// Leitura dos sensores
		read_and_parse(
			control_info_struct->socket_local,
			control_info_struct->endereco_destino,
			control_info_struct->prefix,
			control_info_struct->controlled_variable,
			control_info_struct->TAM_BUFFER
		);

		read_and_parse(
			control_info_struct->socket_local,
			control_info_struct->endereco_destino,
			"sti0",
			control_info_struct->Ti,
			control_info_struct->TAM_BUFFER
		);

		controller(
			control_info_struct->controller_setup1,
			control_info_struct->error,
			control_info_struct->ref,
			control_info_struct->controlled_variable,
			control_info_struct->socket_local,
			control_info_struct->endereco_destino,
			control_info_struct->small_interval,
			control_info_struct->TAM_BUFFER
		);

		controller(
			control_info_struct->controller_setup2,
			control_info_struct->error,
			control_info_struct->ref,
			control_info_struct->controlled_variable,
			control_info_struct->socket_local,
			control_info_struct->endereco_destino,
			control_info_struct->small_interval,
			control_info_struct->TAM_BUFFER
		);

		// Tempo de resposta
		clock_gettime(CLOCK_MONOTONIC, &t1);
		response_time = (t1.tv_sec - t0.tv_sec) * NSEC_PER_SEC + (t1.tv_nsec - t0.tv_nsec);

		// Exporta tempo de resposta
		fprintf(output_file,"%ld\n", response_time);

		t0.tv_nsec += control_info_struct->small_interval;

		while (t0.tv_nsec >= NSEC_PER_SEC) {
			t0.tv_nsec -= NSEC_PER_SEC;
			t0.tv_sec++;
		}
	}
	fclose(output_file);
}

void *control_thread_function(void *received_struct) {
	control_function(received_struct);
}

int main(int argc, char *argv[]) {
	if (argc < 3) { 
		fprintf(stderr,"Uso: udpcliente endereço porta palavra \n");
		fprintf(stderr,"onde o endereço é o endereço do servidor \n");
		fprintf(stderr,"porta é o número da porta do servidor \n");
		fprintf(stderr,"exemplo de uso:\n");
		fprintf(stderr,"   udpcliente baker.das.ufsc.br 1234 \"ola\"\n");
		exit(FALHA);
	}


	// Socket
	int porta_destino = atoi( argv[2]);
	int socket_local = cria_socket_local();
	struct sockaddr_in endereco_destino = cria_endereco_destino(argv[1], porta_destino);


	// Message
	int TAM_BUFFER = 10000;    
  	char temperature[TAM_BUFFER + 1];
  	char height[TAM_BUFFER + 1];
  	char Ti[TAM_BUFFER + 1];


	// State
  	char state[7] = "OK";

	// Errors
	float temperature_error;
	float height_error;


	// Time
	const int temperature_small_interval = 50000000; // 50ms
	const int height_small_interval = 70000000; // 70ms
	const int alarm_small_interval = 10000000; // 10ms

	// Define controladores
	struct controller_setup Na_controller_setup = {
		.prefix = "ana",
		.overflow = 10.0,
		.underflow = 0.0,
		.ku = 500.0,
		.pu = 0.5,
		.TAM_BUFFER = TAM_BUFFER,
	};
	Na_controller_setup.kp = 0.45 * Na_controller_setup.ku;
	Na_controller_setup.ki = 1.2 * Na_controller_setup.kp / Na_controller_setup.pu;

	struct controller_setup Q_controller_setup = {
		.prefix = "aq-",
		.overflow = 1000000.0,
		.underflow = 0.0,
		.ku = 10000000.0,
		.pu = 0.5,
		.TAM_BUFFER = TAM_BUFFER,
	};
	Q_controller_setup.kp = 0.45 * Q_controller_setup.ku;
	Q_controller_setup.ki = 1.2 * Q_controller_setup.kp / Q_controller_setup.pu;

	struct controller_setup Ni_controller_setup = {
		.prefix = "ani",
		.overflow = 100.0,
		.underflow = 0.0,
		.ku = 500.0,
		.pu = 0.1,
		.TAM_BUFFER = TAM_BUFFER,
	};
	Ni_controller_setup.kp = 0.45 * Ni_controller_setup.ku;
	Ni_controller_setup.ki = 1.2 * Ni_controller_setup.kp / Ni_controller_setup.pu;

	struct controller_setup Nf_controller_setup = {
		.prefix = "anf",
		.overflow = 100.0,
		.underflow = 0.0,
		.ku = -500.0,
		.pu = 0.1,
		.TAM_BUFFER = TAM_BUFFER,
	};
	Nf_controller_setup.kp = 0.45 * Nf_controller_setup.ku;
	Nf_controller_setup.ki = 1.2 * Nf_controller_setup.kp / Nf_controller_setup.pu;


	// Define all temperature necessary control info
	struct control_info temperature_control_info = {
		.prefix = "st-0",
		.controller_setup1 = &Na_controller_setup,
		.controller_setup2 = &Q_controller_setup,
		.error = &temperature_error,
		.ref = &temperature_ref,
		.Ti = Ti,
		.controlled_variable = temperature,
		.socket_local = socket_local,
		.endereco_destino = endereco_destino,
		.small_interval = temperature_small_interval,
		.TAM_BUFFER = TAM_BUFFER,
	};


	// Define all height necessary control info
	struct control_info height_control_info = {
		.prefix = "sh-0",
		.controller_setup1 = &Ni_controller_setup,
		.controller_setup2 = &Nf_controller_setup,
		.error = &height_error,
		.ref = &height_ref,
		.Ti = Ti,
		.controlled_variable = height,
		.socket_local = socket_local,
		.endereco_destino = endereco_destino,
		.small_interval = height_small_interval,
		.TAM_BUFFER = TAM_BUFFER,
	};


	// Define information necessary for the alarm
	struct state_info alarm_info = {
		.state = state,
		.temperature = temperature,
		.small_interval = alarm_small_interval,
	};


	// Define threads
	pthread_t inputs_thread;
	pthread_create(&inputs_thread, NULL, inputs_thread_function, NULL);
	
	pthread_t alarm_thread;
	pthread_attr_t alarm_thread_attr;
	pthread_attr_init(&alarm_thread_attr);
	pthread_create(&alarm_thread, &alarm_thread_attr, alarm_thread_function, &alarm_info);

	pthread_t temperature_control_thread;
	pthread_attr_t temperature_control_thread_attr;
	pthread_attr_init(&temperature_control_thread_attr);
	pthread_create(&temperature_control_thread, &temperature_control_thread_attr, control_thread_function, &temperature_control_info);

	pthread_t height_control_thread;
	pthread_attr_t height_control_thread_attr;
	pthread_attr_init(&height_control_thread_attr);
	pthread_create(&height_control_thread, &height_control_thread_attr, control_thread_function, &height_control_info);

	while(1) {

		pthread_mutex_lock(&screen_mutex);

		// Inteface usuario
		printf("\n\n\n\n");
		printf("State                      <<< %s >>>\n", alarm_info.state);
		printf("Alarming temperature       --- %f ---\n\n", alarming_temperature);
		
		printf("Temperature reference      >>> %f <<<\n", temperature_ref);
		printf("Temperature                --- %s\n", temperature);
		printf("Q                          --- %s\n", Q_controller_setup.control_received_message);
		printf("Na                         --- %s\n\n", Na_controller_setup.control_received_message);
		
		printf("Height reference           >>> %f <<<\n", height_ref);
		printf("Height                     --- %s\n", height);
		printf("Ni                         --- %s\n", Ni_controller_setup.control_received_message);
		printf("Nf                         --- %s\n\n", Nf_controller_setup.control_received_message);

		printf("Ti                         --- %s\n\n", Ti);

		// nc_printf("Response time              --- %ld ns\n\n", response_time);

		printf("Para mudar a referencia: \n");
		printf("Pressione r + Enter: \n");


    	pthread_mutex_unlock(&screen_mutex);


		usleep(500000);
	}
}
