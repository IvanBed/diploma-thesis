CREATE FUNCTION check_password() RETURNS integer
     AS '/var/lib/postgresql/password-validation/password-validation', 'check_password'
     LANGUAGE C STRICT;