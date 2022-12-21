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

unsigned short currMsg = 1; //inicializa esperando a mensagem 1

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

ssize_t rdt_receive(int cfd, char* data, struct sockaddr_in * caddr)
{
	//assumes that the bind was already done by caller
	int res = 0, keepGoing = 1;
	int addr_len = sizeof(struct sockaddr_in);

	tMessage response, ACK;

	while(keepGoing == 1)
	{
		bzero(&response, sizeof(tMessage));

		res = recvfrom(cfd, (tMessage *)&response, sizeof(tMessage), 0,
					(struct sockaddr *) caddr, &addr_len);
		
		
		if(!isCorrupt(response)){ //se nao esta corrompido
			
			if (isSeq(response, currMsg)) //Se eh o pacote esperado
			{
				ACK = make_msg(0, response.h.seq, NULL, 0); //Definir ACK
				memcpy(data, response.msg, response.h.size_msg);
				currMsg++;
				keepGoing = 0;
			} else { //se nao eh o esperado, o sender pode nao ter recebido o ack do pacote anterior, entao reenviar
				//seria necessario reconferir? (ex.: isSeq(response, currMsg-1))?
				ACK = make_msg(0, (currMsg - 1), NULL, 0);//Reenviar ACK do pacote anterior
				keepGoing = 1;
			}
		
			sendto(cfd, (tMessage *)&ACK, sizeof(response), MSG_CONFIRM,
				(struct sockaddr *) caddr, sizeof(*caddr));
		} //se esta corrompido, melhor deixar dar timeout la no sender! (sera?)
		else
		{
			printf("\n$ rdt_recieve->recvfrom: Pacote corrompido recebido:");
			show_msg(response);
		}
		
	}
	return res; //return the byte amount of res
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
	int cfd, addr_len = sizeof(struct sockaddr_in);

	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(atoi(argv[1]));
	addr.sin_family = AF_INET;
	if (bind(ls, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		perror("Error occured while executing bind()");
		return -1;
	}

	char recieved_msg[MAX_MSG];
	int res = 0;
	while (1)
	{
		bzero(&caddr, addr_len);
		bzero(recieved_msg, MAX_MSG);
		cfd = ls;
		res = rdt_receive(cfd, recieved_msg, (struct sockaddr_in *)&caddr);
		printf("Cliente IP(%s):Porta(%d): %d bytes: %s \n",
				inet_ntoa(caddr.sin_addr),
				ntohs(caddr.sin_port),
				res,
				recieved_msg);
		fflush(stdout);
		
	}
	close(cfd);
	return 0;
}
