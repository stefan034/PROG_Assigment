#ifndef INC_SENSOR_UART_H_
#define INC_SENSOR_UART_H_

#include "stm32f0xx_hal.h"

/* Stuurt één meting als tekstregel naar de Raspberry Pi:
 *   $BME,<temp in 0.01 C>,<druk in Pa>,<vocht in 0.01 %RH>*<XOR-checksum hex>\r\n
 * Zijn alle waarden 0 (sensor niet bereikbaar), dan wordt "$ERR,nodata*CS" gestuurd.
 * Er wordt bewust geen float-printf gebruikt (werkt ook met newlib-nano).
 */
void SensorUart_Send(UART_HandleTypeDef *huart, float temperature, float pressurePa, float humidity);

#endif /* INC_SENSOR_UART_H_ */
