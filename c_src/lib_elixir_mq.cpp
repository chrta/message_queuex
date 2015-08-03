#include "nifpp.h"

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <iostream>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <errno.h>


static boost::asio::io_service* io = nullptr;
constexpr int MAXBUFLEN = 1024;

struct message
{
  int i;
};
  
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
#if 0
    if (res > 0)
    {
      enif_alloc_binary(res, &r);
      memcpy(r.data, buf, res);
      
      ERL_NIF_TERM priority = enif_make_int(env, prio);
      ERL_NIF_TERM status = enif_make_atom(env, "ok");
      ERL_NIF_TERM data =  enif_make_binary(env, &r);
      return enif_make_tuple3(env, status, priority, data);
    }
    else if (errno == EAGAIN)
    {
      enif_alloc_binary(0, &r);
      ERL_NIF_TERM priority = enif_make_int(env, 0);
      ERL_NIF_TERM status = enif_make_atom(env, "ok");
      ERL_NIF_TERM data =  enif_make_binary(env, &r);
      return enif_make_tuple3(env, status, priority, data);
    }
    
    return report_errno_error(env, errno);
#endif
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

static MessageQueue* queue = nullptr;

static ERL_NIF_TERM report_string_error(ErlNifEnv* env, const char* message)
{
  ERL_NIF_TERM text = enif_make_string(env, message, ERL_NIF_LATIN1);
  ERL_NIF_TERM status = enif_make_atom(env, "error");
  
  return enif_make_tuple2(env, status, text);
}

static ERL_NIF_TERM report_errno_error(ErlNifEnv* env, int error_number)
{
  return report_string_error(env, strerror(error_number));
}

extern "C" ERL_NIF_TERM _open(ErlNifEnv* env, int arc, const ERL_NIF_TERM argv[])
{
  char path[MAXBUFLEN];
  char atom_buf[MAXBUFLEN];
  int open_flags;
  int read_flag = 0;
  int write_flag = 0;
  
  ERL_NIF_TERM opts;
  ERL_NIF_TERM val;

  if (!enif_get_string(env, argv[0], path, MAXBUFLEN, ERL_NIF_LATIN1))
  {
    return enif_make_badarg(env);
  }
  
  opts = argv[1];
  if (!enif_is_list(env, opts))
  {
    return enif_make_badarg(env);
  }
    
  while(enif_get_list_cell(env, opts, &val, &opts))
  {
    if (!enif_get_atom(env, val, atom_buf, sizeof(atom_buf), ERL_NIF_LATIN1))
    {
      return enif_make_badarg(env);
    }
    if (strcmp("read", atom_buf) == 0)
    {
      read_flag = 1;
    }
    else if (strcmp("write", atom_buf) == 0)
    {
      write_flag = 1;
    }
    else
    {
      return enif_make_badarg(env);
    }
  }

  MessageQueue::OpenFlags flags;

  if (read_flag && write_flag)
  {
    flags = MessageQueue::OpenFlags::READ_WRITE;
  }
  else if (read_flag)
  {
    flags = MessageQueue::OpenFlags::READ_ONLY;
  }
  else if (write_flag)
  {
    flags = MessageQueue::OpenFlags::WRITE_ONLY;
  }

  delete queue;
  queue = nullptr;
  
  try
  {
    queue = new MessageQueue{*io, path, flags};
    //TODO delete ptr

    ERL_NIF_TERM value = enif_make_int(env, queue->getId());
    ERL_NIF_TERM status = enif_make_atom(env, "ok");

    std::cout << "Open ok: " << queue->getId() << std::endl;
    return enif_make_tuple2(env, status, value);
  }
  catch(...)
  {
    return report_errno_error(env, errno);
  }  
}

extern "C" ERL_NIF_TERM _read(ErlNifEnv* env, int arc, const ERL_NIF_TERM argv[])
{
  mqd_t queueId = -1;
  char buf[MAXBUFLEN];
  ErlNifBinary r;
  ssize_t res;
  unsigned int prio = 0;

  enif_get_int(env, argv[0], &queueId);
  res = mq_receive(queueId, buf, MAXBUFLEN, &prio);

  if (res > 0)
  {
    enif_alloc_binary(res, &r);
    memcpy(r.data, buf, res);

    ERL_NIF_TERM priority = enif_make_int(env, prio);
    ERL_NIF_TERM status = enif_make_atom(env, "ok");
    ERL_NIF_TERM data =  enif_make_binary(env, &r);
    return enif_make_tuple3(env, status, priority, data);
  }
  else if (errno == EAGAIN)
  {
    enif_alloc_binary(0, &r);
    ERL_NIF_TERM priority = enif_make_int(env, 0);
    ERL_NIF_TERM status = enif_make_atom(env, "ok");
    ERL_NIF_TERM data =  enif_make_binary(env, &r);
    return enif_make_tuple3(env, status, priority, data);
  }

  return report_errno_error(env, errno);
}

extern "C" ERL_NIF_TERM _write(ErlNifEnv* env, int arc, const ERL_NIF_TERM argv[])
{
  ErlNifBinary data;
  int result;
  mqd_t queue = -1;
  unsigned int prio = 0;

  enif_get_int(env, argv[0], &queue);
  enif_get_uint(env, argv[1], &prio);
  if (!enif_inspect_binary(env, argv[2], &data))
  {
    return report_string_error(env, "Error converting given data into binary");
  }

  result = mq_send(queue, (char*) data.data, data.size, prio);

  if (result == 0)
  {
    return enif_make_atom(env, "ok");
  }

  return report_errno_error(env, errno);
}


extern "C" ERL_NIF_TERM _close(ErlNifEnv* env, int arc, const ERL_NIF_TERM argv[])
{
  mqd_t queueId = -1;
  
  enif_get_int(env, argv[0], &queueId);
  try
  {
    auto temp = queue;
    queue = nullptr;
    delete temp;
  }
  catch(...)
  {
    return report_errno_error(env, errno);
  }
  return enif_make_atom(env, "ok");
#if 0
  if (mq_close(queueId) == 0)
  {
    return enif_make_atom(env, "ok");
  }
  return report_errno_error(env, errno);
#endif
}


static void* thread_func(void* arg)
{
  try
  {
    boost::asio::io_service io_service;
    io = &io_service;
    //do not exit run() when running out of work
    boost::asio::io_service::work work(io_service);
    //Reader reader(ioService);
    io_service.run();
    io = nullptr;
    std::cout << "Asio exit ok" << std::endl;
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }
  io = nullptr;
  
  return nullptr;
}


static ErlNifThreadOpts *thread_opts = nullptr;
static ErlNifTid tid;

extern "C" int load(ErlNifEnv* /*env*/, void** /*priv*/, ERL_NIF_TERM /*load_info*/)
{
  thread_opts = enif_thread_opts_create("thread_opts");

  if (enif_thread_create("", &tid, thread_func, nullptr, thread_opts) != 0)
  {
    return -1;
  }

  return 0;
}

extern "C" void unload(ErlNifEnv* /*env*/, void* /*priv*/)
{
  void* resp;

  if (io)
  {
    io->stop();
  }

  enif_thread_join(tid, &resp);

  enif_thread_opts_destroy(thread_opts);
}


ErlNifFunc nif_funcs[] =
{
  {"_open",  2, _open,  0},
  {"_read",  1, _read,  0},
  {"_write", 3, _write, 0},
  {"_close", 1, _close, 0}
};

extern "C"
{
ERL_NIF_INIT(Elixir.MessageQueue.Nif, nif_funcs, &load, NULL, NULL, &unload)
}
