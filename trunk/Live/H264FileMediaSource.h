#ifndef BXC_RTSPSERVER_H264FILEMEDIASOURCE_H
#define BXC_RTSPSERVER_H264FILEMEDIASOURCE_H


#include <string>
#include "MediaSource.h"

class H264FileMediaSource : public MediaSource
{
public:
    static H264FileMediaSource* createNew(UsageEnvironment* env, const std::string& file);

    H264FileMediaSource(UsageEnvironment* env, const std::string& file);
    virtual ~H264FileMediaSource();

protected:
    virtual void handleTask();

private:
    int getFrameFromH264File(uint8_t* frame, int size);

private:
    FILE* mFile;
};

#endif //BXC_RTSPSERVER_H264FILEMEDIASOURCE_H