/******************************************************************************

 @file  simpleBLECentral.c

 @brief This file contains the Simple BLE Central sample application for use
        with the CC2540 Bluetooth Low Energy Protocol Stack.

 Group: WCS, BTS
 Target Device: CC2540, CC2541

 ******************************************************************************
 
 Copyright (c) 2010-2016, Texas Instruments Incorporated
 All rights reserved.

 IMPORTANT: Your use of this Software is limited to those specific rights
 granted under the terms of a software license agreement between the user
 who downloaded the software, his/her employer (which must be your employer)
 and Texas Instruments Incorporated (the "License"). You may not use this
 Software unless you agree to abide by the terms of the License. The License
 limits your use, and you acknowledge, that the Software may not be modified,
 copied or distributed unless embedded on a Texas Instruments microcontroller
 or used solely and exclusively in conjunction with a Texas Instruments radio
 frequency transceiver, which is integrated into your product. Other than for
 the foregoing purpose, you may not use, reproduce, copy, prepare derivative
 works of, modify, distribute, perform, display or sell this Software and/or
 its documentation for any purpose.

 YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
 PROVIDED “AS IS” WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
 NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
 TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
 NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
 LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
 INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
 OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
 OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
 (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

 Should you have any questions regarding your right to use this Software,
 contact Texas Instruments Incorporated at www.TI.com.

 ******************************************************************************
 Release Name: ble_sdk_1.4.2.2
 Release Date: 2016-06-09 06:57:10
 *****************************************************************************/

/*********************************************************************
 * INCLUDES
 */

#include "bcomdef.h"
#include "OSAL.h"
#include "OSAL_PwrMgr.h"
#include "OnBoard.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_lcd.h"
#include "gatt.h"
#include "ll.h"
#include "hci.h"
#include "gapgattserver.h"
#include "gattservapp.h"
#include "central.h"
#include "gapbondmgr.h"
#include "simpleGATTprofile.h"
#include "simpleBLECentral.h"
//#include "battservice.h"
#include "hal_adc.h"

/*********************************************************************
 * MACROS
 */

// Length of bd addr as a string
#define B_ADDR_STR_LEN                        15

/*********************************************************************
 * CONSTANTS
 */

// Maximum number of scan responses
#define DEFAULT_MAX_SCAN_RES                  8

// Scan duration in ms
#define DEFAULT_SCAN_DURATION                 4000

// Discovey mode (limited, general, all)
#define DEFAULT_DISCOVERY_MODE                DEVDISC_MODE_ALL

// TRUE to use active scan
#define DEFAULT_DISCOVERY_ACTIVE_SCAN         TRUE

// TRUE to use white list during discovery
#define DEFAULT_DISCOVERY_WHITE_LIST          FALSE

// TRUE to use high scan duty cycle when creating link
#define DEFAULT_LINK_HIGH_DUTY_CYCLE          FALSE

// TRUE to use white list when creating link
#define DEFAULT_LINK_WHITE_LIST               FALSE

// Default RSSI polling period in ms
#define DEFAULT_RSSI_PERIOD                   1000

// Whether to enable automatic parameter update request when a connection is formed
#define DEFAULT_ENABLE_UPDATE_REQUEST         FALSE

// Minimum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define DEFAULT_UPDATE_MIN_CONN_INTERVAL      150

// Maximum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define DEFAULT_UPDATE_MAX_CONN_INTERVAL      200

// Slave latency to use if automatic parameter update request is enabled
#define DEFAULT_UPDATE_SLAVE_LATENCY          0

// Supervision timeout value (units of 10ms) if automatic parameter update request is enabled
#define DEFAULT_UPDATE_CONN_TIMEOUT           250

// Default passcode
#define DEFAULT_PASSCODE                      19655

// Default GAP pairing mode
#define DEFAULT_PAIRING_MODE                  GAPBOND_PAIRING_MODE_INITIATE

// Default MITM mode (TRUE to require passcode or OOB when pairing)
#define DEFAULT_MITM_MODE                     FALSE

// Default bonding mode, TRUE to bond
#define DEFAULT_BONDING_MODE                  TRUE

// Default GAP bonding I/O capabilities
#define DEFAULT_IO_CAPABILITIES               GAPBOND_IO_CAP_DISPLAY_ONLY

// Default service discovery timer delay in ms
#define DEFAULT_SVC_DISCOVERY_DELAY           10

// TRUE to filter discovery results on desired service UUID
#define DEFAULT_DEV_DISC_BY_SVC_UUID          TRUE

// led blink timer short delay in ms
#define BATTERY_LED_BLINK_SHORT_ON_DELAY      100

// led blink timer long delay in ms
#define BATTERY_LED_BLINK_LONG_ON_DELAY       400

#define BATTERY_LED_BLINK_OFF_DELAY           200

// led blink timer break delay in ms
#define BATTERY_LED_BLINK_BREAK_DELAY         1000

// ADC voltage levels
#define BATT_ADC_LEVEL_3V           409
#define BATT_ADC_LEVEL_2V           273

