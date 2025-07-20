 #include "work_queue.hpp"
#include "esp_log.h"

static const char *TAG = "WorkQueue";
WorkQueue& WorkQueue::get()
{
    static WorkQueue wq;
    return wq;
}

WorkQueue::WorkQueue() :
    Task(TAG),
    workQ(10)
{
    start();
} 

WorkQueue::~WorkQueue()
{

}

void WorkQueue::invoke(WorkFunc&& func)
{
    workQ.push(std::move(func), std::chrono::seconds(4));
}

void WorkQueue::task()
{
    while(1)
    {
        WorkFunc func;
        if(not workQ.pop(func, std::chrono::seconds(4)))
        {
            continue;
        }
        if(func)
        {
            func();
        }
    }
}