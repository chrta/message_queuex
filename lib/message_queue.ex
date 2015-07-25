defmodule MessageQueue do
    @on_load :init

  def init() do
    :erlang.load_nif("./priv_dir/lib_elixir_mq", 0)
  end

  @spec open(String.t) :: {atom, integer}
  def open(mq_file) do
    _open(String.to_char_list(mq_file))
  end

  @spec open(integer) :: {atom, integer, bitstring}
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

  def _open(mq_file) do
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
