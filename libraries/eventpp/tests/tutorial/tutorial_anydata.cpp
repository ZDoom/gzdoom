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
#include "eventpp/utilities/anydata.h"
#include "eventpp/eventqueue.h"
#include "tutorial.h"

#include <iostream>

namespace {

// In the tutorials here, we define an event class hierarchy, Event is the base class.

// Define the event types
enum class EventType
{
	// for MouseEvent
	mouse,

	// for KeyEvent
	key,

	// for MessageEvent
	message,

	// For a simple std::string event which is not derived from Event 
	text,
};

class Event
{
public:
	Event() {
	}
};

class MouseEvent : public Event
{
public:
	MouseEvent(const int x, const int y)
		: x(x), y(y)
	{
	}

	int getX() const { return x; }
	int getY() const { return y; }

private:
	int x;
	int y;
};

class KeyEvent : public Event
{
public:
	explicit KeyEvent(const int key)
		: key(key)
	{
	}

	int getKey() const { return key; }

private:
	int key;
};

class MessageEvent : public Event
{
public:
	explicit MessageEvent(const std::string & message)
		: message(message) {
	}

	std::string getMessage() const { return message; }

private:
	std::string message;
};

// eventMaxSize is the maximum size of all possible types to use in AnyData, it's used to construct AnyData.
constexpr std::size_t eventMaxSize = eventpp::maxSizeOf<
		Event,
		KeyEvent,
		MouseEvent,
		MessageEvent,
		std::string
	>();

void onMessageEvent(const MessageEvent & e)
{
	std::cout << "Received MessageEvent in free function, message="
		<< e.getMessage() << std::endl;
}

TEST_CASE("AnyData tutorial 1, basic")
{
	std::cout << std::endl << "AnyData tutorial 1, basic" << std::endl;

	eventpp::EventQueue<EventType, void (const eventpp::AnyData<eventMaxSize> &)> queue;
	// Append a listener. Note the listener argument is `const Event &` which is different with the prototype in
	// the queue definition. This works since `AnyData` can convert to any data type automatically.
	queue.appendListener(EventType::key, [](const Event & e) {
		std::cout << "Received KeyEvent, key="
			<< static_cast<const KeyEvent &>(e).getKey() << std::endl;
	});
	queue.appendListener(EventType::mouse, [](const Event & e) {
		std::cout << "Received MouseEvent, x=" << static_cast<const MouseEvent &>(e).getX()
			<< " y=" << static_cast<const MouseEvent &>(e).getY() << std::endl;
	});
	// Even more convenient, the argument type can be the concrete class such as MessageEvent,
	// but be sure the listener only receive MessageEvent. If it also receives MouseEvent,
	// we can expect crash.
	queue.appendListener(EventType::message, [](const MessageEvent & e) {
		std::cout << "Received MessageEvent, message="
			<< e.getMessage() << std::endl;
	});
	// Not only lambda, we can also use free function, or member function as the listener.
	queue.appendListener(EventType::message, &onMessageEvent);
	// Put events into the queue. Any data type, such as KeyEvent, MouseEvent, can be put
	// as long as the data size doesn't exceed eventMaxSize.
	queue.enqueue(EventType::key, KeyEvent(255));
	queue.enqueue(EventType::mouse, MouseEvent(3, 5));
	queue.enqueue(EventType::message, MessageEvent("Hello, AnyData"));
	queue.process();
}

TEST_CASE("AnyData tutorial 2, unrelated data")
{
	std::cout << std::endl << "AnyData tutorial 2, unrelated data" << std::endl;

	// It's possible to send event with data that doesn't have the same base class, such as Event.
	// To do so, the listener prototype must be `const void *` instead of `const Event &` in previous tutorial.
	struct Policies {
		using Callback = std::function<void (const EventType, const void *)>;
	};
	eventpp::EventQueue<
		EventType,
		void (const EventType, const eventpp::AnyData<eventMaxSize> &),
		Policies> queue;
	queue.appendListener(EventType::key, [](const EventType type, const void * e) {
		REQUIRE(type == EventType::key);
		std::cout << "Received KeyEvent, key=" << static_cast<const KeyEvent *>(e)->getKey() << std::endl;
	});
	queue.appendListener(EventType::mouse, [](const EventType type, const void * e) {
		REQUIRE(type == EventType::mouse);
		std::cout << "Received MouseEvent, x=" << static_cast<const MouseEvent *>(e)->getX()
			<< " y=" << static_cast<const MouseEvent *>(e)->getY() << std::endl;
	});
	queue.appendListener(EventType::message, [](const EventType type, const void * e) {
		REQUIRE(type == EventType::message);
		std::cout << "Received MessageEvent, message="
			<< static_cast<const MessageEvent *>(e)->getMessage() << std::endl;
	});
	queue.appendListener(EventType::text, [](const EventType type, const void * e) {
		REQUIRE(type == EventType::text);
		std::cout << "Received text event, text=" << *static_cast<const std::string *>(e) << std::endl;
	});
	// Send events
	queue.enqueue(EventType::key, KeyEvent(255));
	queue.enqueue(EventType::mouse, MouseEvent(3, 5));
	queue.enqueue(EventType::message, MessageEvent("Hello, AnyData"));
	// Send a std::string as the event data which doesn't derive from Event.
	queue.enqueue(EventType::text, std::string("This is a text"));
	queue.process();
}

TEST_CASE("AnyData tutorial 3, use AnyData in listener")
{
	std::cout << std::endl << "AnyData tutorial 3, use AnyData in listener" << std::endl;

	using MyData = eventpp::AnyData<eventMaxSize>;
	eventpp::EventQueue<EventType, void (const EventType, const MyData &)> queue;
	queue.appendListener(EventType::key, [](const EventType type, const MyData & e) {
		REQUIRE(type == EventType::key);
		REQUIRE(e.isType<KeyEvent>());
		std::cout << "Received KeyEvent, key=" << e.get<KeyEvent>().getKey() << std::endl;
	});
	queue.appendListener(EventType::mouse, [](const EventType type, const MyData & e) {
		REQUIRE(type == EventType::mouse);
		REQUIRE(e.isType<MouseEvent>());
		std::cout << "Received MouseEvent, x=" << e.get<MouseEvent>().getX()
			<< " y=" << e.get<MouseEvent>().getY() << std::endl;
	});
	queue.appendListener(EventType::message, [](const EventType type, const MyData & e) {
		REQUIRE(type == EventType::message);
		REQUIRE(e.isType<MessageEvent>());
		std::cout << "Received MessageEvent, message=" << e.get<MessageEvent>().getMessage() << std::endl;
	});
	queue.appendListener(EventType::text, [](const EventType type, const MyData & e) {
		REQUIRE(type == EventType::text);
		REQUIRE(e.isType<std::string>());
		std::cout << "Received text event, text=" << e.get<std::string>() << std::endl;
	});
	queue.enqueue(EventType::key, KeyEvent(255));
	queue.enqueue(EventType::mouse, MouseEvent(3, 5));
	queue.enqueue(EventType::message, MessageEvent("Hello, AnyData"));
	queue.enqueue(EventType::text, std::string("This is a text"));
	queue.process();
}


} // namespace
