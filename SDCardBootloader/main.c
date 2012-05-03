/*-------------------------------------------------------------------------/
/  Stand-alone MMC boot loader  R0.01
/--------------------------------------------------------------------------/
/
/  Copyright (C) 2010, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/ Heavily modified by Brian Schiffer for use in TicTocTrac
/ Primary change: used avr/boot.h functions for easier & safer operation
/
/--------------------------------------------------------------------------/
/ Dec 6, 2010  R0.01  First release
/--------------------------------------------------------------------------/
/ This is a stand-alone MMC/SD boot loader for megaAVRs. It requires a 4KB
/ boot section for code, four GPIO pins for MMC/SD as shown in sch.jpg and
/ nothing else. To port the boot loader into your project, follow the
/ instruction sdescribed below.
/
/ 1. Setup the hardware. Attach a memory card socket to the any GPIO port
/    where you like. Select boot size at least 4KB for the boot loader with
/    BOOTSZ fuses and enable boot loader with BOOTRST fuse.
/
/ 2. Setup the software. Change the four port definitions in the asmfunc.S.
/    Change MCU_TARGET, BOOT_ADR and MCU_FREQ in the Makefile. The BOOT_ADR
/    is a BYTE address of boot section in the flash. Build the boot loader
/    and write it to the device with a programmer.
/
/ 3. Build the application program and output it in binary form instead of
/    hex format. Rename the file "app.bin" and put it into the memory card.
/
/ 4. Insert the card and turn the target power on. When the boot loader found
/    the application file, the file is written into the flash memory prior to
/    start the application program. On-board LED lights (if exist) during
/    the flash programming operation.
/
/-------------------------------------------------------------------------*/


#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <string.h>
#include "pff.h"
#include <util/delay.h>
#include <avr/boot.h>
#include <avr/interrupt.h>

void flash_erase (DWORD);				/* Erase a flash page (asmfunc.S) */
void flash_write (DWORD, const BYTE*);	/* Program a flash page (asmfunc.S) */

FATFS Fatfs;				/* Petit-FatFs work area */
BYTE Buff[SPM_PAGESIZE];	/* Page data buffer */

int main (void)
{
	uint8_t sreg; // Store the interrupts
	uint16_t i;
	DWORD fa;	/* Flash address */
	WORD br;	/* Bytes read */
	// Disable WDT
	MCUSR &= ~(1 << WDRF); 
   	wdt_disable();
	// Disable Interrupts
	sreg = SREG;
	cli();

	pf_mount(&Fatfs);	/* Initialize file system */
	if (pf_open("tictoc.bin") == FR_OK) {	/* Open application file */
		for (fa = 0; fa < BOOT_ADR; fa += SPM_PAGESIZE) {	/* Update all application pages */
			boot_page_erase_safe(fa); // Erase a page
			memset(Buff, 0xFF, SPM_PAGESIZE);	/* Clear buffer */
			pf_read(Buff, SPM_PAGESIZE, &br);	/* Read a page data */
			if (br) {
				uint8_t *buf = Buff;
				for (i=0; i<SPM_PAGESIZE; i+=2){ //Load into page buffer
					uint16_t w = *buf++;
					w += (*buf++) << 8;
					boot_page_fill(fa+i,w);
				}				
				boot_page_write_safe(fa); // Write the page of data
			}
		}
		
	}
	boot_rww_enable_safe(); // Re-enable RWW before running
	SREG = sreg; 		// Re-eanble interrupts

	if (pgm_read_word(0) != 0xFFFF){		/* Start application if exist */
		asm volatile("jmp 0000");
	}
		
	while(1); // Nothing to do here
}

