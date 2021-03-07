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

#include <time.h>

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
	float ref,
	char *temperature,
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
	error = ref - atof(temperature);
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
	
	send_request(socket_local, endereco_destino, cs->control_action_str_parsed, cs->control_received_message, TAM_BUFFER);
}

float read_and_parse_temperature(
	int socket_local,
	struct sockaddr_in endereco_destino,
	char *temperature,
	int TAM_BUFFER
){
	char T_msg_recebida[TAM_BUFFER];
	char temperature_prefix[] = "st-0";
	const size_t temperature_prefix_size = strlen(temperature_prefix);

	send_request(socket_local, endereco_destino, "st-0", T_msg_recebida, TAM_BUFFER);
	slice_str(T_msg_recebida, temperature, temperature_prefix_size - 1, TAM_BUFFER);
}

int main(int argc, char *argv[])
{
	if (argc < 4) { 
		fprintf(stderr,"Uso: udpcliente endereço porta palavra \n");
		fprintf(stderr,"onde o endereço é o endereço do servidor \n");
		fprintf(stderr,"porta é o número da porta do servidor \n");
		fprintf(stderr,"palavra é a palavra que será enviada ao servidor \n");
		fprintf(stderr,"exemplo de uso:\n");
		fprintf(stderr,"   udpcliente baker.das.ufsc.br 1234 \"ola\"\n");
		exit(FALHA);
	}

	// Socket
	int porta_destino = atoi( argv[2]);
	int socket_local = cria_socket_local();
	struct sockaddr_in endereco_destino = cria_endereco_destino(argv[1], porta_destino);

	// Message
	int TAM_BUFFER = 1000;    
  	char temperature[TAM_BUFFER + 1];

	float ref = atof(argv[3]);
	float error;

	// Time
	struct timespec t0, t1;
	const int small_interval = 30000000;
	const int big_interval = 500000000; /* 1000ms*/
	const int small_intervals_in_big_interval = big_interval / small_interval; /* 1000ms*/
	int small_loop_count = 0; /* 1000ms*/
	long response_time;
	clock_gettime(CLOCK_MONOTONIC ,&t0);
	t0.tv_sec++; // start after one second

	// File
	FILE *output_file;

	output_file = fopen("response_times.csv", "w");
	
	if(output_file == NULL) {
        printf("File couldn't be opened\n");
        exit(1);
    }

	struct controller_setup Na_controller_setup = {
		"ana",
		10.0,
		0.0,
		500.0,
		0.5,
		TAM_BUFFER,
	};
	Na_controller_setup.kp = 0.45 * Na_controller_setup.ku;
	Na_controller_setup.ki = 1.2 * Na_controller_setup.kp / Na_controller_setup.pu;

	struct controller_setup Q_controller_setup = {
		"aq-",
		1000000.0,
		0.0,
		10000000.0,
		0.5,
		TAM_BUFFER,
	};
	Q_controller_setup.kp = 0.45 * Q_controller_setup.ku;
	Q_controller_setup.ki = 1.2 * Q_controller_setup.kp / Q_controller_setup.pu;

	struct controller_setup Ni_controller_setup = {
		"ani",
		100.0,
		0.0,
		-500.0,
		0.5,
		TAM_BUFFER,
	};
	Ni_controller_setup.kp = 0.45 * Ni_controller_setup.ku;
	Ni_controller_setup.ki = 1.2 * Ni_controller_setup.kp / Ni_controller_setup.pu;

	int number_of_loops = 0;
	while(number_of_loops <= 20000) {
		/* wait until next shot */
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t0, NULL);

		read_and_parse_temperature(socket_local, endereco_destino, temperature, TAM_BUFFER);

		controller(&Na_controller_setup, &error, ref, temperature, socket_local, endereco_destino, small_interval, TAM_BUFFER);
		controller(&Q_controller_setup, &error, ref, temperature, socket_local, endereco_destino, small_interval, TAM_BUFFER);
		controller(&Ni_controller_setup, &error, ref, temperature, socket_local, endereco_destino, small_interval, TAM_BUFFER);

		clock_gettime(CLOCK_MONOTONIC, &t1);

		response_time = (t1.tv_sec - t0.tv_sec) * NSEC_PER_SEC + (t1.tv_nsec - t0.tv_nsec);

		fprintf(output_file,"%ld\n", response_time);

		t0.tv_nsec += small_interval;

		while (t0.tv_nsec >= NSEC_PER_SEC) {
			t0.tv_nsec -= NSEC_PER_SEC;
			t0.tv_sec++;
		}

		small_loop_count++;
		number_of_loops++;

		// printf("count %d\n", small_loop_count);

		if (small_loop_count >= small_intervals_in_big_interval){
			printf("\n\n\n\n");
			printf("Reference >>>>>>>> %f <<<<<<<<\n\n", ref);
			printf("Temperature - Mensagem de resposta >>> %s\n", temperature);
			printf("Na - Mensagem de resposta >>> %s\n", Na_controller_setup.control_received_message);
			printf("Q - Mensagem de resposta >>> %s\n", Q_controller_setup.control_received_message);
			printf("Ni - Mensagem de resposta >>> %s\n", Ni_controller_setup.control_received_message);
			printf("Reference error >>> %f\n", error);
			printf("Response time >>> %ld ns\n", response_time);

			small_loop_count = 0;
		}
	}
	fclose(output_file);
}
