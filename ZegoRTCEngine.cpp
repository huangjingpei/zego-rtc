#include "ZegoRTCEngine.h"
#include <algorithm>
#include <thread>
#include <chrono>
#include <memory>
#include "../../UI/gbs/media/GBSAudioWriter.h"

#include "ZegoExpress/win/x64/include/ZegoExpressSDK.h"
#include "ZegoExpress/win/x64/include/ZegoExpressEventHandler.h"
#include "ZegoExpress/win/x64/include/ZegoExpressDefines.h"
using namespace ZEGO;
using namespace ZEGO::EXPRESS;

class ZegoRTCEngineImpl : public IZegoEventHandler,
			  public ZegoRTCEngine,
	public std::enable_shared_from_this<ZegoRTCEngineImpl> {
public:
	ZegoRTCEngineImpl();
	virtual ~ZegoRTCEngineImpl();

public:
	void CreateEngine() override;
	void setScenario(bool isAudio, bool isVideo, std::string focusStream, void *player) override;
	void LoginRoom(std::string roomId, std::string userId) override;
	void LogoutRoom() override;
	bool IsLogin() override;
	void MuteMic(bool mute) override;
	

	void PushAudioData(void *data, int size, uint64_t ts) override;

	void BeginTalk(std::string streamId, void *view) override;
	void EndTalk() override;
	void DestroyEngine() override;

private:
	void onRoomStateUpdate(const std::string &roomID, ZegoRoomState state, int errorCode,
			       const std::string &extendData) override;
	void onRoomStateChanged(const std::string &roomID, ZegoRoomStateChangedReason reason, int errorCode,
				const std::string &extendedData) override;
	void onRoomExtraInfoUpdate(const std::string &roomID,
				   const std::vector<ZegoRoomExtraInfo> &roomExtraInfoList) override;
	void onRoomUserUpdate(const std::string &roomID, ZegoUpdateType updateType,
			      const std::vector<ZegoUser> &userList) override;
	void onRoomStreamUpdate(const std::string &roomID, ZegoUpdateType updateType,
				const std::vector<ZegoStream> &streamList, const std::string &extendData) override;
	void onRoomStreamExtraInfoUpdate(const std::string &roomID, const std::vector<ZegoStream> &streamList) override;
	void onPublisherSendAudioFirstFrame(ZegoPublishChannel channel) override;
	void onPublisherSendVideoFirstFrame(ZegoPublishChannel channel) override;

private:
	std::atomic<bool> muted_{false};
	std::atomic<bool> logined_{false};
	std::atomic<bool> publishing_{false};
	std::atomic<bool> audio_enable_{true};
	std::atomic<bool> video_enable_{true};
	std::string room_id_{};
	std::string user_id_{};
	std::string focus_stream_{};
	void *player_;
	std::string play_stream_id_{};
	std::vector<ZegoStream> zego_stream_list_{};
	IZegoExpressEngine *engine_{nullptr};

	std::thread worker_thread_;
    std::atomic<bool> thread_running_{true};
};


ZegoRTCEngineImpl::ZegoRTCEngineImpl() {}
ZegoRTCEngineImpl::~ZegoRTCEngineImpl() {}

