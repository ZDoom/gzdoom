# Benchmarks

Hardware: HP laptop, Intel(R) Core(TM) i5-8300H CPU @ 2.30GHz, 16 GB RAM  
Software: Windows 10, MinGW GCC 11.3.0, MSVC 2022  
Time unit: milliseconds (unless explicitly specified)  

Unless it's specified, the default compiler is GCC.  
The hardware used for benchmark is pretty medium to low end at the time of benchmarking (December 2023).  

## EventQueue enqueue and process -- single threading

<table>
<tr>
    <th>Iterations</th>
    <th>Queue size</th>
    <th>Event count</th>
    <th>Event Types</th>
    <th>Listener count</th>
    <th>Time of single threading</th>
    <th>Time of multi threading</th>
</tr>
<tr>
    <td>100k</td>
    <td>100</td>
    <td>10M</td>
    <td>100</td>
    <td>100</td>
    <td>289</td>
    <td>939</td>
</tr>
<tr>
    <td>100k</td>
    <td>1000</td>
    <td>100M</td>
    <td>100</td>
    <td>100</td>
    <td>2822</td>
    <td>9328</td>
</tr>
<tr>
    <td>100k</td>
    <td>1000</td>
    <td>100M</td>
    <td>1000</td>
    <td>1000</td>
    <td>2923</td>
    <td>9502</td>
</tr>
<table>

Given `eventpp::EventQueue<size_t, void (size_t), Policies>`, which `Policies` is either single threading or multi threading, the benchmark adds `Listener count` listeners to the queue, each listener is an empty lambda. Then the benchmark starts timing. It loops `Iterations` times. In each loop, the benchmark puts `Queue size` events, then process the event queue.  
There are `Event types` kinds of event type. `Event count` is `Iterations * Queue size`.  
The EventQueue is processed in one thread. The Single/Multi threading in the table means the policies used.

## EventQueue enqueue and process -- multiple threading

<table>
<tr>
    <th>Mutex</th>
    <th>Enqueue threads</th>
    <th>Process threads</th>
    <th>Event count</th>
    <th>Event Types</th>
    <th>Listener count</th>
    <th>Time</th>
</tr>
<tr>
    <td>std::mutex</td>
    <td>1</td>
    <td>1</td>
    <td>10M</td>
    <td>100</td>
    <td>100</td>
    <td>1824</td>
</tr>
<tr>
    <td>SpinLock</td>
    <td>1</td>
    <td>1</td>
    <td>10M</td>
    <td>100</td>
    <td>100</td>
    <td>1303</td>
</tr>

<tr>
    <td>std::mutex</td>
    <td>1</td>
    <td>3</td>
    <td>10M</td>
    <td>100</td>
    <td>100</td>
    <td>2989</td>
</tr>
<tr>
    <td>SpinLock</td>
    <td>1</td>
    <td>3</td>
    <td>10M</td>
    <td>100</td>
    <td>100</td>
    <td>3186</td>
</tr>

<tr>
    <td>std::mutex</td>
    <td>2</td>
    <td>2</td>
    <td>10M</td>
    <td>100</td>
    <td>100</td>
    <td>3151</td>
</tr>
<tr>
    <td>SpinLock</td>
    <td>2</td>
    <td>2</td>
    <td>10M</td>
    <td>100</td>
    <td>100</td>
    <td>3049</td>
</tr>

<tr>
    <td>std::mutex</td>
    <td>4</td>
    <td>4</td>
    <td>10M</td>
    <td>100</td>
    <td>100</td>
    <td>1657</td>
</tr>
<tr>
    <td>SpinLock</td>
    <td>4</td>
    <td>4</td>
    <td>10M</td>
    <td>100</td>
    <td>100</td>
    <td>1659</td>
</tr>

<tr>
    <td>std::mutex</td>
    <td>16</td>
    <td>16</td>
    <td>10M</td>
    <td>100</td>
    <td>100</td>
    <td>708</td>
</tr>
<tr>
    <td>SpinLock</td>
    <td>16</td>
    <td>16</td>
    <td>10M</td>
    <td>100</td>
    <td>100</td>
    <td>1891</td>
</tr>
</table>

