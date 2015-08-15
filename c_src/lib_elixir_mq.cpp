#include "nifpp.h"

#include "message_queue.hpp"

#include <iostream>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>



/** \todo Remove this global instance. */
static boost::asio::io_service* io = nullptr;

/**
 * @brief Implements the adaption from the generic MessageQueue to erlang nif.
 */
class MessageQueueNif : public MessageQueue<MessageQueueNif>
{
public:
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
     * @param[in] pid The pid of the process that should be notified in case of an event, like new data.
     */
    MessageQueueNif(boost::asio::io_service& ioService, const std::string& queueName, OpenFlags flags,
                    long maximum_message_count, size_t message_size, ErlNifPid pid)
        : MessageQueue(ioService, queueName, flags, maximum_message_count, message_size)
        , owner(pid)
    {}

    /**
     * @brief Handles new data read from the queue.
     *
     * This method is called from the base class if new data is available on the queue.
     *
     * The given data and priority is send to the associated process.
     *
     * @param[in] data The received data.
     * @param[in] priority The priority of the received data.
     */
    void on_mq_data(const std::vector<uint8_t>& data, int priority)
    {
        ErlNifEnv* env = enif_alloc_env();

        nifpp::binary bin_data(data.size());
        std::copy(data.begin(), data.end(), bin_data.data);

        nifpp::TERM message = nifpp::make(env, std::make_tuple(nifpp::make(env, nifpp::str_atom("mq")),
                                                               nifpp::make(env, getId()),
                                                               nifpp::make(env, priority),
                                                               nifpp::make(env, bin_data)));
        enif_send(NULL, &owner, env, message);
        enif_free_env(env);
    }

    /**
     * @brief Handles errors that occurred during reading from the queue.
     *
     * Currently an empty implementation.
     */
    void on_mq_read_error(int /*error_number*/)
    {
        //std::cout << "on_mq_read_error " << strerror(error_number) << std::endl;
        //do not report read errors to the user
    }

private:

    /** This process is notified in case of an event on the queue. */
    ErlNifPid owner;
};

/** Contains all message queue instances. */
static std::map<int, std::unique_ptr<MessageQueueNif>> queues;

/**
 * @brief Returns an error tuple containing the given message.
 *
 * @param[in] env The environment.
 * @param[in] message The error message.
 * @return The tuple in the form {:error, message}
 */
static ERL_NIF_TERM report_string_error(ErlNifEnv* env, const std::string& message)
{ 
    return nifpp::make(env, std::make_tuple(nifpp::str_atom("error"), message));
}

/**
 * @brief Returns an error tuple containing the errno message.
 *
 * @param[in] env The environment.
 * @param[in] message The errno value that is converted into the error message.
 * @return The tuple in the form {:error, message}
 */
static ERL_NIF_TERM report_errno_error(ErlNifEnv* env, int error_number)
{
    return report_string_error(env, strerror(error_number));
}

/**
 * @brief Opens a message queue.
 *
 * @param[in] env The environment.
 * @param[in] arc The count of arguments.
 * @param[in] argv The function arguments.
 * @retval {:ok, fd} if the queue could be opened
 * @retval {:error, reason} if opening the queue failed
 * @retval ArgumentError if the supplied arguments in argv are invalid
 */
