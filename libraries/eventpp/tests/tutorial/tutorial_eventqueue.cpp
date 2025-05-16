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
#include "eventpp/utilities/orderedqueuelist.h"

#include "tutorial.h"

#include <iostream>
#include <thread>

namespace {

TEST_CASE("EventQueue tutorial 1, basic")
{
	std::cout << std::endl << "EventQueue tutorial 1, basic" << std::endl;

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
}

TEST_CASE("EventQueue tutorial 2, multiple threading")
{
	std::cout << std::endl << "EventQueue tutorial 2, multiple threading" << std::endl;

	using EQ = eventpp::EventQueue<int, void (int)>;
	EQ queue;

	constexpr int stopEvent = 1;
	constexpr int otherEvent = 2;

	// Start a thread to process the event queue.
	// All listeners are invoked in that thread.
	std::thread thread([stopEvent, otherEvent, &queue]() {
		volatile bool shouldStop = false;
		queue.appendListener(stopEvent, [&shouldStop](int) {
			shouldStop = true;
		});
		queue.appendListener(otherEvent, [](const int index) {
			std::cout << "Got event, index is " << index << std::endl;
		});

		while(! shouldStop) {
			queue.wait();

			queue.process();
		}
	});

	// Enqueue an event from the main thread. After sleeping for 10 milliseconds,
	// the event should have be processed by the other thread.
	queue.enqueue(otherEvent, 1);
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	std::cout << "Should have triggered event with index = 1" << std::endl;

	queue.enqueue(otherEvent, 2);
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	std::cout << "Should have triggered event with index = 2" << std::endl;

	{
		// EventQueue::DisableQueueNotify is a RAII class that
		// disables waking up any waiting threads.
		// So no events should be triggered in this code block.
		// DisableQueueNotify is useful when adding lots of events at the same time
		// and only want to wake up the waiting threads after all events are added.
		EQ::DisableQueueNotify disableNotify(&queue);

		queue.enqueue(otherEvent, 10);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		std::cout << "Should NOT trigger event with index = 10" << std::endl;
		
		queue.enqueue(otherEvent, 11);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		std::cout << "Should NOT trigger event with index = 11" << std::endl;
	}
	// The DisableQueueNotify object is destroyed here, and has resumed
	// waking up waiting threads. So the events should be triggered.
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	std::cout << "Should have triggered events with index = 10 and 11" << std::endl;

	queue.enqueue(stopEvent, 1);
	thread.join();
}

// In tutorial 3, we will demonstrate how to make EventQueue dispatch higher priority event earlier.

// First let's define the event struct. e is the event type, priority determines the priority.
struct MyEvent
{
	int e;
	int priority;
};

// The comparison function object used by eventpp::OrderedQueueList.
// The function compares the event by priority.
struct MyCompare
{
	template <typename T>
	bool operator() (const T & a, const T & b) const {
		return a.template getArgument<0>().priority > b.template getArgument<0>().priority;
	}
};

// Define the EventQueue policy
struct MyPolicy
{
	template <typename Item>
	using QueueList = eventpp::OrderedQueueList<Item, MyCompare >;

	static int getEvent(const MyEvent & event) {
		return event.e;
	}
};

TEST_CASE("EventQueue tutorial 3, ordered queue")
{
	std::cout << std::endl << "EventQueue tutorial 3, ordered queue" << std::endl;

	using EQ = eventpp::EventQueue<int, void(const MyEvent &), MyPolicy>;
	EQ queue;

	queue.appendListener(3, [](const MyEvent & event) {
		std::cout << "Get event " << event.e << "(should be 3)."
			<< " priority: " << event.priority << std::endl;
	});
	queue.appendListener(5, [](const MyEvent & event) {
		std::cout << "Get event " << event.e << "(should be 5)."
			<< " priority: " << event.priority << std::endl;
	});
	queue.appendListener(7, [](const MyEvent & event) {
		std::cout << "Get event " << event.e << "(should be 7)."
			<< " priority: " << event.priority << std::endl;
	});

	// Add an event, the first number 5 is the event type, the second number 100 is the priority.
	// After the queue processes, the events will be processed from higher priority to lower priority.
	queue.enqueue(MyEvent{ 5, 100 });
	queue.enqueue(MyEvent{ 5, 200 });
	queue.enqueue(MyEvent{ 7, 300 });
	queue.enqueue(MyEvent{ 7, 400 });
	queue.enqueue(MyEvent{ 3, 500 });
	queue.enqueue(MyEvent{ 3, 600 });

	queue.process();
}

// In tutorial 4, we will demonstrate how different event classes are used in a typical event system in an application.
// A typical event system may look like, there is a base event class, each event type has a corresponding event class
// that inherits from the base event. When emitting an event, a pointer/referent to the base event class is used,
// the event listener then cast the base event to proper derived event.

// This is the definition of event types
enum class EventType
{
	// for MouseEvent
	mouse,
	
