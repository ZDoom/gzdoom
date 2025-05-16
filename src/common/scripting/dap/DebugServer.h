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
	~DebugServer();

	void RunRestartThread();
	bool Listen(int port);
	void Stop();
	private:
	using ResetThreadLock = std::unique_lock<std::mutex>;
	std::unique_ptr<ZScriptDebugger> debugger;
	std::unique_ptr<dap::net::Server> m_server;
	std::condition_variable cv;
	std::mutex mutex; // guards 'terminate'
	bool stopped = false;
	bool terminate = false;
	bool closed = false;
	bool quitting = false; // On receiving a disconnect request with a terminateDebuggee flag
	std::thread restart_thread;
};
}