// 2022/4/26
// 数据库连接池头文件

#ifndef CONNECTION_POOL
#define CONNECTION_POOL

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include <fstream>
#include "../lock/locker.h"
#include "../log/log.h"

class connection_pool
{
public:
    std::string m_url;           // 主机地址
    std::string m_port;          // 数据库端口号
    std::string m_user;          // 登录数据库用户名
    std::string m_passwd;        // 登录数据库密码
    std::string m_database_name; // 数据库名字
    int m_is_close_log;          // 日志开关

    MYSQL *get_connection();              // 获取数据库连接
    bool release_connection(MYSQL *conn); // 释放连接
    int get_free_connection();            // 获取连接
    void destroy_pool();                  // 销毁所有连接

    static connection_pool *get_instance(); // 单例模式
    void init(std::string url, std::string user, std::string passwd, std::string db_name, int port, int max_conn, int is_close_log);

private:
    connection_pool();
    ~connection_pool();

    int m_max_conn;  // 最大连接数
    int m_cur_conn;  // 当前已使用的连接数
    int m_free_conn; // 当前空闲的连接数
    locker lock;
    list<MYSQL *> conn_list; // 连接池
    sem reserve;
};

// 数据库连接池管理类，类似于智能指针
class connection_RAII
{
public:
    connection_RAII(MYSQL **conn, connection_pool *conn_pool);
    ~connection_RAII();

private:
    MYSQL *conn_RAII;
    connection_pool *pool_RAII;
};

#endif