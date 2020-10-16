/**
 * @file posix_ipc.h
 * @author yuwangliang (wotsen@outlook.com)
 * @brief 基于posix标准的ipc(进程间通信)
 * @version 0.1
 * @date 2020-10-03
 * 
 * @copyright Copyright (c) 2020 yuwangliang
 * 
 */

#pragma once

#include <stddef.h>
#include <string>
#include <time.h>
#include <signal.h>
#include <semaphore.h>
#include "def.h"

namespace ipc {

typedef int pipe_t;
typedef int fifo_t;
typedef int msgq_t;
typedef sem_t* psem_t;

/**
 * @brief posix进程间通信基类
 * 
 */
class IPC
{
public:
	IPC() {}
	virtual ~IPC() {}

	/**
	 * @brief 消息读取
	 * 
	 * @param data 数据
	 * @param len 数据长度
	 * @param wait 超时时间，单位ms，默认0，-1时为阻塞
	 * @return int 成功读取的长度，小于0异常
	 */
	virtual int read(void* data, const size_t len, const time_t wait=0) = 0;

	/**
	 * @brief 消息写入
	 * 
	 * @param data 数据
	 * @param len 数据长度
	 * @param wait 超时时间，单位ms，默认0，-1时为阻塞
	 * @return int 成功写入的长度，小于0为异常
	 */
	virtual int write(const void* data, const size_t len, const time_t wait=0) = 0;

	/**
	 * @brief 关闭
	 * 
	 */
	virtual void close(void) = 0;
};

/**
 * @brief pipe管道
 * @details 半双工，只能用于父子进程，一端只能读，一端只能写
 */
class Pipe : public IPC
{
public:
	Pipe();
	~Pipe();

	// 读取
	virtual int read(void* data, const size_t len, const time_t wait=0) override;
	// 写入
	virtual int write(const void* data, const size_t len, const time_t wait=0) override;
	// 关闭全部
	virtual void close(void) override;

	// 关闭读端
	void close_r(void);
	// 关闭写端
	void close_w(void);

	// 设置管道容量
	bool set_size(const size_t &size);

	/**
	 * @brief 设置阻塞
	 * 
	 * @param en true-阻塞，false-非阻塞
	 * @return true 成功
	 * @return false 失败
	 */
	bool set_block(const bool en);

	// 获取读端描述符
	pipe_t read_fd(void);
	// 获取写端描述符
	pipe_t write_fd(void);
private:
	pipe_t pipe_[2];
};

/**
 * @brief 双向管道通信，可用于无亲缘关系的线程
 * 
 */
class Fifo : public IPC {
public:
	// mode:
	//   'r': read         open if exists
	//   'a': append       created if not exists
	//   'w': write        created if not exists, truncated if exists
	//   'm': modify       like 'w', but not truncated if exists
	//   'd': default      可读可写
	Fifo(const char* path, const char mode);
	~Fifo();

	// 打开管道文件
	bool open(const char mode);
	// 设置阻塞，非阻塞
	bool set_block(const bool en);

	// 读取
	virtual int read(void* data, const size_t len, const time_t wait=0) override;
	// 写入
	virtual int write(const void* data, const size_t len, const time_t wait=0) override;
	// 关闭全部
	virtual void close(void) override;

private:
	std::string path_;
	fifo_t fd_;
};

/**
 * @brief 消息队列
 * @note 需要在链接时使用-lrt
 */
class PosixMsgQueue : public IPC {
public:
	PosixMsgQueue(const char* path, const char mode, const size_t maxmsg=8192, const bool srv=false);
	~PosixMsgQueue();

	// 设置阻塞
	bool set_block(const bool en);
	// 销毁
	void destroy(void);

	// 异步
    /*
     * Posix消息队列允许异步事件通知，以告知何时有一个消息放置到了某个空消息队列中。这种通知有两种方式可供选择：
     * 一、产生一个信号
     * 二、创建一个线程来执行指定的函数
     * */

