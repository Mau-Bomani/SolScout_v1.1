
.PHONY: help up down restart logs status health backup restore clean build

# Detect available Docker/Podman Compose command
COMPOSE_CMD := $(shell if command -v docker >/dev/null 2>&1 && docker compose version >/dev/null 2>&1; then echo "docker compose"; \
                     elif command -v docker-compose >/dev/null 2>&1; then echo "docker-compose"; \
                     elif command -v podman-compose >/dev/null 2>&1; then echo "podman-compose"; \
                     else echo ""; fi)

# Detect container engine
ENGINE_CMD := $(shell if command -v docker >/dev/null 2>&1; then echo "docker"; \
                      elif command -v podman >/dev/null 2>&1; then echo "podman"; \
                      else echo ""; fi)

SECRETS_DIR := /home/leland/Desktop/Code/SolScout_v1.1/secrets

help:
	@echo "SoulScout Infrastructure Management"
	@echo "=================================="
	@echo "up          - Start all services"
	@echo "down        - Stop all services"
	@echo "restart     - Restart all services"
	@echo "logs        - Show logs for all services"
	@echo "status      - Show service status"
	@echo "health      - Run health checks"
	@echo "backup      - Create database backup"
	@echo "restore     - Restore from backup (requires BACKUP_FILE=path)"
	@echo "clean       - Remove stopped containers and unused images"
	@echo "build       - Build custom images"

check-docker:
	@if [ -z "$(COMPOSE_CMD)" ]; then \
		echo "❌ Docker Compose or Podman Compose not found. Please install one."; \
		exit 1; \
	fi
	@if [ -z "$(ENGINE_CMD)" ]; then \
		echo "❌ Docker or Podman not found. Please install one."; \
		exit 1; \
	fi
	@echo "Using: $(COMPOSE_CMD) with $(ENGINE_CMD)"

up: check-docker
	@echo "Starting SoulScout infrastructure..."
	@$(COMPOSE_CMD) up -d postgres redis
	@sleep 30
	@$(COMPOSE_CMD) up -d

down: check-docker
	@echo "Stopping SoulScout infrastructure..."
	@$(COMPOSE_CMD) down

restart: check-docker
	@echo "Restarting SoulScout infrastructure..."
	@$(COMPOSE_CMD) restart

logs: check-docker
	@$(COMPOSE_CMD) logs -f

status: check-docker
	@$(COMPOSE_CMD) ps

health:
	@echo "Running health checks..."
	@curl -fsS http://localhost:8080/health >/dev/null 2>&1 && echo "✅ TG Gateway" || echo "❌ TG Gateway"
	@curl -fsS http://localhost:8081/health >/dev/null 2>&1 && echo "✅ Portfolio" || echo "❌ Portfolio"
	@curl -fsS http://localhost:8082/health >/dev/null 2>&1 && echo "✅ Ingestor" || echo "❌ Ingestor"
	@curl -fsS http://localhost:8083/health >/dev/null 2>&1 && echo "✅ Analytics" || echo "❌ Analytics"
	@curl -fsS http://localhost:8084/health >/dev/null 2>&1 && echo "✅ Notifier" || echo "❌ Notifier"

backup: check-docker
	@mkdir -p ops/backups
	@echo "Creating database backup..."
	@$(COMPOSE_CMD) exec -T postgres pg_dump -U $(cat $(SECRETS_DIR)/postgres_user) -d soulsct > ops/backups/backup_$(date +%Y%m%d_%H%M%S).sql
	@echo "Backup created in ops/backups/"

restore: check-docker
	@if [ -z "$(BACKUP_FILE)" ]; then echo "Usage: make restore BACKUP_FILE=path/to/backup.sql"; exit 1; fi
	@echo "Restoring from $(BACKUP_FILE)..."
	@$(COMPOSE_CMD) stop tg_gateway portfolio ingestor analytics notifier
	@cat $(BACKUP_FILE) | $(COMPOSE_CMD) exec -T postgres psql -U $(cat $(SECRETS_DIR)/postgres_user) -d soulsct
	@$(COMPOSE_CMD) up -d

clean: check-docker
	@echo "Cleaning up..."
	@$(ENGINE_CMD) system prune -f
	@$(ENGINE_CMD) volume prune -f

build: check-docker
	@echo "Building custom images..."
	@$(ENGINE_CMD) build -t soulscout/healthcheck:latest -f ops/healthcheck.dockerfile ops/
