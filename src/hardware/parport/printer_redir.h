
#ifndef DOSBOX_PRREDIR_H
#define DOSBOX_PRREDIR_H

#include "dosbox.h"
#include "parport.h"
#include "printer_if.h"

class CPrinterRedir final : public CParallel {
public:
#if C_PRINTER
	static bool printer_used;
#endif

	CPrinterRedir(const CPrinterRedir&) = delete;            // prevent copying
	CPrinterRedir& operator=(const CPrinterRedir&) = delete; // prevent assignment

	CPrinterRedir(uint8_t nr, CommandLine* cmd);
	~CPrinterRedir();
	
	uint8_t Read_PR() override;
	uint8_t Read_CON() override;
	uint8_t Read_SR() override;

	void Write_PR(uint8_t) override;
	void Write_CON(uint8_t) override;
	void Write_IOSEL(uint8_t) override;

	bool Putchar(uint8_t) override;

	void handleUpperEvent(uint16_t type) override;
};

#endif // DOSBOX_PRREDIR_H
