# PANTALLA MICROESTACIONES

Pantalla ESP32 + OLED 0,96" SSD1306 (128x64, I2C) que muestra en vivo la
última medición de las microestaciones TecRural (temperatura, humedad y
presión). Incluye una API serverless mínima en Vercel que lee de Neon la
última medición de cada estación con permiso de publicación.

```
ESP32 + OLED ──HTTPS GET──► /api/latest (Vercel) ──► Neon (measurements)
```

## API

`GET /api/latest` devuelve la última medición validada por estación publicada:

```json
{
  "updatedAt": "2026-10-10T12:28:16.000Z",
  "stationCount": 1,
  "stations": [
    {
      "id": "esp32c3-01",
      "name": "TECRURAL REYES 113",
      "zone": "Huéscar, Granada",
      "connectivity": "online",
      "temperatureC": 22.6,
      "humidityPct": 39,
      "pressureHpa": 912.5,
      "batteryMv": null,
      "lux": null,
      "sequence": 1537,
      "observedAt": "2026-10-10T12:28:16.000Z",
      "freshnessSeconds": 936
    }
  ]
}
```

- `pressureHpa` se calcula a partir de `pressure_pa / 100`.
- `freshnessSeconds` es la antigüedad en segundos de la muestra; el ESP32 la
  muestra como "hace N min" sin necesidad de NTP.
- Solo lectura: no expone coordenadas ni credenciales.

### Despliegue

1. Desplegar en Vercel como proyecto `tecrural-ms-pantalla`.
2. Fijar la variable de entorno `DATABASE_URL` (Neon) en **Production**.
3. `npx vercel --prod` desde la raíz del repo.

### Prueba local (solo lectura)

```bash
DATABASE_URL="postgresql://..." node scripts/test-latest.mjs
```

## Firmware

Carpeta `firmware/esp32_oled_meteo/`. Placa objetivo: ESP32 DevKit clásico
(pista `esp32:esp32:esp32`).

### Librerías

```bash
arduino-cli lib install "Adafruit SSD1306" "ArduinoJson"
```

### Configuración

```bash
cp firmware/esp32_oled_meteo/config.h.example firmware/esp32_oled_meteo/config.h
# editar WIFI_SSID, WIFI_PASSWORD y (si hace falta) API_URL
```

### Cableado (OLED 0,96" I2C -> ESP32 DevKit)

| OLED | ESP32 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

Dirección I2C: `0x3C` (la habitual en SSD1306).

### Subir

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/esp32_oled_meteo
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 firmware/esp32_oled_meteo
```

La pantalla refresca cada 60 s (`REFRESH_MS`), muestra el nombre de la
estación, temperatura en grande, humedad, presión y la antigüedad de la
lectura con su número de secuencia.