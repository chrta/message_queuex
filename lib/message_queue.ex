defmodule MessageQueue do
  alias MessageQueue.Nif, as: Nif
  
  @spec open(String.t, [atom]) :: {atom, integer}
  def open(mq_file, options \\ [:read]) do
    Nif.open(mq_file, options)
  end

  @spec read(integer) :: {atom, integer, bitstring}
  def read(fd) do
    Nif.read(fd)
  end

  @spec write(integer,integer, bitstring) :: atom
  def write(fd, priority, bin_data) do
    Nif.write(fd, priority, bin_data)
  end

  @spec close(integer) :: atom
  def close(fd) do
    Nif.close(fd)
  end
end
