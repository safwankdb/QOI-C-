# QOI Compression C++

An implementation of [QOI Image Format](https://qoiformat.org/) with an encoder and a decoder. QOI is an O(n) single pass lossless image compression algorithm which is 20-50x faster than PNG with comparable compression ratio.

### QOIF Specification
- Author has upload the original 1 page spec at [https://qoiformat.org/qoi-specification.pdf](https://qoiformat.org/qoi-specification.pdf).
- I have implemented a version with a 14 bit hash in ```qoi_large_hash.cpp```. Seems to be slightly better.

### TODO
- [ ] Add some benchmarks.
- [ ] Experiment with hash function.