// Application states
enum
{
  BLE_STATE_IDLE,
  BLE_STATE_CONNECTING,
  BLE_STATE_CONNECTED,
  BLE_STATE_DISCONNECTING
};

// Discovery states
enum
{
  BLE_DISC_STATE_IDLE,                // Idle
  BLE_DISC_STATE_SVC,                 // Service discovery
  BLE_DISC_STATE_CHAR                 // Characteristic discovery
};

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

// Task ID for internal task/event processing
static uint8 simpleBLETaskId;

// GAP GATT Attributes
static const uint8 simpleBLEDeviceName[GAP_DEVICE_NAME_LEN] = "Shifter";

// Number of scan results and scan result index
static uint8 simpleBLEScanRes;
static uint8 simpleBLEScanIdx;

// Scan result list
static gapDevRec_t simpleBLEDevList[DEFAULT_MAX_SCAN_RES];

// Scanning state
static uint8 simpleBLEScanning = FALSE;

// RSSI polling state
static uint8 simpleBLERssi = FALSE;

// Connection handle of current connection 
static uint16 simpleBLEConnHandle = GAP_CONNHANDLE_INIT;

// Application state
static uint8 simpleBLEState = BLE_STATE_IDLE;

// Discovery state
static uint8 simpleBLEDiscState = BLE_DISC_STATE_IDLE;

// Discovered service start and end handle
static uint16 simpleBLESvcStartHdl = 0;
static uint16 simpleBLESvcEndHdl = 0;

// Discovered characteristic handle
static uint16 simpleBLECharHdl = 0x00;

// Value to write
static uint8 simpleBLECharVal = 0;
static uint8 valueToBeSent = 0;
static uint8 rearShiftUpCharVal = 17; // 0x11
static uint8 rearShiftDownCharVal = 18; // 0x12
//static uint8 frontShiftUpCharVal = 33; // 0x21
//static uint8 frontShiftDownCharVal = 34; // 0x22

// GATT read/write procedure state
static bool simpleBLEProcedureInProgress = FALSE;

// Peripheral MAC Addr
//static uint8 peripheralAddr[ ] = { 48, 27, 5, 56, 193, 164 }; //JDY-10
//static uint8 peripheralAddr[ ] = { 0x44, 0xED, 0xB0, 0x39, 0xCD, 0x20 }; //ceramic antenna
//static uint8 peripheralAddr[ ] = { 0xE3, 0xF0, 0x03, 0x83, 0x8B, 0x8C };
static uint8 peripheralAddr[ ] = { 0xFE, 0xA2, 0x56, 0xB1, 0x8C, 0x50 };

// battery related varibles
static uint8 batteryLevel = 0;
static uint8 currentBlinkRound = 0;
static uint8 currentBlinkBit = 0;
static bool currentBlinkState = FALSE;
static uint8 batteryLevelDebugCount = 0;

static uint8 connectRetryCount = 0;

static uint8 debugValue = 0;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void simpleBLECentralProcessGATTMsg( gattMsgEvent_t *pMsg );
static void simpleBLECentralRssiCB( uint16 connHandle, int8  rssi );
static uint8 simpleBLECentralEventCB( gapCentralRoleEvent_t *pEvent );
static void simpleBLECentralPasscodeCB( uint8 *deviceAddr, uint16 connectionHandle,
                                        uint8 uiInputs, uint8 uiOutputs );
static void simpleBLECentralPairStateCB( uint16 connHandle, uint8 state, uint8 status );
static void simpleBLECentral_HandleKeys( uint8 shift, uint8 keys );
static void simpleBLECentral_ProcessOSALMsg( osal_event_hdr_t *pMsg );
static void simpleBLEGATTDiscoveryEvent( gattMsgEvent_t *pMsg );
static void simpleBLECentralStartDiscovery( void );
static bool simpleBLEFindSvcUuid( uint16 uuid, uint8 *pData, uint8 dataLen );
static void simpleBLEAddDeviceInfo( uint8 *pAddr, uint8 addrType );
char *bdAddr2Str ( uint8 *pAddr );
static void performWriteToPeer(uint8 writeVal);
static void batteryLevelIndicate( void );
static void simpleBLECentralLEDBlink( void );
static void tryEstablishLink( void );

/*********************************************************************
 * PROFILE CALLBACKS
 */

// GAP Role Callbacks
static const gapCentralRoleCB_t simpleBLERoleCB =
{
  simpleBLECentralRssiCB,       // RSSI callback
  simpleBLECentralEventCB       // Event callback
};

// Bond Manager Callbacks
static const gapBondCBs_t simpleBLEBondCB =
{
  simpleBLECentralPasscodeCB,
  simpleBLECentralPairStateCB
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      SimpleBLECentral_Init
 *
 * @brief   Initialization function for the Simple BLE Central App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notification).
 *
 * @param   task_id - the ID assigned by OSAL.  This ID should be
 *                    used to send messages and set timers.
 *
 * @return  none
 */
