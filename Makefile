
SOURCES = main.cpp stack.cpp need.cpp wind.cpp pprint.cpp
HEADERS = ast.hpp stack.hpp
main: $(SOURCES) $(HEADERS)
	g++ --std c++17 -o main $(SOURCES)
