defmodule MessageQueue.Nif do
  @on_load :init

  def init() do
		path = :filename.join(:code.priv_dir(:message_queue), 'lib_elixir_mq')
    :ok = :erlang.load_nif(path, 0)
  end

	# options [:read, :write]
  @spec open(String.t, [atom]) :: {atom, integer}
  def open(mq_file, options) do
    _open(String.to_char_list(mq_file), options)
  end

  @spec read(integer) :: {atom, integer, bitstring}
  def read(fd) do
    _read(fd)
  end

  @spec write(integer,integer, bitstring) :: atom
  def write(fd, priority, bin_data) do
    _write(fd, priority, bin_data)
  end

  @spec close(integer) :: atom
  def close(fd) do
    _close(fd)
  end

  def _open(mq_file, options) do
    :erlang.nif_error("NIF library not loaded")
  end

  def _read(fd) do
    :erlang.nif_error("NIF library not loaded")
  end

  def _close(fd) do
    :erlang.nif_error("NIF library not loaded")
  end

  def _write(fd, priority, bin_data) do
    :erlang.nif_error("NIF library not loaded")
  end
end
