#pragma once

#define EC_TRY_OR_RTN(expr, ec)                 \
    do {                                        \
        expr;                                   \
        if (ec) {                               \
            std::cerr << ec.message() << '\n';  \
            return;                             \
        }                                       \
    }                                           \
    while(false)