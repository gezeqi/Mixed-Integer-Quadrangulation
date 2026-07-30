// ---------------------------------------------------------------------------
// Stage 3 of MIQ pipeline: extract a pure-quad mesh from the integer-grid map
// produced by MIQ, using libQEx (Ebke et al. 2013, "QEx: Robust Quad Mesh
// Extraction"). libQEx is consumed through its C API (interfaces/c/qex.h).
//
// Flow:
//   libigl (V, F)  +  MIQ (UV, FUV) ->  qex_TriMesh
//     -> qex_extractQuadMesh ->
//   qex_QuadMesh  ->  (V_quad, F_quad)  ->  OBJ + viewer
// ---------------------------------------------------------------------------
#include <igl/readOBJ.h>
#include <igl/readOFF.h>
#include <igl/writeOBJ.h>
#include <igl/local_basis.h>
#include <igl/copyleft/comiso/nrosy.h>
#include <igl/copyleft/comiso/miq.h>
#include <igl/opengl/glfw/Viewer.h>

#include <qex.h>            // libQEx C API

#include <Eigen/Core>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#include <fstream>
#include <cstdint>

int main(int argc, char* argv[])
{
    using namespace Eigen;
    _putenv_s("OPENBLAS_NUM_THREADS", "1");   // belt + suspenders, see 02_miq

    const std::string mesh_path = (argc > 1) ? argv[1] : "data/bumpy.off";
    const std::string out_path  = (argc > 2) ? argv[2] : "quad_out.obj";

    // --- 1) Load + cross field + MIQ (same prologue as 02_miq) ------------
    MatrixXd V; MatrixXi F;
    if (!igl::readOFF(mesh_path, V, F) && !igl::readOBJ(mesh_path, V, F)) {
        std::cerr << "Failed to load " << mesh_path << "\n"; return 1;
    }

    // VectorXi b(1); b << 0;
    // MatrixXd bc(1, 3); bc << 1, 0, 0;
    // MatrixXd X1; VectorXd Sing;
    // igl::copyleft::comiso::nrosy(V, F, b, bc, VectorXi(), VectorXd(),
    //                              MatrixXd(), 4, 0.5, X1, Sing);

    // MatrixXd B1, B2, B3;
    // igl::local_basis(V, F, B1, B2, B3);
    // MatrixXd X2(F.rows(), 3);
    // for (int f = 0; f < F.rows(); ++f)
    //     X2.row(f) = Vector3d(B3.row(f)).cross(Vector3d(X1.row(f)));





    // ------------------------------------------------------------
    // Load cross frames
    // Usage: .\04_quad_extract_load_frames.exe .\data\model.obj .\output.obj '.\cross_frames.bin'
    // ------------------------------------------------------------
    MatrixXd X1(F.rows(), 3);
    MatrixXd X2(F.rows(), 3);

    const std::string bin_path  = (argc > 3) ? argv[3] : "data/cross_frames.bin";
    std::ifstream fin(bin_path, std::ios::binary);

    if (!fin)
    {
        std::cerr << "Cannot open cross_frames.bin\n";
        return 1;
    }

    uint32_t face_num;
    fin.read(reinterpret_cast<char*>(&face_num), sizeof(uint32_t));

    if (face_num != static_cast<uint32_t>(F.rows()))
    {
        std::cerr << "Face number mismatch!\n";
        return 1;
    }

    for (int f = 0; f < F.rows(); ++f)
    {
        float buf[6];

        fin.read(reinterpret_cast<char*>(buf), sizeof(buf));

        X1.row(f) << buf[0], buf[1], buf[2];
        X2.row(f) << buf[3], buf[4], buf[5];
    }

    fin.close();

    std::cout << "[trace] Loaded cross frame for "
            << F.rows()
            << " triangles."
            << std::endl;












    MatrixXd UV; MatrixXi FUV;
    const std::vector<int> no_round;
    const std::vector<std::vector<int>> no_features;
    igl::copyleft::comiso::miq(V, F, X1, X2,
        UV, FUV,
        /*grad*/100.0, /*stiff*/5.0,
        /*directRound*/false, /*iter*/0, /*localIter*/5,
        /*doRound*/true, /*singRound*/true,
        no_round, no_features);
    std::cout << "[trace] MIQ: V=" << V.rows() << " F=" << F.rows()
              << " UV=" << UV.rows() << " FUV=" << FUV.rows() << std::endl;
    

    // --- 2) Repackage into libQEx's C structs -----------------------------
    // libQEx expects:
    //   - vertices[vertex_count]
    //   - tris[tri_count]           (per-tri vertex indices)
    //   - uvTris[tri_count]         (per-tri-corner UVs; this is FUV indexed
    //                                into UV)
    std::vector<qex_Point3> qex_verts(V.rows());
    for (int i = 0; i < V.rows(); ++i)
        qex_verts[i] = { V(i, 0), V(i, 1), V(i, 2) };

    std::vector<qex_Tri>    qex_tris(F.rows());
    std::vector<qex_UVTri>  qex_uvtris(F.rows());
    for (int f = 0; f < F.rows(); ++f) {
        for (int c = 0; c < 3; ++c) {
            qex_tris[f].indices[c]      = static_cast<qex_Index>(F(f, c));
            const int uv_idx            = FUV(f, c);
            qex_uvtris[f].uvs[c]        = { UV(uv_idx, 0), UV(uv_idx, 1) };
        }
    }

    qex_TriMesh in  = { (unsigned)qex_verts.size(), (unsigned)qex_tris.size(),
                        qex_verts.data(), qex_tris.data(), qex_uvtris.data() };
    qex_QuadMesh out = { 0, 0, nullptr, nullptr };

    // --- 3) Run QEx -------------------------------------------------------
    std::cout << "[trace] calling qex_extractQuadMesh..." << std::endl;
    qex_extractQuadMesh(&in, /*vertex valences*/nullptr, &out);
    std::cout << "[trace] QEx done: V_quad=" << out.vertex_count
              << " F_quad=" << out.quad_count << std::endl;
    if (out.quad_count == 0) {
        std::cerr << "QEx returned empty mesh; check that UV is seamless "
                     "(non-seamless MIQ output triggers this).\n";
        return 2;
    }

    // --- 4) Convert back to Eigen + save + visualize ----------------------
    MatrixXd Vq(out.vertex_count, 3);
    for (unsigned i = 0; i < out.vertex_count; ++i)
        Vq.row(i) << out.vertices[i].x[0], out.vertices[i].x[1], out.vertices[i].x[2];

    MatrixXi Fq(out.quad_count, 4);
    for (unsigned f = 0; f < out.quad_count; ++f)
        Fq.row(f) << out.quads[f].indices[0], out.quads[f].indices[1],
                     out.quads[f].indices[2], out.quads[f].indices[3];

    // free libQEx output (the library malloc'd it; caller frees)
    std::free(out.vertices);
    std::free(out.quads);

    // libigl's writeOBJ doesn't natively support quad faces (it wants Nx3).
    // Write OBJ manually to preserve quad topology:
    {
        FILE* fp = std::fopen(out_path.c_str(), "w");
        if (!fp) { std::cerr << "open " << out_path << " failed\n"; return 3; }
        for (int i = 0; i < Vq.rows(); ++i)
            std::fprintf(fp, "v %.9g %.9g %.9g\n", Vq(i,0), Vq(i,1), Vq(i,2));
        for (int f = 0; f < Fq.rows(); ++f)
            std::fprintf(fp, "f %d %d %d %d\n",
                Fq(f,0)+1, Fq(f,1)+1, Fq(f,2)+1, Fq(f,3)+1);
        std::fclose(fp);
        std::cout << "[trace] wrote " << out_path << std::endl;
    }

    // For the viewer we need to triangulate each quad (V.data() display only).
    MatrixXi Fq_tri(Fq.rows() * 2, 3);
    for (int f = 0; f < Fq.rows(); ++f) {
        Fq_tri.row(2*f    ) << Fq(f,0), Fq(f,1), Fq(f,2);
        Fq_tri.row(2*f + 1) << Fq(f,0), Fq(f,2), Fq(f,3);
    }

    igl::opengl::glfw::Viewer viewer;
    viewer.data().set_mesh(Vq, Fq_tri);
    // Fill is drawn from the triangulated faces above, but we DO NOT want
    // libigl to draw the triangle wireframe (that'd expose the fake diagonals).
    // Turn it off and rely on add_edges below to draw the TRUE quad edges only.
    viewer.data().show_lines = false;

    // Draw the real quad edges (4 per face) on top:
    MatrixXd P1(Fq.rows()*4, 3), P2(Fq.rows()*4, 3);
    for (int f = 0; f < Fq.rows(); ++f) {
        for (int k = 0; k < 4; ++k) {
            P1.row(4*f + k) = Vq.row(Fq(f, k));
            P2.row(4*f + k) = Vq.row(Fq(f, (k+1) % 4));
        }
    }
    viewer.data().add_edges(P1, P2, RowVector3d(0, 0, 0));
    viewer.data().line_width = 1.5f;        // thicker quad wire
    viewer.launch();
    return 0;
}
