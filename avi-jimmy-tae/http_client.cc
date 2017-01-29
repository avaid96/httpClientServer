#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>

#define BUFSIZE 1024

int write_n_bytes(int fd, char * buf, int count);

int main(int argc, char * argv[]) {
    char * server_name = NULL;
    int server_port = 0;
    char * server_path = NULL;

    int sock = 0;
    int rc = -1;
    int datalen = 0;
    bool ok = true;
    struct sockaddr_in sa;
    FILE * wheretoprint = stdout;
    struct hostent * site = NULL;
    char * req = NULL;

    char buf[BUFSIZE + 1];
    char * bptr = NULL;
    char * bptr2 = NULL;
    char * endheaders = NULL;
   
    struct timeval timeout;
    fd_set set;

    /*parse args */
    if (argc != 5) {
	fprintf(stderr, "usage: http_client k|u server port path\n");
	exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];

    /* initialize minet */
    if (toupper(*(argv[1])) == 'K') { 
	minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') { 
	minet_init(MINET_USER);
    } else {
	fprintf(stderr, "First argument must be k or u\n");
	exit(-1);
    }

    /* create socket */
    sock = minet_socket(SOCK_STREAM);
    if(sock==-1) {
	printf("Failed to create socket");
	return sock;
    }
    
    // Do DNS lookup
    /* Hint: use gethostbyname() */
    site = gethostbyname(server_name);
    if(site==NULL) {
	printf("Failed to resolve hostname");
	minet_close(sock);
	return -1;
    }
    
    /* set address */
    /* set address */
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(server_port);
    sa.sin_addr.s_addr = *(unsigned long *)site -> h_addr_list[0];

    /* connect socket */
    if(minet_connect(sock, (struct sockaddr_in *) &sa)!= 0) {
	minet_close(sock);
	return -1;
    }

    /* create buffer for request */
    char *request;
    request=(char *)malloc(strlen("GET  HTTP/1.0\r\n\r\n")+strlen(server_path)+1);
    sprintf(request, "GET %s HTTP/1.0\r\n\r\n", server_path);
   
    /* send request */
    int count = minet_write(sock, request, strlen(request));
    if(count < 0) {
	fprintf(stderr, "Failed to send request!\n");
	minet_close(sock);
	return -1;
    }

    /* wait till socket can be read */

    FD_ZERO(&set);
    FD_SET(sock, &set);
    if(FD_ISSET(sock, &set) == 0) {
 	fprintf(stderr, "Failed to add sock to set!\n");
	minet_close(sock);
	exit(-1);
    }
	
    /* Hint: use select(), and ignore timeout for now. */
    rc = minet_select(sock+1, &set, 0, 0, &timeout);
    if(rc<0) {
	fprintf(stderr, "Failed to select!\n");
	minet_close(sock);
	exit(-1);
    }

    /* first read loop -- read headers */
    while ((rc = minet_read(sock, buf + datalen, BUFSIZE - datalen)) > 0) {
	datalen += rc;  
	buf[datalen] = '\0';

	if ((endheaders = strstr(buf, "\r\n\r\n")) != NULL) {
	    endheaders += 4;
	    break;
	}
    }  

    if (rc < 0) {
	minet_perror("Can't find headers");
	exit(-1);
    }
    
    /* examine return code */   
    //Skip "HTTP/1.0"
    //remove the '\0'
    // Normal reply has return code 200
    //printf("After \n");
        
    bptr = buf;
    bptr2 = strsep(&bptr, " "); 
    bptr2[strlen(bptr)] = ' ';
    bptr2 = strsep(&bptr, " ");

    int errCode = atoi(bptr2);
    if(atoi(bptr2) != 200) {
	ok = false;
        wheretoprint = stderr;
    }
    bptr2[strlen(bptr2)] = ' ';	   

    /* print first part of response */

    if(ok) {
	fprintf(wheretoprint, "%s", endheaders);
    } else {
	//fprintf(wheretoprint, "%s", buf);
	fprintf(wheretoprint, "%s\n", endheaders);
    }

    /* second read loop -- print out the rest of the response */
    while ((rc = minet_read(sock, buf, BUFSIZE)) != 0) {
	if (rc < 0) {	     
	    minet_perror("all data not read ");
	    break;
	}

	datalen += rc;
	buf[rc] = '\0';
	fprintf(wheretoprint, "%s", buf);
    }
    
    /*close socket and deinitialize */
    free(req);
    minet_close(sock);
    minet_deinit();

    if (ok) {
	return 0;
    } else {
	return -1;
    }
}

int write_n_bytes(int fd, char * buf, int count) {
    int rc = 0;
    int totalwritten = 0;

    while ((rc = minet_write(fd, buf + totalwritten, count - totalwritten)) > 0) {
	totalwritten += rc;
    }
    
    if (rc < 0) {
	return -1;
    } else {
	return totalwritten;
    }
}


