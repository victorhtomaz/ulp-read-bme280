#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rtc_io.h"

#include "ulp_bme280.h"

#include "hulp_i2cbb.h"

#define SCL_PIN GPIO_NUM_15
#define SDA_PIN GPIO_NUM_4
#define LED_ERR GPIO_NUM_2   // exemplo: GPIO2, precisa ser RTC-capable

#define SLAVE_ADDR 0x76 // Endereço I2C do BME280

#define SLAVE_READ8_SUBADDR 0xF7 // Endereço do primeiro registrador de leitura (pressão)

static const uint8_t BME280_REG_CTRL_HUM  = 0xF2; // Endereço do registrador de controle de umidade
static const uint8_t BME280_REG_CTRL_MEAS = 0xF4; // Endereço do registrador de controle de medição

RTC_DATA_ATTR ulp_var_t ulp_nacks;
RTC_DATA_ATTR ulp_var_t ulp_buserrors;

// Configuram forma inicialização do sensor
static RTC_SLOW_ATTR ulp_var_t ulp_write_ctrl_hum[] = {
    HULP_I2C_CMD_HDR(SLAVE_ADDR, BME280_REG_CTRL_HUM, 1),
    HULP_I2C_CMD_1B(0x05), // Configura oversampling x16 para humidade
};

static RTC_SLOW_ATTR ulp_var_t ulp_write_ctrl_meas[] = {
    HULP_I2C_CMD_HDR(SLAVE_ADDR, BME280_REG_CTRL_MEAS, 1),
    HULP_I2C_CMD_1B(0xB5), // Configura oversampling x16 para temperatura e pressão, forced mode
};

RTC_SLOW_ATTR ulp_var_t ulp_read_cmd[HULP_I2C_CMD_BUF_SIZE(8)] = {
     HULP_I2C_CMD_HDR(SLAVE_ADDR, SLAVE_READ8_SUBADDR, 8), // Lê 8 bytes a partir do registrador de pressão
};

RTC_DATA_ATTR ulp_var_t  last_press_msb;
RTC_DATA_ATTR ulp_var_t  last_press_lsb;
RTC_DATA_ATTR ulp_var_t  last_press_xlsb;

RTC_DATA_ATTR ulp_var_t  last_temp_msb;
RTC_DATA_ATTR ulp_var_t  last_temp_lsb;
RTC_DATA_ATTR ulp_var_t  last_temp_xlsb;

RTC_DATA_ATTR ulp_var_t  last_hum_msb;
RTC_DATA_ATTR ulp_var_t  last_hum_lsb;

