defmodule MessageQueue.Nif do
  @on_load :init

  def init() do
		path = :filename.join(:code.priv_dir(:message_queue), 'lib_elixir_mq')
    :ok = :erlang.load_nif(path, 0)
  end

  @spec open(String.t, [MessageQueue.mode], {non_neg_integer, non_neg_integer}) :: {:ok, non_neg_integer} | {:error, String.t}
  def open(mq_file, flags, sizes) do
    _open(String.to_char_list(mq_file), flags, sizes)
  end

  @spec read(non_neg_integer) :: {atom, non_neg_integer, bitstring}
  def read(fd) do
    _read(fd)
  end

	@spec write(non_neg_integer, non_neg_integer, bitstring) :: :ok | {:error, String.t}
  def write(fd, priority, bin_data) do
    _write(fd, priority, bin_data)
  end

	@spec controlling_process(non_neg_integer, pid) :: :ok | {:error, String.t}
	def controlling_process(fd, pid) do
    _controlling_process(fd, pid)
	end

	@spec close(non_neg_integer) :: :ok | {:error, String.t}
  def close(fd) do
    _close(fd)
  end

  def _open(_mq_file, _flags, _sizes) do
    :erlang.nif_error("NIF library not loaded")
  end

  def _read(_fd) do
    :erlang.nif_error("NIF library not loaded")
  end

  def _close(_fd) do
    :erlang.nif_error("NIF library not loaded")
  end

  def _write(_fd, _priority, _bin_data) do
    :erlang.nif_error("NIF library not loaded")
  end

	def _controlling_process(_fd, _pid) do
    :erlang.nif_error("NIF library not loaded")
  end
end
