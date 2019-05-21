demo : ByteOrder.h ResourceTypes.h configuration.h ResourceTypes.cpp main.cpp
	g++ main.cpp ResourceTypes.cpp -o demo

.PHONY : clean
clean :
	rm demo
