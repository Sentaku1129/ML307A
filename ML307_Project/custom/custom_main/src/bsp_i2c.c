#include "stdint.h"
#include "cm_i2c.h"
#include "cm_os.h"

#include "bsp_i2c.h"
#include "bsp_uart.h"
#include "custom_main.h"
#include "inv_mpu.h"

#define CM_DEMO_I2C_LOG uart0_printf

#define MPU6050_DEV_ADDR 0x68
#define MPU6050_I2C_ID CM_I2C_DEV_0

/**
 * @brief       MPU6050软件复位
 * @param       无
 * @retval      无
 */
void mpu6050_sw_reset(void)
{
    mpu6050_write_byte(MPU_PWR_MGMT1_REG, 0x80);
    osDelay(100);
    mpu6050_write_byte(MPU_PWR_MGMT1_REG, 0x00);
}

/**
 * @brief       MPU6050设置陀螺仪传感器量程范围
 * @param       frs: 0 --> ±250dps
 *                   1 --> ±500dps
 *                   2 --> ±1000dps
 *                   3 --> ±2000dps
 */
int32_t mpu6050_set_gyro_fsr(uint8_t fsr)
{
    return mpu6050_write_byte(MPU_GYRO_CFG_REG, fsr << 3);
}

/**
 * @brief       MPU6050设置加速度传感器量程范围
 * @param       frs: 0 --> ±2g
 *                   1 --> ±4g
 *                   2 --> ±8g
 *                   3 --> ±16g
 */
int32_t mpu6050_set_accel_fsr(uint8_t fsr)
{
    return mpu6050_write_byte(MPU_ACCEL_CFG_REG, fsr << 3);
}

/**
 * @brief       MPU6050设置数字低通滤波器频率
 * @param       lpf: 数字低通滤波器的频率（Hz）
 * @retval      MPU6050_EOK : 函数执行成功
 *              MPU6050_EACK: IIC通讯ACK错误，函数执行失败
 */
int32_t mpu6050_set_lpf(uint16_t lpf)
{
    uint8_t dat;

    if (lpf >= 188)
    {
        dat = 1;
    }
    else if (lpf >= 98)
    {
        dat = 2;
    }
    else if (lpf >= 42)
    {
        dat = 3;
    }
    else if (lpf >= 20)
    {
        dat = 4;
    }
    else if (lpf >= 10)
    {
        dat = 5;
    }
    else
    {
        dat = 6;
    }

    return mpu6050_write_byte(MPU_CFG_REG, dat);
}

/**
 * @brief       MPU6050设置采样率
 * @param       rate: 采样率（4~1000Hz）
 * @retval      int32_t: 错误:<=0 正确>0
 */
int32_t mpu6050_set_rate(uint16_t rate)
{
    int32_t ret;
    uint8_t dat;

    if (rate > 1000)
    {
        rate = 1000;
    }

    if (rate < 4)
    {
        rate = 4;
    }

    dat = 1000 / rate - 1;
    ret = mpu6050_write_byte(MPU_SAMPLE_RATE_REG, dat);
    if (ret <= 0)
    {
        return ret;
    }

    ret = mpu6050_set_lpf(rate >> 1);
    if (ret <= 0)
    {
        return ret;
    }

    return ret;
}

int32_t mpu6050_write_byte(uint8_t addr, uint8_t data)
{
    uint8_t tmp[2] = {0};
    int32_t ret = -1;

    // tmp[0] = (addr >> 8) & 0xff;
    // tmp[1] = addr & 0xff;
    tmp[0] = addr & 0xff;
    tmp[1] = data;

    ret = cm_i2c_write(MPU6050_I2C_ID, MPU6050_DEV_ADDR, tmp, 2);
    if (ret < 0)
    {
        CM_DEMO_I2C_LOG("i2c write mpu6050 addr err:0x%x\r\n", ret);
        return -1;
    }
    osDelay(10); // 延时等待写入完成

    return ret;
}

int32_t mpu6050_read_byte(uint16_t addr, uint8_t *data, uint32_t len)
{
    int8_t tmp[2] = {0};
    int32_t ret;

    if (data == NULL)
    {
        CM_DEMO_I2C_LOG("mpu6050_read_byte data ptr err\r\n");
        return -1;
    }

    // tmp[0] = (addr >> 8) & 0xff;
    // tmp[1] = addr & 0xff;
    tmp[0] = addr & 0xff;

    ret = cm_i2c_write(MPU6050_I2C_ID, MPU6050_DEV_ADDR, tmp, 1);
    if (ret < 0)
    {
        CM_DEMO_I2C_LOG("i2c read addr err(w):%08x\r\n", ret);
        return ret;
    }

    ret = cm_i2c_read(MPU6050_I2C_ID, MPU6050_DEV_ADDR, data, len);
    if (ret < 0)
    {
        CM_DEMO_I2C_LOG("i2c read data err(r):%08x\r\n", ret);
        return ret;
    }

    return ret;
}

int32_t MPU_IIC_Init()
{
    cm_i2c_cfg_t config = {
        CM_I2C_ADDR_TYPE_7BIT,
        CM_I2C_MODE_MASTER, // 目前仅支持模式
        CM_I2C_CLK_100KHZ   // master模式,(100KHZ)
    };

    CM_DEMO_I2C_LOG("i2c test start, i2c num:%d!!\n", MPU6050_I2C_ID);

    int32_t ret = cm_i2c_open(MPU6050_I2C_ID, &config);

    if (ret != 0)
    {
        CM_DEMO_I2C_LOG("i2c init err, ret=%d\n", ret);
        return -1;
    }
    CM_DEMO_I2C_LOG("i2c init ok\n");
    return 0;
}

