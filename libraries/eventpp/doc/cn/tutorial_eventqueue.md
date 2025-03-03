# EventQueue 使用教程

注意：如果想尝试运行教程代码，建议使用 `tests/unittest` 目录下的代码。本文中的示例代码可能已经过期而无法编译。

### 教程 1 基本用法

**代码**

```C++
eventpp::EventQueue<int, void (const std::string &, std::unique_ptr<int> &)> queue;

queue.appendListener(3, [](const std::string & s, std::unique_ptr<int> & n) {
    std::cout << "Got event 3, s is " << s << " n is " << *n << std::endl;
});

// 监听器原型不需要和 dispatcher 完全一致，参数类型兼容即可
queue.appendListener(5, [](std::string s, const std::unique_ptr<int> & n) {
    std::cout << "Got event 5, s is " << s << " n is " << *n << std::endl;
});
queue.appendListener(5, [](const std::string & s, std::unique_ptr<int> & n) {
    std::cout << "Got another event 5, s is " << s << " n is " << *n << std::endl;
});

// 将事件加入队列，首个参数是事件类型。监听器在入队列期间不会被触发
queue.enqueue(3, "Hello", std::unique_ptr<int>(new int(38)));
queue.enqueue(5, "World", std::unique_ptr<int>(new int(58)));

// 处理事件队列，分发队列中的所有事件
queue.process();
```

**输出**

> Got event 3, s is Hello n is 38  
> Got event 5, s is World n is 58  
> Got another event 5, s is World n is 58  

**解读**
`EventDispatcher<>::dispatch()` 触发监听器的动作是同步的。但异步事件队列在某些场景下能发挥更大的作用（例如 Windows 消息队列、游戏中的消息队列等）。EventQueue 就是用于满足该类需求的事件队列。  
`EventQueue<>::enqueue()` 将事件加入队列，其参数和 `dispatch` 的参数完全相同。  
`EventQueue<>::process()` 用于分发队列中的事件。不调用 process ，事件就不会被分发。  
事件队列的典型用例：在 GUI 应用中，每个组件都调用 `EventQueue<>::enqueue()` 来发布事件，然后主事件循环调用 `EventQueue<>()::process()` 来 dispatch 所有队列中的事件。  
`EventQueue` 支持将不可拷贝对象作为事件参数，例如上面例子中的 unique_ptr

### 教程 2 —— 多线程

**代码**

```c++
using EQ = eventpp::EventQueue<int, void (int)>;
EQ queue;

constexpr int stopEvent = 1;
constexpr int otherEvent = 2;

// 启动一个新线程来处理事件队列。所有监听器都会在该线程中启动运行
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

// 将一个主线程的事件加入队列。在休眠 10 ms 时，该事件应该已经被另一个线程处理了
queue.enqueue(otherEvent, 1);
std::this_thread::sleep_for(std::chrono::milliseconds(10));
std::cout << "Should have triggered event with index = 1" << std::endl;

queue.enqueue(otherEvent, 2);
std::this_thread::sleep_for(std::chrono::milliseconds(10));
std::cout << "Should have triggered event with index = 2" << std::endl;

{
    // EventQueue::DisableQueueNotify 是一个 RAII 类，能避免唤醒其他的等待线程。
    // 所以该代码块内不会触发任何事件。
    // 当需要一次性添加很多事件，希望在事件都添加完成后才唤醒等待线程时，
    // 就可以使用 DisableQueueNotify 
    EQ::DisableQueueNotify disableNotify(&queue);
    
    queue.enqueue(otherEvent, 10);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::cout << "Should NOT trigger event with index = 10" << std::endl;
    
    queue.enqueue(otherEvent, 11);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::cout << "Should NOT trigger event with index = 11" << std::endl;
}
// DisableQueueNotify 对象在此处销毁，恢复唤醒其他的等待线程。因此事件都会在此处触发
std::this_thread::sleep_for(std::chrono::milliseconds(10));
std::cout << "Should have triggered events with index = 10 and 11" << std::endl;

queue.enqueue(stopEvent, 1);
thread.join();
```

**输出**

> Got event, index is 1  
> Should have triggered event with index = 1  
> Got event, index is 2  
> Should have triggered event with index = 2  
> Should NOT trigger event with index = 10  
> Should NOT trigger event with index = 11  
> Got event, index is 10  
> Got event, index is 11  
> Should have triggered events with index = 10 and 11

