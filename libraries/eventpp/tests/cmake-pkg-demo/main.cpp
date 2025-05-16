// eventpp library
// Copyright (C) 2018 Wang Qi (wqking)
// Github: https://github.com/wqking/eventpp
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//   http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Include the head
#include "eventpp/eventqueue.h"

#include <iostream>

int main()
{
	eventpp::EventQueue<int, void (const std::string &, std::unique_ptr<int> &)> queue;

	queue.appendListener(3, [](const std::string & s, std::unique_ptr<int> & n) {
		std::cout << "Got event 3, s is " << s << " n is " << *n << std::endl;
	});
	// The listener prototype doesn't need to be exactly same as the dispatcher.
	// It would be find as long as the arguments is compatible with the dispatcher.
	queue.appendListener(5, [](std::string s, const std::unique_ptr<int> & n) {
		std::cout << "Got event 5, s is " << s << " n is " << *n << std::endl;
	});
	queue.appendListener(5, [](const std::string & s, std::unique_ptr<int> & n) {
		std::cout << "Got another event 5, s is " << s << " n is " << *n << std::endl;
	});

	// Enqueue the events, the first argument is always the event type.
	// The listeners are not triggered during enqueue.
	queue.enqueue(3, "Hello", std::unique_ptr<int>(new int(38)));
	queue.enqueue(5, "World", std::unique_ptr<int>(new int(58)));

	// Process the event queue, dispatch all queued events.
	queue.process();

	return 0;
}

