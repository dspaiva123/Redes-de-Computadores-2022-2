#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/time.h>


#define MAX_MSG 1000
#define MAX_MSG_AMOUNT 40
#define din_timeout 1
#define test_corrupt 0
#define STOUS 1000000 //10**6

unsigned short currMsg = 1; //inicializa enviando a mensagem 1

/* ########## rdt_timeout.h #################*/

double estimated_rtt = 2; //TODO tempo em segundos estimado de rtt (obtido previamente?)
double deviation = 0.1; //TODO desvio

struct timeval get_time_in_timeval(double time) 
{
    struct timeval tp;
    bzero(&tp,sizeof(struct timeval));
    double seconds = floor(time);
    tp.tv_sec = (time_t) seconds;
    tp.tv_usec = STOUS * (time - seconds);

    return tp;
}

/*Get current time and return it in (double) seconds*/
double get_time_in_seconds()
{
	struct timeval tp;
	bzero(&tp, sizeof(struct timeval));
	gettimeofday(&tp, 0);
	return ((double) tp.tv_sec + ((double) tp.tv_usec / STOUS) );
}

/*Returns the adjusted timeout by recieving the sample timeout
 * Uses the formula: 
 * given alpha = 1/8, beta = 1/4:
 * 	RTT_estimado = (1 - alpha)*RTT_estimado + alpha*RTT_estimado
 * 	dev = (1- beta)*dev + betha*|RTT_amostrado - RTT_estimado|
 * 	timeout = RTT_estimado + 4*dev
 */
double get_timeout_in_ms(double initial, double final)
{
	double timeout = 0;
    double sample_rtt = final - initial;
	double alfa = (double) 1 / 8;
	double beta = (double) 1 / 4;

    estimated_rtt = ((1 - alfa) * estimated_rtt) + (alfa * sample_rtt);
    deviation = ((1 - beta) * deviation) + (beta * abs(sample_rtt - estimated_rtt));
	timeout = (estimated_rtt + (4 * deviation));//the timeout to be set

	//debug:
	printf("\n$$ Timeout Ajustado:\n$$ - Amostra: %lf\n", sample_rtt);
	printf("$$ - RTT_estimado: %lf\n$$ - Desvio: %lf\n$$ - Timeout novo: %lf\n", estimated_rtt, deviation, timeout);
	
    return timeout;
}

