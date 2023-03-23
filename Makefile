
SOURCES = main.cpp stack.cpp need.cpp unwind.cpp type.cpp pprint.cpp
HEADERS = ast.hpp error.hpp stack.hpp unwind.hpp
main: $(SOURCES) $(HEADERS)
	g++ -O0 -g --std c++17 -o main $(SOURCES)
