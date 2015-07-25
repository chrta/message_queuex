defmodule MessageQueue do
	  @on_load :init

  def init() do
    :erlang.load_nif("./priv_dir/lib_elixir_mq", 0)
  end

  def open(mq_file) do
    _open(String.to_char_list(mq_file))
  end

  def read(fd) do
    _read(fd)
  end

  def write(fd, priority, bin_data) do
    _write(fd, priority, bin_data)
  end

  def close(fd) do
    _close(fd)
  end

  def _open(mq_file) do
    "NIF library not loaded"
  end

  def _read(fd) do
    "NIF library not loaded"
  end

  def _close(fd) do
    "NIF library not loaded"
  end

  def _write(fd, priority, bin_data) do
    "NIF library not loaded"
  end
end
