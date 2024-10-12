#include "SelectPoller.h"
#include "../Base/Log.h"

SelectPoller::SelectPoller()
{
    FD_ZERO(&mReadSet);
    FD_ZERO(&mWriteSet);
    FD_ZERO(&mExceptionSet);
}

SelectPoller::~SelectPoller()
{

}

SelectPoller* SelectPoller::createNew()
{
    return new SelectPoller();
    //    return New<SelectPoller>::allocate();
}

bool SelectPoller::addIOEvent(IOEvent* event)
{
    return updateIOEvent(event);
}


// updateIOEvent 函数的主要目的是维护一个文件描述符的集合
// 以便使用 select 轮询机制高效地处理多个 I/O 事件
bool SelectPoller::updateIOEvent(IOEvent* event)
{
    int fd = event->getFd();
    if (fd < 0)
    {
        LOGE("fd=%d",fd);

        return false;
    }

    // 从mReadSet移除fd描述符
    FD_CLR(fd, &mReadSet);
    FD_CLR(fd, &mWriteSet);
    FD_CLR(fd, &mExceptionSet);

    IOEventMap::iterator it = mEventMap.find(fd);

    //如果事件已存在，依据当前事件的状态（读、写、错误）更新对应的文件描述符集合。
    if (it != mEventMap.end()) //先前已经添加则修改
    {
        if (event->isReadHandling())
            FD_SET(fd, &mReadSet);
        if (event->isWriteHandling())
            FD_SET(fd, &mWriteSet);
        if (event->isErrorHandling())
            FD_SET(fd, &mExceptionSet);
    }

    // 如果事件不存在，则根据当前事件的状态将文件描述符添加到相应的集合，并将事件插入到 mEventMap 中
    else //先前未添加则添加IO事件
    {
        if (event->isReadHandling())
            FD_SET(fd, &mReadSet);
        if (event->isWriteHandling())
            FD_SET(fd, &mWriteSet);
        if (event->isErrorHandling())
            FD_SET(fd, &mExceptionSet);

        mEventMap.insert(std::make_pair(fd, event));
    }

    // 根据当前事件映射的状态，更新 mMaxNumSockets，以便 select 函数能够知道要监视的最大文件描述符
    if (mEventMap.empty())
        mMaxNumSockets = 0;
    else
        mMaxNumSockets = mEventMap.rbegin()->first + 1; //更新最大文件描述符+1（map会自动排序）

    return true;
}

bool SelectPoller::removeIOEvent(IOEvent* event)
{
    int fd = event->getFd();
    if (fd < 0)
        return false;

    FD_CLR(fd, &mReadSet);
    FD_CLR(fd, &mWriteSet);
    FD_CLR(fd, &mExceptionSet);

    IOEventMap::iterator it = mEventMap.find(fd);
    if (it != mEventMap.end())
        mEventMap.erase(it);

    if (mEventMap.empty())
        mMaxNumSockets = 0;
    else
        mMaxNumSockets = mEventMap.rbegin()->first + 1;

    return true;
}

void SelectPoller::handleEvent() {

    fd_set readSet = mReadSet;
    fd_set writeSet = mWriteSet;
    fd_set exceptionSet = mExceptionSet;
    struct timeval timeout;
    int ret;
    int rEvent;

    timeout.tv_sec = 1000;// 秒
    timeout.tv_usec = 0;//微秒
    //LOGI("mEventMap.size() = %d，select io start", mEventMap.size());

    ret = select(mMaxNumSockets, &readSet, &writeSet, &exceptionSet, &timeout);


    if (ret < 0){
        return;
    }else {
        //LOGI("检测到活跃的描述符 ret=%d", ret);
    }
    //LOGI("select io end");

    for (IOEventMap::iterator it = mEventMap.begin(); it != mEventMap.end(); ++it)
    {
        rEvent = 0;
        if (FD_ISSET(it->first, &readSet))
            rEvent |= IOEvent::EVENT_READ;

        if (FD_ISSET(it->first, &writeSet))
            rEvent |= IOEvent::EVENT_WRITE;

        if (FD_ISSET(it->first, &exceptionSet))
            rEvent |= IOEvent::EVENT_ERROR;

        if (rEvent != 0)
        {
            it->second->setREvent(rEvent);
            mIOEvents.push_back(it->second);
        }
    }

    for (auto& ioEvent : mIOEvents) {
        ioEvent->handleEvent();
    }

    mIOEvents.clear();

}



