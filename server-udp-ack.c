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

#define MAX_MESSAGE_LENGHT 1000
#define RECEIVER 2

char clientMessage[MAX_MESSAGE_LENGHT];

struct message_t
{
	int id;
	char message[MAX_MESSAGE_LENGHT];
	uint8_t checksum;
};

typedef struct message_t tMessage;

float getTime()
{
	struct timeval tp;
	gettimeofday(&tp, 0);
	float seconds = tp.tv_sec;
	float microseconds = tp.tv_usec;
	return (1000 * seconds +  microseconds);
}

void rdt_send(int fd, tMessage request, tMessage response, struct sockaddr_in servaddr)
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
}

void rdt_receive(int fd, tMessage request, tMessage response, struct sockaddr_in servaddr)
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
}

int main(int argc, char **argv)
{

	if (argc != 2)
	{
		printf("Try %s <port>\n", argv[0]);
		return 0;
	}

	while (1)
	{
		int ls = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (ls == -1)
		{
			perror("Error occured while executing socket()");
			return -1;
		}

		tMessage request;
		// request = (*message_t) malloc(sizeof(message_t));

		tMessage response;
		// response = (*message_t) malloc(sizeof(message_t));

		response.id = RECEIVER;
		request.id = -1;

		struct sockaddr_in addr, caddr;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(atoi(argv[1]));
		addr.sin_family = AF_INET;
		if (bind(ls, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		{
			perror("Error occured while executing bind()");
			return -1;
		}

		int addr_len;
		int cfd;

		addr_len = sizeof(struct sockaddr_in);
		bzero(&caddr, addr_len);
		cfd = ls;
		rdt_receive(cfd, request, response, (struct sockaddr_in)caddr);

		// rdt_send(cfd, request, response, (struct sockaddr_in)caddr);
		close(cfd);
	}

	return 0;
}
