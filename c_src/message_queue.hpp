#pragma once

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <iostream>

#include <mqueue.h>


constexpr int MAXBUFLEN = 1024;

struct message
{
    int i;
};

template <typename T>
class MessageQueue
{
public:

    enum class OpenFlags {READ_ONLY, WRITE_ONLY, READ_WRITE, INVALID};

    MessageQueue(boost::asio::io_service& ioService, const std::string& queueName, OpenFlags flags)
        : ioService(ioService),
          streamDescriptor(ioService),
          timer(ioService),
          mqid(-1)
    {
        if (flags == OpenFlags::READ_ONLY)
        {
            mqid = mq_open(queueName.c_str(), O_RDONLY | O_NONBLOCK);
        }
        else
        {
            int open_flags = O_CREAT | O_NONBLOCK;
            struct mq_attr mattr;
            mattr.mq_maxmsg = 10;
            mattr.mq_msgsize = sizeof(struct message);

            if (flags == OpenFlags::WRITE_ONLY)
            {
                open_flags |= O_WRONLY;
            }
            else if (flags == OpenFlags::READ_WRITE)
            {
                open_flags |= O_RDWR;
            }

            mqid = mq_open(queueName.c_str(), open_flags, S_IREAD | S_IWRITE, &mattr);
        }

        std::cout << "message queue mqid = " << mqid << std::endl;

        if (mqid == -1)
        {
            std::cerr << "Failed to open queue with error: " << strerror(errno) << std::endl;
            throw std::runtime_error("Failed to open queue");
        }

        streamDescriptor.assign(mqid);
#if 0
        streamDescriptor.async_write_some(
                    boost::asio::null_buffers(),
                    boost::bind(&MessageQueue::handleWrite,
                                this,
                                boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred));
#endif
        streamDescriptor.async_read_some(
                    boost::asio::null_buffers(),
                    boost::bind(&MessageQueue::handleRead,
                                this,
                                boost::asio::placeholders::error));
    }

    ~MessageQueue()
    {
        //TODO handle it in the io service, otherwise race condition
        do_close();
    }

    void handleWrite(const boost::system::error_code &/*ec*/, std::size_t /*bytes_transferred*/)
    {
        //now the mq is writable
        if (write_data.empty())
        {
            return;
        }
        std::cout << "handleWrite start " << write_data.size() << " bytes" << std::endl;
        int sendRet = mq_send(mqid, reinterpret_cast<const char*>(write_data.data()), write_data.size(), write_priority);
        write_data.clear();
        std::cout << "sendRet = " << sendRet << std::endl;
//        timer.expires_from_now(boost::posix_time::microseconds(100));
//        timer.async_wait(boost::bind(&MessageQueue::handleTimer, this,
//                                     boost::asio::placeholders::error));
    }
#if 0
    void handleTimer(const boost::system::error_code& ec)
    {
        message msg;
        int sendRet = mq_send(mqid, (const char*)&msg, sizeof(message), 0);
        std::cout << "sendRet = " << sendRet << std::endl;

        streamDescriptor.async_write_some(
                    boost::asio::null_buffers(),
                    boost::bind(&MessageQueue::handleWrite,
                                this,
                                boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred));
    }
#endif
    void handleRead(const boost::system::error_code &ec)
    {
        if (ec)
        {
            static_cast<T*>(this)->on_mq_read_error(ec.value());
            return;
        }

        char buf[MAXBUFLEN];
        unsigned int prio = 0;

        int res = mq_receive(mqid, buf, MAXBUFLEN, &prio);
        std::cout << "RX " << res << " bytes from " << mqid << std::endl;
        if (res < 0)
        {
            std::cout << "Errno is " << strerror(errno) << std::endl;
        }
        if (res > 0)
        {
            std::vector<uint8_t> data(buf, buf + res);
            static_cast<T*>(this)->on_mq_data(data, prio);
        }
        else
        {
            static_cast<T*>(this)->on_mq_read_error(errno);
        }
    }

    mqd_t getId() const
    {
        return mqid;
    }

    void write(const std::vector<uint8_t> data, int priority)
    {
        std::cout << "write start " << data.size() << " bytes" << std::endl;
        //TODO SYNC!!
        write_data = data;
        write_priority = priority;
        //ioService.post(boost::bind(&MessageQueue::do_write, this, data, priority));
        streamDescriptor.async_write_some(
                    boost::asio::null_buffers(),
                    boost::bind(&MessageQueue::handleWrite,
                                this,
                                boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred));
    }

    void close()
    {
        ioService.post(boost::bind(&MessageQueue::do_close, this));
    }

private:

    void do_write(const boost::system::error_code &ec, const std::vector<uint8_t> data, int priority)
    {
        std::cout << "Do write " << data.size() << " bytes with prio " << priority << std::endl;
    }

    void do_close()
    {
        if (mqid >= 0)
        {
            mq_close(mqid);
            mqid = -1;
        }
    }


    boost::asio::io_service& ioService;
    boost::asio::posix::stream_descriptor streamDescriptor;
    boost::asio::deadline_timer timer;
    mqd_t mqid;

    std::vector<uint8_t> write_data;
    int write_priority;
};
