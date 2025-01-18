#!/usr/bin/env python3

import sys
import CompileInstruction

# List of already inserted libraries.
# To prevent multiple insertions of the same library.
# Global to allow access in recursion
libraryList = []

# If the program we assemble is the BDOS operating system
BDOSos = False

# If we have to assemble the program as a BDOS user program
BDOSprogram = False

# Global offset of program in memory
programOffset = 0

# Remove unreachable code
optimizeSize = False


def removeFunctionFromCode(parsedLines, toRemove):
    returnList = []

    start = -1
    end = -1

    belowCodeSection = False

    for idx, line in enumerate(parsedLines):
        if line[1][0] == "Int:":
            belowCodeSection = True
        if start == -1:
            if line[1][0] == toRemove + ":":
                start = idx
                # print (start)
                # print(parsedLines[start])
        elif end == -1:
            # continue until next function found
            if line[1][0][-1] == ":":
                if not belowCodeSection:
                    if "Label_" not in line[1][0]:
                        end = idx
                        # print (end)
                        # print(parsedLines[end])
                # when we are below the code section, stop at any new label
                else:
                    end = idx

    returnList = returnList + parsedLines[0:start]
    returnList = returnList + parsedLines[end:]

    return returnList


def removeUnreachebleCode(parsedLines):
    # print("orig len:", len(parsedLines))
    returnList = parsedLines

    asm = [x[1] for x in parsedLines]
    functionNames = []
    jumps = []

    for x in asm:

        if len(x) > 0:
            if x[0][-1] == ":":
                if "Label_" not in x[0]:
                    functionNames.append(x[0][:-1])
            if x[0] == "addr2reg":
                if "Label_" not in x[1]:
                    jumps.append(x[1])

            if x[0] == "jump":
                if "Label_" not in x[1]:
                    jumps.append(x[1])

    # for f in functionNames:
    #    print(f)

    # for j in jumps:
    #    print(j)

    unusedFunctions = list(
        (set(functionNames).difference(jumps)).difference(["Main", "Int", "Syscall"])
    )

    foundUnusedFunctions = len(unusedFunctions)

    for u in unusedFunctions:
        # print(u)
        returnList = removeFunctionFromCode(returnList, u)

    # recursive check
    if foundUnusedFunctions > 0:
        returnList = removeUnreachebleCode(returnList)

    # print("after len:", len(returnList))

    return returnList


def parseLines(fileName):
    parsedLines = []
    with open(fileName, "r") as f:
        for i, line in enumerate(f, start=1):
            # do something special in case of a .ds instruction
            if len(line) > 4 and line.split(" ", maxsplit=1)[0] == ".ds":
                parsedLines.append(
                    (i, [".ds", line.split(" ", maxsplit=1)[1].rstrip("\n")])
                )
            else:
                parsedLine = line.strip().split(";", maxsplit=1)[0].split()
                if parsedLine != []:
                    parsedLines.append((i, parsedLine))

    parsedLines.append((0, [".EOF"]))  # add end of file token
    return parsedLines


def moveDataDown(parsedLines):
    for idx, line in enumerate(parsedLines):
        if line[1][0] == ".EOF":  # return when gone through entire file
            return parsedLines
        if line[1][0] == ".data":  # when we found the start of a .data segment
            while (
                parsedLines[idx][1][0] != ".code"
                and parsedLines[idx][1][0] != ".rdata"
                and parsedLines[idx][1][0] != ".bss"
                and parsedLines[idx][1][0] != ".EOF"
            ):  # move all lines to the end until .code, .rdata or .EOF
                parsedLines.append(parsedLines.pop(idx))

    # should not get here
    print("SHOULD NOT GET HERE")
    sys.exit(1)
    return None


