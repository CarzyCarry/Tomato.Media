//
// Tomato Media
// 视频流渲染 Sink
// 
// 作者：SunnyCase 
// 创建日期 2015-08-07
#pragma once
#include "IVideoRender.h"
#include "StreamRenderSinkBase.h"
#include "Utility/MFWorkerQueueProvider.h"
#include <atomic>

DEFINE_NS_MEDIA

///<summary>视频流 Sink</summary>
class VideoStreamRenderSink : public StreamRenderSinkBase
{
	// Sink 状态
	enum VideoStreamRenderSinkState
	{
		// 未加载（未设置媒体类型）
		NotInitialized,
		// 已加载（已设置媒体类型，且 Flush 后）
		Initialized,
		// 缓冲中
		Prerolling,
		// 准备完毕（缓冲完毕）
		Ready,
	};
public:
	VideoStreamRenderSink(DWORD identifier, MediaRenderSink* mediaSink, IVideoRender* videoRender);

	STDMETHODIMP GetMediaTypeHandler(IMFMediaTypeHandler ** ppHandler) override;
	STDMETHODIMP ProcessSample(IMFSample * pSample) override;
	STDMETHODIMP PlaceMarker(MFSTREAMSINK_MARKER_TYPE eMarkerType, const PROPVARIANT * pvarMarkerValue, const PROPVARIANT * pvarContextValue) override;
	STDMETHODIMP Flush(void) override;

	STDMETHODIMP IsMediaTypeSupported(IMFMediaType * pMediaType, IMFMediaType ** ppMediaType) override;
	STDMETHODIMP GetMediaTypeCount(DWORD * pdwTypeCount) override;
	STDMETHODIMP GetMediaTypeByIndex(DWORD dwIndex, IMFMediaType ** ppType) override;
	STDMETHODIMP SetCurrentMediaType(IMFMediaType * pMediaType) override;
	STDMETHODIMP GetCurrentMediaType(IMFMediaType ** ppMediaType) override;
	STDMETHODIMP GetMajorType(GUID * pguidMajorType) override;

#if (WINVER >= _WIN32_WINNT_WIN8)
	STDMETHODIMP RegisterThreadsEx(DWORD * pdwTaskIndex, LPCWSTR wszClassName, LONG lBasePriority) override;
	STDMETHODIMP SetWorkQueueEx(DWORD dwMultithreadedWorkQueueId, LONG lWorkItemBasePriority) override;
#elif (WINVER >= _WIN32_WINNT_VISTA) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
	STDMETHODIMP RegisterThreads(DWORD dwTaskIndex, LPCWSTR wszClass) override;
	STDMETHODIMP SetWorkQueue(DWORD dwWorkQueueId) override;
#endif
	STDMETHODIMP UnregisterThreads(void) override;

	virtual void NotifyPreroll(MFTIME hnsUpcomingStartTime) override;
private:
	void OnSetMediaType();
	///<param name="setInited">是否设置状态为 Initialized。</param>
	///<remarks>调用前需对状态加锁</remarks>
	void FlushCore(bool setInited = false);

	///<remarks>调用前需对状态加锁</remarks>
	void PostSampleRequest();

	///<remarks>调用前需对状态加锁</remarks>
	void PostSampleRequestIfNeeded();

	///<remarks>调用前不能对状态加锁</remarks>
	void OnProcessIncomingSamples(IMFSample* sample);

	///<remarks>调用前需对状态加锁</remarks>
	void RegisterWorkThreadIfNeeded();

	///<summary>将缓存的采样解码为帧</summary>
	///<remarks>调用前不能对状态加锁</remarks>
	void OnDecodeFrame();
private:
	UINT32 frameWidth, frameHeight;
	WRL::ComPtr<IVideoRender> videoRender;
	WRL::ComPtr<IMFMediaType> mediaType;
	std::mutex stateMutex;
	VideoStreamRenderSinkState sinkState = NotInitialized;
	std::queue<WRL::ComPtr<IMFSample>> sampleCache;
	std::queue<Frame> frameCache;
	std::mutex sampleCacheMutex;
	std::mutex frameCacheMutex;
	MFWorkerQueueProviderRef workerQueue;
	bool workThreadRegistered = false;

	std::shared_ptr<WorkerThread> decodeFrameWorker;
};

END_NS_MEDIA