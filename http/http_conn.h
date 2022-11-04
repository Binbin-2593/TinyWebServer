#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
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
#include <map>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

class http_conn
{
public:
    static const int FILENAME_LEN = 200;//设置读取文件的名称m_real_file大小
    static const int READ_BUFFER_SIZE = 2048;//设置读缓冲区m_read_buf大小
    static const int WRITE_BUFFER_SIZE = 1024;//设置写缓冲区m_write_buf大小
    enum METHOD{//报文的请求方法，本项目只用到GET和POST（0、1）
        GET = 0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT,PATH
    };
    //主状态机的状态
    enum CHECK_STATE{
        CHECK_STATE_REQUESTLINE = 0,//解析请求行
        CHECK_STATE_HEADER,//解析请求头
        CHECK_STATE_CONTENT//解析消息体，仅用于解析POST请求
    };
    //报文解析的结果
    enum HTTP_CODE{
        NO_REQUEST,//请求不完整，需要继续读取请求报文数据,跳转主线程继续监测读事件
        GET_REQUEST,//获得了完整的HTTP请求,调用do_request完成请求资源映射
        BAD_REQUEST,//HTTP请求报文有语法错误或请求资源为目录,跳转process_write完成响应报文
        NO_RESOURCE,//请求资源不存在,跳转process_write完成响应报文
        FORBIDDEN_REQUEST,//请求资源禁止访问，没有读取权限,跳转process_write完成响应报文
        FILE_REQUEST,//请求资源可以正常访问,跳转process_write完成响应报文
        INTERNAL_ERROR,//服务器内部错误，该结果在主状态机逻辑switch的default下，一般不会触发
        CLOSED_CONNECTION
    };
    //从状态机的状态
    enum LINE_STATUS{
        LINE_OK = 0,//完整读取一行
        LINE_BAD,//报文语法有误
        LINE_OPEN//读取的行不完整
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    //初始化套接字地址，函数内部会调用私有方法init
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);
    //关闭http连接
    void close_conn(bool real_close = true);
    //处理http连接的请求，并完成响应发送
    void process();
    //读取浏览器端发来的全部数据
    bool read_once();
    //响应报文写入函数
    bool write();
    sockaddr_in *get_address(){return &m_address;}
    //同步线程初始化数据库读取表
    void initmysql_result(connection_pool *connPool);
    //重要：线程池子线程，处理IO这一动作的触发更新的两个“标志变量”
    int timer_flag;//初始化为0，IO处理失败赋值1：表示读请求报文OR写响应报文失败
    int improv;//只要去处理事件，这个事件的此值就赋值1，初始0:事件未处理，1:事件已处理

private:
    void init();
    
    //从m_read_buf读取，并处理请求报文
    HTTP_CODE process_read();
    //向m_write_buf写入响应报文数据
    bool process_write(HTTP_CODE ret);

    //主状态机解析报文中的请求行数据
    HTTP_CODE parse_request_line(char *text);
    //主状态机解析报文中的请求头数据
    HTTP_CODE parse_headers(char *text);
    //主状态机解析报文中的请求内容
    HTTP_CODE parse_content(char *text);
    //生成响应报文
    HTTP_CODE do_request();

    /*m_start_line是行在buffer中的起始位置，将该位置后面的数据赋给text
      此时从状态机已提前将一行的末尾字符\r\n变为\0\0，所以text可以直接取出完整的行进行解析*/
    //m_start_line是已经解析的字符        
    //get_line用于将指针向后偏移，指向未处理的字符
    char *get_line() { return m_read_buf + m_start_line; };
    //从状态机读取一行，分析是请求报文的哪一部分
    LINE_STATUS parse_line();
    void unmap();
    //根据响应报文格式，生成对应8个部分，以下函数均由do_request调用
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;//客户总量
    MYSQL *mysql;
    int m_state;  //读为0, 写为1

private:
    int m_sockfd;
    sockaddr_in m_address;
    //*******存储读取的请求报文数据
    char m_read_buf[READ_BUFFER_SIZE];
    //缓冲区中m_read_buf中数据的最后一个字节的下一个位置
    int m_read_idx;
    //m_read_buf读取的位置m_checked_idx
    int m_checked_idx;
    //m_read_buf中已经解析的字符个数
    int m_start_line;

    //****存储发出的响应报文数据
    char m_write_buf[WRITE_BUFFER_SIZE];
    //指示buffer中的长度
    int m_write_idx;
    //主状态机的状态
    CHECK_STATE m_check_state;
    //请求方法
    METHOD m_method;

    //以下为解析请求报文中对应的6个变量
    //存储读取文件的名称
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;//POST请求体部内容长度字段，根据此变量分辨是GETorPOST
    bool m_linger;//http长连接

    //读取服务器上的文件地址
    char *m_file_address;
    struct stat m_file_stat;
    //io向量机制iovec
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;        //是否启用的POST
    char *m_string; //存储请求体数据（用户名、密码）
    int bytes_to_send;//剩余发送字节数
    int bytes_have_send;//已发送字节数
    char *doc_root;//网站根目录，文件夹内存放请求的资源和跳转的html文件

    map<string, string> m_users;
    int m_TRIGMode;
    int m_close_log;//日志

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif
