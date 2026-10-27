// #include <stdio.h>#
#include <iostream> // cout
#include <cstdint> //for uint16_t
#include <queue> // queue and priority queue
#include <functional> // function pointer
#include <chrono> // now, time in seconds
#include <atomic> // atomic variables
#include <mutex> // mutex, lock_guard
#include <condition_variable> // condition_variable
#include <thread> //sleep_for, sleep_until, threads

using namespace std;

atomic<bool> enableError{false};
// atomic<bool> enableError{true};

// enum class ErrorCodes
// {
//     success,
//     sendUdpFailedImmediate,
//     sendUdpFailedDelayed,
//     sendUdpFailedRepeating
// };

class EventLoop
{
    public:

        using Task = function<void()>;
        using ErrorHandle = function<void(string)>;

        void sendImmediately(string& ip, uint16_t& port, const string& mesg, ErrorHandle errHandler = nullptr);
        void sendAfterXSeconds(const string& ip, const uint16_t& port, const string& mesg, const uint8_t& delaySeconds);
        void sendEveryXseconds(string& ip, uint16_t& port, const string& mesg, uint8_t intervalSeconds);

        void run();

        void stop(uint8_t timeoutSeconds = 0);

        inline bool isStopped()
        {
            return stopFlag.load();
        };

    private:
        int sendUdp(const string& ip,const uint16_t& port, const string& mesg);

        // A separate message struct is unnecessary here. The lambda captures
        // the message data and sends it immediately or after a delay.
        // struct messageItem
        // {
        //     string ip;
        //     uint16_t port;
        //     string mesg;
        // };

        struct delayedTask
        {
            Task t;
            chrono::steady_clock::time_point whenToRun; 
            uint8_t intervalSeconds = 0U;

            bool operator<(const delayedTask& other) const
            {
                return this->whenToRun > other.whenToRun;
            }
        };

        queue<Task> immediateQueue;
        priority_queue<delayedTask> scheduledQueue;

        atomic<bool> stopFlag{false};

        mutex queueMutex; // Mutex to protect access to the queues

        condition_variable cv; // Condition variable to notify the event loop of new tasks
};

class CounterClock
{
    public:
        CounterClock(EventLoop& loop) : loop(loop){};
        void runCounter();

    private:
        uint8_t cnt = 0U;
        inline void displayCnt()
        {
            ++cnt;
            cout << "Count is : " << static_cast<int>(cnt) << endl;
            this_thread::sleep_for(chrono::seconds(1));
        };
        EventLoop& loop;
};

void CounterClock::runCounter()
{
    while(!loop.isStopped())
    {
        displayCnt();
    }
}

int EventLoop::sendUdp(const string& ip,const uint16_t& port, const string& mesg)
{
    if(enableError)
    {
        return -1;
    }
    else
    {
        cout << "Sending message : " << mesg << " to " << ip << ":" << port << endl;
        return 0;
    }
}

void EventLoop::sendImmediately(string& ip, uint16_t& port, const string& mesg, ErrorHandle errHandler)
{
    // A lambda's call operator is const by default, so its value captures
    // cannot be modified. `mutable` would allow modifying those captured
    // copies, but it is unnecessary here because sendUdp takes const references.
    // q1.push([this,mesg,ip,port]() mutable {
    //     this->sendUdp(ip, port, mesg);
    //     cout << "Sending immediate message" << endl;
    // });

    {
        lock_guard<mutex> lock(queueMutex);
        immediateQueue.push([this,mesg,ip,port, errHandler]() {
            // cout << "Sending immediate message" << endl;
            int result = this->sendUdp(ip, port, mesg);
            if(result!=0 && errHandler)
            {
                errHandler("Immediate Message Failed");
            }
        });
    }

    // The extra scope releases the lock immediately after the queue update,
    // before any subsequent work is performed.
    
    cv.notify_one(); // Notify the event loop that a new task is available
}

void EventLoop::sendAfterXSeconds(const string& ip, const uint16_t& port, const string& mesg, const uint8_t& delaySeconds)
{
    delayedTask dt;
    dt.t = [this, ip, port, mesg, delaySeconds](){
        // cout << "Sending delayed message after " << static_cast<int>(delaySeconds) << " seconds." << endl;
        this->sendUdp(ip, port, mesg);

        // NOTE: If you dont cast the uint to int, it will not print the number of seconds correctly. It will print a character instead of the number.
    };
    dt.whenToRun = chrono::steady_clock::now() + chrono::seconds(delaySeconds);

    lock_guard<mutex> lock(queueMutex);
    scheduledQueue.push(dt);

    cv.notify_one(); // Notify the event loop that a new task is available
}

void EventLoop::sendEveryXseconds(string& ip, uint16_t& port, const string& mesg, uint8_t intervalSeconds)
{
    delayedTask dt;

    // Note: The lambda function shoudl call & or = before the capture list to capture the variables by reference or by value. If you do [this, =], it throwws and error. 
    dt.t = [this, ip, port, mesg, intervalSeconds](){
        // cout << "Sending repeating message every " << static_cast<int>(intervalSeconds) << " seconds." << endl;
        this->sendUdp(ip, port, mesg);
    };
    dt.intervalSeconds = intervalSeconds;
    dt.whenToRun = chrono::steady_clock::now() + chrono::seconds(intervalSeconds);

    lock_guard<mutex> lock(queueMutex);
    scheduledQueue.push(dt);

    cv.notify_one(); // Notify the event loop that a new task is available
}

