# Overview of heterogeneous classes
<!--begintoc-->
## Table Of Contents

* [Description](#a2_1)
* [Warning](#a2_2)
* [Usage](#a2_3)
  * [Headers](#a3_1)
  * [Template parameters](#a3_2)
* [Differences between heterogeneous classes vs homogeneous classes](#a2_4)
<!--endtoc-->

<a id="a2_1"></a>
## Description

'CallbackList', 'EventDispatcher', and 'EventQueue' are homogeneous. All listeners must have the same prototype. For example,

```c++
eventpp::EventDispatcher<int, void ()> dispatcher;
dispatcher.appendListener(3, []() {}); // OK
dispatcher.appendListener(3, [](std::string) {}); // wrong, can't listen for void(std::string)
```

There are heterogeneous counterparts, 'HeterCallbackList', 'HeterEventDispatcher', and 'HeterEventQueue'. These classes allow listeners having different prototypes. For example,

```c++
eventpp::HeterEventDispatcher<int, eventpp::HeterTuple<void (), void (std::string)> > dispatcher;
dispatcher.appendListener(3, []() {}); // OK
dispatcher.appendListener(3, [](std::string) {}); // OK
```

<a id="a2_2"></a>
## Warning

The heterogeneous classes are mostly for proof of concept purpose. Misusing them most likely means your application design has flaws.
You should stick with the homogeneous classes, even though sometimes the heterogeneous classes look convenient (but with overhead).  
The heterogeneous classes may be not well maintained or supported in the future. You use them on your own risk.

<a id="a2_3"></a>
## Usage

<a id="a3_1"></a>
### Headers

eventpp/hetercallbacklist.h  
eventpp/hetereventdispatcher.h  
eventpp/hetereventqueue.h  

<a id="a3_2"></a>
### Template parameters

```c++
template <
    typename PrototypeList,
    typename Policies = DefaultPolicies
>
class HeterCallbackList;

template <
    typename Event,
    typename PrototypeList,
    typename Policies = DefaultPolicies
>
class HeterEventDispatcher;

template <
    typename Event,
    typename PrototypeList,
    typename Policies = DefaultPolicies
>
class HeterEventQueue;
```

For comparison, below are the template parameters for the homogeneous counterparts

```c++
template <
    typename Prototype,
    typename Policies = DefaultPolicies
>
class CallbackList;

template <
    typename Event,
    typename Prototype,
    typename Policies = DefaultPolicies
>
class EventDispatcher;

template <
    typename Event,
    typename Prototype,
    typename Policies = DefaultPolicies
>
class EventQueue;
```

The only difference is the `Prototype` in homo-classes becomes `PrototypeList` in heter-classes.  
In the homo-classes, `Prototype` is a single function type such as `void ()`.  
In the heter-classes, `PrototypeList` is a list of function types in `eventpp::HeterTuple`, such as `eventpp::HeterTuple<void (), void (std::string), void (int, int)>`.  
Note: Ideally it would be better to use `std::tuple` instead of `eventpp::HeterTuple`, but the problem is that the tuple is instantiated in HeterEventDispatcher which cause compile error that function type can't be instantiated.

<a id="a2_4"></a>
## Differences between heterogeneous classes vs homogeneous classes

1. Heterogeneous classes has both overhead on performance and memory usage. Usually event system is the core component in an application, the performance is critical.  
2. Heterogeneous classes can't have the same API interface as homogeneous classes, because some APIs are impossible or very difficult to implement in heterogeneous classes.  
3. Heterogeneous classes doesn't support eventpp::ArgumentPassingAutoDetect. That means the event in the argument can't be detected automatically.  
