#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char* user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static void doit(int connfd);
static void parse_uri(char* uri, char* hostname, char*path, int* port);
static void build_http_request_to_endServer(char* request_to_endServer, char* hostname, char* path, int port, rio_t* rio_client);
static int connect_endServer(char *hostname,int port);
static void pthread_doit(void* connfd);

static void parse_uri(char* uri, char* hostname, char*path, int* port) {
	printf("enter %s+\n",__FUNCTION__);
	// 需要解析出主机名，文件路径名，如果有端口号的话规定端口，没有则默认80
	*port = 80;
	char* pos = NULL;
	pos = strstr(uri, "http://");
	if(pos != uri){
		printf("the uri is invalid\n");
		return;
	}
	pos+=strlen("http://");
	char* pos2 = strstr(pos, ":");
	if(pos2 != NULL){
		// 有特定的端口号
		*pos2 = '\0';
		sscanf(pos, "%s", hostname);
		sscanf(pos2+1, "%d%s", port, path);
	} else {
		pos2 = strstr(pos, "/");
		if(pos2 != NULL){
			// 有文件路径
			*pos2 = '\0';
			sscanf(uri, "%s", hostname);
			*pos2='/';
			sscanf(pos2, "%s", path);
		} else {
			// 无文件路径
			sscanf(pos, "%s", hostname);
		}
	}
}

static void build_http_request_to_endServer(char* request_to_endServer, char* hostname, char* path, int port, rio_t* rio_client_ptr) {
	// 已知要访问的服务器的主机名，端口号，还有客户端向我们发过来的rio_t结构体（之前仅仅解析了request头）；现在需要构造一个新的请求发送给服务器
	char request_header[MAXLINE];
	sprintf(request_header, "GET %s HTTP/1.0\r\n", path); // 这里实际上是利用了之前读取的请求行的内容，并且加上了parse_uri解析的内容

	// 从client-proxy文件描述符中读取请求头，生成新请求头
	char buf[MAXLINE]; // 局部缓存变量
	char host_buf[MAXLINE];
	char other_hdr[MAXLINE];//存放其他头部

	while(Rio_readlineb(rio_client_ptr, buf, MAXLINE) > 0) {
		// EOF, 请求头 报文 以“\r\n”结尾
		if(strcmp(buf, "\r\n")==0){
			break;
		}
		
		if(!strncasecmp(buf, "Host",sizeof("Host"))) {
			strcpy(host_buf, buf); // 如果是host的话，先将这一行添加到host缓存中
			continue;
		}

		// strncasecmp是忽略大小写比较前n个字符，相同则返回0
		// Q：函数头读取,strncasecmp，相同为0，不同为非0;原代码是&&，无法理解这里为什么不用||？
		// A：这里可能是因为要复制其他的头，我们是拷贝这3个头之外的其他的头
		if(!strncasecmp(buf,"Connection",strlen("Connection"))
                ||!strncasecmp(buf,"Proxy-Connection",strlen("Proxy-Connection"))
                ||!strncasecmp(buf,"User-Agent",strlen("User-Agent")))
        {
			continue; // 这三个都跳过，其他的requestheader完全复制
        }

    	strcat(other_hdr,buf);//其他的完全复制
	}
	
	if(strlen(host_buf)==0)
    {
        sprintf(host_buf,"Host: %s\r\n",hostname); // 如果没能从请求头中读取到host，那么应该根据uri生成一个hostname作为请求头
    }

	sprintf(request_to_endServer,"%s%s%s%s%s%s%s",
            request_header,
            host_buf,
            "Connection: close\r\n",
            "Proxy-Connection: close\r\n",
            user_agent_hdr,
            other_hdr,
            "\r\n");

    return ;
}

static int connect_endServer(char* hostname, int port) {
	char buf[MAXLINE];
	sprintf(buf, "%d", port);
	return Open_clientfd(hostname, buf);
}

static void pthread_doit(void* connfd) {
	Pthread_detach(pthread_self()); // ?
	int fd = (int)connfd;
	// 以下实际上和part1相同
	doit(fd);
	Close(fd);
}

static void doit(int connfd) {
	char buf[MAXLINE];
	char uri[MAXLINE];
	char method[MAXLINE];
	char version[MAXLINE];

	rio_t rio_client; // 用于读取客户端的请求
	Rio_readinitb(&rio_client, connfd); // connfd是从client 和 proxy之间的表述符号，从proxy指向client
	Rio_readlineb(&rio_client, buf, MAXLINE); // 仅读取请求行，缓存从fd读取字符串，最多读取MAXLINE字节
	// 注意，此时已经读取了一行，即请求行(request line)，后面接着的是请求头(request header)
	
	sscanf(buf, "%s %s %s", method, uri, version); // 解析请求行 
	if(strcasecmp(method, "GET")){
		printf("proxy does not implement this method\n");
		return;
	}

	char hostname[MAXLINE];
	char path[MAXLINE];
	int port = 0;
	parse_uri(uri, hostname, path, &port); // 解析uri

	char request_to_endServer[MAXLINE];
	build_http_request_to_endServer(request_to_endServer, hostname, path, port, &rio_client); // 建立发送到服务器的http请求

	// 代理proxy建立与服务器的连接
	int end_connfd = connect_endServer(hostname, port); // 从proxy指向服务器的描述符
	if(end_connfd < 0){
		printf("proxy connect to the end_server failed\n");
		return;
	}
	printf("begin to send to server+\n");
	// 向服务器发送构建的http请求
	rio_t rio_server;
	Rio_readinitb(&rio_server, end_connfd);
	Rio_writen(end_connfd, request_to_endServer, strlen(request_to_endServer)); // 向socket的fd写入http请求
	// 接收http请求
	char buf_read_from_server[MAXLINE];
	size_t n; // 接收了多少字节
	while((n = Rio_readlineb(&rio_server, buf_read_from_server,MAXLINE)) != 0) {
        printf("proxy received %d bytes,then send\n",n);
        Rio_writen(connfd,buf_read_from_server,n); // 转发到客户端
	}

    Close(end_connfd);
}


int main(int argc, char** argv)
{
    printf("%s", user_agent_hdr);
	int listenfd, connfd;
	char hostname[MAXLINE];
	char port[MAXLINE];
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
	// check command-line args
	if (argc != 2){
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}

	listenfd = Open_listenfd(argv[1]);
	while(1){
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen); // 读取客户端的请求，建立fd
		Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
		printf("Accepted connection from (%s,%s)\n", hostname, port);
		pthread_t tid;
		Pthread_create(&tid, NULL, pthread_doit, (void*)connfd);
	}

    return 0;
}

