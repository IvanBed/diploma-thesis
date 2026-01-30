
CREATE TABLE IF NOT EXISTS password_store (id SERIAL PRIMARY KEY, password TEXT);

do $$
declare
    passwords_count integer; 

begin

    SELECT count(*) 
	INTO passwords_count
	FROM password_store;

	IF passwords_count <> 10000 THEN
		TRUNCATE password_store;
		COPY password_store(password) FROM '/var/lib/postgresql/password-validation/10k-most-common.txt';
        END IF;
end; $$;
