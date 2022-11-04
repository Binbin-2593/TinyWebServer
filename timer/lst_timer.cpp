#include "lst_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst()
{
    head = NULL;
    tail = NULL;
}
//常规清空一个链表
sort_timer_lst::~sort_timer_lst()
{
    util_timer *tmp = head;
    while (tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

//添加定时器，内部调用私有成员add_timer,添加时维持升序
void sort_timer_lst::add_timer(util_timer *timer)
{
    if (!timer)//if空
    {
        return;
    }
    if (!head)//if头空，第一个的时候
    {
        head = tail = timer;
        return;
    }
    //维持升序
    if (timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    //当节点的时间值大于头节点，调用私有成员，调整内部结点，找到正确的位置插入
    add_timer(timer, head);
}
//调整定时器，任务发生变化时，调整定时器在链表中的位置（定时结束之前又有数据传输，刷新了定时）
void sort_timer_lst::adjust_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    util_timer *tmp = timer->next;
    //被调整的定时器在链表尾部
    //定时器超时值仍然小于下一个定时器超时值，不调整
    if (!tmp || (timer->expire < tmp->expire))
    {
        return;
    }
    //被调整定时器是链表头结点，将定时器取出，重新插入
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    else//被调整定时器在内部，将定时器取出，重新插入
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

//删除定时器
void sort_timer_lst::del_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    //链表中只有一个定时器，需要删除该定时器
    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    //被删除的定时器为头结点
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    //被删除的定时器为尾结点
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    //被删除的定时器在链表内部，常规链表结点删除
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}
//定时任务处理函数
/*
1.遍历定时器升序链表容器，从头结点开始依次处理每个定时器，直到遇到尚未到期的定时器
2.若当前时间小于定时器超时时间，跳出循环，即未找到到期的定时器
3.若当前时间大于定时器超时时间，即找到了到期的定时器，执行回调函数，然后将它从链表中删除，然后继续遍历*/
void sort_timer_lst::tick()
{
    if (!head)
    {
        return;
    }
    //获取当前时间
    time_t cur = time(NULL);
    util_timer *tmp = head;
    //遍历定时器链表
    while (tmp)
    {
        //当前时间小于定时器的超时时间，后面的定时器也没有到期
        if (cur < tmp->expire)
        {
            break;
        }
        //当前定时器到期，则调用回调函数，执行定时事件（事件就是epoll树上删除，关闭fd）
        tmp->cb_func(tmp->user_data);//对当前定时器节点调用

        //将处理后的定时器从链表容器中删除，并重置头结点
        head = tmp->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;//开启新一轮（因为下面的节点还有超时的）
    }
}
//主要用于调整链表内部结点
void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head)
{
    util_timer *prev = lst_head;//lst_head，此次链表调整的起始遍历节点
    util_timer *tmp = prev->next;
    //遍历当前结点之后的链表，按照超时时间找到目标定时器对应的位置，常规双向链表插入操作
    while (tmp)
    {
        if (timer->expire < tmp->expire)
        {//双向链表的调整，顾及前后两个指针指向
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if (!tmp)//当要调整的节点只有起始节点时
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

//初始化时间周期
void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

//对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT，(对添加的fd（包括信号管道、监听socket、连接socket）做一些性质参数设置)
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;
//通过参数不同设置两种触发模式
    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;//实现对端断开连接时服务器上触发的一个EPOLLIN事件的底层处理，https://blog.51cto.com/u_15284125/3052430

    if (one_shot)
        event.events |= EPOLLONESHOT;//一个socket只能同时被一个线程（进程）操作,https://blog.csdn.net/wenfan0934/article/details/113831146
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);//添加
    setnonblocking(fd);//非阻塞
}

//信号处理函数（仅仅通过管道发送信号值，不处理信号对应的逻辑，缩短异步执行时间，减少对主程序的影响）
void Utils::sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    //可重入性表示中断后再次进入该函数，环境变量与之前相同，不会丢失数据
    int save_errno = errno;
    int msg = sig;
    //将信号值从管道写端写入，传输字符类型，而非整型，由程序主循环读取
    send(u_pipefd[1], (char *)&msg, 1, 0);//socket写入函数
    //将原来的errno赋值为当前的errno
    errno = save_errno;
}

//设置信号函数（仅关注SIGTERM和SIGALRM两个信号），实现功能：捕捉信号
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    //创建sigaction结构体变量；sigaction结构体,信号的捕获与跟踪
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    //信号处理函数中仅仅发送信号值，不做对应逻辑处理
    sa.sa_handler = handler;//信号处理函数
    if (restart)
        sa.sa_flags |= SA_RESTART;//sa_flags用于指定信号处理的行为，SA_RESTART，使被信号打断的系统调用自动重新发起
    //将所有信号添加到信号集中，sa_mask：执行捕捉函数期间，临时屏蔽的信号集
    sigfillset(&sa.sa_mask);
    //执行sigaction函数，sigaction是一个内置信号捕捉函数
    assert(sigaction(sig, &sa, NULL) != -1);//外面套一个assert是为了加一个安全保障，assert( expression )：当expression为FALSE时，程序将会终止；https://www.cnblogs.com/javawebsoa/archive/2013/05/30/3108944.html
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_timer_lst.tick();//对定时器链表进行监察
    alarm(m_TIMESLOT);//一次alarm调用只会引起一次SIGALARM信号，所以我们要在一次收到信号并处理完后，重新定时，以不断出发SIGALRM信号
}
//展示错误信息
void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
//工作：epoll下树，http连接-1,由定时器处理函数tick()调用
void cb_func(client_data *user_data)
{
    //删除非活动连接在socket上的注册事件
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);

    close(user_data->sockfd);
    //减少连接数
    http_conn::m_user_count--;
}
