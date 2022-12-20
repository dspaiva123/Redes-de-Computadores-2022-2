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
#include <math.h>

#define MAX_MSG 1000
#define din_timeout 1

struct header_t{
	unsigned int seq; //seq number
	unsigned int ack; //ZERO se pacote normal
	unsigned short checksum;
    unsigned short size_msg;
};

struct message_t{
    struct header_t h;
	unsigned char msg[MAX_MSG];
};

typedef struct message_t tMessage;

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

int get_msg_size(tMessage msg){
    return (sizeof(struct header_t) + msg.h.size_msg);
}

/* returns a tMessage package containing:
	ACK -> 0, if is a message and ACK number if is an ACK package
	Message -> the data to be transmitted
	msg_size -> the size of the message
*/
tMessage make_msg(unsigned int seq, unsigned int ack, void* message, unsigned short msg_size)
{
	tMessage pkt;
	bzero(&pkt, sizeof(tMessage));
	pkt.h.seq = seq;
    pkt.h.ack = ack;
	memcpy(pkt.msg, message, msg_size);
    pkt.h.checksum = 0;
	pkt.h.checksum = rfc_checksum((unsigned short *) &pkt, get_msg_size(pkt));
	return pkt;
}

void show_msg(tMessage msg)
{
    if(msg.h.ack != 0) printf("\n=============\nACK Package:\n");
    else printf("\n=============\nMSG Package:\n");

    printf("|Header:\n| seq: %d\n| ack: %d\n| CS: %d\n| msg_size: %d\n|Message: \n| msg: \"%s\"\n=============\n", msg.h.seq, msg.h.ack, msg.h.checksum, msg.h.size_msg, msg.msg);
}

int isCorrupt(tMessage msg) //Verica se Checksum é diferente do esperado
{
	return (msg.h.checksum != rfc_checksum((unsigned short *) &msg, get_msg_size(msg)));
}

int isACK(tMessage msg, unsigned int expAck) //Verifica se é o ACK esperado
{
	return (msg.h.ack == expAck); //lembrar de que o reciever tem que enviar a mensagem do pacote sendo uma string de "0" ou "1"
}

/* ########## timeout.h #################*/

#define STOUS 1000000 //10**6

double estimated_rtt = 0.2; //TODO tempo em segundos estimado de rtt (obtido previamente?)
double deviation = 0; //TODO desvio


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
    double sample_rtt = final - initial;
	double alfa = 1 / 8;
	double beta = 1 / 4;

    estimated_rtt = ((1 - alfa) * estimated_rtt) + (alfa * sample_rtt);
    deviation = ((1 - beta) * deviation) + (beta * abs(sample_rtt - estimated_rtt));

    return(estimated_rtt + (4 * deviation));//the timeout to be set
}

/* set the time given in timeval struct *timeout* as timeout to the *fd* socket */
void set_timeout(int fd, double timeout_insec)
{
	struct timeval timeout = get_time_in_timeval(timeout_insec);
	if (setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
		perror("setsockopt(..., SO_RCVTIMEO ,...");
}

// ############### RDT ###############################

unsigned short currMsg = 1; //inicializa enviando a mensagem 1

void rdt_send(int fd, void * data, unsigned short data_size, char *ip, char *port)
{
	int currerrno = errno; //keep track of errno to check if there was a timeout
	int keepGoing = 1;
	int res, len;

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

	while(keepGoing == 1){
		if(din_timeout) t0 = get_time_in_seconds(); //get initial time

		sendto(fd, (tMessage*)&request, get_msg_size(request), MSG_CONFIRM,
			(struct sockaddr *) &servaddr, sizeof(servaddr));

		printf("sendto: Message Sent\n");

		//Await for ACK
		bzero(&response, sizeof(tMessage));
		len = sizeof(struct sockaddr_in);
		res = recvfrom(fd, (tMessage*)&response, sizeof(response), MSG_WAITALL,
				(struct sockaddr *) &servaddr, &len);

		//NÓ de Wait for ACK
		//verificar o response se foi o ACK certo e o ack do pacote enviado

		if (errno != EAGAIN) //Se nao chegou o pacote, reenviar o pacote!
		{
			show_msg(response); //debug purposes

			//Check if it is corrupted:
			if (isCorrupt(response))
			{
				printf("rdt_send: Corrupted Package recieved: Trying again...\n");
				errno = 0; //reset errno to try again
			} 
			else
			{
				//Check the ack recieved:
				if (isACK(response, currMsg)){
					//Pacote recebido: Ajustar o timeout medio
					if(din_timeout)
					{
						tf = get_time_in_seconds(); //INDEPENDENTE DE SE FOR O CERTO, o timeout é em cima de um pacote enviar e seu tempo a ser recevido de volta
						timeout = get_timeout_in_ms(t0, tf);
						set_timeout(fd, timeout);
					}
					
					currMsg ++;
					keepGoing = 0; //OK!
					errno = 0;
					printf("sendto: Message Succesfully Sent \n");
				} 
				else 
				{
					printf("rdt_send: ACK not recieved: Trying again...\n");
					errno = 0; //clear errno to go on
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


/*  void rdt_receive(int fd, void* data, struct sockaddr_in caddr)
{

	int res, len;
	len = sizeof(struct sockaddr_in);
	res = recvfrom(fd, (tMessage *)&response, sizeof(tMessage), MSG_WAITALL,
				   (struct sockaddr *)&servaddr, &len);

	printf("Received: %s\n", response.message);
	strcpy(clientMessage, response.message);

	sendto(fd, (tMessage *)&response, sizeof(response), MSG_CONFIRM,
		   (struct sockaddr *)&servaddr, sizeof(servaddr));

	printf("Response Sent = %s\n", clientMessage);
} */

int main(int argc, char* argv[])
{
	if(argc < 4)
	{
		printf("Use: <ip> <porta> <mensagem>\n");
		return(-1);
	}

	//reading the ip, port and message
	char* ip, *port, *data;
	ip = argv[1];
	port = argv[2];
	data = argv[3];
	unsigned short data_size = strlen(argv[3]);

	//creating socket:
	int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if(fd < 0)
	{
		perror("Erro no Socket");
       	exit(EXIT_FAILURE);
	}

	//Conffigurando o TIMEOUT (mover pro rdt_send?)
	struct timeval timeout;
	//definir timeout
	// timeout para recebimento
	if (setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
		perror("setsockopt(..., SO_RCVTIMEO ,...");

	rdt_send(fd, data, data_size, ip, port);

	close(fd);
	return 0;
}