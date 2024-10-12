#ifndef ZYX_RTSPSERVER_MEDIASESSIONMANAGER_H
#define ZYX_RTSPSERVER_MEDIASESSIONMANAGER_H

#include <map>
#include <string>
class MediaSession;
class MediaSessionManager
{
public:
	static MediaSessionManager* createNew();

	MediaSessionManager();
	~MediaSessionManager();
public:
	bool addSession(MediaSession* session);
	bool removeSession(MediaSession* session);
	MediaSession* getSession(const std::string& name);


private:
	std::map<std::string, MediaSession*> mSessMap;
};



#endif //ZYX_RTSPSERVER_MEDIASESSIONMANAGER_H
