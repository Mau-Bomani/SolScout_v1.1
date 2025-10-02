
CREATE OR REPLACE FUNCTION cleanup_old_market_data() RETURNS void AS $
BEGIN
    DELETE FROM market_snapshots WHERE created_at < NOW() - INTERVAL '30 days';
    DELETE FROM ohlcv_data WHERE created_at < NOW() - INTERVAL '90 days' AND interval_minutes = 1;
    DELETE FROM ohlcv_data WHERE created_at < NOW() - INTERVAL '1 year' AND interval_minutes = 5;
    DELETE FROM alert_history WHERE created_at < NOW() - INTERVAL '6 months';
    DELETE FROM audit_logs WHERE created_at < NOW() - INTERVAL '3 months';
    RAISE NOTICE 'Cleanup completed at %', NOW();
END;
$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION vacuum_analyze_tables() RETURNS void AS $
DECLARE
    table_name text;
BEGIN
    FOR table_name IN 
        SELECT tablename FROM pg_tables WHERE schemaname = 'public'
    LOOP
        EXECUTE 'VACUUM ANALYZE ' || quote_ident(table_name);
    END LOOP;
    RAISE NOTICE 'Vacuum analyze completed at %', NOW();
END;
$ LANGUAGE plpgsql;
