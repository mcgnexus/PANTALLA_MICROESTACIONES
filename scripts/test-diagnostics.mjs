// Ejecutar con DATABASE_URL definido. Solo lectura; no imprime credenciales.
import handler from '../api/diagnostics.mjs';
const res = {
  setHeader() {},
  status(code) { this.code = code; return this; },
  json(body) {
    console.log(JSON.stringify({ http: this.code, ...body }, null, 2));
    if (this.code !== 200) process.exitCode = 1;
  },
};
await handler({ method: 'GET' }, res);
