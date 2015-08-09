defmodule MessageQueueTest do
  use ExUnit.Case

	@test_filename "/mq_test.tmp"

	# Create the message queue, if it does not exist
	setup do
		{:ok, fd} = MessageQueue.open @test_filename,  [:read, :write]
		:ok = MessageQueue.close fd
  end

	test "open and close" do
		{:ok, fd} = MessageQueue.open @test_filename
		:ok = MessageQueue.close fd
	end

	test "read empty" do
		{:ok, fd} = MessageQueue.open @test_filename
		:ok = receive do
			_ -> "Did not expect to receive anything"
		after
			1_000 -> :ok
		end
		{:error, 'Bad file descriptor'} = MessageQueue.read fd + 1
		:ok = MessageQueue.close fd
	end

	test "write and read" do
		{:ok, fd} = MessageQueue.open @test_filename,  [:read, :write]
		:ok = receive do
			_ -> "Did not expect to receive anything"
		after
			1_000 -> :ok
		end
		:ok = MessageQueue.write fd, 5, "1234"
		{:error, 'Bad file descriptor'} = MessageQueue.write fd + 1, 6, "5678"
		:ok = receive do
			{:mq, fd, 5, "1234"} -> :ok
			_ -> {:error, "Received unexpected stuff"}
		after
			1_000 -> {:error, "Receive timeout"}
		end
		:ok = receive do
			_ -> {:error, "Did not expect to receive anything"}
		after
			1_000 -> :ok
		end
		:ok = MessageQueue.close fd
	end
end
