# Example programs. Built by default (they double as living documentation
# and compile checks for the public API). Each example is guarded by the
# module it needs, so configuring with a module OFF stays buildable.

if(LIBGLOT_BUILD_SQL)
    add_executable(sql_transpile sql_transpile.cpp)
    target_link_libraries(sql_transpile PRIVATE libglot::sql)
endif()

if(LIBGLOT_BUILD_MIME)
    add_executable(mime_inspect mime_inspect.cpp)
    target_link_libraries(mime_inspect PRIVATE libglot::mime)
endif()
