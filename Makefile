muti_chat_server:chat_business.o reactor.o 
	g++ chat_business.o reactor.o -o muti_chat_server -std=c++11 -lpthread

chat_business.o:chat_business.cpp chat_business.h
	g++ -c chat_business.cpp -o chat_business.o -std=c++11 

reactor.o:reactor.cpp chat_business.h threadpool.h locker.h 
	g++ -c reactor.cpp -o reactor.o -std=c++11  -lpthread
	
clean:
	rm -rf *.o muti_chat_server
