/*
 *  Copyright (C) 2002-2005  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dosbox.h"

#if C_PHYSFS

//#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
//#include <string.h>
#include <physfs.h>

#include "dos_inc.h"
#include "string_utils.h"
#include "drives.h"
#include "support.h"
#include "cross.h"

// yuck. Hopefully, later physfs versions improve things
/* The hackishness level is quite low, but to get perfect, here is my personal wishlist for PHYSFS:
 - mounting zip files at arbitrary locations (already in CVS, I think)
 - rename support
 - a better API for stat() infos
 - more stdio-like API for seek, open and truncate
 - perhaps a ramdisk as write dir?
*/


namespace PhysFS
{

// PHYSFS_sint64 PHYSFS_fileLength(const char* name);
static PHYSFS_sint64 get_file_length(const char* name) {
	PHYSFS_file *fhandle = PHYSFS_openRead(name);
	if (fhandle == NULL) return 0;
	PHYSFS_sint64 size = PHYSFS_fileLength(fhandle);
	PHYSFS_close(fhandle);
	return size;
}

// const char *PHYSFS_getLastError(void)
static const char* get_last_error(void) {
	// const PHYSFS_ErrorCode pErrorCode = PHYSFS_getLastErrorCode();
	return PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
}

// int PHYSFS_isDirectory(const char *fname)
static int is_directory(const char* fname) {
	// int PHYSFS_exists(const char *fname);
	// int PHYSFS_stat(const char *fname, PHYSFS_Stat *stat);
	PHYSFS_Stat pStat;
	if (PHYSFS_exists(fname) && PHYSFS_stat(fname, &pStat) && (pStat.filetype == PHYSFS_FILETYPE_DIRECTORY)) {
		return 1;
	}
	return 0;
}

// PHYSFS_sint64 PHYSFS_getLastModTime(const char *filename)
static PHYSFS_sint64 get_last_mod_time(const char* filename) {
	PHYSFS_Stat pStat;
	// if (PHYSFS_exists(filename) && PHYSFS_stat(filename, &pStat)) {
	if (PHYSFS_stat(filename, &pStat) != -1) {
		return pStat.modtime;
	}
	return -1;
}

// PHYSFS_sint64 PHYSFS_read(PHYSFS_File *handle, void *buffer, PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
static PHYSFS_sint64 read(PHYSFS_File* handle, void* buffer, PHYSFS_uint32 objSize, PHYSFS_uint32 objCount) {
	// PHYSFS_sint64 PHYSFS_readBytes(PHYSFS_File *handle, void *buffer, PHYSFS_uint64 len)
	const PHYSFS_uint64 len = (objSize * objCount);
	const PHYSFS_sint64 numBytes = PHYSFS_readBytes(handle, buffer, len);
	if (numBytes != -1) {
		return (numBytes / objSize);
	}
	return numBytes;
}

// PHYSFS_sint64 PHYSFS_write(PHYSFS_File *handle, const void *buffer, PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
static PHYSFS_sint64 write(PHYSFS_File *handle, const void *buffer, PHYSFS_uint32 objSize, PHYSFS_uint32 objCount) {
	// PHYSFS_sint64 PHYSFS_writeBytes(PHYSFS_File *handle, const void *buffer, PHYSFS_uint64 len)
	const PHYSFS_uint64 len = (objSize * objCount);
	const PHYSFS_sint64 numBytes = PHYSFS_writeBytes(handle, buffer, len);
	if (numBytes != -1) {
		return (numBytes / objSize);
	}
	return numBytes;
}

// int PHYSFS_addToSearchPath(const char *newDir, int appendToPath)
static int add_to_search_path(const char *newDir, int appendToPath) {
	// int PHYSFS_mount(const char *newDir, const char *mountPoint, int appendToPath)
	return PHYSFS_mount(newDir, NULL, appendToPath);
}

} // namespace PhysFS


extern std::string capturedir;
static uint8_t physfs_used = 0;

