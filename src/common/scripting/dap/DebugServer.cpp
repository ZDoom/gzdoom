#include "DebugServer.h"
#include <thread>
#include <functional>
#include <common/engine/printf.h>

// Main entry point for the debug server

namespace DebugServer
{
DebugServer::DebugServer()
{
	terminate = false;
	stopped = false;
	quitting = false;
	debugger = std::unique_ptr<ZScriptDebugger>(new ZScriptDebugger());
}

void DebugServer::runRestartThread()
{
	while (true)
	{
		std::unique_lock<std::mutex> lock(mutex);
		cv.wait(lock, [&] { return terminate; });
		terminate = false;
		quitting = debugger->EndSession();
		if (stopped || quitting)
		{
			break;
		}
	}
	if (quitting)
	{
		throw CExitEvent(0);
	}
}

bool DebugServer::Listen(int port)
{
	terminate = false;
	stopped = false;
	quitting = false;
	if (!port)
	{
		return false;
	}
	if (!m_server)
	{
		m_server = dap::net::Server::create();
	}
	else
	{
		Stop();
	}
	restart_thread = std::thread(std::bind(&DebugServer::runRestartThread, this));

	auto onClientConnected = [&](const std::shared_ptr<dap::ReaderWriter> &connection)
	{
		std::shared_ptr<dap::Session> sess;
		sess = dap::Session::create();
		sess->bind(connection);
		// Registering a handle for when we send the DisconnectResponse;
		// After we send the disconnect response, the restart thread will stop the session and restart the server.
		sess->registerSentHandler(
			[&](const dap::ResponseOrError<dap::DisconnectResponse> &)
			{
				std::unique_lock<std::mutex> lock(mutex);
				terminate = true;
				cv.notify_all();
			});
		LogInternal("DAP Client connected\n");
		debugger->StartSession(sess);
	};

	auto onError = [&](const char *msg)
	{
		LogInternalError("Server error: %s\n", msg);
	};

	m_server->start(port, onClientConnected, onError);
	return true;
}

void DebugServer::Stop()
{
	{
		std::unique_lock<std::mutex> lock(mutex);
		terminate = true;
		stopped = true;
		cv.notify_all();
	}
	m_server->stop();
	if (restart_thread.joinable())
	{
		restart_thread.join();
	}
}

DebugServer::~DebugServer() { Stop(); }
}