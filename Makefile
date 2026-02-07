.PHONY: all clean format format-code format-docs check-format build test e2e-test coverage coverage-report help

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

# Format all files
format: format-code format-docs
	@echo "All files formatted!"

# Format code files only
format-code:
	@echo "Formatting code with clang-format..."
	@if command -v clang-format > /dev/null 2>&1; then \
		find include examples benchmarks -name "*.c" -o -name "*.h" -o -name "*.cpp" | \
		xargs clang-format -i; \
		echo "Code format complete!"; \
	else \
		echo "clang-format not found. Please install clang-format."; \
		exit 1; \
	fi

# Format markdown documents
format-docs:
	@echo "Formatting markdown documents..."
	@if command -v prettier > /dev/null 2>&1; then \
		prettier --write "*.md"; \
		echo "Markdown format complete!"; \
	else \
		echo "prettier not found. Please install prettier: npm install -g prettier"; \
		exit 1; \
	fi

# Check code formatting
check-format:
	@echo "Checking code formatting..."
	@if command -v clang-format > /dev/null 2>&1; then \
		files=$$(find include examples benchmarks -name "*.c" -o -name "*.h" -o -name "*.cpp"); \
		if ! echo "$$files" | xargs clang-format --dry-run --Werror; then \
			echo "Code formatting issues found. Run 'make format' to fix."; \
			exit 1; \
		fi; \
		echo "All code files are properly formatted!"; \
	else \
		echo "clang-format not found. Please install clang-format."; \
		exit 1; \
	fi

# Check markdown formatting
check-docs-format:
	@echo "Checking markdown formatting..."
	@if command -v prettier > /dev/null 2>&1; then \
		if ! prettier --check "*.md"; then \
			echo "Markdown formatting issues found. Run 'make format-docs' to fix."; \
			exit 1; \
		fi; \
		echo "All markdown files are properly formatted!"; \
	else \
		echo "prettier not found. Please install prettier: npm install -g prettier"; \
		exit 1; \
	fi

# Run tests
test:
	@echo "Running tests..."
	@if [ -d "build" ]; then \
		cd build && ctest --output-on-failure; \
	else \
		echo "Build directory not found. Run 'make build' first."; \
		exit 1; \
	fi

# Run tests with ASAN
test-asan:
	@echo "Running tests with AddressSanitizer..."
	@if [ -d "build_asan" ]; then \
		rm -rf build_asan; \
	fi
	@mkdir build_asan && cd build_asan && cmake -DUVZMQ_ENABLE_ASAN=ON .. && make -j$$(nproc) && ctest --output-on-failure

# Run tests with TSAN
test-tsan:
	@echo "Running tests with ThreadSanitizer..."
	@if [ -d "build_tsan" ]; then \
		rm -rf build_tsan; \
	fi
	@mkdir build_tsan && cd build_tsan && cmake -DUVZMQ_ENABLE_TSAN=ON .. && make -j$$(nproc) && ctest --output-on-failure

# Run end-to-end tests
e2e-test:
	@echo "Running end-to-end tests..."
	@if [ -f "tests/e2e_test.sh" ]; then \
		chmod +x tests/e2e_test.sh; \
		./tests/e2e_test.sh; \
	else \
		echo "E2E test script not found."; \
		exit 1; \
	fi

# Run tests with coverage
coverage:
	@echo "Running tests with coverage..."
	@if [ -d "build" ]; then \
		cd build && ctest --output-on-failure; \
	else \
		echo "Build directory not found. Run 'make build' first."; \
		exit 1; \
	fi

# Generate coverage report
coverage-report:
	@echo "Generating coverage report..."
	@if [ ! -d "build" ]; then \
		echo "Build directory not found. Run 'make build first."; \
		exit 1; \
	fi
	@if command -v lcov > /dev/null 2>&1; then \
		lcov --capture --directory build --output-file coverage.info; \
		lcov --remove coverage.info '/usr/*' '*/third_party/*' '*/tests/*' --output-file coverage.info; \
		lcov --list coverage.info; \
		genhtml coverage.info --output-directory coverage_report; \
		echo "Coverage report generated in coverage_report/index.html"; \
	else \
		echo "lcov not found. Install with: sudo apt-get install lcov"; \
		exit 1; \
	fi

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

# Show help
help:
	@echo "UVZMQ Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all              - Build the project (default)"
	@echo "  build            - Build the project"
	@echo "  clean            - Remove build artifacts"
	@echo "  format           - Format all files (code and docs)"
	@echo "  format-code      - Format code files only"
	@echo "  format-docs      - Format markdown documents"
	@echo "  check-format     - Check code formatting"
	@echo "  check-docs-format- Check markdown formatting"
	@echo "  test             - Run tests"
	@echo "  test-asan        - Run tests with AddressSanitizer"
	@echo "  test-tsan        - Run tests with ThreadSanitizer"
	@echo "  e2e-test         - Run end-to-end tests"
	@echo "  coverage         - Run tests with coverage"
	@echo "  coverage-report  - Generate coverage report"
	@echo "  docs             - Generate documentation with doxygen"
	@echo "  clean-docs       - Remove documentation"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make build            # Build the project"
	@echo "  make format           # Format all files"
	@echo "  make format-code      # Format code files only"
	@echo "  make format-docs      # Format markdown docs only"
	@echo "  make check-format     # Check code formatting"
	@echo "  make test            # Run tests"
	@echo "  make test-asan       # Run tests with AddressSanitizer"
	@echo "  make test-tsan       # Run tests with ThreadSanitizer"
	@echo "  make e2e-test        # Run end-to-end tests"
	@echo "  make coverage-report # Generate coverage report"
	@echo "  make docs            # Generate documentation"
	@echo "  make clean-docs      # Clean documentation"
	@echo "  make clean build     # Clean and rebuild"
