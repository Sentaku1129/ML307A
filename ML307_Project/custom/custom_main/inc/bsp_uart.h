#ifndef _BSP_UART_H_
#define _BSP_UART_H_

#include "cm_os.h"

#ifndef bsp_uart_0
#define bsp_uart_0 0
#endif
#ifndef bsp_uart_1
#define bsp_uart_1 1
#endif
#ifndef bsp_uart_2
#define bsp_uart_2 2
#endif

#define BSP_UART0TX_IOMUX CM_IOMUX_PIN_15, CM_IOMUX_FUNC_FUNCTION1
#define BSP_UART0RX_IOMUX CM_IOMUX_PIN_14, CM_IOMUX_FUNC_FUNCTION1

#define BSP_UART1TX_IOMUX CM_IOMUX_PIN_28, CM_IOMUX_FUNC_FUNCTION1
#define BSP_UART1RX_IOMUX CM_IOMUX_PIN_29, CM_IOMUX_FUNC_FUNCTION1

void uart0_init(void);
void uart1_init(void);

void uart0_printf(char *str, ...);

#endif