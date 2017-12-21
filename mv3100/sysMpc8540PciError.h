/* sysMpc8540PciError.h -  MPC8540 Error Handling for the PCI */

/* Copyright 2005 Motorola Inc. All Rights Reserved */

/*

modification history
--------------------
01a,18aug05,efb  New file.
*/

/*
DESCRIPTION

Register bit definitions, masks, and legal field values for the 
PCI/X error interface are defined in this file.

*/

#ifndef	INCsysMpc8540PciErrorh
#define	INCsysMpc8540PciErrorh

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

/* includes */

#include "vxWorks.h"

/* defines */

/* PCI Error Detect Register */

#define PCI_ERROR_DETECT_MULT_ERR_BIT    0  
#define PCI_ERROR_DETECT_MULT_ERR_MASK   \
        (0x80000000UL >> PCI_ERROR_DETECT_MULT_ERR_BIT)

#define PCI_ERROR_DETECT_ADDR_PARITY_BIT  21  
#define PCI_ERROR_DETECT_ADDR_PARITY_MASK \
        (0x80000000UL >> PCI_ERROR_DETECT_ADDR_PARITY_BIT)

#define PCI_ERROR_DETECT_RCVD_SERR_BIT    22
#define PCI_ERROR_DETECT_RCVD_SERR_MASK   \
        (0x80000000UL >> PCI_ERROR_DETECT_RCVD_SERR_BIT)

#define PCI_ERROR_DETECT_MSTR_PERR_BIT    23
#define PCI_ERROR_DETECT_MSTR_PERR_MASK   \
        (0x80000000UL >> PCI_ERROR_DETECT_MSTR_PERR_BIT)

#define PCI_ERROR_DETECT_TRGT_PERR_BIT    24
#define PCI_ERROR_DETECT_TRGT_PERR_MASK   \
        (0x80000000UL >> PCI_ERROR_DETECT_TRGT_PERR_BIT)
  
#define PCI_ERROR_DETECT_MSTR_ABORT_BIT   25
#define PCI_ERROR_DETECT_MSTR_ABORT_MASK  \
        (0x80000000UL >> PCI_ERROR_DETECT_MSTR_ABORT_BIT)

#define PCI_ERROR_DETECT_TRGT_ABORT_BIT   26
#define PCI_ERROR_DETECT_TRGT_ABORT_MASK  \
        (0x80000000UL >> PCI_ERROR_DETECT_TRGT_ABORT_BIT)

#define PCI_ERROR_DETECT_OWMSV_BIT        27
#define PCI_ERROR_DETECT_OWMSV_MASK       \
        (0x80000000UL >> PCI_ERROR_DETECT_OWMSV_BIT)

#define PCI_ERROR_DETECT_ORMSV_BIT        28
#define PCI_ERROR_DETECT_ORMSV_MASK       \
        (0x80000000UL >> PCI_ERROR_DETECT_ORMSV_BIT)

#define PCI_ERROR_DETECT_IRMSV_BIT        29
#define PCI_ERROR_DETECT_IRMSV_MASK       \
        (0x80000000UL >> PCI_ERROR_DETECT_IRMSV_BIT)

#define PCI_ERROR_DETECT_SCM_BIT          30
#define PCI_ERROR_DETECT_SCM_MASK         \
        (0x80000000UL >> PCI_ERROR_DETECT_SCM_BIT)

#define PCI_ERROR_DETECT_TOE_BIT          31 
#define PCI_ERROR_DETECT_TOE_MASK         \
        (0x80000000UL >> PCI_ERROR_DETECT_TOE_BIT)

/* PCI Error Capture Disable Register */

#define PCI_ERROR_DISABLE_ADDR_PARITY_BIT  21  
#define PCI_ERROR_DISABLE_ADDR_PARITY_MASK \
        (0x80000000UL >> PCI_ERROR_DISABLE_ADDR_PARITY_BIT)

#define PCI_ERROR_DISABLE_RCVD_SERR_BIT    22
#define PCI_ERROR_DISABLE_RCVD_SERR_MASK   \
        (0x80000000UL >> PCI_ERROR_DISABLE_RCVD_SERR_BIT)

