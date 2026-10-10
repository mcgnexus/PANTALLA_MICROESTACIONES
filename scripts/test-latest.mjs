// Prueba local (solo lectura) del handler de /api/latest contra la BD real.
// Uso: DATABASE_URL="postgresql://..." node scripts/test-latest.mjs
import { createServer } from "node:http";
import { once } from "node:events";
import handler from "../api/latest.mjs";

function mockRes() {
  const state = { statusCode: 200, headers: {}, body: null };
  return {
    state,
    status(code) { state.statusCode = code; return this; },
    setHeader(k, v) { state.headers[k] = v; return this; },
    json(body) { state.body = body; return this; },
  };
}

const res = mockRes();
await handler({}, res);
console.log("HTTP", res.state.statusCode);
console.log(JSON.stringify(res.state.body, null, 2));
process.exit(res.state.statusCode === 200 ? 0 : 1);