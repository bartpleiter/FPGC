venv:
	uv sync

mypy:
	uv run mypy BuildTools/ASMPY/asmpy
ruff-lint:
	uv run ruff check
ruff-format:
	uv run ruff format
pytest:
	uv run pytest --cov


lint: ruff-lint mypy
format: ruff-format
test: pytest


check: ruff-format ruff-lint mypy pytest


install:
	uv pip install -e .

uninstall:
	uv pip uninstall fpgc


clean:
	-uv pip uninstall fpgc -y
	-rm -rf asmpy.egg-info
	-rm -rf build
	-rm -rf .venv
	-rm -rf .mypy_cache
	-rm -rf .pytest_cache
	-rm -rf .ruff_cache
	-rm -rf .coverage
	-find . -type d -name __pycache__ -exec rm -r {} \+ 2>/dev/null; true


dev-setup: venv install
	@echo "Development environment is ready!"
	@echo "You can now use:"
	@echo "  uv run asmpy <file> <output>  - Run asmpy with any changes applied"
	@echo "  source .venv/bin/activate     - Activate venv to run asmpy directly"

docs-serve:
	./Scripts/Docs/run.sh

docs-deploy:
	./Scripts/Docs/deploy.sh

sim-cpu:
	./Scripts/Simulation/simulate_cpu.sh

sim-gpu:
	./Scripts/Simulation/simulate_gpu.sh

test-cpu:
	./Scripts/Tests/run_cpu_tests.sh

compile-asm:
	./Scripts/ASM/compile_bare_metal_asm.sh $(file)

flash-asm-uart:
	./Scripts/Programmer/UART/flash_uart.sh

run-asm-uart: compile-asm flash-asm-uart

help:
	@echo "venv                - Create or sync virtual environment with dependencies"
	@echo "install             - Install fpgc tools (ASMPY)"
	@echo "uninstall           - Uninstall fpgc tools"
	@echo "dev-setup           - Install complete development environment setup"
	@echo "lint                - Run ruff lint and mypy"
	@echo "format              - Run ruff format"
	@echo "test                - Run pytest with coverage"
	@echo "check               - Run ruff, mypy and pytest"
	@echo "clean               - Clean up files and remove venv"
	@echo "docs-serve          - Run the documentation website locally"
	@echo "docs-deploy         - Deploy the documentation website"
	@echo "sim-cpu             - Run CPU simulation"
	@echo "sim-gpu             - Run GPU simulation"
	@echo "test-cpu            - Run CPU tests"
	@echo "compile-asm         - Compile ASM file from Software/BareMetalASM/Programs"
	@echo "                      Usage: make compile-asm file=yourfile"
	@echo "                      (only provide the filename without extension)"
	@echo "flash-asm-uart      - Flash compiled ASM binary via UART"
	@echo "run-asm-uart        - Compile and flash ASM binary via UART"
	@echo "help                - Show this help"

# TODO: add commands for compiling a file into a bin, sending bin over UART, triggering simulation scripts, etc.
