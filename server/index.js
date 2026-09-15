const http = require('http');

const host = process.env.CURSORE_HOST || '0.0.0.0';
const port = Number(process.env.CURSORE_PORT || 8766);

const server = http.createServer((req, res) => {
  if (req.method === 'GET' && req.url === '/health') {
    const body = JSON.stringify({ status: 'ok', service: 'cursore-server' });
    res.writeHead(200, { 'content-type': 'application/json; charset=utf-8' });
    res.end(body);
    return;
  }
  res.writeHead(404, { 'content-type': 'application/json; charset=utf-8' });
  res.end(JSON.stringify({ status: 'not_found' }));
});

server.listen(port, host, () => {
  console.log(`cursore-server listening on ${host}:${port}`);
});
