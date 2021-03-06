#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
//#include <linux/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define PACK_SHORT_TO_PACKET( num, buf, pos)     do {\
    ( buf)[( pos)++] = (( num) >> 8) & 0xFF;\
    ( buf)[( pos)++] = ( num) & 0xFF;\
    } while ( 0)

int GetIpFromDomain( const char *pszDomain, char *pszIp, char pszIpLen)
{
    if (pszDomain == NULL || pszIp== NULL || strlen( pszDomain) == 0)
    {
        printf("GetIpFromDomain param error\n");
        return -1;
    }

    struct addrinfo hints;
    struct addrinfo *res, *cur;
    struct sockaddr_in *addr;
    int ret;
    int find = 0;
    char ipbuf[16];
    memset( &hints, 0, sizeof(struct addrinfo));
    //res_ninit( &_res);

    hints.ai_family = AF_INET; /* Allow IPv4 */
    hints.ai_flags = AI_PASSIVE; /* For wildcard IP address */
    hints.ai_protocol = 0; /* Any protocol */
    hints.ai_socktype = SOCK_STREAM;
    ret = getaddrinfo( pszDomain, NULL, &hints, &res);
    if ( ret != 0)
    {
        printf("getaddrinfo failed, %s\n", gai_strerror( ret));
        strcpy( pszIp, pszDomain);
        //res_nclose( &_res);
        return -1;
    }
    
    for ( cur = res; cur != NULL; cur = cur->ai_next)
    {
        addr = (struct sockaddr_in *)cur->ai_addr;
        strncpy( pszIp, inet_ntop( AF_INET, &addr->sin_addr, ipbuf, 16), 16);
        find = 1;
    }
    
    if ( find == 0)
    {
        strcpy( pszIp, pszDomain);
    }
    
    if( res != NULL)
    {
        freeaddrinfo( res);
        res = NULL;
    }
    //res_nclose( &_res);
    return 0;
}

int ConnectTcpSocket(int handle, const char *address, unsigned short port)
{
    int status = 0;
    struct sockaddr_in client;
    memset(&client, 0, sizeof(client));
    client.sin_family = AF_INET;
    client.sin_port = htons(port);
    inet_pton(AF_INET, address, &client.sin_addr);
    status = connect( handle, (struct sockaddr *) &client, sizeof(client)) ;

    if (( errno == EINPROGRESS)|| ( errno == EALREADY) || ( errno == EISCONN) || ( errno == EACCES) || ( status != -1))
    {
        if(( status != -1)|| ( errno == EISCONN) || ( errno == EACCES))
        {
            return 0;
        }
        fd_set rset,wset;
        struct timeval tv;
        FD_ZERO(&rset);
        FD_ZERO(&wset);
        FD_SET(handle, &rset);
        FD_SET(handle, &wset);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        select(handle + 1, &rset, &wset, NULL, &tv);
        if(FD_ISSET(handle,&rset) || FD_ISSET(handle,&wset))
        {
            return 0;
        }
        printf("--status:[%d]-EISCONN:[%d][%d][%d][%d]-errno:[%d]-\n", status, EISCONN, EACCES, EINPROGRESS, EALREADY, errno);
        return 1;
    }

    return -1;
}

unsigned char CreateBCC(const char* buffer,int Start, int size)
{
    unsigned char BCC;    
    BCC=0;
    int i=Start;
    for ( i=Start; i<size+Start; i++)
        BCC^=buffer[i];
    return BCC;
}

