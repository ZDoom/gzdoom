// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// simple_net_client_server demonstrates a minimal DAP network connection
// between a server and client.

#include "dap/io.h"
#include "dap/network.h"
#include "dap/protocol.h"
#include "dap/session.h"

#include <condition_variable>
#include <mutex>

int main(int, char*[]) {
  constexpr int kPort = 19021;

  // Callback handler for a socket connection to the server
  auto onClientConnected =
      [&](const std::shared_ptr<dap::ReaderWriter>& socket) {
        auto session = dap::Session::create();

        // Set the session to close on invalid data. This ensures that data received over the network
        // receives a baseline level of validation before being processed.
        session->setOnInvalidData(dap::kClose);

        session->bind(socket);

        // The Initialize request is the first message sent from the client and
        // the response reports debugger capabilities.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Initialize
        session->registerHandler([&](const dap::InitializeRequest&) {
          dap::InitializeResponse response;
          printf("Server received initialize request from client\n");
          return response;
        });

        // Signal used to terminate the server session when a DisconnectRequest
        // is made by the client.
        bool terminate = false;
        std::condition_variable cv;
        std::mutex mutex;  // guards 'terminate'

        // The Disconnect request is made by the client before it disconnects
        // from the server.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Disconnect
        session->registerHandler([&](const dap::DisconnectRequest&) {
          // Client wants to disconnect. Set terminate to true, and signal the
          // condition variable to unblock the server thread.
          std::unique_lock<std::mutex> lock(mutex);
          terminate = true;
          cv.notify_one();
          return dap::DisconnectResponse{};
        });

        // Wait for the client to disconnect (or reach a 5 second timeout)
        // before releasing the session and disconnecting the socket to the
        // client.
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, std::chrono::seconds(5), [&] { return terminate; });
        printf("Server closing connection\n");
      };

  // Error handler
  auto onError = [&](const char* msg) { printf("Server error: %s\n", msg); };

  // Create the network server
  auto server = dap::net::Server::create();
  // Start listening on kPort.
  // onClientConnected will be called when a client wants to connect.
  // onError will be called on any connection errors.
  server->start(kPort, onClientConnected, onError);

  // Create a socket to the server. This will be used for the client side of the
  // connection.
  auto client = dap::net::connect("localhost", kPort);
  if (!client) {
    printf("Couldn't connect to server\n");
    return 1;
  }

  // Attach a session to the client socket.
  auto session = dap::Session::create();
  session->bind(client);

  // Set an initialize request to the server.
  auto future = session->send(dap::InitializeRequest{});
  printf("Client sent initialize request to server\n");
  printf("Waiting for response from server...\n");
  // Wait on the response.
  auto response = future.get();
  printf("Response received from server\n");
  printf("Disconnecting...\n");
  // Disconnect.
  session->send(dap::DisconnectRequest{});

  return 0;
}
