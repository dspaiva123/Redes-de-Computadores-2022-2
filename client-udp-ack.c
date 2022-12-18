#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/time.h>

#define SENDER 1
#define MAX_MSG 1000
struct message_t{
	char id; //0 ou 1 ou 2 (ack)
	char message[MAX_MSG];
	uint8_t checksum;
};


typedef struct message_t tMessage;

//CORRIGIR DEPOIS!!!
uint16_t rfc_checksum(void* addr,size_t count) {
    register long sum = 0;

        while( count > 1 )  {
           /*  This is the inner loop */
               sum += * (unsigned short *) addr++;
               count -= 2;
       }

           /*  Add left-over byte, if any */
       if( count > 0 )
               sum += * (unsigned char *) addr;

           /*  Fold 32-bit sum to 16 bits */
       while (sum>>16)
           sum = (sum & 0xffff) + (sum >> 16);

       return ~sum;
}

tMessage make_pkt(char id, char msg[MAX_MSG])
{
	tMessage pkt;
	pkt.id = id;
	strcpy(pkt.message,msg);
	pkt.checksum = rfc_checksum(msg,sizeof(msg));
	return pkt;
}

int isCorrupt(tMessage pkt) //VERIFICA SE O CHECKSUM CALCULADO É DIFERENTE DO DO ENVIADO!
{
	return (pkt.checksum != rfc_checksum(pkt.message,sizeof(pkt.message)));
}

int isACK(tMessage pkt, int expID) //VERIFICA SE EH O ACK ESPERADO
{
	if (pkt.id != 2) return 0; //se nao é pacote ack, não é ACK
	return (atoi(pkt.message) == expID); //lembrar de que o reciever tem que enviar a mensagem do pacote sendo uma string de "0" ou "1"
}


//Calculo do TimeOut médio (finalizar)
int num_samples = 1; //num amostras
struct timeval timeout_avg;
#define START_TIMEOUT_sec 2
#define START_TIMEOUT_usec 1;

void init_timeout(struct timeval *tp)
{
	tp->tv_sec = START_TIMEOUT_sec;
	tp->tv_usec = START_TIMEOUT_usec;
}

void set_timeout(int fd, struct timeval timeout)
{
	// timeout para recebimento
	if (setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
		perror("setsockopt(..., SO_RCVTIMEO ,...");
}

double getTime_ms()
{
	struct timeval tp;
	gettimeofday(&tp, 0);
	double seconds = tp.tv_sec;
	double microseconds = tp.tv_usec;
	return (1000 * seconds + microseconds);
}

char currPkt = 0; //inicializa enviando o pacote 0

void rdt_send(int fd, tMessage request, tMessage response, struct sockaddr_in servaddr)
{
	int currerrno = errno; //keep track of errno to check if there was a timeout
	int keepGoing = 1;

	//Parte do timeout movel
	double t1 = 0;
	double t2 = 0;
	struct timeval cTime;
	//fim da parte do timeout movel

	while(keepGoing == 1){
		t1 = getTime_ms();
		sendto(fd, (tMessage*)&request, sizeof(request), MSG_CONFIRM,
			(struct sockaddr *) &servaddr, sizeof(servaddr));

		printf("Message Sent\n");


		int res, len;
		len = sizeof(struct sockaddr_in);
		res = recvfrom(fd, (tMessage*)&response, sizeof(response), MSG_WAITALL,
				(struct sockaddr *) &servaddr, &len);

		//NÓ de Wait for ACK
		//verificar o response se foi o ACK certo e o ack do pacote enviado

		if (errno != EAGAIN)
		{
			printf("Acknoledgment: %d\n", response.id);
			keepGoing = 0;
			t2 = getTime_ms();
			double timeElapsed = t2 - t1;
			printf("Elapsed = %f\n", timeElapsed);
		}
		else
		{
			printf("Timeout! Trying again...\n");
			errno = 0; //reset errno to try again
		}
	}
}


void rdt_receive(int fd, tMessage request, tMessage response, struct sockaddr_in servaddr)
{
	int res, len;

	len = sizeof(struct sockaddr_in);
	res = recvfrom(fd, (tMessage*)&response, sizeof(tMessage), MSG_WAITALL,
		 	(struct sockaddr *) &servaddr, &len);

	printf("Response: %d\n", response.id);

	/*
	sendto(fd, (tMessage*)&request, sizeof(request), MSG_CONFIRM,
		(struct sockaddr *) &servaddr, sizeof(servaddr));

	printf("Response Sent\n");
	*/
}


int main(int argc, char* argv[])
{
	if(argc < 4)
	{
		printf("Use: <ip> <porta> <mensagem>\n");
		return(-1);
	}

	char* ip;
	ip = argv[1];
	char* port;
	port = argv[2];

	struct sockaddr_in servaddr;

	tMessage request;

	tMessage response;

	strcpy(request.message, argv[3]);
	request.id = SENDER;

	strcpy(response.message, "");
	response.id = 0;

	int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if(fd < 0)
	{
		perror("Erro no Socket");
       		exit(EXIT_FAILURE);
	}

	struct timeval timeout;
	timeout.tv_sec = 5; // segundos
	timeout.tv_usec = 0; // microssegundos
	// timeout para recebimento
	if (setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
		perror("setsockopt(..., SO_RCVTIMEO ,...");

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(port));
	servaddr.sin_addr.s_addr = inet_addr(ip);

	rdt_send(fd, request, response, servaddr);
	//rdt_receive(fd, request, response, servaddr);

	close(fd);
	return 0;
}