def moveRDataDown(parsedLines):
    for idx, line in enumerate(parsedLines):
        if line[1][0] == ".EOF":  # return when gone through entire file
            return parsedLines
        if line[1][0] == ".rdata":  # when we found the start of a .rdata segment
            while (
                parsedLines[idx][1][0] != ".code"
                and parsedLines[idx][1][0] != ".data"
                and parsedLines[idx][1][0] != ".bss"
                and parsedLines[idx][1][0] != ".EOF"
            ):  # move all lines to the end until .code, .data or .EOF
                parsedLines.append(parsedLines.pop(idx))

    # should not get here
    print("SHOULD NOT GET HERE")
    sys.exit(1)
    return None


def moveBssDown(parsedLines):
    for idx, line in enumerate(parsedLines):
        if line[1][0] == ".EOF":  # return when gone through entire file
            return parsedLines
        if line[1][0] == ".bss":  # when we found the start of a .rdata segment
            while (
                parsedLines[idx][1][0] != ".code"
                and parsedLines[idx][1][0] != ".data"
                and parsedLines[idx][1][0] != ".rdata"
                and parsedLines[idx][1][0] != ".EOF"
            ):  # move all lines to the end until .code, .data or .EOF
                parsedLines.append(parsedLines.pop(idx))

    # should not get here
    print("SHOULD NOT GET HERE")
    sys.exit(1)
    return None


def removeAssemblerDirectives(parsedLines):
    return [
        line
        for line in parsedLines
        if line[1][0] not in [".code", ".rdata", ".data", ".bss", ".EOF"]
    ]


def insertLibraries(parsedLines):
    returnList = []
    returnList.extend(parsedLines)

    for line in parsedLines:
        if len(line[1]) == 2:
            if (line[1][0]) == "`include":
                if line[1][1] not in libraryList:
                    libraryList.append(line[1][1])
                    insertList = insertLibraries(
                        parseLines(line[1][1])
                    )  # recursion to include libraries within libraries
                    for i in range(len(insertList)):
                        returnList.insert(i, insertList[i])

    return returnList


def compileLine(line):
    compiledLine = ""

    # check what kind of instruction this line is
    switch = {
        "halt": CompileInstruction.compileHalt,
        "read": CompileInstruction.compileRead,
        "write": CompileInstruction.compileWrite,
        "readintid": CompileInstruction.compileIntID,
        "push": CompileInstruction.compilePush,
        "pop": CompileInstruction.compilePop,
        "jump": CompileInstruction.compileJump,
        "jumpo": CompileInstruction.compileJumpo,
        "jumpr": CompileInstruction.compileJumpr,
        "jumpro": CompileInstruction.compileJumpro,
        "beq": CompileInstruction.compileBEQ,
        "bgt": CompileInstruction.compileBGT,
        "bgts": CompileInstruction.compileBGTS,
        "bge": CompileInstruction.compileBGE,
        "bges": CompileInstruction.compileBGES,
        "bne": CompileInstruction.compileBNE,
        "blt": CompileInstruction.compileBLT,
        "blts": CompileInstruction.compileBLTS,
        "ble": CompileInstruction.compileBLE,
        "bles": CompileInstruction.compileBLES,
        "savpc": CompileInstruction.compileSavPC,
        "reti": CompileInstruction.compileReti,
        "ccache": CompileInstruction.compileCcache,
        "or": CompileInstruction.compileOR,
        "and": CompileInstruction.compileAND,
        "xor": CompileInstruction.compileXOR,
        "add": CompileInstruction.compileADD,
        "sub": CompileInstruction.compileSUB,
        "shiftl": CompileInstruction.compileSHIFTL,
        "shiftr": CompileInstruction.compileSHIFTR,
        "shiftrs": CompileInstruction.compileSHIFTRS,
        "not": CompileInstruction.compileNOT,
        "mults": CompileInstruction.compileMULTS,
        "multu": CompileInstruction.compileMULTU,
        "multfp": CompileInstruction.compileMULTFP,
        "slt": CompileInstruction.compileSLT,
        "sltu": CompileInstruction.compileSLTU,
        "load": CompileInstruction.compileLoad,
        "loadhi": CompileInstruction.compileLoadHi,
        "addr2reg": CompileInstruction.compileAddr2reg,
        "load32": CompileInstruction.compileLoad32,
        "nop": CompileInstruction.compileNop,
        ".dw": CompileInstruction.compileDw,
        ".dd": CompileInstruction.compileDd,
        ".db": CompileInstruction.compileDb,
        ".ds": CompileInstruction.compileDs,
        ".dl": CompileInstruction.compileDl,
        "loadlabellow": CompileInstruction.compileLoadLabelLow,
        "loadlabelhigh": CompileInstruction.compileLoadLabelHigh,
        "`include": CompileInstruction.compileNothing,
        ".eof": CompileInstruction.compileNothing,
    }

    try:
        compiledLine = switch[line[0].lower()](line)

    # print errors
    except KeyError:
        # check if line is a label
        if len(line) == 1 and line[0][-1] == ":":
            compiledLine = "Label " + str(line[0])
        # if not a label, raise error
        else:
            raise Exception("Unknown instruction '" + str(line[0]) + "'")

    return compiledLine


