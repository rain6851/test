/* sysMpc8540Smc.c - Support for the MPC8540's DDR Memory Controller */

/*
 * Copyright (c) 2004, 2005, 2013 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

/* Copyright 2004-2005 Motorola, Inc., All Rights Reserved */

/*
modification history
--------------------
01i,07may13,d_l  Fixed build warnings.
01h,14oct05,efb  Added DDR11 errata workaround to memory controller init.
01g,28aug05,efb  Removed core freq global. Moved core freq calc to sysLib.c
01f,15aug05,efb  Added dynamically calculated sysCpuBusSpd routine.
01e,12aug05,pjh  removed sysCpuBusSpd routine. 
01d,17mar05,cak  Added a 200 microsecond delay following initialization of
		 clock configuration registers before enabling the DRAM banks,
		 as per Section 9.6.1 in the MPC8540 Reference Manual.
01c,07feb05,cak  Removed unnecessary code. 
01b,06jan05,cak  Adjusted to hard-coded SPD values to match the memory
		 currently on the mv3100.
01a,27sep04,cak  Initial writing.
*/

/*
DESCRIPTION
This file contains the routines used to calculate the proper configuration
values for the MPC8540's DDR Memory Controller using information contained
in the Serial Presence Detect (SPD) EEPROMs. The SPD information is used to
determine memory timing, bank sizes and starting addresses.

CAVEATS:
This code executes very early in the startup sequence (called from romInit.s),
before the image has been copied to RAM (assuming a non-ROM image). As such,
this file must be added to the BOOT_EXTRA list in the Makefile to prevent it
from being compressed during kernel generation. Additionally, extreme caution
must be exercised when modifying these routines to prevent the use of absolute
memory addresses which reference locations in the RAM-relocated image. These
locations are invalid and references to them will cause unpredictable behavior.
Absolute memory addresses are generated by the compiler when referencing tables,
static data structures and global variables. All of these must be avoided. In
some places in the code, nested if-else constructs are used to avoid the jump
table created when a simple switch construct is used.
*/

/* includes */

#include "vxWorks.h"
#include "config.h"
#include "sysLib.h"
#include "sdramSpd.h"
#include "mpc8540Smc.h"
#include "mpc8540.h"

/* defines */

/*
 * Nanoseconds to Picoseconds
 *
 * The following macro is used to convert the SPD values for 
 * tRP (byte 27), tRRD (byte 28), and tRCD (byte 29) from
 * nanoseconds to picoseconds.  The input to this macro (N) is
 * the actual value read from the SPD at these locations.
 */
 
#define PSECS(N) (((((N) >> 2) & 0x3F) * 1000) + (((N) & 0x3) * 250))
#define MPC8540_DDR_SDRAM_TYPE_DFLT  2	     /* SDRAM Type - 2 for DDR */
#define DEVDISR_DDR_DISABLE  0x00010000UL
#define DDRDLLCR_SET_VAL     0x810C0000UL
#define DDRDLLCR_RESULT_VAL  (DDRDLLCR_SET_VAL | 0x0000010CUL)

/* typedefs */

/* globals */

/* locals */

/* forward declarations */

UINT32 sysCpuBusSpd (void);
LOCAL STATUS sysMpc8540SdramSpeedInit (MPC8540_SMC * pSmcReg, 
				       UCHAR * spdArray[]);
LOCAL UINT32 sysMpc8540SdramSizeInit (MPC8540_SMC * pSmcReg,
			              UCHAR * spdArray[]);
STATUS sysMpc8540GetSpdData (UINT32 spdAddr, UINT32 offset, 
                             UINT32 dataSize, UCHAR * spdData);
UINT32 sysMpc8540SdramInit ();
LOCAL void sysMpc8540SdramSet (MPC8540_SMC * pSmcReg);

/* external references */

IMPORT STATUS sysMotI2cRead (UINT32 devAddr, UINT32 devOffset,
                             UINT32 devAddrBytes, UCHAR *spdData,
                             UINT32 numBytes);
IMPORT void   sysI2cUsDelay (UINT32 delay);

/******************************************************************************
*
* sysCpuBusSpd - routine to calculate the CPU bus speed
*
* This routine returns the returns the CPU bus speed computed by reading the 
* PCI_A_SPD in the "PCI/PMC Control/Status Register" and the CCB clock 
* multiplier which can be found in the 8540 POR PLL Status Register (PORPLLSR).
* The CCB clock speed is a multiple of the PCI bus speed.
* The core frequencey is also calculated and stored globally.
*
* RETURNS: The bus speed (in Hz) or ERROR if PLAT ratio is invalid.
*/

UINT32 sysCpuBusSpd ()
    {
    UINT32 sysClkFreq;
    UINT32 platRatio = 1;
    UINT32 ccsrPorpllsr = 0;
    UINT32 regVal = 0;
    UINT32 oscillatorFreq = 0;

    regVal = *(UINT8 *)BRD_PCI_BUS_A_STAT_REG & 
             BRD_PCI_BUS_A_STAT_SPD_MASK;

    if (regVal == 0)
        {
        oscillatorFreq = FREQ_33_MHZ;
        }
    else if (regVal == 1)
        {
        oscillatorFreq = FREQ_66_MHZ;
        }
    else if (regVal == 2)
        {
        oscillatorFreq = FREQ_100_MHZ;
        }
    else 
        {
        oscillatorFreq = FREQ_133_MHZ;
        }
       
    ccsrPorpllsr = CCSR_READ32 (CCSBAR, CCSR_PORPLLSR);
    platRatio = CCSR_PORPLLSR_PLAT_RATIO_VAL (ccsrPorpllsr);
    sysClkFreq = oscillatorFreq * platRatio;

    return (sysClkFreq);
    }

