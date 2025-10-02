
#!/bin/bash
set -e

echo "Building Docker image..."
docker build -t soulscout-tg-gateway .

echo "Starting Redis container..."
docker run -d --name test-redis -p 6379:6379 redis:7-alpine

echo "Waiting for Redis to be ready..."
sleep 3

echo "Starting TG Gateway in poll mode..."
docker run --rm --name test-gateway \
  --link test-redis:redis \
  -e TG_BOT_TOKEN="dummy_token_for_testing" \
  -e OWNER_TELEGRAM_ID="123456789" \
  -e REDIS_URL="redis://redis:6379" \
  -e GATEWAY_MODE="poll" \
  -e LOG_LEVEL="debug" \
  -p 8080:8080 \
  soulscout-tg-gateway &

GATEWAY_PID=$!

echo "Waiting for gateway to start..."
sleep 5

echo "Testing health endpoint..."
curl -f http://localhost:8080/health || echo "Health check failed"

echo "Cleaning up..."
kill $GATEWAY_PID 2>/dev/null || true
docker stop test-redis
docker rm test-redis

echo "Docker test completed!"
