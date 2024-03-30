/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2024  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "program_parallel.h"

#include <map>

//#include "../hardware/serialport/directserial.h"
//#include "../hardware/serialport/serialdummy.h"
//#include "../hardware/serialport/softmodem.h"
//#include "../hardware/serialport/nullmodem.h"
//#include "../hardware/serialport/serialmouse.h"

#include "../hardware/parport/directlpt.h"
#include "../hardware/parport/filelpt.h"
#include "../hardware/parport/printer_redir.h"
// #include "../hardware/parport/printer.h"

#include "program_more_output.h"

// Map the parallel port type enums to printable names
static std::map<PARALLEL_PORT_TYPE, const std::string> parallel_type_names = {
        {PARALLEL_PORT_TYPE::TYPE_DISABLED, "disabled"},
        {PARALLEL_PORT_TYPE::TYPE_FILE,     "file"},
#ifdef C_DIRECTLPT
        {PARALLEL_PORT_TYPE::TYPE_DIRECT,   "reallpt"},
#endif
#if C_PRINTER
        {PARALLEL_PORT_TYPE::TYPE_PRINTER,  "printer"},
#endif
//		{PARALLEL_PORT_TYPE::TYPE_DISNEY,   "disney"},
        {PARALLEL_PORT_TYPE::TYPE_INVALID,  "invalid"},
};

// static int disneyport = 0;

void PARALLEL::showPort(int port)
{
	if (parallelports[port] != nullptr) {
		WriteOut(MSG_Get("PROGRAM_PARALLEL_SHOW_PORT"), port + 1,
		         parallel_type_names[parallelports[port]->parallelType].c_str(),
				 parallelports[port]->commandLineString.c_str());
//	} else if (disneyport == port + 1) {
//		WriteOut(MSG_Get("PROGRAM_PARALLEL_SHOW_PORT"), port + 1,
//		         parallel_type_names[PARALLEL_PORT_TYPE::TYPE_DISNEY].c_str(), "");
	} else {
		WriteOut(MSG_Get("PROGRAM_PARALLEL_SHOW_PORT"), port + 1,
		         parallel_type_names[PARALLEL_PORT_TYPE::TYPE_DISABLED].c_str(), "");
	}
}

