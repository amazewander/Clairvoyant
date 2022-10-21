//#include "npi.h"
#include "OSAL.h"

enum LevelChangeResult {LEVEL_CHANGE_SUCCESS, LEVEL_CHANGE_FAIL, LEVEL_CHANGE_POWER_DOWN};

#define LEVEL_SVN_ADR                                     0x80
   
// I/O pins
#define BUS_LEVEL_PIN                                     0x02
#define BMR_CTL_DIR                                       P2DIR
#define BMR_CTL_PIN                                       0x01 //P2.0
#define BMR_CTL                                           P2_0
#define DERAIL_CTL_DIR                                    P0DIR
#define DERAIL_CTL_PIN                                    0x80 //P0.7
#define DERAIL_CTL                                        P0_7
#define FSYNC_CTL_DIR                                     P0DIR
#define FSYNC_CTL_PIN                                     0x10 //P0.4
#define FSYNC_CTL                                         P0_4

// Timer3 dma channal
#define HAL_DMA_CH_TIMER3                                 2

#define TIMER_OUTPUT_PIN_SEL           P1SEL
#define TIMER_OUTPUT_PIN_DIR           P1DIR
#define TIMER_OUTPUT_PIN_BIT           0x08
#define TIMER_OUTPUT_CHANNEL_CTL       T3CCTL0
#define TIMER_IF_BIT                   0x02       //TIMIF.T3CH0IF

// Timer1
#define SIGNAL_STOP_IF_REG             T1STAT
#define SIGNAL_STOP_IF_BIT             0x02       //T1STAT.CH1IF           

#define P2DIR_PRIP0_T1_0_1             (0x02 << 6)
#define PRIP1_T3                       0x60
#define T3CFG                          0x20
#define T3_START                       0x10
#define T3_CLR                         0x04
#define T3CCTLn_MODE                   0x04
#define T3CCTLn_IM                     0x40
#define T3CCTLn_CLEAR_ON_CMP           0x08
#define T3CCTLn_SET_ON_CMP             (~0x38)

#define LEVEL_CHANGE_CHECK_EVT                               0x0001
#define LEVEL_CHANGE_RESEND_EVT                              0x0002
#define BUS_LEVEL_CHECK_EVT                                  0x0004
#define RECONNECT_DERAIL_EVT                                 0x0008
#define SEND_SIGNAL_INIT_FINISH_EVT                          0x0010
#define BMR_RESTART_EVT                                      0x0020
#define BMR_RESTART_CHECK_EVT                                0x0040
#define SIGNAL_DEBUG_EVT                                     0x0100

#define LEVEL_CHANGE_CHECK_DELAY                             1
#define LEVEL_CHANGE_RESEND_DELAY                            55
#define BUS_LEVEL_CHECK_DELAY                                5000
#define RECONNECT_DERAIL_DELAY                               200
#define SEND_SIGNAL_INIT_DELAY                               1
#define BMR_RESTART_DELAY                                    5000
#define BMR_RESTART_CHECK_DELAY                              1500
#define COLLECT_VOLTAGE_WINDOW                               3750 // 15ms

#define LEVEL_CHANGE_RESEND_LIMIT                            1

#define LEVEL_UP                       TRUE
#define LEVEL_DOWN                     FALSE
#define MAX_LEVEL                      10

#define ADC_RESOLUTION                 HAL_ADC_RESOLUTION_8

#define SIGNAL_PRE_LEVEL_CHANGE        0x01
#define SIGNAL_LEVEL_CHANGE            0x02

#define getBusVoltage()                                   HalAdcRead(HAL_ADC_CHN_AIN1, ADC_RESOLUTION)
#define connectBMR()                                      BMR_CTL = 1
#define disconnectBMR()                                   BMR_CTL = 0
#define connectDerailleur()                               DERAIL_CTL = 1
#define disconnectDerailleur()                            DERAIL_CTL = 0
#define fsyncOn()                                         FSYNC_CTL = 0
#define fsyncOff()                                        FSYNC_CTL = 1
#define timer1Start()                                     T1CTL |= 0x01
#define timer1Stop()                                      T1CTL &= ~0x03
#define timer1Reset()                                     T1CNTL = 0
#define timer3Start()                                     T3CTL |= 0x10  
#define timer3Stop()                                      T3CTL &= ~0x10
#define timer3Reset()                                     T3CTL |= 0x04

#define REFERENCE_9832_CLOCK                              16000000L
//#define BITS_PER_DEG		11.3777777777778	// 4096 / 360

#define SPI_CONFIG                                        0
#define SPI_SLEEP                                         1
#define SPI_WAKE                                          2

#define POWER_DOWN_DETERMINATION_VOLTAGE                  40

/*********************************************************************
 * FUNCTIONS
 */

/*
 * Task Initialization for the BLE Application
 */
extern void Signal_Init( uint8 task_id );

/*
 * Task Event Processor for the BLE Application
 */
extern uint16 Signal_ProcessEvent( uint8 task_id, uint16 events );

extern void startConfigSignal(void);

extern void levelChange(bool direction);