/*
 * 15213 proxy-lab
 * Name: Xuan Li
 * Andrew ID: xuanli1
 *
 * DESIGN PATTERN:
 *
 * implement a proxy to get, parse the request from client,
 * connect to server with hostname and port number, and 
 * provide feedback from server to client. The feedback 
 * is stored in the LRU cache generated with linked list.  
 * 
 */


#include <stdio.h>
#include "csapp.h"
#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including these long lines in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static const char *host_hdr = "Host: %s\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";

/* major functions */
void assemble_header(rio_t *client_riop, char *header_buf,char *host, char *append);
int  parse_uri(char *uri, char *host, char *append);
void error_msg(int fd, char *cause, char *num, char *bmsg, char *dmsg);
void *thread_wrapper(void *varptr);
void thread_pro(int connfd_client);
void adjust_cache(CACHE *cache, char *uri, int connfd_client);
void get_header(char *header, char *key);

CACHE *cache;

int main(int argc, char **argv) {
    int listenfd, *connfdp, port_client;
    struct sockaddr_in client_addr;
    socklen_t client_length = sizeof(client_addr);
    pthread_t thread_id;	

    cache = cache_init();

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    port_client = atoi(argv[1]);
    Signal(SIGPIPE, SIG_IGN);

    if ((listenfd = Open_listenfd(port_client)) < 0) {
        fprintf(stderr, "Error: open_listenfd\n");
        exit(0);
    }

    while (1) {
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *) &client_addr, &client_length);
        Pthread_create(&thread_id, NULL, thread_wrapper, connfdp);
    }

    return 0;
}

/* 
 * get information from client's request and 
 * forward to the serve with a reassembled one
 */
void assemble_header(rio_t *rioptr, char *headerbuf, char *host, char *append) {
    char hostbuf[MAXLINE], requestbuf[MAXLINE], 
         extrbuf[MAXLINE],      index[MAXLINE],
	                  tempHeadbuf[MAXLINE];

    Rio_readlineb(rioptr, requestbuf, MAXLINE);

    while (strcmp(requestbuf, "\r\n")) {
        sprintf(hostbuf, host_hdr, host);
        get_header(requestbuf, index);
        if (!strcmp(index, "Host")) {
            memset(hostbuf, 0, strlen(hostbuf));
            strcpy(hostbuf, requestbuf);
        }
        else if (strcmp(index, "User-Agent"       ) &&
                 strcmp(index, "Accept"           ) &&
                 strcmp(index, "Accept-Encoding"  ) &&
                 strcmp(index, "Connection"       ) &&
                 strcmp(index, "Proxy-Connection")) {
            strcpy(extrbuf, requestbuf);
        }

    if (Rio_readlineb(rioptr, requestbuf, MAXLINE) < 0) {
            break;
        }
    }
   	
    sprintf(tempHeadbuf, "GET %s HTTP/1.0\r\n", append);
    strcat(headerbuf, tempHeadbuf         );
    strcat(headerbuf, hostbuf             );
    strcat(headerbuf, user_agent_hdr      );
    strcat(headerbuf, accept_hdr          );
    strcat(headerbuf, accept_encoding_hdr );
    strcat(headerbuf, connection_hdr      );
    strcat(headerbuf, proxy_connection_hdr);
    strcat(headerbuf, "\r\n"              );
    strcat(headerbuf, extrbuf             );

    return;
}   

/*
 * parse and store uri to get hostname and port number.
 */
int parse_uri(char *uri, char *host, char *append) {
    int portnum;
    char *port_index;
    char uri_buf[MAXLINE], port_buf[MAXLINE];
    char *uri_buf_ptr = uri_buf;
  
    strcpy(uri_buf, uri);

    if (!strncmp(uri_buf, "http://", 7)) {
        uri_buf_ptr += 7;
    }

    while (*uri_buf_ptr != ':' && *uri_buf_ptr != '/') {
        *host++ = *uri_buf_ptr++;
    }

    *host = '\0';
    if (*uri_buf_ptr == '/') {
        while (*uri_buf_ptr != '\0') {
            *append++ = *uri_buf_ptr++;
        }
        portnum = 80;
    }

    else {
        uri_buf_ptr++;
	// point to the start of port
        port_index = uri_buf_ptr;
        while (*uri_buf_ptr != '/') {
            uri_buf_ptr++;
        }
        *uri_buf_ptr = '\0';
	// get port 
        strcpy(port_buf, port_index);
        *uri_buf_ptr = '/';
	// convert to integer
        portnum = atoi(port_buf);
        strcpy(append, uri_buf_ptr);
    }
    return portnum;
}

