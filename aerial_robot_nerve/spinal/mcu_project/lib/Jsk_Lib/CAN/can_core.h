/*
 * can_core.h
 *
 *  Created on: 2016/10/24
 *      Author: anzai
 */

#ifndef APPLICATION_CAN_CAN_CORE_H_
#define APPLICATION_CAN_CAN_CORE_H_

#if defined (STM32F756xx) || defined (STM32F746xx) || defined (STM32F745xx) || defined (STM32F765xx) || \
    defined (STM32F767xx) || defined (STM32F769xx) || defined (STM32F777xx) || defined (STM32F779xx) || \
    defined (STM32F722xx) || defined (STM32F723xx) || defined (STM32F732xx) || defined (STM32F733xx) || \
    defined (STM32F730xx) || defined (STM32F750xx)
#include "stm32f7xx_hal.h"
#endif

#if defined (STM32H743xx) || defined (STM32H753xx)  || defined (STM32H750xx) || defined (STM32H742xx) || \
    defined (STM32H745xx) || defined (STM32H755xx)  || defined (STM32H747xx) || defined (STM32H757xx) || \
    defined (STM32H7A3xx) || defined (STM32H7A3xxQ) || defined (STM32H7B3xx) || defined (STM32H7B3xxQ) || defined (STM32H7B0xx)  || defined (STM32H7B0xxQ) || \
    defined (STM32H735xx) || defined (STM32H733xx)  || defined (STM32H730xx) || defined (STM32H730xxQ)  || defined (STM32H725xx) || defined (STM32H723xx)
#include "stm32h7xx_hal.h"
#endif


#include "stdint.h"
#include "string.h"
#include <vector>
#include "can_constants.h"

namespace CAN {
	void init(CAN_HandleTypeDef* hcan);
	CAN_HandleTypeDef* getHcanInstance();
	void sendMessage(uint8_t device_id, uint8_t message_id, uint8_t slave_id, uint32_t DLC, uint8_t* data, uint32_t timeout);

	inline void CAN_START()
	{
		HAL_CAN_Start(getHcanInstance());
		HAL_CAN_ActivateNotification(getHcanInstance(), CAN_IT_RX_FIFO1_MSG_PENDING);
	}

	inline uint8_t getDeviceId(CAN_RxHeaderTypeDef rx_header) {
		return static_cast<uint8_t>(((rx_header.StdId) >> (MESSAGE_ID_LEN + SLAVE_ID_LEN)) & ((1 << DEVICE_ID_LEN) - 1));
	}

	inline uint8_t getMessageId(CAN_RxHeaderTypeDef rx_header) {
		return static_cast<uint8_t>(((rx_header.StdId) >> SLAVE_ID_LEN) & ((1 << MESSAGE_ID_LEN) - 1));
	}

	inline uint8_t getSlaveId(CAN_RxHeaderTypeDef rx_header) {
		return static_cast<uint8_t>((rx_header.StdId) & ((1 << SLAVE_ID_LEN) - 1));
	}

	inline uint32_t getDlc(CAN_RxHeaderTypeDef rx_header) {
		return rx_header.DLC;
	}
}

#endif /* APPLICATION_CAN_CAN_CORE_H_ */
