#include <string.h>

#include "signal.h"
#include "hal_drivers.h"
#include "hal_adc.h"
#include "comdef.h"
#include "osal_snv.h"
#include "OSAL_PwrMgr.h"
#include "spi.h"

//local function
static void sendSignalInit(uint8 signal);
static void sendSignal(void);
static void levelChangeCheck(void);
static void busLevelCheck(void);
static void getTime(uint16* timer);
static void levelChangeResend( void );
static void levelChangeComplete(enum LevelChangeResult result);
static void configDDS(void);
//static void sleepDDS(void);
static void wakeDDS(void);
static void initTimer3DMA(void);
static void startDDSSignal(void);
static void stopDDSSignal(void);
static void sendSPIData(uint8* data, uint8 len);
static void restartBMR(void);
static void prepareRestartBMR(void);
static void restartBMRCheck(void);

static uint8 levelSignals[11][28] = {
  {149, 7, 11, 19, 7, 3, 3, 27, 11, 3, 3, 3, 3, 3, 3, 15, 3, 3, 7, 15, 3, 39, 7, 7, 3, 3, 3, 8},
  {149, 7, 11, 19, 7, 3, 3, 27, 11, 7, 7, 3, 3, 15, 3, 3, 7, 15, 3, 35, 3, 7, 3, 7, 3, 3, 3, 4},
  {149, 7, 11, 19, 7, 3, 3, 27, 11, 11, 3, 3, 3, 15, 3, 3, 7, 15, 3, 35, 31, 3, 4},
  {149, 7, 11, 19, 7, 3, 3, 27, 11, 39, 3, 3, 7, 15, 3, 31, 3, 7, 3, 3, 3, 7, 7, 4},
  {149, 7, 11, 19, 7, 3, 3, 27, 11, 3, 7, 27, 3, 3, 7, 15, 3, 31, 3, 3, 3, 3, 11, 3, 7, 4},
  {149, 7, 11, 19, 7, 3, 3, 27, 11, 3, 3, 31, 3, 3, 7, 15, 3, 31, 7, 3, 7, 7, 16},
  {149, 7, 11, 19, 7, 3, 3, 27, 11, 7, 3, 3, 7, 15, 3, 3, 7, 15, 3, 31, 11, 7, 3, 7, 3, 3, 4},
  {149, 7, 11, 19, 7, 3, 3, 27, 11, 11, 7, 19, 3, 3, 7, 15, 3, 27, 3, 11, 15, 3, 7, 4},
  {149, 7, 11, 19, 7, 3, 3, 27, 11, 3, 15, 19, 3, 3, 7, 15, 3, 27, 3, 7, 3, 7, 3, 7, 7, 4},
  {149, 7, 11, 19, 7, 3, 3, 27, 11, 3, 3, 3, 7, 19, 3, 3, 7, 15, 3, 27, 3, 3, 3, 3, 3, 7, 20},
  {149, 7, 11, 19, 7, 3, 3, 27, 11, 11, 11, 15, 3, 3, 7, 15, 3, 27, 3, 3, 19, 3, 3, 7, 4}
};

static uint8 signalLength[11] = {28, 28, 23, 24, 26, 23, 27, 24, 26, 27, 25};

static uint8 preLevelChangeSignal[9] = {0x67,0x4,0xb2,0x46,0x40,0x0,0x40,0x1,0x1};

uint8 signal_TaskID;   // Task ID for internal task/event processing

static uint8 currentSignal[28];

static uint8 currentLevel;
static uint8 tmpLevel;

static uint16 baseVoltage = 70;

static bool levelChangeable = TRUE;

uint8 signalResendCount;
uint8 levelChangeFailCount = 0;

uint16 levelSignalSendTime = 0;

bool ddsSending = false;

