 #include "work_queue.hpp"
#include "esp_log.h"

static const char *TAG = "WorkQueue";
WorkQueue& WorkQueue::get()
{
    static WorkQueue wq;
    return wq;
}

WorkQueue::WorkQueue() :
    Task(__func__)
{
    start();
} 

WorkQueue::~WorkQueue()
{

}

void WorkQueue::invoke(WorkFunc&& func)
{
    std::unique_lock uk(mutex);
    workQ.emplace_back(std::move(func));
    uk.unlock();
    cv.notify_one();
}

void WorkQueue::task()
{
    while(1)
    {
        std::unique_lock uk(mutex);
        if(not cv.wait_for(uk, std::chrono::seconds(4), 
            [this]
            {
                return workQ.size();
            }))
        {
            continue;;
        }
        WorkFunc func = std::move(workQ.front());
        workQ.pop_front();
        uk.unlock();
        if(func)
        {
            func();
        }
    }
}