class physfsFile final : public DOS_File {
public:
	physfsFile(const char* name, PHYSFS_file* handle, uint16_t devinfo, const char* physname, bool write);
	physfsFile(const physfsFile&)            = delete; // prevent copying
	physfsFile& operator=(const physfsFile&) = delete; // prevent assignment
	bool Read(uint8_t* data, uint16_t* size) override;
	bool Write(uint8_t* data, uint16_t* size) override;
	bool Seek(uint32_t* pos, uint32_t type) override;
	bool prepareRead();
	bool prepareWrite();
	bool Close() override;
	uint16_t GetInformation() override;
	bool UpdateDateTimeFromHost() override;
private:
	PHYSFS_file* fhandle = nullptr;
	uint16_t info		 = 0;
	char pname[CROSS_LEN];

	enum class LastAction : uint8_t { None, Read, Write };
	LastAction last_action = LastAction::None;
};

// Need to strip "/.." components and transform '\\' to '/' for physfs 
static char* normalize(char* name, const char* basedir) {
	int last = strlen(name)-1;
	strreplace(name, '\\', '/');
	while (last >= 0 && name[last] == '/') name[last--] = 0;
	if (last > 0 && name[last] == '.' && name[last-1] == '/') name[last-1] = 0;
	if (last > 1 && name[last] == '.' && name[last-1] == '.' && name[last-2] == '/') {
		name[last-2] = 0;
		char *slash = strrchr(name, '/');
		if (slash) *slash = 0;
	}
	if (strlen(basedir) > strlen(name)) { strcpy(name, basedir); strreplace(name, '\\', '/'); }
	last = strlen(name)-1;
	while (last >= 0 && name[last] == '/') name[last--] = 0;
	if (name[0] == 0) name[0] = '/';
	LOG_MSG("PHYSFS: File access: %s", name);
	return name;
}

physfsDrive::physfsDrive(const char* startdir, uint16_t _bytes_sector, uint8_t _sectors_cluster, uint16_t _total_clusters, uint16_t _free_clusters, uint8_t _mediaid)
		   :localDrive(startdir,_bytes_sector,_sectors_cluster,_total_clusters,_free_clusters,_mediaid) {
	char newname[CROSS_LEN+1];

	/* No writedir given, use capture directory */
	if(startdir[0] == ':') {
		LOG_MSG("PHYSFS: No writedir given, using capture directory!");

		// safe_strcpy(newname, capturedir.c_str());
		safe_strcat(newname, startdir);
	} else {
		safe_strcpy(newname, startdir);
	}

	CROSS_FILENAME(newname);
	if (!physfs_used) {
		PHYSFS_init("");
		PHYSFS_permitSymbolicLinks(1);
	}

	physfs_used++;
	char *lastdir = newname;
	char *dir = strchr(lastdir+(((lastdir[0]|0x20) >= 'a' && (lastdir[0]|0x20) <= 'z')?2:0),':');
	while (dir) {
		*dir++ = 0;
		if((lastdir == newname) && !strchr(dir+(((dir[0]|0x20) >= 'a' && (dir[0]|0x20) <= 'z')?2:0),':')) {
			// If the first parameter is a directory, the next one has to be the archive file,
			// do not confuse it with basedir if trailing : is not there!
			int tmp = strlen(dir)-1;
			dir[tmp++] = ':';
			dir[tmp++] = CROSS_FILESPLIT;
			dir[tmp] = '\0';
		}
		if (*lastdir && PhysFS::add_to_search_path(lastdir,true) == 0) {
			LOG_MSG("PHYSFS: Couldn't add '%s': %s", lastdir, PhysFS::get_last_error());
		}
		lastdir = dir;
		dir = strchr(lastdir+(((lastdir[0]|0x20) >= 'a' && (lastdir[0]|0x20) <= 'z') ? 2 : 0), ':');
	}
	const char *oldwrite = PHYSFS_getWriteDir();
	if (oldwrite) oldwrite = strdup(oldwrite);
	if (!PHYSFS_setWriteDir(newname)) {
		if (!oldwrite)
			LOG_MSG("PHYSFS: Can't use '%s' for writing, you might encounter problems", newname);
		else
			PHYSFS_setWriteDir(oldwrite);
	}
	if (oldwrite) free((char *)oldwrite);

	safe_strcpy(basedir,lastdir);

	allocation.bytes_sector=_bytes_sector;
	allocation.sectors_cluster=_sectors_cluster;
	allocation.total_clusters=_total_clusters;
	allocation.free_clusters=_free_clusters;
	allocation.mediaid=_mediaid;

	dirCache.SetBaseDir(basedir, this);

	{ // formerly physfsDrive::GetInfo()
		safe_sprintf(info, "PHYSFS directory %s in ", basedir);
		char **files = PHYSFS_getSearchPath(), **list = files;
		while (*files != NULL) {
			safe_strcat(info, *files++);
			safe_strcat(info, ", ");
		}
		PHYSFS_freeList(list);

		if (PHYSFS_getWriteDir() != NULL) {
			safe_strcat(info, "writing to ");
			safe_strcat(info, PHYSFS_getWriteDir());
		} else {
			safe_strcat(info, "read-only");
		}
	}
}

