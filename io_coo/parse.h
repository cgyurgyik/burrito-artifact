#pragma once

#include <nanobind/stl/string.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#include "types.h"

namespace io_coo {

template<typename index_t, typename value_t>
COO2D<index_t, value_t> parse2D(const std::string filename) {
    COO2D<index_t, value_t> ret;

    std::ifstream file(filename);
    // file.open(filename, std::ios::out);   

    if (file.is_open()) {
        // std::string fline;
        // if (!std::getline(file, fline)) {
        //     std::cerr << "mtx file is empty" << std::endl;
        //     return ret;
        // }

        // std::stringstream flineStream(fline);

        std::string head, type, formats, field, symmetry;
        // formats = [coordinate array]
        // type = [matrix tensor]
        // field = [real integer complex pattern]
        // symmetry = [general symmetric skew-symmetric Hermitian]
        file >> head >> type >> formats >> field >> symmetry;
        if (head != "%%MatrixMarket") {
            std::cerr << "Unknown header of MatrixMarket" << std::endl;
            std::cerr << head << std::endl;
            exit(-1);
            return ret;
        } else if (type != "matrix" && type != "tensor") {
            std::cerr << "Unknown type of MatrixMarket" << std::endl;
            std::cerr << type << std::endl;
            exit(-1);
            return ret;
        } else if (field != "real") {
            std::cerr << "MatrixMarket field not available" << std::endl;
            std::cerr << field << std::endl;
            exit(-1);
            return ret;
        } else if (symmetry != "general" && symmetry != "symmetric" && symmetry != "skew-symmetric") {
            // TODO: handle symmetric.
            std::cerr << "MatrixMarket symmetry not available" << std::endl;
            std::cerr << symmetry << std::endl;
            exit(-1);
            return ret;
        }
        file.ignore(2048, '\n');

        while (file.peek() == '%') {
            // std::string line;
            // std::getline(file, line);
            file.ignore(2048, '\n');
            // file.ignore(1);
            // std::cerr << "ignoring line: " << line << "\n";
        }

        // std::cout << "peeking: " << file.peek() << "\n";
        uint64_t N = 0, M = 0, nnz = 0;
        file >> N >> M >> nnz;

        const bool is_skew = (symmetry == "skew-symmetric");
        const bool is_symm = (symmetry == "symmetric") || is_skew;
        // const uint64_t nnz_alloc = symm ? nnz * 2 : nnz;
        index_t *c0 = new index_t[nnz];
        index_t *c1 = new index_t[nnz];
        value_t *v = new value_t[nnz];

        if (is_symm) {
            uint64_t num_symm = 0;
            for (uint64_t c = 0; c < nnz; c++) {
                uint64_t i, j;
                double val;
                // Only "pattern" has this issue.
                // file >> i >> j;
                // if (file.peek() == '\n') {
                //     val = 1.0f;
                // } else {
                //     file >> val;
                // }
                file >> i >> j >> val;
                c0[c] = (index_t)(i - 1);
                c1[c] = (index_t)(j - 1);
                v[c] = (value_t)val;
                num_symm += (i != j);
            }
            // std::cerr << "Number nnz: " << nnz << "\n";
            // std::cerr << "Number symmetric: " << num_symm << "\n";
            if (num_symm != 0) {
                index_t *_c0 = new index_t[nnz + num_symm];
                index_t *_c1 = new index_t[nnz + num_symm];
                value_t *_v = new value_t[nnz + num_symm];
                // TODO: either we do this n^2 or we don't sort.
                // I'm going with the latter for now.
                std::memcpy(_c0, c0, nnz * sizeof(index_t));
                std::memcpy(_c1, c1, nnz * sizeof(index_t));
                std::memcpy(_v, v, nnz * sizeof(value_t));
                delete[] c0;
                delete[] c1;
                delete[] v;
                c0 = _c0;
                c1 = _c1;
                v = _v;
                uint64_t counter = nnz;

                for (uint64_t c = 0; c < nnz; c++) {
                    if (c0[c] != c1[c]) {
                        c0[counter] = c1[c];
                        c1[counter] = c0[c];
                        if (is_skew) {
                            v[counter] = -v[c];
                        } else {
                            v[counter] = v[c];
                        }
                        counter++;
                    }
                }
                assert(counter == nnz + num_symm);
                nnz += num_symm;
            }
        } else {
            for (uint64_t c = 0; c < nnz; c++) {
                uint64_t i, j;
                double val;
                // Only "pattern" has this issue.
                // file >> i >> j;
                // if (file.peek() == '\n') {
                //     val = 1.0f;
                // } else {
                //     file >> val;
                // }
                file >> i >> j >> val;
                c0[c] = (index_t)(i - 1);
                c1[c] = (index_t)(j - 1);
                v[c] = (value_t)val;
            }
            assert(counter == nnz);
        }

        file.close();

        // These *have* to be size_t
        size_t shape_nnz[1] = { (size_t)nnz };
        nb::capsule owner_c0(c0, [](void *p) noexcept {
            delete[] (index_t *)p;
        });
        nb::capsule owner_c1(c1, [](void *p) noexcept {
            delete[] (index_t *)p;
        });
        nb::capsule owner_v(v, [](void *p) noexcept {
            delete[] (value_t *)p;
        });

        ret.row = Array<index_t>(c0, /* ndim = */ 1, shape_nnz, owner_c0);
        ret.col = Array<index_t>(c1, /* ndim = */ 1, shape_nnz, owner_c1);
        ret.data = Array<float>(v, /* ndim = */ 1, shape_nnz, owner_v);
        ret.N = N;
        ret.M = M;
    } else {
        std::cerr << "Failed to open (and parse from) file: " << filename << std::endl;
        assert(false);
    }
    return ret;
}

}  // namespace io_coo
