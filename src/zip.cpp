#include "zip.h"

#include <stdexcept>

// ---------------------------------------------------------------------------
// Inflate (RFC 1951), modelled on Mark Adler's "puff".
namespace {

constexpr int MAXBITS = 15;

struct Huffman {
    short count[MAXBITS + 1];
    short symbol[288];
};

struct InflateError {};

struct State {
    const uint8_t* in;
    size_t inLen;
    size_t inPos = 0;
    uint32_t bitbuf = 0;
    int bitcnt = 0;
    std::vector<uint8_t>* out;

    int bits(int need) {
        uint32_t val = bitbuf;
        while (bitcnt < need) {
            if (inPos >= inLen) throw InflateError();
            val |= (uint32_t)in[inPos++] << bitcnt;
            bitcnt += 8;
        }
        bitbuf = val >> need;
        bitcnt -= need;
        return (int)(val & ((1u << need) - 1));
    }
};

int decode(State& s, const Huffman& h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= MAXBITS; len++) {
        code |= s.bits(1);
        int count = h.count[len];
        if (code - count < first) return h.symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -10;
}

int construct(Huffman& h, const short* length, int n) {
    for (int len = 0; len <= MAXBITS; len++) h.count[len] = 0;
    for (int sym = 0; sym < n; sym++) h.count[length[sym]]++;
    if (h.count[0] == n) return 0;
    int left = 1;
    for (int len = 1; len <= MAXBITS; len++) {
        left <<= 1;
        left -= h.count[len];
        if (left < 0) return left;
    }
    short offs[MAXBITS + 1];
    offs[1] = 0;
    for (int len = 1; len < MAXBITS; len++) offs[len + 1] = offs[len] + h.count[len];
    for (int sym = 0; sym < n; sym++)
        if (length[sym] != 0) h.symbol[offs[length[sym]]++] = (short)sym;
    return left;
}

void stored(State& s) {
    s.bitbuf = 0;
    s.bitcnt = 0;
    if (s.inPos + 4 > s.inLen) throw InflateError();
    unsigned len = s.in[s.inPos] | (s.in[s.inPos + 1] << 8);
    unsigned nlen = s.in[s.inPos + 2] | (s.in[s.inPos + 3] << 8);
    s.inPos += 4;
    if (len != (~nlen & 0xffff)) throw InflateError();
    if (s.inPos + len > s.inLen) throw InflateError();
    s.out->insert(s.out->end(), s.in + s.inPos, s.in + s.inPos + len);
    s.inPos += len;
}

void codes(State& s, const Huffman& lencode, const Huffman& distcode) {
    static const short lens[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                   31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const short lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const short dists[30] = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
                                    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const short dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    std::vector<uint8_t>& out = *s.out;
    int symbol;
    do {
        symbol = decode(s, lencode);
        if (symbol < 0) throw InflateError();
        if (symbol < 256) {
            out.push_back((uint8_t)symbol);
        } else if (symbol > 256) {
            symbol -= 257;
            if (symbol >= 29) throw InflateError();
            int len = lens[symbol] + s.bits(lext[symbol]);
            symbol = decode(s, distcode);
            if (symbol < 0 || symbol >= 30) throw InflateError();
            size_t dist = dists[symbol] + s.bits(dext[symbol]);
            if (dist > out.size()) throw InflateError();
            size_t from = out.size() - dist;
            for (int i = 0; i < len; i++) out.push_back(out[from + i]);
        }
    } while (symbol != 256);
}

void fixedBlock(State& s) {
    static bool built = false;
    static Huffman lencode, distcode;
    if (!built) {
        short lengths[288];
        int sym = 0;
        for (; sym < 144; sym++) lengths[sym] = 8;
        for (; sym < 256; sym++) lengths[sym] = 9;
        for (; sym < 280; sym++) lengths[sym] = 7;
        for (; sym < 288; sym++) lengths[sym] = 8;
        construct(lencode, lengths, 288);
        for (sym = 0; sym < 30; sym++) lengths[sym] = 5;
        construct(distcode, lengths, 30);
        built = true;
    }
    codes(s, lencode, distcode);
}

void dynamicBlock(State& s) {
    static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    short lengths[320];
    Huffman lencode, distcode;
    int nlen = s.bits(5) + 257;
    int ndist = s.bits(5) + 1;
    int ncode = s.bits(4) + 4;
    if (nlen > 286 || ndist > 30) throw InflateError();
    int index = 0;
    for (; index < ncode; index++) lengths[order[index]] = (short)s.bits(3);
    for (; index < 19; index++) lengths[order[index]] = 0;
    if (construct(lencode, lengths, 19) != 0) throw InflateError();
    index = 0;
    while (index < nlen + ndist) {
        int symbol = decode(s, lencode);
        if (symbol < 0) throw InflateError();
        if (symbol < 16) {
            lengths[index++] = (short)symbol;
        } else {
            short len = 0;
            if (symbol == 16) {
                if (index == 0) throw InflateError();
                len = lengths[index - 1];
                symbol = 3 + s.bits(2);
            } else if (symbol == 17) {
                symbol = 3 + s.bits(3);
            } else {
                symbol = 11 + s.bits(7);
            }
            if (index + symbol > nlen + ndist) throw InflateError();
            while (symbol--) lengths[index++] = len;
        }
    }
    if (lengths[256] == 0) throw InflateError();
    int err = construct(lencode, lengths, nlen);
    if (err && (err < 0 || nlen != lencode.count[0] + lencode.count[1])) throw InflateError();
    err = construct(distcode, lengths + nlen, ndist);
    if (err && (err < 0 || ndist != distcode.count[0] + distcode.count[1])) throw InflateError();
    codes(s, lencode, distcode);
}

}  // namespace

bool Inflate(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out, size_t expected) {
    State s;
    s.in = src;
    s.inLen = srcLen;
    s.out = &out;
    out.clear();
    out.reserve(expected);
    try {
        int last;
        do {
            last = s.bits(1);
            int type = s.bits(2);
            if (type == 0)
                stored(s);
            else if (type == 1)
                fixedBlock(s);
            else if (type == 2)
                dynamicBlock(s);
            else
                return false;
        } while (!last);
    } catch (InflateError&) {
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
static uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

bool ZipReadAll(const std::vector<uint8_t>& a, std::vector<ZipEntry>& out) {
    if (a.size() < 22) return false;
    // locate end of central directory
    size_t eocd = std::string::npos;
    size_t minPos = a.size() > 65557 ? a.size() - 65557 : 0;
    for (size_t i = a.size() - 22 + 1; i-- > minPos;) {
        if (rd32(&a[i]) == 0x06054b50) {
            eocd = i;
            break;
        }
    }
    if (eocd == std::string::npos) return false;
    uint16_t count = rd16(&a[eocd + 10]);
    size_t cd = rd32(&a[eocd + 16]);
    for (int i = 0; i < count; i++) {
        if (cd + 46 > a.size() || rd32(&a[cd]) != 0x02014b50) return !out.empty();
        uint16_t flags = rd16(&a[cd + 8]);
        uint16_t method = rd16(&a[cd + 10]);
        uint32_t csize = rd32(&a[cd + 20]);
        uint32_t usize = rd32(&a[cd + 24]);
        uint16_t nlen = rd16(&a[cd + 28]);
        uint16_t xlen = rd16(&a[cd + 30]);
        uint16_t clen = rd16(&a[cd + 32]);
        uint32_t lho = rd32(&a[cd + 42]);
        if (cd + 46 + nlen > a.size()) return false;
        std::string name((const char*)&a[cd + 46], nlen);
        cd += 46 + nlen + xlen + clen;
        if (flags & 1) continue;  // encrypted
        if (name.empty() || name.back() == '/') continue;
        if (lho + 30 > a.size() || rd32(&a[lho]) != 0x04034b50) continue;
        size_t dataPos = lho + 30 + rd16(&a[lho + 26]) + rd16(&a[lho + 28]);
        if (dataPos + csize > a.size()) continue;
        ZipEntry e;
        e.name = name;
        if (method == 0) {
            e.data.assign(a.begin() + dataPos, a.begin() + dataPos + csize);
        } else if (method == 8) {
            if (!Inflate(&a[dataPos], csize, e.data, usize)) continue;
        } else {
            continue;
        }
        out.push_back(std::move(e));
    }
    return true;
}