void SimpleBLECentral_Init( uint8 task_id )
{
  simpleBLETaskId = task_id;
  
  HCI_EXT_SetTxPowerCmd(HCI_EXT_TX_POWER_4_DBM);
  HCI_EXT_SetRxGainCmd(HCI_EXT_RX_GAIN_HIGH);

  // Setup Central Profile
  {
    uint8 scanRes = DEFAULT_MAX_SCAN_RES;
    GAPCentralRole_SetParameter ( GAPCENTRALROLE_MAX_SCAN_RES, sizeof( uint8 ), &scanRes );
  }
  
  // Setup GAP
  GAP_SetParamValue( TGAP_GEN_DISC_SCAN, DEFAULT_SCAN_DURATION );
  GAP_SetParamValue( TGAP_LIM_DISC_SCAN, DEFAULT_SCAN_DURATION );
  GAP_SetParamValue( TGAP_CONN_EST_INT_MIN, DEFAULT_UPDATE_MIN_CONN_INTERVAL);
  GAP_SetParamValue( TGAP_CONN_EST_INT_MAX, DEFAULT_UPDATE_MAX_CONN_INTERVAL);
  GAP_SetParamValue( TGAP_CONN_EST_LATENCY, DEFAULT_UPDATE_SLAVE_LATENCY);
  GAP_SetParamValue( TGAP_CONN_EST_SUPERV_TIMEOUT, DEFAULT_UPDATE_CONN_TIMEOUT); 
  //GAP_SetParamValue( TGAP_REJECT_CONN_PARAMS, TRUE);
  
  //GAP_SetParamValue( TGAP_CONN_PAUSE_PERIPHERAL, 0 );
  //GAP_SetParamValue( TGAP_CONN_PAUSE_CENTRAL, 0 );
  GGS_SetParameter( GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, (uint8 *) simpleBLEDeviceName );

  // Setup the GAP Bond Manager
  {
    uint32 passkey = DEFAULT_PASSCODE;
    uint8 pairMode = DEFAULT_PAIRING_MODE;
    uint8 mitm = DEFAULT_MITM_MODE;
    uint8 ioCap = DEFAULT_IO_CAPABILITIES;
    uint8 bonding = DEFAULT_BONDING_MODE;
    GAPBondMgr_SetParameter( GAPBOND_DEFAULT_PASSCODE, sizeof( uint32 ), &passkey );
    GAPBondMgr_SetParameter( GAPBOND_PAIRING_MODE, sizeof( uint8 ), &pairMode );
    GAPBondMgr_SetParameter( GAPBOND_MITM_PROTECTION, sizeof( uint8 ), &mitm );
    GAPBondMgr_SetParameter( GAPBOND_IO_CAPABILITIES, sizeof( uint8 ), &ioCap );
    GAPBondMgr_SetParameter( GAPBOND_BONDING_ENABLED, sizeof( uint8 ), &bonding );
  }  

  // Initialize GATT Client
  VOID GATT_InitClient();

  // Register to receive incoming ATT Indications/Notifications
  GATT_RegisterForInd( simpleBLETaskId );

  // Initialize GATT attributes
  GGS_AddService( GATT_ALL_SERVICES );         // GAP
  GATTServApp_AddService( GATT_ALL_SERVICES ); // GATT attributes

  // Register for all key events - This app will handle all key events
  RegisterForKeys( simpleBLETaskId );
  
  // makes sure LEDs are off
  HalLedSet( HAL_LED_ALL, HAL_LED_MODE_OFF );
  
  // Setup a delayed profile startup
  osal_set_event( simpleBLETaskId, START_DEVICE_EVT );
}

/*********************************************************************
 * @fn      SimpleBLECentral_ProcessEvent
 *
 * @brief   Simple BLE Central Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  events not processed
 */
uint16 SimpleBLECentral_ProcessEvent( uint8 task_id, uint16 events )
{
  
  VOID task_id; // OSAL required parameter that isn't used in this function
  
  if ( events & SYS_EVENT_MSG )
  {
    uint8 *pMsg;

    if ( (pMsg = osal_msg_receive( simpleBLETaskId )) != NULL )
    {
      simpleBLECentral_ProcessOSALMsg( (osal_event_hdr_t *)pMsg );

      // Release the OSAL message
      VOID osal_msg_deallocate( pMsg );
    }

    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }

  if ( events & START_DEVICE_EVT )
  {
    // Start the Device
    VOID GAPCentralRole_StartDevice( (gapCentralRoleCB_t *) &simpleBLERoleCB );

    // Register with bond manager after starting device
    GAPBondMgr_Register( (gapBondCBs_t *) &simpleBLEBondCB );

    return ( events ^ START_DEVICE_EVT );
  }

  if ( events & START_DISCOVERY_EVT )
  {
    simpleBLECentralStartDiscovery( );
    
    return ( events ^ START_DISCOVERY_EVT );
  }
  
  if ( events & LED_BLINK_EVT )
  {
    simpleBLECentralLEDBlink( );
    
    return ( events ^ LED_BLINK_EVT );
  }
  
  if ( events & EST_CONN_TIMEOUT_EVT)
  {
    if(connectRetryCount++ < 5){ // if retry count not reach limit, retry connection.
      tryEstablishLink();
    } else {
      GAP_TerminateLinkReq( simpleBLETaskId, 0xFFFF, 0 ); // cancel the pending connection request
      simpleBLEState = BLE_STATE_IDLE;
    }
    return ( events ^ EST_CONN_TIMEOUT_EVT);
  }
  
  // Discard unknown events
  return 0;
}

