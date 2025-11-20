#ifndef ULP_BME280_H
#define ULP_BME280_H

#include "hulp.h"

extern RTC_DATA_ATTR ulp_var_t last_press_msb;
extern RTC_DATA_ATTR ulp_var_t last_press_lsb;
extern RTC_DATA_ATTR ulp_var_t last_press_xlsb;
extern RTC_DATA_ATTR ulp_var_t last_temp_msb;
extern RTC_DATA_ATTR ulp_var_t last_temp_lsb;
extern RTC_DATA_ATTR ulp_var_t last_temp_xlsb;
extern RTC_DATA_ATTR ulp_var_t last_hum_msb;
extern RTC_DATA_ATTR ulp_var_t last_hum_lsb;

void init_bme280_ulp(void);

#endif