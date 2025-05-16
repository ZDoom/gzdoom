# Class ScopedRemover reference

<!--begintoc-->
## Table Of Contents

* [Description](#a2_1)
* [API reference](#a2_2)
  * [Header](#a3_1)
  * [Template parameters](#a3_2)
  * [Member functions](#a3_3)
  * [Sample code](#a3_4)
  * [Automatic disconnection](#a3_5)
<!--endtoc-->

<a id="a2_1"></a>
## Description

ScopedRemover is a utility class that automatically removes listeners when ScopedRemover object goes out of scope.  

<a id="a2_2"></a>
## API reference

<a id="a3_1"></a>
### Header

eventpp/utilities/scopedremover.h

<a id="a3_2"></a>
### Template parameters

```c++
template <typename DispatcherType>
class ScopedRemover;
```

`DispatcherType` can be CallbackList, EventDispatcher, EventQueue, HeterCallbackList, HeterEventDispatcher, or HeterEventQueue.

<a id="a3_3"></a>
### Member functions

```c++
// for EventDispatcher, EventQueue, HeterEventDispatcher, or HeterEventQueue
explicit ScopedRemover(DispatcherType & dispatcher);
// for CallbackList or HeterCallbackList
explicit ScopedRemover(CallbackListType & callbackList);

ScopedRemover(ScopedRemover && other) noexcept;
ScopedRemover & operator = (ScopedRemover && other) noexcept;
void swap(ScopedRemover & other) noexcept;
```

ScopedRemover can be moved, move assigned and swapped, but can't be copied or assigned.

**Member functions for EventDispatcher and EventQueue**
```c++
void reset();

void setDispatcher(DispatcherType & dispatcher);

typename DispatcherType::Handle appendListener(
        const typename DispatcherType::Event & event,
        const typename DispatcherType::Callback & listener
    );

typename DispatcherType::Handle prependListener(
        const typename DispatcherType::Event & event,
        const typename DispatcherType::Callback & listener
    );

typename DispatcherType::Handle insertListener(
        const typename DispatcherType::Event & event,
        const typename DispatcherType::Callback & listener,
        const typename DispatcherType::Handle & before
    );
bool removeListener(
        const typename DispatcherType::Event & event,
        const typename DispatcherType::Handle handle
    );
```

**Member functions for CallbackList**
```c++
void reset();

void setCallbackList(CallbackListType & callbackList);

typename CallbackListType::Handle append(
        const typename CallbackListType::Callback & callback
    );

typename CallbackListType::Handle prepend(
        const typename CallbackListType::Callback & callback
    );

typename CallbackListType::Handle insert(
        const typename CallbackListType::Callback & callback,
        const typename CallbackListType::Handle & before
    );
bool remove(const typename CallbackListType::Handle handle);
```

The function `reset()` removes all listeners which added by ScopedRemover from the dispatcher or callback list, as if the ScopedRemover object has gone out of scope.  

The functions `setDispatcher()` and `setCallbackList` sets the dispatcher or callback list, and reset the ScopedRemover object.  

The functions `removeListener` and `remove` remove the listener, similar to the same name functions in the underlying class (CallbackList, EventDispatcher, or EventQueue). They are useful to remove the listeners without destroying the ScopedRemover object. The functions return `true` if the listener is removed successfully, `false` if the listener is not found.  

The other member functions that have the same names with the corresponding underlying class (CallbackList, EventDispatcher, or EventQueue). Those functions add listener to the dispatcher.  

<a id="a3_4"></a>
### Sample code

```c++
#include "eventpp/utilities/scopedremover.h"
#include "eventpp/eventdispatcher.h"

eventpp::EventDispatcher<int, void ()> dispatcher;
using Remover = eventpp::ScopedRemover<eventpp::EventDispatcher<int, void ()> >;
constexpr int event = 3;

dispatcher.appendListener(event, [&dataList]() {
    // listener A
});

{
    Remover r1(dispatcher);
    r1.prependListener(event, [&dataList]() {
        // listener B
    });
    {
        Remover r2(dispatcher);
        auto handle = r2.appendListener(event, [&dataList]() {
            // listener C
        });
        {
            Remover r3(dispatcher);
            r3.insertListener(event, [&dataList]() {
                // listener D
            }, handle);

            dispatcher.dispatch(event);
            // All listeners were triggered.
        }
        
        // listener D was removed

        dispatcher.dispatch(event);
        // Listeners A, B, C were triggered.
    }

    // listener C was removed

    dispatcher.dispatch(event);
    // Listeners A, B were triggered.
}

// listener B was removed

dispatcher.dispatch(event);
// Listeners A were triggered.

```

<a id="a3_5"></a>
### Automatic disconnection

ScopedRemover can be used to auto disconnect listeners when the object involved in the listeners is destroyed. For example, pseudo code,  

**Code without ScopedRemover**  

```c++
SomeDispatcher someDispatcher;

class MyClass
{
    MyClass()
    {
        someDispatcher.appendListener(SomeEvent, callback of myListener);
    }
    
    void myListener() {}
};
```

In above code, when the object of MyClass is destroyed and `myListener` is not removed from `someDispatcher`, when `someDispatcher` triggers `SomeEvent`, it will invoke on dangling pointer and the program will crash on segment fault.

**Code with ScopedRemover**  

```c++
SomeDispatcher someDispatcher;

class MyClass
{
    MyClass() : scopedRemover(someDispatcher)
    {
        scopedRemover.appendListener(SomeEvent, callback of myListener);
    }
    
    void myListener() {}

    eventpp::ScopedRemover<SomeDispatcher> scopedRemover;
};
```

In above code, when the object of MyClass is destroyed, `myListener` is automatically removed from `someDispatcher`, `someDispatcher` will not invoke on any dangling pointer.