# compiles lines that can be compiled directly
def passOne(parsedLines):
    passOneResult = []

    for line in parsedLines:
        try:
            compiledLine = compileLine(line[1])

            # fix instructions that have multiple lines

            if compiledLine.split()[0] == "loadBoth":
                passOneResult.append(
                    (
                        line[0],
                        compileLine(
                            ["load", compiledLine.split()[2], compiledLine.split()[3]]
                        ),
                    )
                )
                compiledLine = compileLine(
                    ["loadhi", compiledLine.split()[1], compiledLine.split()[3]]
                )

            if compiledLine.split()[0] == "loadLabelHigh":
                passOneResult.append(
                    (line[0], "loadLabelLow " + " ".join(compiledLine.split()[1:]))
                )

            if compiledLine.split()[0] == "data":
                for i in compiledLine.split():
                    if i != "data":
                        passOneResult.append((line[0], i + " //data"))
            else:
                if compiledLine != "ignore":
                    passOneResult.append((line[0], compiledLine))
        except Exception as e:
            print("Error in line " + str(line[0]) + ": " + " ".join(line[1]))
            print("The error is: {0}".format(e))
            print("Assembler will now exit")
            sys.exit(1)

    return passOneResult


# reads and removes define statements, stores them into dictionary
def obtainDefines(content):
    defines = {}  # list of definitions with their value

    contentWithoutDefines = []  # lines without defines
    defineLines = []  # lines with defines

    # seperate defines from other lines
    for line in content:
        if line[1][0].lower() == "define":

            # do error checking
            if len(line[1]) != 4 or line[1][2] != "=":
                print("Error in line " + str(line[0]) + ": " + " ".join(line[1]))
                print("Invalid define statement")
                print("Assembler will now exit")
                sys.exit(1)

            defineLines.append(line)
        else:
            contentWithoutDefines.append(line)

    # parse the lines with defines
    for line in defineLines:
        if line[1][1] in defines:
            print("Error: define " + line[1][1] + " is already defined")
            print("Assembler will now exit")
            sys.exit(1)
        defines.update({line[1][1]: line[1][3]})

    return defines, contentWithoutDefines


# replace defined words with their value
def processDefines(defines, content):
    replacedContent = []  # lines where defined words have been replaced

    # for each line, replace the words with their corresponding value if defined
    for line in content:
        replacedContent.append((line[0], [defines.get(word, word) for word in line[1]]))

    return replacedContent


# adds interrupts, program length placeholder and jump to main
# skip program length placeholder in case of BDOS program
# add jump to syscall if BDOS os
# NOTE: because of a unknown bug in B32P (probably related to return address of interrupt directly after jumping to SDRAM from ROM,
# the 4th instruction needs to be jump Main as well
def addHeaderCode(parsedLines):
    if BDOSprogram:
        header = [(0, "jump Main"), (0, "jump Int"), (0, "jump Main"), (0, "jump Main")]
    elif BDOSos:
        header = [
            (0, "jump Main"),
            (0, "jump Int"),
            (0, "LengthOfProgram"),
            (0, "jump Main"),
            (0, "jump Syscall"),
        ]
    else:
        header = [
            (0, "jump Main"),
            (0, "jump Int"),
            (0, "LengthOfProgram"),
            (0, "jump Main"),
        ]

    return header + parsedLines


