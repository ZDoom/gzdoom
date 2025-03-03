# Class AnyData reference
<!--begintoc-->
## Table Of Contents

- [Class AnyData reference](#class-anydata-reference)
  - [Table Of Contents](#table-of-contents)
  - [Description](#description)
  - [Use AnyData](#use-anydata)
    - [Header](#header)
    - [Class AnyData template parameters](#class-anydata-template-parameters)
    - [Use AnyData in EventQueue](#use-anydata-in-eventqueue)
      - [get](#get)
      - [getAddress](#getaddress)
      - [isType](#istype)
  - [Global function](#global-function)
    - [maxSizeOf](#maxsizeof)
  - [Tutorial](#tutorial)
<!--endtoc-->

## Description

Class `AnyData` is a data structure that can hold any data types without any dynamic heap allocation, and `AnyData` can be passed to `EventQueue` in place of the data it holds. The purpose is to eliminate the heap allocation time which is commonly used as `std::shared_ptr`. `AnyData` can improve the performance by about 30%~50%, comparing to heap allocation with `std::shared_ptr`.  

`AnyData` is not type safe. Abusing it may lead you problems. If you understand how it works, and use it properly, you can gain performance improvement.  
`AnyData` should be used for extreme performance optimization, such as the core event system in a game engine. In a performance non-critical system such as desktop GUI, you may not need `AnyData`, otherwise, it's premature optimization.  
`AnyData` should be only used in `EventQueue`, it's not general purpose class, don't use it for other purpose.  

For example, assume we have the event class hierarchy (we will use these event classes in example code all over this document),

```c++
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
```

Without using `AnyData`, we need to use shared pointer to put an event into an `EventQueue`, for example,

```c++
using Queue = eventpp::EventQueue<EventType, void (const std::shared_ptr<Event> &)>;
Queue eventQueue;
eventQueue.enqueue(EventType::key, std::make_shared<KeyEvent>(123));
eventQueue.enqueue(EventType::mouse, std::make_shared<MouseEvent>(100, 200));
```

The problem of the shared pointer approach is that it needs dynamic heap allocation, and it is pretty slow.
One solution is to use a small object pool to reuse the allocated objects, another solution is `AnyData`.

Now with `AnyData`, we can eliminate the usage of shared pointer and heap allocation, for example,

```c++
using Queue = eventpp::EventQueue<EventType, void (const eventpp::AnyData<eventMaxSize> &)>;
Queue eventQueue;
eventQueue.enqueue(EventType::key, KeyEvent(123));
eventQueue.enqueue(EventType::mouse, MouseEvent(100, 200));
```

## Use AnyData

### Header

eventpp/utilities/anydata.h  

### Class AnyData template parameters

```c++
template <std::size_t maxSize>
class AnyData;
```

`AnyData` requires one constant template parameter. It's the max size of the underlying types. Any data types with any data size can be used to construct `AnyData`. If the data size is not larger than `maxSize`, the data is stored inside `AnyData`. If it's larger, the data is stored on the heap with dynamic allocation.  
`AnyData` uses at least `maxSize` bytes, even if the underlying data is only 1 byte long. So `AnyData` might use slightly more memory than the shared pointer solution, but also may not, because shared pointer solution has other memory overhead.  

### Use AnyData in EventQueue

`AnyData` can be used as the callback arguments in EventQueue. 

```c++
eventpp::EventQueue<EventType, void (const EventType, const eventpp::AnyData<eventMaxSize> &)> queue;
queue.appendListener(EventType::key, [](const Event & e) {
    std::cout << "Received KeyEvent, key=" << static_cast<const KeyEvent &>(e).getKey() << std::endl;
});
```

Since `AnyData` can convert to any types automatically, here the listener functions can receive `const Event &` instead of `AnyData`. When `EventQueue` passes `AnyData` to the listener, `AnyData` can cast to `const Event &` automatically.  
Even better, we can use the concrete type as the argument directly, for example,  
```c++
queue.appendListener(EventType::key, [](const KeyEvent & e) {
    std::cout << "Received KeyEvent, key=" << e.getKey() << std::endl;
});
```
Note such listener should only receive the specified type, here is `KeyEvent`. If it receives other data type, it will crash your program.

`AnyData` can convert to reference or pointer. When it converts to reference, the reference refers to the underlying data. When it converts to pointer, the pointer points to the address of the underlying data. The special conversion of pointer allow we use unrelated data types as event arguments and receive the argument as `const void *`. For example,  

```c++
eventpp::EventQueue<EventType, void (const EventType, const eventpp::AnyData<eventMaxSize> &)> queue;
queue.appendListener(EventType::key, [](const EventType type, const void * e) {
    assert(type == EventType::key);
    std::cout << "Received KeyEvent, key=" << static_cast<const KeyEvent *>(e)->getKey() << std::endl;
});
queue.appendListener(EventType::text, [](const EventType type, const void * e) {
    assert(type == EventType::text);
    std::cout << "Received text event, text=" << *static_cast<const std::string *>(e) << std::endl;
});
queue.enqueue(EventType::key, KeyEvent(255));
queue.enqueue(EventType::text, std::string("This is a text"));
queue.process();
```

The listener argument can also be `AnyData` directly, for example,

```c++
queue.appendListener(EventType::key, [](const EventType type, const eventpp::AnyData<eventMaxSize> & e) {
    std::cout << "Received KeyEvent, key=" << e.get<KeyEvent>().getKey() << std::endl;
});
```

This is really bad design because then your code is coupled with `eventpp` tightly. I highly recommend you not to do so. But if you do want to do it, here are the `AnyData` member functions which helps you to use it.  

#### get

```c++
template <typename T>
const T & get() const;
```

Return a reference to the underlying data as type `T`. `AnyData` doesn't check if the underlying data is of type `T`, it simply returns a reference to the underlying data, so it's not type safe.

#### getAddress

```c++
const void * getAddress() const;
```

Return a pointer to the address of the underlying data.

#### isType

```c++
template <typename T>
bool isType() const;
```

Return true if the underlying data type is `T`, false if not.  
This function compares the exactly types, it doesn't check any class hierarchy. For example, if an `AnyData` holds `KeyEvent`, then `isType<KeyEvent>()` will return true, but `isType<Event>()` will return false.  

## Global function

### maxSizeOf

```c++
template <typename ...Ts>
constexpr std::size_t maxSizeOf();
```

Return the maximum size of types Ts... For example,  
```c++
maxSizeOf<KeyEvent, MouseEvent, int, double>();
```

## Tutorial

Below is the tutorial code. The complete code can be found in `tests/tutorial/tutorial_anydata.cpp`  

```c++
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
```
