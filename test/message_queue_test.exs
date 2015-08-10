defmodule MessageQueueTest do
  use ExUnit.Case

	@test_queuename "/mq_test.tmp"

	# Create the message queue, if it does not exist
	setup do
		{:ok, fd} = MessageQueue.open @test_queuename,  [:read, :write], {10, 10}
		receive do
			_ -> empty_queue
		after
			100 ->
		end
		:ok = MessageQueue.close fd
  end

	defp empty_queue do
		receive do
			_ -> empty_queue
		after
			100 ->
		end
	end

	test "open and close" do
		{:ok, fd} = MessageQueue.open @test_queuename
		:ok = MessageQueue.close fd
	end

	test "read empty" do
		{:ok, fd} = MessageQueue.open @test_queuename
		:ok = receive do
			_ -> "Did not expect to receive anything"
		after
			100 -> :ok
		end
		{:error, 'Bad file descriptor'} = MessageQueue.read fd + 1
		:ok = MessageQueue.close fd
	end

	test "write and read" do
		{:ok, fd} = MessageQueue.open @test_queuename,  [:read, :write], {10, 10}
		:ok = receive do
			_ -> "Did not expect to receive anything"
		after
			100 -> :ok
		end
		:ok = MessageQueue.write fd, 1, "1234"
		{:error, 'Bad file descriptor'} = MessageQueue.write fd + 1, 6, "5678"
		:ok = receive do
			{:mq, ^fd, 1, "1234"} -> :ok
			_ -> {:error, "Received unexpected stuff"}
		after
			100 -> {:error, "Receive timeout"}
		end
		:ok = receive do
			_ -> {:error, "Did not expect to receive anything"}
		after
			100 -> :ok
		end
		:ok = MessageQueue.close fd
	end

	test "write large message" do
		{:ok, fd} = MessageQueue.open @test_queuename,  [:read, :write], {10, 10}
		:ok = receive do
			_ -> "Did not expect to receive anything"
		after
			100 -> :ok
		end
		:ok = MessageQueue.write fd, 2, "1234"
		:ok = MessageQueue.write fd, 3, "12345678901" # try to write 11 bytes
		:ok = receive do
			{:mq, ^fd, 2, "1234"} -> :ok
			_ -> {:error, "Received unexpected stuff"}
		after
			100 -> {:error, "Receive timeout"}
		end
		:ok = receive do
			_ -> {:error, "Received unexpected stuff"}
		after
			100 -> :ok
		end
		:ok = MessageQueue.close fd
	end

	# This test may fail is the messages are send to slow
	test "message priority" do
		{:ok, fd} = MessageQueue.open @test_queuename,  [:read, :write], {10, 10}
		:ok = receive do
			_ -> "Did not expect to receive anything"
		after
			100 -> :ok
		end
		:ok = MessageQueue.write fd, 4, "1234"
		:ok = MessageQueue.write fd, 5, "abcd" #higher prio

		:ok = receive do
			{:mq, ^fd, 5, "abcd"} -> :ok
			{:mq, ^fd, 4, "1234"} -> {:error, "Received message in the wrong order"}
			_ -> {:error, "Received unexpected stuff"}
		after
			100 -> {:error, "Receive timeout"}
		end

		:ok = receive do
			{:mq, ^fd, 4, "1234"} -> :ok
			_ -> {:error, "Received unexpected stuff"}
		after
			1_000 -> {:error, "Receive timeout"}
		end
		
		:ok = MessageQueue.close fd
	end

	test "parallel write" do
		{:ok, fd} = MessageQueue.open @test_queuename,  [:read, :write], {10, 10}
		Enum.each(1..1_000, fn(x) ->
			spawn(fn() ->
				:ok = MessageQueue.write fd, x, "#{x}"
			end)
		end)
		Enum.map(1..1_000, fn(_) ->
			:ok = receive do
				{:mq, fd, prio, data} -> :ok
			after
				100 -> :error
			end
		end)
		:ok = MessageQueue.close fd
	end
end
