ERLANG_PATH = $(shell erl -eval 'io:format("~s", [lists:concat([code:root_dir(), "/erts-", erlang:system_info(version), "/include"])])' -s init stop -noshell)

CFLAGS = -g -O3 -ansi -pedantic -Wall -Wextra -I$(ERLANG_PATH)
CXXFLAGS = $(CFLAGS) -std=c++14

LDFLAGS += -lboost_system -lboost_thread

ifneq ($(OS),Windows_NT)
	CFLAGS += -fPIC

	ifeq ($(shell uname),Darwin)
		LDFLAGS += -dynamiclib -undefined dynamic_lookup
	else
		LDFLAGS += -shared
	endif
endif

priv/lib_elixir_mq.so: c_src/lib_elixir_mq.cpp c_src/message_queue.hpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<

clean:
	$(RM) -r priv/*
