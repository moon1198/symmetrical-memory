#include "webserver.h"

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>

void setnonblock(int fd);
void addfd(int epollfd, int fd, bool one_shot);

int WebServer::m_pipeout = -1;

WebServer::WebServer() : m_threadpool(NULL), users(NULL), m_timer_lst(NULL) {

}

WebServer::~WebServer() {
	if (m_threadpool != NULL) {
		delete m_threadpool;
	}
	if (users != NULL) {
		delete users;
	}
	if (m_timer_lst != NULL) {
		delete m_timer_lst;
	}
}

void WebServer::init(int port = 9006, int thread_num = 8) {
	m_port = port;
	m_thread_num = thread_num;
}

void WebServer::timer_lst_init() {
	m_timer_lst = new Timer_lst;
}

void WebServer::threadpool_init() {
	m_threadpool = new Threadpool<Http_client>(m_thread_num);
	assert(m_threadpool != NULL);
	users = new Http_client[MAX_FD];
}

void WebServer::event_listen() {
	int ret = 0;
	m_lisfd = socket(AF_INET, SOCK_STREAM, 0);
	assert(m_lisfd > 0);

	struct sockaddr_in my_addr;
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(m_port);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	//
	//inet_aton("121.196.245.213", &my_addr.sin_addr);
	//inet_pton(AF_INET, "121.196.245.213", &my_addr.sin_addr);
	//inet_aton("127.0.0.1", &my_addr.sin_addr);
	//sockaddr_t addr_len= sizeof();

	ret = bind(m_lisfd, (const struct sockaddr *) &my_addr, sizeof(my_addr)); 
	assert(ret != -1);

	ret = listen(m_lisfd, 10);
	assert(ret != -1);

	m_epollfd = epoll_create1(0);
	assert(m_epollfd != -1);
	Http_client::m_epollfd = m_epollfd;
	addfd(m_epollfd, m_lisfd, false);

	
	ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
	assert(ret != -1);
	setnonblock(m_pipefd[1]);
	addfd(m_epollfd, m_pipefd[0], false);
	WebServer::m_pipeout = m_pipefd[0];

	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = sig_handler;
	sigfillset(&act.sa_mask);
	ret = sigaction(SIGALRM, &act, NULL);
	assert(ret != -1);
	ret = sigaction(SIGTERM, &act, NULL);
	assert(ret != -1);

	//alarm(TIMESLOT);


}

void WebServer::event_loop() {

	bool terminate = false;
	bool timeout = false;
	epoll_event events[MAX_EVENT_NUM];

	while (!terminate) {
		std::cout << "listen..." << std::endl;
		int event_num = epoll_wait(m_epollfd, events, MAX_EVENT_NUM, -1);
		std::cout << "errno = " << errno << std::endl;
		std::cout << "event_num = " << event_num << std::endl;
		assert(event_num >= 0);

		for (int i = 0; i < event_num; ++ i) {
			int sockfd = events[i].data.fd;
			//std::cout << "sockfd = " << sockfd << std::endl;
			//std::cout << "events[i].events = " << events[i].events << std::endl;

			//new connection request
			if (sockfd == m_lisfd) {
				
				struct sockaddr peer_addr;
				socklen_t peer_addr_len = sizeof(peer_addr);
				int peer_fd = accept(m_lisfd, &peer_addr, &peer_addr_len);
				if (Http_client::m_user_num >= MAX_FD) {
					std::cout << "fd connection too busy!" << std::endl;
					continue;
				}

				users[peer_fd].new_user(peer_fd, (struct sockaddr_in*) &peer_addr);
				new_timer(peer_fd, (struct sockaddr_in*) &peer_addr);
				continue;
			}

			else if (sockfd == m_pipefd[0] && (events[i].events & EPOLLIN)) {
				deal_sig(&timeout, &terminate);
			}

			else if (events[i].events & EPOLLIN) {
				if (users[sockfd].Read()) {
					m_threadpool->push(&users[sockfd]);
				}
				continue;
			}

			else if (events[i].events & EPOLLOUT) {
				std::cout << "write....." << std::endl;

				if (!users[sockfd].Write()) {
					users[sockfd].close_conn();
				}
				continue;
			}
		}
		if (timeout) {
			timer_handler();
			timeout = false;
		}

	}
}

void setnonblock(int fd) {
	int old_stat = fcntl(fd, F_GETFL);
	int new_stat = old_stat | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_stat);
}

void addfd(int epollfd, int fd, bool one_shot) {
	epoll_event ev;
	ev.data.fd = fd;
	//edge triggle and half-close
	ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	//触发一次事件通知后，该描述符将disable，无法收到事件通知，需要再一次修改ev.events
	//可避免竞态
	if (one_shot) 
		ev.events |= EPOLLONESHOT;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
	setnonblock(fd);
}

void delfd(int epollfd, int fd) {
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
}

void modfd(int epollfd, int fd, int mode, bool one_shot) {
	epoll_event ev;
	ev.data.fd = fd;
	//edge triggle and half-close
	ev.events = EPOLLET | EPOLLRDHUP | mode;
	//触发一次事件通知后，该描述符将disable，无法收到事件通知，需要再一次修改ev.events
	//可避免竞态
	if (one_shot) 
		ev.events |= EPOLLONESHOT;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
}

void WebServer::sig_handler (int sig) {
	int prev_errno = errno;
	int msg = sig;
	send(WebServer::m_pipeout, (char *) &msg, 1, 0);
	//send(1, (char *) &msg, 1, 0);
	errno = prev_errno;
}

void WebServer::deal_sig (bool* timeout, bool* terminate) {
	char msg[1024];
	int len = recv(m_pipefd[0], msg, 1024, 0);
	for (int i = 0; i < len; ++ i) {
		switch ((int) msg[i]) {
			case SIGALRM:
				*timeout = true;
				break;
			case SIGTERM:
				*terminate = true;
				break;
			default:
				//undefine signal
				break;
		}
	}
}

void WebServer::timer_handler() {
	m_timer_lst->tick();
	alarm(TIMESLOT);
}


void WebServer::new_timer(int sockfd, struct sockaddr_in* addr) {
	Timer* tmp_timer = new Timer();

	tmp_timer->data.addr = *addr;
	tmp_timer->data.m_sockfd = sockfd;
	tmp_timer->expire = time(NULL) + 3 * TIMESLOT;
	tmp_timer->cb_func = cb_func;

	m_timer_lst->add_timer(tmp_timer);

}

void WebServer::cb_func(client_data* data) {

}


