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
	int id;
	char message[MAX_MSG];
	uint8_t checksum;
};


typedef struct message_t tMessage;

double getTime()
{
	struct timeval tp;
	gettimeofday(&tp, 0);
	double seconds = tp.tv_sec;
	double microseconds = tp.tv_usec;
	return (1000 * seconds + microseconds);
}

void rdt_send(int fd, tMessage request, tMessage response, struct sockaddr_in servaddr)
{
	int currerrno = errno;
	int keepGoing = 1;

	double t1 = 0;
	double t2 = 0;

	while(keepGoing == 1){
		t1 = getTime();
		sendto(fd, (tMessage*)&request, sizeof(request), MSG_CONFIRM,
			(struct sockaddr *) &servaddr, sizeof(servaddr));

		printf("Message Sent\n");


		int res, len;
		len = sizeof(struct sockaddr_in);
		res = recvfrom(fd, (tMessage*)&response, sizeof(response), MSG_WAITALL,
				(struct sockaddr *) &servaddr, &len);

		if (errno != EAGAIN)
		{
			printf("Acknoledgment: %d\n", response.id);
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