There are `Enqueue threads` threads enqueuing events to the queue, and `Process threads` threads processing the events. The total event count is `Event count`. `Mutex` is the mutex type used to protect the data.  
The multi threading version shows slower than previous single threading version, since the mutex locks cost time.  
When there are fewer threads (about around the number of CPU cores which is 4 here), `eventpp::SpinLock` has better performance than `std::mutex`. But there are much more threads than CPU cores (here is 16 enqueue threads and 16 process threads), `eventpp::SpinLock` has worse performance than `std::mutex`.  

## CallbackList append/remove callbacks

The benchmark loops 100K times, in each loop it appends 1000 empty callbacks to a CallbackList, then remove all that 1000 callbacks. So there are totally 100M append/remove operations.  
The total benchmarked time is about 16000 milliseconds. That's to say in 1 milliseconds there can be 6000 append/remove operations.

## CallbackList invoking VS native function invoking

Iterations: 100,000,000  

<table>
<tr>
    <th>Function</th>
    <th>Compiler</th>
    <th>Native invoking</th>
    <th>CallbackList single threading</th>
    <th>CallbackList multi threading</th>
</tr>

<tr>
    <td rowspan="2">Inline global function</td>
    <td>MSVC</td>
    <td>139</td>
    <td>1267</td>
    <td>3058</td>
</tr>
<tr>
    <td>GCC</td>
    <td>141</td>
    <td>1149</td>
    <td>2563</td>
</tr>

<tr>
    <td rowspan="2">Non-inline global function</td>
    <td>MSVC</td>
    <td>143</td>
    <td>1273</td>
    <td>3047</td>
</tr>
<tr>
    <td>GCC</td>
    <td>132</td>
    <td>1218</td>
    <td>2583</td>
</tr>

<tr>
    <td rowspan="2">Function object</td>
    <td>MSVC</td>
    <td>139</td>
    <td>1198</td>
    <td>2993</td>
</tr>
<tr>
    <td>GCC</td>
    <td>141</td>
    <td>1107</td>
    <td>2633</td>
</tr>

<tr>
    <td rowspan="2">Member virtual function</td>
    <td>MSVC</td>
    <td>159</td>
    <td>1221</td>
    <td>3076</td>
</tr>
<tr>
    <td>GCC</td>
    <td>140</td>
    <td>1231</td>
    <td>2691</td>
</tr>

<tr>
    <td rowspan="2">Member non-virtual function</td>
    <td>MSVC</td>
    <td>140</td>
    <td>1266</td>
    <td>3054</td>
</tr>
<tr>
    <td>GCC</td>
    <td>140</td>
    <td>1193</td>
    <td>2701</td>
</tr>

<tr>
    <td rowspan="2">Member non-inline virtual function</td>
    <td>MSVC</td>
    <td>158</td>
    <td>1223</td>
    <td>3103</td>
</tr>
<tr>
    <td>GCC</td>
    <td>133</td>
    <td>1231</td>
    <td>2676</td>
</tr>

<tr>
    <td rowspan="2">Member non-inline non-virtual function</td>
    <td>MSVC</td>
    <td>134</td>
    <td>1266</td>
    <td>3028</td>
</tr>
<tr>
    <td>GCC</td>
    <td>134</td>
    <td>1205</td>
    <td>2652</td>
</tr>

<tr>
    <td rowspan="2">All functions</td>
    <td>MSVC</td>
    <td>91</td>
    <td>903</td>
    <td>2214</td>
</tr>
<tr>
    <td>GCC</td>
    <td>89</td>
    <td>858</td>
    <td>1852</td>
</tr>

</table>

Testing functions  
```c++
#if defined(_MSC_VER)
#define NON_INLINE __declspec(noinline)
#else
// gcc
#define NON_INLINE __attribute__((noinline))
#endif

volatile int globalValue = 0;

void globalFunction(int a, const int b)
{
    globalValue += a + b;
}

NON_INLINE void nonInlineGlobalFunction(int a, const int b)
{
    globalValue += a + b;
}

struct FunctionObject
{
    void operator() (int a, const int b)
    {
        globalValue += a + b;
    }

    virtual void virFunc(int a, const int b)
    {
        globalValue += a + b;
    }

    void nonVirFunc(int a, const int b)
    {
        globalValue += a + b;
    }

    NON_INLINE virtual void nonInlineVirFunc(int a, const int b)
    {
        globalValue += a + b;
    }

    NON_INLINE void nonInlineNonVirFunc(int a, const int b)
    {
        globalValue += a + b;
    }
};

#undef NON_INLINE
```