/* set the time given in timeval struct *timeout* as timeout to the *fd* socket */
void set_timeout(int fd, double timeout_insec)
{
	struct timeval timeout = get_time_in_timeval(timeout_insec);
	if (setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
		perror("setsockopt(..., SO_RCVTIMEO ,...");
}

/* ########## rdt_checksum.h #################*/

/*	Addr: pointer to msg
 *	Count: size of msg */
unsigned short rfc_checksum(unsigned short * addr,size_t count) {
    register long sum = 0;

        while( count > 1 )  {
           /*  This is the inner loop */
               sum += * (unsigned short *) addr++;
               count -= 1;
       }

           /*  Add left-over byte, if any */
       if( count > 0 )
               sum += * (unsigned char *) addr;

           /*  Fold 32-bit sum to 16 bits */
       while (sum>>16)
           sum = (sum & 0xffff) + (sum >> 16);

       return ~sum;
}

/* ########## rdt.h #################*/

struct header_t{
	unsigned int seq; //seq number
	unsigned int ack; //ZERO se pacote normal
	unsigned short checksum; //calculated checksum using the above RFC_checksum function from checksum.h
    unsigned short size_msg; //size of message payload LIMITED BY MAX_MSG
};

struct message_t{
    struct header_t h;
	unsigned char msg[MAX_MSG];
};

typedef struct message_t tMessage;


/* Given a tMessage msg, returns the respective size of packet
 * aa 
 */
int get_msg_size(tMessage msg){
    return (sizeof(struct header_t) + msg.h.size_msg);
}

/* returns a tMessage package containing:
 *	ACK -> 0, if is a message and ACK number if is an ACK package
 *	Message -> the data to be transmitted
 *	msg_size -> the size of the message
 */
tMessage make_msg(unsigned int seq, unsigned int ack, void* message, unsigned short msg_size)
{
	tMessage pkt;
	bzero(&pkt, sizeof(tMessage));
	pkt.h.seq = seq;
    pkt.h.ack = ack;
	memcpy(pkt.msg, message, msg_size);
	pkt.h.size_msg = msg_size;
    pkt.h.checksum = 0;
	pkt.h.checksum = rfc_checksum((unsigned short *) &pkt, get_msg_size(pkt));
	return pkt;
}

// Print the content of given tMessage msg
void show_msg(tMessage msg)
{
    if(msg.h.ack != 0) printf("\n$ ------------\n$ ACK Package:\n");
    else printf("\n$ ------------\n$ MSG Package:\n");

    printf("$ |Header:\n$ | seq: %d\n$ | ack: %d\n$ | CS: %d\n$ | msg_size: %d\n$ |Message: \n$ | msg: \"%s\"\n$ ------------\n", msg.h.seq, msg.h.ack, msg.h.checksum, msg.h.size_msg, msg.msg);
}

/* Checks if a msg packet is corrupted and returns 1 if yes:
 * - Recalculates the checksum using the same method as make_msg()
 * - evaluates if it is different from the checksum in the msg packet
 */
int isCorrupt(tMessage msg)
{
	unsigned short checksum = msg.h.checksum;
	msg.h.checksum = 0;
	return (checksum != rfc_checksum((unsigned short *) &msg, get_msg_size(msg)) );
}

/* Checks if the tMessage msg is an expAck ACK
 * - Given the expected ack, compares if the given msg is an ACK for this expAck
 */
int isACK(tMessage msg, unsigned int expAck) //Verifica se é o ACK esperado
{
	return (msg.h.ack == expAck); //lembrar de que o reciever tem que enviar a mensagem do pacote sendo uma string de "0" ou "1"
}

/* Checks if the given msg is the expected seq
 * - Given the expected seq, compares if the given msg is the msg of the exp seq
 */
int isSeq(tMessage msg, unsigned int expSeq)
{
	return (msg.h.seq == expSeq);
}

void rdt_send(int fd, void * data, unsigned short data_size, char *ip, char *port)
{
	int keepGoing = 1;
	int res;
	unsigned int len;

	//Parte do timeout movel:
	double t0 = 0;
	double tf = 0;
	double timeout;
	if(currMsg == 1){
		//the first call assumes the initial estimated_RTT as timeout (or send a "testing" package?)
		timeout = estimated_rtt;
		set_timeout(fd, timeout);
	}
	
	struct sockaddr_in servaddr;
	tMessage request, response;

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(port));
	servaddr.sin_addr.s_addr = inet_addr(ip);

	request = make_msg(currMsg, 0, data, data_size);

	if(test_corrupt && ((currMsg % 2) == 0)){ 
		request.msg[data_size-1] = 'X'; //CORROMPENDO CADA DADO DE SEQ PAR
	}

	while(keepGoing == 1){ //Continualy tries to send the msg
		if(din_timeout) t0 = get_time_in_seconds(); //get initial time
		
		sendto(fd, (tMessage*)&request, get_msg_size(request), MSG_CONFIRM,
			(struct sockaddr *) &servaddr, sizeof(struct sockaddr_in));

		printf("$ rdt_send->sendto: Message Sent\n");

		//Await for ACK
		bzero(&response, sizeof(tMessage));
		len = sizeof(struct sockaddr_in);
		res = recvfrom(fd, (tMessage*)&response, sizeof(response), 0,
				(struct sockaddr *) &servaddr, &len);

		//NÓ de Wait for ACK
		//verificar o response se foi o ACK certo e o ack do pacote enviado

		if (errno != EAGAIN) //Se nao chegou o pacote, reenviar o pacote!
		{
			printf("$ rdt_send->recvfrom: Packet recieved:");
			show_msg(response); //debug purposes

			//Check if it is corrupted:
			if (isCorrupt(response))
			{
				printf("$ rdt_send->recvfrom: Corrupted Package recieved: Trying again...\n");
				errno = 0; //reset errno to try again
			} 
			else
			{
				//Check the ack recieved:
				if (isACK(response, currMsg)){
					printf("$ rdt_send: ACK recieved\n");
					//Pacote recebido: Ajustar o timeout medio
					if(din_timeout)
					{	
						printf("$ rdt_send: Ajustando Timeout\n");
						tf = get_time_in_seconds(); //SE FOR O CERTO
						timeout = get_timeout_in_ms(t0, tf);
						set_timeout(fd, timeout);
						
					}
					
					currMsg ++;
					keepGoing = 0; //OK!
					errno = 0;
					printf("$ rdt_send: Message Succesfully Sent \n");
				} 
				else 
				{
					printf("$ rdt_send: Unexpected (or not ACK) recieved: Trying again...\n");
					errno = 0; //clear errno to go on
				}
			}	
		}
		else
		{
			printf("$ rdt_send->recv_from: Timeout! Trying again...\n");
			errno = 0; //reset errno to try again
		}
	}
}


int main(int argc, char* argv[])
{
	if(argc < 4)
	{
		printf("Use: %s <ip> <porta> <mensagem1> ... <mensagem n>\n", argv[0]);
		return(-1);
	} else if ((argc - 3) > MAX_MSG_AMOUNT) {
		printf("Max message amount exceeded, curr limit is %d", MAX_MSG_AMOUNT);
		return(-1);
	}

	//reading the ip, port and message
	char* ip, *port, *data[MAX_MSG_AMOUNT];
	ip = argv[1];
	port = argv[2];
	
	int i;
	unsigned short data_size[MAX_MSG_AMOUNT];;

	for (i = 0; i < (argc-3); i++)
	{
		data[i] = argv[i+3];
		data_size[i] = strlen(argv[i+3]);
	}
	

	//creating socket:
	int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if(fd < 0)
	{
		perror("Erro no Socket");
       	exit(EXIT_FAILURE);
	}
	for ( i = 0; i < (argc-3); i++)
	{
		rdt_send(fd, data[i], data_size[i], ip, port);
	}
	
	close(fd);
	return 0;
}