venv:
	uv sync

mypy:
	uv run mypy asmpy
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
	pipx install .


clean:
	-pipx uninstall asmpy
	-rm -rf asmpy.egg-info
	-rm -rf build
	-rm -rf .venv
	-rm -rf .mypy_cache
	-rm -rf .pytest_cache
	-rm -rf .ruff_cache
	-rm -rf .coverage
	-find . -type d -name __pycache__ -exec rm -r {} \+ 2>/dev/null; true


help:
	@echo "venv     - Create or sync virtual environment with dependencies"
	@echo "lint     - Run ruff lint and mypy"
	@echo "format   - Run ruff format"
	@echo "test     - Run pytest with coverage"
	@echo "check    - Run ruff, mypy and pytest"
	@echo "install  - Install asmpy globally"
	@echo "clean    - Clean up files and remove venv"
	@echo "help     - Show this help"
