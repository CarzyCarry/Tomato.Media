//
// Tomato Media
// 媒体播放器接口
// 
// 作者：SunnyCase 
// 创建日期 2015-08-05
#pragma once
#include "common.h"
#include "MediaSource.h"

DEFINE_NS_MEDIA

///<summary>媒体播放器接口</summary>
public interface class IMediaPlayer
{
	void SetMediaSource(MediaSource^ mediaSource);
	void Play();
	void Pause();

	property Windows::Media::SystemMediaTransportControls^ SystemMediaTransportControls
	{
		Windows::Media::SystemMediaTransportControls^ get();
	}

	event Windows::Foundation::TypedEventHandler<IMediaPlayer^, Platform::Object^>^ MediaOpened;
	event Windows::Foundation::TypedEventHandler<IMediaPlayer^, Platform::Object^>^ MediaEnded;
	event Windows::Foundation::TypedEventHandler<IMediaPlayer^, Windows::Media::Playback::MediaPlayerFailedEventArgs^>^ MediaFailed;
	event Windows::Foundation::TypedEventHandler<IMediaPlayer^, Platform::Object^>^ CurrentStateChanged;
};

END_NS_MEDIA