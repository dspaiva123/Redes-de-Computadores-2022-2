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


unsigned short currMsg = 1; //inicializa enviando o pacote 1
#define MAX_MSG 1000

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

int get_msg_size(tMessage msg){
    return (sizeof(struct header_t) + msg.h.size_msg);
}

/* void exemplo(){
    tMessage msg;
    msg.h.checksum = 0;
    msg.h.checksum = checksum(&msg, get_msg_size(msg));   
    //PARA O ACK BASTA PASSAR ISSO SÓ QUE: checksum(&msg, sizeof(struct header_t));
} */

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

/* ########## timeout.h #################*/

#define STOUS 1000000

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

double get_time_in_seconds(struct timeval *tp)
{
	gettimeofday(tp, 0);
	return ((double) tp->tv_sec + ((double) tp->tv_usec / STOUS) );
}

double get_timeout_in_ms(double sample_rtt)
{
    double alfa = 1 / 8;
    estimated_rtt = ((1 - alfa) * estimated_rtt) + (alfa * sample_rtt);

    double beta = 1 / 4;
    deviation = ((1 - beta) * deviation) + (beta * abs(sample_rtt - estimated_rtt));

    return(estimated_rtt + (4 * deviation));
}
/* ########## MAIN PROGRAM #################*/
/* u_int16_t check(char data[1000]){
    return rfc_checksum(data, sizeof(data)*8);
}
void test(char data[1000]){
    //printf("%d + %d", data[0], sizeof(data));
    printf("\n%d\n", check(data));
}

int isCorrupt(tMessage pkt) //VERIFICA SE O CHECKSUM CALCULADO É DIFERENTE DO DO ENVIADO!
{
	return (pkt.checksum != rfc_checksum(pkt.message,strlen(pkt.message)));
} */

tMessage make_msg(char ACK, void* message, unsigned short msg_size)
{
	tMessage pkt;
	bzero(&pkt, sizeof(tMessage));
	pkt.h.seq = currMsg;
    pkt.h.ack = ACK;
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

int main(int argc, char const *argv[])
{
    tMessage msg;
    bzero(&msg, sizeof(tMessage));
    msg = make_msg(1, NULL, 0);
    show_msg(msg);
    bzero(&msg, sizeof(tMessage));
    msg = make_msg(0, argv[1], strlen(argv[1]));
    show_msg(msg);
    /* msg.checksum = rfc_checksum(msg.message,strlen(msg.message));
    printf("\n%s - %d\n", msg.message, msg.checksum); */
    
    return 0;
}
