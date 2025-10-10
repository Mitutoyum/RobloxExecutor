#include <iostream>
#include <vector>
#include <Windows.h>
#include <memory>

#include "ixwebsocket/IXNetSystem.h"
#include "executor/process.h"
#include "executor/websocket.h"
#include "executor/repl.h"


int main() {
	EnableVirtualTerminal();
	ix::initNetSystem();

	Websocket server("127.0.0.1", 9001);
	REPL repl(server);

	server.Run();
	repl.Run();


	return 0;
}