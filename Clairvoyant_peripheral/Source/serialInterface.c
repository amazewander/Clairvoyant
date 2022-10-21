#include "hal_uart.h"
#include "serialInterface.h"
#include "BLE_Bridge.h"
#include "bcomdef.h"

//local function
static void SerialInterface_ProcessOSALMsg( osal_event_hdr_t *pMsg );

uint8 serialInterface_TaskID;   // Task ID for internal task/event processing

#define SERIAL_MSG_START_ID        0xAB //indicate the start of serial message
#define SERIAL_ACK                 0xA5 //inidicate an ACK
#define SERIAL_DATA                0xAD //indiciate data from central device

uint8 serialBuffer[RX_BUFF_SIZE];

uint16 serialBufferOffset = 0;
uint8 packet_length = 0;

//static uint8 temp_buf[48];

void SerialInterface_Init( uint8 task_id )
{
  serialInterface_TaskID = task_id;
    
  NPI_InitTransport(cSerialPacketParser);
}

uint16 SerialInterface_ProcessEvent( uint8 task_id, uint16 events )
{
  
  VOID task_id; // OSAL required parameter that isn't used in this function
  
  if ( events & SYS_EVENT_MSG )
  {
    uint8 *pMsg;
    
    if ( (pMsg = osal_msg_receive( serialInterface_TaskID )) != NULL )
    {
      SerialInterface_ProcessOSALMsg( (osal_event_hdr_t *)pMsg );
      
      // Release the OSAL message
      VOID osal_msg_deallocate( pMsg );
    }
    
    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }
  
  // Discard unknown events
  return 0;
}

static void SerialInterface_ProcessOSALMsg( osal_event_hdr_t *pMsg )
{
  switch ( pMsg->event )
  {
  default:
    // do nothing
    break;
  }
}

void cSerialPacketParser( uint8 port, uint8 events )
{  
  //unused input parameters
  (void)port;
  (void)events;
  
   uint8 numBytes;
      
   // get the number of available bytes to process
   numBytes = NPI_RxBufLen();
   // check if there's any serial port data to process
   while ( numBytes > 0 )
   {
         // alloc temporary buffer
         uint8 *temp_buf;
         temp_buf = osal_mem_alloc( numBytes );
         
         //store dma buffer into temp buffer
         (void)NPI_ReadTransport(temp_buf, numBytes);

         // copy data from temp buffer into rx circular buffer
         for (uint8 i = 0; i < numBytes; i++)
         {
           //copy one byte to data buffer
           serialBuffer[serialBufferOffset] = temp_buf[i];                    
           //update offset
           serialBufferOffset = circular_add(serialBufferOffset,1);
         }
         //free temp buffer
         osal_mem_free(temp_buf);
         numBytes = 0;
   }  
}

uint8 sendAckMessage(uint8 bytes_sent)
{
  uint8 data[3] = {0};
  
  data[0]= SERIAL_MSG_START_ID;
  data[1]= SERIAL_ACK;
  data[2]= bytes_sent;
  uint8 success_len = HalUARTWrite(NPI_UART_PORT, (uint8*)data, 3);
  if (success_len == 3)
  {
    return SUCCESS;
  }
  else
  {
    return 1;   //ack wasn't sent over UAR
  }
}