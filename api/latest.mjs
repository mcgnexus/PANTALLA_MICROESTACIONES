// GET /api/latest
// Última medición de cada miniestación publicada (mediciones validadas y no
// borradas). Punto final ligero y sin AEMET, pensado para que una pantalla
// ESP32 con OLED lea los datos cada minuto.
import { neon } from "@neondatabase/serverless";

const round = (value, digits) =>
  value == null ? null : Number(Number(value).toFixed(digits));

export default async function handler(_req, res) {
  const dbUrl = process.env.DATABASE_URL;
  if (!dbUrl) {
    res.status(500).json({ error: "DATABASE_URL no configurada" });
    return;
  }

  try {
    const sql = neon(dbUrl, { fetchOptions: { cache: "no-store" } });

    const rows = await sql`
      SELECT DISTINCT ON (d.id)
        d.id,
        d.name,
        d.public_zone,
        m.observed_at,
        m.temperature_c,
        m.humidity_pct,
        m.pressure_pa,
        m.battery_mv,
        m.lux,
        m.sequence,
        ds.connectivity,
        (extract(epoch FROM (now() - m.observed_at)))::int AS freshness_seconds
      FROM devices d
      JOIN measurements m ON m.device_id = d.id
      LEFT JOIN device_status ds ON ds.device_id = d.id
      WHERE d.active = true
        AND d.publish_permission = true
        AND m.is_validated = true
        AND m.deleted_at IS NULL
      ORDER BY d.id, m.observed_at DESC
    `;

    const stations = rows.map((r) => ({
      id: r.id,
      name: r.name,
      zone: r.public_zone ?? null,
      connectivity: r.connectivity ?? null,
      temperatureC: round(r.temperature_c, 1),
      humidityPct: round(r.humidity_pct, 0),
      pressureHpa: r.pressure_pa == null ? null : round(r.pressure_pa / 100, 1),
      batteryMv: r.battery_mv == null ? null : Number(r.battery_mv),
      lux: r.lux == null ? null : round(r.lux, 0),
      sequence: r.sequence == null ? null : Number(r.sequence),
      observedAt: r.observed_at ? new Date(r.observed_at).toISOString() : null,
      freshnessSeconds:
        r.freshness_seconds == null ? null : Math.max(0, Number(r.freshness_seconds)),
    }));

    res.setHeader("Cache-Control", "public, max-age=20, s-maxage=20");
    res.status(200).json({
      updatedAt: new Date().toISOString(),
      stationCount: stations.length,
      stations,
    });
  } catch (error) {
    res.status(500).json({ error: error instanceof Error ? error.message : "Error interno" });
  }
}