/******************************************************************************
*
* sysMpc8540DDRClockPeriod - routine to calculate the DDR clock period 
*
* The CCB clock runs at the same frequency as the DDR data clock, which is 
* twice the actual DDR clock (since it is double data rate memory). All the 
* memory controller parameters are defined in terms of the half-rate clock,
* so this is the value we use to calculate the memory controller parameters.
*
* RETURNS: The clock period (in ps).
*/

UINT32 sysMpc8540DDRClockPeriod ()
    {
    UINT64 freq = (UINT64)sysCpuBusSpd ();
    UINT64 period = 1000000000000ULL;
    
    period = period/freq;

    return (UINT32)(period * 2);
    }

/******************************************************************************
*
* sysMpc8540SdramSpeedInit - calculate the MPC8540's timing parameters 
*
* The purpose of this function is to initialize the timing parameters in the 
* MPC8540's DDR memory controller. 
*
* RETURNS: OK, or ERROR if the SPD is not valid. 
*/

LOCAL STATUS sysMpc8540SdramSpeedInit
    (
    MPC8540_SMC * pSmcReg,  /* points to caller's SMC register storage area */
    UCHAR * spdArray[]      /* array of pointers to SPD buffers */
    )
    {
    UCHAR * pData;       	/* address of SPD buffer */
    UINT32 spdValue;       	/* Temp SPD Data Value */
    int spd; 		  	/* Bank index counter */
    int validSpd;               /* SPD Validity flag */
    UINT32 clockPeriod;    	/* PicoSeconds per clock cycle */
    UINT32 casLatency;		/* Cas Latency of SDRAM */
    UINT32 sdramTcl;		/* Cas Latency */
    UINT32 sdramRefreshInterval;	/* Refresh Interval */
    UINT32 sdramTrcd;		/* tRCD Activate to Command */
    UINT32 sdramTrp;		/* tRP Precharge Command Period */
    UINT32 sdramTras;		/* tRAS Minimum Row Active Time */
    UINT32 sdramTrrd;		/* tRRD Activate Bank A to Activate Bank B */
    UINT32 sdramTrfc;		/* tRFC Refresh Command Period */
    UINT32 sdramEcc;		/* Module Configuration Type */
    UINT32 sdramRegDimm;	/* Module Attributes */
    UINT32 spdCas;
    UINT32 spdRcl;
    UINT32 spdRcl2;
    UINT32 casTemp = 0;
    UINT32 cycleTime;
 
    clockPeriod = sysMpc8540DDRClockPeriod();
    cycleTime = clockPeriod/100;

    validSpd = FALSE;
    casLatency = 0x0;
    sdramTcl = 0;	
    sdramRefreshInterval = 0;
    sdramTrcd = 0;
    sdramTrp = 0;
    sdramTras = 0;
    sdramTrrd = 0;
    sdramTrfc = 0;
    sdramEcc = 0xFF;
    sdramRegDimm = 0xFF;

    /* Start with the first SPD and check all SPDs */

    for (spd = 0; spd < 4; spd+=2)
        {

	/* Make sure we have a valid SPD */

        pData = spdArray[spd];

        if (pData != NULL)
            { 
            validSpd = TRUE;

            /*
             * Get the SDRAM Device Attributes CAS latency. The CL
             * parameter must be the greater of all SDRAM CAS 
	     * latencies.
             */

            spdCas = pData[SPD_CL_INDEX];
	    spdRcl = pData[SPD_TCYC_RCL_INDEX];
	    spdRcl2 = pData[SPD_TCYC_RCL2_INDEX];

	    /* determine the minimum CAS latency */

	    if (spdCas & 0x20)		/* maximum CAS = 3.5 */
		{
		casTemp = 35;
		if (spdCas & 0x08 && spdRcl2 <= cycleTime)
		    {
		    casTemp = 25;
		    }
		else if (spdCas & 0x10 && spdRcl <= cycleTime)
		    {
		    casTemp = 30;
		    }
		}
	    else if (spdCas & 0x10)	/* maximum CAS = 3.0 */
		{
		casTemp = 30;
		if (spdCas & 0x04 && spdRcl2 <= cycleTime)
		    {
		    casTemp = 20;
		    }
		else if (spdCas & 0x08 && spdRcl <= cycleTime)
		    {
		    casTemp = 25;
		    }
		}
	    else if (spdCas & 0x08)	/* maximum CAS = 2.5 */
		{
		casTemp = 25;
		if (spdCas & 0x02 && spdRcl2 <= cycleTime)
		    {
		    casTemp = 15;
		    }
		else if (spdCas & 0x04 && spdRcl <= cycleTime)
		    {
		    casTemp = 20;
		    }
		}
	    else if (spdCas & 0x04)	/* maximum CAS = 2.0 */
		{
		casTemp = 20;
		if (spdCas & 0x02 && spdRcl2 <= cycleTime)
		    {
		    casTemp = 10;
		    }
		else if (spdCas & 0x04 && spdRcl <= cycleTime)
		    {
		    casTemp = 15;
		    }
		}
	    else if (spdCas & 0x02)	/* maximum CAS = 1.5 */
		{
		casTemp = 15;
		if (spdCas & 0x04 && spdRcl <= cycleTime)
		    {
		    casTemp = 10;
		    }
		}
	    else if (spdCas & 0x01)	/* maximum CAS = 1.0 */
		{
		casTemp = 10;
		}

	    if (casTemp > casLatency)
		casLatency = casTemp;
 
            /*
             * Get the SDRAM Minimum RAS Pulse Width (tRAS).  
             */

            spdValue = pData[SPD_TRAS_INDEX];
            if (spdValue > sdramTras)
                sdramTras = spdValue;
   
            /*
             * Get the SDRAM Minimum Row Precharge Time (tRP).
             */

            spdValue = pData[SPD_TRP_INDEX];
            if (spdValue > sdramTrp)
                sdramTrp = spdValue;
   
            /*
             * Get the SDRAM Minimum RAS to CAS Delay (tRCD).
             */

            spdValue = pData[SPD_TRCD_INDEX];
            if (spdValue > sdramTrcd) 
                sdramTrcd = spdValue;

            /*
             * Get the Refresh Command Period (tRFC). 
             */

            spdValue = pData[SPD_TRFC_INDEX];
            if (spdValue > sdramTrfc) 
                sdramTrfc = spdValue;

	    /*
	     * Get the SDRAM Activate Bank A to Activate Bank B time (tRRD).
	     */

	    spdValue = pData[SPD_TRRD_INDEX];
	    if (spdValue > sdramTrrd)
		sdramTrrd = spdValue;

	    /*
	     * Get the SDRAM Refresh Rate/Type. We must clear bit 7 - Self
	     * Refresh Flag.
	     */

	    spdValue = (pData[SPD_REFRESH_RATE_INDEX] & ~SPD_REF_SELF_REFRESH);
	    if (spdValue == 0x0) 
		{
		if (sdramRefreshInterval < 0x3)
		    sdramRefreshInterval = spdValue;
		}
	    else if (spdValue < 0x3) 
		     {
		     if (sdramRefreshInterval < 0x3)
		         {
			 if (spdValue > sdramRefreshInterval)
			 sdramRefreshInterval = spdValue;
			 }
		     }
	    else 
		{
		if (sdramRefreshInterval >= 0x3)
		    {
		    if (spdValue > sdramRefreshInterval)
			sdramRefreshInterval = spdValue;
		    }
		else sdramRefreshInterval = spdValue;
		}

            /*
             * Get the SDRAM error detection/correction type. 
             * Use the worst type found.
             */

            spdValue = pData[SPD_DIMM_TYPE_INDEX];
            if (spdValue < sdramEcc) 
                sdramEcc = spdValue;

	    /*
	     * Get the SDRAM Module Attributes - registered or not.
	     */

	    spdValue = pData[SPD_ATTRIBUTES_INDEX];
	    if ((spdValue & SPD_MOD_ATTR_REG) < sdramRegDimm)
		sdramRegDimm = spdValue;

	    }
	}

    /*  
     * tRAS equals the Minimum RAS Pulse Width divided by the 
     * ClockPeriod.   
     */
   
    if (sdramTras)
        {
        sdramTras = sdramTras * 1000;  /* convert to picoseconds */	
        sdramTras = ((sdramTras + (clockPeriod - 1)) / clockPeriod);
        }
    else
        sdramTras = MPC8540_DDR_SDRAM_TRAS_DFLT;
    
    /* 
     * tRP equals the Minimum Row Precharge Time divided by the
     * ClockPeriod. 
     */

    if (sdramTrp)
        {
        sdramTrp = PSECS(sdramTrp);  /* convert to picoseconds */
        sdramTrp = ((sdramTrp + (clockPeriod - 1)) / clockPeriod);
        }
    else
        sdramTrp = MPC8540_DDR_SDRAM_TRP_DFLT;
   
    /*
     * tRCD equals the Minimum RAS to CAS delay divided by the 
     * ClockPeriod.   
     */
     
    if (sdramTrcd)
        {
	sdramTrcd = PSECS(sdramTrcd);  /* convert to picoseconds */
        sdramTrcd = ((sdramTrcd + (clockPeriod - 1)) / clockPeriod);
        }
    else
        sdramTrcd = MPC8540_DDR_SDRAM_TRCD_DFLT;
   
    /*  
     * tRFC equals the Refresh Command Period divided by the 
     * ClockPeriod.   
     */
   
    if (sdramTrfc)
        {
	sdramTrfc = sdramTrfc * 1000;  /* convert to picoseconds */
        sdramTrfc = ((sdramTrfc + (clockPeriod - 1)) / clockPeriod);
	sdramTrfc = (sdramTrfc - 8);
        }
    else
        sdramTrfc = MPC8540_DDR_SDRAM_TRFC_DFLT; 

    /*
     * tRRD equals the Minimum Row Active to Row Active delay divided by the 
     * ClockPeriod.   
     */
     
    if (sdramTrrd)
        {
	sdramTrrd = PSECS(sdramTrrd);  /* convert to picoseconds */
        sdramTrrd = ((sdramTrrd + (clockPeriod - 1)) / clockPeriod);
        }
    else
        sdramTrrd = MPC8540_DDR_SDRAM_TRRD_DFLT;
  
    /*
     * CAS latency
     */

    if (casLatency == 10)
	casLatency = 1;		/* CAS = 1 */
    else if (casLatency == 15)
	casLatency = 2;		/* CAS = 1.5 */
    else if (casLatency == 20)
	casLatency = 3;		/* CAS = 2.0 */
    else if (casLatency == 25)
	casLatency = 4;		/* CAS = 2.5 */
    else if (casLatency == 30)
	casLatency = 5;		/* CAS = 3.0 */
    else if (casLatency == 35)
	casLatency = 6;		/* CAS = 3.5 */
    else
	casLatency = 4;		/* default to CAS = 2.5 */	

    /* 
     * Determine if we are ECC capable or not.  If the DIMMs are ECC
     * capable, as indicated in the SPD (Serial Presence Detect), then
     * allow the user to enable, or disable, ECC support in the system
     * memory controller.  Disabling ECC support will result in a slight
     * performance increase, since for writes that are smaller than 64
     * bits a full read-modify-write does not need to be performed for
     * a write to SDRAM. 
     */

#ifdef INCLUDE_ECC
    if (sdramEcc == 0x2)
	sdramEcc = 0x1;
    else
#endif /* INCLUDE_ECC */
	sdramEcc = 0x0;

    /*
     * Determine if we are registered or unbuffered.
     */

    if (sdramRegDimm & SPD_MOD_ATTR_REG)
	sdramRegDimm = 0x1;
    else
	sdramRegDimm = 0x0;

    /*
     * Calculate the Refresh Interval Count Value
     *
     * The Refresh Period is given in the SPD in microseconds, the MPC8540 
     * must be programmed with the number of cycles.  Therefore, we
     * calculate the refresh count value in the following way:
     *
     * Refresh Interval Value(cycles) = Refresh Period(ps)/clockPeriod(ps)
     *
     * According to the MPC8540 Reference manual, page 9-39 states
     * that "To ensure that the latency caused by a memory transaction
     * does not violate the device refresh period, it is recommended
     * that the programmed value of REFINT be less than that required
     * by the SDRAM".  So, after we calculate the value required by the
     * SDRAM we will decrease that value by 0x10, as per the recommendation. 
     */

    if (sdramRefreshInterval == SPD_REF_NORMAL)
	sdramRefreshInterval = SPD_REF_NORMAL_MS;
    else if (sdramRefreshInterval == SPD_REF_DIV4)
	sdramRefreshInterval = SPD_REF_DIV4_MS;
    else if (sdramRefreshInterval == SPD_REF_DIV2)
	sdramRefreshInterval = SPD_REF_DIV2_MS;
    else if (sdramRefreshInterval == SPD_REF_2X)
	sdramRefreshInterval = SPD_REF_2X_MS;
    else if (sdramRefreshInterval == SPD_REF_4X)
	sdramRefreshInterval = SPD_REF_4X_MS;
    else if (sdramRefreshInterval == SPD_REF_8X)
	sdramRefreshInterval = SPD_REF_8X_MS;
    else
	sdramRefreshInterval = SPD_REF_NORMAL_MS;

    sdramRefreshInterval = ((sdramRefreshInterval * 1000) / clockPeriod);
    sdramRefreshInterval = (sdramRefreshInterval - 0x10);

    /*
     * Load the parameters. 
     */

    pSmcReg->sdramTmngConfig1 |=
	((sdramTrp << DDR_SDRAM_TIMING_CFG_1_PRETOACT_BIT) |
	 (sdramTras << DDR_SDRAM_TIMING_CFG_1_ACTTOPRE_BIT) |
	 (sdramTrcd << DDR_SDRAM_TIMING_CFG_1_ACTTORW_BIT) |
	 (casLatency << DDR_SDRAM_TIMING_CFG_1_CASLAT_BIT) |
	 (sdramTrfc << DDR_SDRAM_TIMING_CFG_1_REFREC_BIT) |
	 (MPC8540_DDR_SDRAM_WRREC_VAL << DDR_SDRAM_TIMING_CFG_1_WRREC_BIT) |
	 (sdramTrrd << DDR_SDRAM_TIMING_CFG_1_ACTTOACT_BIT) |
	 (MPC8540_DDR_SDRAM_WRTORD_VAL >> DDR_SDRAM_TIMING_CFG_1_WRTORD_BIT));

    pSmcReg->sdramTmngConfig2 |=
	((DDR_SDRAM_WR_DATA_DELAY_VAL << 
	  DDR_SDRAM_TIMING_CFG_2_WR_DATA_DELAY_BIT) |
	 (DDR_SDRAM_ACSM_VAL << DDR_SDRAM_TIMING_CFG_2_ACSM_BIT) |
	 (DDR_SDRAM_CPO_VAL << DDR_SDRAM_TIMING_CFG_2_CPO_BIT));

    pSmcReg->sdramCtrlConfig |=
	((MPC8540_DDR_SDRAM_TYPE_DFLT << DDR_SDRAM_SDRAM_CFG_SDRAM_TYPE_BIT) |
	 (sdramRegDimm << DDR_SDRAM_SDRAM_CFG_RD_EN_BIT) |
	 (sdramEcc << DDR_SDRAM_SDRAM_CFG_ECC_EN_BIT) |
	 (DDR_SDRAM_2T_EN_VAL << DDR_SDRAM_SDRAM_CFG_2T_EN_BIT) |
	 (DDR_SDRAM_NCAP_VAL << DDR_SDRAM_SDRAM_CFG_NCAP_BIT) |
	 (DDR_SDRAM_DYN_PWR_VAL << DDR_SDRAM_SDRAM_CFG_DYN_PWR_BIT) |
	 (DDR_SDRAM_SREN_VAL << DDR_SDRAM_SDRAM_CFG_SREN_BIT));

    pSmcReg->sdramIntervalConfig |=
        ((DDR_SDRAM_BSTOPRE_VAL << DDR_SDRAM_SDRAM_INTERVAL_BSTOPRE_BIT) |
         (sdramRefreshInterval << DDR_SDRAM_SDRAM_INTERVAL_REFINT_BIT));

    if (validSpd == TRUE) 
	return(OK);
    else 
	return(ERROR);
    }