/*********************************************************************
 * @fn      simpleBLECentral_ProcessOSALMsg
 *
 * @brief   Process an incoming task message.
 *
 * @param   pMsg - message to process
 *
 * @return  none
 */
static void simpleBLECentral_ProcessOSALMsg( osal_event_hdr_t *pMsg )
{
  switch ( pMsg->event )
  {
    case KEY_CHANGE:
      simpleBLECentral_HandleKeys( ((keyChange_t *)pMsg)->state, ((keyChange_t *)pMsg)->keys );
      break;

    case GATT_MSG_EVENT:
      simpleBLECentralProcessGATTMsg( (gattMsgEvent_t *) pMsg );
      break;
  }
}

/*********************************************************************
 * @fn      simpleBLECentral_HandleKeys
 *
 * @brief   Handles all key events for this device.
 *
 * @param   shift - true if in shift/alt.
 * @param   keys - bit field for key events. Valid entries:
 *                 HAL_KEY_SW_2
 *                 HAL_KEY_SW_1
 *
 * @return  none
 */
uint8 gStatus;
static void simpleBLECentral_HandleKeys( uint8 shift, uint8 keys )
{
  (void)shift;  // Intentionally unreferenced parameter

  if ( keys & HAL_KEY_SW_2 )
  {
    performWriteToPeer(rearShiftDownCharVal);
  }
  if ( keys & HAL_KEY_SW_1 )
  {
    //HAL_TOGGLE_LED1();
    performWriteToPeer(rearShiftUpCharVal);
  }

}

static void performWriteToPeer(uint8 writeVal){
  uint8 status;
  valueToBeSent = writeVal;
  if ( simpleBLEState == BLE_STATE_CONNECTED &&
          simpleBLECharHdl != 0 &&
          simpleBLEProcedureInProgress == FALSE &&
          valueToBeSent > 0 ) // normal state valid to send data
  {
    
    simpleBLEProcedureInProgress = TRUE;
    // Do a write
    attWriteReq_t req;
      
    req.pValue = GATT_bm_alloc( simpleBLEConnHandle, ATT_WRITE_REQ, 1, NULL );
    if ( req.pValue != NULL )
    {
      req.handle = simpleBLECharHdl;
      req.len = 1;
      req.pValue[0] = valueToBeSent;
      req.sig = 0;
      req.cmd = 0;
      //req.sig = FALSE;
      //req.cmd = TRUE;
      status = GATT_WriteCharValue( simpleBLEConnHandle, &req, simpleBLETaskId );
      //status = GATT_WriteNoRsp( simpleBLEConnHandle, &req );
      debugValue = status;
      if ( status != SUCCESS )
      {
        GATT_bm_free( (gattMsg_t *)&req, ATT_WRITE_REQ );
        if(status == bleNotConnected){
          simpleBLEState = BLE_STATE_IDLE;
        }
        simpleBLEProcedureInProgress = FALSE;
      } else {
        valueToBeSent = 0;
      }
    }
    else
    {
      status = bleMemAllocError;
    }
    //GATT_bm_free( (gattMsg_t *)&req, ATT_WRITE_REQ );
  }
  
  if(simpleBLEState == BLE_STATE_IDLE) // if BLE not connected, connect
  {
    connectRetryCount = 0;
    tryEstablishLink();
  }
  if(simpleBLEState == BLE_STATE_CONNECTED && simpleBLECharHdl == 0) // if Characteristics not discovered, go discover
  {
    osal_set_event( simpleBLETaskId, START_DISCOVERY_EVT );
  }
  batteryLevelIndicate();
}

static void tryEstablishLink(){
  GAP_TerminateLinkReq( simpleBLETaskId, 0xFFFF, 0 ); // cancel the pending connection request
  simpleBLEState = BLE_STATE_CONNECTING;
  GAPCentralRole_EstablishLink( DEFAULT_LINK_HIGH_DUTY_CYCLE,
                               DEFAULT_LINK_WHITE_LIST,
                               ADDRTYPE_PUBLIC, peripheralAddr );
  osal_start_timerEx( simpleBLETaskId, EST_CONN_TIMEOUT_EVT, 350);
}

