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

## Diagnóstico de comunicación

`GET /api/diagnostics` ejecuta una consulta real a Neon y devuelve
`database: "ok"`, `checkedAt` (hora de PostgreSQL) y, por estación pública:
`receivedAt`, `observedAt`, `lastContact`, `sequence`, sus antigüedades y
`receptionState`. La respuesta lleva `no-store` para navegador y CDN.
Solo admite GET y no escribe en ninguna tabla ni modifica el contacto del sensor.
Incluso sin estaciones o sin medidas puede confirmar que la base es accesible.

La OLED alterna cada cinco segundos entre la lectura y el diagnóstico:

- `Pant API 200 BD OK`: la pantalla pudo consultar la API de pantalla y Neon.
- `Web/BD HTTP 200`: la pantalla alcanzó `/health` de la app principal, que
  ejecuta `SELECT 1`. No equivale a comprobar una subida de medidas.
- `Recibido N min`: antigüedad de la última fila recibida del sensor, incluyendo
  filas inválidas; `Medida N min` usa la última medida válida y no borrada.
- `Recepcion retrasada`: supera dos intervalos de sincronización más 60 segundos.
  No se reutiliza el campo almacenado `connectivity` como prueba de conexión.
- Códigos HTTP negativos indican fallo de transporte en el ESP32. Los códigos
  4xx/5xx corresponden a respuestas del servidor; el puerto serie los registra.

Configura `STATION_ID`, `DIAGNOSTICS_URL` y `MAIN_HEALTH_URL` en `config.h`.
Primero despliega la API nueva; después compila y carga el firmware OLED.
La estación medidora no necesita cambios ni debe compartir su token con la OLED.
Que la OLED comunique no demuestra que el sensor esté enviando; si comparten
hotspot, permite contrastar el acceso HTTPS desde esa red.

Verificación de API: `npm ci` y `npm test`.

## Instalación del firmware

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