/******************************************************************************
*
* sysMpc8540SdramSizeInit - determine chip select starting & ending addresses  
*
* This function's purpose is to determine the values to be programmed into 
* the chip select registers for all banks of memory. 
*
* RETURNS: Size of system memory. 
*/

LOCAL UINT32 sysMpc8540SdramSizeInit
    (
    MPC8540_SMC * pSmcReg,  /* points to caller's SMC register storage area */
    UCHAR * spdArray[]      /* array of pointers to SPD buffers */
    )
    {
    UCHAR * pData;          /* SPD pointer for current bank */
    int bank;      	    /* Bank index counter */
    UINT32 numDimmBanks;    /* Number of DIMM Banks supported */
    UINT32 numRow;	    /* Number of row addresses */
    UINT32 numColumn;	    /* Number of column addresses */
    UINT32 numDevBanks;	    /* Number of device banks */
    UINT32 sdramSize = 0x0; /* SDRAM Size for the bank */
    UINT32 *pSdramCsxBnds;  /* Pointer to chip select bounds register */
    UINT32 *pSdramCsxCfg;   /* Pointer to chip select Config register */
    UINT32 memorySize;	    /* Amount of system memory */
    UINT32 startAddress;    /* start address for chip select */
    UINT32 endAddress;	    /* end address for chip select */

    /* Start with system memory at 0x0 */

    memorySize = 0x0;

    /* Fill the registers with bank data from the SPD devices. */

    for (bank = 0; bank < 4; bank+=2)
        {
        if ((pData = spdArray[bank]) != NULL)
            {

	    /* 
	     * retrieve the row, column, device bank, 
	     * and DIMM bank information 
	     */

	    numRow = (pData[SPD_ROW_ADDR_INDEX] & 0x0F);
	    numColumn = (pData[SPD_COL_ADDR_INDEX] & 0x0F);
	    numDevBanks = pData[SPD_DEV_BANKS_INDEX];
	    numDimmBanks = pData[SPD_NUM_DIMMBANKS_INDEX];

	    /* check for valid number of DIMM banks */

	    if (numDimmBanks < 1 || numDimmBanks > 2)
              numDimmBanks = 1;

            /* 
	     * Get the size of the Bank in MB 
	     *
	     * bankSize = (Total Row Addresses * Total Column Addresses *
     	     *              Number Device Banks * Data Width in Bytes);
     	     *
     	     * Data Width in Bytes is the data width of the DIMM which is
     	     * equivalent to the data width of the bus.  It is hard-coded to
     	     * 8 since for PowerPC we will always have a 64-bit or 8 byte bus.
	     */

            sdramSize = ((1 << numRow) * (1 << numColumn) *
                         numDevBanks * 8);

	    /* determine the start address for the chip select */

	    startAddress = ((memorySize & 0xFF000000) >> 8);

	    /* update the memorySize variable to the size */

	    memorySize += sdramSize;

	    /* determine the end address for the chip select */

	    endAddress = (((memorySize - 1) & 0xFF000000) >> 24);
 
	    /* Chip Select Bounds register */
 
            pSdramCsxBnds = &pSmcReg->csnBnds[bank];
	    *pSdramCsxBnds = (startAddress | endAddress);

	    /* Chip Select Configuration register */

	    pSdramCsxCfg = &pSmcReg->csnCfg[bank];
	    *pSdramCsxCfg |= ((((numColumn - 0x8) & 0x07) << 
			       DDR_SDRAM_CSn_CONFIG_COLUMN_BIT) |
                              (((numRow-0xC) & 0x07) << 
				DDR_SDRAM_CSn_CONFIG_ROW_BIT) |
			     (UINT32)(1 << DDR_SDRAM_CSn_CONFIG_CS_EN_BIT));

	    /* configure next chip select, if DIMM has two banks */

	    if (numDimmBanks == 2)
                {
                if ((pData[SPD_ROW_ADDR_INDEX] & 0xF0) != 0)
                    {
                    numRow = ((pData[SPD_ROW_ADDR_INDEX] & 0xF0) >> 4);
                    }
                if ((pData[SPD_COL_ADDR_INDEX] & 0xF0) != 0)
                    {
                    numColumn = ((pData[SPD_COL_ADDR_INDEX] & 0xF0) >> 4);
                    }
		sdramSize = ((1 << numRow) * (1 << numColumn) *
                    	     numDevBanks * 8);
	
	        /* determine the start address for the chip select */

	        startAddress = ((memorySize & 0xFF000000) >> 8);

	        /* update the memorySize variable to the size */

	        memorySize += sdramSize;

	        /* determine the end address for the chip select */

	        endAddress = (((memorySize - 1) & 0xFF000000) >> 24);
 
	        /* Chip Select Bounds register */
 
                pSdramCsxBnds = &pSmcReg->csnBnds[bank+1];
	        *pSdramCsxBnds = (startAddress | endAddress);

	        /* Chip Select Configuration register */

	        pSdramCsxCfg = &pSmcReg->csnCfg[bank+1];
	        *pSdramCsxCfg |= ((((numColumn-0x8) & 0x07) << 
				   DDR_SDRAM_CSn_CONFIG_COLUMN_BIT) |
                                  (((numRow-0xC) & 0x07) << 
				    DDR_SDRAM_CSn_CONFIG_ROW_BIT) |
		                (UINT32)(1 << DDR_SDRAM_CSn_CONFIG_CS_EN_BIT));
	        }
	    }
        }                       

    /*
     * Configure the mode for each of the chip selects.  The mode
     * can be either page mode or auto precharge mode.
     */

    pSdramCsxCfg = &pSmcReg->csnCfg[0];
    *pSdramCsxCfg |= (DDR_SDRAM_CS_0_MODE << DDR_SDRAM_CSn_CONFIG_AP_EN_BIT);
    pSdramCsxCfg = &pSmcReg->csnCfg[1];
    *pSdramCsxCfg |= (DDR_SDRAM_CS_1_MODE << DDR_SDRAM_CSn_CONFIG_AP_EN_BIT);
    pSdramCsxCfg = &pSmcReg->csnCfg[2];
    *pSdramCsxCfg |= (DDR_SDRAM_CS_2_MODE << DDR_SDRAM_CSn_CONFIG_AP_EN_BIT);
    pSdramCsxCfg = &pSmcReg->csnCfg[3];
    *pSdramCsxCfg |= (DDR_SDRAM_CS_3_MODE << DDR_SDRAM_CSn_CONFIG_AP_EN_BIT);

    /* Configure the DDR SDRAM Mode Configuration Register */

    pSmcReg->sdramModeConfig |=
	((DDR_SDRAM_SDMODE_VAL << DDR_SDRAM_SDRAM_MODE_SDMODE_BIT) |
	 (DDR_SDRAM_ESDMODE_VAL << DDR_SDRAM_SDRAM_MODE_ESDMODE_BIT));

    return(memorySize); 	 /* return the amount of system memory */
    }

