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

help:
	@echo "venv      - Create or sync virtual environment with dependencies"
	@echo "install   - Install fpgc tools in editable mode for development"
	@echo "uninstall - Uninstall fpgc tools editable installation"
	@echo "dev-setup - Install complete development environment setup"
	@echo "lint      - Run ruff lint and mypy"
	@echo "format    - Run ruff format"
	@echo "test      - Run pytest with coverage"
	@echo "check     - Run ruff, mypy and pytest"
	@echo "clean     - Clean up files and remove venv"
	@echo "help      - Show this help"