int main(void)
{
    char domin[ 128] = { 0};
    int port = 0;
    int type = -1;
    char serverIp[ 16] = { 0};
    int ret = -1;
    int sock_fd= -1;
    fd_set s_fds;
    ssize_t size;
	int max_fd; 			/* ?????????????????????????????????????????? */
	struct timeval tv;		/* ?????????????????? */
    int timeCount = 0;
    char buf[1024] = { 0}; 
    int reuse = 0;
    int pos = 0;
    unsigned char bcc = 0;
    
    printf("????????????:\n");
    scanf("%s", domin);
    printf("???????????????:\n");
    scanf("%d", &port);
    printf("????????????:0-TCP,1-UDP\n");
    scanf("%d", &type);
    if( 1 == type)
    {
        printf("????????????????????????\n");
        return 0;
    }
    
    ret = GetIpFromDomain( domin, serverIp, sizeof( serverIp));
    if( ret < 0)
    {
        printf("Domain to IP failed\n");
        return ret;
    }

    sock_fd = socket( AF_INET, SOCK_STREAM, 0);
    if( sock_fd < 0)
    {
        printf("socket create failed, errno is %d\n", errno);
        printf("??????,??????????????????????????????????????????\n");
        return sock_fd;
    }
    setsockopt( sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse));
    
    ret = ConnectTcpSocket( sock_fd, serverIp, port);
    if( ret != 0)
    {
        printf("connect server [%s:%d] failed!!\n", domin, port); 
        close( sock_fd);
        printf("????????????,???????????????????????????????????????????????????\n");
        return -1;
    }
    printf("????????????\n");
    buf[ pos++] = '#';
    buf[ pos++] = '#';
    buf[ pos++] = 0x1;
    buf[ pos++] = 0xFE;
    strcpy( buf+pos, "1234567890123456");
    pos += strlen("1234567890123456");
    buf[ pos++] = 0x1;

    buf[ pos++] = 0x0;

    time_t timep;
    struct tm *p;
    time( &timep);
    p=gmtime( &timep);
    buf[ pos++] = 1900+p->tm_year - 2000;  
    buf[ pos++] = 1+p->tm_mon;  
    buf[ pos++] = p->tm_mday;  
    buf[ pos++] = p->tm_hour;  
    buf[ pos++] = p->tm_min;  
    buf[ pos++] = p->tm_sec;  
    buf[ pos++] = 0;  
    buf[ pos++] = 1;  
    strcpy( buf+pos, "10101010102020202020");
    pos += strlen("10101010102020202020");
    buf[ pos++] = 0;  
    buf[ pos++] = 0;  
    buf[ pos++] = '\0';  
    buf[ 23] = pos-24;//??????

    bcc = CreateBCC( buf, 2, pos - 3);
    buf[ pos++] = bcc;   
    
    size = write( sock_fd, buf, pos); 
    int tempi = 0;
    printf("???????????? :\n");
    for( tempi = 0; tempi < pos; tempi++)
    {
   	printf("%02x ", ( unsigned char)buf[ tempi]); 
    }
    printf("\n");
    printf("????????????\n");
    //memset( buf, 0, sizeof( buf));
    
    for(;;)
	{
		FD_ZERO( &s_fds);     			/* ??????s_fds?????? */
	    //FD_SET( STDIN_FILENO, &s_fds);   /* ???????????????????????? */
		FD_SET( sock_fd, &s_fds);			/* ???????????????fd?????? */
		max_fd = sock_fd;

		tv.tv_sec  = 15;
	        tv.tv_usec = 0;
		
		ret = select(max_fd+1, &s_fds, NULL, NULL, &tv);

		if(ret < 0)
		{
            perror("select error");
            break;
        }
		else if(ret == 0)
		{
            		timeCount++;
            		printf("???????????????,?????????%d ???\n",  15 *timeCount);
    size = write( sock_fd, buf, pos); 
    				if( 15 * timeCount >= 600)
				{
					break;
				}
			continue;				/* time out */
		}

		if(FD_ISSET(sock_fd,&s_fds))				/* ??????????????? */
		{
			memset(buf, 0, sizeof(buf)); 
			size = read(sock_fd, buf, sizeof(buf)); 
			if(size > 0) 
			{  
				printf("message recv %d Byte: \n%s\n",( int)size,buf);
			} 
			else if(size < 0)
			{
				printf("recv failed!errno code is %d,errno message is '%s'\n",errno, strerror(errno));
				break;
			} 
			else
			{
				printf("server disconnect!\n");
				break;
			} 
		}
		
	}
				int timeLast = 15 * timeCount;	
				if( timeLast >= 30 && timeLast < 600)//TCP??????30????????????????????????????????????
				{
					printf("???????????????%d????????????????????????????????????\n",timeLast);
				}
				else if( timeLast >= 600)
				{
						
					printf("???????????????%d??????????????????????????????\n",timeLast);
				}
				else
				{
				
					printf("??????????????????%d??????????????????????????????????????????\n",timeLast);
				}
	close(sock_fd); 
}