physfsDrive::~physfsDrive() {
	if(!physfs_used) {
		LOG_MSG("PHYSFS: Invalid reference count!");
		return;
	}
	physfs_used--;
	if(!physfs_used) {
		LOG_MSG("PHYSFS: Calling PHYSFS_deinit()");
		PHYSFS_deinit();
	}
}

#if 0
const char* physfsDrive::GetInfo() const {
	safe_sprintf(info, "PHYSFS directory %s in ", basedir);
	char **files = PHYSFS_getSearchPath(), **list = files;
	while (*files != NULL) {
		safe_strcat(info, *files++);
		safe_strcat(info, ", ");
	}
	PHYSFS_freeList(list);

	if (PHYSFS_getWriteDir() != NULL) {
		safe_strcat(info, "writing to ");
		safe_strcat(info, PHYSFS_getWriteDir());
	} else {
		safe_strcat(info, "read-only");
	}
	return info;
}
#endif

bool physfsDrive::FileCreate(DOS_File** file, char* name, FatAttributeFlags /*attributes*/) {
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandNameAndNormaliseCase(newname);
	normalize(newname, basedir);

	/* Test if file exists, don't add to dirCache then */
	bool existing_file = PHYSFS_exists(newname);

	char *slash = strrchr(newname, '/');
	if (slash && slash != newname) {
		*slash = 0;
		if (!PhysFS::is_directory(newname)) return false;
		PHYSFS_mkdir(newname);
		*slash = '/';
	}

	PHYSFS_file *hand = PHYSFS_openWrite(newname);
	if (!hand) {
		LOG_MSG("PHYSFS: Warning: file creation failed: %s (%s)", newname, PhysFS::get_last_error());
		return false;
	}

	/* Make the 16 bit device information */
	*file = new physfsFile(name, hand, 0x202, newname, true);
	(*file)->flags = OPEN_READWRITE;
	if(!existing_file) {
		safe_strcpy(newname, basedir);
		safe_strcat(newname, name);
		CROSS_FILENAME(newname);
		dirCache.AddEntry(newname, true);
	}
	return true;
}

bool physfsDrive::FileOpen(DOS_File** file, char* name, uint32_t flags) {
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandNameAndNormaliseCase(newname);
	normalize(newname, basedir);

	PHYSFS_file * hand;

	if (!PHYSFS_exists(newname)) return false;
	if ((flags & 0xf) == OPEN_READ) {
		hand = PHYSFS_openRead(newname);
	} else {
		// open for reading, deal with writing later
		hand = PHYSFS_openRead(newname);
	}

	if (!hand) {
		if((flags&0xf) != OPEN_READ) {
			PHYSFS_file *hmm = PHYSFS_openRead(newname);
			if (hmm) {
				PHYSFS_close(hmm);
				LOG_MSG("PHYSFS: Warning: file %s exists and failed to open in write mode.\nPlease mount a write directory (see docs).", newname);
			}
		}
		return false;
	}

	*file = new physfsFile(name,hand,0x202,newname,false);
	(*file)->flags = flags;  // for the inheritance flag and maybe check for others.
	return true;
}

bool physfsDrive::FileUnlink(char* name) {
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);

	dirCache.ExpandNameAndNormaliseCase(newname);
	normalize(newname, basedir);

	if (PHYSFS_delete(newname)) {
		CROSS_FILENAME(newname);
		dirCache.DeleteEntry(newname);
		return true;
	};
	return false;
}

