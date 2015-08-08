#include "nifpp.h"

#include "message_queue.hpp"

#include <iostream>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>



static boost::asio::io_service* io = nullptr;

static ErlNifPid* dst = nullptr;

class MessageQueueNif : public MessageQueue<MessageQueueNif>
{
public:
MessageQueueNif(boost::asio::io_service& ioService, const std::string& queueName, OpenFlags flags)
  : MessageQueue(ioService, queueName, flags)
{}

void on_mq_data(const std::vector<uint8_t>& data, int priority)
{
    std::cout << "on_mq_data" << data.size() << " bytes, prio " << priority << std::endl;
    if (!dst)
    {
        return;
    }
    ErlNifEnv* env = enif_alloc_env();

    nifpp::binary bin_data(data.size());
    std::copy(data.begin(), data.end(), bin_data.data);
    nifpp::TERM message = nifpp::make(env, std::make_tuple(nifpp::make(env, nifpp::str_atom("ok")), nifpp::make(env, priority), nifpp::make(env, bin_data)));
    enif_send(NULL, dst, env, message);
    enif_free_env(env);
}

void on_mq_read_error(int error_number)
{
    std::cout << "on_mq_read_error " << strerror(error_number) << std::endl;
    //do not report read errors to the user
}
};

static MessageQueueNif* queue = nullptr;

static ERL_NIF_TERM report_string_error(ErlNifEnv* env, const std::string& message)
{ 
  return nifpp::make(env, std::make_tuple(nifpp::str_atom("error"), message));
}

static ERL_NIF_TERM report_errno_error(ErlNifEnv* env, int error_number)
{
  return report_string_error(env, strerror(error_number));
}

extern "C" ERL_NIF_TERM _open(ErlNifEnv* env, int /*arc*/, const ERL_NIF_TERM argv[])
{
  std::string path;
  bool read_flag = false;
  bool write_flag = false;
  
  ERL_NIF_TERM opts;

  if (!nifpp::get(env, argv[0], path))
  {
    return enif_make_badarg(env);
  }
  
  opts = argv[1];

  std::vector<nifpp::str_atom> open_flags;
  if (!nifpp::get(env, opts, open_flags))
  {
    return enif_make_badarg(env);
  }

  for (auto atom : open_flags)
  {
      if (atom == "read")
      {
        read_flag = true;
      }
      else if (atom == "write")
      {
        write_flag = true;
      }
      else
      {
        return enif_make_badarg(env);
      }
  }

  MessageQueueNif::OpenFlags flags = MessageQueueNif::OpenFlags::INVALID;

  if (read_flag && write_flag)
  {
    flags = MessageQueueNif::OpenFlags::READ_WRITE;
  }
  else if (read_flag)
  {
    flags = MessageQueueNif::OpenFlags::READ_ONLY;
  }
  else if (write_flag)
  {
    flags = MessageQueueNif::OpenFlags::WRITE_ONLY;
  }

  delete queue;
  queue = nullptr;
  delete dst;
  dst = nullptr;
  
  try
  {
    queue = new MessageQueueNif{*io, path, flags};
    dst = new ErlNifPid;
    enif_self(env, dst);
    //TODO delete ptr

    std::cout << "Open ok: " << queue->getId() << std::endl;
    return nifpp::make(env, std::make_tuple(nifpp::str_atom("ok"), queue->getId()));
  }
  catch(...)
  {
    return report_errno_error(env, errno);
  }  
}

extern "C" ERL_NIF_TERM _read(ErlNifEnv* env, int /*arc*/, const ERL_NIF_TERM argv[])
{
#if 0
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
#endif
  return report_errno_error(env, /*errno*/ 9);
}

extern "C" ERL_NIF_TERM _write(ErlNifEnv* env, int /*arc*/, const ERL_NIF_TERM argv[])
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


extern "C" ERL_NIF_TERM _close(ErlNifEnv* env, int /*arc*/, const ERL_NIF_TERM argv[])
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


static void* thread_func(void* /*arg*/)
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
  thread_opts = enif_thread_opts_create(const_cast<char*>("thread_opts"));

  if (enif_thread_create(const_cast<char*>(""), &tid, thread_func, nullptr, thread_opts) != 0)
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
