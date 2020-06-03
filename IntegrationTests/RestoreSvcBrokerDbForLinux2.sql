USE[SvcBrokerTest];
IF NOT EXISTS (SELECT * FROM sys.sql_logins WHERE NAME = N'tester')
BEGIN
    CREATE LOGIN [tester]
		WITH PASSWORD=N'tester',
			 DEFAULT_DATABASE=[SvcBrokerTest],
			 CHECK_EXPIRATION=OFF,
			 CHECK_POLICY=OFF;
END;

IF NOT EXISTS (SELECT * FROM sys.sysusers WHERE NAME = N'tester')
BEGIN
    CREATE USER [tester] FOR LOGIN [tester];
END;

ALTER ROLE [db_datareader] ADD MEMBER [tester];
ALTER ROLE [db_datawriter] ADD MEMBER [tester];
ALTER ROLE [db_ddladmin] ADD MEMBER [tester];
ALTER ROLE [db_owner] ADD MEMBER [tester];