int32_t mpu6050_data_task()
{
    uint8_t id = 0; // 存储MPU6050设备id
    int32_t ret = 0;

    MPU_IIC_Init();

    /* 初始化MPU6050 */
    mpu6050_sw_reset();                                 /* ATK-MS050软件复位 */
    mpu6050_set_gyro_fsr(3);                            /* 陀螺仪传感器，±2000dps */
    mpu6050_set_accel_fsr(1);                           /* 加速度传感器，±4g */
    mpu6050_set_rate(50);                               /* 采样率，50Hz */
    mpu6050_write_byte(MPU_INT_EN_REG, 0x00);           /* 关闭所有中断 */
    mpu6050_write_byte(MPU_USER_CTRL_REG, 0x00);        /* 关闭IIC主模式 */
    mpu6050_write_byte(MPU_FIFO_EN_REG, 0x00);          /* 关闭FIFO */
    mpu6050_write_byte(MPU_INTBP_CFG_REG, 0x80);        /* INT引脚低电平有效 */
    ret = mpu6050_read_byte(MPU_DEVICE_ID_REG, &id, 1); /* 读取设备ID */
    if (id != MPU6050_IIC_ADDR)
    {
        uart0_printf("ret is %d\r\n", ret);
        uart0_printf("mpu6050_data_task: addr is err %d\r\n", id);
    }
    else
    {
        uart0_printf("ret is %d\r\n", ret);
        uart0_printf("mpu6050_data_task: addr is %d\r\n", id);
    }
    mpu6050_write_byte(MPU_PWR_MGMT1_REG, 0x01); /* 设置CLKSEL，PLL X轴为参考 */
    mpu6050_write_byte(MPU_PWR_MGMT2_REG, 0x00); /* 加速度与陀螺仪都工作 */
    mpu6050_set_rate(50);                        /* 采样率，50Hz */

    if(mpu_dmp_init() != 0)
    {
        uart0_printf("%s: dmp init err!\r\n", __func__);
    }

    g_sensor_t i2c_sensor = {0};
    uint8_t sensor_data[6];
    uint8_t sensor_status = 0x00;

    while (1)
    {
        // 获取加速度
        ret = mpu6050_read_byte(MPU_ACCEL_XOUTH_REG, sensor_data, 6);
        if (ret == 6)
        {
            i2c_sensor.g_acceleration.x = ((float)((int16_t)(sensor_data[0] << 8) | sensor_data[1])) / 32768 * 4;
            i2c_sensor.g_acceleration.y = ((float)((int16_t)(sensor_data[2] << 8) | sensor_data[3])) / 32768 * 4;
            i2c_sensor.g_acceleration.z = ((float)((int16_t)(sensor_data[4] << 8) | sensor_data[5])) / 32768 * 4;
            sensor_status |= 0x01;
        }
        else
        {
            uart0_printf("ret is %d\r\n", ret);
            uart0_printf("mpu6050_data_task: acc get err!\r\n");
        }

        // 获取角速度
        ret = mpu6050_read_byte(MPU_GYRO_XOUTH_REG, sensor_data, 6);
        if (ret == 6)
        {
            i2c_sensor.g_palstance.x = ((float)((int16_t)(sensor_data[0] << 8) | sensor_data[1])) / 32768 * 2000;
            i2c_sensor.g_palstance.y = ((float)((int16_t)(sensor_data[2] << 8) | sensor_data[3])) / 32768 * 2000;
            i2c_sensor.g_palstance.z = ((float)((int16_t)(sensor_data[4] << 8) | sensor_data[5])) / 32768 * 2000;
            sensor_status |= 0x02;
        }
        else
        {
            uart0_printf("mpu6050_data_task: pal get err!\r\n");
        }

        // 获取角度
        ret = mpu_dmp_get_data(&i2c_sensor.g_angle.x, &i2c_sensor.g_angle.y, &i2c_sensor.g_angle.z);
        if(ret == 0)
        {
            uart0_printf("%s: get angle OK!\r\n", __func__);
        }
        else
        {
            uart0_printf("%s: get angle err!\r\n", __func__);
        }


        if (sensor_status == 0x07)
        {
            sensor_status = 0x00;
            if (osMessageQueuePut(sensor_MessageQueueId, &i2c_sensor, 0, 0) != osOK)
            {
                uart0_printf("%s: send err!!!\r\n");
            }
            else
            {
                uart0_printf("mpu6050_data_task: sensor_data had send\r\n");
                uart0_printf("%s: %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f\r\n", __func__,
                             i2c_sensor.g_acceleration.x, i2c_sensor.g_acceleration.y, i2c_sensor.g_acceleration.z,
                             i2c_sensor.g_palstance.x, i2c_sensor.g_palstance.y, i2c_sensor.g_palstance.z,
                             i2c_sensor.g_angle.x, i2c_sensor.g_angle.y, i2c_sensor.g_angle.z);
            }
        }
        osDelay(1000);
    }
    cm_i2c_close(MPU6050_I2C_ID);
    return 0;
}