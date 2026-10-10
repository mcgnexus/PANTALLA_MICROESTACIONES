import test from 'node:test';
import assert from 'node:assert/strict';
import { createDiagnosticsHandler } from './diagnostics.mjs';

function response() {
  return { headers: {}, setHeader(k, v) { this.headers[k] = v; },
    status(code) { this.code = code; return this; }, json(body) { this.body = body; return this; } };
}
const checked_at = '2026-10-10T17:00:00Z';
async function call(rows) {
  const previous = process.env.DATABASE_URL;
  process.env.DATABASE_URL = 'postgresql://test';
  const res = response();
  try { await createDiagnosticsHandler(() => async () => rows)({ method: 'GET' }, res); }
  finally {
    if (previous == null) delete process.env.DATABASE_URL;
    else process.env.DATABASE_URL = previous;
  }
  return res;
}
test('BD accesible no significa que la estación esté transmitiendo', async () => {
  const res = await call([{ checked_at, id: 's1', name: 'Urbana', sync_interval_s: 900,
    received_at: '2026-10-10T16:00:00Z', observed_at: '2026-10-10T15:59:00Z',
    last_contact: '2026-10-10T16:00:00Z', sequence: 100 }]);
  assert.equal(res.body.database, 'ok');
  assert.equal(res.body.stations[0].receptionState, 'delayed');
  assert.equal(res.body.stations[0].receptionAgeSeconds, 3600);
  assert.equal(res.headers['Vercel-CDN-Cache-Control'], 'no-store');
});
test('distingue recepción reciente, estación sin datos y catálogo vacío', async () => {
  const recent = await call([{ checked_at, id: 's1', sync_interval_s: 900,
    received_at: '2026-10-10T16:59:00Z' }]);
  assert.equal(recent.body.stations[0].receptionState, 'recent');
  const never = await call([{ checked_at, id: 's1', sync_interval_s: 900 }]);
  assert.equal(never.body.stations[0].receptionState, 'never');
  assert.equal(never.body.stations[0].receptionAgeSeconds, null);
  const empty = await call([{ checked_at, id: null }]);
  assert.deepEqual(empty.body.stations, []);
  assert.equal(empty.body.database, 'ok');
});
test('rechaza escritura antes de consultar la base', async () => {
  const res = response();
  await createDiagnosticsHandler(() => { throw new Error('no query'); })({ method: 'POST' }, res);
  assert.equal(res.code, 405);
});
test('fallo de BD devuelve 503 sin filtrar el mensaje del proveedor', async () => {
  const previous = process.env.DATABASE_URL;
  process.env.DATABASE_URL = 'postgresql://test';
  const res = response();
  try {
    await createDiagnosticsHandler(() => async () => { throw new Error('secret connection info'); })({ method: 'GET' }, res);
    assert.equal(res.code, 503);
    assert.equal(res.body.database, 'unavailable');
    assert.doesNotMatch(JSON.stringify(res.body), /secret/);
  } finally {
    if (previous == null) delete process.env.DATABASE_URL;
    else process.env.DATABASE_URL = previous;
  }
});
