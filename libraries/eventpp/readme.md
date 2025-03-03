# eventpp -- C++ library for event dispatcher and callback list

- [eventpp -- C++ library for event dispatcher and callback list](#eventpp----c-library-for-event-dispatcher-and-callback-list)
    - [Facts and features](#facts-and-features)
    - [License](#license)
    - [Version 0.1.3](#version-013)
    - [Source code](#source-code)
    - [Supported compilers](#supported-compilers)
    - [C++ standard requirements](#c-standard-requirements)
    - [Quick start](#quick-start)
        - [Namespace](#namespace)
        - [Use eventpp in your project](#use-eventpp-in-your-project)
        - [Using CallbackList](#using-callbacklist)
        - [Using EventDispatcher](#using-eventdispatcher)
        - [Using EventQueue](#using-eventqueue)
    - [Documentations](#documentations)
    - [Motivations](#motivations)
    - [Change log](#change-log)
    - [Contributors](#contributors)

eventpp is a C++ event library for callbacks, event dispatcher, and event queue. With eventpp you can easily implement signal and slot mechanism, publisher and subscriber pattern, or observer pattern.  
eventpp is mature and production-ready.

![C++](https://img.shields.io/badge/C%2B%2B-11-blue)
![Compilers](https://img.shields.io/badge/Compilers-GCC%2FMSVC%2FClang%2FIntel-blue)
![License](https://img.shields.io/badge/License-Apache--2.0-blue)
![CI](https://github.com/wqking/eventpp/workflows/CI/badge.svg)
[![Vcpkg port](https://img.shields.io/vcpkg/v/eventpp)](https://vcpkg.link/ports/eventpp)
[![Conan Center](https://img.shields.io/conan/v/eventpp)](https://conan.io/center/recipes/eventpp)

## Facts and features

- **Powerful**
    - Supports synchronous event dispatching and asynchronous event queue.
    - Configurable and extensible with policies and mixins.
    - Supports event filter via mixins.
- **Robust**
    - Supports nested event. During the process of handling an event, a listener can safely dispatch event and append/prepend/insert/remove other listeners.
    - Thread safety. Supports multi-threading.
    - Exception safety. Most operations guarantee strong exception safety.
    - Well tested. Backed by unit tests.
- **Fast**
    - The EventQueue can process 10M events in 1 second (10K events per millisecond).
    - The CallbackList can invoke 100M callbacks in 1 second (100K callbacks per millisecond).
    - The CallbackList can add/remove 5M callbacks in 1 second (5K callbacks per millisecond).
    - With the helper class AnyData, it's possible to avoid heap allocation when sending events via EventQueue.
- **Flexible and easy to use**
    - Listeners and events can be of any type and do not need to be inherited from any base class.
    - Utilities that can ease the usage, such as auto disconnecting, one shot listener, argument type adapter, etc.
    - Header only, no source file, no need to build. Does not depend on other libraries.
    - Only requires C++ 11.
    - Written in portable and standard C++, no hacks or quirks.

## License

Apache License, Version 2.0  

## Version 0.1.3

`eventpp` package is available in C++ package managers Vcpkg, Conan, Hunter, and Homebrew.  
The master branch is usable and stable.  
Don't worry about the large time span between commits and releases. The library is actively maintained.  
The master branch is currently fully back compatible with the first version. So your project won't get any back compatible issues.  
If you find any back compatible issue which is not announced, please report a bug.

## Source code

[https://github.com/wqking/eventpp](https://github.com/wqking/eventpp)

## Supported compilers

Tested with MSVC 2022 and 2019, MinGW (Msys) GCC 7.2, Ubuntu GCC 5.4, Intel C++ 2022, and MacOS GCC.
GCC 4.8.3 can compile the library, but I don't support or maintain for GCC prior to GCC 5.
In brief, MSVC, GCC, Clang that has well support for C++11, or released after 2019, should be able to compile the library.

## C++ standard requirements
* To Use the library  
    * The library: C++11.  
* To develop the library
    * Unit tests: C++17.
    * Tutorials: C++11.
    * Benchmarks: C++11.

## Quick start

### Namespace

`eventpp`

### Use eventpp in your project

`eventpp` package is available in C++ package managers Vcpkg, Conan, and Hunter. There are various methods to use eventpp.  
Please [read the document](doc/install.md) for details.

### Using CallbackList
```c++
#include "eventpp/callbacklist.h"
eventpp::CallbackList<void (const std::string &, const bool)> callbackList;
callbackList.append([](const std::string & s, const bool b) {
    std::cout << std::boolalpha << "Got callback 1, s is " << s << " b is " << b << std::endl;
});
callbackList.append([](std::string s, int b) {
    std::cout << std::boolalpha << "Got callback 2, s is " << s << " b is " << b << std::endl;
});
callbackList("Hello world", true);
```

### Using EventDispatcher
```c++
#include "eventpp/eventdispatcher.h"
eventpp::EventDispatcher<int, void ()> dispatcher;
dispatcher.appendListener(3, []() {
    std::cout << "Got event 3." << std::endl;
});
dispatcher.appendListener(5, []() {
    std::cout << "Got event 5." << std::endl;
});
dispatcher.appendListener(5, []() {
    std::cout << "Got another event 5." << std::endl;
});
// dispatch event 3
dispatcher.dispatch(3);
// dispatch event 5
dispatcher.dispatch(5);
```

### Using EventQueue
```c++
#include "eventpp/eventqueue.h"
eventpp::EventQueue<int, void (const std::string &, const bool)> queue;

queue.appendListener(3, [](const std::string s, bool b) {
    std::cout << std::boolalpha << "Got event 3, s is " << s << " b is " << b << std::endl;
});
queue.appendListener(5, [](const std::string s, bool b) {
    std::cout << std::boolalpha << "Got event 5, s is " << s << " b is " << b << std::endl;
});

// The listeners are not triggered during enqueue.
queue.enqueue(3, "Hello", true);
queue.enqueue(5, "World", false);

// Process the event queue, dispatch all queued events.
queue.process();
```

## Documentations

* Setup
    * [Install and use eventpp in your project](doc/install.md)
* Core classes and functions
    * [Overview](doc/introduction.md)
    * [Tutorials of CallbackList](doc/tutorial_callbacklist.md)
    * [Tutorials of EventDispatcher](doc/tutorial_eventdispatcher.md)
    * [Tutorials of EventQueue](doc/tutorial_eventqueue.md)
    * [Class CallbackList reference](doc/callbacklist.md)
    * [Class EventDispatcher reference](doc/eventdispatcher.md)
    * [Class EventQueue reference](doc/eventqueue.md)
    * [Policies -- configure eventpp](doc/policies.md)
    * [Mixins -- extend eventpp](doc/mixins.md)
* Utilities
    * [Utility class AnyData -- zero heap allocation event data in EventQueue](doc/anydata.md)
    * [Utility argumentAdapter -- adapt pass-in argument types to the types of the functioning being called](doc/argumentadapter.md)
    * [Utility conditionalFunctor -- pre-check the condition before calling a function](doc/conditionalfunctor.md)
    * [Utility class CounterRemover -- auto remove listeners after triggered certain times](doc/counterremover.md)
    * [Utility class ConditionalRemover -- auto remove listeners when certain condition is satisfied](doc/conditionalremover.md)
    * [Utility class ScopedRemover -- auto remove listeners when out of scope](doc/scopedremover.md)
    * [Utility class OrderedQueueList -- make EventQueue ordered](doc/orderedqueuelist.md)
    * [Utility class AnyId -- use various data types as EventType in EventDispatcher and EventQueue](doc/anyid.md)
    * [Utility header eventmaker.h -- auto generate event classes](doc/eventmaker.md)
    * [Document of utilities functions](doc/eventutil.md)
* Miscellaneous
    * [Build eventpp for development](doc/build_for_development.md)
    * [Performance benchmarks](doc/benchmark.md)
    * [FAQs, tricks, and tips](doc/faq.md)
* Tips and tricks
    * [Samples for typical use cases](doc/tip_sample_use_cases.md)
    * [Use C++ data type as event identifier](doc/tip_use_type_as_id.md)
* Heterogeneous classes and functions, for proof of concept, usually you don't need them
    * [Overview of heterogeneous classes](doc/heterogeneous.md)
    * [Class HeterCallbackList](doc/hetercallbacklist.md)
    * [Class HeterEventDispatcher](doc/hetereventdispatcher.md)
    * [Class HeterEventQueue](doc/hetereventqueue.md)
* Translated documents
    * [Chinese version 中文版](doc/cn/readme.md), thanks @marsCatXdu for the translation.

## Motivations

I (wqking) am a big fan of observer pattern (publish/subscribe pattern), and I used this pattern extensively in my code. I either used GCallbackList in my [cpgf library](https://github.com/cpgf/cpgf) which is too simple and unsafe (not support multi-threading or nested events), or repeated coding event dispatching mechanism such as I did in my [Gincu game engine](https://github.com/wqking/gincu) (the latest version has be rewritten to use eventpp). Both methods are not fun nor robust.  
Thanking to C++11, now it's quite easy to write a reusable event library with beautiful syntax (it's a nightmare to simulate the variadic template in C++03), so here is `eventpp`.

## Change log

**Version 0.1.3**  Sep 21, 2023  
Added utility class AnyData.  
Small bugs fixes and improvements.  
  
**Version 0.1.2**  Mar 11, 2022  
Bug fix.  
Added more unit tests.  
Added utilities argumentAdapter and conditionalFunctor.  
Added utilities AnyId.  
Added event maker macros.  
  
**Version 0.1.1**  Dec 13, 2019  
Added HeterCallbackList, HeterEventDispatcher, and HeterEventQueue.

**Version 0.1.0**  Sep 1, 2018  
First version.  
Added CallbackList, EventDispatcher, EventQueue, CounterRemover, ConditionalRemover, ScopedRemover, and utilities.

## Contributors

<table>
<tr>
<td align="center"><a href="https://github.com/wqking/"><img alt="wqking" src="https://github.com/wqking.png?s=100" width="100px;" /></a><span>wqking</span></td>
<td align="center"><a href="https://github.com/rotolof/"><img alt="rotolof" src="https://github.com/rotolof.png?s=100" width="100px;" /></a><span>rotolof</span></td>
<td align="center"><a href="https://github.com/OlivierLDff/"><img alt="OlivierLDff" src="https://github.com/OlivierLDff.png?s=100" width="100px;" /></a><span>OlivierLDff</span></td>
<td align="center"><a href="https://github.com/devbharat/"><img alt="devbharat" src="https://github.com/devbharat.png?s=100" width="100px;" /></a><span>devbharat</span></td>
<td align="center"><a href="https://github.com/mpiccolino-tealblue/"><img alt="mpiccolino-tealblue" src="https://github.com/mpiccolino-tealblue.png?s=100" width="100px;" /></a><span>mpiccolino-tealblue</span></td>
<td align="center"><a href="https://github.com/bazfp/"><img alt="bazfp" src="https://github.com/bazfp.png?s=100" width="100px;" /></a><span>bazfp</span></td>
<td align="center"><a href="https://github.com/Martinii89/"><img alt="Martinii89" src="https://github.com/Martinii89.png?s=100" width="100px;" /></a><span>Martinii89</span></td>
</tr>
<tr>
<td align="center"><a href="https://github.com/ludekvodicka/"><img alt="ludekvodicka" src="https://github.com/ludekvodicka.png?s=100" width="100px;" /></a><span>ludekvodicka</span></td>
<td align="center"><a href="https://github.com/Volatus/"><img alt="Volatus" src="https://github.com/Volatus.png?s=100" width="100px;" /></a><span>Volatus</span></td>
<td align="center"><a href="https://github.com/wallel/"><img alt="wallel" src="https://github.com/wallel.png?s=100" width="100px;" /></a><span>wallel</span></td>
<td align="center"><a href="https://github.com/mangrooveforest/"><img alt="mangrooveforest" src="https://github.com/mangrooveforest.png?s=100" width="100px;" /></a><span>mangrooveforest</span></td>
<td align="center"><a href="https://github.com/haberturdeur/"><img alt="haberturdeur" src="https://github.com/haberturdeur.png?s=100" width="100px;" /></a><span>haberturdeur</span></td>
<td align="center"><a href="https://github.com/shellinspector/"><img alt="shellinspector" src="https://github.com/shellinspector.png?s=100" width="100px;" /></a><span>shellinspector</span></td>
<td align="center"><a href="https://github.com/bd1es/"><img alt="bd1es" src="https://github.com/bd1es.png?s=100" width="100px;" /></a><span>bd1es</span></td>
</tr>
<tr>
<td align="center"><a href="https://github.com/qgyhd1234/"><img alt="qgyhd1234" src="https://github.com/qgyhd1234.png?s=100" width="100px;" /></a><span>qgyhd1234</span></td>
<td align="center"><a href="https://github.com/frostius/"><img alt="frostius" src="https://github.com/frostius.png?s=100" width="100px;" /></a><span>frostius</span></td>
<td align="center"><a href="https://github.com/gelijian/"><img alt="gelijian" src="https://github.com/gelijian.png?s=100" width="100px;" /></a><span>gelijian</span></td>
<td align="center"><a href="https://github.com/MH2033/"><img alt="MH2033" src="https://github.com/MH2033.png?s=100" width="100px;" /></a><span>MH2033</span></td>
<td align="center"><a href="https://github.com/zhllxt/"><img alt="zhllxt" src="https://github.com/zhllxt.png?s=100" width="100px;" /></a><span>zhllxt</span></td>
<td align="center"><a href="https://github.com/marsCatXdu/"><img alt="marsCatXdu" src="https://github.com/marsCatXdu.png?s=100" width="100px;" /></a><span>marsCatXdu</span></td>
<td align="center"><a href="https://github.com/Chaojimengnan/"><img alt="Chaojimengnan" src="https://github.com/Chaojimengnan.png?s=100" width="100px;" /></a><span>Chaojimengnan</span></td>
</tr>
<tr>
<td align="center"><a href="https://github.com/henaiguo/"><img alt="henaiguo" src="https://github.com/henaiguo.png?s=100" width="100px;" /></a><span>henaiguo</span></td>
<td align="center"><a href="https://github.com/sr-tream/"><img alt="sr-tream" src="https://github.com/sr-tream.png?s=100" width="100px;" /></a><span>sr-tream</span></td>
<td align="center"><a href="https://github.com/Drise13/"><img alt="Drise13" src="https://github.com/Drise13.png?s=100" width="100px;" /></a><span>Drise13</span></td>
<td align="center"><a href="https://github.com/iamzone/"><img alt="iamzone" src="https://github.com/iamzone.png?s=100" width="100px;" /></a><span>iamzone</span></td>
<td align="center"><a href="https://github.com/RazielXYZ/"><img alt="RazielXYZ" src="https://github.com/RazielXYZ.png?s=100" width="100px;" /></a><span>RazielXYZ</span></td>
</tr>
</table>

I (wqking) would like to sincerely thank all participants for the contributions. Your contributions make `eventpp` better and bright future.
I maintain the contributors list manually, according to the criteria below,

1. Your Pull Request is approved and merged to any branch.
2. Or there is commit or work related to your reported issue. That's to say, the bug you reported is fixed, or the advice you suggested is adopted.

If you  think you should be on the contributors list, such as I miss out your work, or you have work not technology related but can help
`eventpp` growing (such as posting review in social network with large amount of readers), please contact me, I'm happy to get the contributors
list larger.
