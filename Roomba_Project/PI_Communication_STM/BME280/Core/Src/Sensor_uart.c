#include "Sensor_uart.h"
#include <stdio.h>

static void SendFrame(UART_HandleTypeDef *huart, const char *payload)
{
	char frame[64];
	uint8_t cs = 0;

	for (const char *p = payload; *p; ++p)
	{
		cs ^= (uint8_t)*p;
	}

	int n = snprintf(frame, sizeof(frame), "$%s*%02X\r\n", payload, (unsigned int)cs);
	if (n > 0 && n < (int)sizeof(frame))
	{
		HAL_UART_Transmit(huart, (uint8_t *)frame, (uint16_t)n, 100);
	}
}

static long RoundToLong(float v)
{
	return (long)(v + (v >= 0.0f ? 0.5f : -0.5f));
}

void SensorUart_Send(UART_HandleTypeDef *huart, float temperature, float pressurePa, float humidity)
{
	char payload[48];

	// BME280_Measure() zet alles op 0 als de sensor niet reageert
	if (temperature == 0.0f && pressurePa == 0.0f && humidity == 0.0f)
	{
		SendFrame(huart, "ERR,nodata");
		return;
	}

	snprintf(payload, sizeof(payload), "BME,%ld,%ld,%ld",
	         RoundToLong(temperature * 100.0f),
	         RoundToLong(pressurePa),
	         RoundToLong(humidity * 100.0f));
	SendFrame(huart, payload);
}
