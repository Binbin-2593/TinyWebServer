/*
 * @Author: Binbin-2593 1600382936@qq.com
 * @Date: 2021-08-29 18:29:59
 * @LastEditors: Binbin-2593 1600382936@qq.com
 * @LastEditTime: 2022-07-18 23:39:06
 * @FilePath: /TinyWebServer-master/main.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "config.h"
//main这样写的好处是，main本质上就是提供了入口，入口不需要太复杂
int main(int argc, char *argv[])
{
    //需要修改的数据库信息,登录名,密码,库名
    //数据库是这个服务器所要所要操作的数据资源
    string user = "root";
    string passwd = "root";
    string databasename = "qgydb";

    //命令行解析
    Config config;
    config.parse_arg(argc, argv);

    WebServer server;

    //初始化
    server.init(config.PORT, user, passwd, databasename, config.LOGWrite, 
                config.OPT_LINGER, config.TRIGMode,  config.sql_num,  config.thread_num, 
                config.close_log, config.actor_model);
    

    //日志
    server.log_write();

    //数据库
    server.sql_pool();

    //线程池
    server.thread_pool();

    //触发模式
    server.trig_mode();

    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;
}