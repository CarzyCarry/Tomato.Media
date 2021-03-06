//
// Tomato Media
// 后台音频播放器客户端
// 
// (c) SunnyCase 
// 创建日期 2015-05-11
#include "pch.h"
#include "IBackgroundMediaPlayerHandler.h"
#include "BackgroundMediaPlayerClient.h"
#include "ApplicationDataHelper.h"
#include "BackgroundMediaPlayer.h"

using namespace Platform;
using namespace NS_MEDIA;
using namespace NS_MEDIA::details;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Media;

BackgroundMediaPlayerClient::BackgroundMediaPlayerClient(Windows::UI::Xaml::Interop::TypeName mediaPlayerHandlerTypeName)
{
	// 设置处理器类型并初始化后台音频任务
	SetSetting<settings::BackgroundMediaPlayerHandlerFullNameSetting>(mediaPlayerHandlerTypeName.Name);
	AttachMessageListener();
}

void BackgroundMediaPlayerClient::AttachMessageListener()
{
	bool failed = true;
	int retryCount = 2;

	while (--retryCount >= 0)
	{
		int hr = S_OK;
		try
		{
			messageListenerToken = Playback::BackgroundMediaPlayer::MessageReceivedFromBackground += ref new EventHandler<Playback::MediaPlayerDataReceivedEventArgs ^>(
				this, &BackgroundMediaPlayerClient::OnMessageReceivedFromBackground);
			failed = false;
			break;
		}
		catch (Exception^ ex)
		{
			if (ex->HResult == RPC_S_SERVER_UNAVAILABLE)
				hr = ex->HResult;
			else
				throw;
		}
		Playback::BackgroundMediaPlayer::Shutdown();
	}
	if (failed)
		throw ref new COMException(RPC_S_SERVER_UNAVAILABLE);
}

void BackgroundMediaPlayerClient::DetachMessageListener()
{
	Playback::BackgroundMediaPlayer::MessageReceivedFromBackground -= messageListenerToken;
}

void BackgroundMediaPlayerClient::OnMessageReceivedFromBackground(Object ^ sender, Playback::MediaPlayerDataReceivedEventArgs ^ e)
{
	auto valueSet = e->Data;
	auto key = (String^)valueSet->Lookup(L"MessageId");
	if (key == BackgroundMediaPlayerActivatedMessageKey)
		PlayerActivated(this, nullptr);
	else if (key == BackgroundMediaPlayerUserMessageKey)
		MessageReceived(this, ref new MessageReceivedEventArgs((String^)e->Data->Lookup(L"MessageTag"), (String^)e->Data->Lookup(L"MessageContent")));
}

void BackgroundMediaPlayerClient::SendMessage(Platform::String^ tag, Platform::String ^ message)
{
	auto valueSet = ref new ValueSet();
	valueSet->Insert(L"MessageId", BackgroundMediaPlayerUserMessageKey);
	valueSet->Insert(L"MessageTag", tag);
	valueSet->Insert(L"MessageContent", message);

	bool failed = true;
	int retryCount = 2;

	while (--retryCount >= 0)
	{
		int hr = S_OK;
		try
		{
			Playback::BackgroundMediaPlayer::SendMessageToBackground(valueSet);
			failed = false;
			break;
		}
		catch (Exception^ ex)
		{
			if (ex->HResult == RPC_S_SERVER_UNAVAILABLE)
				hr = ex->HResult;
			else
				throw;
		}
		try
		{
			Playback::BackgroundMediaPlayer::Shutdown();
			AttachMessageListener();
		}
		catch (...)
		{
		}
	}
	if (failed)
		throw ref new COMException(RPC_S_SERVER_UNAVAILABLE);
}