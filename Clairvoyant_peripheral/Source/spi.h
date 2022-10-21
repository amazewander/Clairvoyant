#include "hal_dma.h"


/* ------------------------------------------------------------------------------------------------
 *                                           Constants
 * ------------------------------------------------------------------------------------------------
 */

/* The bridge utility application uses 4 DMA channels for 2 sets of Rx/Tx by DMA, and thereby also
 * implements a special DMA ISR handler, which accomodates this extra definition
 */
#if !defined HAL_SPI_CH_RX
#define HAL_SPI_CH_RX              HAL_DMA_CH_RX
#endif
#if !defined HAL_SPI_CH_TX
#define HAL_SPI_CH_TX              HAL_DMA_CH_TX
#endif

#define SPI_MAX_PKT_LEN            256

/* Overlay the SPI Rx/Tx buffers on the UART Rx/Tx buffers when running with a run-time selectable
 * UART port by SPI or by UART in order to save XDATA space
 */
#if !defined HAL_SPI_ON_UART_BUFS
#define HAL_SPI_ON_UART_BUFS       FALSE
#endif

/* UxCSR - USART Control and Status Register */
#define CSR_MODE                   0x80
#define CSR_RE                     0x40
#define CSR_SLAVE                  0x20
#define CSR_FE                     0x10
#define CSR_ERR                    0x08
#define CSR_RX_BYTE                0x04
#define CSR_TX_BYTE                0x02
#define CSR_ACTIVE                 0x01

/* UxUCR - USART UART Control Register */
#define UCR_FLUSH                  0x80
#define UCR_FLOW                   0x40
#define UCR_D9                     0x20
#define UCR_BIT9                   0x10
#define UCR_PARITY                 0x08
#define UCR_SPB                    0x04
#define UCR_STOP                   0x02
#define UCR_START                  0x01

#define P2DIR_PRIPO                0xC0

#define HAL_DMA_U0DBUF             0x70C1
#define HAL_DMA_U1DBUF             0x70F9

#undef  UxCSR
#undef  UxUCR
#undef  UxDBUF
#undef  UxBAUD
#undef  UxGCR
#define UxCSR                      U1CSR
#define UxUCR                      U1UCR
#define UxDBUF                     U1DBUF
#define UxBAUD                     U1BAUD
#define UxGCR                      U1GCR

#undef  PxSEL
#undef  HAL_UART_PERCFG_BIT
#undef  HAL_UART_PRIPO
#undef  HAL_UART_Px_SEL_S
#undef  HAL_UART_Px_SEL_M
#define PxSEL                      P1SEL
#define HAL_UART_PERCFG_BIT        0x02         /* USART1 on P1, Alt-2; so set this bit */
#define HAL_UART_PRIPO             0x40         /* USART1 priority over UART0 */
#define HAL_UART_Px_SEL_S          0xF0         /* Peripheral I/O Select for Slave: SO/SI/CLK/CSn */
#define HAL_UART_Px_SEL_M          0xE0         /* Peripheral I/O Select for Master: MI/MO/CLK*/

#undef  DMA_PAD
#undef  DMA_UxDBUF
#undef  DMATRIG_RX
#undef  DMATRIG_TX
#define DMA_PAD                    U1BAUD
#define DMA_UxDBUF                 HAL_DMA_U1DBUF
#define DMATRIG_RX                 HAL_DMA_TRIG_URX1
#define DMATRIG_TX                 HAL_DMA_TRIG_UTX1

#undef  PxIEN
#undef  PxIFG
#undef  PxIF
#undef  PICTL_BIT
#undef  IENx
#undef  IEN_BIT

#define PxIEN                      P1IEN
#define PxIFG                      P1IFG
#define PxIF                       P1IF
#define SPI_RDYIn                  P1_4
#define SPI_RDYIn_BIT              BV(4)
/* Falling edge ISR on P1.4-7 pins */
#define PICTL_BIT                  BV(2)
#define IENx                       IEN2
#define IEN_BIT                    BV(4)

#undef  PxDIR
/* Settings for SPI Master for SPI == 2 */
#define PxDIR                      P1DIR
#define SPI_RDYOut                 P1_4
#define SPI_RDYOut_BIT             BV(4)

/* ------------------------------------------------------------------------------------------------
 *                                           TypeDefs
 * ------------------------------------------------------------------------------------------------
 */

#if (SPI_MAX_PKT_LEN <= 256)
typedef uint8  spiLen_t;
#else
typedef uint16 spiLen_t;
#endif

/* ------------------------------------------------------------------------------------------------
 *                                           Macros
 * ------------------------------------------------------------------------------------------------
 */

#if SPI_MAX_PKT_LEN == 256
#define SPI_LEN_T_INCR(LEN)  (LEN)++
#else
#define SPI_LEN_T_INCR(LEN) st (  \
  if (++(LEN) >= SPI_MAX_PKT_LEN) \
  { \
    (LEN) = 0; \
  } \
)
#endif

#define SPI_CLR_RX_BYTE(IDX)   (spiRxBuf[(IDX)] = BUILD_UINT16(0, (DMA_PAD ^ 0xFF)))
#define SPI_GET_RX_BYTE(IDX)   (*(volatile uint8 *)(spiRxBuf+(IDX)))
#define SPI_NEW_RX_BYTE(IDX)   ((uint8)DMA_PAD == HI_UINT16(spiRxBuf[(IDX)]))

#define SPI_DAT_LEN(PBUF)      ((PBUF)[SPI_LEN_IDX])
#define SPI_PKT_LEN(PBUF)      (SPI_DAT_LEN((PBUF)) + SPI_FRM_LEN)

#define SPI_RDY_IN()           (SPI_RDYIn == 0)
#define SPI_RDY_OUT()          (SPI_RDYOut == 0)

#define SPI_CLOCK_RX(CNT) st ( \
  for (spiLen_t cnt = 0; cnt < (CNT); cnt++) \
  { \
    UxDBUF = 0; \
    while (UxCSR & CSR_ACTIVE); \
  } \
)
#define SPI_CLR_RDY_OUT()       SPI_CLR_CSn_OUT()

#define SPI_RX_RDY()            (spiRxHead != spiRxTail)

static __no_init uint8  spiTxPkt[SPI_MAX_PKT_LEN]; /* Can be trimmed as per requirement*/

extern void HalUARTInitSPI(void);
extern void HalUARTOpenSPI();
extern spiLen_t HalUARTWriteSPIDirect(spiLen_t len);
extern spiLen_t HalUARTWriteSPI(uint8 *buf, spiLen_t len);
extern void writeCompleteCallback(void);