void ZegoRTCEngineImpl::CreateEngine()
{

	if (engine_ == nullptr) {
		ZegoEngineProfile profile;
		profile.appID = 1556046237;
		profile.appSign = "9d3129378f49f240ad132df482450e7a83f22b7993d0916c9f261185768c3fe1";
		profile.scenario = ZegoScenario::ZEGO_SCENARIO_HIGH_QUALITY_VIDEO_CALL;
		engine_ = ZegoExpressSDK::createEngine(profile, shared_from_this());
		ZegoCustomAudioConfig audioConfig;
		audioConfig.sourceType = ZEGO_AUDIO_SOURCE_TYPE_CUSTOM;
		engine_->enableCustomAudioIO(true, &audioConfig);
		engine_->enableAudioCaptureDevice(false);
		
	}
	worker_thread_ = std::thread([this]() {
		std::shared_ptr<GBSAudioWriter> audioWriter = GBSAudioWriter::Create();
		while (thread_running_) {
			// 记录开始时间
			auto start_time = std::chrono::steady_clock::now();

			if (engine_) {
				static char data[640] = {0};
				unsigned int maxlen = 640;
				ZegoAudioFrameParam renderAudioFrameParam;
				renderAudioFrameParam.sampleRate = ZEGO_AUDIO_SAMPLE_RATE_16K;
				renderAudioFrameParam.channel = ZEGO_AUDIO_CHANNEL_STEREO;
				engine_->fetchCustomAudioRenderPCMData((unsigned char *)data, maxlen,
								       renderAudioFrameParam);
				audioWriter->write((const uint8_t *)data, 640);

				////// 保存 PCM 数据到文件
				//static FILE *pcm_file = nullptr;
				//if (!pcm_file) {
				//	pcm_file = fopen("audio_data.pcm", "wb");
				//	if (!pcm_file) {
				//		return;
				//	}
				//}
				//fwrite(data, 1, 640, pcm_file);
				//fflush(pcm_file);
			}

			// 计算执行时间
			auto end_time = std::chrono::steady_clock::now();
			auto execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
			
			// 计算需要sleep的时间（10ms - 执行时间）
			auto sleep_time = std::chrono::milliseconds(10) - execution_time;
			
			// 如果还有剩余时间则sleep
			if (sleep_time.count() > 0) {
				std::this_thread::sleep_for(sleep_time);
			} else if (sleep_time.count() < 0){
				std::this_thread::sleep_for(std::chrono::milliseconds(10));

			}
		}
	});
}
void ZegoRTCEngineImpl::DestroyEngine()
{
	if (engine_) {
		//destroy engine
		ZegoExpressSDK::destroyEngine(engine_);
		engine_ = nullptr;
	}
	thread_running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}
void ZegoRTCEngineImpl::LoginRoom(std::string roomId, std::string userId)
{
	room_id_ = roomId;
	user_id_ = userId;
	ZegoUser user(user_id_, user_id_);
	ZegoRoomConfig roomConfig;
	roomConfig.isUserStatusNotify = true;
	engine_->loginRoom(room_id_, user, roomConfig);
}
void ZegoRTCEngineImpl::LogoutRoom()
{
	if (engine_) {
		engine_->logoutRoom(room_id_);
	}
}

void ZegoRTCEngineImpl::BeginTalk(std::string streamId, void *view)
{
	if (!publishing_) {
		if (video_enable_ == false) {
			engine_->enableCamera(false);
		}
		if (video_enable_ == true) {
			ZegoCanvas canvas(view);
			engine_->enableCamera(true);
			engine_->startPreview(&canvas);
		}

		if (audio_enable_ || video_enable_) {
			engine_->startPublishingStream(streamId);
			publishing_ = true;
		}
	}
}
void ZegoRTCEngineImpl::EndTalk()
{
	if (publishing_) {
		engine_->stopPublishingStream();
		if (video_enable_) {
			engine_->stopPreview();
		}
		publishing_ = false;
	}
}

void ZegoRTCEngineImpl::PushAudioData(void *data, int size, uint64_t ts) {
	if (publishing_) {
		ZegoAudioFrameParam param;
		param.sampleRate = ZEGO_AUDIO_SAMPLE_RATE_48K;
		param.channel = ZEGO_AUDIO_CHANNEL_STEREO;
		if (engine_) {
			engine_->sendCustomAudioCapturePCMData((unsigned char *)(data), size, param);
		}


		
	}
}

void ZegoRTCEngineImpl::MuteMic(bool mute) {
	muted_ = mute;
}

bool ZegoRTCEngineImpl::IsLogin() {
	return logined_;
}

