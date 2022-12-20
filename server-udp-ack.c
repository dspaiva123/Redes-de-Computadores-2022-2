#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/time.h>

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


/* void rdt_send(int fd, tMessage request, tMessage response, struct sockaddr_in servaddr)
{

	int currerrno = errno;
	int keepGoing = 1;

	double t1 = 0;
	double t2 = 0;

	while(keepGoing == 1)
	{
		t1 = getTime();
		sendto(fd, (tMessage*)&request, sizeof(request), MSG_CONFIRM,
			(struct sockaddr *) &servaddr, sizeof(servaddr));

		printf("Response Sent\n");

		int res, len;
		len = sizeof(struct sockaddr_in);
		res = recvfrom(fd, (tMessage*)&response, sizeof(response), MSG_WAITALL,
				(struct sockaddr *) &servaddr, &len);

		if(errno != EAGAIN)
		{
			printf("Acknoledgement: %d\n", response.id);
			keepGoing = 0;
			t2 = getTime();
			double timeElapsed = t2 - t1;
			printf("Elapsed = %f\n", timeElapsed);
		}
		else
		{
			printf("Timeout! Trying again...\n");
			errno = 0; //reset errno to try again
		}
	}
} */

ssize_t rdt_receive(int cfd, void* data)
{
	//assumes that the bind was already done by caller
	int res = 0, len = 0;

	tMessage response;
	bzero(&response, sizeof(tMessage));

	struct sockaddr_in caddr;
	int addr_len = sizeof(struct sockaddr_in);
	bzero(&caddr, addr_len);

	len = sizeof(struct sockaddr_in);
	res = recvfrom(cfd, (tMessage *)&response, sizeof(tMessage), MSG_WAITALL,
				   (struct sockaddr *)&caddr, &len);

	show_msg(response);
	
	//FALTA AS CONDICOES
	//VER SE É O PACOTE ESPERADO!!!
	tMessage ACK;
	//CRIAR O ACK
	ACK = make_msg(0, response.h.seq, NULL, 0);
	sendto(cfd, (tMessage *)&ACK, sizeof(response), MSG_CONFIRM,
		   (struct sockaddr *)&caddr, sizeof(caddr));
	strcpy(data, response.msg);
}

int main(int argc, char **argv)
{

	if (argc != 2)
	{
		printf("Try %s <port>\n", argv[0]);
		return 0;
	}

	int ls = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (ls == -1)
		{
			perror("Error occured while executing socket()");
			return -1;
		}

	struct sockaddr_in addr, caddr;
	int cfd;

	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(atoi(argv[1]));
	addr.sin_family = AF_INET;
	if (bind(ls, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		perror("Error occured while executing bind()");
		return -1;
	}

	char recieved_msg[MAX_MSG];

	while (1)
	{
		bzero(recieved_msg, MAX_MSG);
		cfd = ls;
		rdt_receive(cfd, recieved_msg);
		
	}
	close(cfd);
	return 0;
}
