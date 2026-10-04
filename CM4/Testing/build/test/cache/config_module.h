#include "../App/Inc/power_fsm.h"
#include "../App/Inc/rtos_iface.h"
#include "../App/Inc/config_cmd.h"






typedef enum {

    LORA_SET_TIMEOUT = 0x01,

    LORA_SET_THRESHOLD = 0x02,

    LORA_FORCE_READ = 0x03,

    LORA_SET_BATTERY_TH = 0x04,

    LORA_ENABLE_SENSOR = 0x05,

    LORA_SET_WINDOW = 0x06,

    LORA_FORCE_SYSTEM_MODE = 0x07,

} lora_opcode_t;











void ConfigModule_Init(power_fsm_t* p_power_fsm);









_Bool ConfigModule_RegisterSensor(uint8_t sensor_id, queue_handle_t queue);

_Bool ConfigModule_RegisterTaskHandle(uint8_t sensor_id, void* task_handle);









void ConfigModule_HandleLoRaFrame(const uint8_t* payload, uint8_t size);







void Config_ApplySystemMode(power_state_t mode);
