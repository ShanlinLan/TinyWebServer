// 2022/4/27
// http_conn头文件

#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

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
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>
#include <mutex>

#include "../lock/locker.h"
#include "../log/log.h"
#include "../timer/lst_timer.h"
#include "../CGImysql/sql_connection_pool.h"

class http_conn
{
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;

    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };

    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

    static int m_epollfd;    // 事件注册表fd
    static int m_user_count; // 用户数量
    MYSQL *mysql;            // mysql连接
    int m_state;             // 读为0，写为1
    int timer_flag;
    int improv;

public:
    http_conn() {}
    ~http_conn() {}

    // 初始化连接
    void init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode, int close_log,
              std::string user, std::string passwd, std::string sqlname);

    // 关闭连接
    void close_conn(bool real_close = true);

    // 处理请求
    void process();

    // 读入数据
    bool read_once();

    // 写入数据
    bool write();

    // 获取连接地址
    sockaddr_in *get_address()
    {
        return &m_address;
    }

    // 初始化mysql连接
    void init_mysql_result(connection_pool *conn_pool);

private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() { return m_read_buf + m_start_line; }
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

private:
    int m_sockfd;                      // 客户连接fd
    sockaddr_in m_address;             // 客户连接地址
    char m_read_buf[READ_BUFFER_SIZE]; // 读缓冲
    int m_read_idx;
    int m_checked_idx;
    int m_start_line;
    char m_write_buf[WRITE_BUFFER_SIZE]; // 写缓冲
    int m_write_idx;
    CHECK_STATE m_check_state;      // 状态机所处状态
    METHOD m_method;                // 请求方法
    char m_real_file[FILENAME_LEN]; //请求文件名
    char *m_url;                    // 请求资源路径
    char *m_version;                // 协议版本
    char *m_host;                   // 请求主机号
    int m_content_length;           // 请求体长度
    bool m_linger;                  // 是否保持连接
    char *m_file_address;           // 请求资源地址
    struct stat m_file_stat;        // 资源状态
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;        // 是否启用POST
    char *m_string; // 存储请求体数据
    int bytes_to_send;
    int bytes_have_send;
    char *doc_root; // 网站根目录

    map<string, string> m_users; // 已存在的用户
    int m_TRIGMode;              // 触发模式
    int m_is_close_log;          // 是否关闭日志

    char sql_user[100];   // SQL语句
    char sql_passwd[100]; // 密码
    char sql_name[100];   // 用户名

};

#endif