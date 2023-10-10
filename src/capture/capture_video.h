/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_CAPTURE_VIDEO_H
#define DOSBOX_CAPTURE_VIDEO_H

#include "render.h"

class VideoEncoder {
public:
	virtual void CaptureVideoAddFrame(const RenderedImage& image,
	                                  const float frames_per_second) = 0;

	virtual void CaptureVideoAddAudioData(const uint32_t sample_rate,
	                                      const uint32_t num_sample_frames,
	                                      const int16_t* sample_frames) = 0;

	virtual void CaptureVideoFinalise() = 0;

	virtual ~VideoEncoder() = default;
};

class ZmbvEncoder : public VideoEncoder {
public:
	void CaptureVideoAddFrame(const RenderedImage& image,
	                          const float frames_per_second) override;

	void CaptureVideoAddAudioData(const uint32_t sample_rate,
	                              const uint32_t num_sample_frames,
	                              const int16_t* sample_frames) override;

	void CaptureVideoFinalise() override;
};

#if C_FFMPEG

#include "capture.h"
#include "rwqueue.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

struct VideoScalerWork
{
	int64_t pts = 0;
	RenderedImage image = {};
};

struct FfmpegVideoScaler {
	RWQueue<VideoScalerWork> queue{32};
	std::thread thread = {};

	bool is_working = false;
};

struct FfmpegVideoEncoder {
	RWQueue<AVFrame*> queue{32};
	std::thread thread = {};

	const AVCodec* codec          = nullptr;
	AVCodecContext* codec_context = nullptr;

	// Accessed only in main thread, used to check if needs re-init
	// If one of these changes, create a new file
	Fraction pixel_aspect_ratio = {};
	int frames_per_second       = 0;
	uint16_t width              = 0;
	uint16_t height             = 0;

	bool is_working = false;
	bool ready_for_init = false;

	bool Init(CaptureType container);
	void Free();
	bool UpdateSettingsIfNeeded(uint16_t width, uint16_t height, Fraction pixel_aspect_ratio, int frames_per_second);
};

struct FfmpegAudioEncoder {
	RWQueue<int16_t> queue{48000};
	std::thread thread = {};

	const AVCodec* codec          = nullptr;
	AVCodecContext* codec_context = nullptr;
	AVFrame* frame                = nullptr;
	SwrContext* resampler_context = nullptr;

	// Accessed only in main thread, used to check if needs re-init
	// If sample rate changes, create a new file
	uint32_t sample_rate = 0;

	bool is_working = false;
	bool ready_for_init = false;

	bool Init();
	void Free();
};

struct FfmpegMuxer {
	RWQueue<AVPacket*> queue{64};
	std::thread thread = {};

	AVFormatContext* format_context = nullptr;

	bool is_working = false;

	// Muxer requires both video and audio encoders to be initalised first.
	bool Init(const FfmpegVideoEncoder& video_encoder,
	          const FfmpegAudioEncoder& audio_encoder,
	          const CaptureType container);
	void Free();
};

class FfmpegEncoder : public VideoEncoder {
public:
	FfmpegEncoder() = delete;
	FfmpegEncoder(const Section_prop* secprop);
	~FfmpegEncoder();
	FfmpegEncoder(const FfmpegEncoder&)            = delete;
	FfmpegEncoder& operator=(const FfmpegEncoder&) = delete;

	void CaptureVideoAddFrame(const RenderedImage& image,
	                          const float frames_per_second) override;

	void CaptureVideoAddAudioData(const uint32_t sample_rate,
	                              const uint32_t num_sample_frames,
	                              const int16_t* sample_frames) override;

	void CaptureVideoFinalise() override;

	CaptureType container = CaptureType::VideoMkv;

private:
	std::mutex mutex                 = {};
	std::condition_variable waiter   = {};
	FfmpegVideoScaler video_scaler   = {};
	FfmpegVideoEncoder video_encoder = {};
	FfmpegAudioEncoder audio_encoder = {};
	FfmpegMuxer muxer                = {};
	int64_t main_thread_video_frame  = 0;
	bool worker_threads_are_initalised = false;

	// Guarded by mutex, only set in destructor
	bool is_shutting_down = false;

	bool InitEverything();
	void FreeEverything();
	void ScaleVideo();
	void EncodeVideo();
	void EncodeAudio();
	void Mux();
	void StopQueues();
	void StartQueues();
};

#endif // C_FFMPEG
#endif // DOSBOX_CAPTURE_VIDEO_H