extern "C" ERL_NIF_TERM _open(ErlNifEnv* env, int /*arc*/, const ERL_NIF_TERM argv[])
{
    std::string path;
    bool read_flag = false;
    bool write_flag = false;
    long maximum_message_count = 0;
    size_t message_size = 0;

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

    if (write_flag)
    {
        std::tuple<int, int> sizes;
        if (!nifpp::get(env, argv[2], sizes))
        {
            return enif_make_badarg(env);
        }

        if (std::get<0>(sizes) >= 0)
        {
            maximum_message_count = std::get<0>(sizes);
        }

        if (std::get<1>(sizes) >= 0)
        {
            message_size = std::get<0>(sizes);
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

    ErlNifPid owner;
    enif_self(env, &owner);
    try
    {
        auto queue = std::make_unique<MessageQueueNif>(*io, path, flags, maximum_message_count, message_size, owner);
        auto queueId = queue->getId();
        queues[queueId] = std::move(queue);

        return nifpp::make(env, std::make_tuple(nifpp::str_atom("ok"), queueId));
    }
    catch(...)
    {
        return report_errno_error(env, errno);
    }
}

/**
 * @brief Do not use this function
 *
 * This maybe implemented in the future as an synchronous read or completely removed.
 */
extern "C" ERL_NIF_TERM _read(ErlNifEnv* env, int /*arc*/, const ERL_NIF_TERM[])
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

/**
 * @brief Writes data to the given message queue.
 *
 * @param[in] env The environment.
 * @param[in] arc The count of arguments.
 * @param[in] argv The function arguments.
 * @retval :ok if the data could be queued.
 * @retval ArgumentError if the supplied arguments in argv are invalid
 */
extern "C" ERL_NIF_TERM _write(ErlNifEnv* env, int /*arc*/, const ERL_NIF_TERM argv[])
{
    mqd_t queueId = -1;
    if (!nifpp::get(env, argv[0], queueId))
    {
        return enif_make_badarg(env);
    }

    if (queues.find(queueId) == queues.end())
    {
        return report_errno_error(env, EBADF);
    }

    int priority = 0;
    if (!nifpp::get(env, argv[1], priority))
    {
        return enif_make_badarg(env);
    }

    ErlNifBinary bin_data;
    if (!nifpp::get(env, argv[2], bin_data))
    {
        return enif_make_badarg(env);
    }

    std::vector<uint8_t> data(bin_data.data, bin_data.data+ bin_data.size);
    queues[queueId]->write(std::move(data), priority);

    return nifpp::make(env, nifpp::str_atom("ok"));
}


/**
 * @brief Closes the given message queue.
 *
 * @param[in] env The environment.
 * @param[in] arc The count of arguments.
 * @param[in] argv The function arguments.
 * @retval :ok if the queue could be closed.
 * @retval {:error, reason} if an error occurred.
 * @retval ArgumentError if the supplied arguments in argv are invalid
 */
extern "C" ERL_NIF_TERM _close(ErlNifEnv* env, int /*arc*/, const ERL_NIF_TERM argv[])
{
    mqd_t queueId = -1;

    if (!nifpp::get(env, argv[0], queueId))
    {
        return enif_make_badarg(env);
    }

    if (queues.find(queueId) == queues.end())
    {
        return report_errno_error(env, EBADF);
    }

    try
    {
        queues.erase(queueId);
    }
    catch(...)
    {
        return report_errno_error(env, errno);
    }
    return nifpp::make(env, nifpp::str_atom("ok"));
}


/**
 * @brief The background thread function.
 *
 * This function is the event loop and runs the asio io_service.
 *
 * @return nullptr
 */
static void* thread_func(void* /*arg*/)
{
    try
    {
        boost::asio::io_service io_service;
        io = &io_service;
        //do not exit run() when running out of work
        boost::asio::io_service::work work(io_service);
        io_service.run();
        queues.clear();
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


/** The thread options of the background thread */
static ErlNifThreadOpts *thread_opts = nullptr;

/** The thread id of the background thread */
static ErlNifTid tid;

/**
 * @brief Executed when the nif is loaded.
 *
 * Creates the background thread.
 *
 * @retval 0 if successful
 * @retval -1 if an error occurred
 */
extern "C" int load(ErlNifEnv* /*env*/, void** /*priv*/, ERL_NIF_TERM /*load_info*/)
{
    thread_opts = enif_thread_opts_create(const_cast<char*>("thread_opts"));

    if (enif_thread_create(const_cast<char*>(""), &tid, thread_func, nullptr, thread_opts) != 0)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief Executed when the nif is unloaded.
 *
 * Destroys all message queues and stops the background thread.
 *
 */
extern "C" void unload(ErlNifEnv* /*env*/, void* /*priv*/)
{
    void* resp;

    if (io)
    {
        queues.clear();
        io->stop();
    }

    enif_thread_join(tid, &resp);

    enif_thread_opts_destroy(thread_opts);
}

/** Contains all nif functions. */
ErlNifFunc nif_funcs[] =
{
    {"_open",  3, _open,  0},
    {"_read",  1, _read,  0},
    {"_write", 3, _write, 0},
    {"_close", 1, _close, 0}
};

extern "C"
{
ERL_NIF_INIT(Elixir.MessageQueue.Nif, nif_funcs, &load, NULL, NULL, &unload)
}
