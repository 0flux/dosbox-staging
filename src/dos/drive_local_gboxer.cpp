#include "drive_local_gboxer.h"

#include "dos_inc.h"
#include "inout.h"

#include "Coalface.h"
#include <glib.h>


using GBoxer::LocalFile;

LocalFile::LocalFile(const char *filename, ADB::VFILE *handle)
    : m_fhandle           {handle},
      m_read_only_medium  {false}
{
	attr = DOS_ATTR_ARCHIVE;
	open = true;
	name = 0;

	// UpdateDateTimeFromHost();
	SetName(filename);
}

LocalFile::~LocalFile()
{
  // g_message("//////////////////////// LocalFile::~LocalFile()");
}

// TODO Maybe use fflush, but that seemed to fuck up in visual c
bool LocalFile::Read(Bit8u *data, Bit16u *size)
{
  // Check if file opened in write-only mode
	if ((this->flags & 0xf) == OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

  // --Added 2011-11-03 by Alun Bestor to avoid errors on files
  // whose backing media has disappeared
  if (!m_fhandle) {
    *size = 0;
    // IMPLEMENTATION NOTE: you might think we ought to return false here,
    // but no! We return true to be consistent with DOSBox's behaviour,
    // which appears to be the behaviour expected by DOS.
    return true;
  }
  // --End of modifications

  *size = (Bit16u)Coalface::read_local_file(data, *size, m_fhandle);

	/* Fake harddrive motion. Inspector Gadget with soundblaster compatible */
	/* Same for Igor */
	/* hardrive motion => unmask irq 2. Only do it when it's masked as unmasking is realitively heavy to emulate */
	Bit8u mask = IO_Read(0x21);
	if (mask & 0x4 ) IO_Write(0x21, mask & 0xfb);
	return true;
}

bool LocalFile::Write(Bit8u *data, Bit16u *size)
{
  // Check if file opened in read-only mode
	if ((this->flags & 0xf) == OPEN_READ) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

  // --Added 2011-11-03 by Alun Bestor to avoid errors on files
  // whose backing media has disappeared
  if (!m_fhandle) {
    *size = 0;
    // IMPLEMENTATION NOTE: you might think we ought to return false here,
    // but no! We return true to be consistent with DOSBox's behaviour,
    // which appears to be the behaviour expected by DOS.
    return true;
  }
  // --End of modifications

  *size = (Bit16u)Coalface::write_local_file(data, *size, m_fhandle);

  return true;
}

bool LocalFile::Seek(Bit32u *pos, Bit32u type)
{
	int seektype;
	switch (type) {
    case DOS_SEEK_SET: seektype = SEEK_SET; break;
    case DOS_SEEK_CUR: seektype = SEEK_CUR; break;
    case DOS_SEEK_END: seektype = SEEK_END; break;
    default:
      // TODO Give some doserrorcode;
		  return false; // ERROR
	}

  // --Added 2011-11-03 by Alun Bestor to avoid errors on files
  // whose backing media has disappeared
  if (!m_fhandle) {
    *pos = 0;
    // IMPLEMENTATION NOTE: you might think we ought to return false here,
    // but no! We return true to be consistent with DOSBox's behaviour,
    // which appears to be the behaviour expected by DOS.
    return true;
  }
  // --End of modifications

  bool success = Coalface::seek_local_file(m_fhandle, *reinterpret_cast<Bit32s*>(pos), seektype);
  if (!success) {
		// Out of file range, pretend everythings ok
		// and move file pointer top end of file... ?! (Black Thorne)
		Coalface::seek_local_file(m_fhandle, 0, SEEK_END);
  }

	*pos = (Bit32u)Coalface::tell_local_file(m_fhandle);

	return true;
}

bool LocalFile::Close()
{
	// Only close if one reference left
	if (refCtr == 1) {
    if (m_fhandle) {
      Coalface::close_local_file(m_fhandle);
      m_fhandle = nullptr;
    }
		open = false;
	}
	return true;
}

Bit16u LocalFile::GetInformation(void)
{
	return m_read_only_medium ? 0x40 : 0;
}

bool LocalFile::UpdateDateTimeFromHost(void)
{
	if (!open) return false;

  // --Added 2011-11-03 by Alun Bestor to avoid errors on closed files
  if (!m_fhandle) return false;
  // --End of modifications

	struct stat temp_stat;

  // TODO: Better use 'boxer_getLocalPathStats' maybe?
  if (!Coalface::stat_local_file(m_fhandle, &temp_stat)) {
    // Just return true if our backend doesn't support fstat
    return true;
  }

	struct tm * ltime;
	if ((ltime = localtime(&temp_stat.st_mtime)) != 0) {
		time = DOS_PackTime((Bit16u)ltime->tm_hour, (Bit16u)ltime->tm_min, (Bit16u)ltime->tm_sec);
		date = DOS_PackDate((Bit16u)(ltime->tm_year + 1900), (Bit16u)(ltime->tm_mon + 1), (Bit16u)ltime->tm_mday);
	} else {
		time = 1;
    date = 1;
	}

	return true;
}

void LocalFile::FlagReadOnlyMedium(void)
{
	m_read_only_medium = true;
}

void LocalFile::willBecomeUnavailable()
{
  // If the real file is about to become unavailable, then close
  // our file handle but leave the DOS file flagged as 'open'.
  if (m_fhandle) {
    Coalface::close_local_file(m_fhandle);
    m_fhandle = nullptr;
  }
}
