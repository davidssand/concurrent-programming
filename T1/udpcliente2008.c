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

#define	TAM_MEU_BUFFER 1000

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

// int controller(){

// }

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

	int porta_destino = atoi( argv[2]);

	int socket_local = cria_socket_local();

	struct sockaddr_in endereco_destino = cria_endereco_destino(argv[1], porta_destino);

	int size_incoming_message = 1000;    

	char T_msg_recebida[size_incoming_message];
	int nrec_T;
	const size_t T_len = strlen(T_msg_recebida);
  char sliced_T_msg_recebida[T_len + 1];
	float Temperature;

	char Na_msg_recebida[size_incoming_message];
	int nrec_Na;

	float ref = 40.0;
	float error;

	struct timespec t0, t1;
  const int small_interval = 250000000;
  const int big_interval = 1000000000; /* 1000ms*/
  const int small_intervals_in_big_interval = big_interval / small_interval; /* 1000ms*/
  int small_loop_count = 0; /* 1000ms*/

	long response_time;

  clock_gettime(CLOCK_MONOTONIC ,&t0);

  /* start after one second */
  t0.tv_sec++;

  while(1) {
    /* wait until next shot */
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t0, NULL);

		// controller();

		nrec_T = send_request(socket_local, endereco_destino, "st-0", T_msg_recebida, size_incoming_message);
		nrec_Na = send_request(socket_local, endereco_destino, "ana7", Na_msg_recebida, size_incoming_message);
  	slice_str(T_msg_recebida, sliced_T_msg_recebida, 3, size_incoming_message);
		Temperature = atof(sliced_T_msg_recebida);

		error = ref - Temperature;
		
		clock_gettime(CLOCK_MONOTONIC, &t1);

		response_time = (t1.tv_sec - t0.tv_sec) * NSEC_PER_SEC + (t1.tv_nsec - t0.tv_nsec);

		t0.tv_nsec += small_interval;

    while (t0.tv_nsec >= NSEC_PER_SEC) {
      t0.tv_nsec -= NSEC_PER_SEC;
      t0.tv_sec++;
    }

		small_loop_count++;

		printf("count %d\n", small_loop_count);

		if (small_loop_count >= small_intervals_in_big_interval){
			printf("Temperature - Mensagem de resposta com %d bytes >>> %f\n", nrec_T, Temperature);
			printf("Na - Mensagem de resposta com %d bytes >>> %s\n", nrec_Na, Na_msg_recebida);
			printf("Reference error >>> %f\n", error);
			printf("Response time: %ld \n", response_time);

			printf("\n");
			small_loop_count = 0;
		}
	}
}
