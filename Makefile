.PHONY: all clean format build test help

# Default target
all: build

# Build the project
build:
	@echo "Building uvzmq..."
	@if [ ! -d "build" ]; then \
		cmake -B build -DUVZMQ_BUILD_EXAMPLES=ON; \
	fi
	@cmake --build build -j$$(nproc)
	@echo "Build complete!"

# Clean build artifacts
clean:
	@echo "Cleaning build directory..."
	@rm -rf build
	@echo "Clean complete!"

# Format code with clang-format
format:
	@echo "Formatting code with clang-format..."
	@if command -v clang-format > /dev/null 2>&1; then \
		find include examples benchmarks -name "*.c" -o -name "*.h" -o -name "*.cpp" | \
		xargs clang-format -i; \
		echo "Format complete!"; \
	else \
		echo "clang-format not found. Please install clang-format."; \
		exit 1; \
	fi

# Check code formatting
check-format:
	@echo "Checking code formatting..."
	@if command -v clang-format > /dev/null 2>&1; then \
		files=$$(find include examples benchmarks -name "*.c" -o -name "*.h" -o -name "*.cpp"); \
		if ! echo "$$files" | xargs clang-format --dry-run --Werror; then \
			echo "Formatting issues found. Run 'make format' to fix."; \
			exit 1; \
		fi; \
		echo "All files are properly formatted!"; \
	else \
		echo "clang-format not found. Please install clang-format."; \
		exit 1; \
	fi

# Run tests

# Generate documentation with doxygen
docs:
	@echo "Generating documentation..."
	@if command -v doxygen > /dev/null 2>&1; then \
		doxygen Doxyfile; \
		echo "Documentation generated in docs/"; \
	else \
		echo "doxygen not found. Please install doxygen."; \
		exit 1; \
	fi

# Clean documentation
clean-docs:
	@echo "Cleaning documentation..."
	@rm -rf docs html
	@echo "Documentation cleaned!"
test:
	@echo "Running tests..."
	@if [ -d "build" ]; then \
		cd build && ctest --output-on-failure; \
	else \
		echo "Build directory not found. Run 'make build' first."; \
		exit 1; \
	fi


# Show help
help:
	@echo "UVZMQ Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build the project (default)"
	@echo "  build        - Build the project"
	@echo "  clean        - Remove build artifacts"
	@echo "  format       - Format code with clang-format"
	@echo "  check-format - Check code formatting"
	@echo "  test         - Run tests"
	@echo "  docs         - Generate documentation with doxygen"
	@echo "  clean-docs   - Remove documentation"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make build       # Build the project"
	@echo "  make format      # Format all code"
	@echo "  make test        # Run tests"
	@echo "  make docs        # Generate documentation"
	@echo "  make clean-docs   # Clean documentation"
	@echo "  make clean build # Clean and rebuild"
