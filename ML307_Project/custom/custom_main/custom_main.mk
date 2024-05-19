
CUSTOM_MAIN_DIR := custom/custom_main


OC_FILES += $(CUSTOM_MAIN_DIR)/src/custom_main.c
INC      += -I'$(CUSTOM_MAIN_DIR)/inc'
INC      += -I'$(CUSTOM_MAIN_DIR)/eMPL'

# bsp库文件添加
OC_FILES += $(CUSTOM_MAIN_DIR)/src/bsp_uart.c
OC_FILES += $(CUSTOM_MAIN_DIR)/src/bsp_mqtt.c
OC_FILES += $(CUSTOM_MAIN_DIR)/src/bsp_http.c
OC_FILES += $(CUSTOM_MAIN_DIR)/src/bsp_i2c.c

# MPU6050 dmp移植
OC_FILES += $(CUSTOM_MAIN_DIR)/eMPL/inv_mpu_dmp_motion_driver.c
OC_FILES += $(CUSTOM_MAIN_DIR)/eMPL/inv_mpu.c