void PARALLEL::Run()
{
	// Show current parallel port configurations.
	if (!cmd->GetCount()) {
		for (int x = 0; x < 3; x++)
			showPort(x);
		return;
	}

	// Select LPT mode.
	if (cmd->GetCount() >= 1 && !HelpRequested()) {
		// Which LPT did they want to change?
		if (!cmd->FindCommand(1, temp_line)) {
			// Port number not provided or invalid type
			WriteOut(MSG_Get("PROGRAM_PARALLEL_BAD_PORT"), 3);
			return;
		}
		// A port value was provided, can it be converted to an integer?
		int port = -1;
		try {
			port = stoi(temp_line);
		} catch (...) {
		}
		if (port < 1 || port > 3) {
			// Didn't understand the port number.
			WriteOut(MSG_Get("PROGRAM_PARALLEL_BAD_PORT"), 3);
			return;
		}
		const auto port_index = port - 1;
		assert(port_index >= 0 && port_index < 3);
		if (cmd->GetCount() == 1) {
			showPort(port_index);
			return;
		}

		// Helper to print a nice list of the supported port types
		auto write_msg_for_invalid_port_type = [this]() {
			WriteOut(MSG_Get("PROGRAM_PARALLEL_BAD_TYPE"));
			for (const auto& [port_type, port_type_str] : parallel_type_names) {
				// Skip the invalid type; show only valid types
				if (port_type != PARALLEL_PORT_TYPE::TYPE_INVALID) {
					WriteOut(MSG_Get("PROGRAM_PARALLEL_INDENTED_LIST"),
					         port_type_str.c_str());
				}
			}
		};

		// If we're here, then PARALLEL.COM was given more than one
		// argument and the second argument must be the port type.
		constexpr auto port_type_arg_pos = 2; // (indexed starting at 1)
		assert(cmd->GetCount() >= port_type_arg_pos);

		// Which port type do they want?
		temp_line.clear();
		if (!cmd->FindCommand(port_type_arg_pos, temp_line)) {
			// Encountered a problem parsing the port type
			write_msg_for_invalid_port_type();
			return;
		}

		// They entered something, but do we have a matching type?
		auto desired_type = PARALLEL_PORT_TYPE::TYPE_INVALID;
		for (const auto& [type, name] : parallel_type_names) {
			if (temp_line == name) {
				desired_type = type;
				break;
			}
		}
		if (desired_type == PARALLEL_PORT_TYPE::TYPE_INVALID) {
			// They entered a port type; but it was invalid
			write_msg_for_invalid_port_type();
			return;
		}

		// Build command line, if any.
		int i = 3;
		std::string commandLineString = "";
		while (cmd->FindCommand(i++, temp_line)) {
			commandLineString.append(temp_line);
			commandLineString.append(" ");
		}
		CommandLine *commandLine = new CommandLine("PARALLEL.COM",
		                                           commandLineString.c_str());

		bool wantPrinter = (desired_type == PARALLEL_PORT_TYPE::TYPE_PRINTER);
		// bool wantDisney = (desired_type == PARALLEL_PORT_TYPE::TYPE_DISNEY);

		// Remove existing port.
		if (parallelports[port_index]) {
#if C_PRINTER
			if (parallelports[port_index]->parallelType == PARALLEL_PORT_TYPE::TYPE_PRINTER) {
				CPrinterRedir::printer_used = false;
			}
			else if (wantPrinter && CPrinterRedir::printer_used) {
				WriteOut("Printer is already assigned to a different port.\n");
				return;
			}
#endif
			/*
			if (disneyport != port && wantDisney && DISNEY_HasInit()) {
				WriteOut("Disney is already assigned to a different port.\n");
				return;
			}
			*/

			/*
			DOS_PSP curpsp(dos.psp());
			if (dos.psp() != curpsp.GetParent()) {
				char name[5];
				sprintf(name, "LPT%d", port);
				curpsp.CloseFile(name);
			}
			*/

			delete parallelports[port_index];
			parallelports[port_index] = nullptr;
		} else {
#if C_PRINTER
			if (wantPrinter && CPrinterRedir::printer_used) {
				WriteOut("Printer is already assigned to a different port.\n");
				return;
			}
#endif
			/*
			if (disneyport == port) {
				if (wantDisney) {
					showPort(port_index);
					return;
				}
				DISNEY_Close();
				if (!DISNEY_HasInit()) disneyport = 0;
			}
			else if (wantDisney && DISNEY_HasInit()) {
				WriteOut("Disney is already assigned to a different port.\n");
				return;
			}
			*/
		}

		// Recreate the port with the new type.
		switch (desired_type) {
		case PARALLEL_PORT_TYPE::TYPE_INVALID:
		case PARALLEL_PORT_TYPE::TYPE_DISABLED:
			parallelports[port_index] = nullptr;
			break;
		case PARALLEL_PORT_TYPE::TYPE_FILE:
			parallelports[port_index] = new CFileLPT(port_index,
			                                         commandLine);
			if (!parallelports[port_index]->InstallationSuccessful) {
				delete parallelports[port_index];
				parallelports[port_index] = nullptr;
			}
			break;
#ifdef C_DIRECTLPT
		case PARALLEL_PORT_TYPE::TYPE_DIRECT:
			parallelports[port_index] = new CDirectLPT(port_index,
			                                           commandLine);
			if (!parallelports[port_index]->InstallationSuccessful) {
				delete parallelports[port_index];
				parallelports[port_index] = nullptr;
			}
			break;
#endif
#if C_PRINTER
		case PARALLEL_PORT_TYPE::TYPE_PRINTER:
			if (!CPrinterRedir::printer_used) {
				parallelports[port_index] = new CPrinterRedir(port_index,
				                                              commandLine);
				if (parallelports[port_index]->InstallationSuccessful) {
					CPrinterRedir::printer_used = true;
				} else {
					delete parallelports[port_index];
					parallelports[port_index] = nullptr;
				}
			}
			break;
#endif
		/*
		case PARALLEL_PORT_TYPE::TYPE_DISNEY:
			if (!DISNEY_HasInit()) {
				DISNEY_Init(parallel_baseaddr[port_index]);
				if (DISNEY_HasInit()) disneyport=port;
			}
			break;
		*/
		default:
			parallelports[port_index] = nullptr;
			LOG_WARNING("PARALLEL: Unknown parallel port type %d", desired_type);
			break;
		}
		if (parallelports[port_index] != nullptr) {
			parallelports[port_index]->parallelType = desired_type;
			parallelports[port_index]->commandLineString = commandLineString;
		}
		delete commandLine;
		showPort(port_index);
		return;
	}

	// Show help.
	MoreOutputStrings output(*this);
	output.AddString(MSG_Get("PROGRAM_PARALLEL_HELP_LONG"));
	output.Display();

	/*
	WriteOut("Views or changes the parallel port settings.\n"
			 "\n"
			 "PARALLEL [port] [type] [option]\n"
			 "\n"
			 " port   Parallel port number (between 1 and 3).\n"
			 " type   Type of the parallel port, including:\n        ");
	for (int x = 0; x < PARALLEL_PORT_TYPE::TYPE_INVALID; x++) {
		WriteOut("%s", parallel_type_names[static_cast<PARALLEL_PORT_TYPE>(x)].c_str());
		if (x < PARALLEL_PORT_TYPE::TYPE_INVALID - 1) WriteOut(", ");
	}
	WriteOut("\n"
			 " option Parallel options, if any (see [parallel] section of the configuration).\n");
	*/
}

