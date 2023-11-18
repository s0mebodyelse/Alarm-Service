OUTPUT = server
INPUT = ./source/main.cpp
MODULES = ./source/server.cpp ./source/session.cpp 
FLAGS = -Wall -Wextra -std=c++20 
INCLUDE = -lboost_system

build_client:
	g++ -g client.cpp -o client -lboost_system -lboost_program_options $(FLAGS) 

build_server:
	g++ $(INCLUDE) $(INPUT) $(MODULES) -o $(OUTPUT) $(FLAGS)

build_container: build_server
	docker build -t boost_alarm_service .

build: build_client build_server

run_server: build_server
	./server 2000
