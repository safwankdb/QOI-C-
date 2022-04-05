#include <stdint.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

struct Pixel {
    uint8_t r = 0, g = 0, b = 0;
    
    Pixel() {};

    Pixel(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
    
    Pixel(uint8_t * ptr) {
        r = *ptr;
        g = *(ptr + 1);
        b = *(ptr + 2);
    }

    bool operator==(const Pixel& p) {
        return (r == p.r) && (g == p.g) && (b == p.b);
    }

    inline uint8_t hash() {
        return (3 * r + 5 * g + 7 * b) % 64;
    }

};

void decode(std::string inp_name, std::string out_name) {
    std::ifstream inp(inp_name, std::ios_base::binary);
    inp.seekg(0, inp.end);
    size_t length = inp.tellg();
    inp.seekg(0, inp.beg);

    std::vector<uint8_t> buffer(length);
    inp.read((char *) &buffer[0], length);
    int idx = 0;

    static const std::string magic = "qoif";
    for (int i = 0; i < 4; i++) {
        uint8_t byte = buffer[idx++];
        assert(byte == magic[i]);
    }

    uint32_t w = 0, h = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t byte = buffer[idx++];
        w = (w << 8) | byte;
    }
    for (int i = 0; i < 4; i++) {
        uint8_t byte = buffer[idx++];
        inp >> byte;
        h = (h << 8) | byte;
    }
    uint8_t ch = buffer[idx++];
    assert(ch == 0x03);
    uint8_t cs = buffer[idx++];
    assert(cs == 0x00 || cs == 0x01);

    Pixel prev;
    Pixel table[64];
    std::vector<uint8_t> out;
    out.reserve(ch * w * h);
    while (out.size() < ch * h * w) {
        uint8_t byte = buffer[idx++];
        if (byte == 0xFE) {
            uint8_t r = buffer[idx++];
            uint8_t g = buffer[idx++];
            uint8_t b = buffer[idx++];
            Pixel p(r, g, b);
            out.push_back(p.r);
            out.push_back(p.g);
            out.push_back(p.b);
            table[p.hash()] = p;
            prev = p;
        } else if ((byte & 0xC0) == 0x00) {
            Pixel p = table[byte & 0x3F];
            out.push_back(p.r);
            out.push_back(p.g);
            out.push_back(p.b);
            prev = p;
        } else if ((byte & 0xC0) == 0x40) {
            uint8_t dr = ((byte & 0x30) >> 4) - 2;
            uint8_t dg = ((byte & 0x0C) >> 2) - 2;
            uint8_t db = (byte & 0x03) - 2;
            Pixel p(prev.r + dr, prev.g + dg, prev.b + db);
            out.push_back(p.r);
            out.push_back(p.g);
            out.push_back(p.b);
            table[p.hash()] = p;
            prev = p;
        } else if ((byte & 0xC0) == 0x80) {
            uint8_t dg = (byte & 0x3F) - 32;
            byte = buffer[idx++];
            uint8_t dr_dg = ((byte & 0xF0) >> 4) - 8;
            uint8_t db_dg = (byte & 0x0F) - 8;
            Pixel p(prev.r + dr_dg + dg, prev.g + dg, prev.b + db_dg + dg);
            out.push_back(p.r);
            out.push_back(p.g);
            out.push_back(p.b);
            table[p.hash()] = p;
            prev = p;
        } else if ((byte & 0xC0) == 0xC0) {
            uint8_t run = (byte & 0x3F) + 1;
            while (run--) {
                out.push_back(prev.r);
                out.push_back(prev.g);
                out.push_back(prev.b);
            }
        } else {
            assert(false);
        }
    }
    uint8_t byte;
    for (int i = 0; i < 7; i++) {
        byte = buffer[idx++];
        assert(byte == 0x00);
    }
    byte = buffer[idx++];
    assert(byte == 0x01);
    assert(idx == length);
    stbi_write_png(out_name.c_str(), w, h, ch, (char*) &out[0], w * ch);
}

void encode(uint8_t * image, uint32_t w, uint32_t h, std::string name) {
    Pixel prev;
    Pixel table[64];
    std::vector<uint8_t> out;
    static const std::string magic = "qoif";
    for (char c : magic) {
        out.push_back(c);
    }
    for (int i = 0; i < 4; i++) {
        out.push_back((w >> (24 - 8*i)) & 0xFF);
    }
    for (int i = 0; i < 4; i++) {
        out.push_back((h >> (24 - 8*i)) & 0xFF);
    }
    out.push_back(0x03);
    out.push_back(0x01);

    Pixel p(image);
    out.push_back(0xFE);
    out.push_back(p.r);
    out.push_back(p.g);
    out.push_back(p.b);
    prev = p;
    table[p.hash()] = p;
    for (int i = 1; i < h * w; i++) {
        uint8_t *ptr = image + (3 * i);
        Pixel p(ptr);
        uint8_t hash = p.hash();
        if (p == prev) {
            int run = 1;
            while (run < 62 && i + 1 < h * w) {
                if (Pixel(image + (3 * (i + 1))) == prev) {
                    i++;
                    run++;
                } else {
                    break;
                }
            }
            uint8_t byte = (3 << 6) | (run - 1);
            out.push_back(byte);
        } else if (table[hash] == p) {
            out.push_back(hash);
            prev = p;
        } else {
            int dr = int (p.r) - prev.r;
            int dg = int (p.g) - prev.g;
            int db = int (p.b) - prev.b;
            if (-2 <= dr && dr < 2  && -2 <= dg && dg < 2  && -2 <= db && db < 2) {
                uint8_t byte = (1 << 6) | ((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2);
                out.push_back(byte);
            } else if (-32 <= dg && dg < 32 
                        && -8 <= (dr-dg) && (dr-dg) < 8
                        && -8 <= (db-dg) && (db-dg) < 8) {
                uint8_t byte = (2 << 6) | (dg + 32);
                out.push_back(byte);
                byte = ((dr - dg + 8) << 4) | (db - dg + 8);
                out.push_back(byte);
            } else {
                out.push_back(0xFE);
                out.push_back(p.r);
                out.push_back(p.g);
                out.push_back(p.b);
            }
            prev = p;
            table[hash] = p;
        }
    }
    for (int i = 0; i < 7; i++) {
        out.push_back(0x00);
    }
    out.push_back(0x01);
    std::ofstream(name, std::ios::binary).write((const char *) &out[0], out.size());
}

int main() {
    int width, height, bpp;
    uint8_t* rgb_image = stbi_load("img/2.png", &width, &height, &bpp, 3);
    encode(rgb_image, width, height, "img/2.qoi");
    stbi_image_free(rgb_image);
    decode("img/2.qoi", "img/2_out.png");
    return 0;
}
