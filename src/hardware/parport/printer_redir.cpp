/*
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

#if C_PRINTER

#include "parport.h"
// #include "callback.h"
#include "printer_redir.h"
#ifdef BOXER_APP
#include "BXCoalface.h"
#endif // BOXER_APP

// Purpose of this is to pass LPT register access to the virtual printer 

// static
#if C_PRINTER
bool CPrinterRedir::printer_used = false;
#endif

CPrinterRedir::CPrinterRedir(uint8_t nr, CommandLine* cmd)
		: CParallel(nr, cmd)
{
#ifdef BOXER_APP
	InstallationSuccessful = boxer_PRINTER_isInited(nr);
#else
	InstallationSuccessful = PRINTER_isInited();
#endif
}

CPrinterRedir::~CPrinterRedir()
{
	// close file
}

bool CPrinterRedir::Putchar(uint8_t val)
{
	Write_CON(0xD4);
	// strobe data out
	Write_PR(val);
	Write_CON(0xD5); // strobe pulse
	Write_CON(0xD4); // strobe off
	Read_SR();		 // clear ack

#if PARALLEL_DEBUG
	log_par(dbg_putchar, "putchar  0x%2x", val);
	if (dbg_plainputchar) fprintf(debugfp, "%c", val);
#endif

	return true;
}
uint8_t CPrinterRedir::Read_PR()
{
#ifdef BOXER_APP
	return boxer_PRINTER_readdata(0, 1);
#else
	return PRINTER_readdata(0, 1);
#endif
}
uint8_t CPrinterRedir::Read_CON()
{
#ifdef BOXER_APP
	return boxer_PRINTER_readcontrol(0, 1);
#else
	return PRINTER_readcontrol(0, 1);
#endif
}
uint8_t CPrinterRedir::Read_SR()
{
#ifdef BOXER_APP
	return boxer_PRINTER_readstatus(0, 1);
#else
	return PRINTER_readstatus(0, 1);
#endif
}
void CPrinterRedir::Write_PR(uint8_t val)
{
#ifdef BOXER_APP
	boxer_PRINTER_writedata(0, val, 1);
#else
	PRINTER_writedata(0, val, 1);
#endif
}
void CPrinterRedir::Write_CON(uint8_t val)
{
#ifdef BOXER_APP
	boxer_PRINTER_writecontrol(0, val, 1);
#else
	PRINTER_writecontrol(0, val, 1);
#endif
}
void CPrinterRedir::Write_IOSEL(uint8_t /*val*/)
{
	// nothing
}
void CPrinterRedir::handleUpperEvent(uint16_t /*type*/)
{
	// nothing
}

#endif // C_PRINTER
