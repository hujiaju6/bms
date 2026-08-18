#ifndef __UI_MESSAGES_H
#define __UI_MESSAGES_H

#include "bms_data.h"
#include "bsp_can.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RS485_DATA_MAX_LEN  256

typedef struct {
    uint16_t len;
    uint8_t  data[RS485_DATA_MAX_LEN];
} RS485_Data_t;

typedef struct {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
} RTC_Time_t;

typedef enum {
    UI_MSG_NONE = 0,
    UI_MSG_BMS_UPDATE,
    UI_MSG_CAN_RX,
    UI_MSG_CAN_STATUS,
    UI_MSG_RS485_RX,
    UI_MSG_RS485_TX,
    UI_MSG_RTC_TIME,
} UI_MessageType_t;

typedef struct {
    UI_MessageType_t type;
    union {
        BMS_Data_t      bms;
        CAN_DataPacket_t can;
        uint32_t         status;
        RS485_Data_t     rs485;
        RTC_Time_t       rtc;
    } payload;
} UI_Message_t;

typedef enum {
    CAN_CMD_NONE = 0,
    CAN_CMD_SEND_TEST,
} CAN_CommandType_t;

typedef struct {
    CAN_CommandType_t type;
} CAN_Command_t;

#ifdef __cplusplus
}
#endif

#endif /* __UI_MESSAGES_H */
