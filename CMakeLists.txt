# cmake_minimum_required(VERSION <specify CMake version here>)
project(RDT)

set(CMAKE_CXX_STANDARD 14)

add_executable(ALTERNATING-BIT-PROTOCOL
        rdt_abp.cpp Utils.h ABP.h)

add_executable(GO-BACK-N
        rdt_gbn.cpp Utils.h GBN.h)

add_executable(GO-BACK-N-CUMULATIVE-ACK
        rdt_gbn_cumulative_ack.cpp Utils.h GBN_cumulative_ack.h)
