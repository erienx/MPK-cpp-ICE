.PHONY: all clean
all: system tram client

SIP.cpp SIP.h:
	slice2cpp SIP.ice

system: System.cpp SIP.cpp
	g++ -I. System.cpp SIP.cpp -lIce -lpthread -o system

tram: Tram.cpp SIP.cpp
	g++ -I. Tram.cpp SIP.cpp -lIce -lpthread -o tram

client: Client.cpp SIP.cpp
	g++ -I. Client.cpp SIP.cpp -lIce -lpthread -o client

clean:
	rm -f SIP.cpp SIP.h system tram client
