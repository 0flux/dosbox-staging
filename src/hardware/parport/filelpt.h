/*
 *  Copyright (C) 2002-2006  The DOSBox Team
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

// include guard
#ifndef DOSBOX_FILELPT_H
#define DOSBOX_FILELPT_H

#include "dosbox.h"
#include "parport.h"


typedef enum { FILE_DEV, FILE_CAPTURE, FILE_APPEND } DFTYPE;

class CFileLPT final : public CParallel {
public:
	CFileLPT(const CFileLPT&) = delete;            // prevent copying
	CFileLPT& operator=(const CFileLPT&) = delete; // prevent assignment

	CFileLPT(uint8_t nr, CommandLine* cmd);
	~CFileLPT();

	uint8_t Read_PR() override;
	uint8_t Read_CON() override;
	uint8_t Read_SR() override;

	void Write_PR(uint8_t) override;
	void Write_CON(uint8_t) override;
	void Write_IOSEL(uint8_t) override;

	bool Putchar(uint8_t) override;

	void handleUpperEvent(uint16_t type) override;

private:
	bool OpenFile();
	
	bool fileOpen = false;
	DFTYPE filetype = FILE_DEV;				// which mode to operate in (capture,fileappend,device)
	FILE* file = nullptr;
	std::string name = "";					// name of the thing to open
	bool addFF = false;						// add a formfeed character before closing the file/device
	bool addLF = false;						// if set, add line feed after carriage return if not used by app

	uint8_t lastChar = 0;					// used to save the previous character to decide wether to add LF
	const uint16_t* codepage_ptr = nullptr;	// pointer to the translation codepage if not null

	bool ack_polarity = false;

	uint8_t datareg = 0;
	uint8_t controlreg = 0;

	bool autofeed = false;
	bool ack = false;
	uint32_t timeout = 0;
	uint32_t lastUsedTick = 0;
};

#endif	// include guard
