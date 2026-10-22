#!/bin/bash

case "$1" in
    bench)
	echo "========================================"
        echo "🚀 Running benchmark..."
        echo "========================================"
        rm -f main
        gcc -o main main.c -mavx2 -mbmi -O3 -lcpucycles -DBENCH=1 -DFIXED_NEG
        taskset --cpu-list 0 ./main
        ;;
    test)
	echo "========================================"
        echo "🧪 Running tests (AddressSanitizer + DEBUG)..."
        echo "========================================"
        rm -f main
        gcc -o main main.c \
            -mavx2 \
	    -mbmi \
            -O2 \
            -g3 \
            -fsanitize=address \
            -Wall \
            -pedantic \
            -Wuninitialized \
            -Wno-unused-function \
            -Wno-unused-variable \
            -DDEBUG=1
        taskset --cpu-list 0 ./main
        ;;
    leakage)
	echo "========================================"
        echo ""
        echo "========================================"
        rm -f main
        gcc -o main main.c \
            -mavx2 \
            -msse4.1 \
	        -O0 \
	        -g3 \
	        -fno-omit-frame-pointer \
		-DCT_ANALYSIS=1
        
        valgrind --tool=memcheck \
            --track-origins=yes \
            --leak-check=no \
            --error-limit=no \
            ./main
        ;;

    *)
        echo "Uso: $0 {bench|test}"
        exit 1
        ;;
esac
