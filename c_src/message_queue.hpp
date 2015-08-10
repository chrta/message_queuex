#pragma once

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
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

    MessageQueue(boost::asio::io_service& ioService, const std::string& queueName, OpenFlags flags, long maximum_message_count = 0, size_t message_size = 0)
        : ioService(ioService),
          streamDescriptor(ioService),
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
            mattr.mq_maxmsg = maximum_message_count;
            mattr.mq_msgsize = message_size;

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

        if (mqid == -1)
        {
            std::cerr << "Failed to open queue with error: " << strerror(errno) << std::endl;
            throw std::runtime_error("Failed to open queue");
        }

        streamDescriptor.assign(mqid);

        streamDescriptor.async_read_some(
                    boost::asio::null_buffers(),
                    boost::bind(&MessageQueue::handleRead,
                                this,
                                boost::asio::placeholders::error));
    }

    ~MessageQueue()
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        close();
        cond.wait(lock);
    }

    void handleWrite(const boost::system::error_code &/*ec*/, std::size_t /*bytes_transferred*/)
    {
        //now the mq is writable
        if (write_data.empty())
        {
            return;
        }

        int sendRet = mq_send(mqid, reinterpret_cast<const char*>(write_data.data()), write_data.size(), write_priority);
        if (sendRet == 0)
        {
            write_data.clear();
            return;
        }

        std::cerr << "Sending to mq failed: " << strerror(errno) << std::endl;

        //Sending failed, try again
        streamDescriptor.async_write_some(
                    boost::asio::null_buffers(),
                    boost::bind(&MessageQueue::handleWrite,
                                this,
                                boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred));

    }

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
        //TODO SYNC!!
        write_data = data;
        write_priority = priority;

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

    void do_close()
    {
        streamDescriptor.cancel();
        if (mqid >= 0)
        {
            mq_close(mqid);
            mqid = -1;
        }
        cond.notify_all();
    }


    boost::asio::io_service& ioService;
    boost::asio::posix::stream_descriptor streamDescriptor;
    boost::mutex mutex;
    boost::condition_variable cond;
    mqd_t mqid;

    std::vector<uint8_t> write_data;
    int write_priority;
};