# move labels to the next line
def moveLabels(parsedLines):
    returnList = []

    # move to next line
    # (old iteration) for idx, line in enumerate(parsedLines):
    idx = 0
    while idx < len(parsedLines):
        line = parsedLines[idx]
        if line[1].lower().split()[0] == "label":
            if idx < len(parsedLines) - 1:

                if parsedLines[idx + 1][1].lower().split()[0] == "label":
                    # (OLD) if we have a label directly below, insert a nop as a quick fix
                    # parsedLines.insert(idx+1, (0, "$*" + line[1].split()[1] + "*$ " +"00000000000000000000000000000000 //NOP to quickfix double labels"))

                    # if we have a label directly below, insert the label in the first non-label line
                    i = 2
                    labelDone = False
                    while idx + i < len(parsedLines) - 1 and not labelDone:
                        if parsedLines[idx + i][1].lower().split()[0] != "label":
                            labelDone = True
                            parsedLines[idx + i] = (
                                parsedLines[idx + i][0],
                                "$*"
                                + line[1].split()[1]
                                + "*$ "
                                + parsedLines[idx + i][1],
                            )
                            # add label in comments, but only if the line does not need to have a second pass
                            # TODO implement this!
                            # if parsedLines[idx+i][1].split()[1][0] == "0" or parsedLines[idx+i][1].split()[1][0] == "1":
                            #    parsedLines[idx+i][1] = parsedLines[idx+i][1] + " @" + line[1].split()[1][:-1]
                        i += 1
                else:
                    parsedLines[idx + 1] = (
                        parsedLines[idx + 1][0],
                        "$*" + line[1].split()[1] + "*$ " + parsedLines[idx + 1][1],
                    )
                    # add label in comments, but only if the line does not need to have a second pass
                    # TODO implement this!
                    # if parsedLines[idx+1][1].split()[1][0] == "0" or parsedLines[idx+1][1].split()[1][0] == "1":
                    #    parsedLines[idx+1][1] = parsedLines[idx+1][1] + " @" + line[1].split()[1][:-1]
            else:
                print(
                    "Error: label "
                    + line[1].split()[1]
                    + " has no instructions below it"
                )
                print("Assembler will now exit")
                sys.exit(1)
        idx += 1

    # remove original labels
    for line in parsedLines:
        if line[1].lower().split()[0] != "label":
            returnList.append(line)

    return returnList


# renumbers each line
def redoLineNumbering(parsedLines):
    returnList = []

    for idx, line in enumerate(parsedLines):
        returnList.append((idx + programOffset, line[1]))

    return returnList


# removes label prefix and returns a map of labels to line numbers
# assumes that $* does not occur somewhere else, and that labels are seperated by space
def getLabelMap(parsedLines):
    labelMap = {}
    returnList = []

    for line in parsedLines:
        numberOfLabels = line[1].count("$*")
        for i in range(numberOfLabels):
            if line[1].split()[i][:2] == "$*" and line[1].split()[i][-3:] == ":*$":
                if line[1].split()[i][2:-3] in labelMap:
                    print(
                        "Error: label "
                        + line[1].split()[i][2:-3]
                        + " is already defined"
                    )
                    print("Assembler will now exit")
                    sys.exit(1)

                labelMap[line[1].split()[i][2:-3]] = line[0]

        if line[1].split()[0][:2] == "$*" and line[1].split()[0][-2:] == "*$":
            returnList.append((line[0], line[1].split("*$ ")[-1]))
        else:
            returnList.append(line)

    return returnList, labelMap