/******************************************************************************
*
* sysMpc8540GetSpdData - read and validate the spd information.
*
* This function reads the contents of the caller specified serial presence
* detect EEPROM and validates the checksum.
*
* RETURNS: TRUE if the SPD contents are valid, FALSE if not.
*
*/

STATUS sysMpc8540GetSpdData
    (
    UINT32 spdAddr,      /* SROM address for current bank */
    UINT32 offset,       /* first byte of SROM to read */
    UINT32 dataSize,     /* number of SROM bytes to read */
    UCHAR *spdData       /* address of caller's SPD buffer */
    )
    {
    register UCHAR checksum = 0;    /* running checksum */
    register int   index;        /* index into SPD data buffer */

    if ( sysMotI2cRead (spdAddr, offset, 1, spdData, dataSize) == OK)
        {
        for (index = 0; index < SPD_CHECKSUM_INDEX; index++)
            checksum += spdData[index];
        checksum %= 256; 
        if (checksum == spdData[SPD_CHECKSUM_INDEX])
            return (OK);
        }
    return (ERROR);
    }

/******************************************************************************
*
* sysMpc8540SdramInit - calculate the proper MPC8540 smc initialization values
*
* This function reads the serial presence detect EEPROM(s) and calculates the
* proper values for configuring the MPC8540 DDR Memory Controller.
*
* RETURNS: Size of memory configured 
*/