    /*
     * notification非空，该进程被注册为接收该消息队列的通知
     * notification空，且当前队列已被注册，则已存在的注册被撤销
     * 对一个消息队列来说，任一时刻只有一个进程可以被注册
     * 在mq_receive调用中的阻塞比任何通知的注册都优先收到消息
     * 当通知被发送给注册进程时，注册即被撤销，该进程若要重新注册，则必须重新调用mq_notify()
     * struct sigevent {
     *      // SIGEV_NONE：事件发生时，什么也不做；SIGEV_SIGNAL：事件发生时，将sigev_signo指定的信号发送给指定的进程；
     *      // SIGEV_THREAD：事件发生时，内核会（在此进程内）以sigev_notify_attributes为线程属性创建一个线程，并让其执行
     *      int sigev_notify;
     *      int sigev_signo; // 信号编号，与常见的信号一致，在sigev_notify = SIGEV_SIGNAL时使用，指定信号类别
     *      union sigval sigev_value; // sigev_notify = SIGEV_SIGEV_THREAD时使用，作为sigev_notify_function的参数
     *      // union sigval {
     *      //      int sival_int;
     *      //      void *sival_ptr;
     *      // }
     *
     *      // 下面两种方式选择一种即可
     *      void (*sigev_notify_function)(union sigval); // 以sigev_value为其参数，在sigev_notify = SIGEV_THREAD时使用，其他情况下置NULL
     *      pthread_attr_t *sigev_notify_attributes; // 在sigev_notify = SIGEV_THREAD时使用，指定创建线程的属性，其他情况下置NULL
     * }
     * */
	bool notify(const struct sigevent *notification);

	// 读取
	virtual int read(void* data, const size_t len, const time_t wait=0) override;
	// 写入
	virtual int write(const void* data, const size_t len, const time_t wait=0) override;
	// 关闭全部
	virtual void close(void) override;

private:
	msgq_t fd_;
	std::string path_;
	size_t maxmsg_;
};

class ISem {
public:
	ISem() {}
	virtual ~ISem() {}

	virtual bool sem_p(const bool block=true) = 0;
	virtual bool sem_v(void) = 0;
	virtual void close(void) = 0;
};

/**
 * @brief 信号量
 * 
 */
class NameSem : public ISem {
public:
	NameSem(const char* name, const bool srv=false);
	~NameSem();

	virtual bool sem_p(const bool block=true) override;
	virtual bool sem_v(void) override;
	virtual void close(void) override;

	int value(void);
	void unlink(void);

private:
	psem_t sem_;
	std::string name_;
};

class Sem : public ISem {
public:
	// 如果使用共享，需要放在共享内存中
	Sem(const bool shared=false);
	~Sem();

	virtual bool sem_p(const bool block=true) override;
	virtual bool sem_v(void) override;
	virtual void close(void) override;

private:
	sem_t sem_;
};

/**
 * @brief 共享内存
 * @details 共享内存快的一个原因是建立映射后不在有内核数据拷贝
 * 
 */
// https://www.cnblogs.com/songhe364826110/p/11530732.html
class PosixShareMem {
public:
	// mode:
	//   'r': read         open if exists
	//   'a': append       created if not exists
	//   'w': write        created if not exists, truncated if exists
	//   'm': modify       like 'w', but not truncated if exists
	//   'd': default      可读可写
	PosixShareMem(const char* name, const char mode);
	~PosixShareMem();

	// 'r' 只读
	// 'w' 只写
	// 'e' 可执行
	// 'd' 可读可写
	// 'n' 不可访问
	// 'm' 可读可写可执行
	void* map(const size_t len, const char mode='d', const bool shared=false, off_t offset=0);
	void unmap(void);

	void destroy(void);

	void* addr(void);
	// TODO:sync

private:
	void* addr_;
	int fd_;
	size_t len_;
	std::string name_;
};

/**
 * @brief 信号
 * https://blog.csdn.net/flowing_wind/article/details/79967588
 * 
 */

/**
 * @brief 网络通信
 * 
 */
	
int trans_mode(const char mode);
bool set_fd_block(const int fd, const bool en);
int wait_write(const int fd, const time_t wait);
int wait_read(const int fd, const time_t wait);
int write(const int fd, const void* data, const size_t len, const time_t wait);
int read(const int fd, void* data, const size_t len, const time_t wait);

} // !namespace ipc