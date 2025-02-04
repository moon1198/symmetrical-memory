#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/select.h>

#include <assert.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define handle_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while (0)
#define P_NUMS 2

void * handler_accept(int sfd, fd_set *readFds) {

	struct sockaddr_in peer_addr;
	socklen_t peer_addr_len = sizeof(peer_addr);

	//printf("sfd = %d\n", sfd);
	int cfd = accept(sfd, (struct sockaddr *) &peer_addr, &peer_addr_len);
	//printf("cfd = %d\n", cfd);
	if (cfd == -1) 
		return NULL;
	char ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(peer_addr.sin_addr), ip, INET_ADDRSTRLEN);
	FD_SET(cfd, readFds);
	printf("tid : <%lu> fd : <%d> : Contecting to %s : %hu.\n", pthread_self(), cfd, ip, ntohs(peer_addr.sin_port));
}
void * handler_read(int cfd, fd_set *readFds) {

		char buf[1024];
		int recv_len = recv(cfd, buf, sizeof(buf), 0);
		buf[recv_len] = '\0';
		printf("%s", buf);
		int send_len = send(cfd, buf, recv_len, 0);
		

	printf("tid : <%lu> fd : <%d> : Close connection.\n", pthread_self(), cfd);
	FD_CLR(cfd, readFds);
	close(cfd);
}

int main (int argc, char*argv[]) {

	int port = 9006;
	if (argc == 2) {
		port = atoi(argv[1]);
	}
	//init my_addr
	struct sockaddr_in my_addr;
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;	//ipv4
	my_addr.sin_port = htons(port);	//port
	inet_aton("127.0.0.1", &my_addr.sin_addr);	//address


	int sfd = socket(AF_INET, SOCK_STREAM, 0);		//
	assert(sfd > 0);
	int bind_ret = bind(sfd, (const struct sockaddr *) &my_addr, sizeof(my_addr));
	listen(sfd, 4);
	assert(bind_ret != -1);

	struct sockaddr_in peer_addr;
	socklen_t peer_addr_len = sizeof(peer_addr);
	

	while (1) {
		int max_fd = 8;
		fd_set readFds;
		FD_ZERO(&readFds);
			FD_SET(sfd, &readFds);
		//for (int i = sfd; i < max_fd; ++ i) {
		//	FD_SET(i, &readFds);
		//}
		int rc = select(1024, &readFds, NULL, NULL, NULL);

		for (int i = sfd; i < 1024; ++ i) {
			if (i == sfd && FD_ISSET(i, &readFds)) {
				printf("accept\n");
				
				handler_accept(sfd, &readFds);
			}
			if (i != sfd && FD_ISSET(i, &readFds)) {
				printf("read i = %d\n", sfd);

				handler_read(i, &readFds);
			}
		}
	}

	printf("All have finished\n");

	close(sfd);
	return 0;
}
