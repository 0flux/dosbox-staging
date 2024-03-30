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

#include "dosbox.h"

#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#include "support.h"
// #include "hardware.h"			// OpenCaptureFile
// #include "../../capture/capture.h"
#include "inout.h"
#include "pic.h"
#include "setup.h"
//#include "timer.h"
#include "bios.h"					// SetLPTPort(..)
#include "callback.h"				// CALLBACK_Idle

#include "parport.h"
#include "directlpt.h"
#include "printer_redir.h"
#include "filelpt.h"
//#include "dos_inc.h"


// default ports & interrupts
const uint8_t parallel_defaultirq[] = {7, 5, 12};
const char *const parallel_lptname[] = {"LPT1", "LPT2", "LPT3"};

device_LPT::device_LPT(class CParallel* ppclass)
	: m_pclass	{ppclass}
{
	SetName(parallel_lptname[m_pclass->port_index]);
}

device_LPT::~device_LPT()
{
	//LOG_MSG("device_LPT::~device_LPT()");
}

bool device_LPT::Read(uint8_t* /*data*/, uint16_t* size)
{
	*size = 0;
	LOG(LOG_DOSMISC, LOG_NORMAL)("LPTDEVICE: Read called");
	return true;
}

bool device_LPT::Write(uint8_t* data, uint16_t* size)
{
	for (uint16_t i=0; i<*size; i++) {
		if (!m_pclass->Putchar(data[i])) return false;
	}
	return true;
}

bool device_LPT::Seek(uint32_t* pos, uint32_t /*type*/)
{
	*pos = 0;
	return true;
}

bool device_LPT::Close()
{
	return false;
}

uint16_t device_LPT::GetInformation()
{
	return 0x80A0;
}



// LPT1 - LPT3 objects
CParallel *parallelports[3] = {nullptr};

static uint8_t PARALLEL_Read(io_port_t port, io_width_t /*iolen*/)
{
	uint8_t retval = 0xff;

	for (uint8_t i=0; i<3; i++) {
		if (!parallelports[i]) continue;
		if (parallel_baseaddr[i] == (port & 0xfffc)) {
			switch (port & 0x7) {
				case 0: retval = parallelports[i]->Read_PR(); break;
				case 1: retval = parallelports[i]->Read_SR(); break;
				case 2: retval = parallelports[i]->Read_CON(); break;
			}

#if PARALLEL_DEBUG
			const char* const dbgtext[] = {"DAT", "STA", "COM", "???"};
			parallelports[i]->log_par(parallelports[i]->dbg_cregs,
									  "read  0x%2x from %s.", retval,
									  dbgtext[port & 0x3]);
#endif
			return retval;	
		}
	}
	return retval;
}

static void PARALLEL_Write(io_port_t port, io_val_t val, io_width_t /*iolen*/)
{
	for (uint8_t i=0; i<3; i++) {
		if (!parallelports[i]) continue;
		if (parallel_baseaddr[i] == (port & 0xfffc)) {
#if PARALLEL_DEBUG
			const char* const dbgtext[] = {"DAT", "IOS", "CON", "???"};
			parallelports[i]->log_par(parallelports[i]->dbg_cregs,
									  "write 0x%2x to %s.", val,
									  dbgtext[port & 0x3]);
			if (parallelports[i]->dbg_plaindr && !(port & 0x3)) {
				fprintf(parallelports[i]->debugfp,"%c", val);
			}
#endif
			switch (port & 0x3) {
				case 0: parallelports[i]->Write_PR(val); return;
				case 1: parallelports[i]->Write_IOSEL(val); return;
				case 2: parallelports[i]->Write_CON(val); return;
			}
		}
	}
}

#if PARALLEL_DEBUG
void CParallel::log_par(bool active, char const* format, ...)
{
	if(active) {
		// copied from DEBUG_SHOWMSG
		char buf[512];
		buf[0]=0;
		sprintf(buf,"%12.3f ",PIC_FullIndex());
		va_list msg;
		va_start(msg,format);
		vsprintf(buf+strlen(buf),format,msg);
		va_end(msg);
		// Add newline if not present
		Bitu len=strlen(buf);
		if(buf[len-1]!='\n') strcat(buf,"\r\n");
		fputs(buf,debugfp);
	}
}
#endif