# compiles all labels
def passTwo(parsedLines, labelMap):
    # lines that start with these names should be compiled
    toCompileList = [
        "jump",
        "beq",
        "bgt",
        "bgts",
        "bge",
        "bges",
        "bne",
        "blt",
        "blts",
        "ble",
        "bles",
        "loadlabellow",
        "loadlabelhigh",
        ".dl",
    ]

    for idx, line in enumerate(parsedLines):
        if line[1].lower().split()[0] in toCompileList:
            for idx2, word in enumerate(line[1].split()):
                if word in labelMap:
                    x = line[1].split()
                    x[idx2] = str(labelMap.get(word))
                    y = compileLine(x)
                    parsedLines[idx] = (parsedLines[idx][0], y)

    return parsedLines


# check if all labels are compiled
def checkNoLabels(parsedLines):
    toCompileList = [
        "jump",
        "beq",
        "bgt",
        "bgts",
        "bge",
        "bges",
        "bne",
        "blt",
        "blts",
        "ble",
        "bles",
        "loadlabellow",
        "loadlabelhigh",
        ".dl",
    ]

    for idx, line in enumerate(parsedLines):
        if line[1].lower().split()[0] in toCompileList:
            labelPos = 0
            if line[1].lower().split()[0] in [
                "jump",
                "loadlabellow",
                "loadlabelhigh",
                ".dl",
            ]:
                labelPos = 1
            if line[1].lower().split()[0] in [
                "beq",
                "bgt",
                "bgts",
                "bge",
                "bges",
                "bne",
                "blt",
                "blts",
                "ble",
                "bles",
            ]:
                labelPos = 3
            print("Error: label " + line[1].split()[labelPos] + " is undefined")
            print("Assembler will now exit")
            sys.exit(1)

        if line[1].lower().split()[0] == "label":
            print(
                "Error: label "
                + line[1].split()[1]
                + " is used directly after another label"
            )
            print("Assembler will now exit")
            sys.exit(1)


def main():
    # check assemble mode and offset
    global BDOSos
    global BDOSprogram
    global programOffset
    global optimizeSize

    if len(sys.argv) >= 3:
        BDOSprogram = sys.argv[1].lower() == "bdos"
        if BDOSprogram:
            programOffset = CompileInstruction.getNumber(sys.argv[2])
    if len(sys.argv) >= 2:
        BDOSos = sys.argv[1].lower() == "os"
    if sys.argv[len(sys.argv) - 1] == "-O":
        optimizeSize = True

    # parse lines from file
    parsedLines = parseLines("code.asm")

    # move .data sections down
    parsedLines = moveDataDown(parsedLines)

    # move .rdata sections down
    parsedLines = moveRDataDown(parsedLines)

    # move .bss sections down
    parsedLines = moveBssDown(parsedLines)

    # remove all .code, .data, .rdata, .bss and .EOF lines
    parsedLines = removeAssemblerDirectives(parsedLines)

    # insert libraries
    parsedLines = insertLibraries(parsedLines)

    if optimizeSize:
        parsedLines = removeUnreachebleCode(parsedLines)

    # obtain and remove the define statements
    defines, parsedLines = obtainDefines(parsedLines)

    # replace defined words with their value
    parsedLines = processDefines(defines, parsedLines)

    # do pass one
    passOneResult = passOne(parsedLines)

    # add interrupt code and jumps
    passOneResult = addHeaderCode(passOneResult)

    # move labels to the next line
    passOneResult = moveLabels(passOneResult)

    # redo line numbers for jump addressing
    # from this point no line should become multiple lines in the final code!
    # also no shifting in line numbers!
    passOneResult = redoLineNumbering(passOneResult)

    # removes label prefixes and creates mapping from label to line
    passOneResult, labelMap = getLabelMap(passOneResult)

    # do pass two
    passTwoResult = passTwo(passOneResult, labelMap)

    # check if all labels are processed
    checkNoLabels(passTwoResult)

    # only add length of program if not BDOS user program
    if not BDOSprogram:
        lenString = "{0:032b}".format(len(passTwoResult)) + " //Length of program"
        # calculate length of program
        passTwoResult[2] = (2, lenString)

    # print result without line numbers
    for line in passTwoResult:
        print(line[1])


if __name__ == "__main__":
    main()