void Signal_Init( uint8 task_id ){
  signal_TaskID = task_id;
  
  // init adc set reference voltage
  P0SEL |= 0x02;
  HalAdcSetReference(HAL_ADC_REF_AVDD);
  
  /***************************************************************************
  * Setup interrupt
  *
  * Enables global interrupts (IEN0.EA = 1) and interrupts from Timer 1
  * (IEN1.T1IE = 1).
  */
  EA = 1;
  T1IE = 1;
  
  // init Timer 1 & 3
  T1CTL |= 0x0C;  // set Timer 1 prescaler divider value 128, one tick every 4us
  T1CCTL1 = 0x44; // set Timer 1 channel 1 to compare mode and enable interrupt
  T1CC1L = 31;  // set Timer 1 channel 1 compare value
  T1CC1H = 0;
  
  T3CTL = 0x82; // set Timer 3 prescaler divider value 16; Modulo mode; overflow interrupt disabled; Timer is also cleared
  //T3CC0 = 0x20; // set modulo compare value 32(interrupts will be 1MHz)
  TIMER_OUTPUT_PIN_SEL |= TIMER_OUTPUT_PIN_BIT;          // Selects P1_3 as peripheral I/O.
  TIMER_OUTPUT_PIN_DIR |= TIMER_OUTPUT_PIN_BIT;          // and output.
  //TIMER_OUTPUT_CHANNEL_CTL = T3CCTLn_IM | T3CCTLn_MODE;  // Set channel 0 to compare mode.
  TIMER_OUTPUT_CHANNEL_CTL = 0x14;
  P2SEL |= PRIP1_T3; // Set Timer 3 priority
  
  // init output pins
  BMR_CTL_DIR |= BMR_CTL_PIN;
  DERAIL_CTL_DIR |= DERAIL_CTL_PIN;
  FSYNC_CTL_DIR |= FSYNC_CTL_PIN;
  
  P1DIR |= 0x60;
  
  HalUARTInitSPI();
  
  initTimer3DMA();
}

static void initTimer3DMA(void){  
  DMAIE = 1; // enable dma interrupt
  
  /* Setup Tx by DMA */
  halDMADesc_t *ch = HAL_DMA_GET_DESC1234( HAL_DMA_CH_TIMER3 );
  
  /* Abort any pending DMA operations (in case of a soft reset) */
  HAL_DMA_ABORT_CH( HAL_DMA_CH_TIMER3 );
  
  /* The start address of the destination */
  HAL_DMA_SET_DEST( ch, 0x70CD );
  
  /* Using the length field to determine how many bytes to transfer */
  HAL_DMA_SET_VLEN( ch, HAL_DMA_VLEN_USE_LEN );
  
  /* One byte is transferred each time */
  HAL_DMA_SET_WORD_SIZE( ch, HAL_DMA_WORDSIZE_BYTE );
  
  /* The bytes are transferred 1-by-1 on Tx Complete trigger */
  HAL_DMA_SET_TRIG_MODE( ch, HAL_DMA_TMODE_SINGLE );
  HAL_DMA_SET_TRIG_SRC( ch, HAL_DMA_TRIG_T3_CH0 );
  
  /* The source address is incremented by 1 byte after each transfer */
  HAL_DMA_SET_SRC_INC( ch, HAL_DMA_SRCINC_1 );
  HAL_DMA_SET_SOURCE( ch, currentSignal );
  
  /* The destination address is constant */
  HAL_DMA_SET_DST_INC( ch, HAL_DMA_DSTINC_0 );
  
  /* The DMA Tx done is serviced by ISR */
  HAL_DMA_SET_IRQ( ch, HAL_DMA_IRQMASK_DISABLE );
  
  /* Xfer all 8 bits of a byte xfer */
  HAL_DMA_SET_M8( ch, HAL_DMA_M8_USE_8_BITS );
  
  /* DMA has highest priority for memory access */
  HAL_DMA_SET_PRIORITY( ch, HAL_DMA_PRI_HIGH );
}

void startConfigSignal(void){   
  HalUARTOpenSPI();
  
  // init 9832 registers
  configDDS();
  
  osal_snv_read(LEVEL_SVN_ADR, 1, &currentLevel);  
  if(currentLevel > MAX_LEVEL){
    currentLevel = MAX_LEVEL;
  }
  
  connectBMR();
  connectDerailleur();
  
  osal_pwrmgr_device(PWRMGR_ALWAYS_ON);
}

