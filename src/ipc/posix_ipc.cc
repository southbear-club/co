/**
 * @file posix_ipc.cc
 * @author yuwangliang (wotsen@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2020-10-03
 * 
 * @copyright Copyright (c) 2020 yuwangliang
 * 
 */

#include <exception>
#include <stdexcept>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "co/fs.h"
#include "co/posix_ipc.h"

namespace ipc {

#define IPC_INVALID_FD -1

int trans_mode(const char mode) {
    if (mode == 'r') {
        return O_RDONLY;
    } else if (mode == 'a') {
        return O_WRONLY | O_CREAT | O_APPEND;
    } else if (mode == 'w') {
        return O_WRONLY | O_CREAT | O_TRUNC;
    } else if (mode == 'm') {
        return O_WRONLY | O_CREAT;
    } else if (mode == 'd') {
        return O_RDWR | O_CREAT | O_APPEND;
    } else {
        return -1;
    }
}

bool set_fd_block(const int fd, const bool en) {
	int flag = fcntl(fd, F_GETFL, 0);
	if (flag < 0) { return false; }

	if (en) {
		flag &= ~O_NONBLOCK;
	} else {
		flag |= O_NONBLOCK;
	}

	return fcntl(fd, F_SETFL, flag) == 0;
}

/**
 * @brief 检测可写
 * 
 * @param fd 
 * @param wait 单位ms
 * @return int -1为异常，0为超时，1为可写
 */
int wait_write(const int fd, const time_t wait) {
	fd_set wfds;
	struct timeval tv;
	FD_ZERO(&wfds);
	FD_SET(fd, &wfds);

	if (wait >= 0) {
		tv.tv_sec = wait / 1000;
		tv.tv_usec = wait % 1000 * 1000;
	}

	int ret = select(fd + 1, nullptr, &wfds, nullptr, wait < 0 ? nullptr : &tv);

	// 异常
	if (ret < 0) {
		return -1;
	}

	// 超时
	if (ret == 0) {
		return 0;
	}

	// 有效
	if (FD_ISSET(fd, &wfds)) {
		return 1;
	}

	// 无效
	return 0;
}

/**
 * @brief 检测可读
 * 
 * @param fd 
 * @param wait 单位ms
 * @return int -1为异常，0为超时，1为可写
 */
int wait_read(const int fd, const time_t wait) {
	fd_set rfds;
	struct timeval tv;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	if (wait >= 0) {
		tv.tv_sec = wait / 1000;
		tv.tv_usec = wait % 1000 * 1000;
	}

	int ret = select(fd + 1, &rfds, nullptr, nullptr, wait < 0 ? nullptr : &tv);

	if (ret < 0) {
		return -1;
	}

	if (ret == 0) {
		return 0;
	}

	if (FD_ISSET(fd, &rfds)) {
		return 1;
	}

	return 0;
}

int write(const int fd, const void* data, const size_t len, const time_t wait) {
	int ret = wait_write(fd, wait);

	if (ret == 1) {
		return ::write(fd, data, len);
	} else if (ret < 0) {
		return -1;
	}

	return 0;
}

int read(const int fd, void* data, const size_t len, const time_t wait) {
	int ret = wait_read(fd, wait);

	if (ret == 1) {
		return ::read(fd, data, len);
	} else if (ret < 0) {
		return -1;
	}

	return 0;
}

Pipe::Pipe() {
	pipe_[0] = IPC_INVALID_FD;
	pipe_[1] = IPC_INVALID_FD;
	// block_ = true;
	if (pipe(pipe_) < 0) {
		throw std::runtime_error("open pipe error");
	}
}

Pipe::~Pipe(void) {
	close();
}

void Pipe::close_r(void)
{
	if (pipe_[0] >= 0) {
		::close(pipe_[0]);
	}

	pipe_[0] = IPC_INVALID_FD;
}

void Pipe::close_w(void)
{
	if (pipe_[1] >= 0) {
		::close(pipe_[1]);
	}

	pipe_[1] = IPC_INVALID_FD;
}

void Pipe::close(void) {
	close_w();
	close_r();
}

bool Pipe::set_size(const size_t &size) {
	if (pipe_[0] < 0 && pipe_[1] < 0) { return false; }
	// TODO:不支持，使用默认大小
	// fcntl(pipefd[1], F_SETPIPE_SZ, BUFSIZE);
	return true;
}

bool Pipe::set_block(const bool en) {
	if (pipe_[0] >= 0) {
		if (set_fd_block(pipe_[0], en)) {
			return true;
		}
		return false;
	}

	if (pipe_[1] >= 0) {
		if (set_fd_block(pipe_[1], en)) {
			return true;
		}
		return false;
	}

	return false;
}

pipe_t Pipe::read_fd(void) {
	return pipe_[0];
}

pipe_t Pipe::write_fd(void) {
	return pipe_[1];
}

int Pipe::write(const void* data, const size_t len, const time_t wait) {
	if (pipe_[1] < 0) { return -1; }

	return ipc::write(pipe_[1], data, len, wait);
}

int Pipe::read(void* data, const size_t len, const time_t wait) {
	if (pipe_[0] < 0) { return -1; }

	return ipc::read(pipe_[0], data, len, wait);
}

Fifo::Fifo(const char* path, const char mode) : path_(path), fd_(IPC_INVALID_FD) {
	if (!path || trans_mode(mode) < 0) {
		throw std::invalid_argument("path or mode error");
	}

	if (mkfifo(path, trans_mode(mode)) < 0) {
		throw std::runtime_error("mkfifo error");
	}
}

Fifo::~Fifo() {
	close();
}

bool Fifo::open(const char mode) {
	// 当前描述符已打开
	if (fd_ >= 0) { return false; }

	if (trans_mode(mode) < 0) {
		return false;
	}

	fd_ = ::open(path_.c_str(), trans_mode(mode));
	if (fd_ < 0) {
		fd_ = IPC_INVALID_FD;
		return false;
	}

	return true;
}

void Fifo::close(void) {
	if (fd_ >= 0) {
		::close(fd_);
		fd_ = IPC_INVALID_FD;
	}
}

bool Fifo::set_block(const bool en) {
	if (set_fd_block(fd_, en)) {
		return true;
	}

	return false;
}

int Fifo::write(const void* data, const size_t len, const time_t wait) {
	if (fd_ < 0) { return -1; }

	return ipc::write(fd_, data, len, wait);
}

int Fifo::read(void* data, const size_t len, const time_t wait) {
	if (fd_ < 0) { return -1; }

	return ipc::read(fd_, data, len, wait);
}

PosixMsgQueue::PosixMsgQueue(const char* path, const char mode, const size_t maxmsg, const bool srv)
		: fd_(IPC_INVALID_FD), path_(path), maxmsg_(maxmsg) {
	struct mq_attr attr;
    memset(&attr, 0, sizeof(attr));

	attr.mq_msgsize = maxmsg_;
	attr.mq_maxmsg = 20;

    fd_ = mq_open(path, trans_mode(mode), 0666, &attr);

	if (fd_ < 0) {
		throw std::runtime_error("open msg queue error");
	}
}

PosixMsgQueue::~PosixMsgQueue() {
	close();
}

void PosixMsgQueue::close(void) {
	if (fd_ >= 0) {
		mq_close(fd_);
		fd_ = IPC_INVALID_FD;
	}
}

void PosixMsgQueue::destroy(void) {
	close();
	mq_unlink(path_.c_str());
}

int PosixMsgQueue::write(const void* data, const size_t len, const time_t wait) {
	if (fd_ < 0) { return -1; }

	if (wait < 0) {
		if (mq_send(fd_, (const char *)data, len, _SC_MQ_PRIO_MAX) < 0)
		{
			return -1;
		}

		return len;
	} else {
		struct timespec abs_timeout = {
			wait / 1000, wait % 1000 * 1000
		};

		if (mq_timedsend(fd_, (const char *)data, len, _SC_MQ_PRIO_MAX, &abs_timeout) < 0) {
			return -1;
		}

		return len;
	}
}

int PosixMsgQueue::read(void* data, const size_t len, const time_t wait) {
	if (fd_ < 0) { return -1; }
	unsigned int prio = _SC_MQ_PRIO_MAX;

	if (len > maxmsg_) {
		return -1;
	}

	if (wait < 0) {
		// 实际传参必须和设置的消息最大长度一样大
		return mq_receive(fd_, (char*)data, maxmsg_, &prio);
	} else {
		struct timespec abs_timeout = {
			wait / 1000, wait % 1000 * 1000
		};

		return mq_timedreceive(fd_, (char*)data, maxmsg_, &prio, &abs_timeout);
	}
}

bool PosixMsgQueue::set_block(const bool en) {
	if (fd_ < 0) { return false; }
	struct mq_attr attr;

    memset(&attr, 0, sizeof(attr));

	if (mq_getattr(fd_, &attr) < 0) {
		return false;
	}

	attr.mq_flags = en ? 0 : O_NONBLOCK;

	return 0 == mq_setattr(fd_, &attr, nullptr);
}

bool PosixMsgQueue::notify(const struct sigevent *notification) {
	if (fd_ < 0) { return false; }
	return 0 == mq_notify(fd_, notification);
}

NameSem::NameSem(const char* name, const bool srv) : sem_(nullptr), name_(name) {
	sem_t *sem = sem_open(name, srv ? O_CREAT|O_EXCL : O_CREAT, 0666, 1);

	if (SEM_FAILED == sem) {
		throw std::runtime_error("open sem error");
	}

	sem_ = (psem_t)sem;
}

NameSem::~NameSem() {
	close();
}

void NameSem::close(void) {
	if (sem_) {
		sem_t *sem = (sem_t *)sem_;
		sem_close(sem);
		sem_ = nullptr;
	}
}

void NameSem::unlink(void) {
	close();
	sem_unlink(name_.c_str());
}

int NameSem::value(void) {
	if (!sem_) { return -1; }

	sem_t *sem = (sem_t *)sem_;
	int val = 0;
	int ret = sem_getvalue(sem, &val);

	if (ret < 0) {
		return -1;
	}

	return val;
}

bool NameSem::sem_p(const bool block) {
	if (!sem_) { return false; }

	sem_t *sem = (sem_t *)sem_;
	if (!block) {
		return 0 == sem_trywait(sem);
	} else {
		return 0 == sem_wait(sem);
	}
}

bool NameSem::sem_v(void) {
	if (!sem_) { return false; }

	sem_t *sem = (sem_t *)sem_;

	return 0 == sem_post(sem);
}

Sem::Sem(const bool shared) {
	memset(&sem_, 0, sizeof(sem_));

	if (sem_init(&sem_, shared ? 1 : 0, 1) < 0) {
		throw std::runtime_error("sem init error");
	}
}

Sem::~Sem() {
	close();
}

void Sem::close(void) {
	sem_destroy(&sem_);
	memset(&sem_, 0, sizeof(sem_));
}

bool Sem::sem_p(const bool block) {
	if (!block) {
		return 0 == sem_trywait(&sem_);
	} else {
		return 0 == sem_wait(&sem_);
	}
}

bool Sem::sem_v(void) {
	return 0 == sem_post(&sem_);
}

PosixShareMem::PosixShareMem(const char *name, const char mode) : addr_(nullptr), fd_(IPC_INVALID_FD), name_(name) {
	if (!name || trans_mode(mode) < 0) {
		throw std::invalid_argument("name or mode error");
	}

	if ((fd_ = shm_open(name, trans_mode(mode), 0666)) < 0) {
		throw std::runtime_error("shm_open error");
	}
}

PosixShareMem::~PosixShareMem() {
	unmap();
}

void PosixShareMem::destroy(void) {
	unmap();

	if (!name_.empty()) {
		shm_unlink(name_.c_str());
	}
}

void* PosixShareMem::map(const size_t len, const char mode, const bool shared, off_t offset) {
	if (addr_) { return nullptr; }

	len_ = len;

	int prot = 0;

	switch (mode)
	{
	case 'r':
		prot = PROT_READ;
		break;
	
	case 'w':
		prot = PROT_WRITE;
		break;
	
	case 'e':
		prot = PROT_EXEC;
		break;
	
	case 'd':
		prot = PROT_READ | PROT_WRITE;
		break;
	
	case 'n':
		prot = PROT_NONE;
		break;
	
	case 'm':
		prot = PROT_READ | PROT_WRITE | PROT_EXEC;
		break;
	
	default:
		throw std::runtime_error("mode not support");
		break;
	}

	if (mmap(nullptr, len_, prot, shared ? MAP_SHARED : MAP_FIXED, offset) < 0)  {
		throw std::runtime_error("mmp error");
	}
}

void PosixShareMem::unmap(void) {
	if (addr_) {
		munmap(addr_, len_);
	}
}

void* PosixShareMem::addr(void) {
	return addr_;
}

}