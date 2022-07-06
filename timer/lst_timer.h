// 2022/4/26
// lst_timer头文件
// 基于升序链表的定时器，用于处理非活动连接

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

// 与用户连接相关的数据结构
struct client_data
{
    sockaddr_in address; // 连接地址
    int sockfd;          // 连接fd
    util_timer *timer;   // 连接定时器
};

// 定时器链表节点
class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

    time_t expire;
    void (*cb_func)(client_data *); // 定时器处理函数的函数指针
    client_data *user_data;
    util_timer *prev;
    util_timer *next;
};

// 基于升序链表的定时器
class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();

    // 添加定时器
    void add_timer(util_timer *timer);

    // 调整定时器时间
    void adjust_timer(util_timer *timer);

    // 删除定时器
    void del_timer(util_timer *timer);

    // 定时器执行的事件，检查链表中是否有超时事件
    void tick();

private:
    void add_timer(util_timer *timer, util_timer *lst_head);
    util_timer *head;
    util_timer *tail;
};

// 工具类
class utils
{
public:
    utils(){};
    ~utils(){};

    void init(int timeslot);

    // 对文件描述符设置非阻塞
    int setnonblocking(int fd);

    // 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    // 信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    // 定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int epollfd, const char *info);

    static int *u_pipefd;
    sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

// 定时器处理函数
void cb_func(client_data *user_data);

#endif