// 2022/4/26
// sql_connection_pool实现文件

#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

connection_pool::connection_pool()
{
    m_cur_conn = 0;
    m_free_conn = 0;
}

connection_pool *connection_pool::get_instance()
{
    static connection_pool conn_pool;
    return &conn_pool;
}

// 初始化数据库连接池
void connection_pool::init(std::string url, std::string user, std::string passwd, std::string db_name,
                           int port, int max_conn, int is_close_log)
{
    m_url = url;
    m_port = port;
    m_user = user;
    m_passwd = passwd;
    m_database_name = db_name;
    m_is_close_log = is_close_log;

    for (int i = 0; i < max_conn; i++)
    {
        MYSQL *conn = NULL;
        conn = mysql_init(conn);
        if (conn == NULL)
        {
            LOG_ERROR("MySQL Error");
            exit(1);
        }

        conn = mysql_real_connect(conn, url.c_str(), user.c_str(), passwd.c_str(), db_name.c_str(), port, NULL, 0);
        if (conn == NULL)
        {
            LOG_ERROR("MySQL Error");
            exit(1);
        }
        LOG_INFO("%dth MySQl Connection Thread Create", i + 1);
        conn_list.push_back(conn);
        ++m_free_conn;
    }

    // 创建信号量
    reserve = sem(m_free_conn);
    m_max_conn = m_free_conn;
}

// 当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::get_connection()
{
    MYSQL *conn = NULL;
    if (0 == conn_list.size())
    {
        return NULL;
    }
    reserve.wait();
    lock.lock();
    conn = conn_list.front();
    conn_list.pop_front();

    --m_free_conn;
    ++m_cur_conn;

    lock.unlock();
    return conn;
}

// 释放当前使用的连接
bool connection_pool::release_connection(MYSQL *conn)
{
    if (conn == NULL)
    {
        return false;
    }
    lock.lock();
    conn_list.push_back(conn);
    ++m_free_conn;
    --m_cur_conn;
    lock.unlock();
    reserve.post();
    return true;
}

// 销毁数据库连接池
void connection_pool::destroy_pool()
{
    lock.lock();
    if (conn_list.size() > 0)
    {
        list<MYSQL *>::iterator it;
        for (it = conn_list.begin(); it != conn_list.end(); it++)
        {
            MYSQL *conn = *it;
            mysql_close(conn);
        }
        m_cur_conn = 0;
        m_free_conn = 0;
        conn_list.clear();
    }
    lock.unlock();
}

// 当前空闲的连接数
int connection_pool::get_free_connection()
{
    return m_free_conn;
}

connection_pool::~connection_pool()
{
    destroy_pool();
}

// 从数据库连接池中取出一个数据库连接对象赋给SQL，并在离开作用域时将数据库连接对象还给数据库连接池
connection_RAII::connection_RAII(MYSQL **SQL, connection_pool *conn_pool)
{
    *SQL = conn_pool->get_connection();
    conn_RAII = *SQL;
    pool_RAII = conn_pool;
}

// 在离开作用域时将数据库连接对象还给数据库连接池
connection_RAII::~connection_RAII()
{
    pool_RAII->release_connection(conn_RAII);
}