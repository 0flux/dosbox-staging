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

#ifndef DOSBOX_PARPORT_H
#define DOSBOX_PARPORT_H

#include "dosbox.h"

#ifndef DOSBOX_INOUT_H
#include "inout.h"
#endif
#ifndef DOSBOX_DOS_INC_H
#include "dos_inc.h"
#endif

#include "control.h"

// set to 1 for debug messages and debugging log:
#define PARALLEL_DEBUG 0

enum PARALLEL_PORT_TYPE {
	TYPE_DISABLED = 0,
	TYPE_FILE,
	TYPE_DIRECT,
	TYPE_PRINTER,
	TYPE_INVALID,
};

class CParallel {
public:
	CParallel(const CParallel&) = delete;            // prevent copying
	CParallel& operator=(const CParallel&) = delete; // prevent assignment

#if PARALLEL_DEBUG
	FILE* debugfp; = nullptr;
	bool dbg_data = false;
	bool dbg_putchar = false;
	bool dbg_cregs = false;
	bool dbg_plainputchar = false;
	bool dbg_plaindr = false;
	void log_par(bool active, char const* format, ...);
#endif

	// Constructor
	CParallel(const uint8_t port_idx, CommandLine *cmd);
	virtual ~CParallel();

	bool InstallationSuccessful = false; // check after constructing. If
	                                     // something was wrong, delete it
	                                     // right away.

	IO_ReadHandleObject ReadHandler[3];
	IO_WriteHandleObject WriteHandler[3];

	void setEvent(uint16_t type, float duration);
	void removeEvent(uint16_t type);
	void handleEvent(uint16_t type);
	virtual void handleUpperEvent(uint16_t type) = 0;

	// Bitu port_nr;
	// Bitu base;
	// Bitu irq;

	// uint8_t port_nr;
	const uint8_t port_index;
	// uint16_t baseaddr;
	uint8_t irq;

	// read data line register
	virtual uint8_t Read_PR() = 0;
	virtual uint8_t Read_CON() = 0;
	virtual uint8_t Read_SR() = 0;

	virtual void Write_PR(uint8_t) = 0;
	virtual void Write_CON(uint8_t) = 0;
	virtual void Write_IOSEL(uint8_t) = 0;

	void Write_reserved(uint8_t data, uint8_t address);

	virtual bool Putchar(uint8_t) = 0;
	bool Putchar_default(uint8_t);
	uint8_t getPrinterStatus();
	void initialize();

	// What type of port is this?
	PARALLEL_PORT_TYPE parallelType = PARALLEL_PORT_TYPE::TYPE_DISABLED;

	// How was it created?
	std::string commandLineString = "";

private:
	DOS_Device* mydosdevice = nullptr;
};

extern CParallel* parallelports[];
const uint16_t parallel_baseaddr[3] = {0x378, 0x278, 0x3bc};

// the LPT devices

class device_LPT : public DOS_Device {
public:
	device_LPT(const device_LPT&) = delete;            // prevent copying
	device_LPT& operator=(const device_LPT&) = delete; // prevent assignment

	// Creates a LPT device that communicates with the num-th parallel port, i.e. is LPTnum
	device_LPT(class CParallel* ppclass);
	~device_LPT() override;

	bool Read(uint8_t* data, uint16_t* size) override;
	bool Write(uint8_t* data, uint16_t* size) override;
	bool Seek(uint32_t* pos, uint32_t type) override;
	bool Close() override;

	uint16_t GetInformation() override;

private:
	CParallel*	m_pclass = nullptr;
};

#if C_PRINTER
void PRINTER_AddConfigSection(const config_ptr_t& conf);
#endif

#endif	// DOSBOX_PARPORT_H
