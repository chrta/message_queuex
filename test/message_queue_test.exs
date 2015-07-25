defmodule MessageQueueTest do
  use ExUnit.Case

		@test_filename "/mq_test.tmp"

	test "open and close" do
		{:ok, fd} = MessageQueue.open @test_filename
		:ok = MessageQueue.close fd
	end

	test "read empty" do
		{:ok, fd} = MessageQueue.open @test_filename
		{:ok, 0, ""} = MessageQueue.read fd
		{:error, 'Bad file descriptor'} = MessageQueue.read fd + 1
		:ok = MessageQueue.close fd
	end

	test "write and read" do
		{:ok, fd} = MessageQueue.open @test_filename
		{:ok, 0, ""} = MessageQueue.read fd
		:ok = MessageQueue.write fd, 5, "1234"
		{:error, 'Bad file descriptor'} = MessageQueue.write fd + 1, 6, "5678"
		{:ok, 5, "1234"} = MessageQueue.read fd
		{:ok, 0, ""} = MessageQueue.read fd
		:ok = MessageQueue.close fd
	end


end
