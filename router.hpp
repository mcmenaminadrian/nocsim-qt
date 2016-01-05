#define FLITSIZE 36

#ifndef __ROUTER_
#define __ROUTER_


//simple RPC implementation


class Router {
private:
	long column;
	long row;
	std::bitset<FLITSIZE> buffer;
	bool canAccept;
	bool locked;
	bool sending;
	bool receiving;
	bool readyToSend;
	

public:
	Router(const long c, const long r);
	int send();
	int receive();  

};

#endif
