#pragma once
#include <memory>

#include "executor/client.h"
#include "executor/websocket.h"

class REPL {
private:
	Websocket* _server;
public:
	REPL(Websocket* server);

	void Run();
};