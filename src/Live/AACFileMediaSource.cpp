#include <string.h>
#include "AACFileMediaSource.h"
#include "../Base/Log.h"

AACFileMeidaSource* AACFileMeidaSource::createNew(UsageEnvironment* env, const std::string& file)
{
    return new AACFileMeidaSource(env, file);
}

AACFileMeidaSource::AACFileMeidaSource(UsageEnvironment* env, const std::string& file) :
    MediaSource(env){

    mSourceName = file;
    mFile = fopen(file.c_str(), "rb");

    setFps(43);

    for(int i = 0; i < DEFAULT_FRAME_NUM; ++i){
        mEnv->threadPool()->addTask(mTask);
    }
}

AACFileMeidaSource::~AACFileMeidaSource()
{
    fclose(mFile);
}

void AACFileMeidaSource::handleTask()
{
    std::lock_guard <std::mutex> lck(mMtx);

    if(mFrameInputQueue.empty())
        return;
    MediaFrame* frame = mFrameInputQueue.front();

    frame->mSize = getFrameFromAACFile(frame->temp, FRAME_MAX_SIZE);
    if (frame->mSize < 0) {
        return;
    }
    frame->mBuf = frame->temp;

    mFrameInputQueue.pop();
    mFrameOutputQueue.push(frame);
}

bool AACFileMeidaSource::parseAdtsHeader(uint8_t* in, struct AdtsHeader* res)
{
    memset(res,0,sizeof(*res));

    // syncword: 12bit must be 1
    if ((in[0] == 0xFF)&&((in[1] & 0xF0) == 0xF0))
    {
        res->id = ((unsigned int) in[1] & 0x08) >> 3;
        res->layer = ((unsigned int) in[1] & 0x06) >> 1;
        res->protectionAbsent = (unsigned int) in[1] & 0x01;
        res->profile = ((unsigned int) in[2] & 0xc0) >> 6;
        res->samplingFreqIndex = ((unsigned int) in[2] & 0x3c) >> 2;
        res->privateBit = ((unsigned int) in[2] & 0x02) >> 1;
        res->channelCfg = ((((unsigned int) in[2] & 0x01) << 2) | (((unsigned int) in[3] & 0xc0) >> 6));
        res->originalCopy = ((unsigned int) in[3] & 0x20) >> 5;
        res->home = ((unsigned int) in[3] & 0x10) >> 4;
        res->copyrightIdentificationBit = ((unsigned int) in[3] & 0x08) >> 3;
        res->copyrightIdentificationStart = (unsigned int) in[3] & 0x04 >> 2;
        res->aacFrameLength = (((((unsigned int) in[3]) & 0x03) << 11) |
                                (((unsigned int)in[4] & 0xFF) << 3) |
                                    ((unsigned int)in[5] & 0xE0) >> 5) ;
        res->adtsBufferFullness = (((unsigned int) in[5] & 0x1f) << 6 |
                                        ((unsigned int) in[6] & 0xfc) >> 2);
        res->numberOfRawDataBlockInFrame = ((unsigned int) in[6] & 0x03);

        return true;
    }
    else
    {
        LOGE("failed to parse adts header");
        return false;
    }
}

int AACFileMeidaSource::getFrameFromAACFile(uint8_t* buf, int size)
{
    if (!mFile) {
        return -1;
    }

    uint8_t tmpBuf[7];
    int ret;

    // 从文件 mFile 中读取 7 个字节，每个字节的大小为 1 字节，总共读取 7 字节数据并存储在 tmpBuf 中
    ret = fread(tmpBuf,1,7,mFile);
    if(ret <= 0)
    {
        fseek(mFile,0,SEEK_SET);
        ret = fread(tmpBuf,1,7,mFile);
        if(ret <= 0)
        {
            return -1;
        }
    }

    //  解析tmpBuf, 存储在mAdtsHeader
    if(!parseAdtsHeader(tmpBuf, &mAdtsHeader))
    {
        return -1;
    }

    if(mAdtsHeader.aacFrameLength > size)
        return -1;

    // tmpBuf 中的 7 字节数据（即 ADTS 头）复制到目标缓冲区 buf 中
    memcpy(buf, tmpBuf, 7);

    // 从 mFile 中读取实际的音频帧数据，长度为 aacFrameLength - 7（减去已经复制的 ADTS 头长度），并将数据存储在 buf 中的第 8 字节位置开始
    ret = fread(buf+7,1,mAdtsHeader.aacFrameLength-7,mFile);
    if(ret < 0)
    {
        LOGE("read error");
        return -1;
    }

    return mAdtsHeader.aacFrameLength;
}