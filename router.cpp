#include <bitset>
#include "router.hpp"

Router::Router(const long c, const long r): column(c), row(r)
{
	canAccept = true;
	locked = false;
	sending = false;
	receiving = false;
	readyToSend = false;
}

// return 0 = blocked, 1 = succeeded, -1 = nothing to do

int Router::send()
{
	
	if (!sending && !readyToSend) {
		return -1;
	}

	if (!sending && readyToSend) {
		uint8_t destinationColumn = 0;
		uint8_t destinationRow = 0;
		for (int i = 0; i < 5; i++) {
			buffer[i] = column & (1 << i);
			buffer[i + 5] = row & (1 << i);
			if (buffer[i + 10]) {
				destinationColumn |= (1 << i);
			}
			if (buffer[i + 15]) {
				destinationRow |= (1 << i);
			}  
		}
		sending = true;
		return 0;
	}
}
		
		
		
