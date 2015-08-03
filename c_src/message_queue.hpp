#pragma once

#include <boost/bind.hpp>
#include <boost/asio.hpp>

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

  enum class OpenFlags {READ_ONLY, WRITE_ONLY, READ_WRITE};
  
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
    if (mqid >= 0)
    {
      mq_close(mqid);
    }
  }
  
  void handleWrite(const boost::system::error_code &ec, std::size_t bytes_transferred)
  {
    timer.expires_from_now(boost::posix_time::microseconds(100));
    timer.async_wait(boost::bind(&MessageQueue::handleTimer, this,
			       boost::asio::placeholders::error));
  }
  
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

  void handleRead(const boost::system::error_code &ec)
  {
    std::cout << "Data available on mq: " << ec.message() << std::endl;

    if (ec)
    {
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
      std::vector<uint8_t> data;
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
  
private:
  boost::asio::io_service& ioService;
  boost::asio::posix::stream_descriptor streamDescriptor;
  boost::asio::deadline_timer timer;
  mqd_t mqid;
};
