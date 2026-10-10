// Consulta real, sin caché y de solo lectura. Solo estaciones publicadas.
import { neon } from '@neondatabase/serverless';

const age = (now, value) => value == null ? null
  : Math.max(0, Math.floor((new Date(now) - new Date(value)) / 1000));
const iso = (value) => value == null ? null : new Date(value).toISOString();

export function createDiagnosticsHandler(query = (url) => neon(url, {
  fetchOptions: { cache: 'no-store' },
})) {
  return async function handler(req, res) {
    res.setHeader('Cache-Control', 'no-store');
    res.setHeader('CDN-Cache-Control', 'no-store');
    res.setHeader('Vercel-CDN-Cache-Control', 'no-store');
    if (req.method !== 'GET') {
      res.setHeader('Allow', 'GET');
      return res.status(405).json({ error: 'method_not_allowed' });
    }
    if (!process.env.DATABASE_URL) {
      return res.status(503).json({ database: 'unavailable', error: 'database_not_configured' });
    }
    try {
      const sql = query(process.env.DATABASE_URL);
      // LEFT JOIN conserva incluso una estación que todavía no ha enviado nada.
      const rows = await sql`
        SELECT now() AS checked_at, d.id, d.name, ds.last_contact,
          coalesce((c.config->>'sync_interval_s')::integer, 900) AS sync_interval_s,
          latest.sequence, latest.observed_at,
          received.received_at
        FROM (SELECT 1) clock
        LEFT JOIN devices d ON d.active = true AND d.publish_permission = true
        LEFT JOIN device_status ds ON ds.device_id = d.id
        LEFT JOIN device_configs c ON c.device_id = d.id
        LEFT JOIN LATERAL (
          SELECT sequence, observed_at FROM measurements
          WHERE device_id = d.id AND is_validated = true AND deleted_at IS NULL
          ORDER BY observed_at DESC LIMIT 1
        ) latest ON true
        LEFT JOIN LATERAL (
          SELECT received_at FROM measurements WHERE device_id = d.id
          ORDER BY received_at DESC LIMIT 1
        ) received ON true
        ORDER BY d.id
      `;
      const checkedAt = iso(rows[0].checked_at);
      return res.status(200).json({
        database: 'ok', checkedAt,
        stations: rows.filter((row) => row.id != null).map((row) => {
          const receptionAgeSeconds = age(checkedAt, row.received_at);
          const syncIntervalSeconds = Number(row.sync_interval_s);
          return {
            id: row.id, name: row.name,
            sequence: row.sequence == null ? null : Number(row.sequence),
            observedAt: iso(row.observed_at), receivedAt: iso(row.received_at),
            lastContact: iso(row.last_contact),
            measurementAgeSeconds: age(checkedAt, row.observed_at),
            receptionAgeSeconds, contactAgeSeconds: age(checkedAt, row.last_contact),
            syncIntervalSeconds,
            receptionState: receptionAgeSeconds == null ? 'never'
              : receptionAgeSeconds > syncIntervalSeconds * 2 + 60 ? 'delayed' : 'recent',
          };
        }),
      });
    } catch (error) {
      console.error('diagnostics: database query failed');
      return res.status(503).json({ database: 'unavailable', error: 'database_query_failed' });
    }
  };
}

export default createDiagnosticsHandler();
