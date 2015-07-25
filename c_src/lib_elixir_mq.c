#include "erl_nif.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <errno.h>

#define MAXBUFLEN 1024

#define CREATE_QUEUE_IF_NOT_EXIST

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

static ERL_NIF_TERM _open(ErlNifEnv* env, int arc, const ERL_NIF_TERM argv[])
{
  char path[MAXBUFLEN];
  mqd_t queue;

#ifdef  CREATE_QUEUE_IF_NOT_EXIST
  struct mq_attr attr;
  attr.mq_flags = O_NONBLOCK;
  attr.mq_maxmsg = 10;
  attr.mq_msgsize = 256;
  attr.mq_curmsgs = 0;
#endif
  
  enif_get_string(env, argv[0], path, MAXBUFLEN, ERL_NIF_LATIN1);

#ifdef CREATE_QUEUE_IF_NOT_EXIST  
  queue = mq_open(path, O_RDWR | O_NONBLOCK | O_CREAT, S_IRWXU, &attr);
#else
  queue = mq_open(path, O_RDWR | O_NONBLOCK);
#endif
  
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

static ERL_NIF_TERM _read(ErlNifEnv* env, int arc, const ERL_NIF_TERM argv[])
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

static ERL_NIF_TERM _write(ErlNifEnv* env, int arc, const ERL_NIF_TERM argv[])
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


static ERL_NIF_TERM _close(ErlNifEnv* env, int arc, const ERL_NIF_TERM argv[])
{
  mqd_t queue = -1;
  
  enif_get_int(env, argv[0], &queue);
  if (mq_close(queue) == 0)
  {
    return enif_make_atom(env, "ok");
  }
  return report_errno_error(env, errno);
}

static ErlNifFunc nif_funcs[] =
{
  {"_open", 1, _open},
  {"_read", 1, _read},
  {"_write", 3, _write},
  {"_close", 1, _close}
};

ERL_NIF_INIT(Elixir.MessageQueue.Nif, nif_funcs, NULL, NULL, NULL, NULL)
