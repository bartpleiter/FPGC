import logging
from pathlib import Path
from asmpy.assembler import Assembler
from asmpy.preprocessor import Preprocessor
from asmpy.utils import parse_args, read_input_file
from asmpy.logger import CustomFormatter, configure_logging


def main():
    args = parse_args()

    configure_logging(args.log_level, CustomFormatter(log_details=args.log_details))
    logger = logging.getLogger()

    logger.info("Starting asmpy")
    logger.debug(f"Arguments: {args}")

    source_input_lines = read_input_file(args.file)

    input_file_path = Path(args.file)

    try:
        preprocessed_lines = Preprocessor(
            source_input_lines=source_input_lines,
            file_path=input_file_path,
        ).preprocess()
    except Exception as e:
        logger.error(f"Preprocessor failed: {e}")
        raise e

    assembler = Assembler(preprocessed_lines, args.output)
    try:
        assembler.assemble()
    except Exception as e:
        logger.error(f"Assembler failed: {e}")
        raise e

    logger.info("Assembler finished")


if __name__ == "__main__":
    main()
