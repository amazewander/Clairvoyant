
/* ------------------------------------------------------------------------------------------------
 *                                          Includes
 * ------------------------------------------------------------------------------------------------
 */

#include <string.h>

//#include "hal_assert.h"
//#include "hal_board.h"
//#include "hal_defs.h"
//#include "hal_drivers.h"
#include "hal_dma.h"
//#include "hal_mcu.h"
//#include "hal_types.h"
#if defined POWER_SAVING
#include "OSAL.h"
#include "OSAL_PwrMgr.h"
#endif

#include "spi.h"

/* ------------------------------------------------------------------------------------------------
 *                                           Local Variables
 * ------------------------------------------------------------------------------------------------
 */

/* Convenient way to save RAM space when running with both SPI & UART ports enabled
 * but only using one at a time with a run-time choice */
//static __no_init uint16 spiRxBuf[SPI_MAX_PKT_LEN];
//static __no_init uint8  spiRxDat[SPI_MAX_PKT_LEN];
//static __no_init uint8  spiTxPkt[SPI_MAX_PKT_LEN]; /* Can be trimmed as per requirement*/

static volatile spiLen_t spiTxLen;  /* Total length of payload data bytes to transmit */

static volatile uint8 spiRdyIsr = 0;  /* Sticky flag from SPI_RDY_IN ISR for background polling */
static volatile uint8 writeActive = 0;

static spiLen_t dbgFcsByte;  /* Saves FCS byte when FCS mismatch occurs for debug */
#ifdef RBA_UART_TO_SPI
uint16 badFcsPktCount = 0;/* Counts number of FCS failures */
#endif

/* ------------------------------------------------------------------------------------------------
 *                                          Global Functions
 * ------------------------------------------------------------------------------------------------
 */

void HalUART_DMAIsrSPI(void);
/**************************************************************************************************
 * @fn          HalUARTInitSPI
 *
 * @brief       Initialize the SPI UART Transport.
 *
 * input parameters
 *
 * None.
 *
 * output parameters
 *
 * None.
 *
 * @return      None.
 */
void HalUARTInitSPI(void)
{
  PERCFG |= HAL_UART_PERCFG_BIT;     /* Set UART1 I/O to Alt. 2 location on P1 */
  PxSEL |= HAL_UART_Px_SEL_M;        /* SPI-Master peripheral select */
  UxCSR = 0;                         /* Mode is SPI-Master Mode */
  UxGCR =  15;                       /* Cfg for the max Rx/Tx baud of 2-MHz */
  UxBAUD = 255;
  UxUCR = UCR_FLUSH;                 /* Flush it */
  UxGCR |= BV(5);                    /* Set bit order to MSB */
  UxGCR |= BV(7);                    /* Set polarity positive */

  P2DIR &= ~P2DIR_PRIPO;
  P2DIR |= HAL_UART_PRIPO;
  
  DMAIE = 1; // enable dma interrupt

  /* Setup Tx by DMA */
  halDMADesc_t *ch = HAL_DMA_GET_DESC1234( HAL_SPI_CH_TX );
  
  /* Abort any pending DMA operations (in case of a soft reset) */
  HAL_DMA_ABORT_CH( HAL_SPI_CH_TX );

  /* The start address of the destination */
  HAL_DMA_SET_DEST( ch, DMA_UxDBUF );

  /* Using the length field to determine how many bytes to transfer */
  HAL_DMA_SET_VLEN( ch, HAL_DMA_VLEN_USE_LEN );

  /* One byte is transferred each time */
  HAL_DMA_SET_WORD_SIZE( ch, HAL_DMA_WORDSIZE_BYTE );

  /* The bytes are transferred 1-by-1 on Tx Complete trigger */
  HAL_DMA_SET_TRIG_MODE( ch, HAL_DMA_TMODE_SINGLE );
  HAL_DMA_SET_TRIG_SRC( ch, DMATRIG_TX );

  /* The source address is incremented by 1 byte after each transfer */
  HAL_DMA_SET_SRC_INC( ch, HAL_DMA_SRCINC_1 );
  HAL_DMA_SET_SOURCE( ch, spiTxPkt );

  /* The destination address is constant - the Tx Data Buffer */
  HAL_DMA_SET_DST_INC( ch, HAL_DMA_DSTINC_0 );

  /* The DMA Tx done is serviced by ISR */
  HAL_DMA_SET_IRQ( ch, HAL_DMA_IRQMASK_ENABLE );

  /* Xfer all 8 bits of a byte xfer */
  HAL_DMA_SET_M8( ch, HAL_DMA_M8_USE_8_BITS );

  /* DMA has highest priority for memory access */
  HAL_DMA_SET_PRIORITY( ch, HAL_DMA_PRI_ABSOLUTE );
}

/**************************************************************************************************
 * @fn          HalUARTOpenSPI
 *
 * @brief       Start the SPI UART Transport.
 *
 * input parameters
 *
 * None.
 *
 * output parameters
 *
 * None.
 *
 * @return      None.
 */
void HalUARTOpenSPI()
{

  UxCSR |= CSR_RE;

  PxIFG = 0;
  PxIF = 0;
  IENx |= IEN_BIT;

  (void)dbgFcsByte; /* To suppress warning */
}

/**************************************************************************************************
 * @fn          HalUARTWriteSPI
 *
 * @brief       Transmit data bytes as a SPI packet.
 *
 * input parameters
 *
 * @param       buf - pointer to the memory of the data bytes to send.
 * @param       len - the length of the data bytes to send.
 *
 * output parameters
 *
 * None.
 *
 * @return      Zero for any error; otherwise, 'len'.
 */
spiLen_t HalUARTWriteSPI(uint8 *buf, spiLen_t len)
{  
  (void)memcpy(spiTxPkt, buf, len);
  return HalUARTWriteSPIDirect(len);
}

spiLen_t HalUARTWriteSPIDirect(spiLen_t len)
{  
  while (spiTxLen != 0)
  {
    //return 0;
  }
        
  spiTxLen = len;
  writeActive = 1;
  
  halDMADesc_t *ch = HAL_DMA_GET_DESC1234(HAL_SPI_CH_TX);
  //HAL_DMA_SET_LEN(ch, SPI_PKT_LEN(spiTxPkt)); /* DMA TX might need padding */
  HAL_DMA_SET_LEN(ch, len); /* DMA TX might need padding */

  /* Abort any pending DMA operations */
  HAL_DMA_ABORT_CH( HAL_SPI_CH_TX );
  HAL_DMA_ARM_CH(HAL_SPI_CH_TX);

  asm("NOP"); asm("NOP"); asm("NOP"); asm("NOP"); asm("NOP");
  asm("NOP"); asm("NOP"); asm("NOP"); asm("NOP");

  HAL_DMA_MAN_TRIGGER(HAL_SPI_CH_TX);
  return len;
}

void writeCompleteCallback(void){
  /* Must hold CSn active until Tx flushes */
    while (UxCSR & CSR_ACTIVE );
    writeActive = 0;
    spiTxLen = 0;
    //UxDBUF = 0x00;     /* Clear the garbage */
}