static void Parallel_EventHandler(uint32_t val)
{
	uint32_t parclassid = val & 0x3;
	if (parallelports[parclassid] != nullptr) {
		const auto event_type = static_cast<uint16_t>(val >> 2);
		parallelports[parclassid]->handleEvent(event_type);
	}
}

// void RunIdleTime(Bitu milliseconds)
void RunIdleTime(uint32_t milliseconds)
{
	// Bitu time=SDL_GetTicks()+milliseconds;
	// while(SDL_GetTicks()<time)
	double time = PIC_FullIndex() + milliseconds;
	while(PIC_FullIndex()<time)
		CALLBACK_Idle();
}

/*****************************************************************************/
/* Initialisation                                                           **/
/*****************************************************************************/
CParallel::CParallel(uint8_t port_idx, CommandLine* /*cmd*/)
		: port_index	{port_idx},
		  irq			{0}
{
	const uint16_t base = parallel_baseaddr[port_index];

	irq = parallel_defaultirq[port_index];

#if PARALLEL_DEBUG
	dbg_data	= cmd->FindExist("dbgdata", false);
	dbg_putchar = cmd->FindExist("dbgput", false);
	dbg_cregs	= cmd->FindExist("dbgregs", false);
	dbg_plainputchar = cmd->FindExist("dbgputplain", false);
	dbg_plaindr = cmd->FindExist("dbgdataplain", false);
	
	if(cmd->FindExist("dbgall", false)) {
		dbg_data= 
		dbg_putchar=
		dbg_cregs=true;
		dbg_plainputchar=dbg_plaindr=false;
	}

	if(dbg_data||dbg_putchar||dbg_cregs||dbg_plainputchar||dbg_plaindr)
		debugfp=OpenCaptureFile("parlog",".parlog.txt");
	else debugfp=0;

	if(debugfp == 0) {
		dbg_data= 
		dbg_putchar=dbg_plainputchar=
		dbg_cregs=false;
	} else {
		std::string cleft;
		cmd->GetStringRemain(cleft);

		log_par(true,"Parallel%d: BASE %xh, initstring \"%s\"\r\n\r\n",
			port_idx+1,base,cleft.c_str());
	}
#endif
	LOG_MSG("Parallel%d: BASE %xh", port_idx+1, base);

	for (uint8_t i=0; i<3; i++) {
		WriteHandler[i].Install(i + base, PARALLEL_Write, io_width_t::byte);
		ReadHandler[i].Install(i + base, PARALLEL_Read, io_width_t::byte);
	}

	BIOS_SetLPTPort(port_idx, base);

	mydosdevice = new device_LPT(this);
	DOS_AddDevice(mydosdevice);
}

CParallel::~CParallel(void)
{
	BIOS_SetLPTPort(port_index, 0);

	if (mydosdevice) DOS_DelDevice(mydosdevice);
}

void CParallel::initialize()
{
	Write_IOSEL(0x55);	// output mode
	Write_CON(0x08);	// init low
	Write_PR(0);
	RunIdleTime(10);
	Write_CON(0x0c);	// init high
	RunIdleTime(500);
	//LOG_MSG("printer init");
}

void CParallel::setEvent(uint16_t type, float duration)
{
    PIC_AddEvent(Parallel_EventHandler, static_cast<double>(duration), static_cast<uint32_t>((type << 2) | port_index));
}

void CParallel::removeEvent(uint16_t type)
{
    // TODO
	PIC_RemoveSpecificEvents(Parallel_EventHandler, static_cast<uint32_t>((type << 2) | port_index));
}

void CParallel::handleEvent(uint16_t type)
{
	handleUpperEvent(type);
}

uint8_t CParallel::getPrinterStatus()
{
	/*	7      not busy
		6      acknowledge
		5      out of paper
		4      selected
		3      I/O error
		2-1    unused
		0      timeout  */
	uint8_t statusreg = Read_SR();

	//LOG_MSG("get printer status: %x", statusreg);

	statusreg ^= 0x48;
	return statusreg &~ 0x7;
}