#define PCI_ERROR_DISABLE_MSTR_PERR_BIT    23
#define PCI_ERROR_DISABLE_MSTR_PERR_MASK   \
        (0x80000000UL >> PCI_ERROR_DISABLE_MSTR_PERR_BIT)

#define PCI_ERROR_DISABLE_TRGT_PERR_BIT    24
#define PCI_ERROR_DISABLE_TRGT_PERR_MASK   \
        (0x80000000UL >> PCI_ERROR_DISABLE_TRGT_PERR_BIT)
  
#define PCI_ERROR_DISABLE_MSTR_ABORT_BIT   25
#define PCI_ERROR_DISABLE_MSTR_ABORT_MASK  \
        (0x80000000UL >> PCI_ERROR_DISABLE_MSTR_ABORT_BIT)

#define PCI_ERROR_DISABLE_TRGT_ABORT_BIT   26
#define PCI_ERROR_DISABLE_TRGT_ABORT_MASK  \
        (0x80000000UL >> PCI_ERROR_DISABLE_TRGT_ABORT_BIT)

#define PCI_ERROR_DISABLE_OWMSV_BIT        27
#define PCI_ERROR_DISABLE_OWMSV_MASK       \
        (0x80000000UL >> PCI_ERROR_DISABLE_OWMSV_BIT)

#define PCI_ERROR_DISABLE_ORMSV_BIT        28
#define PCI_ERROR_DISABLE_ORMSV_MASK       \
        (0x80000000UL >> PCI_ERROR_DISABLE_ORMSV_BIT)

#define PCI_ERROR_DISABLE_IRMSV_BIT        29
#define PCI_ERROR_DISABLE_IRMSV_MASK       \
        (0x80000000UL >> PCI_ERROR_DISABLE_IRMSV_BIT)

#define PCI_ERROR_DISABLE_SCM_BIT          30
#define PCI_ERROR_DISABLE_SCM_MASK         \
        (0x80000000UL >> PCI_ERROR_DISABLE_SCM_BIT)

#define PCI_ERROR_DISABLE_TOE_BIT          31 
#define PCI_ERROR_DISABLE_TOE_MASK         \
        (0x80000000UL >> PCI_ERROR_DISABLE_TOE_BIT)

/* PCI Error Interrupt Enable Register */

#define PCI_ERROR_INT_ADDR_PARITY_BIT  21  
#define PCI_ERROR_INT_ADDR_PARITY_MASK \
        (0x80000000UL >> PCI_ERROR_INT_ADDR_PARITY_BIT)

#define PCI_ERROR_INT_RCVD_SERR_BIT    22
#define PCI_ERROR_INT_RCVD_SERR_MASK   \
        (0x80000000UL >> PCI_ERROR_INT_RCVD_SERR_BIT)

#define PCI_ERROR_INT_MSTR_PERR_BIT    23
#define PCI_ERROR_INT_MSTR_PERR_MASK   \
        (0x80000000UL >> PCI_ERROR_INT_MSTR_PERR_BIT)

#define PCI_ERROR_INT_TRGT_PERR_BIT    24
#define PCI_ERROR_INT_TRGT_PERR_MASK   \
        (0x80000000UL >> PCI_ERROR_INT_TRGT_PERR_BIT)
  
#define PCI_ERROR_INT_MSTR_ABORT_BIT   25
#define PCI_ERROR_INT_MSTR_ABORT_MASK  \
        (0x80000000UL >> PCI_ERROR_INT_MSTR_ABORT_BIT)

#define PCI_ERROR_INT_TRGT_ABORT_BIT   26
#define PCI_ERROR_INT_TRGT_ABORT_MASK  \
        (0x80000000UL >> PCI_ERROR_INT_TRGT_ABORT_BIT)

#define PCI_ERROR_INT_OWMSV_BIT        27
#define PCI_ERROR_INT_OWMSV_MASK       \
        (0x80000000UL >> PCI_ERROR_INT_OWMSV_BIT)

