/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
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

// Uncomment to enable file-open diagnostic messages
// #define DEBUG 1

#include "drives.h"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <ctime>
#include <limits>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifdef _MSC_VER
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#include "dos_inc.h"
#include "dos_mscdex.h"
#include "fs_utils.h"
#include "string_utils.h"
#include "cross.h"
#include "inout.h"

bool localDrive::FileCreate(DOS_File** file, char* name, FatAttributeFlags attributes)
{
	// Don't allow overwriting read-only files.
	FatAttributeFlags test_attr = {};
	if (GetFileAttr(name, &test_attr) && test_attr.read_only) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);

#ifdef BOXER_APP
	// --Added 2010-01-18 by Alun Bestor to allow Boxer to selectively deny write access to files
	if (!boxer_shouldAllowWriteAccessToPath((const char *)newname, this)) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
#endif	// BOXER_APP

	// GetExpandNameAndNormaliseCase returns a pointer to a static local
	// string. Make a copy to ensure it doesn't get overwritten by future
	// calls.
	char expanded_name[CROSS_LEN];
	safe_strcpy(expanded_name, dirCache.GetExpandNameAndNormaliseCase(newname));

	const bool file_exists = FileExists(expanded_name);

	attributes.archive = true;
#ifdef BOXER_APP
	// -- Modified 2012-07-24 by Alun Bestor to allow Boxer to shadow local file access
	FILE* file_pointer = boxer_openLocalFile(this, expanded_name, attributes);
#else	// NOT BOXER_APP
	FILE* file_pointer = local_drive_create_file(expanded_name, attributes);
#endif	// BOXER_APP
	if (!file_pointer) {
		LOG_MSG("Warning: file creation failed: %s", expanded_name);
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	if (!file_exists) {
		safe_strcpy(newname, basedir);
		safe_strcat(newname, name);
		CROSS_FILENAME(newname);
		dirCache.AddEntry(newname, true);
	}

	// Make the 16 bit device information
	*file = new localFile(name, expanded_name, file_pointer, basedir);

	(*file)->flags = OPEN_READWRITE;

#ifdef BOXER_APP
	// --Added 2010-08-21 by Alun Bestor to let Boxer monitor DOSBox's file operations
	boxer_didCreateLocalFile(temp_name, this);
#endif	// BOXER_APP

	return true;
}

bool localDrive::IsFirstEncounter(const std::string& filename)
{
	const auto ret = write_protected_files.insert(filename);

	const bool was_inserted = ret.second;
	return was_inserted;
}

// Search the Files[] inventory for an open file matching the requested
// local drive and file name. Returns nullptr if not found.
DOS_File *FindOpenFile(const DOS_Drive *drive, const char *name)
{
	if (!drive || !name)
		return nullptr;

	uint8_t drive_num = DOS_DRIVES; // default out range
	// Look for a matching drive mount
	for (uint8_t i = 0; i < DOS_DRIVES; ++i) {
		if (Drives[i] && Drives[i] == drive) {
			drive_num = i;
			break;
		}
	}
	if (drive_num == DOS_DRIVES) // still out of range, no matching mount
		return nullptr;

	// Look for a matching open filename on the same mount
	for (auto *file : Files)
		if (file && file->IsOpen() && file->GetDrive() == drive_num && file->IsName(name))
			return file; // drive + file path is unique

	return nullptr;
}