class PARPORTS final : public Module_base {
public:
	PARPORTS(Section * configuration) : Module_base (configuration) {
		Section_prop *section = static_cast <Section_prop*>(configuration);

		// iterate through all 3 lpt ports
		char s_property[] = "parallelx";
		for (uint8_t i = 0; i < 3; ++i) {
			// --Modified 2012-02-10 by Alun Bestor:
			// if a parallel port is already occupied by another device (e.g. disney sound source on LPT1), skip it
			uint32_t biosAddress = BIOS_ADDRESS_LPT1;
			switch (i) {
				case 0: biosAddress = BIOS_ADDRESS_LPT1; break;
				case 1: biosAddress = BIOS_ADDRESS_LPT2; break;
				case 2: biosAddress = BIOS_ADDRESS_LPT3; break;
				default: break;
			}
			if (mem_readw(biosAddress) != 0) {
				LOG_MSG("PARALLEL: LPT%d already taken, skipping", i + 1);
				continue;
			}
			// --End of modifications

			// get the configuration property
			s_property[8] = '1' + static_cast<char>(i);
			CommandLine cmd("", section->Get_string(s_property).c_str());

			std::string type;
			cmd.FindCommand(1, type);

			// detect the type
			if (type=="file") {
				parallelports[i] = new CFileLPT(i, &cmd);
				parallelports[i]->parallelType = PARALLEL_PORT_TYPE::TYPE_FILE;
				cmd.GetStringRemain(parallelports[i]->commandLineString);
				if (!parallelports[i]->InstallationSuccessful) {
					delete parallelports[i];
					parallelports[i] = nullptr;
				}
			}
#ifdef C_DIRECTLPT			
			else if (type=="reallpt") {
				parallelports[i] = new CDirectLPT(i, &cmd);
				parallelports[i]->parallelType = PARALLEL_PORT_TYPE::TYPE_DIRECT;
				cmd.GetStringRemain(parallelports[i]->commandLineString);
				if (!parallelports[i]->InstallationSuccessful) {
					delete parallelports[i];
					parallelports[i] = nullptr;
				}
			}
#endif
#if C_PRINTER
			else if (type=="printer") {
				if (!CPrinterRedir::printer_used) {
					parallelports[i] = new CPrinterRedir(i, &cmd);
					parallelports[i]->parallelType = PARALLEL_PORT_TYPE::TYPE_PRINTER;
					cmd.GetStringRemain(parallelports[i]->commandLineString);
					if (parallelports[i]->InstallationSuccessful) {
						CPrinterRedir::printer_used = true;
					} else {
						LOG_MSG("PARALLEL: Error: printer is not enabled.");
						delete parallelports[i];
						parallelports[i] = nullptr;
					}
				} else {
					LOG_MSG("PARALLEL: Error: only one parallel port with printer.");
				}
			}
#endif				
			else if (type=="disabled") {
				parallelports[i] = nullptr;
			} else {
				parallelports[i] = nullptr;
				LOG_MSG("PARALLEL: LPT%" PRIu8 " invalid type \"%s\".",
				        static_cast<uint8_t>(i + 1), type.c_str());
			}
		} // for lpt 1-3
	}

	~PARPORTS() {
		for (uint8_t i=0; i<3; i++) {
			if (parallelports[i]) {
				delete parallelports[i];
				parallelports[i] = nullptr;
			}
		}
	}
};

static PARPORTS *testParallelPortsBaseclass = nullptr;

void PARALLEL_Destroy(Section* /*sec*/)
{
	delete testParallelPortsBaseclass;
	testParallelPortsBaseclass = nullptr;
}

void PARALLEL_Init(Section* sec)
{
	// should never happen
	assert(sec);

	if (testParallelPortsBaseclass) delete testParallelPortsBaseclass;
	testParallelPortsBaseclass = new PARPORTS(sec);

	sec->AddDestroyFunction(&PARALLEL_Destroy, true);
}
