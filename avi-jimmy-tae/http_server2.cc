#include "minet_socket.h"
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>

#define BUFSIZE 1024
#define FILENAMESIZE 100

int handle_connection(int);
int writenbytes(int,char *,int);
int readnbytes(int,char *,int);

int main(int argc,char *argv[])
{
  int server_port;
  int sock,sock2;
  struct sockaddr_in sa,sa2; //sa = server, sa2 = client
  int rc,i;
  fd_set readlist;
  fd_set connections;
  int maxfd;
  int addrlen;
  int newfd;
  int nbytes;
  struct timeval timeout;
  char buf[256];

  /* parse command line args */
  if (argc != 3)
  {
    fprintf(stderr, "usage: http_server1 k|u port\n");
    exit(-1);
  }
  server_port = atoi(argv[2]);
  if (server_port < 1500)
  {
    fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port);
    exit(-1);
  }

  /* initialize and make socket */
  if (toupper(*(argv[1]))=='K'){
        minet_init(MINET_KERNEL);
  } else if (toupper(*argv[1])=='U'){
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

  /* set server address*/
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(server_port);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);

  /* bind listening socket */
  rc = minet_bind(sock, &sa);
  if (rc < 0){
        printf("Error binding socket.");
  }

  /* start listening */
  rc = minet_listen(sock, 5);

  /*initialize list of open connections to empty*/
  FD_ZERO(&connections);
  FD_ZERO(&readlist);
  FD_SET(sock, &connections);
  if(FD_ISSET(sock, &connections) == 0) {
        fprintf(stderr, "Failed to add sock to set!\n");
        minet_close(sock);
        exit(-1);
  }

  maxfd = sock;

  /* connection handling loop */
  while(1)
  {
    /* create read list */
	readlist = connections;
    /* do a select */
	if (minet_select(maxfd+1, &readlist, 0, 0, NULL) == -1){
		perror("select");
		exit(1);
	}

    /* process sockets that are ready */
	for (i = 0; i <= maxfd; i++){
		// REACHES HERE
		if (FD_ISSET(i, &readlist)){
			if (i == sock){
			// HANDLE NEW CONNECTION - THIS PART IS FINE
				addrlen = sizeof(sa2);
				if ((newfd = minet_accept(sock, &sa2)) == -1){
					perror("address not accepted");
				}
				else{
					FD_SET(newfd, &connections);
					if (newfd > maxfd) {
						maxfd = newfd;
					}
				}		
				printf("new connection\n");
			}
			else { 
				// HANDLE DATA FROM A CLIENT
				rc = handle_connection(i);
				FD_CLR(i, &connections);
				/*
				//IF THERE IS A TERMINATION COMMAND
				if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0){
					printf("Recv: %s\n", buf);
					if (nbytes == 0){
						printf("selectserver: socket %d hung up\n", i);
					} else {
						perror("recv");
					}
					close(i); // bye!
					FD_CLR(i, &connections);
					printf("closed connection at %d\n", i);
				} else {
				//THERE IS A REQUEST 
					printf("New: %s\n", buf);
					rc = handle_connection(i);
					FD_CLR(i, &connections);
					}
				}*/	
			}
		}
	}
    }
}

int handle_connection(int sock2)
{
  char filename[FILENAMESIZE+1];
  int rc;
  int fd;
  struct stat filestat;
  char buf[BUFSIZE+1];
  char *headers;
  char *endheaders;
  char *bptr;
  int datalen=0;
  char *ok_response_f = "HTTP/1.0 200 OK\r\n"\
                      "Content-type: text/plain\r\n"\
                      "Content-length: %d \r\n\r\n";
  char ok_response[100];
  char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                         "Content-type: text/html\r\n\r\n"\
                         "<html><body bgColor=black text=white>\n"\
                         "<h2>404 FILE NOT FOUND</h2>\n"\
                         "</body></html>\n";
  bool ok=true;

  /* first read loop -- get request and headers*/
  while ((rc = minet_read(sock2, buf + datalen, BUFSIZE - datalen)) > 0) {
	datalen += rc;
	buf[datalen] = '\0';

        if ((endheaders = strstr(buf, "\r\n\r\n")) != NULL) {
            endheaders += 4;
            break;
	}

  }

  if (rc <= 0){
	if (rc == 0){
		printf("selectserver: socket %d hung up\n", sock2);
	} else {
		perror("recv");
	}
	close(sock2); // bye!
	printf("closed connection at %d\n", sock2);
	return -1;
   }

  /* parse request to get file name */
  /* Assumption: this is a GET request and filename contains no spaces*/
  char delimiter[] = " ";
  char *method, *pathname, *remainder, *context;

  int inputLength = strlen(buf);
  char *inputCopy = (char*) calloc(inputLength + 1, sizeof(char));
  strncpy(inputCopy, buf, inputLength);

  method = strtok_r (inputCopy, delimiter, &context);
  pathname = strtok_r (NULL, delimiter, &context);
  remainder = context;

  int inputLen = strlen(pathname);
  char *pathN= (char*) calloc(inputLen + 1, sizeof(char));
  strncpy(pathN, pathname, inputLen);

  free(inputCopy); 

  /* try opening the file */
  fd=open(pathN, O_RDONLY);
  if (fd<0) {
	ok=false;
	printf("can't open file\n"); 
  }

  /* send response */
  if (ok)
  {
    rc = stat(pathN, &filestat);
	if (rc < 0){
		perror("error getting file stat");
		minet_close(sock2);
		printf("Filestat Not Working\n");
		return -1;
	}
    printf("sending response. found file\n"); 
    /* send headers */
    sprintf(ok_response, ok_response_f, filestat.st_size);
    rc = writenbytes(sock2, ok_response_f, strlen(ok_response_f));

    /* send file */
    while ((rc = read(fd, buf, BUFSIZE)) != 0){
		if (rc < 0){
			printf("Error reading file");
		}
		else{
			rc = writenbytes(sock2, buf, rc);
		}
	}

	if (rc < 0){
		printf("Unable to Send Headers");
	}
  }
  else	// send error response
  {
	rc = writenbytes(sock2, notok_response, strlen(notok_response));
   	if(rc < 0) {
		minet_perror("error writing response\n");
		minet_close(sock2);
		return -1;
	}
  }

  /* close socket and free space */
  free(pathN);
  minet_close(sock2);

  if (ok)
    return 0;
  else
    return -1;

}

int readnbytes(int fd,char *buf,int size)
{
  int rc = 0;
  int totalread = 0;
  while ((rc = minet_read(fd,buf+totalread,size-totalread)) > 0)
    totalread += rc;

  if (rc < 0)
  {
    return -1;
  }
  else
    return totalread;
}

int writenbytes(int fd,char *str,int size)
{
  int rc = 0;
  int totalwritten =0;
  while ((rc = minet_write(fd,str+totalwritten,size-totalwritten)) > 0)
    totalwritten += rc;

  if (rc < 0)
    return -1;
  else
    return totalwritten;
}

