#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define MAX_MESSAGE_LENGHT 1000

char clientMessage[];

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
	strcpy(clientMessage, response.message);



	sendto(fd, (tMessage*)&request, sizeof(request), MSG_CONFIRM,
		(struct sockaddr *) &servaddr, sizeof(servaddr));

	printf("Response Sent\n");
}


int main(int argc, char **argv) {

	if (argc != 2) {
		printf("Try %s <port>\n", argv[0]);
		return 0;
	}

	int ls = socket(AF_INET, SOCK_STREAM, IPPROTO_UDP)
	if (ls==-1) {
		 perror("Error occured while executing socket()");
		 return -1;
	}

	tMessage request;
	request = (*message_t) malloc(sizeof(message_t));

	tMessage response;
	response = (*message_t) malloc(sizeof(message_t));

	struct sockaddr_in_addr;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(atoi(argv[1]));
	addr.sin_family = AF_INET;
	if( bind(ls,(struct sockaddr *)&addr, sizeof(addr))==-1) {
		perror("Error occured while executing bind()");
		return -1;
	}
	
	int addr_len;
	int cfd;

	while (1) {
		addr_len = sizeof(struct sockaddr_in);
		bzero(&caddr, addr_len);
		cfd = accept( ls,
					(struct sockaddr *)&caddr,
					(socklen_t *)&addr_len);
		if (cfd == -1) {
			perror("Error occured while executing accept()");
			close(ls);
			return -1;
		}
		rdt_receive(cfd, request, response, struct sockaddr_in);
		rdt_send(cfd, request, response, struct sockaddr_in);
		close(cfd);
	}
	
	return 0;
}