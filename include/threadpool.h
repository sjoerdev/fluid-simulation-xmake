#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <vector>
#include <queue>
#include <functional>

class thread_pool
{
public:
    void start_pool();
    void add_job(const std::function<void()>& job);
    void stop_pool();
    bool is_busy();
    void parallel_for(int start, int end, const std::function<void(int)>& func);

private:
    void search();
    bool should_terminate = false;           // tells threads to stop looking for jobs
    std::mutex queue_mutex;                  // prevents data races to the job queue
    std::condition_variable mutex_condition; // allows threads to wait on new jobs or termination 
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> jobs;
    std::atomic<int> active_jobs = 0;
};

#endif