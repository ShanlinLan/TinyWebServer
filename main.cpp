#include "./config/config.h"
#include "webserver.h"

int main(int argc, char *argv[])
{
    // 解析命令行参数
    config config;
    config.parse_arg(argc, argv);

    // 初始化
    webserver server(config.DIR_PATH);
    server.init(config.LISTEN_PORT, 
                config.DB_USER, config.DB_PASSWD, config.DB_NAME, 
                config.IS_CLOSE_LOG,
                config.LOG_WRITE_MODE, 
                config.OPT_LINGER, 
                config.TRIG_MODE, 
                config.NUM_SQL, config.NUM_THREAD, 
                config.ACTOR_MODE
                );

    // 日志
    server.log_write();

    // 数据库
    server.sql_pool();

    // 线程池
    server.thread_pool();

    // 触发模式
    server.trig_mode();

    // 监听
    server.event_listen();

    //运行
    server.event_loop();

    return 0;
}