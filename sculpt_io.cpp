#include "sculpt_io.hpp"
#include <fstream>

bool SculptIO::save_sculpt(const SculptMesh &m, const std::string &filename) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;
    const auto &verts = m.vertices();
    const auto &faces = m.faces();
    unsigned int nv = (unsigned int)verts.size();
    unsigned int nf = (unsigned int)faces.size();
    out.write("R3DS", 4);
    out.write((const char *)&nv, 4);
    out.write((const char *)&nf, 4);
    for (const auto &v : verts) {
        out.write((const char *)&v.pos.x, 8);
        out.write((const char *)&v.pos.y, 8);
        out.write((const char *)&v.pos.z, 8);
        out.write((const char *)&v.normal.x, 8);
        out.write((const char *)&v.normal.y, 8);
        out.write((const char *)&v.normal.z, 8);
    }
    for (const auto &f : faces) {
        int iv0 = f.v0, iv1 = f.v1, iv2 = f.v2;
        out.write((const char *)&iv0, 4);
        out.write((const char *)&iv1, 4);
        out.write((const char *)&iv2, 4);
    }
    return true;
}

SculptMesh *SculptIO::load_sculpt(const std::string &filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) return nullptr;
    char hdr[4];
    in.read(hdr, 4);
    if (hdr[0] != 'R' || hdr[1] != '3' || hdr[2] != 'D' || hdr[3] != 'S') return nullptr;
    unsigned int nv = 0, nf = 0;
    in.read((char *)&nv, 4);
    in.read((char *)&nf, 4);
    SculptMesh *m = new SculptMesh();
    m->vertices().resize(nv);
    for (unsigned int i = 0; i < nv; i++) {
        auto &v = m->vertices()[i];
        in.read((char *)&v.pos.x, 8);
        in.read((char *)&v.pos.y, 8);
        in.read((char *)&v.pos.z, 8);
        in.read((char *)&v.normal.x, 8);
        in.read((char *)&v.normal.y, 8);
        in.read((char *)&v.normal.z, 8);
    }
    m->faces().resize(nf);
    for (unsigned int i = 0; i < nf; i++) {
        auto &f = m->faces()[i];
        in.read((char *)&f.v0, 4);
        in.read((char *)&f.v1, 4);
        in.read((char *)&f.v2, 4);
    }
    m->rebuild_topology(true);
    m->rebuild_bvh();
    return m;
}
