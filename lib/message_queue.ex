defmodule MessageQueue do
  @moduledoc ~S"""
  Provides the interface to the posix message queue api.

	After opening the message queue, all received messages are send automatically
	to the process that opened the queue. The format of the message is
	`{:mq, fd, priority, data}`

	`:mq` specifies that the data is received from a message queue.

	`fd` contains the descriptor of the queue.

	`priority` contains the priority of the received message.

	`data` contains the message data.

  ## Examples

			iex> {:ok, fd} = MessageQueue.open "/queue_name", [:read, :write], {10, 10}
			{:ok, 15}
			iex> MessageQueue.write fd, 1, "Data"
			:ok
			iex> MessageQueue.close fd
			iex> receive do
			...>   {:mq, ^fd, prio, data} -> IO.puts "Received message with priority #{prio}: '#{data}'"
			...> after
			...>   100 -> IO.puts "Receive timeout"
			...> end
			:ok

  """

	alias MessageQueue.Nif, as: Nif

	@type mode :: :read | :write

  @doc ~S"""
	Opens the message queue.

	The queue must be closed when it is no longer needed by calling `close/1`.

	See the documentation `man mq_open` for a description of the wrapped
	function.

	The parameter `mq_name` is the name of the message queue. It must start
	with a `/`.

	The parameter `flags` is an array of modes:
	* `:read` - the queue must exist when opening it. It is opened for reading.
  * `:write` - The queue is automatically created, if it does not exist. If this is
	  specified, the last parameter sizes must be given, too.

	The parameter `sizes` is a tuple containing two elements. This parameter is only used,
	when flags contains :write. The first tuple element specifies the maximum message count
	of the created queue. The second element specifies the maximum message size in byte.

	The function returns:
	* `{:ok, fd}` if sucessful. `fd` is the descriptor of the queue that must be used with all subsequent operations on that queue.
  * `{:error, reason}` - the queue could not be opened.

	If the given parameter are malformed, an `ArgumentError` is raised.

  ## Examples

			iex> {:ok, fd} = MessageQueue.open "/queue_name", [:read, :write], {10, 10}
			{:ok, 15}
			iex> MessageQueue.close fd
			:ok

  """
  @spec open(String.t, [mode], {non_neg_integer, non_neg_integer}) :: {:ok, non_neg_integer} | {:error, String.t}
  def open(mq_name, flags \\ [:read], sizes \\ {}) do
    Nif.open(mq_name, flags, sizes)
  end

	@doc ~S"""
	This function is reserved for future use ;).

  *Do not use this function right now.*
	"""
  @spec read(non_neg_integer) :: {atom, non_neg_integer, bitstring}
  def read(fd) do
    Nif.read(fd)
  end

	@doc ~S"""
	Writes data to the queue.

	See the documentation `man mq_send` for a description of the wrapped
	function.

  This function operates asynchronously. This function queues the given data
	for sending. It the queue is writable, the native mq_send() is called.

  The parameter `fd` is the descriptor of the queue returned by `open/3`.

  The parameter `priority` is the priority of the message. A higher priority
	gets delivered before lower priorities.

  The parameter `bin_data` contains the data that should be send.

	The function returns:
	* `:ok` if sucessful. The given data is queued for sending to the message queue.
  * `{:error, reason}` - An error occurred.

	If the given parameter are malformed, an `ArgumentError` is raised.

  ## Examples

			iex> {:ok, fd} = MessageQueue.open "/queue_name", [:read, :write], {10, 10}
			{:ok, 15}
			iex> MessageQueue.write fd, 1, "Data"
			:ok
			iex> MessageQueue.close fd
			iex> IEx.Helpers.flush
			{:mq, 15, 1, "Data"}
			:ok

  """
  @spec write(non_neg_integer, non_neg_integer, bitstring) :: :ok | {:error, String.t}
  def write(fd, priority, bin_data) do
    Nif.write(fd, priority, bin_data)
  end

	@doc ~S"""
	Assigns a new controlling process to the given message queue.

  The controlling process is the process which receives messages from the queue. If called by any other process than
  the current controlling process, an error is returned.

  The parameter `fd` is the descriptor of the queue returned by `open/3`.

  The function returns:
  * `:ok` if sucessful.
  * `{:error, reason}` - the controlling process could not be changed.

	If the given parameter are malformed, an `ArgumentError` is raised.

  ## Examples

			iex> {:ok, fd} = MessageQueue.open "/queue_name", [:read, :write], {10, 10}
			{:ok, 15}
			iex> pid = spawn fn -> receive do
			...> {:mq, fd, prio, data} -> IO.puts "Received data from #{fd}"
			...> end
			...> end
			iex> MessageQueue.controlling_process fd, pid
			:ok
			iex> MessageQueue.write fd, 2, "Test"
			:ok
			Received data from 15
			iex> MessageQueue.close fd
			:ok

  """
  @spec controlling_process(non_neg_integer, pid) :: :ok | {:error, String.t}
  def controlling_process(fd, pid) do
    Nif.controlling_process(fd, pid)
  end

	@doc ~S"""
	Closes an open message queue.

  The parameter `fd` is the descriptor of the queue returned by `open/3`.

	The function returns:
	* `:ok` if sucessful.
  * `{:error, reason}` - the queue could not be closed.

	If the given parameter are malformed, an `ArgumentError` is raised.

  ## Examples

			iex> {:ok, fd} = MessageQueue.open "/queue_name", [:read, :write], {10, 10}
			{:ok, 15}
			iex> MessageQueue.close fd
			:ok


  """
  @spec close(non_neg_integer) :: :ok | {:error, String.t}
  def close(fd) do
    Nif.close(fd)
  end
end