static void batteryLevelIndicate( void )
{
  HalAdcSetReference( HAL_ADC_REF_125V );
  uint16 batteryAdcValue = HalAdcRead( HAL_ADC_CHANNEL_VDD, HAL_ADC_RESOLUTION_10 );
  
  if (batteryAdcValue >= BATT_ADC_LEVEL_3V)
  {
    batteryLevel = 7;
  }
  else if (batteryAdcValue <= BATT_ADC_LEVEL_2V)
  {
    batteryLevel = 0;
  }
  else
  {
      uint16 range =  BATT_ADC_LEVEL_3V - BATT_ADC_LEVEL_2V + 1;
      batteryLevel = (uint8) (((batteryAdcValue - BATT_ADC_LEVEL_2V) * 8) / range);
  }
  
  osal_stop_timerEx(simpleBLETaskId, LED_BLINK_EVT);
  HalLedSet( HAL_LED_1, HAL_LED_MODE_OFF ); // light out
  currentBlinkRound = 0;
  currentBlinkBit = 0;
  currentBlinkState = TRUE;
  osal_start_timerEx( simpleBLETaskId, LED_BLINK_EVT, BATTERY_LED_BLINK_SHORT_ON_DELAY );
}

static void simpleBLECentralLEDBlink( void )
{
  batteryLevelDebugCount++;
  osal_stop_timerEx(simpleBLETaskId, LED_BLINK_EVT);
  uint32 delayTime = 0;
  bool isOver = false;
  if( currentBlinkState )
  {
    if( batteryLevel & ( 1 << (2 - currentBlinkBit) ) )
    {
      delayTime = BATTERY_LED_BLINK_LONG_ON_DELAY;
    }
    else
    {
      delayTime = BATTERY_LED_BLINK_SHORT_ON_DELAY;
    }
  }
  else
  {
    if( currentBlinkBit == 2 )
    {
      if ( currentBlinkRound == 2 )
      {
        isOver = true;
      }
      else
      {
        delayTime = BATTERY_LED_BLINK_BREAK_DELAY;
        currentBlinkBit = 0;
        currentBlinkRound++;
      }
    }
    else
    {
      delayTime = BATTERY_LED_BLINK_OFF_DELAY;
      currentBlinkBit++;
    }
  }
  
  HalLedSet( HAL_LED_1, currentBlinkState ? HAL_LED_MODE_ON : HAL_LED_MODE_OFF );
  currentBlinkState = !currentBlinkState;
  if(!isOver)
  {
    osal_start_timerEx( simpleBLETaskId, LED_BLINK_EVT, delayTime );
  }
}

/*********************************************************************
 * @fn      simpleBLECentralProcessGATTMsg
 *
 * @brief   Process GATT messages
 *
 * @return  none
 */
static void simpleBLECentralProcessGATTMsg( gattMsgEvent_t *pMsg )
{
  if ( simpleBLEState != BLE_STATE_CONNECTED )
  {
    // In case a GATT message came after a connection has dropped,
    // ignore the message
    return;
  }
  
  if ( ( pMsg->method == ATT_READ_RSP ) ||
       ( ( pMsg->method == ATT_ERROR_RSP ) &&
         ( pMsg->msg.errorRsp.reqOpcode == ATT_READ_REQ ) ) )
  {
    if ( pMsg->method == ATT_ERROR_RSP )
    {
      uint8 status = pMsg->msg.errorRsp.errCode;
      
      LCD_WRITE_STRING_VALUE( "Read Error", status, 10, HAL_LCD_LINE_1 );
    }
    else
    {
      // After a successful read, display the read value
      uint8 valueRead = pMsg->msg.readRsp.pValue[0];

      LCD_WRITE_STRING_VALUE( "Read rsp:", valueRead, 10, HAL_LCD_LINE_1 );
    }
    
    simpleBLEProcedureInProgress = FALSE;
  }
  else if ( ( pMsg->method == ATT_WRITE_RSP ) ||
       ( ( pMsg->method == ATT_ERROR_RSP ) &&
         ( pMsg->msg.errorRsp.reqOpcode == ATT_WRITE_REQ ) ) )
  {
    
    if ( pMsg->method == ATT_ERROR_RSP == ATT_ERROR_RSP )
    {
      debugValue = pMsg->msg.errorRsp.errCode;
      
      LCD_WRITE_STRING_VALUE( "Write Error", debugValue, 10, HAL_LCD_LINE_1 );
    }
    else
    {
      // After a succesful write, display the value that was written and increment value
      LCD_WRITE_STRING_VALUE( "Write sent:", simpleBLECharVal++, 10, HAL_LCD_LINE_1 );      
    }
    
    simpleBLEProcedureInProgress = FALSE;    

  }
  else if ( simpleBLEDiscState != BLE_DISC_STATE_IDLE )
  {
    simpleBLEGATTDiscoveryEvent( pMsg );
  }
  
  GATT_bm_free( &pMsg->msg, pMsg->method );
}

/*********************************************************************
 * @fn      simpleBLECentralRssiCB
 *
 * @brief   RSSI callback.
 *
 * @param   connHandle - connection handle
 * @param   rssi - RSSI
 *
 * @return  none
 */
static void simpleBLECentralRssiCB( uint16 connHandle, int8 rssi )
{
    LCD_WRITE_STRING_VALUE( "RSSI -dB:", (uint8) (-rssi), 10, HAL_LCD_LINE_1 );
}

