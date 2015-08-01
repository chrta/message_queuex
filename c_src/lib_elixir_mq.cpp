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

const std::string MSG_QUEUE_NAME = "/test_msg_queue";

struct message
{
  int i;
};
  
class MessageQueue
{
public:
  MessageQueue(boost::asio::io_service& ioService)
    : ioService(ioService),
      streamDescriptor(ioService),
      timer(ioService)
  {
    struct mq_attr mattr;
    mattr.mq_maxmsg = 10;
    mattr.mq_msgsize = sizeof(struct message);
    mqid = mq_open(MSG_QUEUE_NAME.c_str(), O_CREAT | O_WRONLY |
		   O_NONBLOCK, S_IREAD | S_IWRITE, &mattr);
    std::cout << "message queue mqid = " << mqid << std::endl;
    streamDescriptor.assign(mqid);
    
    streamDescriptor.async_write_some(
				      boost::asio::null_buffers(),
				      boost::bind(&MessageQueue::handleWrite,
						this,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
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
  
private:
  boost::asio::io_service& ioService;
  boost::asio::posix::stream_descriptor streamDescriptor;
  boost::asio::deadline_timer timer;
  mqd_t mqid;
};


#define MAXBUFLEN 1024

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
  mqd_t queue;
  int open_flags;
  int read_flag = 0;
  int write_flag = 0;
  
  ERL_NIF_TERM opts;
  ERL_NIF_TERM val;

  struct mq_attr attr;
  attr.mq_flags = O_NONBLOCK;
  attr.mq_maxmsg = 10;
  attr.mq_msgsize = 256;
  attr.mq_curmsgs = 0;
  
  if (!enif_get_string(env, argv[0], path, MAXBUFLEN, ERL_NIF_LATIN1))
  {
    return enif_make_badarg(env);
  }
  
  opts = argv[1];
  if (!enif_is_list(env, opts))
  {
    return enif_make_badarg(env);
  }
    
  open_flags = O_NONBLOCK;

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

  if (read_flag && write_flag)
  {
    open_flags |= O_RDWR;
  }
  else if (read_flag)
  {
    open_flags |= O_RDONLY;
  }
  else if (write_flag)
  {
    open_flags |= O_WRONLY;
  }

  if (write_flag)
  {
    open_flags |= O_CREAT;
    queue = mq_open(path, open_flags, S_IRWXU, &attr);
  }
  else
  {
    queue = mq_open(path, open_flags);
  }
  
  if (queue == -1)
  {
    return report_errno_error(env, errno);
  }
  else
  {
    ERL_NIF_TERM value = enif_make_int(env, queue);
    ERL_NIF_TERM status = enif_make_atom(env, "ok");

    return enif_make_tuple2(env, status, value);
  }
}

extern "C" ERL_NIF_TERM _read(ErlNifEnv* env, int arc, const ERL_NIF_TERM argv[])
{
  mqd_t queue = -1;
  char buf[MAXBUFLEN];
  ErlNifBinary r;
  ssize_t res;
  unsigned int prio = 0;

  enif_get_int(env, argv[0], &queue);
  res = mq_receive(queue, buf, MAXBUFLEN, &prio);

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
  mqd_t queue = -1;
  
  enif_get_int(env, argv[0], &queue);
  if (mq_close(queue) == 0)
  {
    return enif_make_atom(env, "ok");
  }
  return report_errno_error(env, errno);
}

static boost::asio::io_service* io = nullptr;
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
