
# Portfolio Service (SoulScout v1.1)

This service is responsible for tracking Solana wallets, calculating portfolio value, and persisting snapshots. It listens for commands on a Redis stream and publishes replies.

**Host OS Note:** This service is designed to be built and run inside Docker. The developer's host OS is Bazzite Linux (Fedora-based), and no `apt` commands are needed on the host. All dependencies are handled within the Docker image.

## Responsibilities

-   Handles commands: `/balance`, `/holdings`, `/add_wallet`, `/remove_wallet`.
-   Calculates portfolio value in USD based on the v1.1 valuation policy.
-   Persists portfolio snapshots and holding details to a PostgreSQL database.
-   Publishes replies to the `tg_gateway` via a Redis stream.
-   Provides a `GET /health` endpoint for container health checks.
-   **Does not** handle private keys or sign transactions.

## Valuation Policy

Portfolio valuation follows a strict priority list:
1.  **CoinGecko**: If a token is listed, its official USD price is used (tag: `CG`).
2.  **DEX Liquidity**: If not on CoinGecko but has ≥ $75k liquidity on a major DEX (via Jupiter), the pool mid-price is used (tag: `DEX`).
3.  **Estimated Value**: If liquidity is between $25k and $75k, the value is estimated with a 50% haircut (tag: `EST_50`).
4.  **Not Priced**: Tokens with < $25k liquidity are considered illiquid, valued at N/A, and excluded from the total (tag: `NA`).

A note is always included in replies about how many tokens were excluded.

## Environment Variables

| Variable                       | Description                                               | Default Value                |
| ------------------------------ | --------------------------------------------------------- | ---------------------------- |
| `OWNER_TELEGRAM_ID`            | Telegram ID of the system owner.                          | (required)                   |
| `REDIS_URL`                    | Connection string for Redis.                              | `redis://redis:6379`         |
| `STREAM_REQ`                   | Redis stream for incoming command requests.               | `soul.cmd.requests`          |
| `STREAM_REP`                   | Redis stream for outgoing command replies.                | `soul.cmd.replies`           |
| `STREAM_AUDIT`                 | Redis stream for audit events.                            | `soul.audit`                 |
| `PG_DSN`                       | PostgreSQL connection string.                             | (required)                   |
| `RPC_URLS`                     | Comma-separated list of public Solana RPC endpoints.      | (required)                   |
| `COINGECKO_BASE`               | Base URL for CoinGecko API.                               | `https://api.coingecko.com/api/v3` |
| `JUPITER_BASE`                 | Base URL for Jupiter Quote API.                           | `https://quote-api.jup.ag`   |
| `DUST_MIN_USD`                 | Minimum USD value to be considered non-dust.              | `0.50`                       |
| `HAIRCUT_LOW_LIQ_PCT`          | Haircut percentage for low-liquidity assets.              | `50`                         |
| `REQUEST_TIMEOUT_MS`           | Timeout for external API requests.                        | `8000`                       |
| `LISTEN_ADDR`                  | IP address for the health server to listen on.            | `0.0.0.0`                    |
| `LISTEN_PORT`                  | Port for the health server.                               | `8081`                       |
| `SERVICE_NAME`                 | Name of the service for logging.                          | `portfolio`                  |
| `LOG_LEVEL`                    | Logging level (`trace`, `debug`, `info`, `warn`, `error`).| `info`                       |

## Local Usage

### Build the Container

From the project root (`SolScout_v1.1/`):
```bash
docker build -t soulscout/portfolio-service:latest -f portfolio/Dockerfile .
```

### 3. Core Source Files

These files set up the application's entry point, configuration, and utilities.

```cpp
// File: /home/leland/Desktop/Code/SolScout_v1.1/portfolio/src/config.hpp
#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct Config {
    int64_t owner_telegram_id;
    std::string redis_url;
    std::string stream_req;
    std::string stream_rep;
    std::string stream_audit;
    std::string pg_dsn;
    std::vector<std::string> rpc_urls;
    std::string coingecko_base;
    std::string jupiter_base;
    double dust_min_usd;
