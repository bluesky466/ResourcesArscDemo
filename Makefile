demo : ByteOrder.h ResourceTypes.h configuration.h ResourceTypes.cpp main.cpp
	g++ main.cpp ResourceTypes.cpp -o demo -std=c++11

.PHONY : clean
clean :
	rm demo