uint16 Signal_ProcessEvent( uint8 task_id, uint16 events ){
  VOID task_id; // OSAL required parameter that isn't used in this function
  
  if ( events & SYS_EVENT_MSG )
  {
    uint8 *pMsg;
    
    if ( (pMsg = osal_msg_receive( signal_TaskID )) != NULL )
    {      
      // Release the OSAL message
      VOID osal_msg_deallocate( pMsg );
    }
    
    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }
  
  if ( events & LEVEL_CHANGE_CHECK_EVT ){
    levelChangeCheck();
    return (events ^ LEVEL_CHANGE_CHECK_EVT);
  }
  
  if ( events & LEVEL_CHANGE_RESEND_EVT ){
    levelChangeResend();
    return (events ^ LEVEL_CHANGE_RESEND_EVT);
  }
  
  if ( events & BUS_LEVEL_CHECK_EVT ){
    busLevelCheck();
    return (events ^ BUS_LEVEL_CHECK_EVT);
  }
  
  if ( events & RECONNECT_DERAIL_EVT ){
    connectDerailleur();
    levelChangeable = TRUE;  
    return (events ^ RECONNECT_DERAIL_EVT);
  }
  
  if ( events & SEND_SIGNAL_INIT_FINISH_EVT ){
    sendSignal();
    return (events ^ SEND_SIGNAL_INIT_FINISH_EVT);
  }
  
  if ( events & BMR_RESTART_EVT ){
    restartBMR();
    return (events ^ BMR_RESTART_EVT);
  }  
  
  if ( events & BMR_RESTART_CHECK_EVT ){
    restartBMRCheck();
    return (events ^ BMR_RESTART_CHECK_EVT);
  }  
  
  if ( events & SIGNAL_DEBUG_EVT ){
    sendSignalInit(SIGNAL_LEVEL_CHANGE);
    return (events ^ SIGNAL_DEBUG_EVT);
  } 
  
  // Discard unknown events
  return 0;
}

void levelChange(bool direction){
  if ((currentLevel == 0 && direction == LEVEL_DOWN) || (currentLevel == MAX_LEVEL && direction == LEVEL_UP)) {
    return;
  }
  if (levelChangeable) {
    levelChangeable = FALSE;
    signalResendCount = 0;
    
#if defined POWER_SAVING
    osal_pwrmgr_device(PWRMGR_ALWAYS_ON);
    //(void)osal_set_event(Hal_TaskID, HAL_PWRMGR_HOLD_EVENT);
#endif
    
    tmpLevel = currentLevel + (direction == LEVEL_UP ? 1 : -1);
    sendSignalInit(SIGNAL_LEVEL_CHANGE);
  } 
#if defined MY_DEBUG
  else{
    direction = true;
  }
#endif
  return;
}

static void getTime(uint16* timer){
  /* Read the result */
  *timer = (uint16) (T1CNTL);
  *timer |= ((uint16) (T1CNTH) << 8);
}

static void levelChangeCheck(void){
  uint16 cTime;
  getTime(&cTime);
  uint16 pastTime = cTime - levelSignalSendTime;
  if (pastTime < COLLECT_VOLTAGE_WINDOW) { // collect voltage window
    uint16 value = getBusVoltage();
//        if(value>108){
//          value += 1;
//        }else if(value >100){
//          value += 1;
//        }else{
//          value += 1;
//        }
    if (value < POWER_DOWN_DETERMINATION_VOLTAGE) { // power down happened, restart
      uint8 count = 0;
      while (count < 2) {
        if (getBusVoltage() > POWER_DOWN_DETERMINATION_VOLTAGE) {
          break;
        }
        count++;
      }
      if (count >= 2) { // power down confirmed
        levelChangeComplete(LEVEL_CHANGE_POWER_DOWN);
        return;
      }
    } else if (value < baseVoltage) {
      uint8 count = 0;
      while (count < 2) {
        value = getBusVoltage();
        if (value > baseVoltage) {
          break;
        }
        count++;
      }
      if (count >= 2) { // change success
        levelChangeComplete(LEVEL_CHANGE_SUCCESS);
        return;
      }
    }
  } else { // no success, retry
    osal_start_timerEx( signal_TaskID, LEVEL_CHANGE_RESEND_EVT, LEVEL_CHANGE_RESEND_DELAY );
    return;
  }
  // still no change action, check next time
  osal_set_event( signal_TaskID, LEVEL_CHANGE_CHECK_EVT );
  return;
}

