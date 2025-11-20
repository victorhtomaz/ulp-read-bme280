# ULP BME280 - Leitura de Sensor com Coprocessador ULP do ESP32

## 📋 Descrição

Este projeto demonstra a leitura do sensor BME280 (temperatura, pressão e umidade) utilizando o **coprocessador ULP (Ultra Low Power)** do ESP32. O ULP executa leituras I2C via bit-banging enquanto o processador principal permanece em deep sleep, proporcionando consumo de energia extremamente baixo.

### Características Principais

- ✅ Leitura autônoma pelo ULP em deep sleep
- ✅ Comunicação I2C por bit-banging (não utiliza periféricos de hardware)
- ✅ Compensação de valores brutos usando coeficientes de calibração do BME280

## 🔧 Pré-requisitos

### Software

- **ESP-IDF**: v4.4
- **Python**: 3.8 ou superior (incluído na instalação do ESP-IDF)
- **Git**: Para clonar o repositório
- **Toolchain**: xtensa-esp32-elf (instalado automaticamente pelo ESP-IDF)

### Hardware

- **ESP32** (qualquer variante com suporte a ULP)
- **Sensor BME280** (I2C)
- **Cabos de conexão**

#### Pinagem Padrão

| Periférico | GPIO ESP32 |
|------------|-----------|
| SCL (I2C)  | GPIO 15   |
| SDA (I2C)  | GPIO 4    |
| LED Erro   | GPIO 2    |

**Nota:** Os pinos devem ser capazes de operar no modo RTC (RTC GPIO).

## 📦 Instalação e Configuração

### 1. Clonar o Repositório

```bash
git clone https://github.com/victorhtomaz/ulp-read-bme280.git
cd ulp-bme280
```

## 📁 Estrutura do Projeto

```
ulp-bme280/
├── CMakeLists.txt              # Configuração principal do projeto
├── sdkconfig                   # Configurações do ESP-IDF
├── README.md                   # Este arquivo
│
├── main/                       # Código-fonte principal
│   ├── CMakeLists.txt          # Build do componente main
│   ├── main.cpp                # Aplicação principal (app_main)
│   ├── bme280.cpp              # Funções de comunicação I2C e compensação
│   ├── ulp_bme280.cpp          # Programa ULP e inicialização
│   └── include/                # Headers do projeto
│       ├── bme280.h            # Interface do BME280
│       └── ulp_bme280.h        # Interface do programa ULP
│
├── components/                 # Componentes externos
│   └── hulp/                   # Biblioteca HULP (ULP Helper)
```

### Descrição dos Arquivos Principais

#### **`main/main.cpp`**
Contém a função `app_main()` que:
1. Verifica se acordou do deep sleep pelo ULP
2. Se sim, lê os dados salvos na memória RTC e calcula temperatura, pressão e umidade
3. Se não, inicializa o I2C, lê a calibração do BME280 e carrega o programa ULP
4. Entra em deep sleep aguardando o próximo wakeup do ULP

#### **`main/bme280.cpp`**
Implementa:
- Inicialização do I2C hardware (para leitura inicial dos coeficientes)
- Leitura dos coeficientes de calibração do BME280
- Funções de compensação (temperatura, pressão, umidade)

#### **`main/ulp_bme280.cpp`**
Implementa:
- Programa ULP em assembly (usando macros HULP)
- Configuração dos pinos RTC GPIO
- Sequência de leitura I2C via bit-banging:
  1. Escreve no registrador de controle de umidade (0xF2)
  2. Escreve no registrador de controle de medição (0xF4)
  3. Aguarda 120ms para o sensor concluir a medição
  4. Lê 8 bytes a partir do registrador 0xF7 (pressão, temperatura, umidade)
  5. Armazena os dados na memória RTC
  6. Acorda o processador principal

#### **`components/hulp/`**
Biblioteca externa para facilitar a programação do ULP com macros de alto nível.

## 🚀 Uso

### Executar o Projeto

Após gravar o firmware, o ESP32:
1. Inicializa o I2C e lê os coeficientes de calibração do BME280
2. Carrega o programa ULP
3. Entra em deep sleep
4. O ULP acorda periodicamente (configurado para ~10s), lê o sensor e acorda o CPU principal
5. O CPU principal processa os dados e imprime no console
6. Volta para deep sleep

### Exemplo de Saída no Monitor Serial

```
I (312) bme280_main: Valores lidos pelo ULP:
I (322) bme280_main: Temp bytes: MSB=0x0080, LSB=0x0072, XLSB=0x0088
I (322) bme280_main: Press bytes: MSB=0x0064, LSB=0x006A, XLSB=0x005D
I (332) bme280_main: Hum bytes: MSB=0x0079, LSB=0x0044
I (342) bme280_main: Valores brutos combinados:
I (342) bme280_main: Temp: 524424, Press: 410205, Hum: 31044
I (352) bme280_main: Valores compensados:
I (352) bme280_main: Temp: 25.43 °C, Press: 101325.67 Pa, Hum: 45.23 %
I (362) bme280_main: Entrando em deep sleep...
```

## 📚 Referências

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [ULP Coprocessor Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/ulp.html)
- [BME280 Datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf)
- [HULP Library](https://github.com/boarchuz/HULP) - Biblioteca para facilitar programação ULP

## 📄 Licença

Este projeto é disponibilizado sob a **Licença MIT**.

### Bibliotecas de Terceiros

- **HULP (ULP Helper Library)**: [MIT License](components/hulp/LICENSE)
- **ESP-IDF**: Apache License 2.0

## ⚠️ Troubleshooting

### Erro: "GPIO não é RTC-capable"
**Solução:** Use apenas GPIOs suportados pelo RTC (0, 2, 4, 12-15, 25-27, 32-39)

### Valores de temperatura/pressão incorretos
**Solução:** Verifique se os coeficientes de calibração foram lidos corretamente e se o endereço I2C está correto (0x76 ou 0x77)

### ULP não acorda o CPU principal
**Solução:** Verifique as conexões I2C e se o sensor está energizado corretamente. Monitore o LED de erro (GPIO 2)

### Erro: "ULP coprocessor not supported"
**Solução:** Certifique-se de que o ULP está habilitado no menuconfig:
```bash
idf.py menuconfig
# Component config → ESP32-specific → ULP coprocessor support
# [*] Enable Ultra Low Power (ULP) Coprocessor
# (X) FSM (Finite State Machine)
```