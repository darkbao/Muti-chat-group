#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <fcntl.h>

int setnonblocking(int fd)
{
	int old_opt=fcntl(fd,F_GETFL);
	fcntl(fd,F_SETFL,old_opt | O_NONBLOCK);
	return old_opt;
}

int main( int argc, char* argv[] )
{
    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", basename( argv[0] ) );
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi( argv[2] );

    struct sockaddr_in server_address;
    bzero( &server_address, sizeof( server_address ) );
    server_address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &server_address.sin_addr );
    server_address.sin_port = htons( port );

    int sockfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( sockfd >= 0 );
    if ( connect( sockfd, ( struct sockaddr* )&server_address, sizeof( server_address ) ) < 0 )
    {
        printf( "connection failed\n" );
        exit(1);
    }

	setnonblocking(fileno(stdin));
	setnonblocking(sockfd);
    struct pollfd fds[2];
    fds[0].fd = fileno(stdin);
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLHUP;
    fds[1].revents = 0;

    int pipefd1[2];
    int ret1 = pipe( pipefd1 );
    assert( ret1 != -1 );

    int pipefd2[2];
    int ret2 = pipe( pipefd2 );
    assert( ret2 != -1 );
		
    while( 1 )
    {
        int num = poll( fds, 2, -1 );
        if( num < 0 )
        {
            printf( "poll failure\n" );
            break;
        }

        if( fds[1].revents & POLLHUP )
        {
            printf( "server close the connection\n" );
            break;
        }
        else if( fds[1].revents & POLLIN )
        {
		    ret2 = splice( sockfd, NULL, pipefd2[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_NONBLOCK );
            ret2 = splice( pipefd2[0], NULL, fileno(stdout), NULL, 32768, SPLICE_F_MORE | SPLICE_F_NONBLOCK );    
        }

        if( fds[0].revents & POLLIN )
        {
            ret1 = splice( fileno(stdin), NULL, pipefd1[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_NONBLOCK );
            if(ret1==0)
            	break;
            ret1 = splice( pipefd1[0], NULL, sockfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_NONBLOCK );
        }
    }
    
    close( sockfd );
    return 0;
}