/*********************************************************************
 * @fn      simpleBLECentralEventCB
 *
 * @brief   Central event callback function.
 *
 * @param   pEvent - pointer to event structure
 *
 * @return  TRUE if safe to deallocate event message, FALSE otherwise.
 */
static uint8 simpleBLECentralEventCB( gapCentralRoleEvent_t *pEvent )
{
  switch ( pEvent->gap.opcode )
  {
    case GAP_DEVICE_INIT_DONE_EVENT:  
      {
        LCD_WRITE_STRING( "BLE Central", HAL_LCD_LINE_1 );
        LCD_WRITE_STRING( bdAddr2Str( pEvent->initDone.devAddr ),  HAL_LCD_LINE_2 );
        
        //tryEstablishLink();
      }
      break;

    case GAP_DEVICE_INFO_EVENT:
      {
        // if filtering device discovery results based on service UUID
        if ( DEFAULT_DEV_DISC_BY_SVC_UUID == TRUE )
        {
          if ( simpleBLEFindSvcUuid( SIMPLEPROFILE_SERV_UUID,
                                     pEvent->deviceInfo.pEvtData,
                                     pEvent->deviceInfo.dataLen ) )
          {
            simpleBLEAddDeviceInfo( pEvent->deviceInfo.addr, pEvent->deviceInfo.addrType );
          }
        }
      }
      break;
      
    case GAP_DEVICE_DISCOVERY_EVENT:
      {
        // discovery complete
        simpleBLEScanning = FALSE;

        // if not filtering device discovery results based on service UUID
        if ( DEFAULT_DEV_DISC_BY_SVC_UUID == FALSE )
        {
          // Copy results
          simpleBLEScanRes = pEvent->discCmpl.numDevs;
          osal_memcpy( simpleBLEDevList, pEvent->discCmpl.pDevList,
                       (sizeof( gapDevRec_t ) * pEvent->discCmpl.numDevs) );
        }
        
        LCD_WRITE_STRING_VALUE( "Devices Found", simpleBLEScanRes,
                                10, HAL_LCD_LINE_1 );
        if ( simpleBLEScanRes > 0 )
        {
          LCD_WRITE_STRING( "<- To Select", HAL_LCD_LINE_2 );
        }

        // initialize scan index to last device
        simpleBLEScanIdx = simpleBLEScanRes;

      }
      break;

    case GAP_LINK_ESTABLISHED_EVENT:
      {
        osal_stop_timerEx( simpleBLETaskId, EST_CONN_TIMEOUT_EVT);
        if ( pEvent->gap.hdr.status == SUCCESS )
        {          
          simpleBLEState = BLE_STATE_CONNECTED;
          simpleBLEConnHandle = pEvent->linkCmpl.connectionHandle;
          //GAPBondMgr_LinkEst(pEvent->linkCmpl.devAddrType , pEvent->linkCmpl.devAddr, simpleBLEConnHandle, GAP_PROFILE_CENTRAL );

          // If service discovery not performed initiate service discovery
          if ( simpleBLECharHdl == 0 )
          {
            simpleBLEProcedureInProgress = TRUE;  
            osal_start_timerEx( simpleBLETaskId, START_DISCOVERY_EVT, DEFAULT_SVC_DISCOVERY_DELAY );
          }
          else
          {
            if(valueToBeSent > 0){
              performWriteToPeer(valueToBeSent);
            }
          }
                    
          LCD_WRITE_STRING( "Connected", HAL_LCD_LINE_1 );
          LCD_WRITE_STRING( bdAddr2Str( pEvent->linkCmpl.devAddr ), HAL_LCD_LINE_2 );   
        }
        else
        {
          simpleBLEState = BLE_STATE_IDLE;
          simpleBLEConnHandle = GAP_CONNHANDLE_INIT;
          simpleBLERssi = FALSE;
          simpleBLEDiscState = BLE_DISC_STATE_IDLE;
          
          LCD_WRITE_STRING( "Connect Failed", HAL_LCD_LINE_1 );
          LCD_WRITE_STRING_VALUE( "Reason:", pEvent->gap.hdr.status, 10, HAL_LCD_LINE_2 );
        }
      }
      break;

    case GAP_LINK_TERMINATED_EVENT:
      {
        simpleBLEState = BLE_STATE_IDLE;
        simpleBLEConnHandle = GAP_CONNHANDLE_INIT;
        simpleBLERssi = FALSE;
        simpleBLEDiscState = BLE_DISC_STATE_IDLE;
        //simpleBLECharHdl = 0;
        simpleBLEProcedureInProgress = FALSE;
        debugValue = pEvent->linkTerminate.reason;
//        switch(debugValue){
//        case 1:
//        case 2:LCD_WRITE_STRING_VALUE( "Reason:", pEvent->linkTerminate.reason,
//                                10, HAL_LCD_LINE_2 );break;
//        }
        LCD_WRITE_STRING( "Disconnected", HAL_LCD_LINE_1 );
        
      }
      break;

    case GAP_LINK_PARAM_UPDATE_EVENT:
      {
        LCD_WRITE_STRING( "Param Update", HAL_LCD_LINE_1 );
      }
      break;
      
    default:
      break;
  }
  
  return ( TRUE );
}

