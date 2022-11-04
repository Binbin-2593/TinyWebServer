#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    //向请求队列中插入任务请求
    bool append(T *request, int state);//reactor模式
    bool append_p(T *request);//proactor模式

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //是否有任务需要处理
    connection_pool *m_connPool;  //数据库
    int m_actor_model;          //模型切换，1:reactor，0:proactor
};
template <typename T>
threadpool<T>::threadpool( int actor_model, connection_pool *connPool, int thread_number, int max_requests) : m_actor_model(actor_model),m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL),m_connPool(connPool)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];//数组存放线程
    if (!m_threads)
        throw std::exception();
    for (int i = 0; i < thread_number; ++i)
    {
        //循环创建线程，并将工作线程按要求进行运行，将传出的线程ID放在线程数组中
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)//传this是为了在静态线程函数中，引用此类对象调用动态方法run()
        {
            //创建失败，释放线程数组，抛出异常
            delete[] m_threads;
            throw std::exception();
        }
        //将线程进行分离后，不用单独对工作线程进行回收
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
    //构造函数未创建请求队列
}
template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}
template <typename T>
//T *request是一种数据类型，表示需求，在本项目中一个http类对象（即一个http连接）就是一个需求
bool threadpool<T>::append(T *request, int state)
{
    m_queuelocker.lock();
    //根据硬件，预先设置请求队列的最大值
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    //属性：读为0, 写为1
    request->m_state = state;
    //添加任务
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    //信号量提醒有任务要处理
    m_queuestat.post();
    return true;
}
template <typename T>
bool threadpool<T>::append_p(T *request)
{
    m_queuelocker.lock();
    //请求队列插满就不能插了
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}
template <typename T>
//work()是工作线程的功能函数，传的实参为this，调用run()
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}
//run()，用来取出一个线程去工作
template <typename T>
void threadpool<T>::run()
{
    //***重要：这个true就体现了并发，线程池里的线程一直不停的去看请求队列有没有请求，有就取出来处理，线程池中的线程从init之后一直活着
    while (true)//线程池一直在运转
    {
        //信号量等待
        m_queuestat.wait();
        //被唤醒后先加互斥锁
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        //从请求队列中取出第一个任务
        //将任务从请求队列删除
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
            continue;
        //1是reactor模式工作线程要读写数据
        if (1 == m_actor_model)
        {
            if (0 == request->m_state)//根据状态区分读写（读为0，写为1）
            {
                //数据读取成功
                if (request->read_once())
                {
                    request->improv = 1;
                    //从连接池中取出一个数据库连接
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    //process(模板类中的方法,这里是http类)进行处理（逻辑处理工作）
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else
        {//peractor：不在工作线程中IO，在这拿数据做逻辑处理
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }
    }
}
#endif




// template <typename T>
// class ThreadPool{
// public:
//     ThreadPool(int thread_number,int max_request){
//         m_threads=new pthread_t [m_thread_number];
//         for (int i = 0; i < thread_number; ++i){
//             pthread_create(m_threads + i,NULL,work,this);
//             pthread_detach(m_threads[i]);
//         }
//     };
//     ~ThreadPool(){
//         delete[] m_threads;
//     };
//     bool append(T*request){
//         lock;
//         push_back(request);
//         unlock;
//         sem_post;
//     };
// private:
//     static void*work(void*arg){
//         threadpool* pool=(threadpool*)arg;
//         pool->run();
//     };
//     void run(){
//         while(true){
//             sem_wait;
//             lock;
//             request=m_workqueue.front();
//             pop_front;
//             unlock;

//             IO/process();
//         }
//     };
// private:
//     pthread_t *m_threads;
//     list<T*> m_workqueue;
//     int m_thread_number;
//     int max_request;
//     locker m_queuelocker;
//     sem m_requeststat;
// };


