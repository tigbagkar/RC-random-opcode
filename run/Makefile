
run-target:
	g++ -std=c++17 -Wall -Wextra -pthread ../src/Target.cpp ../src/RC.cpp ../src/TcpChannel.cpp -libverbs -o target
	clear
	./target

run-initiator: 
	g++ -std=c++17 -Wall -Wextra -pthread ../src/Initiator.cpp ../src/RC.cpp ../src/TcpChannel.cpp -libverbs -o initiator
	clear
	./initiator

clean:
	rm target initiator

.PHONY: run-target run-initiator clean