static void levelChangeComplete(enum LevelChangeResult result){
  // stop Timer1
  timer1Stop();
  if(result == LEVEL_CHANGE_FAIL && levelChangeFailCount > 10){ // too many failure, maybe already succeed
    result = LEVEL_CHANGE_SUCCESS;
  }
  
  switch(result){
  case LEVEL_CHANGE_SUCCESS : 
    // update currentLevel in snv
    currentLevel = tmpLevel;
    osal_snv_write(LEVEL_SVN_ADR, 1, &currentLevel);
    levelChangeFailCount = -1;
  case LEVEL_CHANGE_FAIL :
    levelChangeFailCount ++;
    osal_start_timerEx( signal_TaskID, BUS_LEVEL_CHECK_EVT, BUS_LEVEL_CHECK_DELAY );
    levelChangeable = TRUE;
    break;
  case LEVEL_CHANGE_POWER_DOWN:
    levelChangeFailCount = 0;
    prepareRestartBMR();
    break;
  }
  
  // enable power saving
#if defined POWER_SAVING
  osal_pwrmgr_device(PWRMGR_BATTERY);
#endif
}

static void restartBMR() {
  connectBMR();
  osal_start_timerEx( signal_TaskID, BMR_RESTART_CHECK_EVT, BMR_RESTART_CHECK_DELAY );
}

static void restartBMRCheck() {
  uint8 count = 0;
  while(++count < 3 && getBusVoltage() < POWER_DOWN_DETERMINATION_VOLTAGE){
  }
  if(count < 3){
    levelChangeable = TRUE;  
  }else{
    prepareRestartBMR();
  }
}

static void prepareRestartBMR(){
  disconnectBMR();
  osal_start_timerEx( signal_TaskID, BMR_RESTART_EVT, BMR_RESTART_DELAY );
}

static void busLevelCheck( void ){
  uint8 count = 0;
  uint16 voltage;
  do {
    voltage = getBusVoltage();
    if(voltage < POWER_DOWN_DETERMINATION_VOLTAGE){
      levelChangeable = FALSE;  
      prepareRestartBMR();
      return;
    }
  }while (++count < 10 && voltage < 101);
  if (count < 10) {
    levelChangeable = FALSE;  
    disconnectDerailleur();
    osal_start_timerEx( signal_TaskID, RECONNECT_DERAIL_EVT, RECONNECT_DERAIL_DELAY );
  }
  return;
}

static void levelChangeResend( void ){
  if(++signalResendCount > LEVEL_CHANGE_RESEND_LIMIT){
    levelChangeComplete(LEVEL_CHANGE_FAIL);
    return;
  }
  
  sendSignalInit(SIGNAL_LEVEL_CHANGE); 
}

static void sendSignalInit(uint8 signal){
  ddsSending = true;
  wakeDDS();
  switch(signal){
  case SIGNAL_PRE_LEVEL_CHANGE:
    (void)memcpy(currentSignal, preLevelChangeSignal, 10);
    break;
  case SIGNAL_LEVEL_CHANGE:
    (void)memcpy(currentSignal, levelSignals[tmpLevel], signalLength[tmpLevel]);
    break;
  }
  
  T3CC0 = 10;
  halDMADesc_t *ch = HAL_DMA_GET_DESC1234(HAL_DMA_CH_TIMER3);
  HAL_DMA_SET_LEN(ch, signalLength[tmpLevel]); /* DMA TX might need padding */
  
  /* Abort any pending DMA operations */
  HAL_DMA_ABORT_CH( HAL_DMA_CH_TIMER3 );
  HAL_DMA_ARM_CH(HAL_DMA_CH_TIMER3);
  
  HAL_DMA_MAN_TRIGGER(HAL_DMA_CH_TIMER3);
  
  osal_start_timerEx( signal_TaskID, SEND_SIGNAL_INIT_FINISH_EVT, SEND_SIGNAL_INIT_DELAY );
}

