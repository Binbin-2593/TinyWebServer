#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../log/log.h"

class util_timer;

//连接资源
struct client_data
{
    //客户端socket地址
    sockaddr_in address;
    int sockfd;
    //定时器
    util_timer *timer;
};

//定时器类，具体的为每个连接创建一个定时器，将其添加到链表中，并按照超时时间升序排列。执行定时任务时，将到期的定时器从链表中删除。
class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    //超时时间
    time_t expire;
    //回调函数
    void (* cb_func)(client_data *);
    //连接资源,(要处理的非活动)
    client_data *user_data;
    //前向定时器
    util_timer *prev;
    //后继定时器
    util_timer *next;
};
//定时器容器设计
//定时器容器类：升序链表
class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();
    /*add_timer函数，将目标定时器添加到链表中，添加时按照升序添加
        若当前链表中只有头尾节点，直接插入
        否则，将定时器按升序插入*/
    void add_timer(util_timer *timer);
    /*
    adjust_timer函数，当定时任务发生变化,调整对应定时器在链表中的位置
        1.客户端在设定时间内有数据收发,则当前时刻对该定时器重新设定时间，这里只是往后延长超时时间
        2.被调整的目标定时器在尾部，或定时器新的超时值仍然小于下一个定时器的超时，不用调整
        3.否则先将定时器从链表取出，重新插入链表*/
    void adjust_timer(util_timer *timer);
    //del_timer函数将超时的定时器从链表中删除
        //常规双向链表删除结点
    void del_timer(util_timer *timer);
    //定时任务处理函数
    void tick();

private:
    void add_timer(util_timer *timer, util_timer *lst_head);

    //创建头尾节点，其中头尾节点没有意义，仅仅统一方便调整
    util_timer *head;
    util_timer *tail;
};




//服务器首先创建定时器容器链表，然后用统一事件源将异常事件，读写事件和信号事件统一处理，根据不同事件的对应逻辑使用定时器
//信号通知处理类
class Utils
{
public:
    Utils() {}
    ~Utils() {}

    //初始化触发信号的时间周期
    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;//管道
    sort_timer_lst m_timer_lst;//定时器容器链表
    static int u_epollfd;//epoll树根的文件描述符
    int m_TIMESLOT;//每隔m_TIMESLOT时间触发SIGALRM信号
};
//回调函数
void cb_func(client_data *user_data);

#endif
