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


//#include "config.h"
//#include "setup.h"
#include "dosbox.h"

#if C_DIRECTLPT

#include "parport.h"
#ifdef WIN32
#include "../../libs/porttalk/porttalk.h"
#endif
#include "directlpt.h"
#include "callback.h"

#ifdef LINUX
#include <linux/ppdev.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#endif
#include <SDL.h>
#ifdef WIN32
//#include "setup.h"
#endif


CDirectLPT::CDirectLPT(uint8_t nr, CommandLine* cmd)
		: CParallel(nr, cmd),
		  m_interruptflag	{true},	// interrupt disabled
		  m_ack_polarity	{false},
#ifdef LINUX
		  m_porthandle		{0}
#endif
#ifdef WIN32
		  // m_driver_handle				{NULL},
		  m_realbaseaddress				{0x378},
		  m_original_ecp_control_reg	{0},
		  m_is_ecp						{false}
#endif
{
	std::string str;

#ifdef LINUX
	if (!cmd->FindStringBegin("realport:", str, false)) {
		LOG_MSG("parallel%d: realport parameter missing.", nr + 1);
		return;
	}
	m_porthandle = open(str.c_str(), O_RDWR);
	if (m_porthandle == -1) {
		LOG_MSG("parallel%d: Could not open port %s.", nr + 1, str.c_str());
		if (errno == 2) LOG_MSG ("The specified port does not exist.");
		else if (errno==EBUSY) LOG_MSG("The specified port is already in use.");
		else if (errno==EACCES) LOG_MSG("You are not allowed to access this port.");
		else LOG_MSG("Errno %d occurred.", errno);
		return;
	}
	if (ioctl(m_porthandle, PPCLAIM, NULL) == -1) {
		LOG_MSG("parallel%d: failed to claim port.", nr + 1);
		return;
	}
	// TODO check return value
#endif
#ifdef WIN32
	if (cmd->FindStringBegin("realbase:", str, false)) {
		if(sscanf(str.c_str(), "%x", &m_realbaseaddress) != 1) {
			LOG_MSG("parallel%d: Invalid realbase parameter.", nr);
			return;
		} 
	}

	if (m_realbaseaddress >= 0x10000) {
		LOG_MSG("Error: Invalid base address.");
		return;
	}
	
	if (!initPorttalk()) {
		LOG_MSG("Error: could not open PortTalk driver.");
		return;
	}

	// Make sure the user doesn't touch critical I/O-ports
	if ((m_realbaseaddress < 0x100) || (m_realbaseaddress & 0x3) ||			// sanity + mainboard res.
	   ((m_realbaseaddress >= 0x1f0) && (m_realbaseaddress <= 0x1f7)) ||	// prim. HDD controller
	   ((m_realbaseaddress >= 0x170) && (m_realbaseaddress <= 0x177)) ||	// sek. HDD controller
	   ((m_realbaseaddress >= 0x3f0) && (m_realbaseaddress <= 0x3f7)) ||	// floppy + prim. HDD
	   ((m_realbaseaddress >= 0x370) && (m_realbaseaddress <= 0x377))) {	// sek. hdd
		LOG_MSG("Parallel Port: Invalid base address.");
		return;
	}
	/*	
	if (m_realbaseaddress != 0x378 && m_realbaseaddress != 0x278 && m_realbaseaddress != 0x3bc) {
		// TODO PCI ECP ports can be on funny I/O-port-addresses
		LOG_MSG("Parallel Port: Invalid base address.");
		return;
	}
	*/

	uint32_t ecpbase = 0;
	if (cmd->FindStringBegin("ecpbase:", str, false)) {
		if (sscanf(str.c_str(), "%x", &ecpbase) != 1) {
			LOG_MSG("parallel%d: Invalid realbase parameter.", nr);
			return;
		}
		m_is_ecp = true;
	} else {
		// 0x3bc cannot be a ECP port
		m_is_ecp = ((m_realbaseaddress & 0x7) == 0);
		if (m_is_ecp) ecpbase = m_realbaseaddress + 0x402;
	}

	// Add the standard parallel port registers
	addIOPermission(check_cast<uint16_t>(m_realbaseaddress));
	addIOPermission(check_cast<uint16_t>(m_realbaseaddress + 1));
	addIOPermission(check_cast<uint16_t>(m_realbaseaddress + 2));
	
	// If it could be a ECP port: make the extended control register accessible
	if (m_is_ecp) addIOPermission(check_cast<uint16_t>(ecpbase));
	
	// Bail out if porttalk fails
	if (!setPermissionList()) {
		LOG_MSG("ERROR SET PERMLIST");
		return;
	}

	if (m_is_ecp) {
		// Check if there is a ECP port (try to set bidir)
		m_original_ecp_control_reg = inportb(ecpbase);
		uint8_t new_bidir = m_original_ecp_control_reg & 0x1F;
		new_bidir |= 0x20;

		outportb(ecpbase, new_bidir);
		if (inportb(ecpbase) != new_bidir) {
			// this is not a ECP port
			outportb(ecpbase, m_original_ecp_control_reg);
			m_is_ecp = false;
		}
	}
	// check if there is a parallel port at all: the autofeed bit
	uint8_t controlreg = inportb(m_realbaseaddress + 2);
	outportb(m_realbaseaddress + 2, controlreg | 2);
	if (!(inportb(m_realbaseaddress + 2) & 0x2)) {
		LOG_MSG("No parallel port detected at 0x%x!", m_realbaseaddress);
		// cannot remember 1
		return;
	}
	
	// check 0
	outportb(m_realbaseaddress + 2, controlreg & ~2);
	if (inportb(m_realbaseaddress + 2) & 0x2) {
		LOG_MSG("No parallel port detected at 0x%x!", m_realbaseaddress);
		// cannot remember 0
		return;
	}
	outportb(m_realbaseaddress + 2, controlreg);
	
	if (m_is_ecp) LOG_MSG("The port at 0x%x was detected as ECP port.", m_realbaseaddress);
	else LOG_MSG("The port at 0x%x is not a ECP port.", m_realbaseaddress);
	
	/*
	// Bidir test
	outportb(m_realbaseaddress + 2, 0x20);
	for(int i = 0; i < 256; i++) {
		outportb(m_realbaseaddress, i);
		if (inportb(m_realbaseaddress) != i) LOG_MSG("NOT %x", i);
	}
	*/
#endif

	// go for it
	m_ack_polarity = false;
	initialize();

	InstallationSuccessful = true;
	// LOG_MSG("InstSuccess");
}

