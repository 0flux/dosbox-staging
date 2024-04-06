#pragma once

/*
#include <stdio.h>
// #include <cstdio>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "dosbox.h"
#include "dos_inc.h"
#include "drives.h"
#include "support.h"
#include "cross.h"
#include "inout.h"
*/
#include "dos_system.h"


namespace ADB {
struct VFILE;
}

namespace GBoxer
{

class LocalFile : public DOS_File {
public:
	LocalFile(const char *filename, ADB::VFILE *handle);
	~LocalFile();

	bool Read(Bit8u *data, Bit16u *size) override;
	bool Write(Bit8u *data, Bit16u *size) override;
	bool Seek(Bit32u *pos, Bit32u type) override;
	bool Close() override;
	Bit16u GetInformation(void) override;
	bool UpdateDateTimeFromHost(void) override;
	void FlagReadOnlyMedium(void);

	// --Added 2011-11-03 by Alun Bestor to let Boxer inform open file handles
	// that their physical backing media will be removed.
	void willBecomeUnavailable(void) override;
	// --End of modifications

private:
	ADB::VFILE*		m_fhandle;
	bool        	m_read_only_medium;
};

} // namespace GBoxer