UINT32 sysMpc8540SdramInit (void) 
    {
    MPC8540_SMC * pSmcReg;   	    /* pointer to SMC register storage area */
    MPC8540_SMC smcReg;	     	    /* SMC register storage area */
    UINT32 memorySize;	     	    /* Size of system memory */
    int x;		     	    /* bank index counter */

    UCHAR * spdPtrs[MPC8540_CHIP_SELECTS];    		/* spd buffer ptrs */
    UCHAR spdData[(MPC8540_CHIP_SELECTS / 2) * SPD_SIZE];  /* spd data */
    UCHAR * pBfr = &spdData[0];                         /* temp buffer ptr */
    UCHAR * pData;
    BOOL valid = FALSE;

    /* Initialize SMC register storage area with default values or 0 */

    pSmcReg = &smcReg;
    for (x = 0; x < MPC8540_CHIP_SELECTS; x++)
	{
	pSmcReg->csnBnds[x] = 0;
	pSmcReg->csnCfg[x] = 0;
	}
    pSmcReg->sdramTmngConfig1 = 0;
    pSmcReg->sdramTmngConfig2 = 0;
    pSmcReg->sdramCtrlConfig = 0;
    pSmcReg->sdramModeConfig = 0;
    pSmcReg->sdramIntervalConfig = 0;

#if (MPC8540_BOARD_TYPE == MPC8540_BOARD_TYPE_SBC)

    /* Read the SPDs */

    spdPtrs[0] = NULL;

    /* read the spd data into the current buffer and validate */

    if (sysMpc8540GetSpdData (SPD_EEPROM_I2C_ADDR, 0, SPD_SIZE, pBfr) 
	== OK)
	{
        valid = TRUE;

        /* save current buffer address and advance to the next buffer */

        spdPtrs[0] = pBfr;
        pBfr += SPD_SIZE;
        }
    else
        {

        /* 
         * There is not a valid SPD at this address so just advance to 
         * the next buffer and leave the spdPtrs[] pointing at NULL.
         */
 
         pBfr += SPD_SIZE;
         }

    spdPtrs[1] = NULL;
    spdPtrs[2] = NULL;
    spdPtrs[3] = NULL;
#endif

    if (!valid)
	{
	spdPtrs[0] = &spdData[0];
	pData = spdPtrs[0];
#if (MPC8540_BOARD_TYPE == MPC8540_BOARD_TYPE_XMC)
	#if (SODIMM_TYPE == MUSHKIN_3726966)
		pData[SPD_CL_INDEX]		= 0x0C;	/* byte 18 */
		pData[SPD_TRAS_INDEX]		= 0x2A;	/* byte 30 */
		pData[SPD_TRP_INDEX]		= 0x48;	/* byte 27 */
		pData[SPD_TRCD_INDEX]		= 0x48;	/* byte 29 */
		pData[SPD_REFRESH_RATE_INDEX]	= 0x82;	/* byte 12 */
		pData[SPD_DIMM_TYPE_INDEX]	= 0x00;	/* byte 11 */
		pData[SPD_ATTRIBUTES_INDEX]	= 0x20;	/* byte 21 */
		pData[SPD_NUM_DIMMBANKS_INDEX]	= 0x02;	/* byte 5 */
		pData[SPD_ROW_ADDR_INDEX]	= 0x0D;	/* byte 3 */
		pData[SPD_COL_ADDR_INDEX]	= 0x0A;	/* byte 4 */
		pData[SPD_DEV_BANKS_INDEX]	= 0x04;	/* byte 17 */
		pData[SPD_TRFC_INDEX]		= 0x48;	/* byte 42 */
		pData[SPD_TRRD_INDEX]		= 0x30;	/* byte 28 */
		pData[SPD_DEV_WIDTH_INDEX]	= 0x08;	/* byte 13 */
		pData[SPD_TCYC_RCL_INDEX]	= 0x75;	/* byte 23 */
		pData[SPD_TCYC_RCL2_INDEX]	= 0x00;	/* byte 25 */
	#elif (SODIMM_TYPE == MICRON_MT9VDDT6472HG)
		pData[SPD_CL_INDEX]		= 0x0C;	/* byte 18 */
		pData[SPD_TRAS_INDEX]		= 0x2A;	/* byte 30 */
		pData[SPD_TRP_INDEX]		= 0x48;	/* byte 27 */
		pData[SPD_TRCD_INDEX]		= 0x48;	/* byte 29 */
		pData[SPD_REFRESH_RATE_INDEX]	= 0x82;	/* byte 12 */
		pData[SPD_DIMM_TYPE_INDEX]	= 0x02;	/* byte 11 */
		pData[SPD_ATTRIBUTES_INDEX]	= 0x20;	/* byte 21 */
		pData[SPD_NUM_DIMMBANKS_INDEX]	= 0x01;	/* byte 5 */
		pData[SPD_ROW_ADDR_INDEX]	= 0x0D;	/* byte 3 */
		pData[SPD_COL_ADDR_INDEX]	= 0x0B;	/* byte 4 */
		pData[SPD_DEV_BANKS_INDEX]	= 0x04;	/* byte 17 */
		pData[SPD_TRFC_INDEX]		= 0x48;	/* byte 42 */
		pData[SPD_TRRD_INDEX]		= 0x2A;	/* byte 28 */
		pData[SPD_DEV_WIDTH_INDEX]	= 0x08;	/* byte 13 */
		pData[SPD_TCYC_RCL_INDEX]	= 0x75;	/* byte 23 */
		pData[SPD_TCYC_RCL2_INDEX]	= 0x00;	/* byte 25 */
	#endif		
#elif (MPC8540_BOARD_TYPE == MPC8540_BOARD_TYPE_SBC)

                pData[SPD_CL_INDEX]             = 0x0C; /* byte 18 */
                pData[SPD_TRAS_INDEX]           = 0x2A; /* byte 30 */
                pData[SPD_TRP_INDEX]            = 0x48; /* byte 27 */
                pData[SPD_TRCD_INDEX]           = 0x48; /* byte 29 */
                pData[SPD_REFRESH_RATE_INDEX]   = 0x82; /* byte 12 */
                pData[SPD_DIMM_TYPE_INDEX]      = 0x02; /* byte 11 */
                pData[SPD_ATTRIBUTES_INDEX]     = 0x24; /* byte 21 */
                pData[SPD_NUM_DIMMBANKS_INDEX]  = 0x01; /* byte 5 */
                pData[SPD_ROW_ADDR_INDEX]       = 0x0D; /* byte 3 */
                pData[SPD_COL_ADDR_INDEX]       = 0x0B; /* byte 4 */
                pData[SPD_DEV_BANKS_INDEX]      = 0x04; /* byte 17 */
                pData[SPD_TRFC_INDEX]           = 0x48; /* byte 42 */
                pData[SPD_TRRD_INDEX]           = 0x30; /* byte 28 */
                pData[SPD_DEV_WIDTH_INDEX]      = 0x08; /* byte 13 */
                pData[SPD_TCYC_RCL_INDEX]       = 0x75; /* byte 23 */
                pData[SPD_TCYC_RCL2_INDEX]      = 0x00; /* byte 25 */
#endif
	spdPtrs[2] = NULL;
	}

    /* calculate the SMC initialization parameters */

    sysMpc8540SdramSpeedInit (pSmcReg, &spdPtrs[0]);

    memorySize = sysMpc8540SdramSizeInit (pSmcReg, &spdPtrs[0]);

    /* Program the SDRAM registers */

    sysMpc8540SdramSet(pSmcReg);

    return (memorySize);
    }

