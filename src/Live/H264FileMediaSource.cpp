#include "H264FileMediaSource.h"
#include "../Base/Log.h"
#include <fcntl.h>

static inline int startCode3(uint8_t* buf);
static inline int startCode4(uint8_t* buf);

H264FileMediaSource* H264FileMediaSource::createNew(UsageEnvironment* env, const std::string& file)
{
    return new H264FileMediaSource(env, file);
    //    return New<H264FileMediaSource>::allocate(env, file);
}

H264FileMediaSource::H264FileMediaSource(UsageEnvironment* env, const std::string& file) :
    MediaSource(env) {

    mSourceName = file;

    mFile = fopen(file.c_str(), "rb");
    if (mFile == nullptr) {
        LOGE("Failed to open file");
        // 处理错误，例如返回或抛出异常
    }
    else {
        LOGI("Succuss open H264File");
    }

    setFps(25);

    for (int i = 0; i < DEFAULT_FRAME_NUM; ++i) {
        mEnv->threadPool()->addTask(mTask);
    }
}

H264FileMediaSource::~H264FileMediaSource()
{
    fclose(mFile);
}

void H264FileMediaSource::handleTask()
{
    std::lock_guard <std::mutex> lck(mMtx);

    if (mFrameInputQueue.empty())
        return;

    MediaFrame* frame = mFrameInputQueue.front();
    int startCodeNum = 0;

    while (true)
    {
        //  每次尝试读取到一个完整的 NALU 到frame->temp
        frame->mSize = getFrameFromH264File(frame->temp, FRAME_MAX_SIZE);
        if (frame->mSize < 0) {
            return;
        }
        if (startCode3(frame->temp)){
            startCodeNum = 3;
        }else{
            startCodeNum = 4;
        }

        // 去掉起始码并调整缓冲区
        frame->mBuf = frame->temp + startCodeNum;
        frame->mSize -= startCodeNum;

        // 获取NAL头的最后 5 位 
        uint8_t naluType = frame->mBuf[0] & 0x1F;
        //LOGI("startCodeNum=%d,naluType=%d,naluSize=%d", startCodeNum, naluType, frame->mSize);

        if (0x09 == naluType) {
            // discard the type byte
            // 表示分隔符 NAL 单元， 跳过该帧并继续读取下一帧
            continue;
        }

        // 如果 NAL 类型是 0x07（序列参数集，SPS）或 0x08（图像参数集，PPS）
        // 则停止读取，但不会跳过帧。
        // SPS 和 PPS 是 H.264 视频流中的重要数据，表示视频编码参数，需要保存下来。
        else if (0x07 == naluType || 0x08 == naluType) {
            //continue;
            break;
        }

        else {
            break;
        }
    }

    mFrameInputQueue.pop();
    mFrameOutputQueue.push(frame);
}

static inline int startCode3(uint8_t* buf)
{
    if (buf[0] == 0 && buf[1] == 0 && buf[2] == 1)
        return 1;
    else
        return 0;
}

static inline int startCode4(uint8_t* buf)
{
    if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1)
        return 1;
    else
        return 0;
}

static uint8_t* findNextStartCode(uint8_t* buf, int len)
{
    int i;

    if (len < 3)
        return NULL;

    for (i = 0; i < len - 3; ++i)
    {
        if (startCode3(buf) || startCode4(buf))
            return buf;

        ++buf;
    }

    if (startCode3(buf))
        return buf;

    return NULL;
}

// 从 H.264 文件中读取一帧数据
int H264FileMediaSource::getFrameFromH264File(uint8_t* frame, int size)
{
    if (!mFile) {
        return -1;
    }

    int r, frameSize;
    uint8_t* nextStartCode;

    r = fread(frame, 1, size, mFile);
    // H.264 帧的起始码, 3 字节的起始码, 4 字节的起始码
    if (!startCode3(frame) && !startCode4(frame)) {
        fseek(mFile, 0, SEEK_SET);
        LOGE("Read % s error, no startCode3 and no startCode4",mSourceName.c_str());
        return -1;
    }

    //从 frame + 3 开始查找（跳过当前的起始码），r - 3 是要查找的字节数
    nextStartCode = findNextStartCode(frame + 3, r - 3);
    if (!nextStartCode) {
        fseek(mFile, 0, SEEK_SET);
        frameSize = r;
        LOGE("Read %s error, no nextStartCode, r=%d", mSourceName.c_str(),r);
    }else {
        //起始码到下一个起始码之间的字节数
        frameSize = (nextStartCode - frame);
        fseek(mFile, frameSize - r, SEEK_CUR);
    }
    return frameSize;
}