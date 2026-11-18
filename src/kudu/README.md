# Kudu

Raft consensus library ("kuduraft") derived from the Apache Kudu codebase. It holds the Raft consensus implementation in consensus/, the RPC layer in rpc/ and thrift/, server components in server/, kserver/ and tserver/, on-disk storage in fs/, and shared utilities in common/, gutil/ and util/, plus integration tests in integration-tests/ and developer tooling in tools/ and scripts/.

It is built and tested alongside //mysql_raft/... and //dbproxy/.... The files in this top-level folder are owned by the mysql_rap oncall (see BUCK).
