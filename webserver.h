// 2022/4/28
// webserver.h文件

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "./http/http_conn.h"
#include "./threadpool/threadpool.h"

const int MAX_FD = 65536;           // 最大文件描述符
const int MAX_EVENT_NUMBER = 10000; // 最大事件数
const int TIMESLOT = 5;             // 最小超时单位

class webserver
{
public:
    webserver(std::string dir_path);
    ~webserver();

    // 初始化服务器
    void init(int listen_port,
              string db_user, string db_passwd, string db_name,
              int is_close_log, int log_write_mode,
              int opt_linger,
              int trig_mode,
              int num_sql, int num_thread,
              int actor_mode);

    // 初始化线程池
    void thread_pool();

    // 初始化数据库连接池
    void sql_pool();

    // 初始化日志
    void log_write();

    // 初始化触发模式
    void trig_mode();

    // 设置网络监听
    void event_listen();

    // 处理各种事件
    void event_loop();

    // 初始化定时器
    void timer(int connfd, struct sockaddr_in client_address);

    // 调整定时器
    void adjust_timer(util_timer *timer);

    // 处理定时器事件
    void deal_timer(util_timer *timer, int sockfd);

    // 处理客户数据
    bool deal_clinet_data();

    // 处理信号事件
    bool deal_with_signal(bool &timeout, bool &stop_server);

    // 处理读事件
    void deal_with_read(int sockfd);

    // 处理写事件
    void deal_with_write(int sockfd);

public:
    int m_listen_port; // 监听端口号

    int m_is_close_log;   // 是否关闭日志
    int m_log_write_mode; // 日志写入方式

    int m_actor_mode; // 并发模型选择

    char *m_root; // 网站根目录

    // 数据库相关
    connection_pool *m_conn_pool;
    string m_db_user;   // 登陆数据库用户名
    string m_db_passwd; // 登陆数据库密码
    string m_db_name;   // 使用数据库名
    int m_num_sql;

    // 线程池相关
    threadpool<http_conn> *m_thread_pool;
    int m_num_thread;

    int m_pipefd[2];
    int m_epollfd;
    int m_listenfd;
    int m_opt_linger;
    int m_trig_mode;
    int m_listen_trig_mode;
    int m_conn_trig_mode;
    http_conn *users;
    epoll_event events[MAX_EVENT_NUMBER];

    // 定时器相关
    client_data *users_timer;
    utils util;
};
#endif
