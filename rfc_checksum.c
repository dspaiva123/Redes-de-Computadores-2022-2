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

#define MAX_MSG 1000
struct message_t{
	char id; //0 ou 1 ou 2 (ack)
	char message[MAX_MSG];
	uint16_t checksum;
};

typedef struct message_t tMessage;

uint16_t rfc_checksum(void* addr,size_t count) {
    register long long sum = 0;

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
u_int16_t check(char data[1000]){
    return rfc_checksum(data, sizeof(data)*8);
}
void test(char data[1000]){
    //printf("%d + %d", data[0], sizeof(data));
    printf("\n%d\n", check(data));
}

int isCorrupt(tMessage pkt) //VERIFICA SE O CHECKSUM CALCULADO É DIFERENTE DO DO ENVIADO!
{
	return (pkt.checksum != rfc_checksum(pkt.message,strlen(pkt.message)));
}

int main(int argc, char const *argv[])
{
    struct timeval t1, t2;
    double ms1,ms2;

   /*  for(int i = 0; i < 10; i++){
        ms1 = getTime_ms(&t1);
        printf("\n%ld e %ld\n", t1.tv_sec, t1.tv_usec);
        sleep(i);
        ms2 = getTime_ms(&t2);
        printf("\n%ld e %ld\n", t2.tv_sec, t2.tv_usec);
        ev_timeout(&t1, &t2);
        printf("\nTIMEOUT: %ld e %ld\n", timeout.tv_sec, timeout.tv_usec);
    } */

    tMessage msg;
    char pkt = 1;
    strcpy(msg.message, argv[1]);
    uint16_t ev = rfc_checksum(argv[1], strlen(argv[1]));
    printf("\n%s - %d\n", argv[1], ev);
    msg.checksum = rfc_checksum(msg.message,strlen(msg.message));
    printf("\n%s - %d\n", msg.message, msg.checksum);
    printf("\n%d\n", !pkt);
    //test(msg.message);

    
    return 0;
}
