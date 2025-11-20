#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_sleep.h"

#include "ulp_bme280.h"
#include "bme280.h"

extern "C" void app_main(void);

static const char *TAG = "bme280_main";

void app_main(void)
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    if (cause == ESP_SLEEP_WAKEUP_ULP) {
        // Leitura do último valor salvo pelo ULP no RTC
        ESP_LOGI(TAG, "Valores lidos pelo ULP:");
        ESP_LOGI(TAG, "Temp bytes: MSB=0x%04X, LSB=0x%04X, XLSB=0x%04X", 
                 last_temp_msb.val, last_temp_lsb.val, last_temp_xlsb.val);
        ESP_LOGI(TAG, "Press bytes: MSB=0x%04X, LSB=0x%04X, XLSB=0x%04X", 
                 last_press_msb.val, last_press_lsb.val, last_press_xlsb.val);
        ESP_LOGI(TAG, "Hum bytes: MSB=0x%04X, LSB=0x%04X", 
                 last_hum_msb.val, last_hum_lsb.val);
        
         // Ajusta os bits lidos para formar os valores brutos completos
        int32_t adc_T = (last_temp_msb.val << 12) | (last_temp_lsb.val << 4)  | (last_temp_xlsb.val >> 4);

        int32_t adc_P = (last_press_msb.val << 12) | (last_press_lsb.val << 4)  | (last_press_xlsb.val >> 4);

        int32_t adc_H = (last_hum_msb.val << 8) | (last_hum_lsb.val);

        ESP_LOGI(TAG, "Valores brutos combinados:");
        ESP_LOGI(TAG, "Temp: %d, Press: %d, Hum: %d", adc_T, adc_P, adc_H);

        // Temperatura precisa ser lida primeiro
        double temp_c = compensate_temperature(adc_T) / 100.0;
        double press_pa = compensate_pressure(adc_P) / 256.0; // ajuste conforme datasheet
        double hum_rh = compensate_humidity(adc_H) / 1024.0;

        ESP_LOGI(TAG, "Valores compensados:");
        ESP_LOGI(TAG, "Temp: %.2f °C, Press: %.2f Pa, Hum: %.2f %%", temp_c, press_pa, hum_rh);
    } else {
        // Primeira execução → inicializa ULP
        // Para realizar a calibração dos dados brutos lidos do bme280 é necessário ler os bits de calibração
        i2c_master_init();
        read_calibration_bme280();
        init_bme280_ulp();
    }

    ESP_LOGI(TAG, "Entrando em deep sleep...");
    esp_sleep_enable_ulp_wakeup();
    esp_deep_sleep_start();
}