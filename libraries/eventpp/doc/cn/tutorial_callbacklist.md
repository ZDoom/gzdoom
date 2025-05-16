# CallbackList 使用教程

注意：如果想尝试运行教程代码，建议使用 `tests/unittest` 目录下的代码。本文中的示例代码可能已经过期而无法编译。

### CallbackList 教程 1, 基础

**代码**

```c++
// 命名空间是 eventpp
// 首个参数是监听器的原型
eventpp::CallbackList<void ()> callbackList;

// 添加一个回调函数，此处即 [](){} 。回调函数并非一定要是 lambda 表达式。
// 函数、std::function 或其他任何满足监听器原型要求的函数对象都可以作为监听器
callbackList.append([](){
    std::cout << "Got callback 1." << std::endl;
});
callbackList.append([](){
    std::cout << "Got callback 2." << std::endl;
});

// 启动回调列表
callbackList();
```

**输出**

> Got callback 1.  
> Got callback 2.

**解读**

首先，定义一个回调列表（ callback list ）

```c++
eventpp::CallbackList<void ()> callbackList;
```

CallbackList 需要至少一个模板参数，作为回调函数的“原型”（ prototype ）。  
“原型”指 C++ 函数类型，例如 `void (int)`, `void (const std::string &, const MyClass &, int, bool)`

然后，添加一个回调函数

```c++
callbackList.append([]() {
    std::cout << "Got callback 1." << std::endl;
});
```

`append` 函数接收一个回调函数作为参数。  
回调函数可以使任何回调目标——函数、函数指针、指向成员函数的指针、lambda 表达式、函数对象等。该回调函数必须可以使用 `callbackList` 中声明的原型调用。

接下来启动回调列表

```c++
callbackList();
```

在回调列表启动执行的过程中，所有回调函数都会按照被加入列表时的顺序执行。

### CallbackList 教程 2, 带参数的回调函数

**代码**

```c++
// 下面这个 CallbackList 的回调函数原型有两个参数
eventpp::CallbackList<void (const std::string &, const bool)> callbackList;

callbackList.append([](const std::string & s, const bool b) {
    std::cout<<std::boolalpha<<"Got callback 1, s is " << s << " b is " << b << std::endl;
});

// 回调函数原型不需要和回调函数列表完全一致。只要参数类型兼容即可
callbackList.append([](std::string s, int b) {
    std::cout<<std::boolalpha<<"Got callback 2, s is " << s << " b is " << b << std::endl;
});

// 启动回调列表
callbackList("Hello world", true);
```

**输出**

> Got callback 1, s is Hello world b is true  
> Got callback 2, s is Hello world b is 1

**解读**

本例中，回调函数列表的回调函数原型接收两个参数： `const std::string &` 和 `const bool`。  
回调函数的原型并不需要和回调完全一致，只要两个函数中的参数能够兼容即可。正如上面例子中的第二个回调函数，其参数为 `[](std::string s, int b)`，其原型与回调列表中的并不相同。

### CallbackList 教程 3, 移除

**代码**

```c++
using CL = eventpp::CallbackList<void ()>;
CL callbackList;

CL::Handle handle2;

// 加一些回调函数
callbackList.append([]() {
    std::cout << "Got callback 1." << std::endl;
});
handle2 = callbackList.append([]() {
    std::cout << "Got callback 2." << std::endl;
});
callbackList.append([]() {
    std::cout << "Got callback 3." << std::endl;
});

callbackList.remove(handler2);

// 启动回调列表。“Got callback 2.” 并不会被触发
callbackList();
```

**输出**

> Got callback 1.  
> Got callback 3.

### CallbackList 教程 4, for each

**代码**

```c++
using CL = eventpp::CallbackList<void ()>;
CL callbackList;

// 添加回调函数
callbackList.append([]() {
    std::cout << "Got callback 1." << std::endl;
});
callbackList.append([]() {
    std::cout << "Got callback 2." << std::endl;
});
callbackList.append([]() {
    std::cout << "Got callback 3." << std::endl;
});

// 下面调用 forEach 移除第二个回调函数
// forEach 回调函数的原型是
// void(const CallbackList::Handle & handle, const CallbackList::Callback & callback)
int index = 0;
callbackList.forEach([&callbackList, &index](const CL::Handle & handle, const CL::Callback & callback) {
    std::cout << "forEach(Handle, Callback), invoked " << index << std::endl;
    if(index == 1) {
        callbackList.remove(handle);
        std::cout << "forEach(Handle, Callback), removed second callback" << std::endl;
    }
    ++index;
});

// forEach 回调函数原型也可以是 void(const CallbackList::Callback & callback)
callbackList.forEach([&callbackList, &index](const CL::Callback & callback) {
    std::cout << "forEach(Callback), invoked" << std::endl;
});

// 启动回调列表。“Got callback 2.” 并不会被触发
callbackList();
```

**输出**

> forEach(Handle, Callback), invoked 0  
> forEach(Handle, Callback), invoked 1  
> forEach(Handle, Callback), removed second callback  
> forEach(Handle, Callback), invoked 2  
> forEach(Callback), invoked  
> forEach(Callback), invoked  
> Got callback 1.  
> Got callback 3.