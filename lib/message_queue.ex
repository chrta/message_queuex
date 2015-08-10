defmodule MessageQueue do
  alias MessageQueue.Nif, as: Nif
  
  @spec open(String.t, [atom], {integer, integer}) :: {atom, integer}
  def open(mq_file, flags \\ [:read], sizes \\ {}) do
    Nif.open(mq_file, flags, sizes)
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