/* 
 * print error message on client page
 */
void error_msg(int fd, char *cause, char *num, char *bmsg, char *dmsg) {
    char buf[MAXLINE], body[MAXBUF];
    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, num, bmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, dmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", num, bmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

/*
 * a wrapper for function doit()
 */
void *thread_wrapper(void *varptr) {
    int connfd_client = *((int *)varptr);
    Pthread_detach(pthread_self());
    Free(varptr);
    thread_pro(connfd_client);
    Close(connfd_client);
    return NULL;
}

/*
 * major client-server interaction process
 */ 
void thread_pro(int connfd_client) {
    char client_request[MAXLINE], method[MAXLINE], 
	 uri[MAXLINE], version[MAXLINE];
    rio_t rio_client;
    //get request from client
    Rio_readinitb(&rio_client, connfd_client);
    if (Rio_readlineb(&rio_client, client_request, MAXLINE) < 0) {
        Close(connfd_client);
        unix_error("Rio_readn ECONNRESET error");
    }
    sscanf(client_request, "%s %s %s", method, uri, version);
    //check if the method is get
    if (strcmp(method, "GET") != 0) {
        error_msg(connfd_client, method, "501", "Invalid Implement",
                    "The method is not supported in proxy.");
        return;
    }

    if (cache_check(cache, uri)) {
        adjust_cache(cache, uri, connfd_client);
    }
   
    else {
        printf("Cache not hit\n");
        char host[MAXLINE], append[MAXLINE];
        int server_port = parse_uri(uri, host, append);
        if (((server_port < 1000) || (server_port > 65535))
        			  && (server_port != 80)) {
            printf("Invalid port number (out of range).\n");
            exit(0);
        }
	int server_fd = open_clientfd_r(host, server_port);
        if ( server_fd  < 0) {
            error_msg(connfd_client, "GET", "999", "connection error",
                        "unable to make connection to server");
            Close(connfd_client);
            return;
        }
	rio_t rio_server;
	char header_server[MAXLINE];
        assemble_header(&rio_client, header_server, host, append);
        if (rio_writen(server_fd, header_server, strlen(header_server)) < 0) {
            if (errno == EPIPE) {
                printf("error: unable to send data to server\n");
                Close(server_fd);
                return;
            }
        }
	Rio_readinitb(&rio_server, server_fd);
        int size = 0;
	int length = 0;
        char add_buf[MAXLINE];
	char object[MAX_OBJECT_SIZE];
        while ((length = Rio_readlineb(&rio_server, add_buf, MAXLINE)) != 0) {
            if (size + length <= MAX_OBJECT_SIZE) {
                memcpy(object + size, add_buf, length);
                size += length;
            }
            if (rio_writen(connfd_client, add_buf, length) < 0) {
                printf("error: unable to send data to client\n");
            }
            memset(add_buf, 0, strlen(add_buf));
        }
        if (size <= MAX_OBJECT_SIZE) {
            cache_update(cache, uri, object, size);
        }
        Close(server_fd);
    }
}                                                                                                   
/*
 * help function: to get client's header
 */
void get_header(char *header, char *key) {
    char key_buf[MAXLINE];
    char *ptr = key_buf;

    strcpy(key_buf, header);

    while (*ptr != ':' && *ptr != '\0') {
        ptr++;
    }
    if (*ptr == ':') {
        *ptr = '\0';
        strcpy(key, key_buf);
    }
    return;
}

/*
 * send the request information from cache when the requested 
 * information (url) is in the cache
 */
void adjust_cache(CACHE *cache, char *uri, int connfd_client) {
    CACHE_B *cached_object =cache_get(cache, uri);
    //write back to client
    if (rio_writen(connfd_client, cached_object->data, cached_object->size) < 0) {
        printf("Error occured when writing to client\n");
        if(errno == EPIPE) {
            Close(connfd_client);
        }
    }
}                                                        