void PARALLEL::AddMessages()
{
	MSG_Add("PROGRAM_PARALLEL_HELP_LONG",
	        "Manage the parallel ports.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]parallel[reset] [color=white][PORT#][reset]                   List all or specified ([color=white]1[reset], [color=white]2[reset], [color=white]3[reset]) ports.\n"
	        "  [color=light-green]parallel[reset] [color=white]PORT#[reset] [color=light-cyan]DEVICE[reset] [settings]   Attach specified device to the given port.\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]DEVICE[reset]  one of: [color=light-cyan]REALLPT[reset], [color=light-cyan]FILE[reset], [color=light-cyan]PRINTER[reset], or [color=light-cyan]DISABLED[reset]\n"
	        "\n"
	        "  Optional settings for each [color=light-cyan]DEVICE[reset]:\n"
#if defined(WIN32)
	        "  For [color=light-cyan]REALLPT[reset] : REALBASE (required), ECPBASE\n"
#elif defined(LINUX)
			"  For [color=light-cyan]REALLPT[reset] : REALPORT (required)\n"
#endif
	        "  For [color=light-cyan]FILE[reset]    : TYPE (DEV:<DEVNAME> or APPEND:<FILE>), TIMEOUT:<MILLISECONDS>,\n"
			"                ADDFF, ADDLF, CP:<CODEPAGE NUMBER>\n"
	        "  For [color=light-cyan]PRINTER[reset] : see [printer] section of the configuration\n"
	        "\n"
	        "Examples:\n"
#if defined(WIN32)
	        "  [color=light-green]PARALLEL[reset] [color=white]1[reset] [color=light-cyan]REALLPT[reset] REALBASE:378           : Use real printer with base address 378\n"
#elif defined(LINUX)
			"  [color=light-green]PARALLEL[reset] [color=white]1[reset] [color=light-cyan]REALLPT[reset] REALPORT:/dev/parport0 : Use real printer on /dev/parport0\n"
#endif
	        "  [color=light-green]PARALLEL[reset] [color=white]2[reset] [color=light-cyan]FILE[reset] DEV:LPT1                  : Forward data to device LPT1\n"
	        "  [color=light-green]PARALLEL[reset] [color=white]2[reset] [color=light-cyan]FILE[reset] APPEND:printout.txt ADDLF : Append to file printout.txt,\n"
			"                                              add automatic linefeeds\n"
	        "  [color=light-green]PARALLEL[reset] [color=white]1[reset] [color=light-cyan]PRINTER[reset]                        : Printer emulation\n");
	MSG_Add("PROGRAM_PARALLEL_SHOW_PORT", "LPT%d: %s %s\n");
	MSG_Add("PROGRAM_PARALLEL_BAD_PORT",
	        "Must specify a numeric port value between 1 and %d, inclusive.\n");
	MSG_Add("PROGRAM_PARALLEL_BAD_TYPE", "Type must be one of the following:\n");
	MSG_Add("PROGRAM_PARALLEL_INDENTED_LIST", "  %s\n");
}
