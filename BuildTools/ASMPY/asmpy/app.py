import logging
import sys
from asmpy.assembler import Assembler
from asmpy.utils import parse_args
from asmpy.logger import CustomFormatter, configure_logging


def main():
    args = parse_args()

    configure_logging(args.log_level, CustomFormatter(log_details=args.log_details))
    logger = logging.getLogger()

    logger.info("Starting assembler")
    logger.debug(f"Arguments: {args}")

    assembler = Assembler(args.file, args.output)

    try:
        assembler.assemble()
    except Exception as e:
        logger.error(f"Assembler failed: {e}")
        sys.exit(1)

    logger.info("Assembler finished")


if __name__ == "__main__":
    main()
