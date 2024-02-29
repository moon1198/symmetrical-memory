#ifndef __HTTP_CLIENT_H_
#define __HTTP_CLIENT_H_

#include <cstddef>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define	READ_BUF_LEN 1024
#define	WRITE_BUF_LEN 4096


class Http_client
{
public:
	Http_client() : m_peer_addr(NULL) {};
	~Http_client(){};
	void init(int fd, const struct sockaddr_in *peer_addr);
	void new_user(int fd, const struct sockaddr_in *peer_addr);

	enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH};
	
	enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};
	
	enum HTTP_CODE
	{
		NO_REQUEST = 0,
		GET_REQUEST,
		BAD_REQUEST,
		NO_RESOURCE,
		FORBIDDEN_REQUEST,
		FILE_REQUEST,
		INTERNAL_ERROR,
		CLOSED_CONNECTION
	};
	
	enum LINE_STATUS 
	{
		LINE_OK = 0,	//完整行
		LINE_BAD,		//错误行
		LINE_OPEN		//不完整行
	};

	static int m_epollfd;
	static int m_user_num;
	int m_fd;


	bool Read();
	bool Write();
	void run ();


private:
	int m_read_idx;
	int m_write_idx;


	//parse
	int m_check_idx;	//location of preprocess_line
	int m_start_idx;	//location of parsing
						//
	//parse result
	METHOD m_method;
	int m_content_len;
	char* m_url;
	char* m_host;
	char* m_connect;
	char* m_version;
	char* m_content;

	char* m_file_address;
	char m_root_path[256];
	char m_file[256];
	struct stat m_file_stat;


	CHECK_STATE check_state;

	const struct sockaddr_in *m_peer_addr;
	char m_readbuf[READ_BUF_LEN];
	char m_writebuf[WRITE_BUF_LEN];

private:
	void refresh();

	HTTP_CODE parse_readbuf();
	LINE_STATUS preprocess_line(char *text);
	HTTP_CODE parse_requestline(char* line);
	HTTP_CODE parse_headers(char* line);
	HTTP_CODE parse_content(char* text);

	HTTP_CODE handle_request();

	bool fill_writebuf();

};



#endif