#pragma once

#include <memory>
#include <string>
class ZegoRTCEngine {
public:
	~ZegoRTCEngine(){}
	static std::shared_ptr<ZegoRTCEngine> Create();
	virtual void CreateEngine() = 0;
	virtual void setScenario(bool isAudio, bool isVideo, std::string focusStream, void *player) = 0;
	virtual void LoginRoom(std::string roomId, std::string userId) = 0;
	virtual void LogoutRoom() = 0;
	virtual bool IsLogin() = 0;

	virtual void MuteMic(bool mute) = 0;
	virtual void PushAudioData(void *data, int size, uint64_t ts) = 0;

	virtual void BeginTalk(std::string streamId, void *view) = 0;
	virtual void EndTalk() = 0;
	virtual void DestroyEngine() = 0;

};

