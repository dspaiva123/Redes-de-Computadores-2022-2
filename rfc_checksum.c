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

unsigned short currPkt = 1; //inicializa enviando o pacote 0
#define MAX_MSG 1000

struct header_t{
	unsigned int seq; //seq number
	unsigned short ack; //ZERO se pacote normal
	unsigned short checksum;
    unsigned short size_msg;
};

struct message_t{
    struct header_t h;
	unsigned char msg[MAX_MSG];
};

typedef struct message_t tMessage;

int get_msg_size(){
    return (sizeof(struct header_t) + msg.h.size_msg);
}

void exemplo(){
    tMessage msg;
    msg.h.checksum = 0;
    msg.h.checksum = checksum(&msg, get_msg_size(msg));   
    //PARA O ACK BASTA PASSAR ISSO SÓ QUE: checksum(&msg, sizeof(struct header_t));
}

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

struct timeval timeout;//Note that timeout stores de AVERAGE timeout to be set
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
    timeout.tv_sec = (uint16_t) (secSum/samples);
    timeout.tv_usec = (uint16_t) (usecSum/samples);
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

tMessage make_pkt(char ACK, void* msg, unsigned short msg_size)
{
	tMessage pkt;
	bzero(&pkt, sizeof(tMessage));
	pkt.seq = currPkt;
    pkt.ack = ACK;
	memcpy(pkt.msg, msg, msg_size);
	pkt.checksum = rfc_checksum((unsigned short *) pkt.msg, msg_size);
	return pkt;
}



int main(int argc, char const *argv[])
{
    tMessage msg;
    bzero(&msg, sizeof(tMessage));
    msg = make_pkt(0, argv[1], strlen(argv[1]));
    printf("\n%s -> CS: %d\n", msg.msg, msg.checksum);
    /* msg.checksum = rfc_checksum(msg.message,strlen(msg.message));
    printf("\n%s - %d\n", msg.message, msg.checksum); */
    
    return 0;
}
