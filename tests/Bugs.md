
- **PostgreSQL 12.9**

    ```console
    2022-05-09 08:54:32.424 WEST [1909488] FATAL:  invalid attribute number 2 for pg_authid_rolname_index
    ```

    - Related resources:
      - https://postgrespro.com/list/thread-id/1488953
      - https://blog.dbi-services.com/dealing-with-corrupted-system-indexes-in-postgresql/ 
      - files causing exception: pgdata/base/(13462|13463|1)/2679