void EventLoop::run()
{
    
    // Makes atomic operations obvious.
    // Interview-friendly.
    while(!stopFlag.load())
    // while(!stopFlag)
    {
        // Acquire Lock
        //     ↓
        // Extract Task
        //     ↓
        // Pop Task
        //     ↓
        // Release Lock
        //     ↓
        // Execute Task

        Task task;
        {
            lock_guard<mutex> lock(queueMutex);
            if(!immediateQueue.empty())
            {
                task = immediateQueue.front();
                immediateQueue.pop();
            }
        }

        // if (task!=nullptr)
        if (task) // Apparently this is idiomatic in C++ to check if a std::function is empty or not. No need to compare with nullptr.
        {
            task();
        }
       
        // Since this is local to this thread, we don't need to protect it with a mutex. It is only used to store the tasks that are ready to run.
        queue<delayedTask> taskQueue;
        
        {
            lock_guard<mutex> lock(queueMutex);
            while (!scheduledQueue.empty() && scheduledQueue.top().whenToRun <= chrono::steady_clock::now())
            {
    
                // unique_lock<mutex> lock(queueMutex);
    
                delayedTask dt = scheduledQueue.top(); // Make a copy
                scheduledQueue.pop();
                if(dt.intervalSeconds != 0)
                {
                    dt.whenToRun += chrono::seconds(dt.intervalSeconds);
                    scheduledQueue.push(dt);
                }
    
                // lock.unlock();
                // dt.t();
                taskQueue.push(dt);
            }
        }

        while(!taskQueue.empty())
        {
            taskQueue.front().t();
            taskQueue.pop();
        }

        unique_lock<mutex> lock(queueMutex);
        if(!scheduledQueue.empty() && immediateQueue.empty())
        {
            chrono::steady_clock::time_point nextTime = scheduledQueue.top().whenToRun;
            // cv.wait_until(lock, nextTime, [&](){ return !immediateQueue.empty() || stopFlag.load();});
            cv.wait_until(lock, nextTime);
            // Remove the lambda predicate from wait_until. Without the predicate, it will reset the timer and wait for the next scheduled task. With the predicate, it will wake up immediately if there is an immediate task or if the stop flag is set.
        }

        // unique_lock<mutex> lock(queueMutex);
        if(scheduledQueue.empty() && immediateQueue.empty())
        {
            cout << "No tasks to run. Event loop is idle." << endl;
            // stopFlag = true;
            // stopFlag.store(true);
            cv.wait(lock, [&](){
                return (!scheduledQueue.empty() || !immediateQueue.empty() || stopFlag.load()); 
            });

            // Instead of stopping, you can put the thread to sleep for a while and then check again. This is useful when you want to wait for new tasks to arrive.
        }
    }
}

void EventLoop::stop(uint8_t timeoutSeconds)
{
    if(!stopFlag.load())
    {
        if(timeoutSeconds==0)
        {
            cout << "Stopping the event loop immediately." << endl;
        }
        else
        {
            cout << "Stopping the event loop in " << static_cast<int>(timeoutSeconds) << " seconds." << endl;
            this_thread::sleep_for(chrono::seconds(timeoutSeconds));
        }

        stopFlag.store(true);
        cv.notify_all();
    }
}

void errorHandlerImmediate(const string& mesg)
{
    cout << "sendUdpFailedImmediate : " << mesg << endl;
}

int main()
{
    cout << "This is the revision of event loops and multithreading." << endl;

    EventLoop loop;

    CounterClock counter(loop);

    string ip = "192.168.0.25";
    uint16_t port = 9000;
    loop.sendImmediately(ip, port, "Send A immediately", errorHandlerImmediate);
    loop.sendImmediately(ip, port, "Send B immediately", [](const string& mesg){
        cout << "Packet B : " << mesg << " We want to retry." << endl;
    });
    
    // enableError.store(false);
    
    loop.sendAfterXSeconds(ip, port, "Send C after 5 seconds", 5);
    loop.sendAfterXSeconds(ip, port, "Send D after 5 seconds", 5);
    loop.sendAfterXSeconds(ip, port, "Send E after 5 seconds", 5);
    
    // loop.sendEveryXseconds(ip, port, "Send F every 3 seconds", 3);
    

    // Note: There are two ways to run the event loop. One is to run it in the main thread and the other is to run it in a separate thread. Here we are running it in a separate thread.
    // Why: Because if we run it in the main thread, the main thread will be blocked and we will not be able to send any more messages. But if we run it in a separate thread, we can send more messages while the event loop is running.
    // Additionally, we can also stop the event loop from another thread. This is useful when we want to stop the event loop after a certain time or after a certain condition is met.
    
    // Two ways of writing the code to run the event loop in a separate thread. One is to use a lambda function and the other is to use a member function pointer. Both are valid and work fine. The choice is yours.
    // thread runThread([&loop](){
    //     loop.run();
    // });
    thread runThread(&EventLoop::run, &loop);

    // this_thread::sleep_for(chrono::seconds(15));
    thread stopThread(&EventLoop::stop, &loop, 10);


    thread countingThread([&counter](){
        counter.runCounter();
    });

    this_thread::sleep_for(chrono::seconds(6));
    loop.sendImmediately(ip, port, "Send G immediately", errorHandlerImmediate);


    runThread.join();
    stopThread.join();
    countingThread.join();

    return 0;
}