bool localDrive::FileOpen(DOS_File **file, char *name, uint32_t flags)
{
	const char *type = nullptr;
	switch (flags&0xf) {
	case OPEN_READ:        type = "rb" ; break;
	case OPEN_WRITE:       type = "rb+"; break;
	case OPEN_READWRITE:   type = "rb+"; break;
	case OPEN_READ_NO_MOD: type = "rb" ; break; //No modification of dates. LORD4.07 uses this
	default:
		DOS_SetError(DOSERR_ACCESS_CODE_INVALID);
		return false;
	}

	// Don't allow opening read-only files in write mode,
	// unless configured otherwise
	FatAttributeFlags test_attr = {};
	if (!always_open_ro_files &&
	    ((flags & 0xf) == OPEN_WRITE || (flags & 0xf) == OPEN_READWRITE) &&
	    (GetFileAttr(name, &test_attr) && test_attr.read_only)) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandNameAndNormaliseCase(newname);

#ifdef BOXER_APP
	// --Added 2010-01-18 by Alun Bestor to allow Boxer to selectively deny write access to files
	if (!strcmp(type, "rb+")) {
		if (!boxer_shouldAllowWriteAccessToPath((const char*)newname, this)) {
			// Copy-pasted from cdromDrive::FileOpen
			if ((flags & 0xf) == OPEN_READWRITE) {
				flags &= ~static_cast<unsigned>(OPEN_READWRITE);
			} else if ((flags & 0xf) == OPEN_WRITE) {
				DOS_SetError(DOSERR_ACCESS_DENIED);
				return false;
			}			
		}
	}
#endif // BOXER_APP

	// If the file's already open then flush it before continuing
	// (Betrayal in Antara)
	auto open_file = dynamic_cast<localFile *>(FindOpenFile(this, name));
	if (open_file)
		open_file->Flush();

#ifdef BOXER_APP
	// -- Modified 2012-07-24 by Alun Bestor to allow Boxer to shadow local file access
	FILE* fhandle = boxer_openLocalFile(this, newname, type);
#else	// NOT BOXER_APP
	FILE* fhandle = fopen(newname, type);
#endif	// BOXER_APP

#ifdef DEBUG
	std::string open_msg;
	std::string flags_str;
	switch (flags & 0xf) {
		case OPEN_READ:        flags_str = "R";  break;
		case OPEN_WRITE:       flags_str = "W";  break;
		case OPEN_READWRITE:   flags_str = "RW"; break;
		case OPEN_READ_NO_MOD: flags_str = "RN"; break;
		default:               flags_str = "--";
	}
#endif

	// If we couldn't open the file, then it's possible that
	// the file is simply write-protected and the flags requested
	// RW access.  So check if this is the case:
	if (!fhandle && flags & (OPEN_READWRITE | OPEN_WRITE)) {
		// If yes, check if the file can be opened with Read-only access:
#ifdef BOXER_APP
		// -- Modified 2012-07-24 by Alun Bestor to allow Boxer to shadow local file access
		fhandle = boxer_openLocalFile(this, newname, "rb");
#else	// NOT BOXER_APP
		fhandle = fopen(newname, "rb");
#endif	// BOXER_APP
		if (fhandle) {
			if (!always_open_ro_files) {
				fclose(fhandle);
				fhandle = nullptr;
			}

#ifdef DEBUG
			if (always_open_ro_files) {
				open_msg = "wanted writes but opened read-only";
			} else {
				open_msg = "wanted writes but file is read-only";
			}
#else
			// Inform the user that the file is being protected against modification.
			// If the DOS program /really/ needs to write to the file, it will
			// crash/exit and this will be one of the last messages on the screen,
			// so the user can decide to un-write-protect the file if they wish.
			// We only print one message per file to eliminate redundant messaging.
			if (IsFirstEncounter(newname)) {
				// For brevity and clarity to the user, we show just the
				// filename instead of the more cluttered absolute path.
				LOG_MSG("FILESYSTEM: protected from modification: %s",
				        get_basename(newname).c_str());
			}
#endif
		}

#ifdef DEBUG
		else {
			open_msg += "failed desired and with read-only";
		}
#endif
	}

#ifdef DEBUG
	else if (!fhandle) {
		open_msg = "failed with desired flags";
	} else {
		open_msg = "succeeded with desired flags";
	}
	LOG_MSG("FILESYSTEM: flags=%2s, %-12s %s",
	        flags_str.c_str(),
	        get_basename(newname).c_str(),
	        open_msg.c_str());
#endif

	if (!fhandle) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}

	*file = new localFile(name, newname, fhandle, basedir);
	(*file)->flags = flags;  // for the inheritance flag and maybe check for others.

	return true;
}

FILE* localDrive::GetSystemFilePtr(const char* const name, const char* const type)
{
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandNameAndNormaliseCase(newname);

#ifdef BOXER_APP
	// -- Modified 2012-07-24 by Alun Bestor to allow Boxer to shadow local file access
	return boxer_openLocalFile(this, newname, type);
#else	// NOT BOXER_APP
	return fopen(newname,type);
#endif	// BOXER_APP
}

bool localDrive::GetSystemFilename(char* sysName, const char* const dosName)
{
	strcpy(sysName, basedir);
	strcat(sysName, dosName);
	CROSS_FILENAME(sysName);
	dirCache.ExpandNameAndNormaliseCase(sysName);
	return true;
}

