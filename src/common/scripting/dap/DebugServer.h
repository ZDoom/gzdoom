#pragma once

#include "ZScriptDebugger.h"
#include <dap/network.h>
#include <thread>
namespace DebugServer
{
class DebugServer
{
	public:
	DebugServer();
	void runRestartThread();
	~DebugServer();

	bool Listen(int port);
	void Stop();
	private:
	std::unique_ptr<ZScriptDebugger> debugger;
	std::unique_ptr<dap::net::Server> m_server;
	std::condition_variable cv;
	std::mutex mutex; // guards 'terminate'
	bool stopped;
	bool terminate;
	std::thread restart_thread;
};
}