void ZegoRTCEngineImpl::onRoomStateUpdate(const std::string &roomID, ZegoRoomState state, int errorCode,
				      const std::string &extendData)
{
	if (state == ZEGO_ROOM_STATE_DISCONNECTED) {
		logined_ = false;
	} else if (state == ZEGO_ROOM_STATE_CONNECTING) {
	} else if (state == ZEGO_ROOM_STATE_CONNECTED) {
		logined_ = true;
	}
}
void ZegoRTCEngineImpl::onRoomStateChanged(const std::string &roomID, ZegoRoomStateChangedReason reason, int errorCode,
				       const std::string &extendedData)
{
	if (reason == ZEGO_ROOM_STATE_DISCONNECTED) {
	} else if (reason == ZEGO_ROOM_STATE_CONNECTING) {
	} else if (reason == ZEGO_ROOM_STATE_CONNECTED) {
	}
}
void ZegoRTCEngineImpl::onRoomExtraInfoUpdate(const std::string &roomID,
					  const std::vector<ZegoRoomExtraInfo> &roomExtraInfoList)
{
}

void ZegoRTCEngineImpl::onRoomUserUpdate(const std::string &roomID, ZegoUpdateType updateType,
				     const std::vector<ZegoUser> &userList)
{
}
void ZegoRTCEngineImpl::onRoomStreamUpdate(const std::string &roomID, ZegoUpdateType updateType,
					   const std::vector<ZegoStream> &streamList, const std::string &extendData)
{
	bool focusStreamAdd = false;
	bool focusStreamRemove = false;

	for_each(streamList.begin(), streamList.end(), [&](ZegoStream stream) {
		auto it = std::find_if(zego_stream_list_.begin(), zego_stream_list_.end(),
				       [&](ZegoStream const &_stream) { return _stream.streamID == stream.streamID; });

		if (updateType == ZEGO_UPDATE_TYPE_ADD && it == zego_stream_list_.end()) {
			zego_stream_list_.push_back(stream);
			if (focus_stream_ == stream.streamID) {
				focusStreamAdd = true;
			}
		}

		if (updateType == ZEGO_UPDATE_TYPE_DELETE && it != zego_stream_list_.end()) {
			zego_stream_list_.erase(it);
			if (focus_stream_ == stream.streamID) {
				focusStreamRemove = true;
			}
		}
	});
	if ((focusStreamAdd || focusStreamRemove) && (!focus_stream_.empty())) {
		if (focusStreamAdd) {
			engine_->startPlayingStream(focus_stream_, nullptr);
		}
		//if (focusStreamRemove) {
		//	engine_->stopPlayingStream(focus_stream_);
		//}
	} else {
		if (!play_stream_id_.empty()) {
			engine_->stopPlayingStream(play_stream_id_);
			play_stream_id_ = "";
		}
		if (zego_stream_list_.size() > 0) {
			play_stream_id_ = zego_stream_list_.at(zego_stream_list_.size() - 1).streamID;
			ZegoCanvas canvas(player_);
			if (engine_ != nullptr) {
				if (player_ == nullptr) {
					engine_->startPlayingStream(play_stream_id_, nullptr);
				} else {
					engine_->startPlayingStream(play_stream_id_, &canvas);
				}
			
			}
		}
	}
}

void ZegoRTCEngineImpl::setScenario(bool isAudio, bool isVideo, std::string focusStream, void *player)
{
	audio_enable_ = isAudio;
	video_enable_ = isVideo;
	focus_stream_ = focusStream;
	player_ = player;
}
void ZegoRTCEngineImpl::onRoomStreamExtraInfoUpdate(const std::string &roomID, const std::vector<ZegoStream> &streamList) {}

void ZegoRTCEngineImpl::onPublisherSendAudioFirstFrame(ZegoPublishChannel channel) {}
void ZegoRTCEngineImpl::onPublisherSendVideoFirstFrame(ZegoPublishChannel channel) {}



std::shared_ptr<ZegoRTCEngine> ZegoRTCEngine::Create()
{
	return std::make_shared<ZegoRTCEngineImpl>();
}
