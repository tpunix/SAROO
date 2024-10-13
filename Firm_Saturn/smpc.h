/*
 * Sega Saturn cartridge flash tool
 * by Anders Montonen, 2012
 *
 * Original software by ExCyber
 * Graphics routines by Charles MacDonald
 *
 * Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0)
 */


#ifndef SMPC_H_
#define SMPC_H_


/******************************************************************************/

#define SMPC_BASE   0x20100000

#define IREG0       REG8(SMPC_BASE+0x01)
#define IREG1       REG8(SMPC_BASE+0x03)
#define IREG2       REG8(SMPC_BASE+0x05)
#define IREG3       REG8(SMPC_BASE+0x07)
#define IREG4       REG8(SMPC_BASE+0x09)
#define IREG5       REG8(SMPC_BASE+0x0b)
#define IREG6       REG8(SMPC_BASE+0x0d)
#define COMREG      REG8(SMPC_BASE+0x1f)
#define OREG0       REG8(SMPC_BASE+0x21)
#define OREG1       REG8(SMPC_BASE+0x23)
#define OREG2       REG8(SMPC_BASE+0x25)
#define OREG3       REG8(SMPC_BASE+0x27)
#define OREG4       REG8(SMPC_BASE+0x29)
#define OREG5       REG8(SMPC_BASE+0x2b)
#define OREG6       REG8(SMPC_BASE+0x2d)
#define OREG7       REG8(SMPC_BASE+0x2f)
#define OREG8       REG8(SMPC_BASE+0x31)
#define OREG9       REG8(SMPC_BASE+0x33)
#define OREG10      REG8(SMPC_BASE+0x35)
#define OREG11      REG8(SMPC_BASE+0x37)
#define OREG12      REG8(SMPC_BASE+0x39)
#define OREG13      REG8(SMPC_BASE+0x3b)
#define OREG14      REG8(SMPC_BASE+0x3d)
#define OREG15      REG8(SMPC_BASE+0x3f)
#define OREG16      REG8(SMPC_BASE+0x41)
#define OREG17      REG8(SMPC_BASE+0x43)
#define OREG18      REG8(SMPC_BASE+0x45)
#define OREG19      REG8(SMPC_BASE+0x47)
#define OREG20      REG8(SMPC_BASE+0x49)
#define OREG21      REG8(SMPC_BASE+0x4b)
#define OREG22      REG8(SMPC_BASE+0x4d)
#define OREG23      REG8(SMPC_BASE+0x4f)
#define OREG24      REG8(SMPC_BASE+0x51)
#define OREG25      REG8(SMPC_BASE+0x53)
#define OREG26      REG8(SMPC_BASE+0x55)
#define OREG27      REG8(SMPC_BASE+0x57)
#define OREG28      REG8(SMPC_BASE+0x59)
#define OREG29      REG8(SMPC_BASE+0x5b)
#define OREG30      REG8(SMPC_BASE+0x5d)
#define OREG31      REG8(SMPC_BASE+0x5f)
#define SR          REG8(SMPC_BASE+0x61)
#define SF          REG8(SMPC_BASE+0x63)
#define PDR1        REG8(SMPC_BASE+0x75)
#define PDR2        REG8(SMPC_BASE+0x77)
#define DDR1        REG8(SMPC_BASE+0x79)
#define DDR2        REG8(SMPC_BASE+0x7b)
#define IOSEL       REG8(SMPC_BASE+0x7d)
#define IOSEL1      1
#define IOSEL2      2

#define EXLE        REG8(SMPC_BASE+0x7f)
#define EXLE1       1
#define EXLE2       2

/* SMPC commands */
#define MSHON       0x00
#define SSHON       0x02
#define SSHOFF      0x03
#define SNDON       0x06
#define SNDOFF      0x07
#define CDON        0x08
#define CDOFF       0x09
#define SYSRES      0x0d
#define CKCHG352    0x0e
#define CKCHG320    0x0f
#define INTBACK     0x10
#define SETTIME     0x16
#define SETSMEM     0x17
#define NMIREQ      0x18
#define RESENAB     0x19
#define RESDISA     0x1a


