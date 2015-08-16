#pragma once

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <iostream>
#include <queue>

#include <mqueue.h>


template <typename T>
class MessageQueue
{
public:

    enum class OpenFlags {READ_ONLY, WRITE_ONLY, READ_WRITE, INVALID};

    /**
     * @brief Constructor
     *
     * @param[in] ioService The asio io_serice instance.
     * @param[in] queueName The name of the message queue.
     * @param[in] flags The flags used to open the queue.
     * @param[in] maximum_message_count The maximum count of messages in the queue. This parameter is only used, when
     *            the queue is openend with the write flag active.
     * @param[in] message_size The maximum size of one message in the queue in byte. This parameter is only used, when
     *            the queue is openend with the write flag active.
     * @throws  std::runtime_error if opening the queue failed.
     */
    MessageQueue(boost::asio::io_service& ioService, const std::string& queueName, OpenFlags flags,
                 long maximum_message_count = 0, size_t message_size = 0)
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

    /**
     * @brief Destructor
     *
     * Closes the queue and waits for it to finish.
     */
    ~MessageQueue()
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        close();
        try
        {
            if (!cond.timed_wait(lock, boost::posix_time::milliseconds(500)))
            {
                std::cerr << "Failed to close the queue asynchronously." << std::endl;
                do_close();
            }
        }
        catch(...)
        {
            std::cerr << "Exception while waiting to close the queue." << std::endl;
            do_close();
        }
    }

    void handleWrite(const boost::system::error_code &/*ec*/, std::size_t /*bytes_transferred*/)
    {
        //now the mq is writable
        if (send_queue.empty())
        {
            return;
        }

        boost::unique_lock<boost::mutex> lock(mutex);
        QueueData write_data = send_queue.top();
        send_queue.pop();
        lock.unlock();

        int sendRet = mq_send(mqid, reinterpret_cast<const char*>(write_data.data.data()),
                              write_data.data.size(), write_data.priority);
        int error = errno;
        if (sendRet)
        {
            if (errno != EAGAIN)
            {
                //This may happen often...
                std::cerr << "Sending to mq failed: " << strerror(error) << std::endl;
            }

            if (errno == EMSGSIZE)
            {
                //Message is too big, discard it
                //TODO: Notify sender
            }
            else
            {
                lock.lock();
                //TODO: Maybe add a retry counter to write_data
                send_queue.push(write_data);
                lock.unlock();
            }
        }

        if (send_queue.empty())
        {
            return;
        }

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

        constexpr int MAXBUFLEN = 1024;
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

        streamDescriptor.async_read_some(
                    boost::asio::null_buffers(),
                    boost::bind(&MessageQueue::handleRead,
                                this,
                                boost::asio::placeholders::error));
    }

    mqd_t getId() const
    {
        return mqid;
    }

    void write(std::vector<uint8_t>&& data, int priority)
    {
        {
            boost::unique_lock<boost::mutex> lock(mutex);
            send_queue.emplace(priority, std::move(data));
        }

        streamDescriptor.async_write_some(
                    boost::asio::null_buffers(),
                    boost::bind(&MessageQueue::handleWrite,
                                this,
                                boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred));
    }

    void close()
    {
        if (ioService.stopped())
        {
            do_close();
        }
        else
        {
            ioService.post(boost::bind(&MessageQueue::do_close, this));
        }
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

    struct QueueData
    {
        QueueData(unsigned int priority, std::vector<uint8_t>&& data)
            : data(data)
            , priority(priority)
        {}

        bool operator<(const QueueData& rhs) const
        {
            return priority < rhs.priority;
        }


        std::vector<uint8_t> data;
        unsigned int priority;

    };

    std::priority_queue<QueueData> send_queue;
};
