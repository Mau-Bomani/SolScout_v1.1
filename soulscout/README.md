# SoulScout Infrastructure

Production-ready Docker Compose orchestration for SoulScout trading system.

## Quick Start

1. Ensure secrets exist in `/home/leland/Desktop/Code/SolScout_v1.1/secrets/`
2. Copy `.env` and configure as needed
3. Run: `make up`
4. Verify: `make health`

## Required Secrets

- `postgres_user` - Database username
- `postgres_password` - Database password  
- `redis_password` - Redis password
- `tg_bot_token` - Telegram bot token
- `owner_telegram_id` - Owner Telegram ID
- `coingecko_api_key` - CoinGecko API key

## Services

- **postgres** (5432) - PostgreSQL database
- **redis** (6379) - Redis cache/messaging
- **tg_gateway** (8080) - Telegram gateway
- **portfolio** (8081) - Portfolio management
- **ingestor** (8082) - Market data ingestion
- **analytics** (8083) - Signal analysis
- **notifier** (8084) - Alert notifications

## Operations

```bash
make up          # Start all services
make down        # Stop all services  
make health      # Check service health
make backup      # Backup database
make logs        # View logs