void init_bme280_ulp(){
    enum {
        LABEL_I2C_READ,
        LABEL_I2C_READ_RETURN,
        LABEL_I2C_WRITE,
        LABEL_WRITE_HUM_RETURN,
        LABEL_WRITE_MEAS_RETURN,
        LABEL_I2C_ERROR,
        LABEL_NEGATIVE,
        LABEL_WAKE,
    };
    const ulp_insn_t program[] = {
        //Configura humidade
        I_MOVO(R1, ulp_write_ctrl_hum), // Configura escrita
        M_MOVL(R3, LABEL_WRITE_HUM_RETURN), //Define ponto de retorno
        M_BX(LABEL_I2C_WRITE), //Salta para a rotina de escrita
        M_LABEL(LABEL_WRITE_HUM_RETURN), //Define label de retorno
        M_BGE(LABEL_I2C_ERROR, 1), //Teste erro na comunicação

        //Configura medição de temperatura e pressão
        I_MOVO(R1, ulp_write_ctrl_meas),
        M_MOVL(R3, LABEL_WRITE_MEAS_RETURN),
        M_BX(LABEL_I2C_WRITE),
        M_LABEL(LABEL_WRITE_MEAS_RETURN),
        M_BGE(LABEL_I2C_ERROR, 1),

        M_DELAY_US_5000_20000(120000), //Aguarda 120ms para medição

        I_MOVO(R1, ulp_read_cmd), //Configura leitura
        M_MOVL(R3, LABEL_I2C_READ_RETURN), //Define ponto de retorno
        M_BX(LABEL_I2C_READ), //Salta para a rotina de leitura
        M_LABEL(LABEL_I2C_READ_RETURN), //Define label de retorno
        M_BGE(LABEL_I2C_ERROR, 1), //Teste erro na comunicação

        //Pegar dois primeiros bytes de pressão -> msb e lsb
        I_MOVI(R0, 0),
        I_GET(R1, R0, ulp_read_cmd[HULP_I2C_CMD_DATA_OFFSET + 0]), // É obrigatório R0 = 0 para GET/PUT
        I_MOVR(R2, R1),

        // Isolar o msb
        I_MOVI(R0, 8),
        I_RSHR(R1, R1, R0),

        // Isolar o lsb
        I_ANDI(R2, R2, 0xFF),

        // Armazenar msb e lsb de pressão
        I_MOVI(R0, 0),
        I_PUT(R1, R0, last_press_msb), // É obrigatório R0 = 0 para GET/PUT
        I_PUT(R2, R0, last_press_lsb),

        // Pegar dois próximos bytes de pressão e temperatura -> press_xlsb e temp_msb
        I_MOVI(R0, 0),
        I_GET(R1, R0, ulp_read_cmd[HULP_I2C_CMD_DATA_OFFSET + 1]),
        I_MOVR(R2, R1),

        // Isolar o xlsb de pressão
        I_MOVI(R0, 8),
        I_RSHR(R1, R1, R0),

        // Isolar o lsb
        I_ANDI(R2, R2, 0xFF),
        
        // Armazenar xlsb de pressão e msb de temperatura
        I_MOVI(R0, 0),
        I_PUT(R1, R0, last_press_xlsb),
        I_PUT(R2, R0, last_temp_msb),

        // Pegar dois próximos bytes de temperatura -> temp_lsb e temp_xlsb
        I_MOVI(R0, 0),
        I_GET(R1, R0, ulp_read_cmd[HULP_I2C_CMD_DATA_OFFSET + 2]),
        I_MOVR(R2, R1),

        // Isolar o lsb
        I_MOVI(R0, 8),
        I_RSHR(R1, R1, R0),

        // Isolar o xlsb
        I_ANDI(R2, R2, 0xFF),

        // Armazenar lsb e xlsb de temperatura
        I_MOVI(R0, 0),
        I_PUT(R1, R0, last_temp_lsb),
        I_PUT(R2, R0, last_temp_xlsb),

        // Pegar dois próximos bytes de humidade -> hum_msb e hum_lsb
        I_MOVI(R0, 0),
        I_GET(R1, R0, ulp_read_cmd[HULP_I2C_CMD_DATA_OFFSET + 3]),
        I_MOVR(R2, R1),

        // Isolar o msb
        I_MOVI(R0, 8),
        I_RSHR(R1, R1, R0),

        // Isolar o lsb
        I_ANDI(R2, R2, 0xFF),

        // Armazenar msb e lsb de humidade
        I_MOVI(R0, 0),
        I_PUT(R1, R0, last_hum_msb),
        I_PUT(R2, R0, last_hum_lsb),

        // Acorda o chip principal sempre que terminar leitura
        I_WAKE(),
        I_HALT(),

        M_LABEL(LABEL_I2C_ERROR),
        I_GPIO_SET(LED_ERR, 1),
        I_END(),
        I_HALT(),

        M_INCLUDE_I2CBB_CMD(LABEL_I2C_READ, LABEL_I2C_WRITE, SCL_PIN, SDA_PIN),
    };

    ESP_ERROR_CHECK(hulp_configure_pin(SCL_PIN, RTC_GPIO_MODE_INPUT_ONLY, GPIO_PULLUP_ONLY, 0));
    ESP_ERROR_CHECK(hulp_configure_pin(SDA_PIN, RTC_GPIO_MODE_INPUT_ONLY, GPIO_PULLUP_ONLY, 0));

    hulp_peripherals_on();

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_ERROR_CHECK(hulp_ulp_load(program, sizeof(program), 10ULL * 1000 * 1000, 0));
    ESP_ERROR_CHECK(hulp_ulp_run(0));
}