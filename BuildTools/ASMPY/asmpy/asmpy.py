import argparse
import structlog
from asmpy.assembler import Assembler

logger = structlog.get_logger()


def parse_args():
    parser = argparse.ArgumentParser(description="Assembler for B32P2.")
    parser.add_argument("file", help="The asm file to assemble")
    parser.add_argument("output", help="The assembled output file")
    return parser.parse_args()


def main():
    args = parse_args()
    logger.info("Starting Assembler")
    assembler = Assembler(args.file, args.output)
    try:
        assembler.assemble()
    except Exception as e:
        logger.error("Assembler failed", error=e)


if __name__ == "__main__":
    main()
