import logging
import sys
from pathlib import Path
from asmpy.assembler import Assembler
from asmpy.preprocessor import Preprocessor
from asmpy.utils import parse_args, read_input_file
from asmpy.logger import CustomFormatter, configure_logging
from asmpy.models.data_types import Number


def main():
    args = parse_args()

    configure_logging(args.log_level, CustomFormatter(log_details=args.log_details))
    logger = logging.getLogger()

    logger.info("Starting asmpy")
    logger.debug(f"Arguments: {args}")

    input_file_path = Path(args.file)
    source_input_lines = read_input_file(input_file_path)

    try:
        preprocessed_lines = Preprocessor(
            source_input_lines=source_input_lines,
            file_path=input_file_path,
        ).preprocess()
    except Exception as e:
        logger.error(f"Preprocessor failed: {e}")
        sys.exit(1)

    try:
        offset_address = Number(args.offset)
    except ValueError as e:
        logger.error(f"Invalid offset value: {args.offset}. {e}")
        sys.exit(1)

    assembler = Assembler(preprocessed_lines, args.output, offset_address=offset_address)
    try:
        assembler.assemble(add_header=args.header)
    except Exception as e:
        logger.error(f"Assembler failed: {e}")
        sys.exit(1)

    logger.info("Assembler finished")


if __name__ == "__main__":
    main()
