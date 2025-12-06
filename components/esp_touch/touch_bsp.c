#include <stdio.h>
#include "touch_bsp.h"
#include "i2c_bsp.h"
#define I2C_Touch_ADDR 0x15

void touch_Init(void)
{
  uint8_t data = 0x00;
  ESP_ERROR_CHECK(I2C_writr_buff(I2C_Touch_ADDR,0x00,&data,1)); //切换正常模式
}
uint8_t getTouch(uint16_t *x,uint16_t *y)
{
  uint8_t _num;
  uint8_t tp_temp[7];
  I2C_read_buff(I2C_Touch_ADDR,0x00,tp_temp,7);
  _num = tp_temp[2];
  if(_num)
  {
    *x = ((uint16_t)(tp_temp[3] & 0x0f)<<8) + (uint16_t)tp_temp[4];
    *y = ((uint16_t)(tp_temp[5] & 0x0f)<<8) + (uint16_t)tp_temp[6];
    return 1;
  }
  return 0;
}