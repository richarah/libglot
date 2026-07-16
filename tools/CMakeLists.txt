# Command-line tools built on the libraries.

add_executable(mime_corpus mime_corpus.cpp)
target_link_libraries(mime_corpus PRIVATE libglot::mime)

add_executable(mime_dump mime_dump.cpp)
target_link_libraries(mime_dump PRIVATE libglot::mime)