bool physfsDrive::FindFirst(char* _dir, DOS_DTA& dta, bool fcb_findfirst) {

	char tempDir[CROSS_LEN];
	safe_strcpy(tempDir,basedir);
	safe_strcat(tempDir,_dir);
	CROSS_FILENAME(tempDir);

	char end[2]={CROSS_FILESPLIT,0};
	if (tempDir[strlen(tempDir)-1]!=CROSS_FILESPLIT) safe_strcat(tempDir,end);

	uint16_t id;
	if (!dirCache.FindFirst(tempDir,id)) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	safe_strcpy(srchInfo[id].srch_dir,tempDir);
	dta.SetDirID(id);

	FatAttributeFlags sAttr = {};
	dta.GetSearchParams(sAttr, tempDir);

	if (sAttr == FatAttributeFlags::Volume) {
		if (strcmp(dirCache.GetLabel(), "") == 0) {
			LOG(LOG_DOSMISC, LOG_ERROR)("DRIVELABEL REQUESTED: none present, returned NOLABEL");
			dta.SetResult("NO_LABEL", 0, 0, 0, FatAttributeFlags::Volume);
			return true;
		}
		dta.SetResult(dirCache.GetLabel(), 0, 0, 0, FatAttributeFlags::Volume);
		return true;
	} else if (sAttr.volume && (*_dir == 0) && !fcb_findfirst) { 
		// should check for a valid leading directory instead of 0
		// exists == true if the volume label matches the searchmask and the path is valid
		if (strcmp(dirCache.GetLabel(), "") == 0) {
			LOG(LOG_DOSMISC, LOG_ERROR)("DRIVELABEL REQUESTED: none present, returned NOLABEL");
			dta.SetResult("NO_LABEL", 0, 0, 0, FatAttributeFlags::Volume);
			return true;
		}
		if (WildFileCmp(dirCache.GetLabel(), tempDir)) {
			dta.SetResult(dirCache.GetLabel(), 0, 0, 0, FatAttributeFlags::Volume);
			return true;
		}
	}
	return FindNext(dta);
}

