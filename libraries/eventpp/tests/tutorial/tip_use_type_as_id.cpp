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

// This tutorial code demonstrates how to use C++ data type as event identifier.
// The tutorial document is in https://github.com/wqking/eventpp/blob/master/doc/tip_use_type_as_id.md

// Include the headers
#include "eventpp/eventqueue.h"
#include "eventpp/utilities/anyid.h"
#include "eventpp/utilities/anydata.h"

#include <typeindex>
#include <typeinfo>
#include <type_traits>

// Follow headers are only for tutorial purpose
#include "tutorial.h"
#include <iostream>

namespace {

// We need a digester class for AnyId.
/*
// This is the C++17 version with cleaner code.
template <typename T>
struct TypeIndexDigesterCpp17
{
	std::type_index operator() (const T & typeInfo) const
	{
		if constexpr (std::is_same<T, std::type_info>::value) {
			return std::type_index(typeInfo);
		}
		else {
			return std::type_index(typeid(T));
		}
	}

};
*/

// This is the C++11 version with SFINAE.
// Basically we need to support two overloaded operator().
// The first one is `std::type_index operator() (const T &) const`,
// it's to get std::type_index from typeid(T) where T is a general type.
// The second one is `std::type_index operator() (const std::type_info &) const`,
// we don't need to apply typeid on the type_info.
template <typename T>
struct TypeIndexDigesterCpp11
{
	template <typename U>
	auto operator() (const U &) const
		-> typename std::enable_if<! std::is_same<U, std::type_info>::value, std::type_index>::type
	{
		return std::type_index(typeid(T));
	}

	template <typename U>
	auto operator() (const U & typeInfo) const
		-> typename std::enable_if<std::is_same<U, std::type_info>::value, std::type_index>::type
	{
		return std::type_index(typeInfo);
	}
};

// Define the maxSize parameter used in AnyData. 128 is an arbitrary hard coded size.
// You may want to calculate the max size of your event struct, such as,
// constexpr std::size_t eventMaxSize = eventpp::maxSizeOf<KeyEvent, MouseEvent, DrawText, Animal>();
// See document for AnyData for more information.
constexpr std::size_t eventMaxSize = 128;
// Now let's define the event queue
using TypeBasedEventQueue = eventpp::EventQueue<
	eventpp::AnyId<TypeIndexDigesterCpp11>,
	void(const eventpp::AnyData<eventMaxSize> &)
>;

// Note AnyData is not type safe, that means
// queue.appendListener(typeid(MouseEvent), [](const KeyEvent & event) {});
// will compile but the listener will receive wrong data and crash.
// To ensure type safety, we may introduce an auxiliary function `safeAppendListener`.
template <typename Event, typename Queue, typename Callback>
void safeAppendListener(Queue & queue, const Callback & callback)
{
	// In C++17, we can use std::is_invocable to check if we can invoke callback(Event()).
	// Here to be compatible with C++11, we use lambda to let the compiler perform the check.
	queue.appendListener(typeid(Event), [callback](const Event & event) {
		callback(event);
	});
}

// We can use any C++ type as the event, not only class, but also enum.
struct KeyEvent { int key; };
struct MouseEvent { int x; int y; };
struct DrawText { std::string text; };
enum class Animal { dog, cat };

TEST_CASE("Tip: Use C++ type as event identifier")
{
	std::cout << std::endl << "Tip: Use C++ type as event identifier" << std::endl;

	TypeBasedEventQueue queue;

	// Append a listener, here we use an object of KeyEvent as the event identifier.
	// This calls the overload `std::type_index operator() (const T &) const` in TypeIndexDigester.
	queue.appendListener(KeyEvent{}, [](const KeyEvent & event) {
		std::cout << "Received KeyEvent, key=" << event.key << std::endl;
	});

	// You may not want to create an object only for an identifier. So here we use typeid.
	// This calls the overload `std::type_index operator() (const std::type_info &) const` in TypeIndexDigester.
	queue.appendListener(typeid(MouseEvent), [](const MouseEvent & event) {
		std::cout << "Received MouseEvent, x=" << event.x << " y=" << event.y << std::endl;
	});
	queue.appendListener(typeid(DrawText), [](const DrawText & event) {
		std::cout << "Received DrawText, text=" << event.text << std::endl;
	});
	// In above code, wrong event type may compile fine but crash your program, for example,
	// queue.appendListener(typeid(DrawText), [](const KeyEvent & event) {});
	
	// safeAppendListener is a better way to append listener. Following won't compile,
	// safeAppendListener<DrawText>(queue, [](const Animal & event) {});
	safeAppendListener<Animal>(queue, [](const Animal & event) {
		std::cout << "Received Animal, the animal is " << (event == Animal::dog ? "dog" : "cat") << std::endl;
	});
	// We can even use primary data type as event.
	safeAppendListener<int>(queue, [](const int event) {
		std::cout << "Received int, the value is " << event << std::endl;
	});
	safeAppendListener<long>(queue, [](const long event) {
		std::cout << "Received long, the value is " << event << std::endl;
	});

	// We have introduced three methods to append a listener, it's for demonstration.
	// In the production code, we should only use one method and be consistent.

	queue.enqueue(KeyEvent{ 9 });
	queue.enqueue(KeyEvent{ 32 });
	queue.enqueue(MouseEvent{ 1024, 768 });
	queue.enqueue(DrawText{ "Hello" });
	queue.enqueue(Animal::dog);
	queue.enqueue(Animal::cat);
	queue.enqueue(3);
	queue.enqueue(5L);
	// This won't trigger any listener since there is no listener for long long.
	queue.enqueue(8LL);

	queue.process();
}

}
