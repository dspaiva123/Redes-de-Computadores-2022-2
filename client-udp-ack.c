#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define SENDER 1

struct message_t{
	int id;
	char message[];
	char checksum[];
};

typedef struct message_t tMessage;

void rdt_send(int fd, tMessage request, tMessage response, struct sockaddr_in servaddr)
{
	sendto(fd, (tMessage*)&request, sizeof(request), MSG_CONFIRM,
		(struct sockaddr *) &servaddr, sizeof(servaddr));

	printf("Message Sent\n");



	int res, len;
	len = sizeof(struct sockaddr_in);
	res = recvfrom(fd, (tMessage*)&response, sizeof(response), MSG_WAITALL,
			(struct sockaddr *) &servaddr, &len);

	printf("Acknoledgment: %s\n", response.message);
}


void rdt_receive(int fd, tMessage request, tMessage response, struct sockaddr_in servaddr)
{
	int res, len;
	len = sizeof(struct sockaddr_in);
	res = recvfrom(fd, (tMessage*)&response, sizeof(tMessage), MSG_WAITALL,
			(struct sockaddr *) &servaddr, &len);

	printf("Received: %s\n", response.message);



	sendto(fd, (tMessage*)&request, sizeof(request), MSG_CONFIRM,
		(struct sockaddr *) &servaddr, sizeof(servaddr));

	printf("Response Sent\n");
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
	request = (*message_t) malloc(sizeof(message_t));

	tMessage response;
	response = (*message_t) malloc(sizeof(message_t));

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

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(port));
	servaddr.sin_addr.s_addr = inet_addr(ip);

	rdt_send(fd, request, response, servaddr);
	rdt_receive(fd, request, response, servaddr);

	close(fd);
	return 0;
}
