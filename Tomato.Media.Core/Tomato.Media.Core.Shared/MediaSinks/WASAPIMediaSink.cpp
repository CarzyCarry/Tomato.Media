﻿//
// Tomato Media
// Media Sink
// 
// (c) SunnyCase 
// 创建日期 2015-03-14
#include "pch.h"
#include "WASAPIMediaSink.h"

using namespace NS_TOMATO;
using namespace NS_TOMATO_MEDIA;
using namespace wrl;
using namespace concurrency;

#if WINAPI_PARTITION_APP
#pragma comment(lib, "Mmdevapi.lib")

using namespace Platform;
using namespace Windows::Media::Devices;

inline double hns2s(REFERENCE_TIME time)
{
	return time * 1.0e-7;
}

class ActivateAudioInterfaceCompletionHandler : public RuntimeClass<RuntimeClassFlags<
	RuntimeClassType::ClassicCom>, FtmBase, IActivateAudioInterfaceCompletionHandler>
{
public:
	ActivateAudioInterfaceCompletionHandler()
	{
	}

	task<ComPtr<IAudioClient2>> Activate()
	{
		// Get a string representing the Default Audio Device Renderer
		auto deviceId = MediaDevice::GetDefaultAudioRenderId(AudioDeviceRole::Default);
		ComPtr<IActivateAudioInterfaceAsyncOperation> asyncOp;

		// This call must be made on the main UI thread.  Async operation will call back to
		// IActivateAudioInterfaceCompletionHandler::ActivateCompleted, which must be an agile interface implementation
		THROW_IF_FAILED(ActivateAudioInterfaceAsync(deviceId->Data(),
			__uuidof(IAudioClient2), nullptr, this, &asyncOp));
		return create_task(completionEvent);
	}

	// 通过 RuntimeClass 继承
	STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation * activateOperation) noexcept override
	{
		HRESULT hr = S_OK;

		try
		{
			ComPtr<IUnknown> audioClientUnk;
			THROW_IF_FAILED(activateOperation->GetActivateResult(&hr, &audioClientUnk));
			THROW_IF_FAILED(hr);

			ComPtr<IAudioClient2> audioClient;
			THROW_IF_FAILED(audioClientUnk.As(&audioClient));
			completionEvent.set(audioClient);
		}
		CATCH_ALL();
		return S_OK;
	}
private:
	std::function<void(ComPtr<IAudioClient2>&&)> completedHandler;
	task_completion_event<ComPtr<IAudioClient2>> completionEvent;
};
#endif

WASAPIMediaSink::WASAPIMediaSink()
	:sampleRequestEvent(CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS))
{
	startPlaybackThread = mcssProvider.CreateMMCSSThread(
		std::bind(&WASAPIMediaSink::OnStartPlayback, this));
	sampleRequestedThread = mcssProvider.CreateMMCSSThread(
		std::bind(&WASAPIMediaSink::OnSampleRequested, this));
}

task<void> WASAPIMediaSink::Initialize()
{
	if (sinkState == MediaSinkState::NotInitialized)
	{
		sinkState = MediaSinkState::Initializing;
#if WINAPI_PARTITION_APP
		auto activateHandler = Make<ActivateAudioInterfaceCompletionHandler>();
		return activateHandler->Activate()
			.then([this, activateHandler](ComPtr<IAudioClient2> audioClient)
		{
			this->audioClient = audioClient;
			ConfigureDevice();

			sinkState = MediaSinkState::Ready;
		}, task_continuation_context::use_arbitrary());
#else
#error "Not support."
#endif
	}
	return task_from_result();
}

void WASAPIMediaSink::SetMediaSourceReader(std::shared_ptr<ISourceReader> sourceReader)
{
	sourceReaderHolder = sourceReader;
	this->sourceReader = sourceReaderHolder.get();
	this->sourceReader->SetAudioFormat(deviceInputFormat.get(), GetBufferFramesPerPeriod());
}

void WASAPIMediaSink::StartPlayback()
{
	if (sinkState == MediaSinkState::Ready ||
		sinkState == MediaSinkState::Stopped)
		startPlaybackThread->Execute();
}

