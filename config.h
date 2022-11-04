/*
 * @Author: Binbin-2593 1600382936@qq.com
 * @Date: 2021-08-29 18:29:59
 * @LastEditors: Binbin-2593 1600382936@qq.com
 * @LastEditTime: 2022-07-18 15:13:02
 * @FilePath: /TinyWebServer-master/config.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "webserver.h"

using namespace std;

class Config
{
public:
    Config();
    ~Config(){};

    void parse_arg(int argc, char*argv[]);

    //端口号
    int PORT;

    //日志写入方式
    int LOGWrite;

    //触发组合模式
    //默认是最低效模式（LT+LT），传参1，实现服务器最高性能
    int TRIGMode;

    //listenfd(监听socket)触发模式，默认LT
    int LISTENTrigmode;

    //connfd（连接socket）触发模式，默认LT
    int CONNTrigmode;

    //优雅关闭链接
    int OPT_LINGER;

    //数据库连接池数量
    int sql_num;

    //线程池内的线程数量
    int thread_num;

    //是否关闭日志
    int close_log;

    //并发模型选择
    int actor_model;
};

#endif