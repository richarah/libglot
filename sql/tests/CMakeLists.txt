# libglot-sql test suite. Each source file is one Catch2 executable;
# individual TEST_CASEs are registered with CTest via catch_discover_tests.

set(SQL_TESTS
    test_advanced_sql
    test_all_dialects_comprehensive
    test_arena
    test_assignments
    test_begin_end_blocks
    test_bugfix_regressions
    test_connect_by
    test_connect_by_lowering
    test_create_procedure
    test_cte_windows_subqueries
    test_cursors
    test_date_trunc
    test_declare_keyword
    test_delimiter
    test_dialect_coverage
    test_dialect_feature_combinations
    test_dialect_transpilation
    test_distinct_on
    test_dml_statements
    test_end_to_end
    test_error_messages
    test_error_recovery
    test_exceptions
    test_extract
    test_fk_check_constraints
    test_flatten
    test_for_keyword
    test_for_loop
    test_for_while_transpile
    test_fulltext_match
    test_grant_revoke
    test_group_by_extensions
    test_if_statement
    test_intern
    test_interval_literals
    test_join_using_natural
    test_json_operations
    test_lateral_joins
    test_loop_break_continue
    test_mad_queries
    test_merge_extended
    test_named_windows
    test_optimizer
    test_order_by_nulls
    test_output_clause
    test_parser
    test_procedure_dialects
    test_qualify
    test_raise
    test_recursive_cte
    test_return_keyword
    test_returns_comprehensive
    test_roundtrip
    test_roundtrip_property
    test_scalar_functions
    test_schema_type
    test_security
    test_sequences
    test_stored_procedures
    test_struct_array_subscript
    test_table_options
    test_tablesample
    test_temporal_tables
    test_temporary_tables
    test_tokenizer
    test_transpiler
    test_unbounded_following
    test_untested_dialects
    test_upsert
    test_utility_statements
    test_values_table_source
    test_while_loop
)

foreach(test_name IN LISTS SQL_TESTS)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE libglot::sql Catch2::Catch2WithMain)
    catch_discover_tests(${test_name} TEST_PREFIX "sql.")
endforeach()