void WASAPIMediaSink::ConfigureDevice()
{
	AudioClientProperties audioProps = { 0 };
	audioProps.cbSize = sizeof(audioProps);
	audioProps.bIsOffload = FALSE;
	audioProps.eCategory = AudioCategory_BackgroundCapableMedia;

	THROW_IF_FAILED(audioClient->SetClientProperties(&audioProps));

	// 获取设备输入格式
	WAVEFORMATEX* waveFormat;
	THROW_IF_FAILED(audioClient->GetMixFormat(&waveFormat));
	deviceInputFormat.reset(waveFormat);

	// 获取设备周期
	THROW_IF_FAILED(audioClient->GetDevicePeriod(&hnsDefaultBufferDuration, nullptr));
	// 初始化 AudioClient
	THROW_IF_FAILED(audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
		0, 0, deviceInputFormat.get(), nullptr));
	// 获取设备缓冲大小
	THROW_IF_FAILED(audioClient->GetBufferSize(&deviceBufferFrames));
	// 获取音频渲染客户端
	THROW_IF_FAILED(audioClient->GetService(IID_PPV_ARGS(renderClient.ReleaseAndGetAddressOf())));
	// Sets the event handle that the system signals when an audio buffer is ready to be processed by the client
	THROW_IF_FAILED(audioClient->SetEventHandle(sampleRequestEvent.Get()));
}

UINT32 WASAPIMediaSink::GetBufferFramesAvailable()
{
	UINT32 paddingFrames = 0;
	// Get padding in existing buffer
	THROW_IF_FAILED(audioClient->GetCurrentPadding(&paddingFrames));

	// In non-HW shared mode, GetCurrentPadding represents the number of queued frames
	// so we can subtract that from the overall number of frames we have
	// 否则返回的是缓冲中已被使用的帧数
	return deviceBufferFrames - paddingFrames;
}

void WASAPIMediaSink::FillBufferAvailable(bool isSilent)
{
	auto framesAvailable = GetBufferFramesAvailable();
	// 没有可用空间则直接返回
	if (!framesAvailable) return;

	if (isSilent || !sourceReader)
	{
		// 用空白填充缓冲
		byte* buffer = nullptr;
		THROW_IF_FAILED(renderClient->GetBuffer(framesAvailable, &buffer));
		THROW_IF_FAILED(renderClient->ReleaseBuffer(framesAvailable, AUDCLNT_BUFFERFLAGS_SILENT));
	}
	// Even if we cancel a work item, this may still fire due to the async
	// nature of things.  There should be a queued work item already to handle
	// the process of stopping or stopped
	else if (sinkState == MediaSinkState::Playing)
		FillBufferFromMediaSource(framesAvailable);
}

void WASAPIMediaSink::FillBufferFromMediaSource(UINT32 framesCount)
{
	BYTE* data = nullptr;
	UINT32 actualBytesRead = 0;
	UINT32 actualBytesToRead = framesCount * deviceInputFormat->nBlockAlign;

	THROW_IF_FAILED(renderClient->GetBuffer(framesCount, &data));
	if (actualBytesRead = sourceReader->Read(data, actualBytesToRead))
	{
		THROW_IF_FAILED(renderClient->ReleaseBuffer((actualBytesRead / deviceInputFormat->nBlockAlign), 0));
	}
	// 未读出数据
	else
	{
		THROW_IF_FAILED(renderClient->ReleaseBuffer(framesCount, AUDCLNT_BUFFERFLAGS_SILENT));
		// 播放结束
		/*if (currentSource->GetState() == SourceReaderState::End)
			StopPlayback();*/
	}
}

size_t WASAPIMediaSink::GetBufferFramesPerPeriod()
{
	REFERENCE_TIME defaultDevicePeriod = 0;
	REFERENCE_TIME minimumDevicePeriod = 0;

	// Get the audio device period
	THROW_IF_FAILED(audioClient->GetDevicePeriod(&defaultDevicePeriod, &minimumDevicePeriod));

	// 100 ns = 1e-7 s
	double devicePeriodInSeconds = hns2s(defaultDevicePeriod);

	return static_cast<UINT32>(std::ceil(deviceInputFormat->nSamplesPerSec * devicePeriodInSeconds));
}

void WASAPIMediaSink::InitializeDeviceBuffer()
{
	FillBufferAvailable(true);
}

void WASAPIMediaSink::OnStartPlayback()
{
	InitializeDeviceBuffer();
	THROW_IF_FAILED(audioClient->Start());
	if (sourceReader)
		sourceReader->Start();
	sinkState = MediaSinkState::Playing;

	sampleRequestedThread->Execute(sampleRequestEvent);
}

void WASAPIMediaSink::OnSampleRequested()
{
	// 锁定保证同时只有一个请求被处理
	std::lock_guard<decltype(sampleRequestMutex)> locker(sampleRequestMutex);
	FillBufferAvailable(false);

	if (sinkState == MediaSinkState::Playing)
	{
		// 安排下一次采样工作
		sampleRequestedThread->Execute(sampleRequestEvent);
	}
}

MEDIA_CORE_API std::unique_ptr<IMediaSink> __stdcall NS_TOMATO_MEDIA::CreateWASAPIMediaSink()
{
	return std::make_unique<WASAPIMediaSink>();
}