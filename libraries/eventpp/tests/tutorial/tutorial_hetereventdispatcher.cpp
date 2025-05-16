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
#include "eventpp/hetereventdispatcher.h"

#include "tutorial.h"

#include <iostream>

namespace {

// This is the definition of event types
enum class EventType
{
	// for MouseEvent
	mouse,

	// for KeyboardEvent
	keyboard,
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

// We are going to dispatch event objects directly without specify the event type explicitly, so we need to let eventpp
// know how to get the event type from the event object. The event policy does that.
// Note ArgumentPassingMode must be eventpp::ArgumentPassingIncludeEvent here,
// the heterogeneous doesn't support eventpp::ArgumentPassingAutoDetect.
struct EventPolicy
{
	using ArgumentPassingMode = eventpp::ArgumentPassingIncludeEvent;

	template <typename E>
	static EventType getEvent(const E & event) {
		return event.getType();
	}
};

TEST_CASE("HeterEventDispatcher tutorial 1, basic")
{
	std::cout << std::endl << "HeterEventDispatcher tutorial 1, basic" << std::endl;

	// The namespace is eventpp
	// the second parameter is a HeterTuple of the listener prototypes.
	eventpp::HeterEventDispatcher<EventType, eventpp::HeterTuple<
			void (const MouseEvent &),
			void (const KeyboardEvent &)
		>,
		EventPolicy
	> dispatcher;

	dispatcher.appendListener(EventType::mouse, [](const MouseEvent & event) {
		std::cout << "Received mouse event, x=" << event.getX() << " y=" << event.getY() << std::endl;
	});
	dispatcher.appendListener(EventType::keyboard, [](const KeyboardEvent & event) {
		std::cout << "Received keyboard event, key=" << (char)event.getKey() << std::endl;
	});

	dispatcher.dispatch(MouseEvent(5, 38));
	dispatcher.dispatch(KeyboardEvent('W'));
}



} // namespace
