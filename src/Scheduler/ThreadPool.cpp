#include "ThreadPool.h"
#include "../Base/Log.h"
ThreadPool* ThreadPool::createNew(int num)
{
    return new ThreadPool(num);
}

ThreadPool::ThreadPool(int num) :
    mThreads(num),
    mQuit(false)
{
    createThreads();
}

ThreadPool::~ThreadPool()
{
    cancelThreads();
}

void ThreadPool::addTask(ThreadPool::Task& task)
{
    std::unique_lock <std::mutex> lck(mMtx);
    mTaskQueue.push(task);
    mCon.notify_one();
}

void ThreadPool::loop(){

    while(!mQuit){

        std::unique_lock <std::mutex> lck(mMtx);
        if (mTaskQueue.empty()) {
            mCon.wait(lck); 
        }
     

        if(mTaskQueue.empty())
            continue;

        Task task = mTaskQueue.front();
        mTaskQueue.pop();

        task.handle();
    }

}

void ThreadPool::createThreads()
{
    std::unique_lock <std::mutex> lck(mMtx);
    for(auto & mThread : mThreads)
        mThread.start(this);
}

void ThreadPool::cancelThreads()
{
    std::unique_lock <std::mutex> lck(mMtx);
    mQuit = true;
    // 唤醒所有等待的线程，使它们能够检查退出条件
    mCon.notify_all();
    for(auto & mThread : mThreads)
        // 等待每个线程完成其执行并安全退出
        mThread.join();

    mThreads.clear();
}

void ThreadPool::MThread::run(void* arg)
{
    ThreadPool* threadPool = (ThreadPool*)arg;
    threadPool->loop();
}