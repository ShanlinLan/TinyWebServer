// 2022/4/28
// config类的实现

#include "config.h"

// 初始化
config::config()
{
    LISTEN_PORT = 7629; // 监听端口号, 默认7629

    IS_CLOSE_LOG = 0;   // 是否关闭日志，默认不关闭
    LOG_WRITE_MODE = 0; // 日志写入方式，默认同步方式

    TRIG_MODE = 0;        // 组合触发模式，默认listenfd LT + connfd LT
    LISTEN_TRIG_MODE = 0; // listenfd触发模式，默认LT
    CONN_TRIG_MODE = 0;   // connfd触发模式，默认LT

    OPT_LINGER = 0; // 优雅关闭链接，默认不使用

    NUM_SQL = 3;    // 数据库连接池数量，默认8
    NUM_THREAD = 2; // 线程池内的线程数量，默认8

    ACTOR_MODE = 0; // 并发模型，默认proactor

    DB_USER = "root";
    DB_PASSWD = "";
    DB_NAME = "tinywebserver";

    DIR_PATH = "/root"; // 网站相对目录，默认为当前目录下的/root
}

void config::parse_arg(int argc, char *argv[])
{
    int opt;
    const char *str = "p:l:m:o:s:t:c:a:";   // 参数格式
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
        case 'p':
        {
            LISTEN_PORT = atoi(optarg);
            break;
        }
        case 'l':
        {
            LOG_WRITE_MODE = atoi(optarg);
            break;
        }
        case 'm':
        {
            TRIG_MODE = atoi(optarg);
            break;
        }
        case 'o':
        {
            OPT_LINGER = atoi(optarg);
            break;
        }
        case 's':
        {
            NUM_SQL = atoi(optarg);
            break;
        }
        case 't':
        {
            NUM_THREAD = atoi(optarg);
            break;
        }
        case 'c':
        {
            IS_CLOSE_LOG = atoi(optarg);
            break;
        }
        case 'a':
        {
            ACTOR_MODE = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }
}