static void sendSignal(void){
  timer3Reset();
  timer3Start();
  
  // restart Timer1
  timer1Reset();
  timer1Start();
  
  // delay for sync signal and timer
  asm("NOP"); asm("NOP"); asm("NOP"); asm("NOP");
  asm("NOP"); asm("NOP"); asm("NOP"); asm("NOP");
  asm("NOP"); asm("NOP"); asm("NOP"); asm("NOP");
  asm("NOP"); asm("NOP"); asm("NOP"); asm("NOP");
  asm("NOP"); asm("NOP"); asm("NOP"); asm("NOP");
  asm("NOP"); asm("NOP");
  startDDSSignal();
}

static void configDDS(){
  uint8 data[] = {0xF8, 0,          // sleep dds
  0x30, 0xFF, 0x21, 0xFF, 0x32, 0xFF, 0x23, 0x0F, // set frequency register 0 1MHz
  0x34, 0, 0x25, 0, 0x36, 0, 0x27, 0, // set frequency register 1 0Hz
  0x18, 0, 0x09, 0x04, // set phase register 0, 90 degree
  0x1A, 0, 0x0B, 0x0C  // set phase register 1, 270 degree
  };
  sendSPIData(data, 26);
  return;
}

static void wakeDDS(){
  uint8 data[] = {0xD0, 0}; // wake up and reset
  sendSPIData(data, 2);
  return;
}

static void startDDSSignal(){
  uint8 data[] = {0xC0, 0};
  sendSPIData(data, 2);
  return;
}

static void stopDDSSignal() {
  uint8 data[] = {
    0xF0, 0           // sleep DDS
  };
  sendSPIData(data, 2);
  
  ddsSending = false;
  return;
}

static void sendSPIData(uint8* data, uint8 len) {
  fsyncOn();
  HalUARTWriteSPI(data, len);
  return;
}

/***************************************************************************************************
*                                    INTERRUPT SERVICE ROUTINE
***************************************************************************************************/

/**************************************************************************************************
* @fn      signalTimer1Isr
*
* @brief   Timer1 ISR
*
* @param
*
* @return
**************************************************************************************************/
HAL_ISR_FUNCTION( signalTimer1Isr, T1_VECTOR )
{
  HAL_ENTER_ISR();
  
  // PSK signal cut out time reached
  if((SIGNAL_STOP_IF_REG & SIGNAL_STOP_IF_BIT) && ddsSending){
    stopDDSSignal();
    timer3Stop();  
    
    getTime(&levelSignalSendTime);
    osal_start_timerEx( signal_TaskID, LEVEL_CHANGE_CHECK_EVT, LEVEL_CHANGE_CHECK_DELAY );
  }
  
  SIGNAL_STOP_IF_REG = 0; // clear timer interrupt flags
  T1IF = 0; // clear CPU Timer1 flag
  
  HAL_EXIT_ISR();
  
  return;
}

/******************************************************************************
* @fn      HalDMAInit
*
* @brief   DMA Interrupt Service Routine
*
* @param   None
*
* @return  None
*****************************************************************************/
HAL_ISR_FUNCTION( spiDmaIsr, DMA_VECTOR )
{
  HAL_ENTER_ISR();
  
  DMAIF = 0;
  // send 9832 config SPI signal over
  if (HAL_DMA_CHECK_IRQ(HAL_DMA_CH_TX))
  {
    HAL_DMA_CLEAR_IRQ(HAL_DMA_CH_TX);
    
    writeCompleteCallback();
    fsyncOff();
  }
  
  HAL_EXIT_ISR();
  
  return;
}