bool physfsDrive::FindNext(DOS_DTA& dta) {
	char *dir_ent;
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
		char* temp_name = dirCache.GetExpandNameAndNormaliseCase(full_name);
		normalize(temp_name, basedir);

		FatAttributeFlags find_attr{ FatAttributeFlags::Archive };
		if (PhysFS::is_directory(temp_name)) {
			find_attr.directory = true;
		}

		if ((find_attr.directory && !search_attr.directory) ||
		    (find_attr.hidden && !search_attr.hidden) ||
		    (find_attr.system && !search_attr.system)) {
			continue;
		}

		// file is okay, setup everything to be copied in DTA Block
		char find_name[DOS_NAMELENGTH_ASCII] = "";
		uint16_t find_date;
		uint16_t find_time;
		uint32_t find_size;

		if (strlen(dir_entcopy) < DOS_NAMELENGTH_ASCII) {
			safe_strcpy(find_name, dir_entcopy);
			upcase(find_name);
		}

		find_size = static_cast<uint32_t>(PhysFS::get_file_length(temp_name));
		const time_t lastModTime = static_cast<time_t>(PhysFS::get_last_mod_time(temp_name));
		struct tm datetime;
		if (cross::localtime_r(&lastModTime, &datetime)) {
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

bool physfsDrive::GetFileAttr(char* name, FatAttributeFlags* attr) {
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandNameAndNormaliseCase(newname);
	normalize(newname, basedir);
	char *last = strrchr(newname, '/');
	if (last == NULL) last = newname-1;

	*attr = 0;
	if (!PHYSFS_exists(newname)) return false;

	*attr = FatAttributeFlags::Archive;
	if (PhysFS::is_directory(newname)) { attr->directory = true; }
	return true;
}

bool physfsDrive::MakeDir(char* dir) {
	char newdir[CROSS_LEN];
	safe_strcpy(newdir, basedir);
	safe_strcat(newdir, dir);

	CROSS_FILENAME(newdir);
	dirCache.ExpandNameAndNormaliseCase(newdir);
	normalize(newdir, basedir);

	if (PHYSFS_mkdir(newdir)) {
		CROSS_FILENAME(newdir);
		dirCache.CacheOut(newdir, true);
		return true;
	}
	return false;
}

bool physfsDrive::RemoveDir(char* dir) {
	char newdir[CROSS_LEN];
	safe_strcpy(newdir, basedir);
	safe_strcat(newdir, dir);
	CROSS_FILENAME(newdir);

	dirCache.ExpandNameAndNormaliseCase(newdir);
	normalize(newdir,basedir);

	if (PhysFS::is_directory(newdir) && PHYSFS_delete(newdir)) {
		CROSS_FILENAME(newdir);
		dirCache.DeleteEntry(newdir, true);
		return true;
	}
	return false;
}

bool physfsDrive::TestDir(char* dir) {
	char newdir[CROSS_LEN];
	safe_strcpy(newdir, basedir);
	safe_strcat(newdir, dir);

	CROSS_FILENAME(newdir);
	dirCache.ExpandNameAndNormaliseCase(newdir);
	normalize(newdir, basedir);

	return (PhysFS::is_directory(newdir));
}

bool physfsDrive::Rename(char* oldname, char* newname) {
	char newold[CROSS_LEN];
	safe_strcpy(newold, basedir);
	safe_strcat(newold, oldname);

	CROSS_FILENAME(newold);
	dirCache.ExpandNameAndNormaliseCase(newold);
	normalize(newold, basedir);

	char newnew[CROSS_LEN];
	safe_strcpy(newnew, basedir);
	safe_strcat(newnew, newname);

	CROSS_FILENAME(newnew);
	dirCache.ExpandNameAndNormaliseCase(newnew);
	normalize(newnew, basedir);
	/* yuck. physfs doesn't have "rename". */
	LOG_MSG("PHYSFS: TODO: rename not yet implemented (%s -> %s)", newold, newnew);

	return false;
}

bool physfsDrive::AllocationInfo(uint16_t* _bytes_sector, uint8_t* _sectors_cluster, uint16_t* _total_clusters, uint16_t* _free_clusters) {
	/* Always report 100 mb free should be enough */
	/* Total size is always 1 gb */
	*_bytes_sector = allocation.bytes_sector;
	*_sectors_cluster = allocation.sectors_cluster;
	*_total_clusters = allocation.total_clusters;
	*_free_clusters = allocation.free_clusters;

	return true;
}

bool physfsDrive::FileExists(const char* name) {
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandNameAndNormaliseCase(newname);
	normalize(newname, basedir);
	return PHYSFS_exists(newname) && !PhysFS::is_directory(newname);
}

bool physfsDrive::FileStat(const char* name, FileStat_Block* const stat_block) {
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	dirCache.ExpandNameAndNormaliseCase(newname);
	normalize(newname, basedir);
	
	// Convert the stat to a FileStat
	const time_t lastModTime = static_cast<time_t>(PhysFS::get_last_mod_time(newname));
	struct tm datetime;
	if (cross::localtime_r(&lastModTime, &datetime)) {
		stat_block->time = DOS_PackTime(datetime);
		stat_block->date = DOS_PackDate(datetime);
	} else {
		stat_block->time = DOS_PackTime(0, 0, 0);
		stat_block->date = DOS_PackDate(1980, 1, 1);
	}
	stat_block->size = static_cast<uint32_t>(PhysFS::get_file_length(newname));

	return true;
}

uint8_t physfsDrive::GetMediaByte() {
	return allocation.mediaid;
}

bool physfsDrive::isRemote() {
	return false;
}

bool physfsDrive::isRemovable() {
	return false;
}

struct opendirinfo {
	char **files;
	int pos;
};

/* helper functions for drive cache */
bool physfsDrive::isdir(const char* name) {
	char myname[CROSS_LEN];
	safe_strcpy(myname, name);
	normalize(myname, basedir);
	return PhysFS::is_directory(myname);
}

void* physfsDrive::open_directory_vfunc(const char* name) {
	char myname[CROSS_LEN];
	safe_strcpy(myname, name);
	normalize(myname, basedir);
	if (!PhysFS::is_directory(myname)) return NULL;

	struct opendirinfo *oinfo = (struct opendirinfo *)malloc(sizeof(struct opendirinfo));
	oinfo->files = PHYSFS_enumerateFiles(myname);
	if (oinfo->files == NULL) {
		LOG_MSG("PHYSFS: nothing found for %s (%s)", myname, PhysFS::get_last_error());
		free(oinfo);
		return NULL;
	}

	oinfo->pos = (myname[1] == 0 ? 0 :-2);
	return (void *)oinfo;
}

void physfsDrive::close_directory_vfunc(void* handle) {
	struct opendirinfo *oinfo = (struct opendirinfo *)handle;
	if (handle == NULL) return;
	if (oinfo->files != NULL) PHYSFS_freeList(oinfo->files);
	free(oinfo);
}

bool physfsDrive::read_directory_first_vfunc(void* dirp, char* entry_name, bool& is_directory) {
	return read_directory_next_vfunc(dirp, entry_name, is_directory);
}

bool physfsDrive::read_directory_next_vfunc(void* dirp, char* entry_name, bool& is_directory) {
	struct opendirinfo *oinfo = (struct opendirinfo *)dirp;
	if (!oinfo) return false;
	if (oinfo->pos == -2) {
		oinfo->pos++;
		safe_strncpy(entry_name, ".", CROSS_LEN);
		is_directory = true;
		return true;
	}
	if (oinfo->pos == -1) {
		oinfo->pos++;
		safe_strncpy(entry_name, "..", CROSS_LEN);
		is_directory = true;
		return true;
	}
	if (!oinfo->files || !oinfo->files[oinfo->pos]) return false;
	safe_strncpy(entry_name, oinfo->files[oinfo->pos++], CROSS_LEN);
	is_directory = isdir(entry_name);
	return true;
}


bool physfsFile::Read(uint8_t* data, uint16_t* size) {
	if ((this->flags & 0xf) == OPEN_WRITE) {        // check if file opened in write-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	if (last_action == LastAction::Write) prepareRead();
	last_action = LastAction::Read;
	PHYSFS_sint64 mysize = PhysFS::read(fhandle, data, 1, (PHYSFS_uint32)*size);
	// LOG_MSG("PHYSFS: Read %i bytes (wanted %i) at %i of %s (%s)", (int)mysize, (int)*size, (int)PHYSFS_tell(fhandle), name, PhysFS::get_last_error());
	*size = (uint16_t)mysize;
	return true;
}

bool physfsFile::Write(uint8_t* data, uint16_t* size) {
	if ((this->flags & 0xf) == OPEN_READ) { // check if file opened in read-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	if (last_action == LastAction::Read) prepareWrite();
	last_action = LastAction::Write;
	if (*size == 0) {
		if (PHYSFS_tell(fhandle) == 0) {
			PHYSFS_close(PHYSFS_openWrite(pname));
			// LOG_MSG("PHYSFS: Truncate %s (%s)", name, PhysFS::get_last_error());
            return false;
		} else {
			LOG_MSG("PHYSFS: TODO: truncate not yet implemented (%s at %lld)", pname, PHYSFS_tell(fhandle));
			return false;
		}
	} else {
		PHYSFS_sint64 mysize = PhysFS::write(fhandle, data, 1, (PHYSFS_uint32)*size);
		// LOG_MSG("PHYSFS: Wrote %i bytes (wanted %i) at %i of %s (%s)", (int)mysize, (int)*size, (int)PHYSFS_tell(fhandle), name, PhysFS::get_last_error());
		*size = (uint16_t)mysize;
		return true;
	}
}

bool physfsFile::Seek(uint32_t* pos, uint32_t type) {
	PHYSFS_sint64 mypos = (int32_t)*pos;
	switch (type) {
		case DOS_SEEK_SET:	break;
		case DOS_SEEK_CUR: 	mypos += PHYSFS_tell(fhandle); break;
		case DOS_SEEK_END: 	mypos += PHYSFS_fileLength(fhandle); break;
		default: 			return false; // ERROR // TODO Give some doserrorcode;
	}

	if (!PHYSFS_seek(fhandle, mypos)) {
		// Out of file range, pretend everythings ok
		// and move file pointer top end of file... ?! (Black Thorne)
		PHYSFS_seek(fhandle, PHYSFS_fileLength(fhandle));
	};
	// LOG_MSG("PHYSFS: Seek to %i (%i at %x) of %s (%s)", (int)mypos, (int)*pos, type, name, PhysFS::get_last_error());

	*pos = (uint32_t)PHYSFS_tell(fhandle);
	return true;
}

bool physfsFile::prepareRead() {
	PHYSFS_uint64 pos = PHYSFS_tell(fhandle);
	PHYSFS_close(fhandle);
	fhandle = PHYSFS_openRead(pname);
	PHYSFS_seek(fhandle, pos);
	// LOG_MSG("PHYSFS: Goto read (%s at %i)", pname, PHYSFS_tell(fhandle));
    return true;
}

#ifndef WIN32
#include <fcntl.h>
#include <errno.h>
#endif

bool physfsFile::prepareWrite() {
	const char *wdir = PHYSFS_getWriteDir();
	if (wdir == NULL) {
		LOG_MSG("PHYSFS: Could not fulfill write request: no write directory set.");
		return false;
	}
	// LOG_MSG("PHYSFS: Goto write (%s at %i)", pname, PHYSFS_tell(fhandle));
	const char *fdir = PHYSFS_getRealDir(pname);
	PHYSFS_uint64 pos = PHYSFS_tell(fhandle);
	char *slash = strrchr(pname, '/');
	if (slash && slash != pname) {
		*slash = 0;
		PHYSFS_mkdir(pname);
		*slash = '/';
	}
	if (strcmp(fdir, wdir)) { /* we need COW */
		// LOG_MSG("PHYSFS: COW", pname, PHYSFS_tell(fhandle));
		PHYSFS_file *whandle = PHYSFS_openWrite(pname);
		if (whandle == NULL) {
			LOG_MSG("PHYSFS: Copy-on-write failed: %s.", PhysFS::get_last_error());
			return false;
		}
		char buffer[65536];
		PHYSFS_sint64 size;
		PHYSFS_seek(fhandle, 0);
		while ((size = PhysFS::read(fhandle, buffer, 1, 65536)) > 0) {
			if (PhysFS::write(whandle, buffer, 1, (PHYSFS_uint32)size) != size) {
				LOG_MSG("PHYSFS: Copy-on-write failed: %s.", PhysFS::get_last_error());
				PHYSFS_close(whandle);
				return false;
			}
		}
		PHYSFS_seek(whandle, pos);
		PHYSFS_close(fhandle);
		fhandle = whandle;
	} else {
		// megayuck - physfs on posix platforms uses O_APPEND. We illegally access the fd directly and clear that flag.
		// LOG_MSG("PHYSFS: noCOW", pname, PHYSFS_tell(fhandle));
		PHYSFS_close(fhandle);
		fhandle = PHYSFS_openAppend(pname);
#ifndef WIN32
		fcntl(**(int**)fhandle->opaque, F_SETFL, 0);
#endif
		PHYSFS_seek(fhandle, pos);
	}
	return true;
}

bool physfsFile::Close() {
	// only close if one reference left
	if (refCtr == 1) {
		PHYSFS_close(fhandle);
		fhandle = nullptr;
		open = false;
	};
	return true;
}

uint16_t physfsFile::GetInformation(void) {
	return info;
}

physfsFile::physfsFile(const char* _name, PHYSFS_file* handle, uint16_t devinfo, const char* physname, bool write) {
	fhandle = handle;
	info = devinfo;
	safe_strcpy(pname, physname);

	// Convert the stat to a FileStat
	const time_t lastModTime = static_cast<time_t>(PhysFS::get_last_mod_time(pname));
	struct tm datetime;
	if (cross::localtime_r(&lastModTime, &datetime)) {
		this->time = DOS_PackTime(datetime);
		this->date = DOS_PackDate(datetime);
	} else {
		this->time = DOS_PackTime(0, 0, 0);
		this->date = DOS_PackDate(1980, 1, 1);
	}

	attr = FatAttributeFlags::Archive;
	last_action = (write ? LastAction::Write : LastAction::Read);

	open = true;
	SetName(_name);
}

bool physfsFile::UpdateDateTimeFromHost(void) {
	if (!open) return false;

	// Convert the stat to a FileStat
	const time_t lastModTime = static_cast<time_t>(PhysFS::get_last_mod_time(pname));
	struct tm datetime;
	if (cross::localtime_r(&lastModTime, &datetime)) {
		this->time = DOS_PackTime(datetime);
		this->date = DOS_PackDate(datetime);
	} else {
		this->time = DOS_PackTime(0, 0, 0);
		this->date = DOS_PackDate(1980, 1, 1);
	}

	return true;
}


// ********************************************
// CDROM DRIVE
// ********************************************

int  MSCDEX_AddDrive(char driveLetter, const char* physicalPath, uint8_t& subUnit);
bool MSCDEX_HasMediaChanged(uint8_t subUnit);
bool MSCDEX_GetVolumeName(uint8_t subUnit, char* name);

/*
nextFreeDirIterator(0),
          iso(false),
          dataCD(false),
          rootEntry{},
          mediaid(0),
          subUnit(0),
          driveLetter('\0')
*/

physfscdromDrive::physfscdromDrive(const char letter, const char* startdir, uint16_t _bytes_sector, uint8_t _sectors_cluster, uint16_t _total_clusters, uint16_t _free_clusters, uint8_t _mediaid, int& error)
		: physfsDrive(startdir, _bytes_sector, _sectors_cluster, _total_clusters, _free_clusters, _mediaid),
		  subUnit(0),
		  driveLetter('\0')
{
	// Init mscdex
	error = MSCDEX_AddDrive(letter,startdir,subUnit);
	// Get Volume Label
	char name[32];
	if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);

	{ // formerly physfscdromDrive::GetInfo()
		safe_sprintf(info, "PHYSFS directory %s in ", basedir);
		safe_strcpy(info, startdir);
		char **files = PHYSFS_getSearchPath(), **list = files;
		while (*files != NULL) {
			safe_strcat(info, *files++);
			safe_strcat(info, ", ");
		}
		PHYSFS_freeList(list);
		safe_strcat(info, "CD-ROM mode (read-only)");
	}
};

#if 0
const char* physfscdromDrive::GetInfo() const {
	safe_sprintf(info, "PHYSFS directory %s in ", basedir);
	char **files = PHYSFS_getSearchPath(), **list = files;
	while (*files != NULL) {
		safe_strcat(info, *files++);
		safe_strcat(info, ", ");
	}
	PHYSFS_freeList(list);
	safe_strcat(info, "CD-ROM mode (read-only)");
}
#endif

bool physfscdromDrive::FileOpen(DOS_File** file, char* name, uint32_t flags)
{
	if ((flags&0xf)==OPEN_READWRITE) {
		flags &= ~OPEN_READWRITE;
	} else if ((flags&0xf)==OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	return physfsDrive::FileOpen(file,name,flags);
};

bool physfscdromDrive::FileCreate(DOS_File** /*file*/, char* /*name*/, FatAttributeFlags /*attributes*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool physfscdromDrive::FileUnlink(char* /*name*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool physfscdromDrive::RemoveDir(char* /*dir*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool physfscdromDrive::MakeDir(char* /*dir*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool physfscdromDrive::Rename(char* /*oldname*/, char* /*newname*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
};

bool physfscdromDrive::GetFileAttr(char* name, FatAttributeFlags* attr)
{
	bool result = physfsDrive::GetFileAttr(name, attr);
	// if (result) *attr |= FatAttributeFlags::ReadOnly;
	if (result) { attr->read_only = true; }
	return result;
};

bool physfscdromDrive::FindFirst(char* _dir, DOS_DTA& dta, bool /*fcb_findfirst*/)
{
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	return physfsDrive::FindFirst(_dir,dta);
};

void physfscdromDrive::SetDir(const char* path)
{
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	physfsDrive::SetDir(path);
};

bool physfscdromDrive::isRemote(void) {
	return true;
}

bool physfscdromDrive::isRemovable(void) {
	return true;
}

Bits physfscdromDrive::UnMount(void) {
	return 0;
}

#endif // C_PHYSFS
