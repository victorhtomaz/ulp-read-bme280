#include "bme280.h"
#include "esp_log.h"
#include "driver/i2c.h"

#define I2C_MASTER_SCL_IO    15    // GPIO do SCL
#define I2C_MASTER_SDA_IO    4     // GPIO do SDA
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   100000  // 100kHz
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

esp_err_t i2c_read_bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len) {
    return i2c_master_write_read_device(I2C_NUM_0, dev_addr, &reg_addr, 1, data, len, 1000 / portTICK_PERIOD_MS);
}

void i2c_master_init() {
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    
    conf.clk_flags = 0;

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                                       I2C_MASTER_RX_BUF_DISABLE,
                                       I2C_MASTER_TX_BUF_DISABLE, 0));
}

RTC_DATA_ATTR bme280_calib_data calib;
static int32_t t_fine; // variável global usada nas compensações

int32_t compensate_temperature(int32_t adc_T) {
    int32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((int32_t)calib.dig_T1 << 1))) * ((int32_t)calib.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)calib.dig_T1))) >> 12) * ((int32_t)calib.dig_T3)) >> 14;
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8; // resultado em centésimos de °C
    return T;
}

uint32_t compensate_pressure(int32_t adc_P) {
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calib.dig_P6;
    var2 = var2 + ((var1 * (int64_t)calib.dig_P5) << 17);
    var2 = var2 + (((int64_t)calib.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)calib.dig_P3) >> 8) + ((var1 * (int64_t)calib.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib.dig_P1) >> 33;

    if (var1 == 0) return 0; // evita divisão por zero

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)calib.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)calib.dig_P7) << 4);
    return (uint32_t)p; // resultado em Pa
}

uint32_t compensate_humidity(int32_t adc_H) {
    int32_t v_x1_u32r;
    v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)calib.dig_H4) << 20) -
                    (((int32_t)calib.dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
                 (((((((v_x1_u32r * ((int32_t)calib.dig_H6)) >> 10) *
                      (((v_x1_u32r * ((int32_t)calib.dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) *
                   ((int32_t)calib.dig_H2) + 8192) >> 14));
    v_x1_u32r = v_x1_u32r - (((((v_x1_u32r >> 15) *
                                (v_x1_u32r >> 15)) >> 7) *
                               ((int32_t)calib.dig_H1)) >> 4);
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
    return (uint32_t)(v_x1_u32r >> 12); // resultado em %RH (fix-point, divida por 1024.0)
}



void read_calibration_bme280(){
    uint8_t calib_data[26];
    uint8_t hum_data[7];

    i2c_read_bytes(SLAVE_ADDR, 0x88, calib_data, 26);

    calib.dig_T1 = (uint16_t)(calib_data[1] << 8 | calib_data[0]);
    calib.dig_T2 = (int16_t)(calib_data[3] << 8 | calib_data[2]);
    calib.dig_T3 = (int16_t)(calib_data[5] << 8 | calib_data[4]);

    calib.dig_P1 = (uint16_t)(calib_data[7] << 8 | calib_data[6]);
    calib.dig_P2 = (int16_t)(calib_data[9] << 8 | calib_data[8]);
    calib.dig_P3 = (int16_t)(calib_data[11] << 8 | calib_data[10]);
    calib.dig_P4 = (int16_t)(calib_data[13] << 8 | calib_data[12]);
    calib.dig_P5 = (int16_t)(calib_data[15] << 8 | calib_data[14]);
    calib.dig_P6 = (int16_t)(calib_data[17] << 8 | calib_data[16]);
    calib.dig_P7 = (int16_t)(calib_data[19] << 8 | calib_data[18]);
    calib.dig_P8 = (int16_t)(calib_data[21] << 8 | calib_data[20]);
    calib.dig_P9 = (int16_t)(calib_data[23] << 8 | calib_data[22]);

    i2c_read_bytes(SLAVE_ADDR, 0xA1, &hum_data[0], 1);
    i2c_read_bytes(SLAVE_ADDR, 0xE1, &hum_data[1], 7);

    calib.dig_H1 = hum_data[0];
    calib.dig_H2 = (int16_t)(hum_data[2] << 8 | hum_data[1]);
    calib.dig_H3 = hum_data[3];
    calib.dig_H4 = (int16_t)((hum_data[4] << 4) | (hum_data[5] & 0x0F));
    calib.dig_H5 = (int16_t)((hum_data[6] << 4) | ((hum_data[5] >> 4) & 0x0F));
    calib.dig_H6 = (int8_t)hum_data[7]; 
}