// Attempt to delete the file name from our local drive mount
bool localDrive::FileUnlink(char* name)
{
	if (!FileExists(name)) {
		LOG_DEBUG("FS: Skipping removal of '%s' because it doesn't exist",
		          name);
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}

	// Don't allow deleting read-only files.
	FatAttributeFlags test_attr = {};
	if (GetFileAttr(name, &test_attr) && test_attr.read_only) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	const char* fullname = dirCache.GetExpandNameAndNormaliseCase(newname);

#ifdef BOXER_APP
	// --Added 2010-12-29 by Alun Bestor to let Boxer selectively prevent file operations
	if (!boxer_shouldAllowWriteAccessToPath((const char *)fullname, this)) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
#endif	// BOXER_APP

	// Can we remove the file without issue?
#ifdef BOXER_APP
	// -- Modified 2012-07-24 by Alun Bestor to allow Boxer to shadow local file access
	if (boxer_removeLocalFile(this, fullname)) {
#else	// NOT BOXER_APP
	if (remove(fullname) == 0) {
#endif	// BOXER_APP
		dirCache.DeleteEntry(newname);
#ifdef BOXER_APP
		// --Added 2010-08-21 by Alun Bestor to let Boxer monitor DOSBox's file operations
		boxer_didRemoveLocalFile(fullname, this);
#endif	// BOXER_APP
		return true;
	}

	// Otherwise maybe the file's opened within our mount ...
	DOS_File *open_file = FindOpenFile(this, name);
	if (open_file) {
		size_t max = DOS_FILES;
		// then close and remove references (as many times as needed),
		while (open_file->IsOpen() && --max) {
			open_file->Close();
			if (open_file->RemoveRef() <= 0) {
				break;
			}
		}
		// and try removing it again.
#ifdef BOXER_APP
		// -- Modified 2012-07-24 by Alun Bestor to allow Boxer to shadow local file access
		if (boxer_removeLocalFile(this, fullname)) {
#else	// NOT BOXER_APP
		if (remove(fullname) == 0) {
#endif	// BOXER_APP
			dirCache.DeleteEntry(newname);
#ifdef BOXER_APP
			// --Added 2010-08-21 by Alun Bestor to let Boxer monitor DOSBox's file operations
			boxer_didRemoveLocalFile(fullname, this);
#endif	// BOXER_APP
			return true;
		}
	}
	LOG_DEBUG("FS: Unable to remove file '%s'", fullname);
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool localDrive::FindFirst(char* _dir, DOS_DTA& dta, bool fcb_findfirst)
{
	char tempDir[CROSS_LEN];
	safe_strcpy(tempDir, basedir);
	safe_strcat(tempDir, _dir);
	CROSS_FILENAME(tempDir);

	if (allocation.mediaid == 0xF0) {
		EmptyCache(); //rescan floppie-content on each findfirst
	}

	// End the temp directory with a slash
	const auto temp_dir_len = strlen(tempDir);
	if (temp_dir_len < 1 || tempDir[temp_dir_len - 1] != CROSS_FILESPLIT) {
		constexpr char end[] = {CROSS_FILESPLIT, '\0'};
		safe_strcat(tempDir, end);
	}

	uint16_t id;
	if (!dirCache.FindFirst(tempDir,id)) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	safe_strcpy(srchInfo[id].srch_dir, tempDir);
	dta.SetDirID(id);

	FatAttributeFlags search_attr = {};
	dta.GetSearchParams(search_attr, tempDir);

	if (this->isRemote() && this->isRemovable()) {
		// cdroms behave a bit different than regular drives
		if (search_attr == FatAttributeFlags::Volume) {
			dta.SetResult(dirCache.GetLabel(), 0, 0, 0, FatAttributeFlags::Volume);
			return true;
		}
	} else {
		if (search_attr == FatAttributeFlags::Volume) {
			if (is_empty(dirCache.GetLabel())) {
				// LOG(LOG_DOSMISC,LOG_ERROR)("DRIVELABEL REQUESTED: none present, returned  NOLABEL");
				// dta.SetResult("NO_LABEL",0,0,0,FatAttributeFlags::Volume);
				// return true;
				DOS_SetError(DOSERR_NO_MORE_FILES);
				return false;
			}
			dta.SetResult(dirCache.GetLabel(), 0, 0, 0, FatAttributeFlags::Volume);
			return true;
		} else if (search_attr.volume && (*_dir == 0) && !fcb_findfirst) {
			// should check for a valid leading directory instead of
			// 0 exists==true if the volume label matches the
			// searchmask and the path is valid
			if (WildFileCmp(dirCache.GetLabel(), tempDir)) {
				dta.SetResult(dirCache.GetLabel(),
				              0,
				              0,
				              0,
				              FatAttributeFlags::Volume);
				return true;
			}
		}
	}
	return FindNext(dta);
}

bool localDrive::FindNext(DOS_DTA& dta)
{
	char* dir_ent;
	struct stat stat_block;
	char full_name[CROSS_LEN];
	char dir_entcopy[CROSS_LEN];

	FatAttributeFlags search_attr = {};
	char search_pattern[DOS_NAMELENGTH_ASCII];

	dta.GetSearchParams(search_attr, search_pattern);
	uint16_t id = dta.GetDirID();

	while (true) {
		if (!dirCache.FindNext(id, dir_ent)) {
			DOS_SetError(DOSERR_NO_MORE_FILES);
			return false;
		}
		if (!WildFileCmp(dir_ent, search_pattern)) {
			continue;
		}

		safe_strcpy(full_name, srchInfo[id].srch_dir);
		safe_strcat(full_name, dir_ent);

		// GetExpandNameAndNormaliseCase might indirectly destroy
		// dir_ent (by caching in a new directory and due to its design
		// dir_ent might be lost.) Copying dir_ent first
		safe_strcpy(dir_entcopy, dir_ent);
		const char* temp_name = dirCache.GetExpandNameAndNormaliseCase(
		        full_name);
#ifdef BOXER_APP
		// --Modified 2012-07-24 by Alun Bestor to wrap local file operations
		if (!boxer_getLocalPathStats(this, temp_name, &stat_block)) {
#else // NOT BOXER_APP
		if (stat(temp_name, &stat_block) != 0) {
#endif	// BOXER_APP
			continue; // No symlinks and such
		}

		if (is_hidden_by_host(temp_name)) {
			continue; // No host-only hidden files
		}

		FatAttributeFlags find_attr = {};
		if (DOSERR_NONE != local_drive_get_attributes(temp_name, find_attr)) {
			continue;
		}

		if ((find_attr.directory && !search_attr.directory) ||
		    (find_attr.hidden && !search_attr.hidden) ||
		    (find_attr.system && !search_attr.system)) {
			continue;
		}

		/*file is okay, setup everything to be copied in DTA Block */
		char find_name[DOS_NAMELENGTH_ASCII] = "";
		uint16_t find_date;
		uint16_t find_time;
		uint32_t find_size;

		if (safe_strlen(dir_entcopy) < DOS_NAMELENGTH_ASCII) {
			safe_strcpy(find_name, dir_entcopy);
			upcase(find_name);
		}

		find_size = (uint32_t)stat_block.st_size;
		struct tm datetime;
		if (cross::localtime_r(&stat_block.st_mtime, &datetime)) {
			find_date = DOS_PackDate(datetime);
			find_time = DOS_PackTime(datetime);
		} else {
			find_time = 6;
			find_date = 4;
		}
		dta.SetResult(find_name,
		              find_size,
		              find_date,
		              find_time,
		              find_attr._data);
		return true;
	}
	return false;
}

bool localDrive::GetFileAttr(char* name, FatAttributeFlags* attr)
{
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandNameAndNormaliseCase(newname);

#ifdef BOXER_APP
	// --Modified 2012-07-24 by Alun Bestor to wrap local file operations
	struct stat status;
	if (boxer_getLocalPathStats(this, newname, &status)) {
		*attr = DOS_ATTR_ARCHIVE;
		if(status.st_mode & S_IFDIR) *attr |= DOS_ATTR_DIRECTORY;
		return true;
	}

	// The caller is responsible to act accordingly, possibly
	// it should set DOS error code (setting it here is not allowed)
	*attr = 0;
	return false;
#else	// NOT BOXER_APP
	if (local_drive_get_attributes(newname, *attr) != DOSERR_NONE) {
		// The caller is responsible to act accordingly, possibly
		// it should set DOS error code (setting it here is not allowed)
		*attr = 0;
		return false;
	}

	return true;
#endif	// BOXER_APP
}

bool localDrive::SetFileAttr(const char* name, const FatAttributeFlags attr)
{
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandNameAndNormaliseCase(newname);

	const auto result = local_drive_set_attributes(newname, attr);
	dirCache.CacheOut(newname);

	if (result != DOSERR_NONE) {
		DOS_SetError(result);
		return false;
	}

	return true;
}

bool localDrive::MakeDir(char* dir)
{
	char newdir[CROSS_LEN];
	safe_strcpy(newdir, basedir);
	safe_strcat(newdir, dir);
	CROSS_FILENAME(newdir);

#ifdef BOXER_APP
	// --Modified 2010-12-29 by Alun Bestor to allow Boxer to selectively prevent file operations,
	// and to prevent DOSBox from creating folders with the wrong file permissions.
	char *fullname = dirCache.GetExpandName(newdir);

	if (!boxer_shouldAllowWriteAccessToPath((const char *)fullname, this)) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	bool created = boxer_createLocalDir(fullname, this);
	if (created) dirCache.CacheOut(newdir, true);
	return created;
#else	// NOT BOXER_APP
	const auto result = local_drive_create_dir(
	        dirCache.GetExpandNameAndNormaliseCase(newdir));
	if (result == DOSERR_NONE) {
		dirCache.CacheOut(newdir, true);
	}
	return (result == DOSERR_NONE);
#endif	// BOXER_APP
}

bool localDrive::RemoveDir(char* dir)
{
	char newdir[CROSS_LEN];
	safe_strcpy(newdir, basedir);
	safe_strcat(newdir, dir);
	CROSS_FILENAME(newdir);
#ifdef BOXER_APP
	// --Modified 2010-12-29 by Alun Bestor to allow Boxer to selectively prevent file operations
	char *fullname = dirCache.GetExpandName(newdir);

	if (!boxer_shouldAllowWriteAccessToPath((const char *)fullname, this)) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	bool removed = boxer_removeLocalDir(fullname, this);
	if (removed) dirCache.DeleteEntry(newdir, true);
	return removed;
#else	// NOT BOXER_APP
	int temp = rmdir(dirCache.GetExpandNameAndNormaliseCase(newdir));
	if (temp==0) dirCache.DeleteEntry(newdir,true);
	return (temp==0);
#endif	// BOXER_APP
}

bool localDrive::TestDir(char* dir)
{
	char newdir[CROSS_LEN];
	safe_strcpy(newdir, basedir);
	safe_strcat(newdir, dir);
	CROSS_FILENAME(newdir);
	dirCache.ExpandNameAndNormaliseCase(newdir);
#ifdef BOXER_APP
	// --Modified 2012-04-27 by Alun Bestor to wrap local file operations
	return boxer_localDirectoryExists(newdir, this);
#else	// NOT BOXER_APP
	// Skip directory test, if "\"
	size_t len = safe_strlen(newdir);
	if (len && (newdir[len-1]!='\\')) {
		// It has to be a directory !
		struct stat test;
		if (stat(newdir,&test))			return false;
		if ((test.st_mode & S_IFDIR)==0)	return false;
	};
	return path_exists(newdir);
#endif	// BOXER_APP
}

bool localDrive::Rename(char* oldname, char* newname)
{
	char newold[CROSS_LEN];
	safe_strcpy(newold, basedir);
	safe_strcat(newold, oldname);
	CROSS_FILENAME(newold);
	dirCache.ExpandNameAndNormaliseCase(newold);

	char newnew[CROSS_LEN];
	safe_strcpy(newnew, basedir);
	safe_strcat(newnew, newname);
	CROSS_FILENAME(newnew);
#ifdef BOXER_APP
	// --Modified 2012-04-27 by Alun Bestor to wrap local file operations
	char *fullname = dirCache.GetExpandName(newnew);
	if (!boxer_shouldAllowWriteAccessToPath((const char *)newold, this) ||
		!boxer_shouldAllowWriteAccessToPath((const char *)fullname, this)) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	bool moved = boxer_moveLocalFile(newold, fullname, this);
	if (moved) dirCache.CacheOut(newnew);
	return moved;
#else	// NOT BOXER_APP
	int temp = rename(newold, dirCache.GetExpandNameAndNormaliseCase(newnew));
	if (temp==0) dirCache.CacheOut(newnew);
	return (temp == 0);
#endif	// BOXER_APP
}

bool localDrive::AllocationInfo(uint16_t* _bytes_sector, uint8_t* _sectors_cluster,
                                uint16_t* _total_clusters, uint16_t* _free_clusters)
{
	*_bytes_sector    = allocation.bytes_sector;
	*_sectors_cluster = allocation.sectors_cluster;
	*_total_clusters  = allocation.total_clusters;
	*_free_clusters   = allocation.free_clusters;
	return true;
}

bool localDrive::FileExists(const char* name)
{
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandNameAndNormaliseCase(newname);
	struct stat temp_stat;
#ifdef BOXER_APP
	// --Modified 2012-04-27 by Alun Bestor to wrap local file operations
	if (!boxer_getLocalPathStats(newname,&temp_stat)!=0) return false;
#else	// NOT BOXER_APP
	if (stat(newname,&temp_stat)!=0) return false;
#endif	// BOXER_APP
	if (temp_stat.st_mode & S_IFDIR) return false;
	return true;
}

bool localDrive::FileStat(const char* name, FileStat_Block* const stat_block)
{
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandNameAndNormaliseCase(newname);
	struct stat temp_stat;

	FatAttributeFlags attributes = {};
#if BOXER_APP
	//--Modified 2012-04-27 by Alun Bestor to wrap local file operations
	if (!boxer_getLocalPathStats(newname, this, &temp_stat) ||
#else	// BOXER_APP
	if (stat(newname, &temp_stat) != 0 ||
#endif	// BOXER_APP
	    local_drive_get_attributes(newname, attributes) != DOSERR_NONE) {
		return false;
	}

	/* Convert the stat to a FileStat */
	stat_block->attr = attributes._data;
	struct tm datetime;
	if (cross::localtime_r(&temp_stat.st_mtime, &datetime)) {
		stat_block->time = DOS_PackTime(datetime);
		stat_block->date = DOS_PackDate(datetime);
	} else {
		LOG_MSG("FS: error while converting date in: %s", name);
	}
	stat_block->size=(uint32_t)temp_stat.st_size;
	return true;
}

uint8_t localDrive::GetMediaByte(void)
{
	return allocation.mediaid;
}

bool localDrive::isRemote(void)
{
	return false;
}

bool localDrive::isRemovable(void)
{
	return false;
}

Bits localDrive::UnMount()
{
	return 0;
}

/* helper functions for drive cache */
#ifdef BOXER_APP
// --Modified 2012-07-25 by Alun Bestor to wrap local filesystem access
void* localDrive::open_directory_vfunc(const char* name) {
	return boxer_openLocalDirectory(name, this);
}

void localDrive::close_directory_vfunc(void* handle) {
	boxer_closeLocalDirectory(handle);
}

bool localDrive::read_directory_first_vfunc(void* handle, char* entry_name, bool& is_directory) {
	return boxer_getNextDirectoryEntry(handle, entry_name, is_directory);
}

bool localDrive::read_directory_next_vfunc(void* handle, char* entry_name, bool& is_directory) {
	return boxer_getNextDirectoryEntry(handle, entry_name, is_directory);
}
#else	// NOT BOXER_APP
void* localDrive::open_directory_vfunc(const char* name) {
	return open_directory(name);
}

void localDrive::close_directory_vfunc(void* handle) {
	// close_directory((dir_information*)handle);
	close_directory(static_cast<dir_information*>(handle));
}

bool localDrive::read_directory_first_vfunc(void* handle, char* entry_name, bool& is_directory) {
	// return read_directory_first((dir_information*)handle, entry_name, is_directory);
	return read_directory_first(static_cast<dir_information*>(handle), entry_name, is_directory);
}

bool localDrive::read_directory_next_vfunc(void* handle, char* entry_name, bool& is_directory) {
	// return read_directory_next((dir_information*)handle, entry_name, is_directory);
	return read_directory_next(static_cast<dir_information*>(handle), entry_name, is_directory);
}
#endif	// BOXER_APP

localDrive::localDrive(const char* startdir, uint16_t _bytes_sector,
                       uint8_t _sectors_cluster, uint16_t _total_clusters,
                       uint16_t _free_clusters, uint8_t _mediaid,
                       bool _always_open_ro_files)
		: dirCache(),
		  always_open_ro_files(_always_open_ro_files),
		  write_protected_files{},
		  allocation{_bytes_sector, _sectors_cluster, _total_clusters, _free_clusters, _mediaid}
{
	type = DosDriveType::Local;
	safe_strcpy(basedir, startdir);
	safe_strcpy(info, startdir);
#ifdef BOXER_APP
	// --Added 2009-10-25 by Alun Bestor to allow Boxer to track the system path for DOSBox drives
	strcpy(systempath, startdir);
#endif	// BOXER_APP
	dirCache.SetBaseDir(basedir, this);
}

// Updates the internal file's current position
bool localFile::ftell_and_check()
{
	if (!fhandle)
		return false;

	stream_pos = ftell(fhandle);
	if (stream_pos >= 0)
		return true;

	LOG_DEBUG("FS: Failed obtaining position in file '%s'", name.c_str());
	return false;
}

// Seeks the internal file to the specified position relative to whence
bool localFile::fseek_to_and_check(long pos, int whence)
{
	if (!fhandle)
		return false;

	if (fseek(fhandle, pos, whence) == 0) {
		stream_pos = pos;
		return true;
	}
	LOG_DEBUG("FS: Failed seeking to byte %ld in file '%s'",
	          stream_pos,
	          name.c_str());
	return false;
}

// Seeks the internal file to the internal position relative to whence
void localFile::fseek_and_check(int whence)
{
	static_cast<void>(fseek_to_and_check(stream_pos, whence));
}

//TODO Maybe use fflush, but that seemed to fuck up in visual c
bool localFile::Read(uint8_t *data, uint16_t *size)
{
	// check if the file is opened in write-only mode
	if ((this->flags & 0xf) == OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

#ifdef BOXER_APP
	// --Added 2011-11-03 by Alun Bestor to avoid errors on files
	// whose backing media has disappeared
	if (!fhandle) {
		*size = 0;
		// IMPLEMENTATION NOTE: you might think we ought to return false here,
		// but no! We return true to be consistent with DOSBox's behaviour,
		// which appears to be the behaviour expected by DOS.
		return true;
	}
#endif	// BOXER_APP

	// Seek if we last wrote
	if (last_action == LastAction::Write)
		if (ftell_and_check())
			fseek_and_check(SEEK_SET);

	last_action = LastAction::Read;
	const auto requested = *size;
	const auto actual = static_cast<uint16_t>(fread(data, 1, requested, fhandle));
	*size = actual; // always save the actual

	if (actual != requested) {
		// LOG_DEBUG("FS: Only read %u of %u requested bytes from file '%s'",
		//           actual,
		//           requested,
		//           name.c_str());

		// Check for host read error
		if (ferror(fhandle)) {
			clearerr(fhandle);
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
	}

	/* Fake harddrive motion. Inspector Gadget with soundblaster compatible */
	/* Same for Igor */
	/* hardrive motion => unmask irq 2. Only do it when it's masked as
	 * unmasking is realitively heavy to emulate */
	uint8_t mask = IO_Read(0x21);
	if (mask & 0x4)
		IO_Write(0x21, mask & 0xfb);
	return true;
}

bool localFile::Write(uint8_t *data, uint16_t *size)
{
	uint32_t lastflags = this->flags & 0xf;
	if (lastflags == OPEN_READ || lastflags == OPEN_READ_NO_MOD) {	// check if file opened in read-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

#ifdef BOXER_APP
	// --Added 2011-11-03 by Alun Bestor to avoid errors on files
	// whose backing media has disappeared
	if (!fhandle) {
		*size = 0;
		// IMPLEMENTATION NOTE: you might think we ought to return false here,
		// but no! We return true to be consistent with DOSBox's behaviour,
		// which appears to be the behaviour expected by DOS.
		return true;
	}
#endif	// BOXER_APP

	// Seek if we last read
	if (last_action == LastAction::Read)
		if (ftell_and_check())
			fseek_and_check(SEEK_SET);

	last_action = LastAction::Write;
	set_archive_on_close = true;

	// Truncate the file
	if (*size == 0) {
		const auto file = cross_fileno(fhandle);
		if (file == -1) {
			LOG_DEBUG("FS: Could not resolve file number for '%s'",
			          name.c_str());
			return false;
		}
		if (!ftell_and_check()) {
			return false;
		}
		if (ftruncate(file, stream_pos) != 0) {
			LOG_DEBUG("FS: Failed truncating file '%s'", name.c_str());
			return false;
		}
		// Truncation succeeded if we made it here
		return true;
	}

	// Otherwise we have some data to write
	const auto requested = *size;
	const auto actual = static_cast<uint16_t>(fwrite(data, 1, requested, fhandle));
	if (actual != requested) {
		LOG_DEBUG("FS: Only wrote %u of %u requested bytes to file '%s'",
		          actual,
		          requested,
		          name.c_str());

		// Check for host write error
		if (ferror(fhandle)) {
			clearerr(fhandle);
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
	}
	*size = actual; // always save the actual
	return true;    // always return true, even if partially written
}

bool localFile::Seek(uint32_t *pos_addr, uint32_t type)
{
	int seektype;
	switch (type) {
	case DOS_SEEK_SET:seektype=SEEK_SET;break;
	case DOS_SEEK_CUR:seektype=SEEK_CUR;break;
	case DOS_SEEK_END:seektype=SEEK_END;break;
	default:
	//TODO Give some doserrorcode;
		return false;//ERROR
	}

#ifdef BOXER_APP
	// --Added 2011-11-03 by Alun Bestor to avoid errors on files
	// whose backing media has disappeared
	if (!fhandle) {
		*pos_addr = 0;
		// IMPLEMENTATION NOTE: you might think we ought to return false here,
		// but no! We return true to be consistent with DOSBox's behaviour,
		// which appears to be the behaviour expected by DOS.
		return true;
	}
#endif	// BOXER_APP

	// The inbound position is actually an int32_t being passed through a
	// uint32_t* pointer (pos_addr), so reinterpret the underlying memory as
	// such to prevent rollover into the unsigned range.
	const auto pos = *reinterpret_cast<int32_t *>(pos_addr);
	if (!fseek_to_and_check(pos, seektype)) {
		// Failed to seek, but try again this time seeking to
		// the end of file, which satisfies Black Thorne.
		stream_pos = 0;
		fseek_and_check(SEEK_END);
	}
#if 0
	fpos_t temppos;
	fgetpos(fhandle,&temppos);
	uint32_t * fake_pos=(uint32_t*)&temppos;
	*pos_addr = *fake_pos;
#endif
	static_cast<void>(ftell_and_check());

	// The inbound position is actually an int32_t being passed through a
	// uint32_t* pointer (pos_addr), so before we save the seeked position
	// back into it we first ensure the current long stream_pos (which is a
	// signed 64-bit on some platforms + OSes) can fit within the int32_t
	// range before assigning it.
	assert(stream_pos >= std::numeric_limits<int32_t>::min() &&
	       stream_pos <= std::numeric_limits<int32_t>::max());
	*reinterpret_cast<int32_t *>(pos_addr) = static_cast<int32_t>(stream_pos);

	last_action = LastAction::None;
	return true;
}

bool localFile::Close()
{
	bool result = true;

	// only close if one reference left
	if (refCtr == 1) {
		if (set_archive_on_close) {
			FatAttributeFlags attributes = {};
			if (DOSERR_NONE !=
			    local_drive_get_attributes(path, attributes)) {
				result = false;
			} else if (!attributes.archive) {
				attributes.archive = true;
				if (DOSERR_NONE !=
				    local_drive_set_attributes(path, attributes)) {
					result = false;
				}
			}
			set_archive_on_close = false;
		}

		if (fhandle) {
			fclose(fhandle);
		}
		fhandle = nullptr;
		open = false;
	};

	if (newtime) {
		// backport from DOS_PackDate() and DOS_PackTime()
		struct tm tim = {};
		tim.tm_sec = (time & 0x1f) * 2;
		tim.tm_min = (time >> 5) & 0x3f;
		tim.tm_hour = (time >> 11) & 0x1f;
		tim.tm_mday = date & 0x1f;
		tim.tm_mon = ((date >> 5) & 0x0f) - 1;
		tim.tm_year = (date >> 9) + 1980 - 1900;
		//  have the C run-time library code compute whether standard
		//  time or daylight saving time is in effect.
		tim.tm_isdst = -1;
		// serialize time
		mktime(&tim);

		utimbuf ftim;
		ftim.actime = ftim.modtime = mktime(&tim);

		char fullname[CROSS_LEN];
		safe_sprintf(fullname, "%s%s", basedir, name.c_str());
		CROSS_FILENAME(fullname);

		// FIXME: utime is deprecated, need a modern cross-platform
		// implementation.
		if (utime(fullname, &ftim)) {
			result = false;
		}
	}

	return result;
}

uint16_t localFile::GetInformation(void)
{
	return read_only_medium ? 0x40 : 0;
}

localFile::localFile(const char* _name, const std_fs::path& path, FILE* handle,
                     const char* _basedir)
        : fhandle(handle),
          path(path),
          basedir(_basedir)
{
	open = true;
	UpdateDateTimeFromHost();
	attr = FatAttributeFlags::Archive;

	SetName(_name);
}

bool localFile::UpdateDateTimeFromHost()
{
	if (!open)
		return false;

#ifdef BOXER_APP
	// --Added 2011-11-03 by Alun Bestor to avoid errors on closed files
	if (!fhandle) return false;
#endif	// BOXER_APP

	// Legal defaults if we're unable to populate them
	time = 1;
	date = 1;

	const auto file = cross_fileno(fhandle);
	if (file == -1)
		return true; // use defaults

	struct stat temp_stat;
	if (fstat(file, &temp_stat) == -1)
		return true; // use defaults

	struct tm datetime;
	if (!cross::localtime_r(&temp_stat.st_mtime, &datetime))
		return true; // use defaults

	time = DOS_PackTime(datetime);
	date = DOS_PackDate(datetime);
	return true;
}

void localFile::Flush()
{
	if (last_action != LastAction::Write)
		return;

	if (ftell_and_check())
		fseek_and_check(SEEK_SET);

	// Always reset the state even if the file is broken
	last_action = LastAction::None;
}

#ifdef BOXER_APP
// --Added 2011-11-03 by Alun Bestor to let Boxer inform open file handles
// that their physical backing media will be removed.
void localFile::willBecomeUnavailable()
{
	// If the real file is about to become unavailable, then close
	// our file handle but leave the DOS file flagged as 'open'.
	if (fhandle) {
		fclose(fhandle);
		fhandle = 0;
	}
}
#endif	// BOXER_APP

// ********************************************
// CDROM DRIVE
// ********************************************

cdromDrive::cdromDrive(const char _driveLetter,
                       const char * startdir,
                       uint16_t _bytes_sector,
                       uint8_t _sectors_cluster,
                       uint16_t _total_clusters,
                       uint16_t _free_clusters,
                       uint8_t _mediaid,
                       int& error)
	: localDrive(startdir,
	             _bytes_sector,
	             _sectors_cluster,
	             _total_clusters,
	             _free_clusters,
	             _mediaid),
	  subUnit(0),
	  driveLetter(_driveLetter)
{
	// Init mscdex
	error = MSCDEX_AddDrive(driveLetter,startdir,subUnit);
	type  = DosDriveType::Cdrom;
	safe_strcpy(info, startdir);
	// Get Volume Label
	char name[32];
	if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
}

bool cdromDrive::FileOpen(DOS_File** file, char* name, uint32_t flags)
{
	if ((flags & 0xf) == OPEN_READWRITE) {
		flags &= ~static_cast<unsigned>(OPEN_READWRITE);
	} else if ((flags & 0xf) == OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	bool success = localDrive::FileOpen(file, name, flags);
	if (success)
		(*file)->SetFlagReadOnlyMedium();
	return success;
}

bool cdromDrive::FileCreate(DOS_File** /*file*/, char* /*name*/,
                            FatAttributeFlags /*attributes*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::FileUnlink(char* /*name*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::RemoveDir(char* /*dir*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::MakeDir(char* /*dir*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::Rename(char* /*oldname*/, char* /*newname*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::GetFileAttr(char* name, FatAttributeFlags* attr)
{
	const bool result = localDrive::GetFileAttr(name, attr);
	if (result) {
		attr->archive   = false;
		attr->system    = false;
		attr->read_only = true;
	}
	return result;
}

bool cdromDrive::FindFirst(char* _dir, DOS_DTA& dta, bool /*fcb_findfirst*/)
{
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	return localDrive::FindFirst(_dir,dta);
}

void cdromDrive::SetDir(const char* path)
{
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	localDrive::SetDir(path);
}

bool cdromDrive::isRemote()
{
	return true;
}

bool cdromDrive::isRemovable()
{
	return true;
}

Bits cdromDrive::UnMount()
{
	if (MSCDEX_RemoveDrive(driveLetter)) {
		return 0;
	}
	return 2;
}
