// GET /api/health
export default function handler(_req, res) {
  res.status(200).json({ status: "ok", service: "pantalla-microestaciones-api" });
}