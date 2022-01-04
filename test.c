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
	int max_fd; 			/* 监控文件描述符中最大的文件号 */
	struct timeval tv;		/* 超时返回时间 */
    int timeCount = 0;
    char buf[1024] = { 0}; 
    int reuse = 0;
    int pos = 0;
    unsigned char bcc = 0;
    
    printf("输入域名:\n");
    scanf("%s", domin);
    printf("输入端口号:\n");
    scanf("%d", &port);
    printf("输入协议:0-TCP,1-UDP\n");
    scanf("%d", &type);
    if( 1 == type)
    {
        printf("暂时不支持该协议\n");
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
        printf("失败,但是和域名是否白名单没有关系\n");
        return sock_fd;
    }
    setsockopt( sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse));
    
    ret = ConnectTcpSocket( sock_fd, serverIp, port);
    if( ret != 0)
    {
        printf("connect server [%s:%d] failed!!\n", domin, port); 
        close( sock_fd);
        printf("连接失败,再次检查域名是否已经添加到白名单中\n");
        return -1;
    }
    printf("连接成功\n");
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
    buf[ 23] = pos-24;//长度

    bcc = CreateBCC( buf, 2, pos - 3);
    buf[ pos++] = bcc;   
    
    size = write( sock_fd, buf, pos); 
    int tempi = 0;
    printf("发送数据 :\n");
    for( tempi = 0; tempi < pos; tempi++)
    {
   	printf("%02x ", ( unsigned char)buf[ tempi]); 
    }
    printf("\n");
    printf("发送成功\n");
    //memset( buf, 0, sizeof( buf));
    
    for(;;)
	{
		FD_ZERO( &s_fds);     			/* 清空s_fds集合 */
	    //FD_SET( STDIN_FILENO, &s_fds);   /* 加入标准输入集合 */
		FD_SET( sock_fd, &s_fds);			/* 加入客户端fd集合 */
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
            		printf("正常连接中,已超过%d 秒\n",  15 *timeCount);
    size = write( sock_fd, buf, pos); 
    				if( 15 * timeCount >= 600)
				{
					break;
				}
			continue;				/* time out */
		}

		if(FD_ISSET(sock_fd,&s_fds))				/* 服务器消息 */
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
				if( timeLast >= 30 && timeLast < 600)//TCP维持30秒可以认为白名单没有问题
				{
					printf("连接持续了%d秒，白名单可能已添加成功\n",timeLast);
				}
				else if( timeLast >= 600)
				{
						
					printf("连接持续了%d秒，白名单已添加成功\n",timeLast);
				}
				else
				{
				
					printf("连接仅持续了%d秒，白名单很可能没有添加成功\n",timeLast);
				}
	close(sock_fd); 
}

