#include "Scheduler/EventScheduler.h"
#include "Scheduler/ThreadPool.h"
#include "Scheduler/UsageEnvironment.h"
#include "Live/MediaSessionManager.h"
#include "Live/RtspServer.h"
#include "Live/H264FileMediaSource.h"
#include "Live/H264FileSink.h"
#include "Live/AACFileMediaSource.h"

#include "Live/AACFileSink.h"
#include "Base/Log.h"

// 函数指针 https://blog.csdn.net/m0_45388819/article/details/113822935

int main() {
    /*
    * 
    程序初始化了一份session名为test的资源，访问路径如下

    // rtp over tcp
    ffplay -i -rtsp_transport tcp  rtsp://127.0.0.1:8554/test

    // rtp over udp
    ffplay -i rtsp://127.0.0.1:8554/test
    
    */

    srand(time(NULL));//时间初始化

    // 通过定时器不断向客户端发送流信息


    // 初始化Windows网络编程和SelectPoller构造初始化
    // 设置setTimerManagerReadCallback可读回调函数， 等待env->scheduler()->loop();触发readCallback回调函数
    // 进而继续：等待H264_Sink和AAC_Sink中的mTimeoutCallback设置好cbTimeout回调函数之后
    // 判断mTimeoutCallback来决定是否调用cbTimeout定时回调函数（不断发送AAC和H264的RTP数据包）
    EventScheduler* scheduler = EventScheduler::createNew(EventScheduler::POLLER_SELECT);

    // 判断触发线程池mTaskCallback回调函数
    // 线程池主要判断是否触发：读取并解析aac和h264文件的任务队列的回调函数（数据来源处理）
    ThreadPool* threadPool = ThreadPool::createNew(1);

    // SessionManager容器用来管理Session。其中一个Session包含1个或多个流，track0，track1，...
    MediaSessionManager* sessMgr = MediaSessionManager::createNew();

    // 初始化UsageEnvironment容器用来存储scheduler和threadPool，方便调用
    UsageEnvironment* env = UsageEnvironment::createNew(scheduler, threadPool);
 
    // 初始化网络地址
    Ipv4Address rtspAddr("127.0.0.1", 8554);

    /*
    将sessMgr rtspAddr env上面初始化的资源存储到RtspServer成员变量
    初始化TCP Socket 地址复用 bind
    根据上边socket fd（也就是rtspserver第一个fd）创建IOEvent并初始化
    设置RtspServer的readCallback回调函数（处理RTSPServer连接、断开以及IO事件的处理）
    初始化TriggerEvent回调，并设置cbCloseConnect回调函数
    */ 
    RtspServer* rtspServer = RtspServer::createNew(env, sessMgr,rtspAddr);

    LOGI("----------session init start------");
    {   
        //创建一个session
        MediaSession* session = MediaSession::createNew("test");

        /*
        设置taskCallback任务回调函数(解析H264裸流，将每一个NALU保存到mFrameInputQueue队列中， 并将处理过的NALU保存再mFrameOutputQueue) 
        打开H264文件
        往线程池任务队列添加任务
        */
        MediaSource* source = H264FileMediaSource::createNew(env, "../data/daliu.h264");

        /* 
        初始化H264_Sink, 创建TimerEvent，设置cbTimeout回调函数（发送RTP数据包）
        调用runEvery 函数使得 让H264_Sink 类中的某个任务可以在指定的时间间隔内重复执行
        */
        Sink* sink = H264FileSink::createNew(env, source);

        // 设置sendPacketCallback回调函数(选择使用TCP/UDP进行发送)
        session->addSink(MediaSession::TrackId0, sink);

        /*
        设置taskCallback任务回调函数(解析AAC裸流)
        打开AAC文件
        往线程池任务队列添加任务
        */
        source = AACFileMeidaSource::createNew(env, "../data/daliu.aac");

        /*
        初始化AAC_Sink, 创建TimerEvent，设置cbTimeout回调函数（发送RTP数据包）
        调用runEvery 函数使得 让AAC_Sink 类中的某个任务可以在指定的时间间隔内重复执行
        */
        sink = AACFileSink::createNew(env, source);

        // 设置sendPacketCallback回调函数(选择使用TCP/UDP进行发送)
        session->addSink(MediaSession::TrackId1, sink);

        //session->startMulticast(); //多播
        
        // 往SessionManager容器添加Session
        sessMgr->addSession(session);
    }
    LOGI("----------session init end------");

    // listen 并且 添加 mAcceptIOEvent 事件
    rtspServer->start();

    env->scheduler()->loop();
    return 0;

}



// 通过多线程和回调函数实现RTSP流媒体数据的解析封装与发送