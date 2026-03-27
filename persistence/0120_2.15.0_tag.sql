BEGIN TRANSACTION;

alter TABLE tags add column unit TEXT NULL check(length(unit) <= 128);

COMMIT;