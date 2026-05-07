## ⚡ Stack IoT para Monitoramento de Energia (PZEM-004T + ESP8266)

Este repositório contém a infraestrutura e os códigos necessários para implantar um sistema completo de monitoramento de energia em tempo real. O projeto utiliza um microcontrolador ESP8266 com o sensor PZEM-004T, enviando dados via MQTT para uma stack local baseada em Docker composta por **Mosquitto, Telegraf, InfluxDB e Grafana (Stack TIG)**.

## 🏗️ Arquitetura do Sistema
1. **Hardware:** ESP8266 (NodeMCU) + Sensor de Energia PZEM-004T v3.0.
2. **Mensageria (Broker):** Eclipse Mosquitto (MQTT).
3. **Coletor de Dados (ETL):** Telegraf.
4. **Banco de Dados de Séries Temporais:** InfluxDB 2.7.
5. **Visualização (Dashboard):** Grafana.

---

## 🚀 1. Preparação do Ambiente (Docker)

Para que os volumes do Docker funcionem corretamente e armazenem seus dados de forma persistente, crie a estrutura de diretórios na sua máquina host (Linux, macOS ou Windows via PowerShell):

```powershell
# Criação das pastas de configuração e persistência de dados
mkdir ~/mosquitto/config
mkdir ~/mosquitto/data
mkdir ~/mosquitto/log
mkdir ~/influxdb_data
mkdir ~/grafana_data
mkdir ~/telegraf_config
```

Aloque os arquivos de configuração nos seus respectivos diretórios:
* Coloque o arquivo `docker-compose.yml` na raiz do seu projeto.
* Coloque o arquivo `mosquitto.conf` em `~/mosquitto/config/`.
* Coloque o arquivo `telegraf.conf` em `~/telegraf_config/`.

### Arquivo: `mosquitto.conf` (Para testes locais)
Para facilitar os testes iniciais e evitar erros de `Não Autorizado (rc=5)`, configure o Mosquitto para aceitar conexões anônimas de qualquer IP:
```conf
listener 1883 0.0.0.0
allow_anonymous true
```

---

## ⚙️ 2. Subindo a Infraestrutura

Abra o terminal no diretório onde está o seu `docker-compose.yml` e execute:

```bash
docker compose up -d
```
*Este comando fará o download das imagens e iniciará os serviços em segundo plano.*

---

## 🗄️ 3. Configuração do InfluxDB e Telegraf

Como estamos usando o **InfluxDB v2**, a autenticação é baseada em Tokens, o que requer uma configuração pós-instalação.

1. Acesse o InfluxDB no navegador: `http://localhost:8086`.
2. Siga o assistente de configuração inicial:
   * **Username / Password:** Defina suas credenciais.
   * **Organization:** `EngEletrica` (Deve ser idêntico ao `telegraf.conf`).
   * **Bucket:** `sensor_pzem` (Deve ser idêntico ao `telegraf.conf`).
3. Vá no menu lateral: **Load Data** -> **API Tokens** -> **Generate API Token** -> **All Access Token**.
4. Copie o Token gerado e cole no seu arquivo `~/telegraf_config/telegraf.conf`:
   ```toml
   [[outputs.influxdb_v2]]
     urls = ["[http://127.0.0.1:8086](http://127.0.0.1:8086)"]
     token = "COLE_SEU_TOKEN_AQUI"
     organization = "EngEletrica"
     bucket = "sensor_pzem"
   ```
5. Reinicie o Telegraf para aplicar o novo token:
   ```bash
   docker restart telegraf
   ```

---

## 💻 4. Firmware do ESP8266

Certifique-se de que a variável `MQTT_SERVIDOR` no código do seu ESP8266 aponta para o **IP da máquina onde o Docker está rodando** (ex: IP local da sua rede Wi-Fi, descubra usando `ipconfig` no Windows ou `ip a` no Linux).

```cpp
// Exemplo de configuração no código Arduino/C++
const char* MQTT_SERVIDOR = "192.168.1.15"; // IP do Servidor/Notebook
const int   MQTT_PORTA = 1883;
const char* MQTT_TOPICO = "tcc/pzem/data";
```
*Nota: Como o Mosquitto está configurado com `allow_anonymous true` neste setup inicial, o ESP8266 se conectará mesmo sem credenciais. Para ambientes de produção, crie um arquivo de senhas (`passwd`) no Mosquitto.*

---

## 📊 5. Configuração dos Gráficos (Grafana e Flux)

1. Acesse o Grafana no navegador: `http://localhost:3000` (Usuário/Senha padrão: `admin` / `admin`).
2. Vá em **Connections** -> **Data Sources** e adicione o **InfluxDB**.
3. Na configuração do Data Source:
   * **Query Language:** Mude para `Flux`.
   * **URL:** `http://localhost:8086` (ou o IP local da máquina host).
   * **Auth:** Desative a opção "Basic Auth".
   * **InfluxDB Details:** Preencha a `Organization`, o `Default Bucket` e o `Token` gerado no passo 3.

### Consultas (Queries) para Criação dos Dashboards
No Grafana, adicione novos painéis utilizando a linguagem **Flux**. Use os códigos abaixo na área de query.

**A. Painel tipo "Stat" (Cards de Valor Atual)**
Mostra a leitura em tempo real. Exemplo para **Tensão (V)**:
```flux
from(bucket: "sensor_pzem")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "pzem_data")
  |> filter(fn: (r) => r["_field"] == "tensao")
  |> last()
```
*(Para outros parâmetros, altere `"tensao"` para `"corrente"`, `"potencia"`, `"frequencia"`, ou `"fator_potencia"`, mapeados conforme o JSON enviado pelo ESP8266).*

**B. Painel tipo "Time Series" (Gráfico de Linhas)**
Mostra a flutuação ao longo do tempo:
```flux
from(bucket: "sensor_pzem")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "pzem_data")
  |> filter(fn: (r) => r["_field"] == "tensao")
  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)
  |> yield(name: "mean")
```

**C. Painel "Consumo Acumulado" (Wh)**
Calcula a diferença da energia gasta apenas no dia atual:
```flux
from(bucket: "sensor_pzem")
  |> range(start: today())
  |> filter(fn: (r) => r["_measurement"] == "pzem_data")
  |> filter(fn: (r) => r["_field"] == "energia")
  |> spread()
```



