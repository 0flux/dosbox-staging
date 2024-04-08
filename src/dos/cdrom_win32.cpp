/*
 *  Copyright (C) 2024-2024  The DOSBox Team
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

#if defined(WIN32)

#include "cdrom.h"

#include <windows.h>
#include <ntddcdrm.h>

static char get_drive_letter(const char* path)
{
	const size_t len = strlen(path);
	if (len < 2 || len > 3) {
		return 0;
	}
	if (path[1] != ':') {
		return 0;
	}
	const char drive_letter = toupper(path[0]);
	if (drive_letter >= 'A' && drive_letter <= 'Z') {
		return drive_letter;
	}
	return 0;
}

bool CDROM_Interface_Win32::IsOpen() const
{
	return cdrom_handle != INVALID_HANDLE_VALUE;
}

CDROM_Interface_Win32::~CDROM_Interface_Win32()
{
	if (IsOpen()) {
		CloseHandle(cdrom_handle);
	}
	cdrom_handle = INVALID_HANDLE_VALUE;
}

bool CDROM_Interface_Win32::Open(const char drive_letter)
{
	const std::string device_path = std::string("\\\\.\\") + drive_letter + ':';

	HANDLE device = CreateFileA(device_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (device == INVALID_HANDLE_VALUE) {
		return false;
	}

	// Test to make sure this device is a CDROM drive
	CDROM_TOC toc = {};
	if (!DeviceIoControl(device, IOCTL_CDROM_READ_TOC, NULL, 0, &toc, sizeof(toc), NULL, NULL)) {
		CloseHandle(device);
		return false;
	}

	if (IsOpen()) {
		CloseHandle(cdrom_handle);
	}
	cdrom_handle = device;
	return true;
}

bool CDROM_Interface_Win32::SetDevice(const char* path)
{
	const char drive_letter = get_drive_letter(path);
	if (!drive_letter) {
		return false;
	}
	if (!Open(drive_letter)) {
		return false;
	}
	InitAudio();
	return true;
}

bool CDROM_Interface_Win32::GetUPC(unsigned char& attr, std::string& upc)
{
	if (!IsOpen()) {
		return false;
	}
	CDROM_SUB_Q_DATA_FORMAT format = {};
	format.Format = IOCTL_CDROM_MEDIA_CATALOG;
	SUB_Q_CHANNEL_DATA data        = {};
	if (!DeviceIoControl(cdrom_handle, IOCTL_CDROM_READ_Q_CHANNEL, &format, sizeof(format), &data, sizeof(data), NULL, NULL)) {
		return false;
	}
	if (!data.MediaCatalog.Mcval) {
		return false;
	}
	attr = 0;
	upc = (const char *)data.MediaCatalog.MediaCatalog;
	return true;
}

bool CDROM_Interface_Win32::GetAudioTracks(uint8_t& stTrack, uint8_t& end, TMSF& leadOut)
{
	if (!IsOpen()) {
		return false;
	}
	CDROM_TOC toc = {};
	if (!DeviceIoControl(cdrom_handle, IOCTL_CDROM_READ_TOC, NULL, 0, &toc, sizeof(toc), NULL, NULL)) {
		return false;
	}
	if (toc.LastTrack >= MAXIMUM_NUMBER_TRACKS) {
		return false;
	}
	stTrack = toc.FirstTrack;
	end     = toc.LastTrack;
	leadOut.min = toc.TrackData[toc.LastTrack].Address[1];
	leadOut.sec = toc.TrackData[toc.LastTrack].Address[2];
	leadOut.fr  = toc.TrackData[toc.LastTrack].Address[3];
	return true;
}

bool CDROM_Interface_Win32::GetAudioTrackInfo(uint8_t track, TMSF& start, unsigned char& attr)
{
	if (!IsOpen()) {
		return false;
	}
	uint8_t index = track - 1;
	if (index >= MAXIMUM_NUMBER_TRACKS) {
		return false;
	}
	CDROM_TOC toc = {};
	if (!DeviceIoControl(cdrom_handle, IOCTL_CDROM_READ_TOC, NULL, 0, &toc, sizeof(toc), NULL, NULL)) {
		return false;
	}
	start.min = toc.TrackData[index].Address[1];
	start.sec = toc.TrackData[index].Address[2];
	start.fr  = toc.TrackData[index].Address[3];
	attr = (toc.TrackData[index].Control << 4) | toc.TrackData[index].Adr;
	return true;
}

bool CDROM_Interface_Win32::GetAudioSub(unsigned char& attr, unsigned char& track,
                                        unsigned char& index, TMSF& relPos, TMSF& absPos)
{
	if (!IsOpen()) {
		return false;
	}
	CDROM_SUB_Q_DATA_FORMAT format = {};
	format.Format                  = IOCTL_CDROM_CURRENT_POSITION;
	SUB_Q_CHANNEL_DATA data        = {};
	if (!DeviceIoControl(cdrom_handle,
	                     IOCTL_CDROM_READ_Q_CHANNEL,
	                     &format,
	                     sizeof(format),
	                     &data,
	                     sizeof(data),
	                     NULL,
	                     NULL)) {
		return false;
	}
	attr = (data.CurrentPosition.Control << 4) | data.CurrentPosition.ADR;
	track = data.CurrentPosition.TrackNumber;
	index = data.CurrentPosition.IndexNumber;
	relPos.min = data.CurrentPosition.TrackRelativeAddress[1];
	relPos.sec = data.CurrentPosition.TrackRelativeAddress[2];
	relPos.fr  = data.CurrentPosition.TrackRelativeAddress[3];
	absPos.min = data.CurrentPosition.AbsoluteAddress[1];
	absPos.sec = data.CurrentPosition.AbsoluteAddress[2];
	absPos.fr  = data.CurrentPosition.AbsoluteAddress[3];
	return true;
}

bool CDROM_Interface_Win32::GetMediaTrayStatus(bool& mediaPresent, bool& mediaChanged, bool& trayOpen)
{
	mediaPresent = true;
	mediaChanged = false;
	trayOpen     = false;
	return true;
}

// TODO: Find a test case and implement these.
// LaserLock copy protection needs this but that needs more work to implment.
// LaserLock currently does not work with CDROM_Interface_Image or CDROM_Interface_Ioctl either which does implement these.
// I could not find any other game that uses this.
bool CDROM_Interface_Win32::ReadSectors(PhysPt buffer, const bool raw, const uint32_t sector, const uint16_t num)
{
	return false;
}

bool CDROM_Interface_Win32::ReadSectorsHost(void* buffer, bool raw, unsigned long sector, unsigned long num)
{
	return false;
}

bool CDROM_Interface_Win32::LoadUnloadMedia(bool unload)
{
	if (!IsOpen()) {
		return false;
	}
	DWORD control_code = unload ? IOCTL_STORAGE_EJECT_MEDIA : IOCTL_CDROM_LOAD_MEDIA;
	return DeviceIoControl(cdrom_handle, control_code, NULL, 0, NULL, 0, NULL, NULL);
}

std::vector<int16_t> CDROM_Interface_Win32::ReadAudio(uint32_t sector, uint32_t num_frames)
{
	constexpr uint32_t MaximumFramesPerCall = 55;
	num_frames = std::min(num_frames, MaximumFramesPerCall);

	std::vector<int16_t> audio_frames(num_frames * SAMPLES_PER_REDBOOK_FRAME);

	RAW_READ_INFO read_info = {};
	read_info.DiskOffset.LowPart = sector * 2048;
	read_info.SectorCount = num_frames;
	read_info.TrackMode = CDDA;

	DeviceIoControl(cdrom_handle, IOCTL_CDROM_RAW_READ, &read_info, sizeof(read_info), audio_frames.data(), audio_frames.size() * sizeof(int16_t), NULL, NULL);

	return audio_frames;
}

#endif // WIN32