#define PCI_ERROR_INT_ORMSV_BIT        28
#define PCI_ERROR_INT_ORMSV_MASK       \
        (0x80000000UL >> PCI_ERROR_INT_ORMSV_BIT)

#define PCI_ERROR_INT_IRMSV_BIT        29
#define PCI_ERROR_INT_IRMSV_MASK       \
        (0x80000000UL >> PCI_ERROR_INT_IRMSV_BIT)

#define PCI_ERROR_INT_SCM_BIT          30
#define PCI_ERROR_INT_SCM_MASK         \
        (0x80000000UL >> PCI_ERROR_INT_SCM_BIT)

#define PCI_ERROR_INT_TOE_BIT          31 
#define PCI_ERROR_INT_TOE_MASK         \
        (0x80000000UL >> PCI_ERROR_INT_TOE_BIT)

/* PCI Error Attributes Capture Register */

#define PCI_ERROR_ATTR_HWORD_BYTE_ENB_BIT    0
#define PCI_ERROR_ATTR_HWORD_BYTE_ENB_MASK   \
        (0xF0000000UL >> PCI_ERROR_ATTR_HWORD_BYTE_ENB_BIT)

#define PCI_ERROR_ATTR_LWORD_BYTE_ENB_BIT    4
#define PCI_ERROR_ATTR_LWORD_BYTE_ENB_MASK   \
        (0xF0000000UL >> PCI_ERROR_ATTR_LWORD_BYTE_ENB_BIT)

#define PCI_ERROR_ATTR_HIGH_PARITY_BIT       8
#define PCI_ERROR_ATTR_HIGH_PARITY_MASK      \
        (0x80000000UL >> PCI_ERROR_ATTR_HIGH_PARITY_BIT)

#define PCI_ERROR_ATTR_LOW_PARITY_BIT        9
#define PCI_ERROR_ATTR_LOW_PARITY_MASK       \
        (0x80000000UL >> PCI_ERROR_ATTR_LOW_PARITY_BIT)

#define PCI_ERROR_ATTR_ERR_SRC_BIT           11
#define PCI_ERROR_ATTR_ERR_SRC_MASK          \
        (0xF8000000UL >> PCI_ERROR_ATTR_ERR_SRC_BIT)

#define PCI_ERROR_ATTR_PCI_CMD_BIT           16
#define PCI_ERROR_ATTR_PCI_CMD_MASK          \
        (0xF8000000UL >> PCI_ERROR_ATTR_PCI_CMD_BIT)

#define PCI_ERROR_ATTR_VLD_BIT               31
#define PCI_ERROR_ATTR_VLD_MASK              \
        (0x80000000UL >> PCI_ERROR_ATTR_VLD_BIT)

#define PCI_ERROR_SRC_PCI                  0x00
#define PCI_ERROR_SRC_LOCAL_BUS            0x04
#define PCI_ERROR_SRC_CONFIG_SPACE         0x08
#define PCI_ERROR_SRC_BOOT_SEQNCR          0x0A
#define PCI_ERROR_SRC_RAPIDIO              0x0C
#define PCI_ERROR_SRC_PROC_INSTR           0x10
#define PCI_ERROR_SRC_PROC_DATA            0x11
#define PCI_ERROR_SRC_DMA                  0x15
#define PCI_ERROR_SRC_RDC                  0x16
#define PCI_ERROR_SRC_SAP                  0x17
#define PCI_ERROR_SRC_TSEC1                0x18
#define PCI_ERROR_SRC_TSEC2                0x19
#define PCI_ERROR_SRC_FEC                  0x1A
#define PCI_ERROR_SRC_RAPIDIO_MSG          0x1C
#define PCI_ERROR_SRC_RAPIDIO_DBELL        0x1D
#define PCI_ERROR_SRC_RAPIDIO_WRITE        0x1E

STATUS sysMpc8540PciErrorIntEnable (void);

#ifdef __cplusplus

    }
#endif /* __cplusplus */

#endif /* sysMpc8540PciErrorh */