/******************************************************************************
*
* sysMpc8540SdramSet - Program the DDR memory controller registers.
*
* This function writes to the DDR memory controller registers of the MPC8540.  
*
* RETURNS: N/A
*/

LOCAL void sysMpc8540SdramSet
    (
    MPC8540_SMC * pSmcReg 	/* points to SMC register storage area */
    )
    {
    UINT32 tries = 0;
    UINT32 reg = 0;

    CCSR_WRITE32 (CCSBAR, CCSR_CS0_BNDS, pSmcReg->csnBnds[0]);
    CCSR_WRITE32 (CCSBAR, CCSR_CS1_BNDS, pSmcReg->csnBnds[1]);
    CCSR_WRITE32 (CCSBAR, CCSR_CS2_BNDS, pSmcReg->csnBnds[2]);
    CCSR_WRITE32 (CCSBAR, CCSR_CS3_BNDS, pSmcReg->csnBnds[3]);
    CCSR_WRITE32 (CCSBAR, CCSR_CS0_CONFIG, pSmcReg->csnCfg[0]);
    CCSR_WRITE32 (CCSBAR, CCSR_CS1_CONFIG, pSmcReg->csnCfg[1]);
    CCSR_WRITE32 (CCSBAR, CCSR_CS2_CONFIG, pSmcReg->csnCfg[2]);
    CCSR_WRITE32 (CCSBAR, CCSR_CS3_CONFIG, pSmcReg->csnCfg[3]);
    CCSR_WRITE32 (CCSBAR, CCSR_TIMING_CFG_1, pSmcReg->sdramTmngConfig1);
    CCSR_WRITE32 (CCSBAR, CCSR_TIMING_CFG_2, pSmcReg->sdramTmngConfig2);
    CCSR_WRITE32 (CCSBAR, CCSR_DDR_SDRAM_CFG, pSmcReg->sdramCtrlConfig);
    CCSR_WRITE32 (CCSBAR, CCSR_DDR_SDRAM_MODE, pSmcReg->sdramModeConfig);
    CCSR_WRITE32 (CCSBAR, CCSR_DDR_SDRAM_INTERVAL, 
		  pSmcReg->sdramIntervalConfig);

    CCSR_WRITE32 (CCSBAR, 0x2e58, (DDR_SDRAM_SBET_VAL << 
				   DDR_SDRAM_ERR_SBE_SBET_BIT));

    CCSR_WRITE32 (CCSBAR, CCSR_DDRDLLCR, DDRDLLCR_SET_VAL);

    /*
     * Per errata DDR11 of MPC8540 PowerQUICC III Device Errata r0.6, delay
     * 200 usecs to provide enough time to override the DDR DLL and ensure the
     * Course Adjustment and TAP Selection values are written as expected. If
     * the values are not written as expected, cycle the state of the DDR
     * controller until the values are latched. If the values don't latch
     * within 100 cycles, attempt to proceed. This workaround is to resolve a 
     * problem where the DDR PLL fails to lock memory clocks upon power-on-
     * reset under low temp and low voltage supply conditions.
     */

    sysI2cUsDelay (200);

    reg = CCSR_READ32 (CCSBAR, CCSR_DEVDISR);

    while ((CCSR_READ32 (CCSBAR, CCSR_DDRDLLCR) != DDRDLLCR_RESULT_VAL) && 
           tries++ < 100)
        {
        CCSR_WRITE32 (CCSBAR, CCSR_DEVDISR, reg | DEVDISR_DDR_DISABLE); 
        sysI2cUsDelay (1);

        CCSR_WRITE32 (CCSBAR, CCSR_DEVDISR, reg); 
        }

    sysI2cUsDelay (200);

    /* enable the DDR SDRAM interface */

    CCSR_WRITE32 (CCSBAR, CCSR_DDR_SDRAM_CFG,
	(CCSR_READ32 (CCSBAR, CCSR_DDR_SDRAM_CFG) | 
		    (UINT32) (1 << DDR_SDRAM_CFG_MEM_EN_BIT)));

    sysI2cUsDelay (200);
    }