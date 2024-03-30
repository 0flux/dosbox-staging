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

#ifndef DOSBOX_CAPTURE_H
#define DOSBOX_CAPTURE_H

#include "render.h"

#include "std_filesystem.h"

#include <optional>
#include <string>

enum class CaptureType {
	Audio,
	Midi,
	RawOplStream,
	RadOplInstruments,
	Video,
	RawImage,
	UpscaledImage,
	RenderedImage,
	SerialLog,
	ParallelLog
};

enum class CaptureState { Off, Pending, InProgress };

void CAPTURE_AddConfigSection(const config_ptr_t& conf);

// TODO move raw OPL and serial log capture into the capture module too

// Create a new empty capture file of the request type. If `path` is not
// provided, the filename is autogenerated via the standard capture naming
// scheme.
FILE* CAPTURE_CreateFile(const CaptureType type,
                         const std::optional<std_fs::path>& path = {});

// Used to add the last rendered frame to be captured either as a screenshot
// or as a video recording (or both).
void CAPTURE_AddFrame(const RenderedImage& image, const float frames_per_second);

void CAPTURE_AddPostRenderImage([[maybe_unused]] const RenderedImage& image);

// Used to add the last rendered chunk of audio output to be captured either
// as an audio recording or the audio stream of a video recording (or both).
void CAPTURE_AddAudioData(const uint32_t sample_rate, const uint32_t num_sample_frames,
                          const int16_t* sample_frames);

void CAPTURE_AddMidiData(const bool sysex, const size_t len, const uint8_t* data);

void CAPTURE_StartVideoCapture();
void CAPTURE_StopVideoCapture();

bool CAPTURE_IsCapturingAudio();
bool CAPTURE_IsCapturingImage();
bool CAPTURE_IsCapturingPostRenderImage();
bool CAPTURE_IsCapturingMidi();
bool CAPTURE_IsCapturingVideo();

// Only used internally in the capture module
int32_t get_next_capture_index(const CaptureType type);

std_fs::path generate_capture_filename(const CaptureType type, const int32_t index);

#endif // DOSBOX_CAPTURE_H
