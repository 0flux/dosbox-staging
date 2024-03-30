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
#ifndef DOSBOX_DIRECTLPT_H
#define DOSBOX_DIRECTLPT_H

// #include "config.h"
// #include "setup.h"
#include "dosbox.h"

#if C_DIRECTLPT

#define DIRECTLPT_AVAILIBLE
#include "parport.h"
#ifdef WIN32
// #include <windows.h>
#endif

class CDirectLPT final : public CParallel {
public:
	CDirectLPT(const CDirectLPT&) = delete;            // prevent copying
	CDirectLPT& operator=(const CDirectLPT&) = delete; // prevent assignment

	CDirectLPT(uint8_t nr, CommandLine* cmd);
	virtual ~CDirectLPT();

	uint8_t Read_PR() override;
	uint8_t Read_CON() override;
	uint8_t Read_SR() override;

	void Write_PR(uint8_t) override;
	void Write_CON(uint8_t) override;
	void Write_IOSEL(uint8_t) override;
	
	bool Putchar(uint8_t) override;

	void handleUpperEvent(uint16_t type) override;

private:
	bool		m_interruptflag;
	bool		m_ack_polarity;
#ifdef LINUX
	int			m_porthandle;
#endif
#ifdef WIN32
	// HANDLE		m_driver_handle;
	uint32_t	m_realbaseaddress;
	uint8_t		m_original_ecp_control_reg;
	bool		m_is_ecp;
#endif
};

#endif	// C_DIRECTLPT
#endif	// include guard
