# Example programs. Built by default (they double as living documentation
# and compile checks for the public API).

add_executable(sql_transpile sql_transpile.cpp)
target_link_libraries(sql_transpile PRIVATE libglot::sql)

add_executable(mime_inspect mime_inspect.cpp)
target_link_libraries(mime_inspect PRIVATE libglot::mime)