/*********************************************************************
 * @fn      pairStateCB
 *
 * @brief   Pairing state callback.
 *
 * @return  none
 */
static void simpleBLECentralPairStateCB( uint16 connHandle, uint8 state, uint8 status )
{
  if ( state == GAPBOND_PAIRING_STATE_STARTED )
  {
    LCD_WRITE_STRING( "Pairing started", HAL_LCD_LINE_1 );
  }
  else if ( state == GAPBOND_PAIRING_STATE_COMPLETE )
  {
    if ( status == SUCCESS )
    {
      LCD_WRITE_STRING( "Pairing success", HAL_LCD_LINE_1 );
    }
//    else
//    {
//      
//      switch(status){
//      case 1 :
//      case 2 :
//      case 3 :
//      case 4 :
//      case 5 :
//      case 6 :
//      case 7 :
//      case 8 :LCD_WRITE_STRING_VALUE( "Pairing fail", status, 10, HAL_LCD_LINE_1 );break;
//      case bleTimeout  :LCD_WRITE_STRING_VALUE( "Pairing fail", status, 10, HAL_LCD_LINE_1 );break;
//      case bleGAPBondRejected   :LCD_WRITE_STRING_VALUE( "Pairing fail", status, 10, HAL_LCD_LINE_1 );break;
//        
//      }
//    }
  }
  else if ( state == GAPBOND_PAIRING_STATE_BONDED )
  {
    if ( status == SUCCESS )
    {
      LCD_WRITE_STRING( "Bonding success", HAL_LCD_LINE_1 );
    }
  }
}

/*********************************************************************
 * @fn      simpleBLECentralPasscodeCB
 *
 * @brief   Passcode callback.
 *
 * @return  none
 */
static void simpleBLECentralPasscodeCB( uint8 *deviceAddr, uint16 connectionHandle,
                                        uint8 uiInputs, uint8 uiOutputs )
{
#if (HAL_LCD == TRUE)

  uint32  passcode;
  uint8   str[7];

  // Create random passcode
  LL_Rand( ((uint8 *) &passcode), sizeof( uint32 ));
  passcode %= 1000000;
  
  // Display passcode to user
  if ( uiOutputs != 0 )
  {
    LCD_WRITE_STRING( "Passcode:",  HAL_LCD_LINE_1 );
    LCD_WRITE_STRING( (char *) _ltoa(passcode, str, 10),  HAL_LCD_LINE_2 );
  }
  
  // Send passcode response
  GAPBondMgr_PasscodeRsp( connectionHandle, SUCCESS, passcode );
#endif
}

/*********************************************************************
 * @fn      simpleBLECentralStartDiscovery
 *
 * @brief   Start service discovery.
 *
 * @return  none
 */
static void simpleBLECentralStartDiscovery( void )
{
  if(simpleBLEDiscState != BLE_DISC_STATE_IDLE){ 
    return;
  }
  uint8 uuid[ATT_BT_UUID_SIZE] = { LO_UINT16(SIMPLEPROFILE_SERV_UUID),
                                   HI_UINT16(SIMPLEPROFILE_SERV_UUID) };
  
  // Initialize cached handles
  simpleBLESvcStartHdl = simpleBLESvcEndHdl = simpleBLECharHdl = 0;

  simpleBLEDiscState = BLE_DISC_STATE_SVC;
  
  // Discovery simple BLE service
  GATT_DiscPrimaryServiceByUUID( simpleBLEConnHandle,
                                 uuid,
                                 ATT_BT_UUID_SIZE,
                                 simpleBLETaskId );
}

/*********************************************************************
 * @fn      simpleBLEGATTDiscoveryEvent
 *
 * @brief   Process GATT discovery event
 *
 * @return  none
 */
