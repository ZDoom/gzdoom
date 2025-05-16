# EventDispatcher 使用教程

注意：如果想尝试运行教程代码，建议使用 `tests/unittest` 目录下的代码。本文中的示例代码可能已经过期而无法编译。

### 教程 1 基本用法

**代码**

```C++
// 命名空间为 eventpp
// 第一个模板参数 int 是事件类型。事件类型可以是其他数据类型的，如 std::string，int 等
// 第二个参数是监听器的原型
eventpp::EventDispatcher<int, void ()> dispatcher;

// 添加一个监听器。这里的 3 和 5 是传给 dispatcher 的，用于标记自身的事件类型
// []() {} 是监听器。
// 监听器并不必须是 lambda，可以使任何满足原型要求的可调用对象，如函数、std::function等
dispatcher.appendListener(3, []() {
    std::cout << "Got event 3." << std::endl;
});
dispatcher.appendListener(5, []() {
    std::cout << "Got event 5." << std::endl;
});
dispatcher.appendListener(5, []() {
    std::cout << "Got another event 5." << std::endl;
});

// 分发事件。第一个参数是事件类型。
dispatcher.dispatch(3);
dispatcher.dispatch(5);
```

**输出**

> Got event 3.  
> Got event 5.  
> Got another event 5.

**解读**

首先定义一个分发器

```c++
eventpp::EventDispatcher<int, void ()> dispatcher;
```

EventDispatcher 类接收两个模板参数。第一个是*事件类型*，此处是 `int` 。第二个是监听器的*原型*。  
*事件类型* 必须能够用作 `std::map` 的 key。也就是说该类型必须支持 `operator <`。  
*原型* 是 C++ 函数类型，例如 `void (int)`, `void (const std::string &, const MyClass &, int, bool)`

然后添加一个监听器

```c++
dispatcher.appendListener(3, []() {
	std::cout << "Got event 3." << std::endl;
});
```

`appendListener` 函数接收两个参数。第一个是 *事件类型* 的 *事件* （译注：此处的“事件类型”指的是用于区分事件的数据类型，此处为 int 。“事件”则是具体的时间值，此处为整数 3 ），此处为 `int` 类型。第二个参数是*回调函数*。  
回调函数可以是任何能够回调的目标——函数、函数指针、成员函数指针、lambda表达式、函数对象等。其必须能够被 `dispatcher` 中声明的 *原型* 调用。  
在上面这段代码的下面，我们还为 事件5 添加了两个监听器。

接下来，使用下面的代码分发事件

```c++
dispatcher.dispatch(3);
dispatcher.dispatch(5);
```

这里分发了两个事件，分别是事件 3 和 5 。  
在事件分发的过程中，所有对应事件的监听器都会按照它们被添加进 EventDispatcher 的顺序逐个执行。

### 教程 2 —— 带参数的监听器

**代码**

```c++
// 定义有两个参数的监听器原型
eventpp::EventDispatcher<int, void (const std::string &, const bool)> dispatcher;

dispatcher.appendListener(3, [](const std::string & s, const bool b) {
    std::cout << std::boolalpha << "Got event 3, s is " << s << " b is " << b << std::endl;
});
// 监听器的原型不需要和 dispatcher 完全一致，只要参数类型能够兼容即可
dispatcher.appendListener(5, [](std::string s, int b) {
    std::cout << std::boolalpha << "Got event 5, s is " << s << " b is " << b << std::endl;
});
dispatcher.appendListener(5, [](const std::string & s, const bool b) {
    std::cout << std::boolalpha << "Got another event 5, s is " << s << " b is " << b << std::endl;
});

// 分发事件。第一个参数是事件类型
dispatcher.dispatch(3, "Hello", true);
dispatcher.dispatch(5, "World", false);
```

**输出** 

> Got event 3, s is Hello b is true  
> Got event 5, s is World b is 0  
> Got another event 5, s is World b is false

**解读**

此处的 dispatcher 回调函数原型接收两个参数：`const std::string &` 和 `const bool`。  
监听器原型不需要和 dispatcher 完全一致，只要参数类型能够兼容即可。例如第二个监听器，`[](std::string s, int b)`，其原型和 dispatcher 并不相同

### 教程 3 —— 自定义事件结构

**代码**

```c++
// 定义一个能够保存所有参数的 Event
struct MyEvent {
    int type;
    std::string message;
    int param;
};

// 定义一个能让 dispatcher 知道如何展开事件类型的 policy
struct MyEventPolicies
{
    static int getEvent(const MyEvent & e, bool /*b*/) {
        return e.type
    }
};

// 将刚刚定义的 MyEventPolicies 用作 EventDispatcher 的第三个模板参数
// 注意：第一个模板参数是事件类型的类型 int ，并非 MyEvent
eventpp::EventDispatcher<
    int,
	void (const MyEvent &, bool),
	MyEventPolicies
> dispatcher;

// 添加一个监听器。注意，第一个参数是事件类型 int，并非 MyEvent
dispatcher.appendListener(3, [](const MyEvent & e, bool b) {
    std::cout
        << std::boolalpha
        << "Got event 3" << std::endl
        << "Event::type is " << e.type << std::endl
        << "Event::message is " << e.message << std::endl
        << "Event::param is " << e.param << std::endl
        << "b is " << b << std::endl
    ;
});

// 启动事件。第一个参数是 Event
dispatcher.dispatch(MyEvent { 3, "Hello world", 38 }, true);
```

**输出**

> Got event 3  
> Event::type is 3  
> Event::message is Hello world  
> Event::param is 38  
> b is true

**解读**

通常的方法是将 Event 类定义为基类，所有其他的事件都从 Event 派生，实际的事件类型则是 Event 的成员（就像 Qt 中的 QEvent ），通过 policy 来为 EventDispatcher 定义如何从 Event 类中获取真正需要的数据。