/******************************************************************************/

#define SCU_BASE 0x25FE0000

/* SCU A-BUS */
#define ASR0        REG(SCU_BASE+0xb0)
#define ASR1        REG(SCU_BASE+0xb4)
#define AREF        REG(SCU_BASE+0xb8)

/* SCU DMA */
#define SCU_DMA_BASE  (SCU_BASE+0x0000)

#define SD0R        REG(SCU_BASE+0x00)
#define SD0W        REG(SCU_BASE+0x04)
#define SD0C        REG(SCU_BASE+0x08)
#define SD0AD       REG(SCU_BASE+0x0c)
#define SD0EN       REG(SCU_BASE+0x10)
#define SD0MD       REG(SCU_BASE+0x14)
#define SD1R        REG(SCU_BASE+0x20)
#define SD1W        REG(SCU_BASE+0x24)
#define SD1C        REG(SCU_BASE+0x28)
#define SD1AD       REG(SCU_BASE+0x2c)
#define SD1EN       REG(SCU_BASE+0x30)
#define SD1MD       REG(SCU_BASE+0x34)
#define SD2R        REG(SCU_BASE+0x40)
#define SD2W        REG(SCU_BASE+0x44)
#define SD2C        REG(SCU_BASE+0x48)
#define SD2AD       REG(SCU_BASE+0x4c)
#define SD2EN       REG(SCU_BASE+0x50)
#define SD2MD       REG(SCU_BASE+0x54)
#define SDSTOP      REG(SCU_BASE+0x60)
#define SDSTAT      REG(SCU_BASE+0x7c)

typedef struct {
	u32 src;
	u32 dst;
	u32 count;
	u32 addv;
	u32 enable;
	u32 mode;
	u32 unk_18;
	u32 unk_1c;
}SCUDMA_REG;

#define SCUDMA ((volatile SCUDMA_REG *)(SCU_DMA_BASE))


/******************************************************************************/

/* CPU SCI port */
#define SMR         REG8(0xfffffe00)
#define BRR         REG8(0xfffffe01)
#define SCR         REG8(0xfffffe02)
#define TDR         REG8(0xfffffe03)
#define SSR         REG8(0xfffffe04)
#define RDR         REG8(0xfffffe05)

/* CPU Timer */
#define TIER        REG8(0xfffffe10)
#define FTCSR       REG8(0xfffffe11)
#define FRCH        REG8(0xfffffe12)
#define FRCL        REG8(0xfffffe13)
#define OCRAH       REG8(0xfffffe14)
#define OCRAL       REG8(0xfffffe15)
#define OCRBH       REG8(0xfffffe14)
#define OCRBL       REG8(0xfffffe15)
#define TCR         REG8(0xfffffe16)
#define TOCR        REG8(0xfffffe17)
#define ICRH        REG8(0xfffffe18)
#define ICRL        REG8(0xfffffe19)


/* CPU UBR */
#define BARA        REG  (0xffffff40)
#define BAMRA       REG  (0xffffff44)
#define BBRA        REG16(0xffffff48)
#define BARB        REG  (0xffffff60)
#define BAMRB       REG  (0xffffff64)
#define BBRB        REG16(0xffffff68)
#define BDRB        REG  (0xffffff70)
#define BDMRB       REG  (0xffffff74)
#define BRCR        REG16(0xffffff78)

/* CPU DMAC */
#define SAR0        REG (0xffffff80)
#define DAR0        REG (0xffffff84)
#define TCR0        REG (0xffffff88)
#define CHCR0       REG (0xffffff8c)
#define VCRDMA0     REG (0xffffffa0)
#define DRCR0       REG8(0xffffff71)
#define SAR1        REG (0xffffff90)
#define DAR1        REG (0xffffff94)
#define TCR1        REG (0xffffff98)
#define CHCR1       REG (0xffffff9c)
#define VCRDMA1     REG (0xffffffa8)
#define DRCR1       REG8(0xffffff72)
#define DMAOR       REG (0xffffffb0)



#endif /* SMPC_H_ */


