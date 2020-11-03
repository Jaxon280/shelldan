#!/bin/bash
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color
BOLD='\033[1m'
GRAY='\033[37m'
BACKRED='\033[41m'
BACKGREEN='\033[102m'

PASSEDCOUNTER=0
TESTNUM=0

assert_exec() {
    ((TESTNUM++))
    input="$1"
    expected="$2"

    cmd="./sample"

    expect -c "
        spawn env /Users/keresu0720/environment/Sandbox-Class/c-kadai/kadai-39/main
        expect \"shellman$ \"
        send \"${cmd}\n\"
        expect \"\"
        send \"${input}\n\"
        expect \"${expected}\"
        exit
    "

    echo
    echo -e "${GREEN}assert_exec() OK${NC}"
    ((PASSEDCOUNTER++))
}

assert_args() {
    ((TESTNUM++))
    input="$1"
    expected="$2"

    cmd="./sample_args"

    expect -c "
        spawn env /Users/keresu0720/environment/Sandbox-Class/c-kadai/kadai-39/main
        expect \"shellman$ \"
        send \"${cmd} ${input}\n\"
        expect \"${expected}\"
        exit
    "

    echo
    echo -e "${GREEN}assert_args() OK${NC}"
    ((PASSEDCOUNTER++))
}

assert_pipe() {
    ((TESTNUM++))
    input="$1"
    expected="$2"

    cmd="./sample"

    expect -c "
        spawn env /Users/keresu0720/environment/Sandbox-Class/c-kadai/kadai-39/main
        expect \"shellman$ \"
        send \"${cmd} | ${cmd}\n\"
        expect \"\"
        send \"${input}\n\"
        expect \"${expected}\"
        exit
    "

    echo
    echo -e "${GREEN}assert_pipe() OK${NC}"
    ((PASSEDCOUNTER++))
}

assert_multipipe() {
    ((TESTNUM++))
    input="$1"
    expected="$2"

    cmd="./sample"

    expect -c "
        spawn env /Users/keresu0720/environment/Sandbox-Class/c-kadai/kadai-39/main
        expect \"shellman$ \"
        send \"${cmd} | ${cmd} | ${cmd} | ${cmd} | ${cmd} | ${cmd} | ${cmd} | ${cmd}\n\"
        expect \"\"
        send \"${input}\n\"
        expect \"${expected}\"
        exit
    "

    echo
    echo -e "${GREEN}assert_multipipe() OK${NC}"
    ((PASSEDCOUNTER++))
}

assert_multipipe_sleep() {
    ((TESTNUM++))
    input="$1"
    expected="$2"

    cmd="./sample_sleep"

    expect -c "
        spawn env /Users/keresu0720/environment/Sandbox-Class/c-kadai/kadai-39/main
        expect \"shellman$ \"
        send \"${cmd} | ${cmd} | ${cmd} | ${cmd} | ${cmd} | ${cmd} | ${cmd} | ${cmd}\n\"
        expect \"\"
        send \"${input}\n\"
        expect \"${expected}\"
        exit
    "

    echo
    echo -e "${GREEN}assert_multipipe_sleep() OK${NC}"
    ((PASSEDCOUNTER++))
}

assert_leftredirect() {
    ((TESTNUM++))
    input="$1"
    expected="$2"

    cmd="./sample"
    filepath="./sample_in.txt"

    expect -c "
        spawn env /Users/keresu0720/environment/Sandbox-Class/c-kadai/kadai-39/main
        expect \"shellman$ \"
        send \"${cmd} < ${filepath}\n\"
        expect \"${expected}\"
        exit
    "

    echo
    echo -e "${GREEN}assert_leftredirect() OK${NC}"
    ((PASSEDCOUNTER++))
}

