// 2022/4/28
// 配置文件

#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <stdlib.h>
#include <unistd.h>

// 配置文件类
class config
{
public:
    config();
    ~config(){};

    // 解析命令行选项参数
    void parse_arg(int argc, char *argv[]);

    int LISTEN_PORT; // 监听端口号

    int IS_CLOSE_LOG;   // 是否关闭日志
    int LOG_WRITE_MODE; // 日志写入方式

    int TRIG_MODE;        // 组合触发模式
    int LISTEN_TRIG_MODE; // listenfd触发模式
    int CONN_TRIG_MODE;   // connfd触发模式

    int OPT_LINGER; // 优雅关闭链接

    int NUM_SQL;    // 数据库连接池数量
    int NUM_THREAD; // 线程池内的线程数量

    int ACTOR_MODE; // 并发模型选择

    std::string DB_USER;   // 数据库用户
    std::string DB_PASSWD; // 数据库密码
    std::string DB_NAME;   // 数据库名字

    std::string DIR_PATH; // 网站相对目录
};

#endif