CDirectLPT::~CDirectLPT()
{
#ifdef LINUX
	if (m_porthandle > 0) close(m_porthandle);
#endif
#ifdef WIN32
	if (InstallationSuccessful && m_is_ecp) {
		outportb(m_realbaseaddress + 0x402, m_original_ecp_control_reg);
	}
#endif
}

bool CDirectLPT::Putchar(uint8_t val)
{	
	//LOG_MSG("putchar: %x",val);

	// check if printer online and not busy
	// PE and Selected: no printer attached
	uint8_t sr=Read_SR();
	//LOG_MSG("SR: %x",sr);
	if((sr&0x30)==0x30)
	{
		LOG_MSG("putchar: no printer");
		return false;
	}
	// error
	if(sr&0x20)
	{
		LOG_MSG("putchar: paper out");
		return false;
	}
	if((sr&0x08)==0)
	{
		LOG_MSG("putchar: printer error");
		return false;
	}

	Write_PR(val);
	// busy
	Bitu timeout = 10000;
	Bitu time = timeout+SDL_GetTicks();

	while(SDL_GetTicks()<time) {
		// wait for the printer to get ready
		for(int i = 0; i < 500; i++) {
			// do NOT run into callback_idle unless we have to (speeds things up)
			sr=Read_SR();
			if(sr&0x80) break;
		}
		if(sr&0x80) break;
		CALLBACK_Idle();
	}
	if(SDL_GetTicks()>=time) {
		LOG_MSG("putchar: busy timeout");
		return false;
	}
	// strobe data out
	// I hope this creates a sufficient long pulse...
	// (I/O-Bus at 7.15 MHz will give some delay)
	
	for(int i = 0; i < 5; i++) Write_CON(0xd); // strobe on
	Write_CON(0xc); // strobe off

#if PARALLEL_DEBUG
	log_par(dbg_putchar,"putchar  0x%2x",val);
	if(dbg_plainputchar) fprintf(debugfp,"%c",val);
#endif

	return true;
}

uint8_t CDirectLPT::Read_PR()
{
	uint8_t retval;
#ifdef LINUX
	ioctl(m_porthandle, PPRDATA, &retval);
#endif
#ifdef WIN32
	retval = inportb(m_realbaseaddress);
#endif
	return retval;
}
uint8_t CDirectLPT::Read_CON()
{
	uint8_t retval;
#ifdef LINUX
	ioctl(m_porthandle, PPRCONTROL, &retval);
#endif
#ifdef WIN32
	retval = inportb(m_realbaseaddress + 2);
	if (!m_interruptflag) { // interrupt activated
		retval &= ~0x10;
	}
#endif
	return retval;
}
uint8_t CDirectLPT::Read_SR()
{
	uint8_t retval;
#ifdef LINUX
	ioctl(m_porthandle, PPRSTATUS, &retval);
#endif
#ifdef WIN32
	retval = inportb(m_realbaseaddress + 1);
#endif
	return retval;
}

void CDirectLPT::Write_PR(uint8_t val)
{
#ifdef LINUX
	ioctl(m_porthandle, PPWDATA, &val);
#endif
#ifdef WIN32
	// LOG_MSG("%c, %x", val, val);
	outportb(m_realbaseaddress, val);
#endif
}
void CDirectLPT::Write_CON(uint8_t val)
{
#ifdef LINUX
	ioctl(m_porthandle, PPWCONTROL, &val); 
#endif
#ifdef WIN32
	// do not activate interrupt
	m_interruptflag = (val&0x10) != 0;
	outportb(m_realbaseaddress + 2, val | 0x10);
#endif
}
void CDirectLPT::Write_IOSEL(uint8_t val)
{
#ifdef LINUX
	// switches direction old-style TODO
	if ((val == 0xAA) || (val == 0x55)) LOG_MSG("TODO implement IBM-style direction switch");
#endif
#ifdef WIN32
	outportb(m_realbaseaddress + 1, val);
#endif
}

void CDirectLPT::handleUpperEvent(uint16_t /*type*/)
{
	// nothing
}

#endif	// C_DIRECTLPT