assert_leftredirect() {
    ((TESTNUM++))
    input="$1"
    expected="$2"

    cmd="./sample"
    filepath="./sample_in.txt"

    expect -c "
        spawn env /Users/keresu0720/environment/Sandbox-Class/c-kadai/kadai-39/main
        expect \"shellman$ \"
        send \"${cmd} < ${filepath}\n\"
        expect \"${expected}\"
        exit
    "

    echo
    echo -e "${GREEN}assert_leftredirect() OK${NC}"
    ((PASSEDCOUNTER++))
}

assert_rightredirect() {
    ((TESTNUM++))
    input="$1"
    expected="$2"

    cmd="./sample"
    filepath="./sample_out.txt"

    expect -c "
        spawn env /Users/keresu0720/environment/Sandbox-Class/c-kadai/kadai-39/main
        expect \"shellman$ \"
        send \"${cmd} > ${filepath}\n\"
        expect \"\"
        send \"${input}\n\"
        expect \"shellman$ \"
        exit
    "

    output=`cat ./sample_out.txt`

    if [ "$output" = "$expected" ]; then
        echo
        echo -e "${GREEN}assert_rightredirect() OK ${input} => ${output} ${NC}"
        ((PASSEDCOUNTER++))

    else
        echo
        echo -e "${RED}assert_rightredirect() OK $input => $expected expected, but got $output ${NC}"
    fi
}

assert_pipeandrightredirect() {
    ((TESTNUM++))
    input="$1"
    expected="$2"

    cmd="./sample"
    filepath="./sample_out.txt"

    expect -c "
        spawn env /Users/keresu0720/environment/Sandbox-Class/c-kadai/kadai-39/main
        expect \"shellman$ \"
        send \"${cmd} | ${cmd} | ${cmd} | ${cmd} | ${cmd} | ${cmd} > ${filepath}\n\"
        expect \"\"
        send \"${input}\n\"
        expect \"shellman$ \"
        exit
    "

    output=`cat ./sample_out.txt`

    if [ "$output" = "$expected" ]; then
        echo
        echo -e "${GREEN}assert_pipeandrightredirect() OK ${input} => ${output} ${NC}"
        ((PASSEDCOUNTER++))
    else
        echo
        echo -e "${RED}assert_pipeandrightredirect() OK $input => $expected expected, but got $output ${NC}"
    fi
}

assert_rightredirectandleftredirect() {
    ((TESTNUM++))
    input="$1"
    expected="$2"

    cmd="./sample"
    in_filepath="./sample_in.txt"
    out_filepath="./sample_out.txt"

    expect -c "
        spawn env /Users/keresu0720/environment/Sandbox-Class/c-kadai/kadai-39/main
        expect \"shellman$ \"
        send \"${cmd} < ${in_filepath} > ${out_filepath}\n\"
        expect \"shellman$ \"
        exit
    "

    output=`cat ./sample_out.txt`

    if [ "$output" = "$expected" ]; then
        echo
        echo -e "${GREEN}assert_rightredirectandleftredirect() OK ${input} => ${output} ${NC}"
        ((PASSEDCOUNTER++))
    else
        echo
        echo -e "${RED}assert_rightredirectandleftredirect() OK $input => $expected expected, but got $output ${NC}"
    fi
}


assert_exec 5 10
assert_args 3 6
assert_pipe 10 40
assert_multipipe 2 512
assert_multipipe_sleep 3 768
assert_leftredirect 7 14
assert_rightredirect 8 16
assert_pipeandrightredirect 8 512
assert_rightredirectandleftredirect 7 14

FAILCOUNTER=$[$TESTNUM-$PASSEDCOUNTER]

if [ "$PASSEDCOUNTER" -eq "$TESTNUM" ]; then
    echo
    echo -e "${BACKGREEN}${GRAY} all test (${PASSEDCOUNTER}cases / ${TESTNUM}cases) has been passed!  ${NC}"
else
    echo
    echo -e "${BACKRED}${GRAY} some cases (${FAILCOUNTER}cases / ${TESTNUM}cases) have been failed. ${NC}"
fi
echo