static void simpleBLEGATTDiscoveryEvent( gattMsgEvent_t *pMsg )
{
  attReadByTypeReq_t req;
  
  if ( simpleBLEDiscState == BLE_DISC_STATE_SVC )
  {
    // Service found, store handles
    if ( pMsg->method == ATT_FIND_BY_TYPE_VALUE_RSP &&
         pMsg->msg.findByTypeValueRsp.numInfo > 0 )
    {
      simpleBLESvcStartHdl = ATT_ATTR_HANDLE(pMsg->msg.findByTypeValueRsp.pHandlesInfo, 0);
      simpleBLESvcEndHdl = ATT_GRP_END_HANDLE(pMsg->msg.findByTypeValueRsp.pHandlesInfo, 0);
    }
    
    // If procedure complete
    if ( ( pMsg->method == ATT_FIND_BY_TYPE_VALUE_RSP  && 
           //pMsg->hdr.status == bleProcedureComplete ) ||
           (pMsg->hdr.status == bleProcedureComplete || pMsg->hdr.status == bleTimeout )) ||
         ( pMsg->method == ATT_ERROR_RSP && pMsg->hdr.status == SUCCESS ) )
    {
      if(pMsg->method == ATT_ERROR_RSP){
        simpleBLEDiscState = BLE_DISC_STATE_IDLE;
      }
      if ( simpleBLESvcStartHdl != 0 )
      {
        // Discover characteristic
        simpleBLEDiscState = BLE_DISC_STATE_CHAR;
        
        req.startHandle = simpleBLESvcStartHdl;
        req.endHandle = simpleBLESvcEndHdl;
        req.type.len = ATT_BT_UUID_SIZE;
        req.type.uuid[0] = LO_UINT16(SIMPLEPROFILE_CHAR1_UUID);
        req.type.uuid[1] = HI_UINT16(SIMPLEPROFILE_CHAR1_UUID);

        GATT_ReadUsingCharUUID( simpleBLEConnHandle, &req, simpleBLETaskId );
      }
    }
  }
  else if ( simpleBLEDiscState == BLE_DISC_STATE_CHAR )
  {
    // Characteristic found, store handle
    if ( pMsg->method == ATT_READ_BY_TYPE_RSP && 
         pMsg->msg.readByTypeRsp.numPairs > 0 )
    {
      simpleBLECharHdl = BUILD_UINT16(pMsg->msg.readByTypeRsp.pDataList[0],
                                      pMsg->msg.readByTypeRsp.pDataList[1]);
      
      LCD_WRITE_STRING( "Simple Svc Found", HAL_LCD_LINE_1 );
      simpleBLEProcedureInProgress = FALSE;
      if(valueToBeSent > 0){
        performWriteToPeer(valueToBeSent);
      }
    }
    
    simpleBLEDiscState = BLE_DISC_STATE_IDLE;

    
  }    
}


/*********************************************************************
 * @fn      simpleBLEFindSvcUuid
 *
 * @brief   Find a given UUID in an advertiser's service UUID list.
 *
 * @return  TRUE if service UUID found
 */
static bool simpleBLEFindSvcUuid( uint16 uuid, uint8 *pData, uint8 dataLen )
{
  uint8 adLen;
  uint8 adType;
  uint8 *pEnd;
  
  pEnd = pData + dataLen - 1;
  
  // While end of data not reached
  while ( pData < pEnd )
  {
    // Get length of next AD item
    adLen = *pData++;
    if ( adLen > 0 )
    {
      adType = *pData;
      
      // If AD type is for 16-bit service UUID
      if ( adType == GAP_ADTYPE_16BIT_MORE || adType == GAP_ADTYPE_16BIT_COMPLETE )
      {
        pData++;
        adLen--;
        
        // For each UUID in list
        while ( adLen >= 2 && pData < pEnd )
        {
          // Check for match
          if ( pData[0] == LO_UINT16(uuid) && pData[1] == HI_UINT16(uuid) )
          {
            // Match found
            return TRUE;
          }
          
          // Go to next
          pData += 2;
          adLen -= 2;
        }
        
        // Handle possible erroneous extra byte in UUID list
        if ( adLen == 1 )
        {
          pData++;
        }
      }
      else
      {
        // Go to next item
        pData += adLen;
      }
    }
  }
  
  // Match not found
  return FALSE;
}

/*********************************************************************
 * @fn      simpleBLEAddDeviceInfo
 *
 * @brief   Add a device to the device discovery result list
 *
 * @return  none
 */
static void simpleBLEAddDeviceInfo( uint8 *pAddr, uint8 addrType )
{
  uint8 i;
  
  // If result count not at max
  if ( simpleBLEScanRes < DEFAULT_MAX_SCAN_RES )
  {
    // Check if device is already in scan results
    for ( i = 0; i < simpleBLEScanRes; i++ )
    {
      if ( osal_memcmp( pAddr, simpleBLEDevList[i].addr , B_ADDR_LEN ) )
      {
        return;
      }
    }
    
    // Add addr to scan result list
    osal_memcpy( simpleBLEDevList[simpleBLEScanRes].addr, pAddr, B_ADDR_LEN );
    simpleBLEDevList[simpleBLEScanRes].addrType = addrType;
    
    // Increment scan result count
    simpleBLEScanRes++;
  }
}

/*********************************************************************
 * @fn      bdAddr2Str
 *
 * @brief   Convert Bluetooth address to string
 *
 * @return  none
 */
char *bdAddr2Str( uint8 *pAddr )
{
  uint8       i;
  char        hex[] = "0123456789ABCDEF";
  static char str[B_ADDR_STR_LEN];
  char        *pStr = str;
  
  *pStr++ = '0';
  *pStr++ = 'x';
  
  // Start from end of addr
  pAddr += B_ADDR_LEN;
  
  for ( i = B_ADDR_LEN; i > 0; i-- )
  {
    *pStr++ = hex[*--pAddr >> 4];
    *pStr++ = hex[*pAddr & 0x0F];
  }
  
  *pStr = 0;
  
  return str;
}


/*********************************************************************
*********************************************************************/