	// for KeyboardEvent
	keyboard,
	
	// for either MouseEvent or KeyboardEvent, it's used to demonstrate
	// how the listener detects event type dynamically
	input,

	// for ChangedEvent
	changed
};

// This is the base event. It has a getType function to return the actual event type.
class Event
{
public:
	explicit Event(const EventType type) : type(type) {
	}

	virtual ~Event() {
	}

	EventType getType() const {
		return type;
	}

private:
	EventType type;
};

class MouseEvent : public Event
{
public:
	MouseEvent(const int x, const int y)
		: Event(EventType::mouse), x(x), y(y)
	{
	}

	int getX() const { return x; }
	int getY() const { return y; }

private:
	int x;
	int y;
};

class KeyboardEvent : public Event
{
public:
	explicit KeyboardEvent(const int key)
		: Event(EventType::keyboard), key(key)
	{
	}

	int getKey() const { return key; }

private:
	int key;
};

class ChangedEvent : public Event
{
public:
	explicit ChangedEvent(const std::string & text)
		: Event(EventType::changed), text(text)
	{
	}

	std::string getText() const { return text; }

private:
	std::string text;
};

// We will pass event as EventPointer, here it's std::shared_ptr<Event>.
// It allows EventQueue to store the events in internal buffer without slicing the objects
// in asynchronous API (EventQueue::enqueue and EventQueue::process, etc).
// If we only use the synchronous API (EventDispatcher, or EventQueue::dispatch),
// we can dispatch events as reference.
using EventPointer = std::shared_ptr<Event>;

// We are going to dispatch event objects directly without specify the event type explicitly,
// so we need to define policy to let eventpp know how to get the event type from the event object.
struct EventPolicy
{
	static EventType getEvent(const EventPointer & event) {
		return event->getType();
	}
};

TEST_CASE("EventQueue tutorial 4, typical event system in an application")
{
	std::cout << std::endl << "EventQueue tutorial 4, typical event system in an application" << std::endl;

	using EQ = eventpp::EventQueue<EventType, void(const EventPointer &), EventPolicy>;
	EQ queue;

	queue.appendListener(EventType::mouse, [](const EventPointer & event) {
		const MouseEvent * mouseEvent = static_cast<const MouseEvent *>(event.get());
		std::cout << "Received mouse event, x=" << mouseEvent->getX() << " y=" << mouseEvent->getY()
			<< std::endl;
	});
	queue.appendListener(EventType::keyboard, [](const EventPointer & event) {
		const KeyboardEvent * keyboardEvent = static_cast<const KeyboardEvent *>(event.get());
		std::cout << "Received keyboard event, key=" << (char)keyboardEvent->getKey()
			<< std::endl;
	});
	queue.appendListener(EventType::input, [](const EventPointer & event) {
		std::cout << "Received input event, ";
		if(event->getType() == EventType::mouse) {
			const MouseEvent * mouseEvent = static_cast<const MouseEvent *>(event.get());
			std::cout << "it's mouse event, x=" << mouseEvent->getX() << " y=" << mouseEvent->getY()
				<< std::endl;
		}
		else if(event->getType() == EventType::keyboard) {
			const KeyboardEvent * keyboardEvent = static_cast<const KeyboardEvent *>(event.get());
			std::cout << "it's keyboard event, key=" << (char)keyboardEvent->getKey() << std::endl;
		}
		else {
			std::cout << "it's an event that I don't understand." << std::endl;
		}
	});
	queue.appendListener(EventType::changed, [](const EventPointer & event) {
		const ChangedEvent * changedEvent = static_cast<const ChangedEvent *>(event.get());
		std::cout << "Received changed event, text=" << changedEvent->getText() << std::endl;
	});

	// Asynchronous API, put events in to the event queue.
	queue.enqueue(std::make_shared<MouseEvent>(123, 567));
	queue.enqueue(std::make_shared<KeyboardEvent>('W'));
	queue.enqueue(std::make_shared<ChangedEvent>("This is new text"));
	queue.enqueue(EventType::input, std::make_shared<MouseEvent>(10, 20));
	// then process all events.
	queue.process();

	// Synchronous API, dispatch events to the listeners directly.
	queue.dispatch(std::make_shared<KeyboardEvent>('Q'));
	queue.dispatch(EventType::input, std::make_shared<ChangedEvent>("Should not display"));
}


} // namespace
