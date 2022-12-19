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
#define din_timeout 1

//TIMEOUT INICIAL:
__time_t init_timeout_sec = 2;		/* Seconds.  */
__suseconds_t init_timeout_usec = 0;	/* Microseconds.  */

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

tMessage make_pkt(char id, char msg[MAX_MSG], u_int16_t checksum)
{
	tMessage pkt;
	pkt.id = id;
	strcpy(pkt.message,msg);
	pkt.checksum = checksum;
	return pkt;
}

int isCorrupt(tMessage pkt) //VERIFICA SE O CHECKSUM CALCULADO É DIFERENTE DO DO ENVIADO!
{
	return (pkt.checksum != rfc_checksum(&pkt.message,sizeof(pkt.message)));
}

int isACK(tMessage pkt, int expID) //VERIFICA SE EH O ACK ESPERADO
{
	if (pkt.id != 2) return 0; //se nao é pacote ack, não é ACK
	return (atoi(pkt.message) == expID); //lembrar de que o reciever tem que enviar a mensagem do pacote sendo uma string de "0" ou "1"
}


/* ########## timeout.h #################*/

struct timeval timeout_avg;//Note that timeout stores de AVERAGE timeout to be set
int samples = 0;
long int secSum = 0;
long int usecSum = 0;

double getTime_ms(struct timeval *tp)
{
	gettimeofday(tp, 0);
	return (1000 * (double) tp->tv_sec + (double) tp->tv_usec);
}

/* Update the average timeout by adding to the current avg
   a given time difference: timevals of final (tf) and initial
   (ti) time */
void ev_timeout(struct timeval *ti, struct timeval *tf){
    secSum += (tf->tv_sec - ti->tv_sec); //diff
    usecSum += (tf->tv_usec - ti->tv_usec);
    samples++;
    timeout_avg.tv_sec = (uint16_t) (secSum/samples);
    timeout_avg.tv_usec = (uint16_t) (usecSum/samples);
}

void set_timeout(int fd, struct timeval timeout)
{
	// timeout para recebimento
	if (setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
		perror("setsockopt(..., SO_RCVTIMEO ,...");
}

// ############### RDT ###############################

char currPkt = 0; //inicializa enviando o pacote 0

void rdt_send(int fd, char data[MAX_MSG], struct sockaddr_in servaddr)
{
	int currerrno = errno; //keep track of errno to check if there was a timeout
	int keepGoing = 1;

	//Parte do timeout movel:
	struct timeval t0, tf;
	
	//Calculando o checksum:
	uint16_t chk = 0; //NAO FUNCIONA O CHECKSUM!!!!!
	tMessage request = make_pkt(currPkt, data, chk);

	while(keepGoing == 1){
		if(din_timeout) getTime_ms(&t0); //get initial time

		sendto(fd, (tMessage*)&request, sizeof(request), MSG_CONFIRM,
			(struct sockaddr *) &servaddr, sizeof(servaddr));

		printf("sendto: Message Sent\n");

		//Check the ack:
		int res, len;
		tMessage response; //redeclare the response all times!!
		len = sizeof(struct sockaddr_in);
		res = recvfrom(fd, (tMessage*)&response, sizeof(response), MSG_WAITALL,
				(struct sockaddr *) &servaddr, &len);

		//NÓ de Wait for ACK
		//verificar o response se foi o ACK certo e o ack do pacote enviado

		if (errno != EAGAIN) //Se nao chegou o pacote, reenviar o pacote!
		{
			//Pacote recebido: Ajustar o timeout medio
			if(din_timeout)
			{
				getTime_ms(&tf); //INDEPENDENTE DE SE FOR O CERTO, o timeout é em cima de um pacote enviar e seu tempo a ser recevido de volta
				ev_timeout(&t0,&tf);
				set_timeout(fd, timeout_avg);
			}
			
			printf("Packet Recieved:\n| ID: %d\n| Content: %s\n| Cksum: %d", response.id, response.message, response.checksum);
			//Pacote recebido: conferir se eh o ack certo!!
			//Check if it is corrupted:
			if (isCorrupt(response))
			{
				printf("rdt_send: Corrupted Package recieved: Trying again...\n");
				errno = 0;
			} 
			else
			{
				//Check the ack recieved:
				if (isACK(response, currPkt)){
					currPkt = !currPkt; //Next package will be: 1, if current was 0. 0, if current was 1.
					keepGoing = 0; //OK!
					printf("sendto: Message Succesfully Sent :D \n");
				} 
				else 
				{
					printf("rdt_send: ACK not recieved: Trying again...\n");
					errno = 0;
				}
			}	
		}
		else
		{
			printf("recv_from: Timeout! Trying again...\n");
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

	//Conffigurando o TIMEOUT
	struct timeval timeout;
	timeout.tv_sec = init_timeout_sec; // segundos
	timeout.tv_usec = init_timeout_usec; // microssegundos
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