
FROM alpine:3.18
RUN apk add --no-cache curl postgresql-client redis
COPY healthcheck.sh /healthcheck.sh
RUN chmod +x /healthcheck.sh
ENTRYPOINT ["/healthcheck.sh"]
