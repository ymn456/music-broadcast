#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <net/if.h>
#include <proto.h>
#include "medialib.h"
#include "thr_channel.h"
#include "thr_list.h"
#include "server_conf.h"


struct server_conf_st server_conf = {.rcvport = DEFAULT_RCVPORT,\
                        .mgroup = DEFAULT_MGROUP,\
                        .media_dir = DEFAULT_MEDIADIR,\
                        .runmode = RUN_DAEMON,\
                        .ifname = DEFAULT_IF};

int serversd;
struct sockaddr_in sndaddr;
struct mlib_listentry_st *list;

static void printfhelp(void)
{
    printf("-M    指定多播组\n");
    printf("-P    指定接受端口\n");
    printf("-F    前台运行\n");
    printf("-D    指定媒体库位置\n");
    printf("-I    指定网络设备\n");
    printf("-H    显示帮助\n");
}


static void daemon_exit(int s)
{
    thr_list_destroy();
    thr_channel_destroyall();
    mlib_freechnlist(list);

    syslog(LOG_WARNING,"signal-%d caught,exit now.",s);
    closelog();

    exit(0);
}

static int daemonize(void)
{
    int fd;
    pid_t pid;

    pid = fork();
    if(pid < 0)
    {
        syslog(LOG_ERR,"fork():%s",strerror(errno));
        return -1;
    }

    if(pid > 0)
        exit(0);

    fd = open("/dev/null",O_RDWR);
    if(fd < 0)
    {
        //perror("open()");
        syslog(LOG_WARNING,"open():%s",strerror(errno));
        return -2;
    }

    else
    {
        dup2(fd,0);
        dup2(fd,1);
        dup2(fd,2);

        if(fd > 2)
            close(fd);
    }

    setsid();

    chdir("/");
    umask(0);

    return 0;
}

static int socket_init(void)
{
    
    struct ip_mreqn mreq;

    serversd = socket(AF_INET,SOCK_DGRAM,0);
    if(serversd < 0)
    {
        syslog(LOG_ERR,"socket():%s",strerror(errno));
        exit(1);
    }

    inet_pton(AF_INET,server_conf.mgroup,&mreq.imr_multiaddr);
    inet_pton(AF_INET,"0.0.0.0",&mreq.imr_address);
    mreq.imr_ifindex = if_nametoindex(server_conf.ifname);
    if(setsockopt(serversd,IPPROTO_IP,IP_MULTICAST_IF,&mreq,sizeof(mreq)) < 0)
    {
        syslog(LOG_ERR,"setsockopt(IP_MUTUCAST_IF):%s",strerror(errno));
        exit(1);
    }
    
    //bind();

    sndaddr.sin_family = AF_INET;
    sndaddr.sin_port = htons(atoi(server_conf.rcvport));
    inet_pton(AF_INET,server_conf.mgroup,&sndaddr.sin_addr.s_addr);
    return 0;
}

int main(int argc,char *argv[])
{
    int c;
    struct sigaction sa;

    sa.sa_handler = daemon_exit;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask,SIGINT);
    sigaddset(&sa.sa_mask,SIGQUIT);
    sigaddset(&sa.sa_mask,SIGTERM); 

    sigaction(SIGTERM,&sa,NULL);
    sigaction(SIGINT,&sa,NULL);
    sigaction(SIGQUIT,&sa,NULL);

    openlog("netradio",LOG_PID|LOG_PERROR,LOG_DAEMON);
    //命令行分析
    while(1)
    {
        c= getopt(argc,argv,"M:P:FD:I:H");
        if(c < 0)
            break;
        switch(c)
        {
            case 'M':
                server_conf.mgroup = optarg;
                break;
            case 'P':
                server_conf.rcvport = optarg;
                break;
            case 'F':
                server_conf.runmode = RUN_FOREGROUND;
                break;
            case 'D':
                server_conf.media_dir = optarg;
                break;
            case 'I':
                server_conf.ifname = optarg;
                break;
            case 'H':
                printfhelp();
                exit(0);
                break;
            default:
                abort();
                break;

        }
        
    }

    //守护进程的实现
    if(server_conf.runmode == RUN_DAEMON)
    {
        if(daemonize() != 0)
            exit(1);
    }

    else if(server_conf.runmode == RUN_FOREGROUND)
    {
        //do nothing
    }
    else
    {
        syslog(LOG_ERR, "EINVAL server_conf.runmode.");
        exit(1);
    }

    //SOCKET初始化
    socket_init();

    //获取频道信息
    
    int list_size;
    int err;

    err = mlib_getchnlist(&list, &list_size);
    if(err)
    {
        syslog(LOG_ERR, "mlib_getchnlist():%s",strerror(errno));
        exit(1);
    }

    //创建节目单线程

    err = thr_list_create(list,list_size);
    if(err)
        exit(1);

    //创建频道线程

    int i;
    for(i= 0;i < list_size; i++)
    {
        err = thr_channel_create(list+i);
        if(err)
        {
            fprintf(stderr,"thr_channel_create():%s\n",strerror(errno));
            exit(1);
        }
    }
    syslog(LOG_DEBUG,"%d channel threads created.",i);

    while(1)
        pause();

    closelog();
    
    exit(0);
}