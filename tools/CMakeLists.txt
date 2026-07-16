# Command-line tools built on the libraries.

add_executable(mime_corpus mime_corpus.cpp)
target_link_libraries(mime_corpus PRIVATE libglot::mime)
